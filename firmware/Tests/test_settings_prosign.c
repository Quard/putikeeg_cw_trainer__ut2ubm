/* Host tests of the prosign style setting and layout version 2 (references/internal/keyer_spec.md, AC-P6 to AC-P12). */

#include <string.h>

#include "eeprom/eeprom.h"
#include "fake_iap.h"
#include "settings/settings.h"
#include "support/test_assert.h"
#include "util/crc8.h"

#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define RECORD_VERSION 2U
#define RECORD_TONE_LOW 3U
#define RECORD_TONE_HIGH 4U
#define RECORD_FLAGS 10U
#define RECORD_CRC 14U

typedef struct {
  uint8_t bytes[SETTINGS_RECORD_SIZE];
  uint16_t show_cw_code;
  uint16_t paddle;
  uint16_t prosigns;
} vector_t;

/* keyer_spec.md section 17.4.2. */
static const vector_t v2_vectors[] = {
    {{0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x03U},
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0xB3U},
     1U,
     0U,
     0U},
    {{0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x05U, 0x05U, 0x08U, 0x20U, 0xEBU},
     1U,
     0U,
     1U},
    {{0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x0BU, 0x05U, 0x08U, 0x20U, 0x2FU},
     1U,
     1U,
     2U},
    {{0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x0DU, 0x05U, 0x08U, 0x20U, 0x5BU},
     1U,
     0U,
     2U},
};
static const vector_t v1_vectors[] = {
    {{0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0x54U},
     1U,
     0U,
     2U},
    {{0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x03U, 0x05U, 0x08U, 0x20U, 0x78U},
     1U,
     1U,
     2U},
    {{0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x0DU, 0x05U, 0x08U, 0x20U, 0xBCU},
     1U,
     0U,
     2U},
};
static const uint8_t v3_record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                        0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x5EU};
static const uint16_t default_values[SETTINGS_ID_COUNT] = {600U, 32U, 32U, 20U, 20U, 2U, 1U, 5U, 8U, 32U, 0U, 2U};

#define V2_COUNT (sizeof(v2_vectors) / sizeof(v2_vectors[0]))
#define V1_COUNT (sizeof(v1_vectors) / sizeof(v1_vectors[0]))

static size_t write_operations(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

static settings_load_status_t load(const uint8_t* record) {
  settings_load_status_t status;
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, SETTINGS_RECORD_SIZE);
  status = settings_load();
  return status;
}

static void start_empty(void) {
  fake_iap_reset();
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_NO_RECORD, settings_load());
  fake_iap_clear_counters();
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

static bool flags_are(uint16_t show_cw_code, uint16_t paddle, uint16_t prosigns) {
  return (settings_get(SETTINGS_ID_SHOW_CW_CODE) == show_cw_code) &&
         (settings_get(SETTINGS_ID_PADDLE_SWAP) == paddle) && (settings_get(SETTINGS_ID_PROSIGN_STYLE) == prosigns);
}

static bool is_saved_record_valid(void) {
  const uint8_t* memory = fake_iap_memory();
  return (memory[0] == SETTINGS_MAGIC_0) && (memory[1] == SETTINGS_MAGIC_1) &&
         (memory[RECORD_VERSION] == SETTINGS_LAYOUT_VERSION) && (memory[RECORD_CRC] == crc8_compute(memory, 14U));
}

static void test_vectors_have_valid_crc(void) {
  size_t index;
  for (index = 0U; index < V2_COUNT; ++index) {
    TEST_ASSERT_EQUAL(v2_vectors[index].bytes[RECORD_CRC], crc8_compute(v2_vectors[index].bytes, 14U));
  }
  for (index = 0U; index < V1_COUNT; ++index) {
    TEST_ASSERT_EQUAL(v1_vectors[index].bytes[RECORD_CRC], crc8_compute(v1_vectors[index].bytes, 14U));
  }
  TEST_ASSERT_EQUAL(v3_record[RECORD_CRC], crc8_compute(v3_record, 14U));
  TEST_ASSERT_EQUAL(2U, SETTINGS_LAYOUT_VERSION);
  TEST_ASSERT_EQUAL(15U, SETTINGS_RECORD_SIZE);
}

