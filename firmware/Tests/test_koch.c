/*
 * Host tests of the Koch tables, the level model and the item generation (references/internal/training_spec.md
 * sections 5.6, 6 and 7, AC-T4 to AC-T7).
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cw/morse.h"
#include "koch/koch.h"
#include "test_assert.h"
#include "util/rng.h"

#define ITEMS_PER_SEED 10000U
#define SEED_COUNT 5U
#define SHARE_MIN_PERCENT 45U
#define SHARE_MAX_PERCENT 55U

static const char expected_koch_order[KOCH_CHAR_COUNT] = {
    'K', 'M', 'R', 'S', 'U', 'A', 'P', 'T', 'L', 'O', 'W',           'I',           '.',           'N', 'J',
    'E', 'F', '0', 'Y', ',', 'V', 'G', '5', '/', 'Q', '9',           'Z',           'H',           '3', '8',
    'B', '?', '4', '2', '7', 'C', '1', 'D', '6', 'X', MORSE_CHAR_BT, MORSE_CHAR_SK, MORSE_CHAR_AR,
};

static const char expected_keyboard_order[KOCH_CHAR_COUNT] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',           'L',           'M',           'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',           '0',           '1',           '2', '3',
    '4', '5', '6', '7', '8', '9', '.', ',', '?', '/', MORSE_CHAR_BT, MORSE_CHAR_SK, MORSE_CHAR_AR,
};

static const uint16_t seeds[SEED_COUNT] = {1U, 0x1234U, 0xBEEFU, 0x7FFFU, 0xFFFFU};

/* Section 6, written independently of koch.c. */
typedef struct {
  koch_kind_t kind;
  uint8_t learned;
  uint8_t items;
  uint8_t length_without_cap; /* 0 at a character level; 255 means "the maximum" */
} level_model_t;

static level_model_t model_of(uint32_t level) {
  level_model_t model;

  if (level >= 63U) {
    model.kind = KOCH_KIND_WORD;
    model.learned = 43U;
    model.items = (uint8_t)(((level - 63U) >= 90U) ? 100U : (10U + (level - 63U)));
    model.length_without_cap = 255U;
  } else {
    const uint32_t group = level / 3U;
    const uint32_t rest = level % 3U;

    model.kind = (rest == 2U) ? KOCH_KIND_WORD : KOCH_KIND_CHAR;
    model.learned = (uint8_t)((rest == 0U) ? (2U + (2U * group)) : (3U + (2U * group)));
    model.items = (rest == 2U) ? 10U : 50U;
    model.length_without_cap = (uint8_t)((rest == 2U) ? (2U + (group / 2U)) : 0U);
  }
  return model;
}

static uint8_t capped_length(level_model_t model, uint8_t max_word_len) {
  if (model.kind == KOCH_KIND_CHAR) {
    return 0U;
  }
  return (model.length_without_cap < max_word_len) ? model.length_without_cap : max_word_len;
}

/* AC-T4 */
static void test_tables(void) {
  uint8_t index;
  uint8_t other;

  for (index = 0U; index < KOCH_CHAR_COUNT; ++index) {
    TEST_ASSERT_EQUAL((uint8_t)expected_koch_order[index], (uint8_t)koch_char(index));
    TEST_ASSERT_EQUAL((uint8_t)expected_keyboard_order[index], (uint8_t)koch_keyboard_char(index));
    TEST_ASSERT(morse_encode(koch_char(index)) != MORSE_PATTERN_INVALID);
    TEST_ASSERT_EQUAL(index, koch_index_of(koch_char(index)));
    /* Same set: every keyboard character is in the Koch table. */
    TEST_ASSERT(koch_index_of(koch_keyboard_char(index)) < KOCH_CHAR_COUNT);
    for (other = (uint8_t)(index + 1U); other < KOCH_CHAR_COUNT; ++other) {
      TEST_ASSERT(koch_char(index) != koch_char(other));
      TEST_ASSERT(koch_keyboard_char(index) != koch_keyboard_char(other));
    }
  }
  TEST_ASSERT_EQUAL(0U, (uint8_t)koch_char(KOCH_CHAR_COUNT));
  TEST_ASSERT_EQUAL(0U, (uint8_t)koch_char(255U));
  TEST_ASSERT_EQUAL(0U, (uint8_t)koch_keyboard_char(KOCH_CHAR_COUNT));
  TEST_ASSERT_EQUAL(0U, (uint8_t)koch_keyboard_char(255U));
  TEST_ASSERT_EQUAL(KOCH_CHAR_COUNT, koch_index_of('\0'));
  TEST_ASSERT_EQUAL(KOCH_CHAR_COUNT, koch_index_of('*'));
  TEST_ASSERT_EQUAL(KOCH_CHAR_COUNT, koch_index_of('k'));
  TEST_ASSERT_EQUAL(KOCH_CHAR_COUNT, koch_index_of(MORSE_CHAR_KN));
  TEST_ASSERT_EQUAL(KOCH_CHAR_COUNT, koch_index_of(MORSE_CHAR_AS));
}

