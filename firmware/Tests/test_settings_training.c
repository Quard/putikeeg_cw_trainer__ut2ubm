/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Host tests of the training flags in record byte 14 and the settings layout version 4
 * (references/internal/menu_subsystem_spec.md section 19, AC-M13 to AC-M17).
 */

#include <stdbool.h>
#include <string.h>

#include "eeprom/eeprom.h"
#include "fake_iap.h"
#include "settings/settings.h"
#include "support/test_assert.h"
#include "util/crc8.h"

#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define RECORD_VERSION 2U
#define RECORD_FLAGS 10U
#define RECORD_TRAINING_FLAGS 14U
#define RECORD_CRC_V3 14U
#define RECORD_CRC 15U

typedef struct {
  uint8_t bytes[SETTINGS_RECORD_SIZE];
  settings_load_status_t status;
  uint16_t rx_unlimited;
  uint16_t tx_unlimited;
  uint16_t ramp_ms;
  uint8_t training_flags; /* byte 14 in RAM after the load */
} vector_t;

/* menu_subsystem_spec.md section 19.4. */
static const vector_t vectors[] = {
    {{0x43U, 0x57U, 0x04U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x00U, 0xD4U},
     SETTINGS_LOAD_OK,
     0U,
     0U,
     6U,
     0x00U},
    {{0x43U, 0x57U, 0x04U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x01U, 0xD3U},
     SETTINGS_LOAD_OK,
     1U,
     0U,
     6U,
     0x01U},
    {{0x43U, 0x57U, 0x04U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x02U, 0xDAU},
     SETTINGS_LOAD_OK,
     0U,
     1U,
     6U,
     0x02U},
    {{0x43U, 0x57U, 0x04U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x03U, 0xDDU},
     SETTINGS_LOAD_OK,
     1U,
     1U,
     6U,
     0x03U},
    {{0x43U, 0x57U, 0x04U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0xFFU, 0x27U},
     SETTINGS_LOAD_OK,
     1U,
     1U,
     6U,
     0x03U},
    {{0x43U, 0x57U, 0x04U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0xFCU, 0x2EU},
     SETTINGS_LOAD_OK,
     0U,
     0U,
     6U,
     0x00U},
    {{0x43U, 0x57U, 0x04U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x9FU, 0x01U},
     SETTINGS_LOAD_BAD_CRC,
     0U,
     0U,
     6U,
     0x00U},
    {{0x43U, 0x57U, 0x04U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x9FU, 0xFFU},
     SETTINGS_LOAD_BAD_CRC,
     0U,
     0U,
     6U,
     0x00U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x0BU, 0xFFU},
     SETTINGS_LOAD_OK,
     0U,
     0U,
     6U,
     0x00U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x49U, 0x05U, 0x08U, 0x20U, 0xC5U, 0xFFU},
     SETTINGS_LOAD_OK,
     0U,
     0U,
     4U,
     0x00U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x68U, 0x08U, 0x08U, 0x20U, 0x8CU, 0xFFU},
     SETTINGS_LOAD_OK,
     0U,
     0U,
     6U,
     0x00U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x0AU, 0x07U},
     SETTINGS_LOAD_BAD_CRC,
     0U,
     0U,
     6U,
     0x00U},
    {{0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x03U, 0xFFU},
     SETTINGS_LOAD_OK,
     0U,
     0U,
     6U,
     0x00U},
    {{0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x03U, 0x05U, 0x08U, 0x20U, 0x78U, 0xFFU},
     SETTINGS_LOAD_OK,
     0U,
     0U,
     6U,
     0x00U},
    {{0x43U, 0x57U, 0x05U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x00U, 0x40U},
     SETTINGS_LOAD_BAD_VERSION,
     0U,
     0U,
     6U,
     0x00U},
};

#define VECTOR_COUNT (sizeof(vectors) / sizeof(vectors[0]))
#define DEFAULT_VECTOR 0U
#define V3_SHOW_CW_OFF_VECTOR 10U
#define V1_REVERSED_VECTOR 13U

