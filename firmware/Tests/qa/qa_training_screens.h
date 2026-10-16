/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef QA_TRAINING_SCREENS_H
#define QA_TRAINING_SCREENS_H

/*
 * QA probes of the start screen and the end screen (references/internal/ui-ux.md 5.15, 5.16.1 to 5.16.3 and the
 * 9.4 notation LVL, LABEL, DESC, CODE, STARTBTN, OKBTN; training_spec.md SL-1 to SL-10). They are written apart from
 * the probes of support/training_harness.h, so the QA tests do not share the start screen oracle with the developer
 * tests. The mark positions and the band geometry come from the formulas of the documents. The 8 x 16 text uses a
 * reference rendering, as BIG of 9.4 defines it; the tests also check its ink box against the ui-ux.md tables.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "support/training_harness.h"

#define QA_WIDTH ((int)LCD_WIDTH)
#define QA_BIG_Y 24
#define QA_CODE_ROW 5
#define QA_CODE_MARK_GAP 3
#define QA_CODE_CHAR_GAP 9
#define QA_DOT_WIDTH 2
#define QA_DASH_WIDTH 6
#define QA_PAGES_0_TO_5 ((uint8_t)0x3FU)
#define QA_ALL_PAGES ((uint8_t)0xFFU)

typedef struct {
  int x0;
  int x1;
  int y0;
  int y1;
} qa_box_t;

/*
 * CODE(p) of ui-ux.md 9.4: sets @p is_on for every mark column of pattern @p p. A space is a code gap of 9 px in
 * place of the 3 px mark gap. The group is centered: x0 = floor((128 - w) / 2). Returns w (0 for an empty pattern).
 */
static inline int qa_code_columns(const char* pattern, bool is_on[LCD_WIDTH]) {
  int width = 0;
  int pass;
  memset(is_on, 0, LCD_WIDTH);
  for (pass = 0; pass < 2; ++pass) {
    const int x0 = (pass == 0) ? 0 : (QA_WIDTH - width) / 2;
    int x = x0;
    int gap = 0;
    bool has_mark = false;
    size_t index;
    for (index = 0U; pattern[index] != '\0'; ++index) {
      const int mark_width = (pattern[index] == '-') ? QA_DASH_WIDTH : QA_DOT_WIDTH;
      int k;
      if (pattern[index] == ' ') {
        gap = QA_CODE_CHAR_GAP;
        continue;
      }
      if (has_mark) {
        x += gap;
      }
      for (k = 0; (pass == 1) && (k < mark_width); ++k) {
        if ((x + k >= 0) && (x + k < QA_WIDTH)) {
          is_on[x + k] = true;
        }
      }
      x += mark_width;
      gap = QA_CODE_MARK_GAP;
      has_mark = true;
    }
    if (pass == 0) {
      width = x;
    }
  }
  return width;
}

/* CODE(p) in row 5, CODE() for an empty pattern: all other pixels of row 5 off. */
static inline bool qa_code(const char* pattern) {
  bool is_on[LCD_WIDTH];
  int x;
  int y;
  (void)qa_code_columns(pattern, is_on);
  for (y = 8 * QA_CODE_ROW; y < 8 * QA_CODE_ROW + 8; ++y) {
    for (x = 0; x < QA_WIDTH; ++x) {
      const bool expected = ((y == 8 * QA_CODE_ROW + 3) || (y == 8 * QA_CODE_ROW + 4)) && is_on[x];
      if (pixel(x, y) != expected) {
        fprintf(stderr, "CODE(\"%s\"): pixel (%d, %d) is %d\n", pattern, x, y, (int)pixel(x, y));
        return false;
      }
    }
  }
  return true;
}

/*
 * Reference rendering of BIG(x, s): @p out gets y 24-39 with only @p text in FONTS_ID_8X16 from (@p x, 24). The
 * framebuffer and its dirty flags stay as they were.
 */
