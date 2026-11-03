/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of settings layout version 4 and the attempt ids (references/internal/menu_subsystem_spec.md section 19,
 * AC-M13 to AC-M17; training_spec.md D-T60, Q8 fault cases F8, F9 and mutants M11, M12). The oracle is the vector
 * table of 19.4, copied byte by byte, and a record parser written from 19.3: the CRC position follows the version,
 * byte 14 of a version 1 to 3 record is its CRC and gives both attempt ids 0, bits 2-7 of byte 14 never survive.
 * Fault injection: power loss, CMD_FAIL and a low supply at each operation of a version 4 save.
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
#define OFFSET_TRAINING 14U
#define OFFSET_CRC_V4 15U
#define OFFSET_CRC_V3 14U
#define ERASED 0xFFU
#define SAVE_OPERATIONS (1U + 2U * SETTINGS_RECORD_SIZE)
#define RX_ID SETTINGS_ID_RX_UNLIMITED
#define TX_ID SETTINGS_ID_TX_UNLIMITED

typedef struct {
  uint8_t bytes[16];
  settings_load_status_t status;
  uint8_t rx;
  uint8_t tx;
  uint8_t ramp;
  uint8_t show;
  uint8_t max_word;
} qa_vector_t;

/* menu_subsystem_spec.md section 19.4, in its order. */
static const qa_vector_t qa_vectors[] = {
    {{0x43, 0x57, 0x04, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x00, 0xD4},
     SETTINGS_LOAD_OK,
     0,
     0,
     6,
     1,
     5},
    {{0x43, 0x57, 0x04, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x01, 0xD3},
     SETTINGS_LOAD_OK,
     1,
     0,
     6,
     1,
     5},
    {{0x43, 0x57, 0x04, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x02, 0xDA},
     SETTINGS_LOAD_OK,
     0,
     1,
     6,
     1,
     5},
    {{0x43, 0x57, 0x04, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x03, 0xDD},
     SETTINGS_LOAD_OK,
     1,
     1,
     6,
     1,
     5},
    {{0x43, 0x57, 0x04, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0xFF, 0x27},
     SETTINGS_LOAD_OK,
     1,
     1,
     6,
     1,
     5},
    {{0x43, 0x57, 0x04, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0xFC, 0x2E},
     SETTINGS_LOAD_OK,
     0,
     0,
     6,
     1,
     5},
    {{0x43, 0x57, 0x04, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x9F, 0x01},
     SETTINGS_LOAD_BAD_CRC,
     0,
     0,
     6,
     1,
     5},
    {{0x43, 0x57, 0x04, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x9F, 0xFF},
     SETTINGS_LOAD_BAD_CRC,
     0,
     0,
     6,
     1,
     5},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x0B, 0xFF},
     SETTINGS_LOAD_OK,
     0,
     0,
     6,
     1,
     5},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x49, 0x05, 0x08, 0x20, 0xC5, 0xFF},
     SETTINGS_LOAD_OK,
     0,
     0,
     4,
     1,
     5},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x68, 0x08, 0x08, 0x20, 0x8C, 0xFF},
     SETTINGS_LOAD_OK,
     0,
     0,
     6,
     0,
     8},
    {{0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x0A, 0x07},
     SETTINGS_LOAD_BAD_CRC,
     0,
     0,
     6,
     1,
     5},
    {{0x43, 0x57, 0x02, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x09, 0x05, 0x08, 0x20, 0x03, 0xFF},
     SETTINGS_LOAD_OK,
     0,
     0,
     6,
     1,
     5},
    {{0x43, 0x57, 0x01, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x03, 0x05, 0x08, 0x20, 0x78, 0xFF},
     SETTINGS_LOAD_OK,
     0,
     0,
     6,
     1,
     5},
    {{0x43, 0x57, 0x05, 0x58, 0x02, 0x20, 0x20, 0x14, 0x14, 0x02, 0x69, 0x05, 0x08, 0x20, 0x00, 0x40},
     SETTINGS_LOAD_BAD_VERSION,
     0,
     0,
     6,
     1,
     5},
};
#define QA_VECTOR_COUNT (sizeof(qa_vectors) / sizeof(qa_vectors[0]))
#define V4_DEFAULTS 0U
#define V3_DEFAULTS 8U