/* AC-P6 */
static void test_values(void) {
  static const uint16_t stored[] = {0U, 1U, 2U};
  size_t index;

  TEST_ASSERT_EQUAL(12U, SETTINGS_ID_COUNT);
  TEST_ASSERT_EQUAL(11U, SETTINGS_ID_PROSIGN_STYLE);
  TEST_ASSERT_EQUAL(0U, SETTINGS_PROSIGN_PUNCTUATION);
  TEST_ASSERT_EQUAL(1U, SETTINGS_PROSIGN_NAME);
  TEST_ASSERT_EQUAL(2U, SETTINGS_PROSIGN_BOTH);
  start_empty();
  TEST_ASSERT_EQUAL(0U, settings_min(SETTINGS_ID_PROSIGN_STYLE));
  TEST_ASSERT_EQUAL(2U, settings_max(SETTINGS_ID_PROSIGN_STYLE));
  TEST_ASSERT_EQUAL(1U, settings_step(SETTINGS_ID_PROSIGN_STYLE));
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, settings_get(SETTINGS_ID_PROSIGN_STYLE));

  TEST_ASSERT(!settings_set(SETTINGS_ID_PROSIGN_STYLE, 2U));
  TEST_ASSERT(!settings_is_edited());
  for (index = 0U; index < sizeof(stored) / sizeof(stored[0]); ++index) {
    start_empty();
    (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, stored[index]);
    TEST_ASSERT_EQUAL(stored[index], settings_get(SETTINGS_ID_PROSIGN_STYLE));
  }
  TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, 0U));
  TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, 3U));
  TEST_ASSERT_EQUAL(2U, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, 0U));
  TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, 255U));
  TEST_ASSERT_EQUAL(2U, settings_get(SETTINGS_ID_PROSIGN_STYLE));

  start_empty();
  TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, 1U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT(!settings_set(SETTINGS_ID_PROSIGN_STYLE, 1U));
  TEST_ASSERT(!settings_is_edited());

  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_COUNT));
  TEST_ASSERT_EQUAL(0U, settings_min(SETTINGS_ID_COUNT));
  TEST_ASSERT_EQUAL(0U, settings_max(SETTINGS_ID_COUNT));
  TEST_ASSERT_EQUAL(0U, settings_step(SETTINGS_ID_COUNT));
  TEST_ASSERT(!settings_set(SETTINGS_ID_COUNT, 1U));
  TEST_ASSERT(!settings_is_edited());
}

/* Sets the three flag ids in one of two orders and checks after each set that the other two stay. */
static void set_flags(uint16_t show_cw_code, uint16_t paddle, uint16_t prosigns, bool is_reverse_order) {
  if (is_reverse_order) {
    (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, prosigns);
    TEST_ASSERT(flags_are(1U, 0U, prosigns));
    (void)settings_set(SETTINGS_ID_PADDLE_SWAP, paddle);
    TEST_ASSERT(flags_are(1U, paddle, prosigns));
    (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, show_cw_code);
  } else {
    (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, show_cw_code);
    TEST_ASSERT(flags_are(show_cw_code, 0U, 2U));
    (void)settings_set(SETTINGS_ID_PADDLE_SWAP, paddle);
    TEST_ASSERT(flags_are(show_cw_code, paddle, 2U));
    (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, prosigns);
  }
  TEST_ASSERT(flags_are(show_cw_code, paddle, prosigns));
}

/* AC-P7 and the combination part of AC-P8. */
static void test_flag_bits(void) {
  uint16_t show_cw_code;
  uint16_t paddle;
  uint16_t prosigns;
  unsigned order;

  for (order = 0U; order < 2U; ++order) {
    for (show_cw_code = 0U; show_cw_code <= 1U; ++show_cw_code) {
      for (paddle = 0U; paddle <= 1U; ++paddle) {
        for (prosigns = 0U; prosigns <= 2U; ++prosigns) {
          const uint8_t flags = (uint8_t)(show_cw_code + 2U * paddle + 4U * prosigns);
          start_empty();
          set_flags(show_cw_code, paddle, prosigns, order != 0U);
          /* Each set of one id with its own current value changes nothing. */
          TEST_ASSERT(!settings_set(SETTINGS_ID_SHOW_CW_CODE, show_cw_code));
          TEST_ASSERT(!settings_set(SETTINGS_ID_PADDLE_SWAP, paddle));
          TEST_ASSERT(!settings_set(SETTINGS_ID_PROSIGN_STYLE, prosigns));
          if (flags == 0x09U) {
            TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
            continue;
          }
          TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
          TEST_ASSERT_EQUAL(flags, fake_iap_memory()[RECORD_FLAGS]);
          TEST_ASSERT_EQUAL(0U, fake_iap_memory()[RECORD_FLAGS] & 0xF0U);
          TEST_ASSERT(is_saved_record_valid());
          TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
          TEST_ASSERT(flags_are(show_cw_code, paddle, prosigns));
        }
      }
    }
  }
}