static size_t write_operations(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

static settings_load_status_t load(const uint8_t* record) {
  settings_load_status_t status;
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, SETTINGS_RECORD_SIZE);
  status = settings_load();
  fake_iap_clear_counters();
  return status;
}

static bool training_flags_are(uint16_t rx_unlimited, uint16_t tx_unlimited) {
  return (settings_get(SETTINGS_ID_RX_UNLIMITED) == rx_unlimited) &&
         (settings_get(SETTINGS_ID_TX_UNLIMITED) == tx_unlimited);
}

/* Byte 14 in RAM, read through a save forced by a tone edit. The tone goes back after the check. */
static uint8_t saved_training_flags(void) {
  const uint16_t tone = settings_get(SETTINGS_ID_TONE_HZ);
  const uint16_t other_tone = (tone == 600U) ? 610U : 600U;
  uint8_t flags;

  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, other_tone));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  flags = fake_iap_memory()[RECORD_TRAINING_FLAGS];
  TEST_ASSERT_EQUAL(SETTINGS_LAYOUT_VERSION, fake_iap_memory()[RECORD_VERSION]);
  TEST_ASSERT_EQUAL(crc8_compute(fake_iap_memory(), RECORD_CRC), fake_iap_memory()[RECORD_CRC]);
  (void)settings_set(SETTINGS_ID_TONE_HZ, tone);
  return flags;
}

static void test_vectors_have_valid_crc(void) {
  size_t index;
  TEST_ASSERT_EQUAL(4U, SETTINGS_LAYOUT_VERSION);
  TEST_ASSERT_EQUAL(16U, SETTINGS_RECORD_SIZE);
  for (index = 0U; index < VECTOR_COUNT; ++index) {
    const uint8_t* bytes = vectors[index].bytes;
    const uint8_t crc_offset = (bytes[RECORD_VERSION] >= 4U) ? RECORD_CRC : RECORD_CRC_V3;
    const bool is_crc_valid = crc8_compute(bytes, crc_offset) == bytes[crc_offset];
    if (!TEST_ASSERT_EQUAL(vectors[index].status != SETTINGS_LOAD_BAD_CRC, is_crc_valid)) {
      fprintf(stderr, "vector %zu\n", index);
    }
  }
}

/* AC-M13 */
static void test_ranges_and_clamp(void) {
  static const settings_id_t ids[2] = {SETTINGS_ID_RX_UNLIMITED, SETTINGS_ID_TX_UNLIMITED};
  static const uint16_t high[] = {2U, 3U, 255U, 256U, 32768U, 65535U};
  size_t id_index;
  size_t index;

  TEST_ASSERT_EQUAL(13U, SETTINGS_ID_RX_UNLIMITED);
  TEST_ASSERT_EQUAL(14U, SETTINGS_ID_TX_UNLIMITED);
  TEST_ASSERT_EQUAL(15U, SETTINGS_ID_COUNT);
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_COUNT));
  TEST_ASSERT_EQUAL(0U, settings_min(SETTINGS_ID_COUNT));
  TEST_ASSERT_EQUAL(0U, settings_max(SETTINGS_ID_COUNT));
  TEST_ASSERT_EQUAL(0U, settings_step(SETTINGS_ID_COUNT));
  TEST_ASSERT(!settings_set(SETTINGS_ID_COUNT, 1U));
  for (id_index = 0U; id_index < 2U; ++id_index) {
    const settings_id_t id = ids[id_index];
    fake_iap_reset();
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, settings_load());
    TEST_ASSERT_EQUAL(0U, settings_min(id));
    TEST_ASSERT_EQUAL(1U, settings_max(id));
    TEST_ASSERT_EQUAL(1U, settings_step(id));
    TEST_ASSERT_EQUAL(0U, settings_get(id));
    TEST_ASSERT(!settings_set(id, 0U));
    TEST_ASSERT(!settings_is_edited());
    for (index = 0U; index < sizeof(high) / sizeof(high[0]); ++index) {
      (void)settings_set(id, 0U);
      TEST_ASSERT(settings_set(id, high[index]));
      TEST_ASSERT_EQUAL(1U, settings_get(id));
      TEST_ASSERT(!settings_set(id, high[index]));
    }
    TEST_ASSERT(settings_is_edited());
  }
}