static size_t write_operations(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

static settings_load_status_t load(const uint8_t* record) {
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, SETTINGS_RECORD_SIZE);
  return settings_load();
}

/* The training flags byte in RAM, read through a save after a tone edit (the save writes the RAM record). */
static uint8_t training_byte_in_ram(void) {
  const uint16_t tone = settings_get(SETTINGS_ID_TONE_HZ);
  (void)settings_set(SETTINGS_ID_TONE_HZ, (tone == 700U) ? 710U : 700U);
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(4U, fake_iap_memory()[OFFSET_VERSION]);
  TEST_ASSERT_EQUAL(crc8_compute(fake_iap_memory(), 15U), fake_iap_memory()[OFFSET_CRC_V4]);
  return fake_iap_memory()[OFFSET_TRAINING];
}

/* AC-M13 */
static void test_ids_and_ranges(void) {
  settings_id_t id;
  TEST_ASSERT_EQUAL(13U, RX_ID);
  TEST_ASSERT_EQUAL(14U, TX_ID);
  TEST_ASSERT_EQUAL(15U, SETTINGS_ID_COUNT);
  TEST_ASSERT_EQUAL(16U, SETTINGS_RECORD_SIZE);
  TEST_ASSERT_EQUAL(4U, SETTINGS_LAYOUT_VERSION);
  for (id = RX_ID; id <= TX_ID; ++id) {
    uint32_t value;
    TEST_ASSERT_EQUAL(0U, settings_min(id));
    TEST_ASSERT_EQUAL(1U, settings_max(id));
    TEST_ASSERT_EQUAL(1U, settings_step(id));
    fake_iap_reset();
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, settings_load());
    TEST_ASSERT_EQUAL(0U, settings_get(id));
    for (value = 0U; value <= 0xFFFFU; value = (value < 300U) ? value + 1U : value * 2U + 1U) {
      const uint16_t expected = (value == 0U) ? 0U : 1U;
      const uint16_t before = settings_get(id);
      TEST_ASSERT_EQUAL(expected != before, settings_set(id, (uint16_t)value));
      TEST_ASSERT_EQUAL(expected, settings_get(id));
    }
  }
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_COUNT));
  TEST_ASSERT(!settings_set(SETTINGS_ID_COUNT, 1U));
  TEST_ASSERT_EQUAL(0U, settings_min(SETTINGS_ID_COUNT));
  TEST_ASSERT_EQUAL(0U, settings_max(SETTINGS_ID_COUNT));
}

/* AC-M14, M11, M12: every vector of 19.4 gives its load result and values, and no load writes. */
static void test_vectors(void) {
  size_t i;
  for (i = 0U; i < QA_VECTOR_COUNT; ++i) {
    const qa_vector_t* vector = &qa_vectors[i];
    const settings_load_status_t status = load(vector->bytes);
    if (!TEST_ASSERT_EQUAL(vector->status, status)) {
      fprintf(stderr, "vector %zu\n", i);
    }
    TEST_ASSERT_EQUAL(vector->rx, settings_get(RX_ID));
    TEST_ASSERT_EQUAL(vector->tx, settings_get(TX_ID));
    TEST_ASSERT_EQUAL(vector->ramp, settings_get(SETTINGS_ID_RAMP_MS));
    TEST_ASSERT_EQUAL(vector->show, settings_get(SETTINGS_ID_SHOW_CW_CODE));
    TEST_ASSERT_EQUAL(vector->max_word, settings_get(SETTINGS_ID_MAX_WORD_LEN));
    TEST_ASSERT_EQUAL(600U, settings_get(SETTINGS_ID_TONE_HZ));
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT_EQUAL(0, memcmp(vector->bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));
    /* After every load path bits 2-7 of byte 14 are 0; the RAM byte holds exactly the two flags. */
    if (!TEST_ASSERT_EQUAL((uint8_t)(vector->rx | (vector->tx << 1)), training_byte_in_ram())) {
      fprintf(stderr, "vector %zu: training byte\n", i);
    }
  }
  /* The default save writes the version 4 default vector. */
  fake_iap_reset();
  (void)settings_load();
  (void)settings_set(SETTINGS_ID_TONE_HZ, 610U);
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  (void)settings_set(SETTINGS_ID_TONE_HZ, 600U);
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(qa_vectors[V4_DEFAULTS].bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));
}

