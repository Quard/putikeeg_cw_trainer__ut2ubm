/* Host tests of the Morse pattern builder and table (references/internal/keyer_spec.md, AC-K40 and AC-K41). */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cw/morse.h"
#include "test_assert.h"

typedef struct {
  const char* code;
  char character;
} morse_entry_t;

/* keyer_spec.md section 6.10, written as codes, so the test does not repeat the index rule of the table. */
static const morse_entry_t expected_entries[] = {
    {".-", 'A'},
    {"-...", 'B'},
    {"-.-.", 'C'},
    {"-..", 'D'},
    {".", 'E'},
    {"..-.", 'F'},
    {"--.", 'G'},
    {"....", 'H'},
    {"..", 'I'},
    {".---", 'J'},
    {"-.-", 'K'},
    {".-..", 'L'},
    {"--", 'M'},
    {"-.", 'N'},
    {"---", 'O'},
    {".--.", 'P'},
    {"--.-", 'Q'},
    {".-.", 'R'},
    {"...", 'S'},
    {"-", 'T'},
    {"..-", 'U'},
    {"...-", 'V'},
    {".--", 'W'},
    {"-..-", 'X'},
    {"-.--", 'Y'},
    {"--..", 'Z'},
    {"-----", '0'},
    {".----", '1'},
    {"..---", '2'},
    {"...--", '3'},
    {"....-", '4'},
    {".....", '5'},
    {"-....", '6'},
    {"--...", '7'},
    {"---..", '8'},
    {"----.", '9'},
    {".-...", MORSE_CHAR_AS},
    {".-.-.", MORSE_CHAR_AR},
    {"-...-", MORSE_CHAR_BT},
    {"-.--.", MORSE_CHAR_KN},
    {"...-.-", MORSE_CHAR_SK},
    {"-..-.", '/'},
    {"..--..", '?'},
    {"..--.-", '_'},
    {".-..-.", '"'},
    {".-.-.-", '.'},
    {".--.-.", '@'},
    {".----.", '\''},
    {"-....-", '-'},
    {"-.-.-.", ';'},
    {"-.-.--", '!'},
    {"-.--.-", ')'},
    {"--..--", ','},
    {"---...", ':'},
};

#define ENTRY_COUNT (sizeof(expected_entries) / sizeof(expected_entries[0]))

static uint8_t pattern_of(const char* code) {
  uint8_t pattern = MORSE_PATTERN_EMPTY;
  for (; *code != '\0'; ++code) {
    pattern = morse_add_element(pattern, *code == '-');
  }
  return pattern;
}

/* AC-K40 */
static void test_table_entries(void) {
  bool is_listed[128] = {false};
  size_t index;
  unsigned value;

  TEST_ASSERT_EQUAL(54U, ENTRY_COUNT);
  for (index = 0U; index < ENTRY_COUNT; ++index) {
    const uint8_t pattern = pattern_of(expected_entries[index].code);
    TEST_ASSERT(pattern < 128U);
    TEST_ASSERT(!is_listed[pattern]);
    is_listed[pattern] = true;
    if (!TEST_ASSERT_EQUAL((unsigned char)expected_entries[index].character, (unsigned char)morse_decode(pattern))) {
      fprintf(stderr, "code %s\n", expected_entries[index].code);
    }
  }
  for (value = 0U; value < 128U; ++value) {
    const char character = morse_decode((uint8_t)value);
    if (!is_listed[value]) {
      TEST_ASSERT_EQUAL(MORSE_CHAR_UNKNOWN, character);
    }
    TEST_ASSERT(!((character >= 'a') && (character <= 'z')));
    TEST_ASSERT(character != ' ');
    TEST_ASSERT(character != '*');
    TEST_ASSERT(character != '<');
    TEST_ASSERT(character != '>');
  }
  for (value = 128U; value <= 255U; ++value) {
    TEST_ASSERT_EQUAL(MORSE_CHAR_UNKNOWN, morse_decode((uint8_t)value));
  }
  TEST_ASSERT_EQUAL(MORSE_CHAR_UNKNOWN, morse_decode(MORSE_PATTERN_EMPTY));
  TEST_ASSERT_EQUAL(MORSE_CHAR_UNKNOWN, morse_decode(MORSE_PATTERN_INVALID));
  TEST_ASSERT_EQUAL(69U, pattern_of("...-.-"));
}

/* AC-K41 */
static void test_pattern_builder(void) {
  uint8_t pattern = MORSE_PATTERN_EMPTY;
  unsigned value;

  pattern = morse_add_element(pattern, false);
  TEST_ASSERT_EQUAL(2U, pattern);
  pattern = morse_add_element(pattern, true);
  TEST_ASSERT_EQUAL(5U, pattern);
  TEST_ASSERT_EQUAL('A', morse_decode(pattern));
  TEST_ASSERT_EQUAL(63U, pattern_of("-----"));
  TEST_ASSERT_EQUAL(127U, pattern_of("------"));
  TEST_ASSERT_EQUAL(64U, pattern_of("......"));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, pattern_of("......."));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, pattern_of("-------"));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, pattern_of("........"));
  TEST_ASSERT_EQUAL(MORSE_CHAR_UNKNOWN, morse_decode(pattern_of("........")));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_add_element(MORSE_PATTERN_INVALID, false));
  TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_add_element(MORSE_PATTERN_INVALID, true));
  for (value = 64U; value <= 255U; ++value) {
    TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_add_element((uint8_t)value, false));
    TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, morse_add_element((uint8_t)value, true));
  }
  for (value = 1U; value < 64U; ++value) {
    TEST_ASSERT_EQUAL((value << 1) | 1U, morse_add_element((uint8_t)value, true));
    TEST_ASSERT_EQUAL(value << 1, morse_add_element((uint8_t)value, false));
  }
}

int main(void) {
  TEST_RUN(test_table_entries);
  TEST_RUN(test_pattern_builder);
  return TEST_FINISH();
}
