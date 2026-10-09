/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests for settings persistence: bit upsets of a stored record (the model of an interrupted erase or program),
 * records of other layout versions, every value of every field in a valid record, settings_set() against a clamp
 * oracle from menu_subsystem_spec.md sections 5.2 and 5.3, and CMD_FAIL at each read of a load. Layout versions 1
 * and 2 both load (keyer_spec.md section 17.4.2); version 1 gives the default prosign style.
 */
#include <string.h>

#include "eeprom/eeprom.h"
#include "fake_iap.h"
#include "settings/settings.h"
#include "support/test_assert.h"
#include "util/crc8.h"

#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define RECORD_BITS (SETTINGS_RECORD_SIZE * 8U)
#define RECORD_VERSION_OFFSET 2U
#define RECORD_FLAGS_OFFSET 10U

/* Layout version 1: the default record of the earlier firmware. */
static const uint8_t default_record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                             0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0x54U};
/* Layout version 2 default vector of keyer_spec.md section 17.4.2. */
static const uint8_t default_record_v2[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                                0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x03U};
static const uint8_t* const base_records[2] = {default_record, default_record_v2};
static const uint16_t default_values[SETTINGS_ID_COUNT] = {600U, 32U, 32U, 20U, 20U, 2U, 1U, 5U, 8U, 32U, 0U, 2U};

/*
 * Spec section 5.2, keyer_spec.md section 6.13 for Paddle and 17.4.2 for Prosigns. Effective speeds use the current
 * base speed as max.
 */
static const uint16_t spec_min[SETTINGS_ID_COUNT] = {400U, 5U, 5U, 5U, 5U, 0U, 0U, 4U, 0U, 0U, 0U, 0U};
static const uint16_t spec_max[SETTINGS_ID_COUNT] = {1000U, 50U, 50U, 50U, 50U, 2U, 1U, 8U, 10U, 63U, 1U, 2U};
static const uint16_t spec_step[SETTINGS_ID_COUNT] = {10U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U};

static uint16_t oracle_clamp(settings_id_t id, uint32_t value, uint16_t base_speed) {
  uint32_t max = spec_max[id];
  if ((id == SETTINGS_ID_RX_EFF_WPM) || (id == SETTINGS_ID_TX_EFF_WPM)) {
    max = base_speed;
  }
  if (value < spec_min[id]) {
    value = spec_min[id];
  }
  if (value > max) {
    value = max;
  }
  return (uint16_t)(spec_min[id] + ((value - spec_min[id]) / spec_step[id]) * spec_step[id]);
}

static size_t write_operations(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

static bool are_defaults(void) {
  settings_id_t id;
  for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
    if (settings_get(id) != default_values[id]) {
      return false;
    }
  }
  return true;
}

static void load_record(const uint8_t* record, settings_load_status_t* status) {
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, SETTINGS_RECORD_SIZE);
  *status = settings_load();
}

/* Values of @p record after the load rules, without the CRC check. */
static void values_of(const uint8_t* record, uint16_t* values) {
  values[SETTINGS_ID_TONE_HZ] = oracle_clamp(SETTINGS_ID_TONE_HZ, (uint32_t)record[3] | ((uint32_t)record[4] << 8), 0U);
  values[SETTINGS_ID_RX_WPM] = oracle_clamp(SETTINGS_ID_RX_WPM, record[5], 0U);
  values[SETTINGS_ID_RX_EFF_WPM] = oracle_clamp(SETTINGS_ID_RX_EFF_WPM, record[6], values[SETTINGS_ID_RX_WPM]);
  values[SETTINGS_ID_TX_WPM] = oracle_clamp(SETTINGS_ID_TX_WPM, record[7], 0U);
  values[SETTINGS_ID_TX_EFF_WPM] = oracle_clamp(SETTINGS_ID_TX_EFF_WPM, record[8], values[SETTINGS_ID_TX_WPM]);
  values[SETTINGS_ID_KEY_TYPE] = oracle_clamp(SETTINGS_ID_KEY_TYPE, record[9], 0U);
  values[SETTINGS_ID_SHOW_CW_CODE] = (uint16_t)(record[10] & 0x01U);
  values[SETTINGS_ID_MAX_WORD_LEN] = oracle_clamp(SETTINGS_ID_MAX_WORD_LEN, record[11], 0U);
  values[SETTINGS_ID_BACKLIGHT] = oracle_clamp(SETTINGS_ID_BACKLIGHT, record[12], 0U);
  values[SETTINGS_ID_CONTRAST] = oracle_clamp(SETTINGS_ID_CONTRAST, record[13], 0U);
  values[SETTINGS_ID_PADDLE_SWAP] = (uint16_t)((record[10] >> 1) & 0x01U);
  /* Version 1 has no prosign style and gets Both; version 2 keeps bits 2-3, and 3 clamps to 2. */
  values[SETTINGS_ID_PROSIGN_STYLE] = (record[RECORD_VERSION_OFFSET] == 1U)
                                          ? 2U
                                          : oracle_clamp(SETTINGS_ID_PROSIGN_STYLE, (record[10] >> 2) & 0x03U, 0U);
}