/* M11: each valid record with the CRC copied to the position of the other layout gives BAD_CRC. */
static void test_crc_position_by_version(void) {
  unsigned version;
  for (version = 1U; version <= 4U; ++version) {
    unsigned trial;
    for (trial = 0U; trial < 256U; ++trial) {
      uint8_t record[16];
      memcpy(record, qa_vectors[V3_DEFAULTS].bytes, sizeof(record));
      record[OFFSET_VERSION] = (uint8_t)version;
      record[5] = (uint8_t)(5U + trial % 40U);
      record[13] = (uint8_t)trial;
      if (version == 4U) {
        record[OFFSET_TRAINING] = (uint8_t)trial;
        record[OFFSET_CRC_V4] = crc8_compute(record, 15U);
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
        TEST_ASSERT_EQUAL(trial & 1U, settings_get(RX_ID));
        TEST_ASSERT_EQUAL((trial >> 1) & 1U, settings_get(TX_ID));
        /* The CRC of 0..13 at byte 14 and a wrong byte 15: not a valid version 4 record. */
        record[OFFSET_CRC_V3] = crc8_compute(record, 14U);
        record[OFFSET_CRC_V4] = (uint8_t)~crc8_compute(record, 15U);
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_CRC, load(record));
      } else {
        record[OFFSET_CRC_V3] = crc8_compute(record, 14U);
        record[OFFSET_CRC_V4] = (uint8_t)trial; /* byte 15 is not read */
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
        TEST_ASSERT_EQUAL(0U, settings_get(RX_ID));
        TEST_ASSERT_EQUAL(0U, settings_get(TX_ID));
        TEST_ASSERT(!settings_has_unsaved_changes());
        /* A wrong byte 14 with a CRC over 0..14 at byte 15: not a valid old record. */
        record[OFFSET_CRC_V3] ^= 0x01U;
        record[OFFSET_CRC_V4] = crc8_compute(record, 15U);
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_CRC, load(record));
      }
      TEST_ASSERT_EQUAL(0U, write_operations());
    }
  }
}

/*
 * F8, AC-M15, AC-M17: a version 1, 2 or 3 record with every flags value loads with both attempt ids 0 and no unsaved
 * change; a Settings exit with no edit writes nothing. The first save writes byte 14 = 0 and the old values.
 */
static void test_old_versions_every_flags_value(void) {
  unsigned version;
  for (version = 1U; version <= 3U; ++version) {
    unsigned flags;
    for (flags = 0U; flags <= 0xFFU; ++flags) {
      uint8_t record[16];
      uint16_t show;
      uint16_t paddle;
      memcpy(record, qa_vectors[V3_DEFAULTS].bytes, sizeof(record));
      record[OFFSET_VERSION] = (uint8_t)version;
      record[OFFSET_FLAGS] = (uint8_t)flags;
      record[OFFSET_CRC_V3] = crc8_compute(record, 14U);
      TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
      TEST_ASSERT_EQUAL(0U, settings_get(RX_ID));
      TEST_ASSERT_EQUAL(0U, settings_get(TX_ID));
      TEST_ASSERT(!settings_has_unsaved_changes());
      TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
      TEST_ASSERT_EQUAL(0U, write_operations());
      show = settings_get(SETTINGS_ID_SHOW_CW_CODE);
      paddle = settings_get(SETTINGS_ID_PADDLE_SWAP);
      TEST_ASSERT_EQUAL(flags & 1U, show);
      TEST_ASSERT_EQUAL((flags >> 1) & 1U, paddle);
      TEST_ASSERT_EQUAL(0U, training_byte_in_ram());
      TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
      TEST_ASSERT_EQUAL(show, settings_get(SETTINGS_ID_SHOW_CW_CODE));
      TEST_ASSERT_EQUAL(paddle, settings_get(SETTINGS_ID_PADDLE_SWAP));
      TEST_ASSERT_EQUAL(0U, settings_get(RX_ID));
    }
  }
}

