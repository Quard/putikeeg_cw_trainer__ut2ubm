#ifndef FREE_INPUT_SCREEN_H
#define FREE_INPUT_SCREEN_H

/*
 * Reference checks of the Free Input text area (references/internal/ui-ux.md sections 5.9 and 9.2) against the
 * golden 5 x 8 font. A row is 25 cells of 5 x 8 pixels from x = 2, in normal video. Include it in one test source
 * per executable.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/font_golden.h"
#include "lcd/lcd_fb.h"

#define SCREEN_LINE_ROW "\x01" /* row_is(): the delimiter line instead of text */

static inline bool screen_pixel(int x, int y) { return lcd_fb_get_pixel((int16_t)x, (int16_t)y); }

static inline bool screen_golden_ink(char character, int column, int row) {
  const unsigned code = (unsigned char)character;
  if ((code < FONT_GOLDEN_5X8_FIRST) || (code > FONT_GOLDEN_5X8_LAST)) {
    return false;
  }
  return (font_golden_5x8[code - FONT_GOLDEN_5X8_FIRST].rows[row] & (1U << (4 - column))) != 0U;
}

/*
 * Row @p r holds exactly @p text from column 0 (NULL or "" for a blank row), the rest of the row blank. With
 * SCREEN_LINE_ROW the row holds only the delimiter: pixels (x, 8r + 3) on for x = 2..126.
 */
static inline bool row_is(int r, const char* text) {
  const bool is_line = (text != NULL) && (strcmp(text, SCREEN_LINE_ROW) == 0);
  const int length = ((text == NULL) || is_line) ? 0 : (int)strlen(text);
  int x;
  int y;

  for (y = 8 * r; y < 8 * r + 8; ++y) {
    for (x = 0; x < 128; ++x) {
      bool expected = false;
      if (is_line) {
        expected = (y == 8 * r + 3) && (x >= 2) && (x <= 126);
      } else if ((x >= 2) && (x < 2 + 5 * length)) {
        expected = screen_golden_ink(text[(x - 2) / 5], (x - 2) % 5, y - 8 * r);
      }
      if (screen_pixel(x, y) != expected) {
        fprintf(stderr, "row %d (\"%s\"): pixel (%d, %d) is %d\n", r, is_line ? "LINE" : (text ? text : ""), x, y,
                (int)screen_pixel(x, y));
        return false;
      }
    }
  }
  return true;
}

/* Rows 1-7 hold @p rows[0] .. @p rows[6]. */
static inline bool text_area_is(const char* const rows[7]) {
  int r;
  bool is_ok = true;
  for (r = 1; r <= 7; ++r) {
    is_ok = row_is(r, rows[r - 1]) && is_ok;
  }
  return is_ok;
}

static inline bool text_area_is_blank(void) {
  static const char* const blank[7] = {NULL, NULL, NULL, NULL, NULL, NULL, NULL};
  return text_area_is(blank);
}

/* A line of 25 copies of @p character. */
static inline const char* full_line(char character, char* buffer) {
  memset(buffer, character, 25U);
  buffer[25] = '\0';
  return buffer;
}

/* ITU codes of the letters and digits, for test input. Returns NULL for other characters. */
static inline const char* morse_code_of(char character) {
  static const char* const letters[26] = {".-",   "-...", "-.-.", "-..",  ".",   "..-.", "--.",  "....", "..",
                                          ".---", "-.-",  ".-..", "--",   "-.",  "---",  ".--.", "--.-", ".-.",
                                          "...",  "-",    "..-",  "...-", ".--", "-..-", "-.--", "--.."};
  static const char* const digits[10] = {"-----", ".----", "..---", "...--", "....-",
                                         ".....", "-....", "--...", "---..", "----."};
  if ((character >= 'A') && (character <= 'Z')) {
    return letters[character - 'A'];
  }
  if ((character >= '0') && (character <= '9')) {
    return digits[character - '0'];
  }
  return NULL;
}

#endif /* FREE_INPUT_SCREEN_H */