/* The flags byte that a save writes after a load of @p record with @p flags. */
static uint8_t saved_flags_of(const uint8_t* record, uint8_t flags) {
  uint16_t values[SETTINGS_ID_COUNT];
  uint8_t copy[SETTINGS_RECORD_SIZE];
  memcpy(copy, record, sizeof(copy));
  copy[RECORD_FLAGS_OFFSET] = flags;
  values_of(copy, values);
  return (uint8_t)(values[SETTINGS_ID_SHOW_CW_CODE] | (values[SETTINGS_ID_PADDLE_SWAP] << 1) |
                   (values[SETTINGS_ID_PROSIGN_STYLE] << 2));
}

static bool are_values(const uint16_t* values) {
  settings_id_t id;
  for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
    if (settings_get(id) != values[id]) {
      return false;
    }
  }
  return true;
}

static void flip(uint8_t* record, unsigned bit) { record[bit / 8U] ^= (uint8_t)(0x80U >> (bit % 8U)); }

/* A changed record either loads as the original values or fails to the defaults; it never gives a mix. */
static void check_upset(const uint8_t* original, const uint8_t* upset, const uint16_t* original_values, size_t* mixes) {
  settings_load_status_t status;
  load_record(upset, &status);
  if (status == SETTINGS_LOAD_OK) {
    if (!are_values(original_values)) {
      ++*mixes;
    }
  } else {
    TEST_ASSERT(are_defaults());
  }
  (void)original;
}

/*
 * An interrupted erase only sets bits (0 -> 1). All 1-, 2- and 3-bit upsets of the stored record must be caught:
 * CRC-8 polynomial 0x07 has Hamming distance 4 over this 120-bit code word. Also all 1- and 2-bit flips in both
 * directions (an interrupted program clears bits).
 */
static void test_bit_upsets_never_load_as_a_mix(void) {
  static const uint8_t originals[4][SETTINGS_RECORD_SIZE] = {
      {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0x54U},
      {0x43U, 0x57U, 0x01U, 0xE8U, 0x03U, 0x32U, 0x05U, 0x05U, 0x05U, 0x00U, 0x00U, 0x08U, 0x00U, 0x3FU, 0x00U},
      {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x03U},
      {0x43U, 0x57U, 0x02U, 0xE8U, 0x03U, 0x32U, 0x05U, 0x05U, 0x05U, 0x00U, 0x06U, 0x08U, 0x00U, 0x3FU, 0x00U},
  };
  size_t which;
  for (which = 0U; which < 4U; ++which) {
    uint8_t original[SETTINGS_RECORD_SIZE];
    uint8_t upset[SETTINGS_RECORD_SIZE];
    uint16_t values[SETTINGS_ID_COUNT];
    size_t mixes = 0U;
    size_t cases = 0U;
    unsigned a;
    unsigned b;
    unsigned c;
    memcpy(original, originals[which], sizeof(original));
    original[14] = crc8_compute(original, 14U);
    values_of(original, values);
    for (a = 0U; a < RECORD_BITS; ++a) {
      for (b = a; b < RECORD_BITS; ++b) {
        memcpy(upset, original, sizeof(upset));
        flip(upset, a);
        if (b != a) {
          flip(upset, b);
        }
        check_upset(original, upset, values, &mixes);
        ++cases;
      }
    }
    for (a = 0U; a < RECORD_BITS; ++a) {
      for (b = a + 1U; b < RECORD_BITS; ++b) {
        for (c = b + 1U; c < RECORD_BITS; ++c) {
          const uint8_t mask_a = (uint8_t)(0x80U >> (a % 8U));
          const uint8_t mask_b = (uint8_t)(0x80U >> (b % 8U));
          const uint8_t mask_c = (uint8_t)(0x80U >> (c % 8U));
          if (((original[a / 8U] & mask_a) != 0U) || ((original[b / 8U] & mask_b) != 0U) ||
              ((original[c / 8U] & mask_c) != 0U)) {
            continue;
          }
          memcpy(upset, original, sizeof(upset));
          flip(upset, a);
          flip(upset, b);
          flip(upset, c);
          check_upset(original, upset, values, &mixes);
          ++cases;
        }
      }
    }
    printf("record %u: %u upsets, %u loaded as a mix\n", (unsigned)which, (unsigned)cases, (unsigned)mixes);
    TEST_ASSERT_EQUAL(0U, mixes);
  }
}