static inline void qa_big_reference(const char* text, int x, bool out[16][LCD_WIDTH]) {
  uint8_t saved[2U * LCD_WIDTH];
  const uint8_t dirty = lcd_fb_dirty_mask();
  uint8_t page;
  int column;
  int y;

  memcpy(saved, lcd_fb_page(3U), LCD_WIDTH);
  memcpy(&saved[LCD_WIDTH], lcd_fb_page(4U), LCD_WIDTH);
  gfx_fill_rect(0, QA_BIG_Y, LCD_WIDTH, 16, LCD_PIXEL_OFF);
  (void)gfx_set_font(FONTS_ID_8X16);
  (void)gfx_draw_text((int16_t)x, QA_BIG_Y, text, GFX_TEXT_NORMAL);
  (void)gfx_set_font(FONTS_ID_5X8);
  for (y = 0; y < 16; ++y) {
    for (column = 0; column < QA_WIDTH; ++column) {
      out[y][column] = pixel(column, QA_BIG_Y + y);
    }
  }
  memcpy(lcd_fb_page(3U), saved, LCD_WIDTH);
  memcpy(lcd_fb_page(4U), &saved[LCD_WIDTH], LCD_WIDTH);
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    if ((dirty & (1U << page)) == 0U) {
      lcd_fb_clear_dirty(page);
    }
  }
}

/* The x of the large text of ui-ux.md 5.16.3: x = 64 - 4 m, m = the cells of the text. */
static inline int qa_big_x(const char* text) { return 64 - 4 * (int)strlen(text); }

/* Pages 3 and 4 hold only BIG(64 - 4 m, s): ITEMLINE() with one BIG of the whole text. */
static inline bool qa_big_line(const char* text) {
  bool expected[16][LCD_WIDTH];
  int x;
  int y;
  qa_big_reference(text, qa_big_x(text), expected);
  for (y = 0; y < 16; ++y) {
    for (x = 0; x < QA_WIDTH; ++x) {
      if (pixel(x, QA_BIG_Y + y) != expected[y][x]) {
        fprintf(stderr, "large text \"%s\" at x %d: pixel (%d, %d) is %d\n", text, qa_big_x(text), x, QA_BIG_Y + y,
                (int)pixel(x, QA_BIG_Y + y));
        return false;
      }
    }
  }
  return true;
}

/* The box of the pixels that are on in y @p y0 to @p y1. Returns false when no pixel is on. */
static inline bool qa_ink_box(int y0, int y1, qa_box_t* box) {
  int x;
  int y;
  bool has_ink = false;
  for (y = y0; y <= y1; ++y) {
    for (x = 0; x < QA_WIDTH; ++x) {
      if (pixel(x, y)) {
        if (!has_ink) {
          box->x0 = x;
          box->x1 = x;
          box->y0 = y;
          box->y1 = y;
          has_ink = true;
        }
        box->x0 = (x < box->x0) ? x : box->x0;
        box->x1 = (x > box->x1) ? x : box->x1;
        box->y0 = (y < box->y0) ? y : box->y0;
        box->y1 = (y > box->y1) ? y : box->y1;
      }
    }
  }
  return has_ink;
}

/* The ink of the large text is the box x0..x1, y0..y1 of the ui-ux.md 5.16.1 and 5.16.3 tables. */
static inline bool qa_big_ink_is(int x0, int x1, int y0, int y1) {
  qa_box_t box;
  if (!qa_ink_box(QA_BIG_Y, QA_BIG_Y + 15, &box)) {
    fprintf(stderr, "large text: no ink\n");
    return false;
  }
  if ((box.x0 != x0) || (box.x1 != x1) || (box.y0 != y0) || (box.y1 != y1)) {
    fprintf(stderr, "large text ink x %d-%d y %d-%d, expected x %d-%d y %d-%d\n", box.x0, box.x1, box.y0, box.y1, x0,
            x1, y0, y1);
    return false;
  }
  return true;
}

/*
 * LVL(s, e) of ui-ux.md 9.4 in row 1 (UX-65). The markers follow the indexes: '<' when @p has_prev, '>' when
 * @p has_next. They differ from s > 1 and s < e only at the D-T5 saturation (V-148).
 */