/* AC-M14, AC-M15: each vector gives its load result and values; a load never writes and leaves no unsaved change. */
static void test_vectors_load(void) {
  size_t index;
  for (index = 0U; index < VECTOR_COUNT; ++index) {
    const vector_t* vector = &vectors[index];
    TEST_ASSERT_EQUAL(vector->status, load(vector->bytes));
    if (!TEST_ASSERT(training_flags_are(vector->rx_unlimited, vector->tx_unlimited))) {
      fprintf(stderr, "vector %zu\n", index);
    }
    TEST_ASSERT_EQUAL(vector->ramp_ms, settings_get(SETTINGS_ID_RAMP_MS));
    TEST_ASSERT_EQUAL(600U, settings_get(SETTINGS_ID_TONE_HZ));
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT(!settings_is_edited());
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
    TEST_ASSERT_EQUAL(0, memcmp(vector->bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));
    if (!TEST_ASSERT_EQUAL(vector->training_flags, saved_training_flags())) {
      fprintf(stderr, "vector %zu\n", index);
    }
  }
  /* Show CW Code off and Max Word Length 8 of the version 3 vector stay. */
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[V3_SHOW_CW_OFF_VECTOR].bytes));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(8U, settings_get(SETTINGS_ID_MAX_WORD_LEN));
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[V1_REVERSED_VECTOR].bytes));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, settings_get(SETTINGS_ID_PROSIGN_STYLE));
}

/*
 * AC-M15, Q8 F8: a version 1 to 3 record loads with both training flags 0 for every flags byte value, whatever
 * byte 15 holds, and an exit with no edit step writes nothing.
 */
static void test_older_versions_every_flags_value(void) {
  static const uint8_t byte_15_values[3] = {0xFFU, 0x03U, 0x00U};
  unsigned version;
  unsigned flags;
  size_t byte_15;

  for (version = 1U; version <= 3U; ++version) {
    for (flags = 0U; flags <= 255U; ++flags) {
      for (byte_15 = 0U; byte_15 < 3U; ++byte_15) {
        uint8_t record[SETTINGS_RECORD_SIZE];
        memcpy(record, vectors[DEFAULT_VECTOR].bytes, sizeof(record));
        record[RECORD_VERSION] = (uint8_t)version;
        record[RECORD_FLAGS] = (uint8_t)flags;
        record[RECORD_CRC_V3] = crc8_compute(record, RECORD_CRC_V3);
        record[RECORD_CRC] = byte_15_values[byte_15];
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
        TEST_ASSERT(training_flags_are(0U, 0U));
        TEST_ASSERT(!settings_has_unsaved_changes());
        TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
        TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
      }
    }
  }
}

/* AC-M14: a CRC at the wrong position for the version fails. */
static void test_crc_position_follows_version(void) {
  uint8_t record[SETTINGS_RECORD_SIZE];

  /* Version 3 with a CRC over 0..14 at byte 15 only. */
  memcpy(record, vectors[DEFAULT_VECTOR].bytes, sizeof(record));
  record[RECORD_VERSION] = 3U;
  record[RECORD_CRC] = crc8_compute(record, RECORD_CRC);
  TEST_ASSERT(crc8_compute(record, RECORD_CRC_V3) != record[RECORD_CRC_V3]);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_CRC, load(record));
  TEST_ASSERT(training_flags_are(0U, 0U));
  /* Version 4 with byte 15 flipped. */
  memcpy(record, vectors[3].bytes, sizeof(record));
  record[RECORD_CRC] ^= 0x01U;
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_CRC, load(record));
  TEST_ASSERT(training_flags_are(0U, 0U));
}

