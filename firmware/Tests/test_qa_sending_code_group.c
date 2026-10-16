/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the shared code-mark helpers (references/internal/training_spec.md D-T47 (1), section 5.11, AC-T46 (3),
 * Q7 mutants M10 to M12; ui-ux.md 5.13.3 and 5.16.3, UX-67). The expected marks come from the CODE(p) oracle of
 * qa/qa_training_screens.h: dot 2 px, dash 6 px, mark gap 3 px, code gap 9 px, x0 = floor((128 - w) / 2).
 */

#include "../src/modes/training_screen_internal.h"
#include "cw/morse.h"
#include "koch/koch.h"
#include "qa/qa_training_screens.h"
#include "support/training_harness.h"

#define CODE_ROW 5U
#define LIVE_ROW 7U

/* The marks of @p pattern as '.' and '-', first element first. */
static const char* code_text(uint8_t pattern) {
  static char text[2][16];
  static size_t slot;
  uint8_t shift = 0U;
  size_t length = 0U;

  slot ^= 1U;
  while ((uint8_t)(pattern >> shift) > 1U) {
    ++shift;
  }
  while (shift != 0U) {
    --shift;
    text[slot][length] = (((pattern >> shift) & 1U) != 0U) ? '-' : '.';
    ++length;
  }
  text[slot][length] = '\0';
  return text[slot];
}

static void clear_row(uint8_t row) { gfx_fill_rect(0, (int16_t)(8U * row), 128, 8, LCD_PIXEL_OFF); }

/* M12: a mark is 2 or 6 px on y 8r+3 and 8r+4; the next x is x + 5 after a dit and x + 9 after a dah. */
static void test_code_mark_geometry(void) {
  static const uint8_t starts[3] = {2U, 41U, 120U};
  static const uint8_t rows[2] = {CODE_ROW, LIVE_ROW};
  size_t start;
  size_t row;
  harness_power_on();
  for (start = 0U; start < 3U; ++start) {
    for (row = 0U; row < 2U; ++row) {
      const uint8_t x = starts[start];
      clear_row(rows[row]);
      TEST_ASSERT_EQUAL(x + 5U, training_screen_draw_code_mark(x, rows[row], false));
      TEST_ASSERT(marks_are(rows[row], x, "."));
      clear_row(rows[row]);
      TEST_ASSERT_EQUAL(x + 9U, training_screen_draw_code_mark(x, rows[row], true));
      TEST_ASSERT(marks_are(rows[row], x, "-"));
    }
  }
  /* A live row of marks from x 2 (ui-ux.md 5.13.3): K is dash 2-7, dot 11-12, dash 16-21. */
  clear_row(LIVE_ROW);
  TEST_ASSERT_EQUAL(11U, training_screen_draw_code_mark(2U, LIVE_ROW, true));
  TEST_ASSERT_EQUAL(16U, training_screen_draw_code_mark(11U, LIVE_ROW, false));
  TEST_ASSERT_EQUAL(25U, training_screen_draw_code_mark(16U, LIVE_ROW, true));
  TEST_ASSERT(marks_are(LIVE_ROW, 2, "-.-"));
}

/* AC-T46 (3), M11: every Koch character alone is centered in row 5, odd and even widths included. */
static void test_code_group_every_koch_character(void) {
  uint8_t index;
  harness_power_on();
  for (index = 0U; index < KOCH_CHAR_COUNT; ++index) {
    const uint8_t pattern = morse_encode(koch_char(index));
    clear_row(CODE_ROW);
    training_screen_draw_code_group(pattern, MORSE_PATTERN_EMPTY);
    if (!TEST_ASSERT(qa_code(code_text(pattern)))) {
      fprintf(stderr, "Koch index %u, code %s\n", (unsigned)index, code_text(pattern));
    }
  }
}

/* AC-T46 (3), M10: two characters form one centered group with the 9 px code gap. */
static void test_code_group_two_characters(void) {
  static const char pairs[][2] = {{'K', 'M'}, {'E', 'T'}, {',', '0'}, {'0', ','}, {'E', 'E'}, {'5', 'J'}};
  char expected[32];
  size_t index;
  harness_power_on();
  for (index = 0U; index < sizeof(pairs) / sizeof(pairs[0]); ++index) {
    const uint8_t first = morse_encode(pairs[index][0]);
    const uint8_t second = morse_encode(pairs[index][1]);
    (void)snprintf(expected, sizeof(expected), "%s %s", code_text(first), code_text(second));
    clear_row(CODE_ROW);
    training_screen_draw_code_group(first, second);
    if (!TEST_ASSERT(qa_code(expected))) {
      fprintf(stderr, "pair %c %c\n", pairs[index][0], pairs[index][1]);
    }
  }
  /* V-137: the L1 group of K and M. */
  clear_row(CODE_ROW);
  training_screen_draw_code_group(morse_encode(koch_char(0U)), morse_encode(koch_char(1U)));
  TEST_ASSERT(qa_code("-.- --"));
}

/* Section 5.11: the group does not clear the row. A pixel outside the marks stays on. */
static void test_code_group_keeps_other_pixels(void) {
  harness_power_on();
  clear_row(CODE_ROW);
  lcd_fb_set_pixel(3, 40, LCD_PIXEL_ON);
  training_screen_draw_code_group(morse_encode('J'), MORSE_PATTERN_EMPTY);
  TEST_ASSERT(pixel(3, 40));
  lcd_fb_set_pixel(3, 40, LCD_PIXEL_OFF);
  TEST_ASSERT(qa_code(".---"));
}

int main(void) {
  TEST_RUN(test_code_mark_geometry);
  TEST_RUN(test_code_group_every_koch_character);
  TEST_RUN(test_code_group_two_characters);
  TEST_RUN(test_code_group_keeps_other_pixels);
  return TEST_FINISH();
}
