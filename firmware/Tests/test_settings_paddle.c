/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Host tests of the Paddle setting in flags bit 1 (references/internal/keyer_spec.md section 6.13, AC-K49, AC-K50). */

#include <string.h>

#include "eeprom/eeprom.h"
#include "fake_iap.h"
#include "settings/settings.h"
#include "support/test_assert.h"
#include "util/crc8.h"

#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define RECORD_FLAGS 10U

/* Layout version 1 records of the earlier firmware load; every save writes layout version 3 (menu spec 18.2). */
static const uint8_t default_record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                             0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0x54U};
static const uint8_t reversed_record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                              0x14U, 0x02U, 0x03U, 0x05U, 0x08U, 0x20U, 0x78U};
static const uint8_t default_record_v3[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                                0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x0BU};
static const uint8_t reversed_record_v3[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                                 0x14U, 0x02U, 0x6BU, 0x05U, 0x08U, 0x20U, 0x27U};

static size_t write_operations(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

static settings_load_status_t load(const uint8_t* record) {
  settings_load_status_t status;
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, SETTINGS_RECORD_SIZE);
  status = settings_load();
  fake_iap_clear_counters();
  return status;
}

static void test_record_vectors_have_valid_crc(void) {
  TEST_ASSERT_EQUAL(default_record[14], crc8_compute(default_record, 14U));
  TEST_ASSERT_EQUAL(reversed_record[14], crc8_compute(reversed_record, 14U));
  TEST_ASSERT_EQUAL(default_record_v3[14], crc8_compute(default_record_v3, 14U));
  TEST_ASSERT_EQUAL(reversed_record_v3[14], crc8_compute(reversed_record_v3, 14U));
}

/* AC-K49 */
static void test_paddle_values(void) {
  fake_iap_reset();
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, settings_load());
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(0U, settings_min(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(1U, settings_max(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(1U, settings_step(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(13U, SETTINGS_ID_COUNT);
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT(!settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT(!settings_set(SETTINGS_ID_PADDLE_SWAP, 7U));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 0U));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_PADDLE_SWAP));
}

/* AC-K49: each flag id changes only its own bit. */
static void test_flags_bits_are_independent(void) {
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(default_record));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT(settings_set(SETTINGS_ID_SHOW_CW_CODE, 0U));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 0U));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT(settings_set(SETTINGS_ID_SHOW_CW_CODE, 1U));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(reversed_record_v3, fake_iap_memory(), SETTINGS_RECORD_SIZE));
  /* Show CW Code off with Paddle Reversed, Prosigns Both and Ramp 6 ms is flags 0x6A. */
  TEST_ASSERT(settings_set(SETTINGS_ID_SHOW_CW_CODE, 0U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0x6AU, fake_iap_memory()[RECORD_FLAGS]);
}

/* AC-K50 */
static void test_record_vectors_load_and_save(void) {
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(reversed_record));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 0U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(default_record_v3, fake_iap_memory(), SETTINGS_RECORD_SIZE));

  /* A record of the earlier firmware has flags bit 1 = 0: Normal, and its CRC stays valid. */
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(default_record));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(reversed_record_v3, fake_iap_memory(), SETTINGS_RECORD_SIZE));
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
}

/* AC-K50: flags bits 2-7 of a version 1 record are ignored on load; bits 4-7 are written as the default ramp. */
static void test_upper_flag_bits_ignored(void) {
  uint8_t record[SETTINGS_RECORD_SIZE];

  memcpy(record, default_record, sizeof(record));
  record[RECORD_FLAGS] = 0xFEU;
  record[14] = crc8_compute(record, 14U);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT(settings_set(SETTINGS_ID_SHOW_CW_CODE, 1U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(reversed_record_v3, fake_iap_memory(), SETTINGS_RECORD_SIZE));

  /* A bad record gives defaults; its upper flag bits do not leak into the next save. */
  memcpy(record, default_record, sizeof(record));
  record[RECORD_FLAGS] = 0xFFU;
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_CRC, load(record));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(reversed_record_v3, fake_iap_memory(), SETTINGS_RECORD_SIZE));
}

/* AC-K50: a change and a change back cause no save. */
static void test_change_back_does_not_save(void) {
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(default_record));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT(settings_has_unsaved_changes());
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 0U));
  TEST_ASSERT(!settings_has_unsaved_changes());
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0U, write_operations());
}

int main(void) {
  TEST_RUN(test_record_vectors_have_valid_crc);
  TEST_RUN(test_paddle_values);
  TEST_RUN(test_flags_bits_are_independent);
  TEST_RUN(test_record_vectors_load_and_save);
  TEST_RUN(test_upper_flag_bits_ignored);
  TEST_RUN(test_change_back_does_not_save);
  return TEST_FINISH();
}