/* A record of any layout version other than 1 and 2, with a valid CRC, gives the defaults and is never written. */
static void test_other_versions_give_defaults(void) {
  unsigned version;
  TEST_ASSERT_EQUAL(2U, SETTINGS_LAYOUT_VERSION);
  for (version = 0U; version <= 0xFFU; ++version) {
    uint8_t record[SETTINGS_RECORD_SIZE];
    settings_load_status_t status;
    if ((version == 1U) || (version == SETTINGS_LAYOUT_VERSION)) {
      continue;
    }
    memcpy(record, default_record, sizeof(record));
    record[2] = (uint8_t)version;
    record[5] = 40U;
    record[14] = crc8_compute(record, 14U);
    load_record(record, &status);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_VERSION, status);
    TEST_ASSERT(are_defaults());
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0, memcmp(record, fake_iap_memory(), sizeof(record)));
  }
}

/*
 * Every value of every one-byte field, and every tone value, in a record with a valid CRC loads clamped. Both
 * layout versions.
 */
static void test_every_field_value_loads_clamped(void) {
  unsigned offset;
  uint32_t value;
  size_t base;
  for (base = 0U; base < 2U; ++base) {
    for (offset = 5U; offset <= 13U; ++offset) {
      for (value = 0U; value <= 0xFFU; ++value) {
        uint8_t record[SETTINGS_RECORD_SIZE];
        uint16_t values[SETTINGS_ID_COUNT];
        settings_load_status_t status;
        memcpy(record, base_records[base], sizeof(record));
        record[offset] = (uint8_t)value;
        record[14] = crc8_compute(record, 14U);
        values_of(record, values);
        load_record(record, &status);
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, status);
        TEST_ASSERT(are_values(values));
        TEST_ASSERT(settings_get(SETTINGS_ID_RX_EFF_WPM) <= settings_get(SETTINGS_ID_RX_WPM));
        TEST_ASSERT(settings_get(SETTINGS_ID_TX_EFF_WPM) <= settings_get(SETTINGS_ID_TX_WPM));
        TEST_ASSERT(!settings_has_unsaved_changes());
        TEST_ASSERT(!settings_is_edited());
        TEST_ASSERT_EQUAL(0U, write_operations());
      }
    }
    for (value = 0U; value <= 0xFFFFU; value += 7U) {
      uint8_t record[SETTINGS_RECORD_SIZE];
      settings_load_status_t status;
      memcpy(record, base_records[base], sizeof(record));
      record[3] = (uint8_t)value;
      record[4] = (uint8_t)(value >> 8);
      record[14] = crc8_compute(record, 14U);
      load_record(record, &status);
      TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, status);
      TEST_ASSERT_EQUAL(oracle_clamp(SETTINGS_ID_TONE_HZ, value, 0U), settings_get(SETTINGS_ID_TONE_HZ));
    }
  }
}

/*
 * Unused flags bits are ignored on load and written as 0 by the next save; bit 1 is Paddle (keyer_spec.md 6.13),
 * bits 2-3 the prosign style of a version 2 record (17.4.2). Every flags byte value of both layout versions.
 */
