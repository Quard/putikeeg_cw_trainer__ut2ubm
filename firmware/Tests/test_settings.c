/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "eeprom/eeprom.h"
#include "fake_iap.h"
#include "settings/settings.h"
#include "support/test_assert.h"
#include "util/crc8.h"

#define CMD_PROGRAM 2U
#define CMD_ERASE 3U

static const uint8_t default_record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                             0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x0BU};
static const uint16_t default_values[SETTINGS_ID_COUNT] = {600U, 32U, 32U, 20U, 20U, 2U, 1U, 5U, 8U, 32U, 0U, 2U, 6U};

static void put_record(const uint8_t* record) { memcpy(fake_iap_memory(), record, SETTINGS_RECORD_SIZE); }

/* Writes @p record with a correct CRC into the fake EEPROM. */
static void put_record_with_crc(uint8_t* record) {
  record[14] = crc8_compute(record, 14U);
  put_record(record);
}

static bool are_defaults(void) {
  settings_id_t id;
  for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
    if (settings_get(id) != default_values[id]) {
      return false;
    }
  }
  return true;
}

static void start_empty(void) {
  fake_iap_reset();
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, settings_load());
  fake_iap_clear_counters();
}

static size_t write_operations(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

static void test_crc_check_value(void) {
  static const uint8_t check[9] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
  TEST_ASSERT_EQUAL(0xF4U, crc8_compute(check, 9U));
  TEST_ASSERT_EQUAL(0x0BU, crc8_compute(default_record, 14U));
  TEST_ASSERT_EQUAL(0x00U, crc8_compute(check, 0U));
}

static void test_empty_eeprom_gives_defaults(void) {
  fake_iap_reset();
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, settings_load());
  TEST_ASSERT(are_defaults());
  TEST_ASSERT_EQUAL(0U, write_operations());
  TEST_ASSERT(!settings_is_edited());
  TEST_ASSERT(!settings_has_unsaved_changes());
}

static void test_valid_record_loads(void) {
  uint8_t record[SETTINGS_RECORD_SIZE];
  memcpy(record, default_record, sizeof(record));
  record[3] = 0xE8U; /* 1000 Hz */
  record[4] = 0x03U;
  record[7] = 25U;
  record[8] = 12U;
  record[9] = 0U;
  record[10] = 0xFEU; /* bit 0 clear, bits 1-7 ignored */
  record[13] = 0U;
  fake_iap_reset();
  put_record_with_crc(record);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  TEST_ASSERT_EQUAL(1000U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT_EQUAL(25U, settings_get(SETTINGS_ID_TX_WPM));
  TEST_ASSERT_EQUAL(12U, settings_get(SETTINGS_ID_TX_EFF_WPM));
  TEST_ASSERT_EQUAL(SETTINGS_KEY_STRAIGHT, settings_get(SETTINGS_ID_KEY_TYPE));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_CONTRAST));
  TEST_ASSERT_EQUAL(0U, write_operations());
}

static void check_load_failure(const uint8_t* record, settings_load_status_t expected) {
  fake_iap_reset();
  put_record(record);
  TEST_ASSERT_EQUAL(expected, settings_load());
  TEST_ASSERT(are_defaults());
  TEST_ASSERT(!settings_has_unsaved_changes());
  TEST_ASSERT_EQUAL(0U, write_operations());
}

static void test_bad_records(void) {
  uint8_t record[SETTINGS_RECORD_SIZE];

  memcpy(record, default_record, sizeof(record));
  record[14] ^= 0x01U;
  check_load_failure(record, SETTINGS_LOAD_BAD_CRC);

  memcpy(record, default_record, sizeof(record));
  record[2] = 4U;
  record[14] = crc8_compute(record, 14U);
  check_load_failure(record, SETTINGS_LOAD_BAD_VERSION);

  memcpy(record, default_record, sizeof(record));
  record[0] = 0xFFU;
  check_load_failure(record, SETTINGS_LOAD_NO_RECORD);

  /* Page erased but not written. */
  memset(record, 0xFF, sizeof(record));
  check_load_failure(record, SETTINGS_LOAD_NO_RECORD);

  /* Fields written, magic not yet. */
  memcpy(record, default_record, sizeof(record));
  record[0] = 0xFFU;
  record[1] = 0xFFU;
  check_load_failure(record, SETTINGS_LOAD_NO_RECORD);

  fake_iap_reset();
  put_record(default_record);
  fake_iap_set_fail_operation(3U);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_READ_FAILED, settings_load());
  TEST_ASSERT(are_defaults());
}

