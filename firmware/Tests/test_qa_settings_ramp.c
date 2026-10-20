/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the Ramp setting and settings layout version 3 (references/internal/sidetone_spec.md section 24.6
 * check 6, menu_subsystem_spec.md sections 5.1, 5.2 and 18.2, AC-M1 to AC-M4, AC-M6, D-RM15). The oracle is the
 * vector table of 18.2, copied byte by byte, and a flags byte built from the four flag fields. Fault injection:
 * power loss, CMD_FAIL, a low supply and an interrupted erase at each operation of a version 3 save, and every
 * flags byte in a bad record.
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
#define OFFSET_FLAGS 10U
#define OFFSET_CRC 14U
#define SAVE_OPERATIONS (1U + 2U * SETTINGS_RECORD_SIZE)
#define RAMP_ID 12U

typedef struct {
  uint8_t bytes[SETTINGS_RECORD_SIZE];
  bool is_ok;
  uint8_t ramp;
  uint8_t show;
  uint8_t paddle;
  uint8_t prosigns;
} qa_vector_t;

/* menu_subsystem_spec.md section 18.2, in its order. */
static const qa_vector_t qa_vectors[] = {
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x0B}, true, 6, 1, 0, 2},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x49, 0x05, 0x08, 0x20, 0xC5}, true, 4, 1, 0, 2},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x89, 0x05, 0x08, 0x20, 0x6F}, true, 8, 1, 0, 2},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0xC9, 0x05, 0x08, 0x20, 0xF4}, true, 12, 1, 0, 2},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x6B, 0x05, 0x08, 0x20, 0x27}, true, 6, 1, 1, 2},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x61, 0x05, 0x08, 0x20, 0xBB}, true, 6, 1, 0, 0},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x6F, 0x05, 0x08, 0x20, 0x7F}, true, 6, 1, 1, 2},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x09, 0x05, 0x08, 0x20, 0x5E}, true, 4, 1, 0, 2},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0xF9, 0x05, 0x08, 0x20, 0x5D}, true, 12, 1, 0, 2},
    {{0x43, 0x57, 0x02, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x09, 0x05, 0x08, 0x20, 0x03}, true, 6, 1, 0, 2},
    {{0x43, 0x57, 0x02, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0xF9, 0x05, 0x08, 0x20, 0x00}, true, 6, 1, 0, 2},
    {{0x43, 0x57, 0x01, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x03, 0x05, 0x08, 0x20, 0x78}, true, 6, 1, 1, 2},
    {{0x43, 0x57, 0x01, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0xF3, 0x05, 0x08, 0x20, 0x7B}, true, 6, 1, 1, 2},
    {{0x43, 0x57, 0x04, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x9F}, false, 6, 1, 0, 2},
};
#define QA_VECTOR_COUNT (sizeof(qa_vectors) / sizeof(qa_vectors[0]))

static const uint8_t qa_default_v3[SETTINGS_RECORD_SIZE] = {0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14,
                                                            0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x0B};

static size_t write_operations(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

static settings_load_status_t load(const uint8_t* record) {
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, SETTINGS_RECORD_SIZE);
  return settings_load();
}

static uint8_t flags_of(unsigned show, unsigned paddle, unsigned prosigns, unsigned ramp) {
  return (uint8_t)(show | (paddle << 1) | (prosigns << 2) | (ramp << 4));
}

/* Every byte that is not flags or CRC must keep the default value of 5.1 after these vectors. */
static bool are_other_fields_default(void) {
  return (settings_get(SETTINGS_ID_TONE_HZ) == 600U) && (settings_get(SETTINGS_ID_RX_WPM) == 32U) &&
         (settings_get(SETTINGS_ID_RX_EFF_WPM) == 32U) && (settings_get(SETTINGS_ID_TX_WPM) == 20U) &&
         (settings_get(SETTINGS_ID_TX_EFF_WPM) == 20U) && (settings_get(SETTINGS_ID_KEY_TYPE) == 2U) &&
         (settings_get(SETTINGS_ID_MAX_WORD_LEN) == 5U) && (settings_get(SETTINGS_ID_BACKLIGHT) == 8U) &&
         (settings_get(SETTINGS_ID_CONTRAST) == 32U);
}

