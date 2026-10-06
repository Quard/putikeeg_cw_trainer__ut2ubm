/*
 * QA tests of the prosign decoder codes and morse_format() (references/internal/keyer_spec.md sections 17.3.1 and
 * 17.4.1, AC-P1 to AC-P5, D-P11). An independent reference model of the 17.4.1 table runs against morse_format()
 * for all 256 decoded values and all 256 styles, in a buffer with guards before and after it, and in a heap buffer
 * of exactly MORSE_TEXT_SIZE bytes for ASan.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "cw/morse.h"
#include "settings/settings.h"
#include "test_assert.h"

#define GUARD_BEFORE 8U
#define GUARD_AFTER 8U
#define BUFFER_SIZE (GUARD_BEFORE + MORSE_TEXT_SIZE + GUARD_AFTER)

/* keyer_spec.md section 17.1: decoder code, ITU code string, name and mark ('\0': none) of each prosign. */
typedef struct {
  uint8_t value;
  const char* code;
  const char* name;
  char mark;
} model_prosign_t;

static const model_prosign_t model_prosigns[5] = {
    {0x01U, "...-.-", "SK", '\0'}, {0x02U, ".-.-.", "AR", '+'}, {0x03U, "-...-", "BT", '='},
    {0x04U, "-.--.", "KN", '('},   {0x05U, ".-...", "AS", '&'},
};

static uint8_t pattern_of(const char* code) {
  uint8_t pattern = 1U;
  for (; *code != '\0'; ++code) {
    pattern = (uint8_t)((pattern << 1) | ((*code == '-') ? 1U : 0U));
  }
  return pattern;
}

/* Reference model of the table in keyer_spec.md section 17.4.1. Writes the text to @p text, returns its length. */
static size_t model_text(uint8_t value, uint8_t style, char* text) {
  size_t i;
  for (i = 0U; i < 5U; ++i) {
    const model_prosign_t* prosign = &model_prosigns[i];
    if (prosign->value != value) {
      continue;
    }
    if (prosign->mark == '\0') {
      (void)snprintf(text, MORSE_TEXT_SIZE, "<%s>", prosign->name);
    } else if (style == 0U) {
      text[0] = prosign->mark;
      text[1] = '\0';
    } else if (style == 1U) {
      (void)snprintf(text, MORSE_TEXT_SIZE, "<%s>", prosign->name);
    } else {
      (void)snprintf(text, MORSE_TEXT_SIZE, "<%s/%c>", prosign->name, prosign->mark);
    }
    return strlen(text);
  }
  text[0] = ((value >= 0x20U) && (value <= 0x7EU)) ? (char)value : '*';
  text[1] = '\0';
  return 1U;
}

/* AC-P1 and D-P11: the codes in the decoder table are the MORSE_CHAR_* values, 0x01 to 0x05, all different. */
static void test_decoder_codes(void) {
  static const char forbidden[] = {'+', '=', '(', '&', '<', '>', '*', ' '};
  size_t i;
  unsigned pattern;
  unsigned prosign_slots = 0U;

  TEST_ASSERT_EQUAL(0x01U, (uint8_t)MORSE_CHAR_SK);
  TEST_ASSERT_EQUAL(0x02U, (uint8_t)MORSE_CHAR_AR);
  TEST_ASSERT_EQUAL(0x03U, (uint8_t)MORSE_CHAR_BT);
  TEST_ASSERT_EQUAL(0x04U, (uint8_t)MORSE_CHAR_KN);
  TEST_ASSERT_EQUAL(0x05U, (uint8_t)MORSE_CHAR_AS);
  for (i = 0U; i < 5U; ++i) {
    TEST_ASSERT_EQUAL(model_prosigns[i].value, (uint8_t)morse_decode(pattern_of(model_prosigns[i].code)));
  }
  TEST_ASSERT_EQUAL(40U, pattern_of(".-..."));
  TEST_ASSERT_EQUAL(42U, pattern_of(".-.-."));
  TEST_ASSERT_EQUAL(49U, pattern_of("-...-"));
  TEST_ASSERT_EQUAL(54U, pattern_of("-.--."));
  for (pattern = 0U; pattern <= 0xFFU; ++pattern) {
    const uint8_t decoded = (uint8_t)morse_decode((uint8_t)pattern);
    if ((decoded >= 0x01U) && (decoded <= 0x05U)) {
      ++prosign_slots;
    }
    TEST_ASSERT(!((decoded >= 0x06U) && (decoded <= 0x1FU)));
    TEST_ASSERT(decoded <= 0x7EU);
    TEST_ASSERT(!((decoded >= (uint8_t)'a') && (decoded <= (uint8_t)'z')));
    for (i = 0U; i < sizeof(forbidden); ++i) {
      TEST_ASSERT(decoded != (uint8_t)forbidden[i]);
    }
  }
  TEST_ASSERT_EQUAL(5U, prosign_slots);
}

