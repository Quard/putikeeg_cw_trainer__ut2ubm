/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Host tests of the prosign codes and morse_format() (references/internal/keyer_spec.md section 17, AC-P1 to AC-P4). */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "cw/morse.h"
#include "settings/settings.h"
#include "test_assert.h"

#define GUARD_BYTE 0xA5U
#define GUARD_SIZE 8U

typedef struct {
  const char* code;
  char decoded;
  const char* texts[3]; /* by style: Punctuation, Prosign, Both */
} prosign_case_t;

/* keyer_spec.md section 17.4.1. */
static const prosign_case_t prosign_cases[] = {
    {"...-.-", MORSE_CHAR_SK, {"<SK>", "<SK>", "<SK>"}}, {".-.-.", MORSE_CHAR_AR, {"+", "<AR>", "<AR/+>"}},
    {"-...-", MORSE_CHAR_BT, {"=", "<BT>", "<BT/=>"}},   {"-.--.", MORSE_CHAR_KN, {"(", "<KN>", "<KN/(>"}},
    {".-...", MORSE_CHAR_AS, {"&", "<AS>", "<AS/&>"}},
};
#define PROSIGN_CASE_COUNT (sizeof(prosign_cases) / sizeof(prosign_cases[0]))

static const uint8_t extra_styles[] = {3U, 255U};
static const uint8_t all_styles[] = {0U, 1U, 2U, 3U, 255U};

static uint8_t pattern_of(const char* code) {
  uint8_t pattern = MORSE_PATTERN_EMPTY;
  for (; *code != '\0'; ++code) {
    pattern = morse_add_element(pattern, *code == '-');
  }
  return pattern;
}

/* Formats into a buffer of MORSE_TEXT_SIZE bytes plus a guard, and checks that the guard and the NUL hold. */
static uint8_t format_checked(uint8_t value, uint8_t style, char* text) {
  char buffer[MORSE_TEXT_SIZE + GUARD_SIZE];
  uint8_t length;
  size_t index;

  memset(buffer, (int)GUARD_BYTE, sizeof(buffer));
  length = morse_format((char)value, style, buffer);
  TEST_ASSERT(length >= 1U);
  TEST_ASSERT(length <= MORSE_TEXT_MAX_LENGTH);
  TEST_ASSERT_EQUAL('\0', buffer[length]);
  for (index = (size_t)length + 1U; index < sizeof(buffer); ++index) {
    if (!TEST_ASSERT_EQUAL(GUARD_BYTE, (uint8_t)buffer[index])) {
      fprintf(stderr, "value 0x%02X style %u: byte %zu written\n", value, style, index);
    }
  }
  memcpy(text, buffer, MORSE_TEXT_SIZE);
  return length;
}

static void check_text(uint8_t value, uint8_t style, const char* expected) {
  char text[MORSE_TEXT_SIZE];
  const uint8_t length = format_checked(value, style, text);
  TEST_ASSERT_EQUAL(strlen(expected), length);
  if (!TEST_ASSERT(strcmp(expected, text) == 0)) {
    fprintf(stderr, "value 0x%02X style %u: expected \"%s\", got \"%s\"\n", value, style, expected, text);
  }
}

/* AC-P1 */
static void test_prosign_codes_in_table(void) {
  static const char other_marks[] = {'+', '=', '(', '&', '<', '>', '*', ' '};
  unsigned value;
  size_t index;

  TEST_ASSERT_EQUAL(0x01U, (uint8_t)MORSE_CHAR_SK);
  TEST_ASSERT_EQUAL(0x02U, (uint8_t)MORSE_CHAR_AR);
  TEST_ASSERT_EQUAL(0x03U, (uint8_t)MORSE_CHAR_BT);
  TEST_ASSERT_EQUAL(0x04U, (uint8_t)MORSE_CHAR_KN);
  TEST_ASSERT_EQUAL(0x05U, (uint8_t)MORSE_CHAR_AS);
  TEST_ASSERT_EQUAL(0x00U, (uint8_t)MORSE_CHAR_UNKNOWN);
  TEST_ASSERT_EQUAL(40U, pattern_of(".-..."));
  TEST_ASSERT_EQUAL(42U, pattern_of(".-.-."));
  TEST_ASSERT_EQUAL(49U, pattern_of("-...-"));
  TEST_ASSERT_EQUAL(54U, pattern_of("-.--."));
  TEST_ASSERT_EQUAL(69U, pattern_of("...-.-"));
  TEST_ASSERT_EQUAL(MORSE_CHAR_AS, morse_decode(40U));
  TEST_ASSERT_EQUAL(MORSE_CHAR_AR, morse_decode(42U));
  TEST_ASSERT_EQUAL(MORSE_CHAR_BT, morse_decode(49U));
  TEST_ASSERT_EQUAL(MORSE_CHAR_KN, morse_decode(54U));
  TEST_ASSERT_EQUAL(MORSE_CHAR_SK, morse_decode(69U));
  for (value = 0U; value <= 255U; ++value) {
    const uint8_t decoded = (uint8_t)morse_decode((uint8_t)value);
    TEST_ASSERT(!((decoded >= 0x06U) && (decoded <= 0x1FU)));
    TEST_ASSERT(!((decoded >= (uint8_t)'a') && (decoded <= (uint8_t)'z')));
    for (index = 0U; index < sizeof(other_marks); ++index) {
      TEST_ASSERT(decoded != (uint8_t)other_marks[index]);
    }
    if ((decoded >= 0x01U) && (decoded <= 0x05U)) {
      TEST_ASSERT((value == 40U) || (value == 42U) || (value == 49U) || (value == 54U) || (value == 69U));
    }
  }
}

