/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the prosign style setting and the layout version 1 to 2 migration (references/internal/keyer_spec.md
 * sections 17.3.3 and 17.4.2, menu_subsystem_spec.md sections 4.8 and 5.1; AC-P6 to AC-P12). An independent oracle
 * parses a record byte by byte and builds the record that a save must write. Fault injection: random version 1 and
 * version 2 records, every flags value, power loss, CMD_FAIL, an interrupted erase and a low supply at each
 * operation of the first save after a version 1 load.
 */

#include <string.h>

#include "eeprom/eeprom.h"
#include "fake_iap.h"
#include "settings/settings.h"
#include "support/test_assert.h"
#include "util/crc8.h"

#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define OFFSET_VERSION 2U
#define OFFSET_TONE_LOW 3U
#define OFFSET_FLAGS 10U
#define OFFSET_CRC 14U
#define ID_COUNT 12U
#define SAVE_OPERATIONS (1U + 2U * SETTINGS_RECORD_SIZE)

typedef struct {
  uint16_t values[ID_COUNT];
} values_t;

/* menu_subsystem_spec.md section 5.2. */
static const uint16_t oracle_min[ID_COUNT] = {400U, 5U, 5U, 5U, 5U, 0U, 0U, 4U, 0U, 0U, 0U, 0U};
static const uint16_t oracle_max[ID_COUNT] = {1000U, 50U, 50U, 50U, 50U, 2U, 1U, 8U, 10U, 63U, 1U, 2U};
static const uint16_t oracle_step[ID_COUNT] = {10U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U};
static const uint16_t oracle_default[ID_COUNT] = {600U, 32U, 32U, 20U, 20U, 2U, 1U, 5U, 8U, 32U, 0U, 2U};

/* keyer_spec.md section 17.4.2. */
static const uint8_t v1_defaults[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                          0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0x54U};
static const uint8_t v1_reversed[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                          0x14U, 0x02U, 0x03U, 0x05U, 0x08U, 0x20U, 0x78U};

static uint32_t rng_state;
static uint32_t rng_next(void) {
  rng_state = rng_state * 1664525U + 1013904223U;
  return rng_state >> 8;
}

static uint16_t oracle_clamp(unsigned id, uint32_t value, uint16_t max) {
  if (value < oracle_min[id]) {
    value = oracle_min[id];
  }
  if (value > max) {
    value = max;
  }
  return (uint16_t)(oracle_min[id] + ((value - oracle_min[id]) / oracle_step[id]) * oracle_step[id]);
}

/* The values that a load of a record with a valid CRC must give (5.1, 5.3, 17.4.2). */
static values_t oracle_load(const uint8_t* record) {
  values_t out;
  const uint8_t flags = record[OFFSET_FLAGS];
  const unsigned style = (unsigned)((flags >> 2) & 0x03U);
  out.values[0] = oracle_clamp(0U, (uint32_t)record[3] | ((uint32_t)record[4] << 8), oracle_max[0]);
  out.values[1] = oracle_clamp(1U, record[5], oracle_max[1]);
  out.values[2] = oracle_clamp(2U, record[6], out.values[1]);
  out.values[3] = oracle_clamp(3U, record[7], oracle_max[3]);
  out.values[4] = oracle_clamp(4U, record[8], out.values[3]);
  out.values[5] = oracle_clamp(5U, record[9], oracle_max[5]);
  out.values[6] = (uint16_t)(flags & 0x01U);
  out.values[7] = oracle_clamp(7U, record[11], oracle_max[7]);
  out.values[8] = oracle_clamp(8U, record[12], oracle_max[8]);
  out.values[9] = oracle_clamp(9U, record[13], oracle_max[9]);
  out.values[10] = (uint16_t)((flags >> 1) & 0x01U);
  out.values[11] = (record[OFFSET_VERSION] == 1U) ? 2U : (uint16_t)((style > 2U) ? 2U : style);
  return out;
}

static values_t oracle_defaults(void) {
  values_t out;
  memcpy(out.values, oracle_default, sizeof(out.values));
  return out;
}

