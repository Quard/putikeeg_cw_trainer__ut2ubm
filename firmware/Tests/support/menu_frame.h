#ifndef MENU_FRAME_H
#define MENU_FRAME_H

/*
 * Expected-frame builder for the startup screen and About tests (references/internal/ui-ux.md sections 5.17, 5.18
 * and 9.5). It draws reference frames with the golden fonts of fixtures/font_golden.h, not with gfx, and compares
 * every framebuffer pixel. Include it in one test source per executable.
 */

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "fixtures/font_golden.h"
#include "lcd/lcd_fb.h"

#define FRAME_WIDTH 128
#define FRAME_HEIGHT 64
#define FRAME_CELL_X(column) (2 + 5 * (column))

static bool frame_expected[FRAME_HEIGHT][FRAME_WIDTH];
static bool frame_has_missing_glyph;

static inline void frame_clear(void) {
  memset(frame_expected, 0, sizeof(frame_expected));
  frame_has_missing_glyph = false;
}

static inline void frame_set(int x, int y, bool is_on) {
  if ((x >= 0) && (x < FRAME_WIDTH) && (y >= 0) && (y < FRAME_HEIGHT)) {
    frame_expected[y][x] = is_on;
  }
}

static inline void frame_fill(int x, int y, int width, int height, bool is_on) {
  int row;
  int column;
  for (row = y; row < y + height; ++row) {
    for (column = x; column < x + width; ++column) {
      frame_set(column, row, is_on);
    }
  }
}

/* 5 x 8 text with its cell top-left at (x, y); inverted text has ink off on a cell background that is on. */
static inline void frame_text5(int x, int y, const char* text, bool is_inverted) {
  for (; *text != '\0'; ++text, x += 5) {
    const unsigned code = (unsigned char)*text;
    int row;
    int column;
    if ((code < FONT_GOLDEN_5X8_FIRST) || (code > FONT_GOLDEN_5X8_LAST)) {
      frame_has_missing_glyph = true;
      continue;
    }
    for (row = 0; row < 8; ++row) {
      for (column = 0; column < 5; ++column) {
        const bool is_ink = (font_golden_5x8[code - FONT_GOLDEN_5X8_FIRST].rows[row] & (1U << (4 - column))) != 0U;
        frame_set(x + column, y + row, is_ink != is_inverted);
      }
    }
  }
}

static inline const font_golden_8x16_t* frame_glyph16(char character) {
  unsigned index;
  for (index = 0U; index < FONT_GOLDEN_8X16_COUNT; ++index) {
    if (font_golden_8x16[index].encoding == (uint8_t)character) {
      return &font_golden_8x16[index];
    }
  }
  return NULL;
}

/* 8 x 16 text in normal video with its cell top-left at (x, y). A space has no ink. */
static inline void frame_big16(int x, int y, const char* text) {
  for (; *text != '\0'; ++text, x += 8) {
    const font_golden_8x16_t* glyph = frame_glyph16(*text);
    int row;
    int column;
    if (*text == ' ') {
      continue;
    }
    if (glyph == NULL) {
      frame_has_missing_glyph = true;
      continue;
    }
    for (row = 0; row < (int)FONT_GOLDEN_8X16_ROWS; ++row) {
      for (column = 0; column < 8; ++column) {
        if ((glyph->rows[row] & (1U << (7 - column))) != 0U) {
          frame_set(x + column, y + row, true);
        }
      }
    }
  }
}

/* ui-ux.md section 5.1: inverted title bar with the text from column 0. */
static inline void frame_title(const char* title) {
  frame_fill(0, 0, FRAME_WIDTH, 8, true);
  frame_text5(FRAME_CELL_X(0), 0, title, true);
}

/*
 * ui-ux.md section 5.2: list row @p r with @p label, an optional @p value that ends in column 22 and an optional
 * marker in column 24. A selected row has a band on y 8r+1..8r+7 and a gap line on y 8r.
 */