/* AC-P2 */
static void test_prosign_texts(void) {
  size_t index;
  uint8_t style;

  for (index = 0U; index < PROSIGN_CASE_COUNT; ++index) {
    const prosign_case_t* entry = &prosign_cases[index];
    TEST_ASSERT_EQUAL((uint8_t)entry->decoded, (uint8_t)morse_decode(pattern_of(entry->code)));
    for (style = 0U; style < 3U; ++style) {
      check_text((uint8_t)entry->decoded, style, entry->texts[style]);
    }
    for (style = 0U; style < sizeof(extra_styles); ++style) {
      check_text((uint8_t)entry->decoded, extra_styles[style], entry->texts[MORSE_STYLE_BOTH]);
    }
  }
}

/* AC-P3 */
static void test_other_values(void) {
  unsigned value;
  size_t style;

  for (value = 0U; value <= 255U; ++value) {
    char expected[2] = {'\0', '\0'};
    if ((value >= 0x01U) && (value <= 0x05U)) {
      continue;
    }
    expected[0] = ((value >= 0x20U) && (value <= 0x7EU)) ? (char)value : '*';
    for (style = 0U; style < sizeof(all_styles); ++style) {
      check_text((uint8_t)value, all_styles[style], expected);
    }
  }
  TEST_ASSERT_EQUAL('*', MORSE_TEXT_UNKNOWN);
}

/* AC-P4: every value and style, with a guard after the text; then an exact-size heap buffer for ASan. */
static void test_bounds(void) {
  unsigned value;
  size_t style;

  TEST_ASSERT_EQUAL(6U, MORSE_TEXT_MAX_LENGTH);
  TEST_ASSERT_EQUAL(7U, MORSE_TEXT_SIZE);
  for (value = 0U; value <= 255U; ++value) {
    for (style = 0U; style < sizeof(all_styles); ++style) {
      char text[MORSE_TEXT_SIZE];
      (void)format_checked((uint8_t)value, all_styles[style], text);
    }
  }
  for (value = 0U; value <= 255U; ++value) {
    for (style = 0U; style <= 255U; ++style) {
      char* exact = malloc(MORSE_TEXT_SIZE);
      TEST_ASSERT(exact != NULL);
      if (exact != NULL) {
        const uint8_t length = morse_format((char)value, (uint8_t)style, exact);
        TEST_ASSERT(length <= MORSE_TEXT_MAX_LENGTH);
        TEST_ASSERT_EQUAL(length, strlen(exact));
        free(exact);
      }
    }
  }
}

/* AC-P5: the morse style codes equal the settings values. */
static void test_style_codes(void) {
  TEST_ASSERT_EQUAL(0U, MORSE_STYLE_PUNCTUATION);
  TEST_ASSERT_EQUAL(1U, MORSE_STYLE_NAME);
  TEST_ASSERT_EQUAL(2U, MORSE_STYLE_BOTH);
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_PUNCTUATION, MORSE_STYLE_PUNCTUATION);
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_NAME, MORSE_STYLE_NAME);
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, MORSE_STYLE_BOTH);
}

int main(void) {
  TEST_RUN(test_prosign_codes_in_table);
  TEST_RUN(test_prosign_texts);
  TEST_RUN(test_other_values);
  TEST_RUN(test_bounds);
  TEST_RUN(test_style_codes);
  return TEST_FINISH();
}