/* AC-M16: round trip of the four flag values; each save writes the version 4 vector and only bytes 0x000-0x00F. */
static void test_round_trip(void) {
  uint16_t value;
  size_t address;

  for (value = 0U; value < 4U; ++value) {
    const uint16_t rx_unlimited = (uint16_t)(value & 1U);
    const uint16_t tx_unlimited = (uint16_t)(value >> 1);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[DEFAULT_VECTOR].bytes));
    (void)settings_set(SETTINGS_ID_RX_UNLIMITED, rx_unlimited);
    (void)settings_set(SETTINGS_ID_TX_UNLIMITED, tx_unlimited);
    TEST_ASSERT_EQUAL(value != 0U, settings_has_unsaved_changes());
    if (value == 0U) {
      TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
      continue;
    }
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0, memcmp(vectors[value].bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));
    TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
    for (address = 0U; address < FAKE_IAP_SIZE; ++address) {
      TEST_ASSERT_EQUAL(address < SETTINGS_RECORD_SIZE ? 1U : 0U, fake_iap_program_count((uint16_t)address));
    }
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
    TEST_ASSERT(training_flags_are(rx_unlimited, tx_unlimited));
    TEST_ASSERT(!settings_has_unsaved_changes());
  }

  /* The default save writes the version 4 default vector. */
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[3].bytes));
  TEST_ASSERT(settings_set(SETTINGS_ID_RX_UNLIMITED, 0U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TX_UNLIMITED, 0U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(vectors[DEFAULT_VECTOR].bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));

  /* An edit of a training flag and back gives no save. */
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[DEFAULT_VECTOR].bytes));
  TEST_ASSERT(settings_set(SETTINGS_ID_TX_UNLIMITED, 1U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TX_UNLIMITED, 0U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0U, write_operations());

  /* A save after a version 3 load writes version 4 with the training flags. */
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[8].bytes));
  TEST_ASSERT(settings_set(SETTINGS_ID_RX_UNLIMITED, 1U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(vectors[1].bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));
}

/* AC-M16, Q8 F9: a power loss after each operation of a version 4 save never leaves a valid record with wrong data. */
static void test_power_loss_at_each_operation(void) {
  size_t point;
  size_t total_operations;

  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[1].bytes));
  TEST_ASSERT(settings_set(SETTINGS_ID_RX_UNLIMITED, 0U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TX_UNLIMITED, 1U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  total_operations = fake_iap_operation_count();
  TEST_ASSERT_EQUAL(1U + 2U * SETTINGS_RECORD_SIZE, total_operations);

  for (point = 1U; point <= total_operations; ++point) {
    settings_load_status_t status;
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[1].bytes));
    (void)settings_set(SETTINGS_ID_RX_UNLIMITED, 0U);
    (void)settings_set(SETTINGS_ID_TX_UNLIMITED, 1U);
    fake_iap_set_power_loss_after(point);
    (void)settings_save_if_changed();
    fake_iap_clear_counters();
    status = settings_load();
    if (status == SETTINGS_LOAD_OK) {
      TEST_ASSERT(training_flags_are(1U, 0U) || training_flags_are(0U, 1U));
    } else {
      TEST_ASSERT(training_flags_are(0U, 0U));
    }
    if (point == total_operations) {
      TEST_ASSERT(training_flags_are(0U, 1U));
    }
  }
}