static inline void frame_list_row(int r, const char* label, const char* value, bool has_marker, bool is_selected) {
  if (is_selected) {
    frame_fill(0, 8 * r + 1, FRAME_WIDTH, 7, true);
  }
  frame_text5(FRAME_CELL_X(0), 8 * r, label, is_selected);
  if (value != NULL) {
    frame_text5(FRAME_CELL_X(23 - (int)strlen(value)), 8 * r, value, is_selected);
  }
  if (has_marker) {
    frame_text5(FRAME_CELL_X(24), 8 * r, ">", is_selected);
  }
  if (is_selected) {
    frame_fill(0, 8 * r, FRAME_WIDTH, 1, false);
  }
}

/* MAIN of ui-ux.md section 9.5 with no stored progress: item @p selected (0..4) under the cursor. */
static inline void frame_main(int selected) {
  frame_clear();
  frame_title("CW Trainer");
  frame_list_row(1, "Receiving", "L1", false, selected == 0);
  frame_list_row(2, "Sending", "L1", false, selected == 1);
  frame_list_row(3, "Free Input", NULL, false, selected == 2);
  frame_list_row(4, "Settings", NULL, true, selected == 3);
  frame_list_row(5, "About", NULL, false, selected == 4);
}

/* SPLASH(v) of ui-ux.md section 9.5: @p version_text is "v" + the version. */
static inline void frame_splash(const char* version_text) {
  frame_clear();
  frame_text5(82, 8, "Putikeeg", false);
  frame_big16(7, 16, "CW Trainer");
  frame_text5(7, 32, "mod by UT2UBM", false);
  frame_text5(122 - 5 * (int)strlen(version_text), 56, version_text, false);
}

/* ABOUT(v) of ui-ux.md section 9.5: @p version_row is "Version " + the version. */
static inline void frame_about(const char* version_row) {
  frame_clear();
  frame_title("About");
  frame_text5(FRAME_CELL_X(0), 8, "Putikeeg CW Trainer", false);
  frame_text5(FRAME_CELL_X(0), 16, "mod by UT2UBM", false);
  frame_text5(FRAME_CELL_X(0), 24, version_row, false);
  frame_text5(FRAME_CELL_X(0), 40, "https://github.com/Quard/", false);
  frame_text5(FRAME_CELL_X(0), 48, "putikeeg_cw_trainer__", false);
  frame_text5(FRAME_CELL_X(0), 56, "ut2ubm", false);
}

/* ui-ux.md sections 5.8 and 8.1: the Display Defaults message with the focused OK button. */
static inline void frame_display_defaults(void) {
  frame_clear();
  frame_title("Display Defaults");
  frame_text5(FRAME_CELL_X(2), 16, "Default contrast and", false);
  frame_text5(FRAME_CELL_X(2), 24, "backlight are in use.", false);
  frame_fill(52, 41, 20, 7, true);
  frame_text5(FRAME_CELL_X(11), 40, "OK", true);
  frame_fill(52, 40, 20, 1, false);
}

/* Compares every pixel of lcd_fb with the expected frame and prints the first differences. */
static inline bool frame_matches(const char* name) {
  int x;
  int y;
  int differences = 0;
  if (frame_has_missing_glyph) {
    fprintf(stderr, "frame %s: a glyph is missing from the golden font\n", name);
    return false;
  }
  for (y = 0; y < FRAME_HEIGHT; ++y) {
    for (x = 0; x < FRAME_WIDTH; ++x) {
      if (lcd_fb_get_pixel((int16_t)x, (int16_t)y) != frame_expected[y][x]) {
        if (differences < 8) {
          fprintf(stderr, "frame %s: pixel (%d, %d) is %d\n", name, x, y, (int)frame_expected[y][x] ? 0 : 1);
        }
        ++differences;
      }
    }
  }
  return differences == 0;
}

#endif /* MENU_FRAME_H */