/* AC-T5 */
static void test_level_model(void) {
  uint32_t level;
  uint8_t max_word_len;
  unsigned long mismatch_count = 0UL;

  TEST_ASSERT_EQUAL(63U, KOCH_FINAL_LEVEL);
  for (level = 0U; level <= (KOCH_FINAL_LEVEL + 1U); ++level) {
    const level_model_t model = model_of(level);

    TEST_ASSERT_EQUAL(model.kind, koch_kind((uint16_t)level));
    TEST_ASSERT_EQUAL(model.learned, koch_learned_count((uint16_t)level));
    TEST_ASSERT_EQUAL(model.items, koch_item_count((uint16_t)level));
    for (max_word_len = 4U; max_word_len <= 8U; ++max_word_len) {
      TEST_ASSERT_EQUAL(capped_length(model, max_word_len), koch_word_length((uint16_t)level, max_word_len));
    }
    TEST_ASSERT_EQUAL((model.kind == KOCH_KIND_WORD) && (model.learned > 40U) ? 40U : model.learned,
                      koch_alphabet_count((uint16_t)level));
  }
  /* Every level index, also above the final phase. */
  for (level = 0U; level <= 65535U; ++level) {
    const level_model_t model = model_of(level);

    if ((koch_kind((uint16_t)level) != model.kind) || (koch_learned_count((uint16_t)level) != model.learned) ||
        (koch_item_count((uint16_t)level) != model.items) ||
        (koch_word_length((uint16_t)level, 6U) != capped_length(model, 6U))) {
      ++mismatch_count;
    }
  }
  TEST_ASSERT_EQUAL(0UL, mismatch_count);
}

typedef struct {
  uint16_t level;
  koch_kind_t kind;
  uint8_t learned;
  char new_char;
  uint8_t items;
  uint8_t length;
} reference_row_t;