static void test_out_of_range_fields_are_clamped(void) {
  uint8_t record[SETTINGS_RECORD_SIZE];
  memcpy(record, default_record, sizeof(record));
  record[3] = 0xDCU; /* 1500 Hz */
  record[4] = 0x05U;
  record[5] = 30U; /* RX speed */
  record[6] = 40U; /* RX effective above RX */
  record[7] = 99U; /* TX speed above max */
  record[8] = 60U; /* TX effective above max */
  record[9] = 7U;  /* key type */
  record[10] = 0x03U;
  record[11] = 1U; /* max word length below min */
  record[12] = 200U;
  record[13] = 64U;
  fake_iap_reset();
  put_record_with_crc(record);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  TEST_ASSERT_EQUAL(1000U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT_EQUAL(30U, settings_get(SETTINGS_ID_RX_WPM));
  TEST_ASSERT_EQUAL(30U, settings_get(SETTINGS_ID_RX_EFF_WPM));
  TEST_ASSERT_EQUAL(50U, settings_get(SETTINGS_ID_TX_WPM));
  TEST_ASSERT_EQUAL(50U, settings_get(SETTINGS_ID_TX_EFF_WPM));
  TEST_ASSERT_EQUAL(2U, settings_get(SETTINGS_ID_KEY_TYPE));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(4U, settings_get(SETTINGS_ID_MAX_WORD_LEN));
  TEST_ASSERT_EQUAL(10U, settings_get(SETTINGS_ID_BACKLIGHT));
  TEST_ASSERT_EQUAL(63U, settings_get(SETTINGS_ID_CONTRAST));
  TEST_ASSERT(!settings_has_unsaved_changes());

  /* Off-grid tone rounds down; speeds below the minimum go up. */
  record[3] = 0x5DU; /* 605 Hz */
  record[4] = 0x02U;
  record[5] = 2U;
  record[6] = 1U;
  fake_iap_reset();
  put_record_with_crc(record);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  TEST_ASSERT_EQUAL(600U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT_EQUAL(5U, settings_get(SETTINGS_ID_RX_WPM));
  TEST_ASSERT_EQUAL(5U, settings_get(SETTINGS_ID_RX_EFF_WPM));
  record[3] = 0x00U; /* 0 Hz */
  record[4] = 0x00U;
  fake_iap_reset();
  put_record_with_crc(record);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  TEST_ASSERT_EQUAL(400U, settings_get(SETTINGS_ID_TONE_HZ));
}

static void test_ranges(void) {
  static const uint16_t mins[SETTINGS_ID_COUNT] = {400U, 5U, 5U, 5U, 5U, 0U, 0U, 4U, 0U, 0U, 0U, 0U, 4U};
  static const uint16_t maxes[SETTINGS_ID_COUNT] = {1000U, 50U, 32U, 50U, 20U, 2U, 1U, 8U, 10U, 63U, 1U, 2U, 12U};
  static const uint16_t steps[SETTINGS_ID_COUNT] = {10U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U};
  settings_id_t id;
  start_empty();
  for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
    TEST_ASSERT_EQUAL(mins[id], settings_min(id));
    TEST_ASSERT_EQUAL(maxes[id], settings_max(id));
    TEST_ASSERT_EQUAL(steps[id], settings_step(id));
  }
  TEST_ASSERT_EQUAL(0U, settings_min(SETTINGS_ID_COUNT));
  TEST_ASSERT_EQUAL(0U, settings_max(SETTINGS_ID_COUNT));
  TEST_ASSERT_EQUAL(0U, settings_step(0xFFU));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_COUNT));
  TEST_ASSERT(!settings_set(SETTINGS_ID_COUNT, 5U));
  TEST_ASSERT(!settings_is_edited());
}