/* AC-P8 */
static void test_version_2_vectors(void) {
  uint8_t expected[SETTINGS_RECORD_SIZE];
  size_t index;

  for (index = 0U; index < V2_COUNT; ++index) {
    const vector_t* vector = &v2_vectors[index];
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(vector->bytes));
    TEST_ASSERT(flags_are(vector->show_cw_code, vector->paddle, vector->prosigns));
    TEST_ASSERT_EQUAL(600U, settings_get(SETTINGS_ID_TONE_HZ));
    TEST_ASSERT(!settings_has_unsaved_changes());
  }

  start_empty();
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 700U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  memcpy(expected, v2_vectors[0].bytes, sizeof(expected));
  expected[RECORD_TONE_LOW] = 0xBCU;
  expected[RECORD_TONE_HIGH] = 0x02U;
  expected[RECORD_CRC] = crc8_compute(expected, 14U);
  TEST_ASSERT_EQUAL(0, memcmp(expected, fake_iap_memory(), sizeof(expected)));
}

/* AC-P9: the version 1 vectors. */
static void test_version_1_vectors(void) {
  size_t index;

  for (index = 0U; index < V1_COUNT; ++index) {
    const vector_t* vector = &v1_vectors[index];
    fake_iap_reset();
    memcpy(fake_iap_memory(), vector->bytes, SETTINGS_RECORD_SIZE);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
    TEST_ASSERT(flags_are(vector->show_cw_code, vector->paddle, vector->prosigns));
    TEST_ASSERT_EQUAL(600U, settings_get(SETTINGS_ID_TONE_HZ));
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT(!settings_is_edited());
    fake_iap_clear_counters();
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
    /* Each start-up loads the version 1 record again, with the same result. */
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
    TEST_ASSERT(flags_are(vector->show_cw_code, vector->paddle, vector->prosigns));
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT_EQUAL(0x01U, fake_iap_memory()[RECORD_VERSION]);
  }
}

/* AC-P9: every flags value of a version 1 record. */
static void test_version_1_every_flags_value(void) {
  unsigned flags;

  for (flags = 0U; flags <= 255U; ++flags) {
    uint8_t record[SETTINGS_RECORD_SIZE];
    memcpy(record, v1_vectors[0].bytes, sizeof(record));
    record[RECORD_FLAGS] = (uint8_t)flags;
    record[RECORD_CRC] = crc8_compute(record, 14U);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
    TEST_ASSERT(flags_are(flags & 0x01U, (flags >> 1) & 0x01U, SETTINGS_PROSIGN_BOTH));
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT(!settings_has_unsaved_changes());
    fake_iap_clear_counters();
    TEST_ASSERT_EQUAL(SETTINGS_SAVE_NOT_NEEDED, settings_save_if_changed());
    TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
  }
}

/* AC-P9: out-of-range fields clamp as in version 2; a bad CRC gives the defaults. */
static void test_version_1_clamp_and_bad_crc(void) {
  uint8_t record[SETTINGS_RECORD_SIZE];

  memcpy(record, v1_vectors[0].bytes, sizeof(record));
  record[3] = 0xDCU; /* 1500 Hz */
  record[4] = 0x05U;
  record[5] = 30U;
  record[6] = 40U;
  record[9] = 7U;
  record[11] = 1U;
  record[13] = 64U;
  record[RECORD_CRC] = crc8_compute(record, 14U);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(record));
  TEST_ASSERT_EQUAL(1000U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT_EQUAL(30U, settings_get(SETTINGS_ID_RX_WPM));
  TEST_ASSERT_EQUAL(30U, settings_get(SETTINGS_ID_RX_EFF_WPM));
  TEST_ASSERT_EQUAL(2U, settings_get(SETTINGS_ID_KEY_TYPE));
  TEST_ASSERT_EQUAL(4U, settings_get(SETTINGS_ID_MAX_WORD_LEN));
  TEST_ASSERT_EQUAL(63U, settings_get(SETTINGS_ID_CONTRAST));
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  TEST_ASSERT_EQUAL(0U, write_operations());

  memcpy(record, v1_vectors[1].bytes, sizeof(record));
  record[RECORD_CRC] ^= 0x01U;
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_CRC, load(record));
  TEST_ASSERT(are_defaults());
  TEST_ASSERT_EQUAL(0U, write_operations());
}