/* M12, AC-M17: every byte 14 of a valid version 4 record; every byte 14 of a bad record of each failure kind. */
static void test_training_byte_every_value(void) {
  unsigned value;
  for (value = 0U; value <= 0xFFU; ++value) {
    unsigned failure;
    uint8_t record[16];
    memcpy(record, qa_vectors[V4_DEFAULTS].bytes, sizeof(record));
    record[OFFSET_TRAINING] = (uint8_t)value;
    record[OFFSET_CRC_V4] = crc8_compute(record, 15U);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
    TEST_ASSERT_EQUAL(value & 1U, settings_get(RX_ID));
    TEST_ASSERT_EQUAL((value >> 1) & 1U, settings_get(TX_ID));
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT_EQUAL(value & 0x03U, training_byte_in_ram());
    for (failure = 0U; failure < 3U; ++failure) {
      memcpy(record, qa_vectors[V4_DEFAULTS].bytes, sizeof(record));
      record[OFFSET_TRAINING] = (uint8_t)value;
      record[OFFSET_CRC_V4] = crc8_compute(record, 15U);
      if (failure == 0U) {
        record[OFFSET_CRC_V4] ^= 0x5AU;
      } else if (failure == 1U) {
        record[OFFSET_VERSION] = 5U;
        record[OFFSET_CRC_V4] = crc8_compute(record, 15U);
      } else {
        record[0] = 0x00U;
      }
      TEST_ASSERT(load(record) != SETTINGS_LOAD_OK);
      TEST_ASSERT_EQUAL(0U, settings_get(RX_ID));
      TEST_ASSERT_EQUAL(0U, settings_get(TX_ID));
      if (!TEST_ASSERT_EQUAL(0U, training_byte_in_ram())) {
        fprintf(stderr, "byte 14 0x%02X failure %u\n", value, failure);
      }
    }
  }
}

/* AC-M16, AC-M17: round trip of the four flag values; the other bit and byte 10 stay; only bytes 0..15 of page 0. */
static void test_round_trip_and_independence(void) {
  unsigned rx;
  for (rx = 0U; rx <= 1U; ++rx) {
    unsigned tx;
    for (tx = 0U; tx <= 1U; ++tx) {
      uint8_t flags_before;
      uint16_t address;
      fake_iap_reset();
      (void)settings_load();
      (void)settings_set(SETTINGS_ID_PADDLE_SWAP, 1U);
      (void)settings_set(SETTINGS_ID_RAMP_MS, 11U);
      (void)settings_save_if_changed();
      flags_before = fake_iap_memory()[OFFSET_FLAGS];
      TEST_ASSERT_EQUAL((rx != 0U), settings_set(RX_ID, (uint16_t)rx));
      TEST_ASSERT_EQUAL(0U, settings_get(TX_ID));
      TEST_ASSERT_EQUAL(tx != 0U, settings_set(TX_ID, (uint16_t)tx));
      TEST_ASSERT_EQUAL(rx, settings_get(RX_ID));
      TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
      TEST_ASSERT_EQUAL(11U, settings_get(SETTINGS_ID_RAMP_MS));
      fake_iap_clear_counters();
      if ((rx | tx) == 0U) {
        TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
        TEST_ASSERT_EQUAL(0U, write_operations());
        continue;
      }
      TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
      TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
      TEST_ASSERT_EQUAL(1U, fake_iap_command_count(CMD_ERASE));
      for (address = 0U; address < FAKE_IAP_PAGE_SIZE; ++address) {
        TEST_ASSERT_EQUAL((address < SETTINGS_RECORD_SIZE) ? 1U : 0U, fake_iap_program_count(address));
      }
      TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(CMD_PROGRAM));
      TEST_ASSERT_EQUAL((uint8_t)(rx | (tx << 1)), fake_iap_memory()[OFFSET_TRAINING]);
      TEST_ASSERT_EQUAL(flags_before, fake_iap_memory()[OFFSET_FLAGS]);
      TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
      TEST_ASSERT_EQUAL(rx, settings_get(RX_ID));
      TEST_ASSERT_EQUAL(tx, settings_get(TX_ID));
      TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
      TEST_ASSERT_EQUAL(11U, settings_get(SETTINGS_ID_RAMP_MS));
      /* Each byte 10 flag id keeps byte 14. */
      (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, 0U);
      (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, 0U);
      (void)settings_set(SETTINGS_ID_RAMP_MS, 4U);
      (void)settings_set(SETTINGS_ID_PADDLE_SWAP, 0U);
      TEST_ASSERT_EQUAL(rx, settings_get(RX_ID));
      TEST_ASSERT_EQUAL(tx, settings_get(TX_ID));
      TEST_ASSERT_EQUAL((uint8_t)(rx | (tx << 1)), training_byte_in_ram());
    }
  }
}

