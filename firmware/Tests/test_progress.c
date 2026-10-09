/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Host tests of the progress record (references/internal/training_spec.md section 10, AC-T9 to AC-T11). */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "eeprom/eeprom.h"
#include "eeprom/eeprom_map.h"
#include "fake_iap.h"
#include "progress/progress.h"
#include "test_assert.h"
#include "util/crc8.h"

#define CMD_READ 1U
#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define LEVEL_COUNT (2U * PROGRESS_SPEED_COUNT)
#define CRC_OFFSET 187U

typedef struct {
  uint16_t levels[2][PROGRESS_SPEED_COUNT];
} levels_t;

static uint8_t level_offset(uint8_t mode, uint8_t wpm) {
  return (uint8_t)(3U + (2U * (wpm - PROGRESS_WPM_MIN)) + ((mode == PROGRESS_MODE_SENDING) ? 92U : 0U));
}

/* Section 10.1, built from the layout table. */
static void build_record(uint8_t* record, const levels_t* levels, uint8_t version) {
  uint8_t mode;
  uint8_t wpm;

  memset(record, 0, PROGRESS_RECORD_SIZE);
  record[0] = 0x4BU;
  record[1] = 0x50U;
  record[2] = version;
  for (mode = 0U; mode < 2U; ++mode) {
    for (wpm = PROGRESS_WPM_MIN; wpm <= PROGRESS_WPM_MAX; ++wpm) {
      const uint16_t level = levels->levels[mode][wpm - PROGRESS_WPM_MIN];

      record[level_offset(mode, wpm)] = (uint8_t)(level & 0xFFU);
      record[level_offset(mode, wpm) + 1U] = (uint8_t)(level >> 8);
    }
  }
  record[CRC_OFFSET] = crc8_compute(record, CRC_OFFSET);
}

static void store_record(const uint8_t* record) {
  memcpy(&fake_iap_memory()[EEPROM_MAP_PROGRESS_ADDRESS], record, PROGRESS_RECORD_SIZE);
}

static void pattern_levels(levels_t* levels, uint16_t salt) {
  uint8_t mode;
  uint8_t index;

  for (mode = 0U; mode < 2U; ++mode) {
    for (index = 0U; index < PROGRESS_SPEED_COUNT; ++index) {
      levels->levels[mode][index] = (uint16_t)((mode * 1000U) + (index * 37U) + salt);
    }
  }
}

static bool are_levels(const levels_t* levels) {
  uint8_t mode;
  uint8_t wpm;
  bool is_equal = true;

  for (mode = 0U; mode < 2U; ++mode) {
    for (wpm = PROGRESS_WPM_MIN; wpm <= PROGRESS_WPM_MAX; ++wpm) {
      if (progress_level(mode, wpm) != levels->levels[mode][wpm - PROGRESS_WPM_MIN]) {
        is_equal = false;
      }
    }
  }
  return is_equal;
}

static bool are_levels_zero(void) {
  levels_t zero;

  memset(&zero, 0, sizeof(zero));
  return are_levels(&zero);
}

static void check_load_wrote_nothing(void) {
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_ERASE));
}

static void load_levels(const levels_t* levels) {
  uint8_t record[PROGRESS_RECORD_SIZE];

  fake_iap_reset();
  build_record(record, levels, PROGRESS_LAYOUT_VERSION);
  store_record(record);
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_load());
  check_load_wrote_nothing();
  TEST_ASSERT_EQUAL(PROGRESS_RECORD_SIZE, fake_iap_command_count(CMD_READ));
  fake_iap_clear_counters();
}

/* Loads a bad record over RAM levels that are not zero, so the clear is visible. */
static void check_bad_load(const uint8_t* record, progress_status_t expected) {
  levels_t levels;

  pattern_levels(&levels, 5U);
  load_levels(&levels);
  fake_iap_reset();
  if (record != NULL) {
    store_record(record);
  }
  TEST_ASSERT_EQUAL(expected, progress_load());
  TEST_ASSERT(are_levels_zero());
  TEST_ASSERT_EQUAL(0U, progress_level(2U, 20U));
  check_load_wrote_nothing();
}