static void test_clamp_and_grid(void) {
  start_empty();
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 615U));
  TEST_ASSERT_EQUAL(610U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 0xFFFFU));
  TEST_ASSERT_EQUAL(1000U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 0U));
  TEST_ASSERT_EQUAL(400U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT(!settings_set(SETTINGS_ID_TONE_HZ, 409U));
  TEST_ASSERT_EQUAL(400U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT(settings_set(SETTINGS_ID_CONTRAST, 64U));
  TEST_ASSERT_EQUAL(63U, settings_get(SETTINGS_ID_CONTRAST));
  TEST_ASSERT(settings_set(SETTINGS_ID_KEY_TYPE, 0U));
  TEST_ASSERT(settings_set(SETTINGS_ID_KEY_TYPE, 9U));
  TEST_ASSERT_EQUAL(2U, settings_get(SETTINGS_ID_KEY_TYPE));
  TEST_ASSERT(!settings_set(SETTINGS_ID_KEY_TYPE, 2U));
  TEST_ASSERT(settings_set(SETTINGS_ID_BACKLIGHT, 0U));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_BACKLIGHT));
  TEST_ASSERT(settings_set(SETTINGS_ID_MAX_WORD_LEN, 3U));
  TEST_ASSERT_EQUAL(4U, settings_get(SETTINGS_ID_MAX_WORD_LEN));
  TEST_ASSERT(settings_set(SETTINGS_ID_SHOW_CW_CODE, 7U) == false);
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
}

static void test_effective_speed_rule(void) {
  start_empty();
  TEST_ASSERT(!settings_set(SETTINGS_ID_RX_EFF_WPM, 40U));
  TEST_ASSERT_EQUAL(32U, settings_get(SETTINGS_ID_RX_EFF_WPM));
  TEST_ASSERT(settings_set(SETTINGS_ID_RX_EFF_WPM, 25U));
  TEST_ASSERT(settings_set(SETTINGS_ID_RX_WPM, 24U));
  TEST_ASSERT_EQUAL(24U, settings_get(SETTINGS_ID_RX_EFF_WPM));
  TEST_ASSERT(settings_set(SETTINGS_ID_RX_WPM, 30U));
  TEST_ASSERT_EQUAL(24U, settings_get(SETTINGS_ID_RX_EFF_WPM));
  TEST_ASSERT_EQUAL(30U, settings_max(SETTINGS_ID_RX_EFF_WPM));
  TEST_ASSERT(settings_set(SETTINGS_ID_TX_WPM, 5U));
  TEST_ASSERT_EQUAL(5U, settings_get(SETTINGS_ID_TX_EFF_WPM));
  TEST_ASSERT_EQUAL(5U, settings_max(SETTINGS_ID_TX_EFF_WPM));
  TEST_ASSERT(!settings_set(SETTINGS_ID_TX_EFF_WPM, 6U));
  TEST_ASSERT_EQUAL(24U, settings_get(SETTINGS_ID_RX_EFF_WPM));
}

static void test_record_bytes_after_save(void) {
  uint8_t expected[SETTINGS_RECORD_SIZE];
  start_empty();
  TEST_ASSERT(settings_set(SETTINGS_ID_MAX_WORD_LEN, 6U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  memcpy(expected, default_record, sizeof(expected));
  expected[11] = 6U;
  expected[14] = crc8_compute(expected, 14U);
  TEST_ASSERT(expected[14] != default_record[14]);
  TEST_ASSERT_EQUAL(0, memcmp(expected, fake_iap_memory(), sizeof(expected)));
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
}

static void test_save_load_round_trip_and_area(void) {
  static const uint16_t values[SETTINGS_ID_COUNT] = {990U, 50U, 41U, 7U, 6U, 0U, 0U, 8U, 0U, 63U, 0U, 0U, 9U};
  settings_id_t id;
  size_t address;
  start_empty();
  for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
    (void)settings_set(id, values[id]);
  }
  TEST_ASSERT(settings_has_unsaved_changes());
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT(!settings_is_edited());
  TEST_ASSERT(!settings_has_unsaved_changes());
  for (address = 0U; address < FAKE_IAP_SIZE; ++address) {
    TEST_ASSERT_EQUAL(address < SETTINGS_RECORD_SIZE ? 1U : 0U, fake_iap_program_count((uint16_t)address));
  }
  for (id = 1U; id < FAKE_IAP_PAGE_COUNT; ++id) {
    TEST_ASSERT_EQUAL(0U, fake_iap_erase_count(id));
  }
  for (address = SETTINGS_RECORD_SIZE; address < FAKE_IAP_SIZE; ++address) {
    TEST_ASSERT_EQUAL(0xFFU, fake_iap_memory()[address]);
  }
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
    TEST_ASSERT_EQUAL(values[id], settings_get(id));
  }
}