typedef enum { FAULT_POWER_LOSS, FAULT_CMD_FAIL, FAULT_SUPPLY_LOW } qa_fault_t;

/*
 * F9: each fault at each operation of the save that sets both attempt ids, after a version 3 load and after a
 * version 4 load. The next load gives the old values, the new values or the defaults, never a mix, and never writes.
 */
static void test_faults_in_version_4_save(void) {
  static const qa_fault_t faults[3] = {FAULT_POWER_LOSS, FAULT_CMD_FAIL, FAULT_SUPPLY_LOW};
  unsigned long outcomes[3] = {0U, 0U, 0U};
  unsigned base;
  for (base = 0U; base < 2U; ++base) {
    uint8_t old_record[16];
    size_t f;
    memcpy(old_record, qa_vectors[(base == 0U) ? V3_DEFAULTS : V4_DEFAULTS].bytes, sizeof(old_record));
    old_record[3] = 0x84U; /* 900 Hz */
    old_record[4] = 0x03U;
    if (base == 0U) {
      old_record[OFFSET_CRC_V3] = crc8_compute(old_record, 14U);
    } else {
      old_record[OFFSET_TRAINING] = 0x02U; /* TX unlimited */
      old_record[OFFSET_CRC_V4] = crc8_compute(old_record, 15U);
    }
    for (f = 0U; f < 3U; ++f) {
      size_t point;
      for (point = 1U; point <= SAVE_OPERATIONS; ++point) {
        const uint16_t old_tx = (base == 0U) ? 0U : 1U;
        uint16_t rx;
        uint16_t tx;
        uint16_t tone;
        TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(old_record));
        TEST_ASSERT_EQUAL(old_tx, settings_get(TX_ID));
        (void)settings_set(RX_ID, 1U);
        (void)settings_set(TX_ID, 1U - old_tx);
        (void)settings_set(SETTINGS_ID_TONE_HZ, 700U);
        fake_iap_clear_counters();
        if (faults[f] == FAULT_POWER_LOSS) {
          fake_iap_set_power_loss_after(point);
        } else if (faults[f] == FAULT_CMD_FAIL) {
          fake_iap_set_fail_operation(point);
        } else {
          fake_iap_set_supply_low_from(point);
        }
        (void)settings_save_if_changed();
        fake_iap_clear_counters();
        (void)settings_load();
        TEST_ASSERT_EQUAL(0U, write_operations());
        rx = settings_get(RX_ID);
        tx = settings_get(TX_ID);
        tone = settings_get(SETTINGS_ID_TONE_HZ);
        if ((rx == 0U) && (tx == old_tx) && (tone == 900U)) {
          ++outcomes[0];
        } else if ((rx == 1U) && (tx == 1U - old_tx) && (tone == 700U)) {
          ++outcomes[1];
        } else if ((rx == 0U) && (tx == 0U) && (tone == 600U)) {
          ++outcomes[2];
        } else {
          TEST_ASSERT(false);
          fprintf(stderr, "mix: base %u fault %zu point %zu\n", base, f, point);
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
  TEST_RUN(test_ids_and_ranges);
  TEST_RUN(test_vectors);
  TEST_RUN(test_crc_position_by_version);
  TEST_RUN(test_old_versions_every_flags_value);
  TEST_RUN(test_training_byte_every_value);
  TEST_RUN(test_round_trip_and_independence);
  TEST_RUN(test_faults_in_version_4_save);
  return TEST_FINISH();
}
