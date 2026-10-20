/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Host tests of the Ramp setting in flags bits 4-7 and the settings layout version 3
 * (references/internal/menu_subsystem_spec.md sections 5.1, 5.2 and 18, AC-M1 to AC-M4).
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
#define RECORD_CRC 14U

typedef struct {
  uint8_t bytes[SETTINGS_RECORD_SIZE];
  settings_load_status_t status;
  uint16_t ramp_ms;
  uint16_t show_cw_code;
  uint16_t paddle;
  uint16_t prosigns;
} vector_t;

/* menu_subsystem_spec.md section 18.2. */
static const vector_t vectors[] = {
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x0BU},
     SETTINGS_LOAD_OK,
     6U,
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x49U, 0x05U, 0x08U, 0x20U, 0xC5U},
     SETTINGS_LOAD_OK,
     4U,
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x89U, 0x05U, 0x08U, 0x20U, 0x6FU},
     SETTINGS_LOAD_OK,
     8U,
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0xC9U, 0x05U, 0x08U, 0x20U, 0xF4U},
     SETTINGS_LOAD_OK,
     12U,
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x6BU, 0x05U, 0x08U, 0x20U, 0x27U},
     SETTINGS_LOAD_OK,
     6U,
     1U,
     1U,
     2U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x61U, 0x05U, 0x08U, 0x20U, 0xBBU},
     SETTINGS_LOAD_OK,
     6U,
     1U,
     0U,
     0U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x6FU, 0x05U, 0x08U, 0x20U, 0x7FU},
     SETTINGS_LOAD_OK,
     6U,
     1U,
     1U,
     2U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x5EU},
     SETTINGS_LOAD_OK,
     4U,
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0xF9U, 0x05U, 0x08U, 0x20U, 0x5DU},
     SETTINGS_LOAD_OK,
     12U,
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x03U},
     SETTINGS_LOAD_OK,
     6U,
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0xF9U, 0x05U, 0x08U, 0x20U, 0x00U},
     SETTINGS_LOAD_OK,
     6U,
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x03U, 0x05U, 0x08U, 0x20U, 0x78U},
     SETTINGS_LOAD_OK,
     6U,
     1U,
     1U,
     2U},
    {{0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0xF3U, 0x05U, 0x08U, 0x20U, 0x7BU},
     SETTINGS_LOAD_OK,
     6U,
     1U,
     1U,
     2U},
    {{0x43U, 0x57U, 0x04U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x9FU},
     SETTINGS_LOAD_BAD_VERSION,
     6U,
     1U,
     0U,
     2U},
};

#define VECTOR_COUNT (sizeof(vectors) / sizeof(vectors[0]))
#define DEFAULT_VECTOR 0U

static size_t write_operations(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

static settings_load_status_t load(const uint8_t* record) {
  settings_load_status_t status;
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, SETTINGS_RECORD_SIZE);
  status = settings_load();
  fake_iap_clear_counters();
  return status;
}

static bool flags_are(uint16_t ramp_ms, uint16_t show_cw_code, uint16_t paddle, uint16_t prosigns) {
  return (settings_get(SETTINGS_ID_RAMP_MS) == ramp_ms) && (settings_get(SETTINGS_ID_SHOW_CW_CODE) == show_cw_code) &&
         (settings_get(SETTINGS_ID_PADDLE_SWAP) == paddle) && (settings_get(SETTINGS_ID_PROSIGN_STYLE) == prosigns);
}

static void test_vectors_have_valid_crc(void) {
  size_t index;
  for (index = 0U; index < VECTOR_COUNT; ++index) {
    TEST_ASSERT_EQUAL(vectors[index].bytes[RECORD_CRC], crc8_compute(vectors[index].bytes, RECORD_CRC));
  }
  TEST_ASSERT_EQUAL(3U, SETTINGS_LAYOUT_VERSION);
  TEST_ASSERT_EQUAL(12U, SETTINGS_ID_RAMP_MS);
  TEST_ASSERT_EQUAL(13U, SETTINGS_ID_COUNT);
}

/* AC-M1 */
static void test_ramp_range_and_clamp(void) {
  static const uint16_t low[] = {0U, 1U, 3U};
  static const uint16_t high[] = {13U, 15U, 16U, 100U, 255U, 256U, 65535U};
  size_t index;

  fake_iap_reset();
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, settings_load());
  TEST_ASSERT_EQUAL(4U, settings_min(SETTINGS_ID_RAMP_MS));
  TEST_ASSERT_EQUAL(12U, settings_max(SETTINGS_ID_RAMP_MS));
  TEST_ASSERT_EQUAL(1U, settings_step(SETTINGS_ID_RAMP_MS));
  TEST_ASSERT_EQUAL(6U, settings_get(SETTINGS_ID_RAMP_MS));
  TEST_ASSERT(!settings_set(SETTINGS_ID_RAMP_MS, 6U));
  TEST_ASSERT(!settings_is_edited());
  for (index = 0U; index < sizeof(low) / sizeof(low[0]); ++index) {
    (void)settings_set(SETTINGS_ID_RAMP_MS, 6U);
    TEST_ASSERT(settings_set(SETTINGS_ID_RAMP_MS, low[index]));
    TEST_ASSERT_EQUAL(4U, settings_get(SETTINGS_ID_RAMP_MS));
  }
  for (index = 0U; index < sizeof(high) / sizeof(high[0]); ++index) {
    TEST_ASSERT(settings_set(SETTINGS_ID_RAMP_MS, 6U));
    TEST_ASSERT(settings_set(SETTINGS_ID_RAMP_MS, high[index]));
    TEST_ASSERT_EQUAL(12U, settings_get(SETTINGS_ID_RAMP_MS));
  }
  for (index = 4U; index <= 12U; ++index) {
    (void)settings_set(SETTINGS_ID_RAMP_MS, (uint16_t)index);
    TEST_ASSERT_EQUAL(index, settings_get(SETTINGS_ID_RAMP_MS));
  }
}