/* Section 6, reference values (length with Max Word Length 8). */
static void test_reference_rows(void) {
  static const reference_row_t rows[] = {
      {0U, KOCH_KIND_CHAR, 2U, 'M', 50U, 0U},
      {1U, KOCH_KIND_CHAR, 3U, 'R', 50U, 0U},
      {2U, KOCH_KIND_WORD, 3U, '\0', 10U, 2U},
      {3U, KOCH_KIND_CHAR, 4U, 'S', 50U, 0U},
      {5U, KOCH_KIND_WORD, 5U, '\0', 10U, 2U},
      {8U, KOCH_KIND_WORD, 7U, '\0', 10U, 3U},
      {59U, KOCH_KIND_WORD, 41U, '\0', 10U, 8U},
      {60U, KOCH_KIND_CHAR, 42U, MORSE_CHAR_SK, 50U, 0U},
      {61U, KOCH_KIND_CHAR, 43U, MORSE_CHAR_AR, 50U, 0U},
      {62U, KOCH_KIND_WORD, 43U, '\0', 10U, 8U},
      {63U, KOCH_KIND_WORD, 43U, '\0', 10U, 8U},
      {64U, KOCH_KIND_WORD, 43U, '\0', 11U, 8U},
      {152U, KOCH_KIND_WORD, 43U, '\0', 99U, 8U},
      {153U, KOCH_KIND_WORD, 43U, '\0', 100U, 8U},
      {65535U, KOCH_KIND_WORD, 43U, '\0', 100U, 8U},
  };
  size_t index;

  for (index = 0U; index < sizeof(rows) / sizeof(rows[0]); ++index) {
    TEST_ASSERT_EQUAL(rows[index].kind, koch_kind(rows[index].level));
    TEST_ASSERT_EQUAL(rows[index].learned, koch_learned_count(rows[index].level));
    TEST_ASSERT_EQUAL(rows[index].items, koch_item_count(rows[index].level));
    TEST_ASSERT_EQUAL(rows[index].length, koch_word_length(rows[index].level, 8U));
    if (rows[index].kind == KOCH_KIND_CHAR) {
      TEST_ASSERT_EQUAL((uint8_t)rows[index].new_char,
                        (uint8_t)koch_char((uint8_t)(koch_learned_count(rows[index].level) - 1U)));
    }
  }
  TEST_ASSERT_EQUAL(10U, koch_item_count(KOCH_FINAL_LEVEL));
  TEST_ASSERT_EQUAL(11U, koch_item_count(KOCH_FINAL_LEVEL + 1U));
  TEST_ASSERT_EQUAL(99U, koch_item_count(KOCH_FINAL_LEVEL + 89U));
  TEST_ASSERT_EQUAL(100U, koch_item_count(KOCH_FINAL_LEVEL + 90U));
  TEST_ASSERT_EQUAL(100U, koch_item_count(65535U));
  TEST_ASSERT_EQUAL(40U, koch_alphabet_count(59U));
  TEST_ASSERT_EQUAL(39U, koch_alphabet_count(56U));
  TEST_ASSERT_EQUAL(43U, koch_alphabet_count(61U));
  TEST_ASSERT_EQUAL(40U, koch_alphabet_count(62U));
  TEST_ASSERT_EQUAL(40U, koch_alphabet_count(65535U));
  TEST_ASSERT_EQUAL(13U, koch_alphabet_count(17U));
  TEST_ASSERT_EQUAL(4U, koch_word_length(17U, 8U));
}

static void test_word_length_clamp(void) {
  /* A final level gives the clamped maximum. */
  TEST_ASSERT_EQUAL(2U, koch_word_length(KOCH_FINAL_LEVEL, 0U));
  TEST_ASSERT_EQUAL(2U, koch_word_length(KOCH_FINAL_LEVEL, 1U));
  TEST_ASSERT_EQUAL(8U, koch_word_length(KOCH_FINAL_LEVEL, 9U));
  TEST_ASSERT_EQUAL(8U, koch_word_length(KOCH_FINAL_LEVEL, 255U));
  TEST_ASSERT_EQUAL(8U, koch_word_length(59U, 255U));
  TEST_ASSERT_EQUAL(2U, koch_word_length(59U, 0U));
  TEST_ASSERT_EQUAL(2U, koch_word_length(2U, 0U));
  TEST_ASSERT_EQUAL(0U, koch_word_length(0U, 255U));
  TEST_ASSERT_EQUAL(0U, koch_word_length(61U, 0U));
}

/* Smallest t with t / n >= p / q, by counting up: an integer reference that does not use the closed form. */
static unsigned reference_threshold(unsigned count, unsigned p, unsigned q) {
  unsigned threshold = 0U;

  while ((q * threshold) < (p * count)) {
    ++threshold;
  }
  return threshold;
}

