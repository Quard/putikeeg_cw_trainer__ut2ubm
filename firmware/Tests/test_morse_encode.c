/* Host tests of morse_encode() (references/internal/training_spec.md section 5.2, AC-T1). */

#include <stdbool.h>
#include <stdint.h>

#include "cw/morse.h"
#include "test_assert.h"

#define PATTERN_FIRST 2U
#define PATTERN_END 128U

static bool is_table_character(char character) {
  uint16_t pattern;

  for (pattern = PATTERN_FIRST; pattern < PATTERN_END; ++pattern) {
    if ((morse_decode((uint8_t)pattern) != MORSE_CHAR_UNKNOWN) && (morse_decode((uint8_t)pattern) == character)) {
      return true;
    }
  }
  return false;
}

static void test_round_trip_for_every_table_entry(void) {
  uint16_t pattern;
  unsigned entry_count = 0U;

  for (pattern = PATTERN_FIRST; pattern < PATTERN_END; ++pattern) {
    const char character = morse_decode((uint8_t)pattern);

    if (character != MORSE_CHAR_UNKNOWN) {
      TEST_ASSERT_EQUAL(pattern, morse_encode(character));
      ++entry_count;
    }
  }
  /* 26 letters, 10 digits, 5 prosigns and 13 punctuation marks (keyer_spec.md section 6.10). */
  TEST_ASSERT_EQUAL(54U, entry_count);
}

static void test_every_other_value_is_invalid(void) {
  uint16_t value;

  for (value = 0U; value < 256U; ++value) {
    const char character = (char)(uint8_t)value;

    if (!is_table_character(character)) {
      TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_encode(character));
    }
  }
}

static void test_named_values(void) {
  TEST_ASSERT_EQUAL(0x05U, morse_encode('A'));
  TEST_ASSERT_EQUAL(0x02U, morse_encode('E'));
  TEST_ASSERT_EQUAL(0x03U, morse_encode('T'));
  TEST_ASSERT_EQUAL(0x3FU, morse_encode('0'));
  TEST_ASSERT_EQUAL(0x20U, morse_encode('5'));
  TEST_ASSERT_EQUAL(69U, morse_encode(MORSE_CHAR_SK));
  TEST_ASSERT_EQUAL(42U, morse_encode(MORSE_CHAR_AR));
  TEST_ASSERT_EQUAL(49U, morse_encode(MORSE_CHAR_BT));
  TEST_ASSERT_EQUAL(54U, morse_encode(MORSE_CHAR_KN));
  TEST_ASSERT_EQUAL(40U, morse_encode(MORSE_CHAR_AS));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_encode(MORSE_CHAR_UNKNOWN));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_encode(' '));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_encode('*'));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_encode('+'));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_encode((char)0x80));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_encode((char)0xFF));
}

static void test_lowercase_is_invalid(void) {
  char letter;

  for (letter = 'a'; letter <= 'z'; ++letter) {
    TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_encode(letter));
  }
}

int main(void) {
  TEST_RUN(test_round_trip_for_every_table_entry);
  TEST_RUN(test_every_other_value_is_invalid);
  TEST_RUN(test_named_values);
  TEST_RUN(test_lowercase_is_invalid);
  return TEST_FINISH();
}