/* AC-M1, AC-28 side: the id, the ranges and the default. */
static void test_contract(void) {
  TEST_ASSERT_EQUAL(RAMP_ID, SETTINGS_ID_RAMP_MS);
  TEST_ASSERT_EQUAL(13U, SETTINGS_ID_COUNT);
  TEST_ASSERT_EQUAL(3U, SETTINGS_LAYOUT_VERSION);
  TEST_ASSERT_EQUAL(4U, settings_min(SETTINGS_ID_RAMP_MS));
  TEST_ASSERT_EQUAL(12U, settings_max(SETTINGS_ID_RAMP_MS));
  TEST_ASSERT_EQUAL(1U, settings_step(SETTINGS_ID_RAMP_MS));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_COUNT));
  TEST_ASSERT(!settings_set(SETTINGS_ID_COUNT, 6U));
  TEST_ASSERT_EQUAL(0x0BU, crc8_compute(qa_default_v3, 14U));
}

/* AC-M2: every vector of 18.2 gives its load result and its values; no load writes. */
static void test_vectors(void) {
  size_t i;
  for (i = 0U; i < QA_VECTOR_COUNT; ++i) {
    const qa_vector_t* vector = &qa_vectors[i];
    const settings_load_status_t status = load(vector->bytes);
    TEST_ASSERT_EQUAL(crc8_compute(vector->bytes, 14U), vector->bytes[OFFSET_CRC]);
    if (!TEST_ASSERT_EQUAL(vector->is_ok ? SETTINGS_LOAD_OK : SETTINGS_LOAD_BAD_VERSION, status)) {
      fprintf(stderr, "vector %zu\n", i);
    }
    if (!TEST_ASSERT_EQUAL(vector->ramp, settings_get(SETTINGS_ID_RAMP_MS))) {
      fprintf(stderr, "vector %zu\n", i);
    }
    TEST_ASSERT_EQUAL(vector->show, settings_get(SETTINGS_ID_SHOW_CW_CODE));
    TEST_ASSERT_EQUAL(vector->paddle, settings_get(SETTINGS_ID_PADDLE_SWAP));
    TEST_ASSERT_EQUAL(vector->prosigns, settings_get(SETTINGS_ID_PROSIGN_STYLE));
    TEST_ASSERT(are_other_fields_default());
    TEST_ASSERT_EQUAL(0U, write_operations());
    /* No edit step: a Settings exit writes nothing (UX-15), also after a version 1, 2 or 4 load. */
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT(!settings_is_edited());
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT_EQUAL(0, memcmp(vector->bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));
  }
}

/*
 * AC-M2: a version 1 or 2 record whose values equal the version 3 defaults has no unsaved change; a ramp edit and
 * an edit back keep the old record. The first real edit writes a full version 3 record.
 */
static void test_old_versions_save_only_after_an_edit(void) {
  size_t i;
  for (i = 9U; i <= 12U; ++i) {
    uint8_t expected[SETTINGS_RECORD_SIZE];
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(qa_vectors[i].bytes));
    TEST_ASSERT(settings_set(SETTINGS_ID_RAMP_MS, 9U));
    TEST_ASSERT(settings_set(SETTINGS_ID_RAMP_MS, 6U));
    fake_iap_clear_counters();
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT(settings_set(SETTINGS_ID_RAMP_MS, 11U));
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    memcpy(expected, qa_default_v3, sizeof(expected));
    expected[OFFSET_FLAGS] = flags_of(1U, qa_vectors[i].paddle, 2U, 11U);
    expected[OFFSET_CRC] = crc8_compute(expected, 14U);
    TEST_ASSERT_EQUAL(0, memcmp(expected, fake_iap_memory(), SETTINGS_RECORD_SIZE));
  }
}