/* AC-T9 */
static void test_load_failures(void) {
  uint8_t record[PROGRESS_RECORD_SIZE];
  levels_t levels;

  check_bad_load(NULL, PROGRESS_NO_RECORD);

  pattern_levels(&levels, 9U);
  build_record(record, &levels, PROGRESS_LAYOUT_VERSION);
  record[0] = 0x4CU;
  record[CRC_OFFSET] = crc8_compute(record, CRC_OFFSET);
  check_bad_load(record, PROGRESS_NO_RECORD);

  build_record(record, &levels, PROGRESS_LAYOUT_VERSION);
  record[1] = 0x51U;
  record[CRC_OFFSET] = crc8_compute(record, CRC_OFFSET);
  check_bad_load(record, PROGRESS_NO_RECORD);

  build_record(record, &levels, 0U);
  check_bad_load(record, PROGRESS_BAD_VERSION);
  build_record(record, &levels, 2U);
  check_bad_load(record, PROGRESS_BAD_VERSION);

  build_record(record, &levels, PROGRESS_LAYOUT_VERSION);
  record[CRC_OFFSET] ^= 0x01U;
  check_bad_load(record, PROGRESS_BAD_CRC);
  build_record(record, &levels, PROGRESS_LAYOUT_VERSION);
  record[100] ^= 0x80U;
  check_bad_load(record, PROGRESS_BAD_CRC);
}

static void test_load_read_failure(void) {
  static const size_t fail_operations[] = {1U, 2U, 100U, PROGRESS_RECORD_SIZE};
  uint8_t record[PROGRESS_RECORD_SIZE];
  levels_t levels;
  size_t index;

  for (index = 0U; index < sizeof(fail_operations) / sizeof(fail_operations[0]); ++index) {
    pattern_levels(&levels, 3U);
    load_levels(&levels);
    build_record(record, &levels, PROGRESS_LAYOUT_VERSION);
    store_record(record);
    fake_iap_set_fail_operation(fail_operations[index]);
    TEST_ASSERT_EQUAL(PROGRESS_READ_FAILED, progress_load());
    TEST_ASSERT(are_levels_zero());
    check_load_wrote_nothing();
  }
}

static void test_load_valid_record(void) {
  levels_t levels;

  pattern_levels(&levels, 11U);
  levels.levels[0][0] = 0xFFFFU;
  levels.levels[1][PROGRESS_SPEED_COUNT - 1U] = 0xFFFFU;
  levels.levels[0][20] = 0x0100U;
  levels.levels[1][3] = 0x00FFU;
  load_levels(&levels);
  TEST_ASSERT(are_levels(&levels));
  TEST_ASSERT_EQUAL(0xFFFFU, progress_level(PROGRESS_MODE_RECEIVING, 5U));
  TEST_ASSERT_EQUAL(0xFFFFU, progress_level(PROGRESS_MODE_SENDING, 50U));
  TEST_ASSERT_EQUAL(0U, progress_level(2U, 20U));
  TEST_ASSERT_EQUAL(0U, progress_level(255U, 20U));
}

static void test_wpm_clamp(void) {
  levels_t levels;

  pattern_levels(&levels, 1U);
  load_levels(&levels);
  TEST_ASSERT_EQUAL(levels.levels[0][0], progress_level(PROGRESS_MODE_RECEIVING, 0U));
  TEST_ASSERT_EQUAL(levels.levels[0][0], progress_level(PROGRESS_MODE_RECEIVING, 4U));
  TEST_ASSERT_EQUAL(levels.levels[1][45], progress_level(PROGRESS_MODE_SENDING, 51U));
  TEST_ASSERT_EQUAL(levels.levels[1][45], progress_level(PROGRESS_MODE_SENDING, 255U));
  TEST_ASSERT(progress_advance(PROGRESS_MODE_SENDING, 255U));
  TEST_ASSERT_EQUAL(levels.levels[1][45] + 1U, progress_level(PROGRESS_MODE_SENDING, 50U));
  TEST_ASSERT(progress_advance(PROGRESS_MODE_RECEIVING, 0U));
  TEST_ASSERT_EQUAL(levels.levels[0][0] + 1U, progress_level(PROGRESS_MODE_RECEIVING, 5U));
}