/* AC-M2: every vector gives its load result; an older record loads with no unsaved change and no save. */
static void test_vectors_load(void) {
  size_t index;
  for (index = 0U; index < VECTOR_COUNT; ++index) {
    const vector_t* vector = &vectors[index];
    TEST_ASSERT_EQUAL(vector->status, load(vector->bytes));
    if (!TEST_ASSERT(flags_are(vector->ramp_ms, vector->show_cw_code, vector->paddle, vector->prosigns))) {
      fprintf(stderr, "vector %zu\n", index);
    }
    TEST_ASSERT_EQUAL(600U, settings_get(SETTINGS_ID_TONE_HZ));
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT(!settings_is_edited());
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
    TEST_ASSERT_EQUAL(vector->bytes[RECORD_VERSION], fake_iap_memory()[RECORD_VERSION]);
  }
}

/* AC-M2: bits 4-7 of a version 1 or 2 record have no meaning, for every flags value. */
static void test_older_versions_every_flags_value(void) {
  unsigned version;
  unsigned flags;
  for (version = 1U; version <= 2U; ++version) {
    for (flags = 0U; flags <= 255U; ++flags) {
      uint8_t record[SETTINGS_RECORD_SIZE];
      memcpy(record, vectors[DEFAULT_VECTOR].bytes, sizeof(record));
      record[RECORD_VERSION] = (uint8_t)version;
      record[RECORD_FLAGS] = (uint8_t)flags;
      record[RECORD_CRC] = crc8_compute(record, RECORD_CRC);
      TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
      TEST_ASSERT_EQUAL(6U, settings_get(SETTINGS_ID_RAMP_MS));
      TEST_ASSERT(!settings_has_unsaved_changes());
    }
  }
}

/* AC-M3: all 9 ramp values survive a save and a load; every save writes version 3. */
static void test_round_trip(void) {
  uint16_t ramp_ms;
  for (ramp_ms = 4U; ramp_ms <= 12U; ++ramp_ms) {
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[DEFAULT_VECTOR].bytes));
    TEST_ASSERT_EQUAL(ramp_ms != 6U, settings_set(SETTINGS_ID_RAMP_MS, ramp_ms));
    TEST_ASSERT_EQUAL((ramp_ms != 6U) ? SETTINGS_SAVE_OK : SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    if (ramp_ms != 6U) {
      TEST_ASSERT_EQUAL(3U, fake_iap_memory()[RECORD_VERSION]);
      TEST_ASSERT_EQUAL((uint8_t)((ramp_ms << 4) | 0x09U), fake_iap_memory()[RECORD_FLAGS]);
      TEST_ASSERT_EQUAL(crc8_compute(fake_iap_memory(), RECORD_CRC), fake_iap_memory()[RECORD_CRC]);
    }
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
    TEST_ASSERT_EQUAL(ramp_ms, settings_get(SETTINGS_ID_RAMP_MS));
  }
  /* A save of the default values writes the default vector. */
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[2].bytes));
  TEST_ASSERT(settings_set(SETTINGS_ID_RAMP_MS, 6U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(vectors[DEFAULT_VECTOR].bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));
  /* A save after a version 2 load writes version 3 with the default ramp. */
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[9].bytes));
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 610U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 600U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0U, write_operations());
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0, memcmp(vectors[4].bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));
}

/* AC-M4: a Ramp change keeps bits 0-3, and a change of each other flag id keeps bits 4-7. */
static void test_flag_independence(void) {
  static const settings_id_t other_ids[3] = {SETTINGS_ID_SHOW_CW_CODE, SETTINGS_ID_PADDLE_SWAP,
                                             SETTINGS_ID_PROSIGN_STYLE};
  static const uint16_t other_values[3][3] = {{0U, 1U, 1U}, {0U, 1U, 1U}, {0U, 1U, 2U}};
  uint16_t ramp_ms;
  size_t id;
  size_t value;

  for (ramp_ms = 4U; ramp_ms <= 12U; ++ramp_ms) {
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vectors[6].bytes)); /* Reversed, Both, bits 2-3 were 3 */
    (void)settings_set(SETTINGS_ID_RAMP_MS, ramp_ms);
    TEST_ASSERT(flags_are(ramp_ms, 1U, 1U, 2U));
    for (id = 0U; id < 3U; ++id) {
      for (value = 0U; value < 3U; ++value) {
        (void)settings_set(other_ids[id], other_values[id][value]);
        TEST_ASSERT_EQUAL(other_values[id][value], settings_get(other_ids[id]));
        TEST_ASSERT_EQUAL(ramp_ms, settings_get(SETTINGS_ID_RAMP_MS));
      }
    }
    TEST_ASSERT(flags_are(ramp_ms, 1U, 1U, 2U));
    TEST_ASSERT_EQUAL(ramp_ms != 6U, settings_has_unsaved_changes());
  }
}

int main(void) {
  TEST_RUN(test_vectors_have_valid_crc);
  TEST_RUN(test_ramp_range_and_clamp);
  TEST_RUN(test_vectors_load);
  TEST_RUN(test_older_versions_every_flags_value);
  TEST_RUN(test_round_trip);
  TEST_RUN(test_flag_independence);
  return TEST_FINISH();
}