/* The version 2 record of @p values (5.1). */
static void oracle_record(const values_t* values, uint8_t* record) {
  record[0] = 0x43U;
  record[1] = 0x57U;
  record[2] = 0x02U;
  record[3] = (uint8_t)values->values[0];
  record[4] = (uint8_t)(values->values[0] >> 8);
  record[5] = (uint8_t)values->values[1];
  record[6] = (uint8_t)values->values[2];
  record[7] = (uint8_t)values->values[3];
  record[8] = (uint8_t)values->values[4];
  record[9] = (uint8_t)values->values[5];
  record[10] = (uint8_t)(values->values[6] | (values->values[10] << 1) | (values->values[11] << 2));
  record[11] = (uint8_t)values->values[7];
  record[12] = (uint8_t)values->values[8];
  record[13] = (uint8_t)values->values[9];
  record[14] = crc8_compute(record, 14U);
}

static bool live_is(const values_t* expected) {
  unsigned id;
  for (id = 0U; id < ID_COUNT; ++id) {
    if (settings_get((settings_id_t)id) != expected->values[id]) {
      return false;
    }
  }
  return true;
}

static values_t live_values(void) {
  values_t out;
  unsigned id;
  for (id = 0U; id < ID_COUNT; ++id) {
    out.values[id] = settings_get((settings_id_t)id);
  }
  return out;
}

static size_t write_operations(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

static settings_load_status_t load(const uint8_t* record) {
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, SETTINGS_RECORD_SIZE);
  return settings_load();
}

/* A random record of layout @p version with a valid CRC; fields are biased to the range edges. */
static void random_record(uint8_t version, uint8_t* record) {
  unsigned offset;
  record[0] = 0x43U;
  record[1] = 0x57U;
  record[2] = version;
  for (offset = 3U; offset < OFFSET_CRC; ++offset) {
    record[offset] = (uint8_t)rng_next();
  }
  if ((rng_next() % 2U) == 0U) {
    const uint16_t tone = (uint16_t)(400U + (rng_next() % 61U) * 10U);
    record[3] = (uint8_t)tone;
    record[4] = (uint8_t)(tone >> 8);
    record[5] = (uint8_t)(5U + rng_next() % 46U);
    record[6] = (uint8_t)(5U + rng_next() % 46U);
    record[9] = (uint8_t)(rng_next() % 3U);
    record[13] = (uint8_t)(rng_next() % 64U);
  }
  record[OFFSET_CRC] = crc8_compute(record, 14U);
}

/* Spec constants of 17.3.2 that the oracle uses. */
static void test_contract_constants(void) {
  TEST_ASSERT_EQUAL(ID_COUNT, SETTINGS_ID_COUNT);
  TEST_ASSERT_EQUAL(11U, SETTINGS_ID_PROSIGN_STYLE);
  TEST_ASSERT_EQUAL(2U, SETTINGS_LAYOUT_VERSION);
  TEST_ASSERT_EQUAL(15U, SETTINGS_RECORD_SIZE);
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, oracle_default[SETTINGS_ID_PROSIGN_STYLE]);
}

/* AC-P8, AC-P9, AC-P11: random records of every version against the oracle. A load never writes the EEPROM. */
static void test_random_records_against_oracle(void) {
  unsigned trial;
  rng_state = 0xC0FFEEU;
  for (trial = 0U; trial < 30000U; ++trial) {
    uint8_t record[SETTINGS_RECORD_SIZE];
    const uint8_t version = (trial % 3U == 2U) ? (uint8_t)rng_next() : (uint8_t)(1U + trial % 3U);
    const bool is_accepted = (version == 1U) || (version == 2U);
    values_t expected;
    settings_load_status_t status;
    random_record(version, record);
    expected = is_accepted ? oracle_load(record) : oracle_defaults();
    status = load(record);
    TEST_ASSERT_EQUAL(is_accepted ? SETTINGS_LOAD_OK : SETTINGS_LOAD_BAD_VERSION, status);
    if (!TEST_ASSERT(live_is(&expected))) {
      fprintf(stderr, "trial %u version %u\n", trial, (unsigned)version);
      return;
    }
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT(!settings_is_edited());
    fake_iap_clear_counters();
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
    TEST_ASSERT_EQUAL(0, memcmp(record, fake_iap_memory(), SETTINGS_RECORD_SIZE));
  }
}