static void test_flag_bits_written_as_zero(void) {
  unsigned flags;
  size_t base;
  for (base = 0U; base < 2U; ++base) {
    for (flags = 0U; flags <= 0xFFU; ++flags) {
      uint8_t record[SETTINGS_RECORD_SIZE];
      settings_load_status_t status;
      const unsigned style = (base == 0U) ? 2U : ((((flags >> 2) & 0x03U) == 3U) ? 2U : ((flags >> 2) & 0x03U));
      memcpy(record, base_records[base], sizeof(record));
      record[10] = (uint8_t)flags;
      record[14] = crc8_compute(record, 14U);
      load_record(record, &status);
      TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, status);
      TEST_ASSERT_EQUAL(flags & 0x01U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
      TEST_ASSERT_EQUAL((flags >> 1) & 0x01U, settings_get(SETTINGS_ID_PADDLE_SWAP));
      TEST_ASSERT_EQUAL(style, settings_get(SETTINGS_ID_PROSIGN_STYLE));
      TEST_ASSERT(!settings_has_unsaved_changes());
      TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 700U));
      TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
      TEST_ASSERT_EQUAL((flags & 0x03U) | (style << 2), fake_iap_memory()[10]);
      TEST_ASSERT_EQUAL(saved_flags_of(record, (uint8_t)flags), fake_iap_memory()[10]);
      TEST_ASSERT_EQUAL(SETTINGS_LAYOUT_VERSION, fake_iap_memory()[RECORD_VERSION_OFFSET]);
      TEST_ASSERT_EQUAL(crc8_compute(fake_iap_memory(), 14U), fake_iap_memory()[14]);
    }
  }
}

/*
 * V-78 / AC-K50: a record of the earlier firmware (flags bit 1 = 0, its own CRC) loads as Normal with no write.
 * The Reversed vector of keyer_spec.md 6.13 loads as Reversed. Toggling one flag id never moves the other bit.
 */
static void test_paddle_record_vectors(void) {
  static const uint8_t reversed_record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                                0x14U, 0x02U, 0x03U, 0x05U, 0x08U, 0x20U, 0x78U};
  uint8_t old_record[SETTINGS_RECORD_SIZE];
  settings_load_status_t status;
  unsigned show;
  unsigned paddle;

  /* Earlier firmware: Show CW Code off, Tone 900 Hz, valid CRC computed over its own bytes. */
  memcpy(old_record, default_record, sizeof(old_record));
  old_record[3] = 0x84U;
  old_record[4] = 0x03U;
  old_record[10] = 0x00U;
  old_record[14] = crc8_compute(old_record, 14U);
  load_record(old_record, &status);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, status);
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(900U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0U, write_operations());

  TEST_ASSERT_EQUAL(0x78U, crc8_compute(reversed_record, 14U));
  load_record(reversed_record, &status);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, status);
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());

  for (show = 0U; show <= 1U; ++show) {
    for (paddle = 0U; paddle <= 1U; ++paddle) {
      load_record(default_record, &status);
      (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, (uint16_t)show);
      (void)settings_set(SETTINGS_ID_PADDLE_SWAP, (uint16_t)paddle);
      TEST_ASSERT_EQUAL(show, settings_get(SETTINGS_ID_SHOW_CW_CODE));
      TEST_ASSERT_EQUAL(paddle, settings_get(SETTINGS_ID_PADDLE_SWAP));
      /* Values above 1 clamp to 1 and keep the other bit. */
      (void)settings_set(SETTINGS_ID_PADDLE_SWAP, 0xFFFFU);
      TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
      TEST_ASSERT_EQUAL(show, settings_get(SETTINGS_ID_SHOW_CW_CODE));
      (void)settings_set(SETTINGS_ID_PADDLE_SWAP, (uint16_t)paddle);
      fake_iap_clear_counters();
      if ((show == 1U) && (paddle == 0U)) {
        /* A change and a change back: no save. */
        TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
        TEST_ASSERT_EQUAL(0U, write_operations());
      } else {
        TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
        /* The save writes layout version 2 with the default prosign style Both in bits 2-3. */
        TEST_ASSERT_EQUAL((uint8_t)(show | (paddle << 1) | (2U << 2)), fake_iap_memory()[10]);
        TEST_ASSERT_EQUAL(SETTINGS_LAYOUT_VERSION, fake_iap_memory()[RECORD_VERSION_OFFSET]);
        TEST_ASSERT_EQUAL(crc8_compute(fake_iap_memory(), 14U), fake_iap_memory()[14]);
      }
    }
  }
}