static inline bool qa_lvl_markers(unsigned shown, unsigned earned, bool has_prev, bool has_next) {
  char text[24];
  row_text_t texts[3];
  size_t count = 1U;
  int length;
  int column;

  (void)snprintf(text, sizeof(text), "L%u of %u", shown, earned);
  length = (int)strlen(text);
  column = (19 - length) / 2;
  texts[0].column = column + 3;
  texts[0].text = text;
  texts[0].is_inverted = false;
  if (has_prev) {
    texts[count].column = column;
    texts[count].text = "<";
    texts[count].is_inverted = false;
    ++count;
  }
  if (has_next) {
    texts[count].column = column + length + 5;
    texts[count].text = ">";
    texts[count].is_inverted = false;
    ++count;
  }
  return row_matches(1, HARNESS_NO_BAND, 0, texts, count);
}

static inline bool qa_lvl(unsigned shown, unsigned earned) {
  return qa_lvl_markers(shown, earned, shown > 1U, shown < earned);
}

/* LABEL(l): centered in row 2; LABEL("") is a blank row 2. */
static inline bool qa_label(const char* label) {
  if (label[0] == '\0') {
    return blank_row(2);
  }
  return centered(2, label);
}

/* DESC(l, s): LABEL(l) and the large text s at x = 64 - 4 m, nothing else in y 24-39. */
static inline bool qa_desc(const char* label, const char* text) { return qa_label(label) && qa_big_line(text); }

/*
 * The band of SL-5 in row 7 with @p label inverted: column c = floor((25 - n) / 2), band cells c - 1 to c + n,
 * y 57-63, y 56 off, all other pixels of row 7 off.
 */
static inline bool qa_band(const char* label) {
  const int length = (int)strlen(label);
  const int column = (25 - length) / 2;
  const int band_x0 = 2 + 5 * (column - 1);
  const int band_x1 = 2 + 5 * (column + length + 1) - 1;
  const row_text_t text = {column, label, true};
  return row_matches(7, band_x0, band_x1, &text, 1U) && !pixel(band_x0 - 1, 60) && !pixel(band_x1 + 1, 60);
}

/* STARTBTN: the band x 47-81. */
static inline bool qa_start_button(void) { return qa_band("Start") && pixel(47, 60) && pixel(81, 60); }

/* OKBTN: the band x 52-71 (UX-68). */
static inline bool qa_ok_button(void) { return qa_band("OK") && pixel(52, 60) && pixel(71, 60) && !pixel(51, 60); }

/*
 * The start screen of ui-ux.md 5.16.1: HDR(@p header), LVL(@p shown, @p earned), DESC(@p label, @p text),
 * CODE(@p code) ("" for CODE()), BLANK(6), STARTBTN and the margin rule.
 */
static inline bool qa_start_screen_is(const char* header, unsigned shown, unsigned earned, const char* label,
                                      const char* text, const char* code) {
  return hdr(header) && qa_lvl(shown, earned) && qa_desc(label, text) && qa_code(code) && blank_row(6) &&
         qa_start_button() && margins_off();
}

/*
 * The end screen of ui-ux.md 5.15 (SL-7) in rows 1-7: row 1 @p status (NULL: blank), row 2 @p score, row 3
 * @p result, BODYOFF(4,6), OKBTN and the margin rule. Row 0 is the caller's check.
 */
static inline bool qa_end_screen_is(const char* status_text, const char* score, const char* result) {
  const bool is_status_ok = (status_text == NULL) ? blank_row(1) : centered(1, status_text);
  return is_status_ok && centered(2, score) && centered(3, result) && body_off(4, 6) && qa_ok_button() && margins_off();
}

/* The pages that are dirty now equal @p mask; then every dirty flag is cleared for the next check. */
static inline bool qa_dirty_then_clear(uint8_t mask) {
  const bool is_ok = dirty_is(mask);
  harness_clear_dirty();
  return is_ok;
}

#endif /* QA_TRAINING_SCREENS_H */