/*
 * AC-P10: after an accepted load and one edit, the save writes exactly the oracle record of the live values:
 * version 2, flags bits 4-7 = 0, a valid CRC. The next load gives the same values.
 */
static void test_first_save_writes_oracle_record(void) {
  unsigned trial;
  rng_state = 0xBEEFU;
  for (trial = 0U; trial < 6000U; ++trial) {
    uint8_t record[SETTINGS_RECORD_SIZE];
    uint8_t expected_record[SETTINGS_RECORD_SIZE];
    values_t expected;
    const unsigned edit = rng_next() % 3U;
    random_record((uint8_t)(1U + trial % 2U), record);
    (void)load(record);
    expected = oracle_load(record);
    if (edit == 0U) {
      expected.values[SETTINGS_ID_TONE_HZ] = (expected.values[SETTINGS_ID_TONE_HZ] == 1000U) ? 990U : 1000U;
      TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, expected.values[SETTINGS_ID_TONE_HZ]));
    } else if (edit == 1U) {
      expected.values[SETTINGS_ID_PROSIGN_STYLE] = (uint16_t)((expected.values[SETTINGS_ID_PROSIGN_STYLE] + 1U) % 3U);
      TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, expected.values[SETTINGS_ID_PROSIGN_STYLE]));
    } else {
      expected.values[SETTINGS_ID_PADDLE_SWAP] ^= 1U;
      TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, expected.values[SETTINGS_ID_PADDLE_SWAP]));
    }
    TEST_ASSERT(live_is(&expected));
    fake_iap_clear_counters();
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    oracle_record(&expected, expected_record);
    if (!TEST_ASSERT_EQUAL(0, memcmp(expected_record, fake_iap_memory(), SETTINGS_RECORD_SIZE))) {
      fprintf(stderr, "trial %u edit %u\n", trial, edit);
      return;
    }
    TEST_ASSERT_EQUAL(0U, fake_iap_memory()[OFFSET_FLAGS] & 0xF0U);
    TEST_ASSERT_EQUAL(1U, fake_iap_command_count(CMD_ERASE));
    TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(CMD_PROGRAM));
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
    TEST_ASSERT(live_is(&expected));
  }
}

/*
 * AC-P7: from each of the 12 start states (a version 2 record), every permutation of the three flag sets reaches
 * each of the 12 targets. After every set: the set id has its value, the other 11 ids do not change, and the saved
 * flags byte is show + 2 * paddle + 4 * prosigns.
 */
static void test_flag_bits_independent_from_every_state(void) {
  static const unsigned orders[6][3] = {{6U, 10U, 11U}, {6U, 11U, 10U}, {10U, 6U, 11U},
                                        {10U, 11U, 6U}, {11U, 6U, 10U}, {11U, 10U, 6U}};
  unsigned start_state;
  for (start_state = 0U; start_state < 12U; ++start_state) {
    unsigned target_state;
    for (target_state = 0U; target_state < 12U; ++target_state) {
      unsigned order;
      for (order = 0U; order < 6U; ++order) {
        uint8_t record[SETTINGS_RECORD_SIZE];
        values_t start_values = oracle_defaults();
        values_t expected;
        uint16_t target[ID_COUNT];
        unsigned step;
        start_values.values[6] = (uint16_t)(start_state & 1U);
        start_values.values[10] = (uint16_t)((start_state >> 1) & 1U);
        start_values.values[11] = (uint16_t)(start_state >> 2);
        oracle_record(&start_values, record);
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
        TEST_ASSERT(live_is(&start_values));
        target[6] = (uint16_t)(target_state & 1U);
        target[10] = (uint16_t)((target_state >> 1) & 1U);
        target[11] = (uint16_t)(target_state >> 2);
        expected = start_values;
        for (step = 0U; step < 3U; ++step) {
          const unsigned id = orders[order][step];
          const bool is_change = expected.values[id] != target[id];
          expected.values[id] = target[id];
          TEST_ASSERT_EQUAL(is_change, settings_set((settings_id_t)id, target[id]));
          if (!TEST_ASSERT(live_is(&expected))) {
            fprintf(stderr, "start %u target %u order %u step %u\n", start_state, target_state, order, step);
          }
        }
        fake_iap_clear_counters();
        if (target_state == start_state) {
          TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
          TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
        } else {
          TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
          TEST_ASSERT_EQUAL(target[6] + 2U * target[10] + 4U * target[11], fake_iap_memory()[OFFSET_FLAGS]);
          TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
          TEST_ASSERT(live_is(&expected));
        }
      }
    }
  }
}