/* AC-M1: settings_set() clamps 0..3 to 4 and 13..65535 to 12, from every start value. */
static void test_set_clamps(void) {
  uint8_t start;
  for (start = 4U; start <= 12U; ++start) {
    uint32_t value;
    for (value = 0U; value <= 0xFFFFU; value = (value < 300U) ? value + 1U : value * 2U + 1U) {
      const uint16_t expected = (uint16_t)((value < 4U) ? 4U : ((value > 12U) ? 12U : value));
      fake_iap_reset();
      (void)settings_load();
      (void)settings_set(SETTINGS_ID_RAMP_MS, start);
      TEST_ASSERT_EQUAL(expected != start, settings_set(SETTINGS_ID_RAMP_MS, (uint16_t)value));
      TEST_ASSERT_EQUAL(expected, settings_get(SETTINGS_ID_RAMP_MS));
    }
  }
  fake_iap_reset();
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, settings_load());
  TEST_ASSERT_EQUAL(6U, settings_get(SETTINGS_ID_RAMP_MS));
}

/* AC-M3: round trip of every ramp value; every save writes version 3 with a valid CRC. */
static void test_round_trip(void) {
  uint8_t ramp;
  fake_iap_reset();
  (void)settings_load();
  (void)settings_set(SETTINGS_ID_TONE_HZ, 610U);
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  (void)settings_set(SETTINGS_ID_TONE_HZ, 600U);
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(qa_default_v3, fake_iap_memory(), SETTINGS_RECORD_SIZE));
  for (ramp = 4U; ramp <= 12U; ++ramp) {
    (void)settings_set(SETTINGS_ID_RAMP_MS, (uint16_t)((ramp == 4U) ? 5U : 4U));
    (void)settings_save_if_changed();
    TEST_ASSERT(settings_set(SETTINGS_ID_RAMP_MS, ramp));
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    TEST_ASSERT_EQUAL(3U, fake_iap_memory()[OFFSET_VERSION]);
    TEST_ASSERT_EQUAL(flags_of(1U, 0U, 2U, ramp), fake_iap_memory()[OFFSET_FLAGS]);
    TEST_ASSERT_EQUAL(crc8_compute(fake_iap_memory(), 14U), fake_iap_memory()[OFFSET_CRC]);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
    TEST_ASSERT_EQUAL(ramp, settings_get(SETTINGS_ID_RAMP_MS));
  }
}

/*
 * AC-M4: from each of the 108 flag states (show, paddle, prosigns, ramp), each flag id set to each value of its
 * range changes only its own field; the saved flags byte is the field sum.
 */
static void test_flag_independence(void) {
  static const settings_id_t ids[4] = {SETTINGS_ID_SHOW_CW_CODE, SETTINGS_ID_PADDLE_SWAP, SETTINGS_ID_PROSIGN_STYLE,
                                       SETTINGS_ID_RAMP_MS};
  static const unsigned mins[4] = {0U, 0U, 0U, 4U};
  static const unsigned maxes[4] = {1U, 1U, 2U, 12U};
  unsigned state;
  for (state = 0U; state < 108U; ++state) {
    unsigned fields[4];
    size_t which;
    fields[0] = state % 2U;
    fields[1] = (state / 2U) % 2U;
    fields[2] = (state / 4U) % 3U;
    fields[3] = 4U + (state / 12U);
    for (which = 0U; which < 4U; ++which) {
      unsigned value;
      for (value = mins[which]; value <= maxes[which]; ++value) {
        uint8_t record[SETTINGS_RECORD_SIZE];
        unsigned expected[4];
        size_t k;
        memcpy(record, qa_default_v3, sizeof(record));
        record[OFFSET_FLAGS] = flags_of(fields[0], fields[1], fields[2], fields[3]);
        record[OFFSET_CRC] = crc8_compute(record, 14U);
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
        memcpy(expected, fields, sizeof(expected));
        expected[which] = value;
        TEST_ASSERT_EQUAL(value != fields[which], settings_set(ids[which], (uint16_t)value));
        for (k = 0U; k < 4U; ++k) {
          if (!TEST_ASSERT_EQUAL(expected[k], settings_get(ids[k]))) {
            fprintf(stderr, "state %u id %zu value %u field %zu\n", state, which, value, k);
          }
        }
        TEST_ASSERT(are_other_fields_default());
        TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 700U));
        TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
        TEST_ASSERT_EQUAL(flags_of(expected[0], expected[1], expected[2], expected[3]),
                          fake_iap_memory()[OFFSET_FLAGS]);
      }
    }
  }
}