/* AC-T10: the written page equals the layout of section 10.1. */
static void test_save_layout(void) {
  uint8_t expected[PROGRESS_RECORD_SIZE];
  uint8_t settings_page[EEPROM_PAGE_SIZE];
  levels_t levels;
  size_t index;

  pattern_levels(&levels, 21U);
  load_levels(&levels);
  TEST_ASSERT(progress_advance(PROGRESS_MODE_SENDING, 33U));
  ++levels.levels[1][33U - PROGRESS_WPM_MIN];
  for (index = 0U; index < EEPROM_PAGE_SIZE; ++index) {
    fake_iap_memory()[index] = (uint8_t)(index * 7U);
    fake_iap_memory()[0x0400U + index] = (uint8_t)(index * 3U);
  }
  memcpy(settings_page, fake_iap_memory(), sizeof(settings_page));
  /* The low-voltage flag from before the save is cleared first. */
  fake_iap_set_supply_low(true);

  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_save());
  build_record(expected, &levels, PROGRESS_LAYOUT_VERSION);
  TEST_ASSERT(memcmp(expected, &fake_iap_memory()[EEPROM_MAP_PROGRESS_ADDRESS], PROGRESS_RECORD_SIZE) == 0);
  for (index = EEPROM_MAP_PROGRESS_ADDRESS + PROGRESS_RECORD_SIZE; index < 0x0400U; ++index) {
    TEST_ASSERT_EQUAL(0xFFU, fake_iap_memory()[index]);
  }
  TEST_ASSERT(memcmp(settings_page, fake_iap_memory(), sizeof(settings_page)) == 0);
  for (index = 0U; index < EEPROM_PAGE_SIZE; ++index) {
    TEST_ASSERT_EQUAL((uint8_t)(index * 3U), fake_iap_memory()[0x0400U + index]);
  }
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));
  TEST_ASSERT_EQUAL(1U, fake_iap_command_count(CMD_ERASE));
  TEST_ASSERT(fake_iap_supply_clear_count() >= 1U);
  for (index = 0U; index < PROGRESS_RECORD_SIZE; ++index) {
    TEST_ASSERT_EQUAL(1U, fake_iap_program_count((uint16_t)(EEPROM_MAP_PROGRESS_ADDRESS + index)));
  }

  /* A new start loads the saved levels. */
  memset(&levels, 0, sizeof(levels));
  fake_iap_clear_counters();
  pattern_levels(&levels, 21U);
  ++levels.levels[1][33U - PROGRESS_WPM_MIN];
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_load());
  TEST_ASSERT(are_levels(&levels));
}

/* Operations of one save on a fake with no faults. */
static size_t save_operation_count(void) {
  levels_t levels;

  pattern_levels(&levels, 2U);
  load_levels(&levels);
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_save());
  return fake_iap_operation_count();
}

/* AC-T10: a failed erase, first write or second write gives SAVE_FAILED and keeps the RAM levels. */
static void test_save_failures(void) {
  const size_t total = save_operation_count();
  /* One erase, then a program and a read-back per byte. */
  const size_t per_byte = (total - 1U) / PROGRESS_RECORD_SIZE;
  const size_t fail_operations[] = {
      1U,                                                 /* erase */
      2U,                                                 /* first byte of the first write */
      1U + (per_byte * (PROGRESS_RECORD_SIZE - 3U)) + 1U, /* last byte of the first write */
      total - (2U * per_byte) + 1U,                       /* magic byte 0, the second write */
      total - per_byte + 1U,                              /* magic byte 1 */
  };
  levels_t old_levels;
  levels_t levels;
  size_t index;

  TEST_ASSERT_EQUAL(1U + (per_byte * PROGRESS_RECORD_SIZE), total);
  for (index = 0U; index < sizeof(fail_operations) / sizeof(fail_operations[0]); ++index) {
    pattern_levels(&old_levels, 40U);
    load_levels(&old_levels);
    TEST_ASSERT(progress_advance(PROGRESS_MODE_RECEIVING, 20U));
    levels = old_levels;
    ++levels.levels[0][15];
    fake_iap_set_fail_operation(fail_operations[index]);
    TEST_ASSERT_EQUAL(PROGRESS_SAVE_FAILED, progress_save());
    TEST_ASSERT(are_levels(&levels));
    /* A failed erase keeps the old record. After the erase, the next start sees level 0 everywhere. */
    fake_iap_clear_counters();
    if (index == 0U) {
      TEST_ASSERT_EQUAL(PROGRESS_OK, progress_load());
      TEST_ASSERT(are_levels(&old_levels));
    } else {
      TEST_ASSERT(progress_load() != PROGRESS_OK);
      TEST_ASSERT(are_levels_zero());
    }
  }

  /* A supply that stays low stops the save before the erase. */
  pattern_levels(&levels, 41U);
  load_levels(&levels);
  fake_iap_set_supply_low_from(1U);
  TEST_ASSERT_EQUAL(PROGRESS_SAVE_FAILED, progress_save());
  TEST_ASSERT(are_levels(&levels));
}