/* AC-P6: every requested value of each flag id from every state clamps to the range and touches only its bits. */
static void test_flag_set_clamps_and_keeps_other_bits(void) {
  static const unsigned flag_ids[3] = {6U, 10U, 11U};
  unsigned start_state;
  for (start_state = 0U; start_state < 12U; ++start_state) {
    size_t which;
    for (which = 0U; which < 3U; ++which) {
      uint32_t requested;
      for (requested = 0U; requested <= 0xFFFFU; requested = (requested < 300U) ? requested + 1U : requested * 3U) {
        uint8_t record[SETTINGS_RECORD_SIZE];
        values_t expected = oracle_defaults();
        const unsigned id = flag_ids[which];
        uint16_t clamped;
        expected.values[6] = (uint16_t)(start_state & 1U);
        expected.values[10] = (uint16_t)((start_state >> 1) & 1U);
        expected.values[11] = (uint16_t)(start_state >> 2);
        oracle_record(&expected, record);
        (void)load(record);
        clamped = oracle_clamp(id, requested, oracle_max[id]);
        TEST_ASSERT_EQUAL(clamped != expected.values[id], settings_set((settings_id_t)id, (uint16_t)requested));
        expected.values[id] = clamped;
        TEST_ASSERT(live_is(&expected));
      }
    }
  }
}

/* AC-P9: a version 1 record and its version 2 twin differ only in the prosign style. */
static void test_version_1_twin_of_version_2(void) {
  unsigned flags;
  for (flags = 0U; flags <= 0xFFU; ++flags) {
    uint8_t record[SETTINGS_RECORD_SIZE];
    values_t v1;
    values_t v2;
    memcpy(record, v1_defaults, sizeof(record));
    record[OFFSET_FLAGS] = (uint8_t)flags;
    record[OFFSET_CRC] = crc8_compute(record, 14U);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
    v1 = live_values();
    TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, v1.values[SETTINGS_ID_PROSIGN_STYLE]);
    record[OFFSET_VERSION] = 2U;
    record[OFFSET_CRC] = crc8_compute(record, 14U);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
    v2 = live_values();
    v2.values[SETTINGS_ID_PROSIGN_STYLE] = SETTINGS_PROSIGN_BOTH;
    TEST_ASSERT_EQUAL(0, memcmp(&v1, &v2, sizeof(v1)));
  }
}

/* AC-P9: a version 1 record with a broken magic, CRC or a version 1 CRC on a version 2 byte loads as defaults. */
static void test_version_1_bad_records(void) {
  const values_t defaults = oracle_defaults();
  uint8_t record[SETTINGS_RECORD_SIZE];
  unsigned bit;

  memcpy(record, v1_reversed, sizeof(record));
  record[OFFSET_VERSION] = 2U; /* the CRC is the one of version 1 */
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_CRC, load(record));
  TEST_ASSERT(live_is(&defaults));

  memcpy(record, v1_reversed, sizeof(record));
  record[0] = 0x42U;
  record[OFFSET_CRC] = crc8_compute(record, 14U);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, load(record));
  TEST_ASSERT(live_is(&defaults));

  for (bit = 0U; bit < 8U; ++bit) {
    memcpy(record, v1_reversed, sizeof(record));
    record[OFFSET_CRC] ^= (uint8_t)(1U << bit);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_CRC, load(record));
    TEST_ASSERT(live_is(&defaults));
    TEST_ASSERT_EQUAL(0U, write_operations());
  }
}

/*
 * UX-15 after a version 1 load: no save without a difference. A change and a change back, and the display
 * defaults of a recovery start, keep the version 1 record in the EEPROM.
 */