/* AC-P2 to AC-P4: every decoded value and every style against the model, with guards around the buffer. */
static void test_format_against_model(void) {
  unsigned value;
  unsigned style;
  unsigned long mismatches = 0U;

  for (value = 0U; value <= 0xFFU; ++value) {
    for (style = 0U; style <= 0xFFU; ++style) {
      char buffer[BUFFER_SIZE];
      char expected[MORSE_TEXT_SIZE + 1U];
      char* const text = &buffer[GUARD_BEFORE];
      const size_t expected_length = model_text((uint8_t)value, (uint8_t)style, expected);
      size_t index;
      uint8_t length;
      bool is_ok;

      memset(buffer, 0x5A, sizeof(buffer));
      length = morse_format((char)value, (uint8_t)style, text);
      is_ok = (length == expected_length) && (length >= 1U) && (length <= MORSE_TEXT_MAX_LENGTH) &&
              (memcmp(text, expected, expected_length + 1U) == 0);
      for (index = 0U; index < GUARD_BEFORE; ++index) {
        is_ok = is_ok && (buffer[index] == 0x5A);
      }
      for (index = GUARD_BEFORE + (size_t)length + 1U; index < sizeof(buffer); ++index) {
        is_ok = is_ok && (buffer[index] == 0x5A);
      }
      if (!is_ok) {
        ++mismatches;
        if (mismatches <= 10U) {
          fprintf(stderr, "value 0x%02X style %u: expected \"%s\" (%zu), got length %u\n", value, style, expected,
                  expected_length, (unsigned)length);
        }
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, mismatches);
}

/* AC-P4: an exact-size heap buffer for every value and style; ASan stops on a write past it. */
static void test_exact_size_buffer(void) {
  unsigned value;
  unsigned style;
  for (value = 0U; value <= 0xFFU; ++value) {
    for (style = 0U; style <= 0xFFU; ++style) {
      char* const exact = malloc(MORSE_TEXT_SIZE);
      uint8_t length;
      if (!TEST_ASSERT(exact != NULL)) {
        return;
      }
      length = morse_format((char)value, (uint8_t)style, exact);
      TEST_ASSERT(length <= MORSE_TEXT_MAX_LENGTH);
      TEST_ASSERT_EQUAL('\0', exact[length]);
      TEST_ASSERT(memchr(exact, '\0', length) == NULL);
      free(exact);
    }
  }
}

/* The six-cell texts exist and fill the buffer exactly: MORSE_TEXT_MAX_LENGTH is a tight bound. */
static void test_max_length_is_reached(void) {
  char text[MORSE_TEXT_SIZE];
  TEST_ASSERT_EQUAL(6U, MORSE_TEXT_MAX_LENGTH);
  TEST_ASSERT_EQUAL(MORSE_TEXT_MAX_LENGTH + 1U, MORSE_TEXT_SIZE);
  TEST_ASSERT_EQUAL(MORSE_TEXT_MAX_LENGTH, morse_format(MORSE_CHAR_KN, MORSE_STYLE_BOTH, text));
  TEST_ASSERT_EQUAL(0, strcmp("<KN/(>", text));
  TEST_ASSERT_EQUAL(4U, morse_format(MORSE_CHAR_SK, MORSE_STYLE_PUNCTUATION, text));
  TEST_ASSERT_EQUAL(0, strcmp("<SK>", text));
}

/* The formatter keeps no state: the same call after a call with another value and style gives the same text. */
static void test_no_state_between_calls(void) {
  char first[MORSE_TEXT_SIZE];
  char second[MORSE_TEXT_SIZE];
  unsigned value;
  for (value = 0U; value <= 0xFFU; ++value) {
    const uint8_t length = morse_format((char)value, MORSE_STYLE_BOTH, first);
    (void)morse_format(MORSE_CHAR_AS, MORSE_STYLE_PUNCTUATION, second);
    (void)morse_format((char)(0xFFU - value), MORSE_STYLE_NAME, second);
    TEST_ASSERT_EQUAL(length, morse_format((char)value, MORSE_STYLE_BOTH, second));
    TEST_ASSERT_EQUAL(0, strcmp(first, second));
  }
}

/* AC-P5 and D-P11: the style codes equal the settings values; the default style is Both. */
static void test_style_codes(void) {
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_PUNCTUATION, MORSE_STYLE_PUNCTUATION);
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_NAME, MORSE_STYLE_NAME);
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, MORSE_STYLE_BOTH);
  TEST_ASSERT_EQUAL(0U, MORSE_STYLE_PUNCTUATION);
  TEST_ASSERT_EQUAL(1U, MORSE_STYLE_NAME);
  TEST_ASSERT_EQUAL(2U, MORSE_STYLE_BOTH);
  TEST_ASSERT_EQUAL('*', MORSE_TEXT_UNKNOWN);
}

int main(void) {
  TEST_RUN(test_decoder_codes);
  TEST_RUN(test_format_against_model);
  TEST_RUN(test_exact_size_buffer);
  TEST_RUN(test_max_length_is_reached);
  TEST_RUN(test_no_state_between_calls);
  TEST_RUN(test_style_codes);
  return TEST_FINISH();
}