/* settings_set() against the oracle for every id and value, from several base speeds. */
static void test_set_matches_clamp_oracle(void) {
  static const uint16_t bases[4] = {5U, 17U, 32U, 50U};
  size_t base_index;
  settings_id_t id;
  uint32_t value;
  for (base_index = 0U; base_index < 4U; ++base_index) {
    for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
      for (value = 0U; value <= 0x10000U; value = (value < 1100U) ? value + 1U : value + 997U) {
        const uint16_t requested = (uint16_t)((value > 0xFFFFU) ? 0xFFFFU : value);
        uint16_t before[SETTINGS_ID_COUNT];
        uint16_t expected;
        settings_id_t other;
        bool is_changed;
        fake_iap_reset();
        (void)settings_load();
        (void)settings_set(SETTINGS_ID_RX_WPM, bases[base_index]);
        (void)settings_set(SETTINGS_ID_TX_WPM, bases[base_index]);
        for (other = 0U; other < SETTINGS_ID_COUNT; ++other) {
          before[other] = settings_get(other);
        }
        expected = oracle_clamp(id, requested,
                                (id == SETTINGS_ID_RX_EFF_WPM)   ? before[SETTINGS_ID_RX_WPM]
                                : (id == SETTINGS_ID_TX_EFF_WPM) ? before[SETTINGS_ID_TX_WPM]
                                                                 : 0U);
        is_changed = settings_set(id, requested);
        TEST_ASSERT_EQUAL(expected, settings_get(id));
        TEST_ASSERT_EQUAL(expected != before[id], is_changed);
        for (other = 0U; other < SETTINGS_ID_COUNT; ++other) {
          uint16_t other_expected = before[other];
          if (other == id) {
            continue;
          }
          if (((id == SETTINGS_ID_RX_WPM) && (other == SETTINGS_ID_RX_EFF_WPM)) ||
              ((id == SETTINGS_ID_TX_WPM) && (other == SETTINGS_ID_TX_EFF_WPM))) {
            other_expected = (before[other] > expected) ? expected : before[other];
          }
          TEST_ASSERT_EQUAL(other_expected, settings_get(other));
        }
      }
    }
  }
}

/* CMD_FAIL at each read of a load gives READ_FAILED and the defaults. */
static void test_read_fail_at_each_byte(void) {
  size_t operation;
  uint8_t record[SETTINGS_RECORD_SIZE];
  memcpy(record, default_record, sizeof(record));
  record[3] = 0x84U; /* 900 Hz */
  record[4] = 0x03U;
  record[14] = crc8_compute(record, 14U);
  for (operation = 1U; operation <= SETTINGS_RECORD_SIZE; ++operation) {
    fake_iap_reset();
    memcpy(fake_iap_memory(), record, sizeof(record));
    fake_iap_set_fail_operation(operation);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_READ_FAILED, settings_load());
    TEST_ASSERT(are_defaults());
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT(fake_iap_is_idle());
  }
}

/* A save with a CMD_FAIL, then a save that succeeds: the record holds exactly the live values. */
static void test_retry_after_fail_writes_live_values(void) {
  size_t operation;
  for (operation = 1U; operation <= 1U + 2U * SETTINGS_RECORD_SIZE; ++operation) {
    uint16_t values[SETTINGS_ID_COUNT];
    settings_id_t id;
    fake_iap_reset();
    (void)settings_load();
    (void)settings_set(SETTINGS_ID_TONE_HZ, 820U);
    (void)settings_set(SETTINGS_ID_TX_WPM, 9U);
    /* Operations count from the last clear; the load above used the first ones. */
    fake_iap_clear_counters();
    fake_iap_set_fail_operation(operation);
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_FAILED, settings_save_if_changed());
    TEST_ASSERT(fake_iap_is_idle());
    (void)settings_set(SETTINGS_ID_CONTRAST, 40U);
    fake_iap_clear_counters();
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
      values[id] = settings_get(id);
    }
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
    TEST_ASSERT(are_values(values));
    TEST_ASSERT_EQUAL(820U, settings_get(SETTINGS_ID_TONE_HZ));
    TEST_ASSERT_EQUAL(9U, settings_get(SETTINGS_ID_TX_EFF_WPM));
    TEST_ASSERT_EQUAL(40U, settings_get(SETTINGS_ID_CONTRAST));
  }
}

int main(void) {
  TEST_RUN(test_bit_upsets_never_load_as_a_mix);
  TEST_RUN(test_other_versions_give_defaults);
  TEST_RUN(test_every_field_value_loads_clamped);
  TEST_RUN(test_flag_bits_written_as_zero);
  TEST_RUN(test_paddle_record_vectors);
  TEST_RUN(test_set_matches_clamp_oracle);
  TEST_RUN(test_read_fail_at_each_byte);
  TEST_RUN(test_retry_after_fail_writes_live_values);
  return TEST_FINISH();
}