static void test_version_1_kept_without_difference(void) {
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(v1_reversed));
  TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, SETTINGS_PROSIGN_NAME));
  TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, SETTINGS_PROSIGN_BOTH));
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 610U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 600U));
  fake_iap_clear_counters();
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
  TEST_ASSERT_EQUAL(0, memcmp(v1_reversed, fake_iap_memory(), SETTINGS_RECORD_SIZE));

  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(v1_reversed));
  settings_use_display_defaults();
  TEST_ASSERT(!settings_is_edited());
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(v1_reversed, fake_iap_memory(), SETTINGS_RECORD_SIZE));
}

typedef enum { FAULT_POWER_LOSS, FAULT_CMD_FAIL, FAULT_SUPPLY_LOW, FAULT_ERASE_ZEROS, FAULT_ERASE_HALF } fault_t;

/* Loads @p old_record, applies the edits of @p edit, and returns the values that a full save must store. */
static values_t prepare_first_save(const uint8_t* old_record, unsigned edit) {
  values_t expected;
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(old_record));
  expected = oracle_load(old_record);
  switch (edit) {
    case 0U: /* the prosign style only */
      expected.values[SETTINGS_ID_PROSIGN_STYLE] = SETTINGS_PROSIGN_PUNCTUATION;
      break;
    case 1U: /* the tone only: the prosign style stays at the default of the version 1 load */
      expected.values[SETTINGS_ID_TONE_HZ] = 450U;
      break;
    default: /* several ids */
      expected.values[SETTINGS_ID_TONE_HZ] = 820U;
      expected.values[SETTINGS_ID_PADDLE_SWAP] ^= 1U;
      expected.values[SETTINGS_ID_SHOW_CW_CODE] ^= 1U;
      expected.values[SETTINGS_ID_PROSIGN_STYLE] = SETTINGS_PROSIGN_NAME;
      expected.values[SETTINGS_ID_CONTRAST] = 7U;
      break;
  }
  {
    unsigned id;
    for (id = 0U; id < ID_COUNT; ++id) {
      (void)settings_set((settings_id_t)id, expected.values[id]);
    }
  }
  TEST_ASSERT(live_is(&expected));
  fake_iap_clear_counters();
  return expected;
}

/*
 * AC-P12: each fault at each operation of the first save after a version 1 load. The next load gives the old
 * values (version 1 values with Both) in full, the new values in full or the defaults, never a mix.
 */