/* AC-P10 */
static void test_save_after_version_1_load(void) {
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(v1_vectors[1].bytes));
  fake_iap_clear_counters();
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 700U));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT(is_saved_record_valid());
  TEST_ASSERT_EQUAL(0x02U, fake_iap_memory()[RECORD_VERSION]);
  TEST_ASSERT_EQUAL(2U, (fake_iap_memory()[RECORD_FLAGS] >> 2) & 0x03U);
  TEST_ASSERT_EQUAL(0x0BU, fake_iap_memory()[RECORD_FLAGS]);
  TEST_ASSERT_EQUAL(0xBCU, fake_iap_memory()[RECORD_TONE_LOW]);
  TEST_ASSERT_EQUAL(0x02U, fake_iap_memory()[RECORD_TONE_HIGH]);
  TEST_ASSERT_EQUAL(0, memcmp(&v1_vectors[1].bytes[5], &fake_iap_memory()[5], 5U));
  TEST_ASSERT_EQUAL(0, memcmp(&v1_vectors[1].bytes[11], &fake_iap_memory()[11], 3U));
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  TEST_ASSERT(flags_are(1U, 1U, SETTINGS_PROSIGN_BOTH));
  TEST_ASSERT_EQUAL(700U, settings_get(SETTINGS_ID_TONE_HZ));

  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, load(v1_vectors[0].bytes));
  TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, SETTINGS_PROSIGN_PUNCTUATION));
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  TEST_ASSERT_EQUAL(0U, (fake_iap_memory()[RECORD_FLAGS] >> 2) & 0x03U);
  TEST_ASSERT_EQUAL(0, memcmp(v2_vectors[1].bytes, fake_iap_memory(), SETTINGS_RECORD_SIZE));
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_PUNCTUATION, settings_get(SETTINGS_ID_PROSIGN_STYLE));
}

/* AC-P11 */
static void test_other_versions(void) {
  unsigned version;

  for (version = 0U; version <= 255U; ++version) {
    uint8_t record[SETTINGS_RECORD_SIZE];
    if ((version == 1U) || (version == 2U)) {
      continue;
    }
    memcpy(record, v2_vectors[2].bytes, sizeof(record));
    record[RECORD_VERSION] = (uint8_t)version;
    record[RECORD_CRC] = crc8_compute(record, 14U);
    TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_VERSION, load(record));
    TEST_ASSERT(are_defaults());
    TEST_ASSERT_EQUAL(0U, write_operations());
    TEST_ASSERT(!settings_has_unsaved_changes());
  }
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_BAD_VERSION, load(v3_record));
  TEST_ASSERT(are_defaults());
  TEST_ASSERT_EQUAL(0U, write_operations());
}

/* Sets up the first save after a load of version 1 vector "Reversed": Tone 450 Hz, Prosigns Prosign. */
static void prepare_first_save(void) {
  fake_iap_reset();
  memcpy(fake_iap_memory(), v1_vectors[1].bytes, SETTINGS_RECORD_SIZE);
  TEST_ASSERT_EQUAL(SETTINGS_LOAD_OK, settings_load());
  (void)settings_set(SETTINGS_ID_TONE_HZ, 450U);
  (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, SETTINGS_PROSIGN_NAME);
  fake_iap_clear_counters();
}

/* AC-P12 */
static void test_power_loss_after_version_1_load(void) {
  size_t point;
  size_t total_operations;

  prepare_first_save();
  TEST_ASSERT_EQUAL(SETTINGS_SAVE_OK, settings_save_if_changed());
  total_operations = fake_iap_operation_count();
  TEST_ASSERT_EQUAL(1U + 2U * SETTINGS_RECORD_SIZE, total_operations);

  for (point = 1U; point <= total_operations; ++point) {
    bool is_old;
    bool is_new;
    prepare_first_save();
    fake_iap_set_power_loss_after(point);
    (void)settings_save_if_changed();
    fake_iap_clear_counters();
    (void)settings_load();
    is_old = (settings_get(SETTINGS_ID_TONE_HZ) == 600U) && flags_are(1U, 1U, SETTINGS_PROSIGN_BOTH);
    is_new = (settings_get(SETTINGS_ID_TONE_HZ) == 450U) && flags_are(1U, 1U, SETTINGS_PROSIGN_NAME);
    if (!TEST_ASSERT(is_old || is_new || are_defaults())) {
      fprintf(stderr, "power loss after operation %zu\n", point);
    }
    if (point == total_operations) {
      TEST_ASSERT(is_new);
    }
  }
}

int main(void) {
  TEST_RUN(test_vectors_have_valid_crc);
  TEST_RUN(test_values);
  TEST_RUN(test_flag_bits);
  TEST_RUN(test_version_2_vectors);
  TEST_RUN(test_version_1_vectors);
  TEST_RUN(test_version_1_every_flags_value);
  TEST_RUN(test_version_1_clamp_and_bad_crc);
  TEST_RUN(test_save_after_version_1_load);
  TEST_RUN(test_other_versions);
  TEST_RUN(test_power_loss_after_version_1_load);
  return TEST_FINISH();
}