/*
 * D-RM15: settings_set_defaults() does not clear the flags byte, so the four flag ids must rewrite all 8 bits. Every
 * flags byte in a record that fails each check (bad CRC, bad version, no magic) gives the default flags 0x69 in RAM
 * and in the next save.
 */
static void test_bad_record_flags_never_survive(void) {
  unsigned flags;
  for (flags = 0U; flags <= 0xFFU; ++flags) {
    unsigned failure;
    for (failure = 0U; failure < 3U; ++failure) {
      uint8_t record[SETTINGS_RECORD_SIZE];
      settings_load_status_t status;
      memcpy(record, qa_default_v3, sizeof(record));
      record[OFFSET_FLAGS] = (uint8_t)flags;
      record[OFFSET_CRC] = crc8_compute(record, 14U);
      if (failure == 0U) {
        record[OFFSET_CRC] ^= 0x5AU;
      } else if (failure == 1U) {
        record[OFFSET_VERSION] = 4U;
        record[OFFSET_CRC] = crc8_compute(record, 14U);
      } else {
        record[1] = 0x00U;
      }
      status = load(record);
      TEST_ASSERT(status != SETTINGS_LOAD_OK);
      TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
      TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_PADDLE_SWAP));
      TEST_ASSERT_EQUAL(2U, settings_get(SETTINGS_ID_PROSIGN_STYLE));
      TEST_ASSERT_EQUAL(6U, settings_get(SETTINGS_ID_RAMP_MS));
      TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 990U));
      TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
      if (!TEST_ASSERT_EQUAL(0x69U, fake_iap_memory()[OFFSET_FLAGS])) {
        fprintf(stderr, "flags 0x%02X failure %u\n", flags, failure);
      }
    }
  }
}

typedef enum { FAULT_POWER_LOSS, FAULT_CMD_FAIL, FAULT_SUPPLY_LOW, FAULT_ERASE_ZEROS, FAULT_ERASE_HALF } qa_fault_t;

/*
 * AC-M6: each fault at each operation of a version 3 save that changes the ramp. The next load gives the old
 * record in full, the new record in full or the defaults, never a mix, and it never writes.
 */