static void test_faults_in_first_save_after_version_1_load(void) {
  static const fault_t faults[5] = {FAULT_POWER_LOSS, FAULT_CMD_FAIL, FAULT_SUPPLY_LOW, FAULT_ERASE_ZEROS,
                                    FAULT_ERASE_HALF};
  uint8_t old_records[3][SETTINGS_RECORD_SIZE];
  uint8_t erase_pattern[FAKE_IAP_PAGE_SIZE];
  unsigned long outcomes[3] = {0U, 0U, 0U};
  size_t which;

  memcpy(old_records[0], v1_defaults, SETTINGS_RECORD_SIZE);
  memcpy(old_records[1], v1_reversed, SETTINGS_RECORD_SIZE);
  /* A non-default version 1 record with flags bits 2-7 set and a field out of range. */
  memcpy(old_records[2], v1_reversed, SETTINGS_RECORD_SIZE);
  old_records[2][3] = 0x84U; /* 900 Hz */
  old_records[2][4] = 0x03U;
  old_records[2][5] = 60U;
  old_records[2][OFFSET_FLAGS] = 0xFEU;
  old_records[2][OFFSET_CRC] = crc8_compute(old_records[2], 14U);

  for (which = 0U; which < 3U; ++which) {
    unsigned edit;
    for (edit = 0U; edit < 3U; ++edit) {
      size_t fault_index;
      const values_t old_values = oracle_load(old_records[which]);
      for (fault_index = 0U; fault_index < 5U; ++fault_index) {
        const fault_t fault = faults[fault_index];
        size_t point;
        const size_t last_point = ((fault == FAULT_ERASE_ZEROS) || (fault == FAULT_ERASE_HALF)) ? 1U : SAVE_OPERATIONS;
        for (point = 1U; point <= last_point; ++point) {
          const values_t new_values = prepare_first_save(old_records[which], edit);
          const values_t defaults = oracle_defaults();
          settings_save_status_t save_status;
          bool is_old;
          bool is_new;
          bool is_default;
          switch (fault) {
            case FAULT_POWER_LOSS:
              fake_iap_set_power_loss_after(point);
              break;
            case FAULT_CMD_FAIL:
              fake_iap_set_fail_operation(point);
              break;
            case FAULT_SUPPLY_LOW:
              fake_iap_set_supply_low_from(point);
              break;
            case FAULT_ERASE_ZEROS:
              memset(erase_pattern, 0x00, sizeof(erase_pattern));
              fake_iap_set_interrupted_erase(point, erase_pattern);
              break;
            default:
              memset(erase_pattern, 0xFF, sizeof(erase_pattern));
              memcpy(erase_pattern, old_records[which], 7U);
              fake_iap_set_interrupted_erase(point, erase_pattern);
              break;
          }
          save_status = settings_save_if_changed();
          /* The supply check runs before the erase and each program, not before the last verify read. */
          if ((fault == FAULT_CMD_FAIL) || ((fault == FAULT_SUPPLY_LOW) && (point < SAVE_OPERATIONS))) {
            TEST_ASSERT_EQUAL(SETTINGS_SAVE_FAILED, save_status);
            TEST_ASSERT(settings_has_unsaved_changes());
            TEST_ASSERT(settings_is_edited());
            TEST_ASSERT(live_is(&new_values));
          }
          fake_iap_clear_counters();
          (void)settings_load();
          is_old = live_is(&old_values);
          is_new = live_is(&new_values);
          is_default = live_is(&defaults);
          if (is_old) {
            ++outcomes[0];
          } else if (is_new) {
            ++outcomes[1];
          } else if (is_default) {
            ++outcomes[2];
          }
          if (!TEST_ASSERT(is_old || is_new || is_default)) {
            fprintf(stderr, "record %zu edit %u fault %u point %zu\n", which, edit, (unsigned)fault, point);
          }
          TEST_ASSERT_EQUAL(0U, write_operations());
        }
      }
    }
  }
  printf("fault outcomes: %lu old, %lu new, %lu defaults\n", outcomes[0], outcomes[1], outcomes[2]);
  /* Every outcome class happens: the sweep reaches the start, the middle and the end of the save. */
  TEST_ASSERT(outcomes[0] > 0U);
  TEST_ASSERT(outcomes[1] > 0U);
  TEST_ASSERT(outcomes[2] > 0U);
}

/* A failed first save after a version 1 load keeps the edit; the next save writes the full version 2 record. */
static void test_retry_after_failed_first_save(void) {
  size_t point;
  for (point = 1U; point <= SAVE_OPERATIONS; ++point) {
    uint8_t expected_record[SETTINGS_RECORD_SIZE];
    const values_t new_values = prepare_first_save(v1_reversed, 2U);
    fake_iap_set_fail_operation(point);
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_FAILED, settings_save_if_changed());
    fake_iap_clear_counters();
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    oracle_record(&new_values, expected_record);
    TEST_ASSERT_EQUAL(0, memcmp(expected_record, fake_iap_memory(), SETTINGS_RECORD_SIZE));
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
    TEST_ASSERT(live_is(&new_values));
  }
}

int main(void) {
  TEST_RUN(test_contract_constants);
  TEST_RUN(test_random_records_against_oracle);
  TEST_RUN(test_first_save_writes_oracle_record);
  TEST_RUN(test_flag_bits_independent_from_every_state);
  TEST_RUN(test_flag_set_clamps_and_keeps_other_bits);
  TEST_RUN(test_version_1_twin_of_version_2);
  TEST_RUN(test_version_1_bad_records);
  TEST_RUN(test_version_1_kept_without_difference);
  TEST_RUN(test_faults_in_first_save_after_version_1_load);
  TEST_RUN(test_retry_after_failed_first_save);
  return TEST_FINISH();
}