/* AC-M17: the training ids and the byte 10 ids do not change each other. */
static void test_flag_independence(void) {
  static const settings_id_t flag_ids[4] = {SETTINGS_ID_SHOW_CW_CODE, SETTINGS_ID_PADDLE_SWAP,
                                            SETTINGS_ID_PROSIGN_STYLE, SETTINGS_ID_RAMP_MS};
  static const uint16_t flag_values[4][3] = {{0U, 1U, 0U}, {1U, 0U, 1U}, {0U, 1U, 2U}, {4U, 12U, 9U}};
  uint16_t value;
  size_t id;
  size_t step;

  for (value = 0U; value < 4U; ++value) {
    const uint16_t rx_unlimited = (uint16_t)(value & 1U);
    const uint16_t tx_unlimited = (uint16_t)(value >> 1);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[value].bytes));
    for (id = 0U; id < 4U; ++id) {
      for (step = 0U; step < 3U; ++step) {
        (void)settings_set(flag_ids[id], flag_values[id][step]);
        TEST_ASSERT_EQUAL(flag_values[id][step], settings_get(flag_ids[id]));
        TEST_ASSERT(training_flags_are(rx_unlimited, tx_unlimited));
      }
    }
    /* Show CW Code 0, Paddle 1, Prosigns 2, Ramp 9: byte 10 is 0x9A. */
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0x9AU, fake_iap_memory()[RECORD_FLAGS]);
    TEST_ASSERT_EQUAL(value, fake_iap_memory()[RECORD_TRAINING_FLAGS]);

    TEST_ASSERT(settings_set(SETTINGS_ID_RX_UNLIMITED, (uint16_t)(1U - rx_unlimited)));
    TEST_ASSERT(training_flags_are((uint16_t)(1U - rx_unlimited), tx_unlimited));
    TEST_ASSERT(settings_set(SETTINGS_ID_TX_UNLIMITED, (uint16_t)(1U - tx_unlimited)));
    TEST_ASSERT(training_flags_are((uint16_t)(1U - rx_unlimited), (uint16_t)(1U - tx_unlimited)));
    TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
    TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
    TEST_ASSERT_EQUAL(2U, settings_get(SETTINGS_ID_PROSIGN_STYLE));
    TEST_ASSERT_EQUAL(9U, settings_get(SETTINGS_ID_RAMP_MS));
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0x9AU, fake_iap_memory()[RECORD_FLAGS]);
    TEST_ASSERT_EQUAL(3U - value, fake_iap_memory()[RECORD_TRAINING_FLAGS]);
  }
}

/* AC-M17: after every load path, bits 2-7 of byte 14 are 0. */
static void test_upper_training_bits_cleared(void) {
  unsigned flags;
  uint8_t record[SETTINGS_RECORD_SIZE];

  for (flags = 0U; flags <= 255U; ++flags) {
    memcpy(record, vectors[DEFAULT_VECTOR].bytes, sizeof(record));
    record[RECORD_TRAINING_FLAGS] = (uint8_t)flags;
    record[RECORD_CRC] = crc8_compute(record, RECORD_CRC);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
    TEST_ASSERT(training_flags_are(flags & 1U, (flags >> 1) & 1U));
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT_EQUAL(flags & 0x03U, saved_training_flags());

    /* The same record with a bad CRC gives the defaults and no bit of byte 14. */
    record[RECORD_CRC] ^= 0x01U;
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_CRC, load(record));
    TEST_ASSERT(training_flags_are(0U, 0U));
    TEST_ASSERT_EQUAL(0U, saved_training_flags());
  }

  /* A failed read and an erased page give byte 14 = 0 too. */
  fake_iap_reset();
  memcpy(fake_iap_memory(), vectors[4].bytes, SETTINGS_RECORD_SIZE);
  fake_iap_set_fail_operation(15U);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_READ_FAILED, settings_load());
  fake_iap_clear_counters();
  TEST_ASSERT(training_flags_are(0U, 0U));
  TEST_ASSERT_EQUAL(0U, saved_training_flags());

  memcpy(record, vectors[4].bytes, sizeof(record));
  record[0] = 0xFFU;
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, load(record));
  TEST_ASSERT(training_flags_are(0U, 0U));
  TEST_ASSERT_EQUAL(0U, saved_training_flags());
}

int main(void) {
  TEST_RUN(test_vectors_have_valid_crc);
  TEST_RUN(test_ranges_and_clamp);
  TEST_RUN(test_vectors_load);
  TEST_RUN(test_older_versions_every_flags_value);
  TEST_RUN(test_crc_position_follows_version);
  TEST_RUN(test_round_trip);
  TEST_RUN(test_power_loss_at_each_operation);
  TEST_RUN(test_flag_independence);
  TEST_RUN(test_upper_training_bits_cleared);
  return TEST_FINISH();
}