/* AC-T30 (4), D-T5, D-T37: the shown level is the index + 1, saturated at 65535. */
static void test_shown_level(void) {
  uint32_t level;

  TEST_ASSERT_EQUAL(1U, koch_shown_level(0U));
  TEST_ASSERT_EQUAL(2U, koch_shown_level(1U));
  TEST_ASSERT_EQUAL(65534U, koch_shown_level(65533U));
  TEST_ASSERT_EQUAL(65535U, koch_shown_level(65534U));
  TEST_ASSERT_EQUAL(65535U, koch_shown_level(65535U));
  for (level = 0U; level < KOCH_LEVEL_MAX; ++level) {
    TEST_ASSERT_EQUAL(level + 1U, koch_shown_level((uint16_t)level));
  }
}

/* AC-T6 (D-T33): ceil(0.9 n) for a character session, ceil(0.8 n) for a word session. */
static void test_pass_threshold(void) {
  unsigned count;

  for (count = 0U; count <= 255U; ++count) {
    TEST_ASSERT_EQUAL(reference_threshold(count, 9U, 10U), koch_pass_threshold(KOCH_KIND_CHAR, (uint8_t)count));
    TEST_ASSERT_EQUAL(reference_threshold(count, 4U, 5U), koch_pass_threshold(KOCH_KIND_WORD, (uint8_t)count));
    /* An unknown kind uses the character rule. */
    TEST_ASSERT_EQUAL(reference_threshold(count, 9U, 10U), koch_pass_threshold(2U, (uint8_t)count));
    TEST_ASSERT_EQUAL(reference_threshold(count, 9U, 10U), koch_pass_threshold(255U, (uint8_t)count));
  }
  TEST_ASSERT_EQUAL(9U, koch_pass_threshold(KOCH_KIND_CHAR, 10U));
  TEST_ASSERT_EQUAL(10U, koch_pass_threshold(KOCH_KIND_CHAR, 11U));
  TEST_ASSERT_EQUAL(45U, koch_pass_threshold(KOCH_KIND_CHAR, 50U));
  TEST_ASSERT_EQUAL(90U, koch_pass_threshold(KOCH_KIND_CHAR, 100U));
  TEST_ASSERT_EQUAL(0U, koch_pass_threshold(KOCH_KIND_CHAR, 0U));
  TEST_ASSERT_EQUAL(230U, koch_pass_threshold(KOCH_KIND_CHAR, 255U));
  TEST_ASSERT_EQUAL(8U, koch_pass_threshold(KOCH_KIND_WORD, 10U));
  TEST_ASSERT_EQUAL(9U, koch_pass_threshold(KOCH_KIND_WORD, 11U));
  TEST_ASSERT_EQUAL(12U, koch_pass_threshold(KOCH_KIND_WORD, 15U));
  TEST_ASSERT_EQUAL(40U, koch_pass_threshold(KOCH_KIND_WORD, 50U));
  TEST_ASSERT_EQUAL(80U, koch_pass_threshold(KOCH_KIND_WORD, 100U));
  TEST_ASSERT_EQUAL(0U, koch_pass_threshold(KOCH_KIND_WORD, 0U));
  TEST_ASSERT_EQUAL(204U, koch_pass_threshold(KOCH_KIND_WORD, 255U));
  TEST_ASSERT_EQUAL(45U, koch_pass_threshold(2U, 50U));
  TEST_ASSERT_EQUAL(45U, koch_pass_threshold(255U, 50U));
}

static bool is_in_percent_range(unsigned long count, unsigned long total) {
  return ((count * 100UL) >= (SHARE_MIN_PERCENT * total)) && ((count * 100UL) <= (SHARE_MAX_PERCENT * total));
}

/* AC-T7: character levels. */
static void check_char_level_mix(uint16_t level) {
  char text[KOCH_WORD_LEN_MAX + 1U];
  const uint8_t learned = koch_learned_count(level);
  size_t seed;

  for (seed = 0U; seed < SEED_COUNT; ++seed) {
    unsigned long counts[KOCH_CHAR_COUNT] = {0};
    unsigned item;
    uint8_t index;

    rng_seed(seeds[seed]);
    for (item = 0U; item < ITEMS_PER_SEED; ++item) {
      memset(text, 'x', sizeof(text));
      TEST_ASSERT_EQUAL(1U, koch_make_item(level, 8U, text));
      TEST_ASSERT_EQUAL(0U, (uint8_t)text[1]);
      index = koch_index_of(text[0]);
      if (!TEST_ASSERT(index < learned)) {
        return;
      }
      ++counts[index];
    }
    TEST_ASSERT(is_in_percent_range(counts[learned - 1U], ITEMS_PER_SEED));
    for (index = 0U; index < (uint8_t)(learned - 1U); ++index) {
      TEST_ASSERT(counts[index] > 0UL);
    }
    if (level == 0U) {
      TEST_ASSERT(is_in_percent_range(counts[0], ITEMS_PER_SEED));
    }
  }
}