/* AC-T10: a power loss after any operation never leaves a valid record with wrong data. */
static void test_save_power_loss(void) {
  const size_t total = save_operation_count();
  size_t after;
  levels_t old_levels;
  levels_t new_levels;
  size_t valid_count = 0U;

  for (after = 1U; after <= total; ++after) {
    pattern_levels(&old_levels, 50U);
    load_levels(&old_levels);
    TEST_ASSERT(progress_advance(PROGRESS_MODE_SENDING, 5U));
    new_levels = old_levels;
    ++new_levels.levels[1][0];
    fake_iap_set_power_loss_after(after);
    (void)progress_save();
    TEST_ASSERT(fake_iap_has_lost_power() || (after == total));

    fake_iap_clear_counters();
    if (progress_load() == PROGRESS_OK) {
      ++valid_count;
      TEST_ASSERT(are_levels(&new_levels));
    } else {
      TEST_ASSERT(are_levels_zero());
    }
  }
  /* Only the last program, magic byte 1, makes the record valid: a loss after it or after its read-back. */
  TEST_ASSERT_EQUAL(2U, valid_count);
}

/* AC-T11 */
static void test_advance_limits(void) {
  levels_t levels;

  memset(&levels, 0, sizeof(levels));
  levels.levels[0][10] = 65534U;
  levels.levels[1][10] = 0x00FFU;
  load_levels(&levels);
  TEST_ASSERT(progress_advance(PROGRESS_MODE_RECEIVING, 15U));
  TEST_ASSERT_EQUAL(65535U, progress_level(PROGRESS_MODE_RECEIVING, 15U));
  TEST_ASSERT(!progress_advance(PROGRESS_MODE_RECEIVING, 15U));
  TEST_ASSERT_EQUAL(65535U, progress_level(PROGRESS_MODE_RECEIVING, 15U));
  TEST_ASSERT(progress_advance(PROGRESS_MODE_SENDING, 15U));
  TEST_ASSERT_EQUAL(0x0100U, progress_level(PROGRESS_MODE_SENDING, 15U));
  levels.levels[0][10] = 65535U;
  levels.levels[1][10] = 0x0100U;
  TEST_ASSERT(are_levels(&levels));

  /* Unknown mode: level 0 and no advance. */
  TEST_ASSERT(!progress_advance(2U, 15U));
  TEST_ASSERT(!progress_advance(255U, 15U));
  TEST_ASSERT_EQUAL(0U, progress_level(2U, 15U));
  TEST_ASSERT(are_levels(&levels));
  TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
}

static void test_advance_changes_only_its_level(void) {
  uint8_t mode;
  uint8_t wpm;
  levels_t levels;

  pattern_levels(&levels, 300U);
  load_levels(&levels);
  for (mode = 0U; mode < 2U; ++mode) {
    for (wpm = PROGRESS_WPM_MIN; wpm <= PROGRESS_WPM_MAX; ++wpm) {
      TEST_ASSERT(progress_advance(mode, wpm));
      ++levels.levels[mode][wpm - PROGRESS_WPM_MIN];
      TEST_ASSERT(are_levels(&levels));
    }
  }
}

static void test_reset(void) {
  levels_t levels;

  pattern_levels(&levels, 77U);
  load_levels(&levels);
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_reset());
  TEST_ASSERT(are_levels_zero());
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));
  fake_iap_clear_counters();
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_load());
  TEST_ASSERT(are_levels_zero());

  /* With every level already 0, a reset still saves. */
  fake_iap_clear_counters();
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_reset());
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));

  /* A failed store gives SAVE_FAILED; the RAM levels are 0. */
  pattern_levels(&levels, 78U);
  load_levels(&levels);
  fake_iap_set_fail_operation(1U);
  TEST_ASSERT_EQUAL(PROGRESS_SAVE_FAILED, progress_reset());
  TEST_ASSERT(are_levels_zero());
}

int main(void) {
  TEST_RUN(test_load_failures);
  TEST_RUN(test_load_read_failure);
  TEST_RUN(test_load_valid_record);
  TEST_RUN(test_wpm_clamp);
  TEST_RUN(test_save_layout);
  TEST_RUN(test_save_failures);
  TEST_RUN(test_save_power_loss);
  TEST_RUN(test_advance_limits);
  TEST_RUN(test_advance_changes_only_its_level);
  TEST_RUN(test_reset);
  return TEST_FINISH();
}