static void test_no_change_no_access(void) {
  start_empty();
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 610U));
  TEST_ASSERT(settings_is_edited());
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 600U));
  TEST_ASSERT(settings_is_edited());
  TEST_ASSERT(!settings_has_unsaved_changes());
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
  TEST_ASSERT(!settings_is_edited());
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

static void test_failed_save_keeps_baseline_and_retries(void) {
  start_empty();
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 700U));
  fake_iap_set_fail_operation(5U);
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_FAILED, settings_save_if_changed());
  TEST_ASSERT(settings_is_edited());
  TEST_ASSERT(settings_has_unsaved_changes());
  TEST_ASSERT_EQUAL(700U, settings_get(SETTINGS_ID_TONE_HZ));
  fake_iap_clear_counters();
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT(!settings_is_edited());
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  TEST_ASSERT_EQUAL(700U, settings_get(SETTINGS_ID_TONE_HZ));
}

/* After a save that stopped at any point, the next load gives the old values, the new values or the defaults. */
static void check_interrupted_saves(bool is_power_loss) {
  static const uint16_t old_tone = 800U;
  static const uint16_t new_tone = 450U;
  size_t point;
  size_t total_operations;

  /* One full save to count its operations. */
  start_empty();
  (void)settings_set(SETTINGS_ID_TONE_HZ, old_tone);
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  (void)settings_set(SETTINGS_ID_TONE_HZ, new_tone);
  (void)settings_set(SETTINGS_ID_CONTRAST, 5U);
  fake_iap_clear_counters();
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  total_operations = fake_iap_operation_count();
  TEST_ASSERT_EQUAL(1U + 2U * SETTINGS_RECORD_SIZE, total_operations);

  for (point = 1U; point <= total_operations; ++point) {
    settings_save_status_t status;
    uint16_t tone;
    uint16_t contrast;
    start_empty();
    (void)settings_set(SETTINGS_ID_TONE_HZ, old_tone);
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    (void)settings_set(SETTINGS_ID_TONE_HZ, new_tone);
    (void)settings_set(SETTINGS_ID_CONTRAST, 5U);
    fake_iap_clear_counters();
    if (is_power_loss) {
      fake_iap_set_power_loss_after(point);
    } else if (point < total_operations) {
      fake_iap_set_fail_operation(point + 1U);
    }
    status = settings_save_if_changed();
    if (!is_power_loss) {
      TEST_ASSERT_EQUAL((point < total_operations) ? SETTINGS_SAVE_FAILED : SETTINGS_SAVE_OK, status);
    }
    fake_iap_clear_counters();
    (void)settings_load();
    tone = settings_get(SETTINGS_ID_TONE_HZ);
    contrast = settings_get(SETTINGS_ID_CONTRAST);
    TEST_ASSERT(((tone == old_tone) && (contrast == 32U)) || ((tone == new_tone) && (contrast == 5U)) ||
                are_defaults());
    if (is_power_loss && (point == 1U)) {
      /* Power lost right after the erase: no valid record. */
      TEST_ASSERT(are_defaults());
    }
    if (point == total_operations) {
      TEST_ASSERT_EQUAL(new_tone, tone);
    }
  }
}

static void test_power_loss_at_each_operation(void) { check_interrupted_saves(true); }

static void test_cmd_fail_at_each_operation(void) { check_interrupted_saves(false); }

static void test_interrupted_erase_patterns(void) {
  uint8_t pattern[FAKE_IAP_PAGE_SIZE];
  uint8_t fill;
  for (fill = 0U; fill < 3U; ++fill) {
    start_empty();
    (void)settings_set(SETTINGS_ID_TONE_HZ, 800U);
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    (void)settings_set(SETTINGS_ID_TONE_HZ, 450U);
    memcpy(pattern, fake_iap_memory(), sizeof(pattern));
    if (fill == 1U) {
      memset(pattern, 0x00, 8U);
    } else if (fill == 2U) {
      memset(&pattern[2], 0xFF, 6U);
    }
    fake_iap_clear_counters();
    fake_iap_set_interrupted_erase(1U, pattern);
    (void)settings_save_if_changed();
    (void)settings_load();
    TEST_ASSERT((settings_get(SETTINGS_ID_TONE_HZ) == 800U) || are_defaults());
  }
}