static void test_faults_during_version_3_save(void) {
  static const qa_fault_t faults[5] = {FAULT_POWER_LOSS, FAULT_CMD_FAIL, FAULT_SUPPLY_LOW, FAULT_ERASE_ZEROS,
                                       FAULT_ERASE_HALF};
  static const uint8_t old_ramps[3] = {10U, 4U, 12U};
  unsigned long outcomes[3] = {0U, 0U, 0U};
  size_t r;
  for (r = 0U; r < 3U; ++r) {
    uint8_t old_record[SETTINGS_RECORD_SIZE];
    size_t f;
    memcpy(old_record, qa_default_v3, sizeof(old_record));
    old_record[3] = 0x84U; /* 900 Hz */
    old_record[4] = 0x03U;
    old_record[OFFSET_FLAGS] = flags_of(0U, 1U, 1U, old_ramps[r]);
    old_record[OFFSET_CRC] = crc8_compute(old_record, 14U);
    for (f = 0U; f < 5U; ++f) {
      const size_t last = ((faults[f] == FAULT_ERASE_ZEROS) || (faults[f] == FAULT_ERASE_HALF)) ? 1U : SAVE_OPERATIONS;
      size_t point;
      for (point = 1U; point <= last; ++point) {
        uint8_t pattern[FAKE_IAP_PAGE_SIZE];
        const uint8_t new_ramp = (uint8_t)((old_ramps[r] == 4U) ? 12U : 4U);
        settings_save_status_t save;
        uint16_t ramp;
        uint16_t tone;
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(old_record));
        TEST_ASSERT(settings_set(SETTINGS_ID_RAMP_MS, new_ramp));
        TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, 0U));
        fake_iap_clear_counters();
        switch (faults[f]) {
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
            memset(pattern, 0x00, sizeof(pattern));
            fake_iap_set_interrupted_erase(point, pattern);
            break;
          default:
            memset(pattern, 0xFF, sizeof(pattern));
            memcpy(pattern, old_record, 7U);
            fake_iap_set_interrupted_erase(point, pattern);
            break;
        }
        save = settings_save_if_changed();
        if (faults[f] == FAULT_CMD_FAIL) {
          TEST_ASSERT_EQUAL(SETTINGS_SAVE_FAILED, save);
          TEST_ASSERT(settings_has_unsaved_changes());
        }
        fake_iap_clear_counters();
        (void)settings_load();
        TEST_ASSERT_EQUAL(0U, write_operations());
        ramp = settings_get(SETTINGS_ID_RAMP_MS);
        tone = settings_get(SETTINGS_ID_TONE_HZ);
        if ((ramp == old_ramps[r]) && (tone == 900U) && (settings_get(SETTINGS_ID_PROSIGN_STYLE) == 1U) &&
            (settings_get(SETTINGS_ID_PADDLE_SWAP) == 1U) && (settings_get(SETTINGS_ID_SHOW_CW_CODE) == 0U)) {
          ++outcomes[0];
        } else if ((ramp == new_ramp) && (tone == 900U) && (settings_get(SETTINGS_ID_PROSIGN_STYLE) == 0U) &&
                   (settings_get(SETTINGS_ID_PADDLE_SWAP) == 1U) && (settings_get(SETTINGS_ID_SHOW_CW_CODE) == 0U)) {
          ++outcomes[1];
        } else if ((ramp == 6U) && (tone == 600U) && (settings_get(SETTINGS_ID_PROSIGN_STYLE) == 2U) &&
                   (settings_get(SETTINGS_ID_PADDLE_SWAP) == 0U) && (settings_get(SETTINGS_ID_SHOW_CW_CODE) == 1U) &&
                   are_other_fields_default()) {
          ++outcomes[2];
        } else {
          TEST_ASSERT(false);
          fprintf(stderr, "mix: ramp %zu fault %zu point %zu\n", r, f, point);
        }
      }
    }
  }
  printf("fault outcomes: %lu old, %lu new, %lu defaults\n", outcomes[0], outcomes[1], outcomes[2]);
  TEST_ASSERT(outcomes[0] > 0U);
  TEST_ASSERT(outcomes[1] > 0U);
  TEST_ASSERT(outcomes[2] > 0U);
}

int main(void) {
  TEST_RUN(test_contract);
  TEST_RUN(test_vectors);
  TEST_RUN(test_old_versions_save_only_after_an_edit);
  TEST_RUN(test_set_clamps);
  TEST_RUN(test_round_trip);
  TEST_RUN(test_flag_independence);
  TEST_RUN(test_bad_record_flags_never_survive);
  TEST_RUN(test_faults_during_version_3_save);
  return TEST_FINISH();
}