static void test_char_item_mix(void) {
  check_char_level_mix(0U);
  check_char_level_mix(1U);
  check_char_level_mix(30U);
  check_char_level_mix(61U);
}

/* AC-T7: word levels. */
static void check_word_level(uint16_t level, uint8_t max_word_len) {
  char text[KOCH_WORD_LEN_MAX + 1U];
  const uint8_t alphabet = koch_alphabet_count(level);
  const uint8_t length = koch_word_length(level, max_word_len);
  unsigned long counts[KOCH_CHAR_COUNT] = {0};
  unsigned item;
  uint8_t position;
  uint8_t index;

  rng_seed(0x2468U);
  for (item = 0U; item < ITEMS_PER_SEED; ++item) {
    memset(text, 'x', sizeof(text));
    TEST_ASSERT_EQUAL(length, koch_make_item(level, max_word_len, text));
    TEST_ASSERT_EQUAL(0U, (uint8_t)text[length]);
    for (position = 0U; position < length; ++position) {
      index = koch_index_of(text[position]);
      if (!TEST_ASSERT(index < alphabet)) {
        return;
      }
      ++counts[index];
    }
  }
  for (index = 0U; index < alphabet; ++index) {
    TEST_ASSERT(counts[index] > 0UL);
  }
  TEST_ASSERT_EQUAL(0UL, counts[KOCH_CHAR_COUNT - 1U] + counts[KOCH_CHAR_COUNT - 2U] + counts[KOCH_CHAR_COUNT - 3U]);
}

static void test_word_items(void) {
  check_word_level(2U, 8U);
  check_word_level(17U, 8U);
  check_word_level(59U, 5U);
  check_word_level(62U, 8U);
  check_word_level(KOCH_FINAL_LEVEL, 4U);
  check_word_level(65535U, 8U);
}

/* Section 7: the call order of rng_below() is part of the contract. */
static void test_rng_call_order(void) {
  char text[KOCH_WORD_LEN_MAX + 1U];
  char expected[KOCH_WORD_LEN_MAX + 1U];
  unsigned item;
  uint8_t position;

  for (item = 0U; item < 500U; ++item) {
    const uint16_t seed = (uint16_t)(item * 131U + 7U);
    uint8_t value;

    rng_seed(seed);
    (void)koch_make_item(30U, 8U, text);
    rng_seed(seed);
    value = (uint8_t)(koch_learned_count(30U) - 1U);
    if (rng_below(2U) != 0U) {
      value = rng_below(value);
    }
    TEST_ASSERT_EQUAL((uint8_t)koch_char(value), (uint8_t)text[0]);

    rng_seed(seed);
    (void)koch_make_item(62U, 6U, text);
    rng_seed(seed);
    for (position = 0U; position < 6U; ++position) {
      expected[position] = koch_char(rng_below(40U));
    }
    expected[6] = '\0';
    TEST_ASSERT(strcmp(expected, text) == 0);
  }
}

int main(void) {
  TEST_RUN(test_tables);
  TEST_RUN(test_level_model);
  TEST_RUN(test_reference_rows);
  TEST_RUN(test_word_length_clamp);
  TEST_RUN(test_shown_level);
  TEST_RUN(test_pass_threshold);
  TEST_RUN(test_char_item_mix);
  TEST_RUN(test_word_items);
  TEST_RUN(test_rng_call_order);
  return TEST_FINISH();
}