static void test_low_supply_during_save(void) {
  size_t point;
  const size_t total_operations = 1U + 2U * SETTINGS_RECORD_SIZE;

  for (point = 1U; point <= total_operations; ++point) {
    uint16_t tone;
    start_empty();
    (void)settings_set(SETTINGS_ID_TONE_HZ, 800U);
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    (void)settings_set(SETTINGS_ID_TONE_HZ, 450U);
    fake_iap_clear_counters();
    fake_iap_set_supply_low_from(point);
    /*
     * Reads do not look at the flag; a flag that comes on before a read stops the next program. Only the last
     * operation, the read-back of magic byte 1, has no program after it.
     */
    if (point < total_operations) {
      TEST_ASSERT_EQUAL(SETTINGS_SAVE_FAILED, settings_save_if_changed());
      TEST_ASSERT(settings_is_edited());
      TEST_ASSERT(settings_has_unsaved_changes());
    } else {
      TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    }
    fake_iap_clear_counters();
    (void)settings_load();
    tone = settings_get(SETTINGS_ID_TONE_HZ);
    TEST_ASSERT((tone == 800U) || (tone == 450U) || are_defaults());
    TEST_ASSERT((tone == 450U) == (point == total_operations));
    if (point == 1U) {
      TEST_ASSERT_EQUAL(0U, fake_iap_erase_count(0U));
      TEST_ASSERT_EQUAL(800U, tone);
    }
  }

  /* An old flag from before the save does not stop it. */
  start_empty();
  (void)settings_set(SETTINGS_ID_TONE_HZ, 450U);
  fake_iap_set_supply_low(true);
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(1U, fake_iap_supply_clear_count());
}

static void test_display_defaults(void) {
  uint8_t record[SETTINGS_RECORD_SIZE];
  memcpy(record, default_record, sizeof(record));
  record[12] = 0U;
  record[13] = 0U;
  record[3] = 0x84U; /* 900 Hz */
  record[4] = 0x03U;
  fake_iap_reset();
  put_record_with_crc(record);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  settings_use_display_defaults();
  TEST_ASSERT_EQUAL(32U, settings_get(SETTINGS_ID_CONTRAST));
  TEST_ASSERT_EQUAL(8U, settings_get(SETTINGS_ID_BACKLIGHT));
  TEST_ASSERT_EQUAL(900U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT(!settings_is_edited());
  TEST_ASSERT(settings_has_unsaved_changes());
  fake_iap_clear_counters();
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
  /* An edit step saves all live values, also the display defaults. */
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 910U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  TEST_ASSERT_EQUAL(32U, settings_get(SETTINGS_ID_CONTRAST));
  TEST_ASSERT_EQUAL(8U, settings_get(SETTINGS_ID_BACKLIGHT));
}

static void test_edited_flag(void) {
  start_empty();
  TEST_ASSERT(!settings_is_edited());
  TEST_ASSERT(!settings_set(SETTINGS_ID_TONE_HZ, 600U));
  TEST_ASSERT(!settings_is_edited());
  TEST_ASSERT(settings_set(SETTINGS_ID_CONTRAST, 33U));
  TEST_ASSERT(settings_is_edited());
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT(!settings_is_edited());
  TEST_ASSERT(settings_set(SETTINGS_ID_CONTRAST, 34U));
  fake_iap_set_supply_low_from(1U);
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_FAILED, settings_save_if_changed());
  TEST_ASSERT(settings_is_edited());
}

int main(void) {
  TEST_RUN(test_crc_check_value);
  TEST_RUN(test_empty_eeprom_gives_defaults);
  TEST_RUN(test_valid_record_loads);
  TEST_RUN(test_bad_records);
  TEST_RUN(test_out_of_range_fields_are_clamped);
  TEST_RUN(test_ranges);
  TEST_RUN(test_clamp_and_grid);
  TEST_RUN(test_effective_speed_rule);
  TEST_RUN(test_record_bytes_after_save);
  TEST_RUN(test_save_load_round_trip_and_area);
  TEST_RUN(test_no_change_no_access);
  TEST_RUN(test_failed_save_keeps_baseline_and_retries);
  TEST_RUN(test_power_loss_at_each_operation);
  TEST_RUN(test_cmd_fail_at_each_operation);
  TEST_RUN(test_interrupted_erase_patterns);
  TEST_RUN(test_low_supply_during_save);
  TEST_RUN(test_display_defaults);
  TEST_RUN(test_edited_flag);
  return TEST_FINISH();
}
