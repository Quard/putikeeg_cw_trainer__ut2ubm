/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "fixtures/font_golden.h"
#include "gfx/fonts.h"
#include "gfx/gfx.h"
#include "lcd/lcd_fb.h"
#include "support/test_assert.h"

#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)

static uint8_t snapshot[FB_SIZE];

static void take_snapshot(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&snapshot[(size_t)page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
}

static bool snapshot_pixel(int x, int y) {
  return (snapshot[((size_t)y >> 3) * LCD_WIDTH + (size_t)x] & (1U << ((unsigned)y & 7U))) != 0U;
}

static bool is_fb_unchanged(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    if (memcmp(&snapshot[(size_t)page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH) != 0) {
      return false;
    }
  }
  return true;
}

static void clear_all_dirty(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
}

static bool golden_pixel(char character, int column, int row) {
  const uint8_t bits = font_golden_5x8[(uint8_t)character - FONT_GOLDEN_5X8_FIRST].rows[row];
  return (bits & (1U << (4U - (unsigned)column))) != 0U;
}

/* Expected pixel of @p text drawn at (x, y) over the snapshot; characters advance by 5. */
static bool expected_text_pixel(const char* text, int x, int y, bool is_inverted, int px, int py) {
  const int length = (int)strlen(text);
  const int column = px - x;
  const int row = py - y;
  if ((column < 0) || (column >= 5 * length) || (row < 0) || (row >= 8)) {
    return snapshot_pixel(px, py);
  }
  return golden_pixel(text[column / 5], column % 5, row) != is_inverted;
}

static bool is_text_drawn(const char* text, int x, int y, bool is_inverted) {
  int px;
  int py;
  for (py = 0; py < (int)LCD_HEIGHT; ++py) {
    for (px = 0; px < (int)LCD_WIDTH; ++px) {
      if (lcd_fb_get_pixel((int16_t)px, (int16_t)py) != expected_text_pixel(text, x, y, is_inverted, px, py)) {
        fprintf(stderr, "\"%s\" at (%d, %d): pixel (%d, %d) is wrong\n", text, x, y, px, py);
        return false;
      }
    }
  }
  return true;
}

static void check_text(const char* text, int x, int y, uint8_t color) {
  int16_t end;
  take_snapshot();
  end = gfx_draw_text((int16_t)x, (int16_t)y, text, color);
  TEST_ASSERT_EQUAL((int16_t)(x + 5 * (int)strlen(text)), end);
  TEST_ASSERT(is_text_drawn(text, x, y, color == GFX_TEXT_INVERTED));
}

/* Runs first: no font is selected yet. */
static void test_no_font(void) {
  lcd_fb_clear();
  take_snapshot();
  clear_all_dirty();
  TEST_ASSERT_EQUAL(7, gfx_draw_text(7, 0, "Abc", GFX_TEXT_NORMAL));
  TEST_ASSERT_EQUAL(0U, gfx_text_width("Abc"));
  TEST_ASSERT(is_fb_unchanged());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
}

static void test_text_rows_normal_and_inverted(void) {
  int row;
  gfx_init();
  for (row = 0; row < 8; ++row) {
    lcd_fb_clear();
    memset(lcd_fb_page(3U), 0x99, LCD_WIDTH);
    check_text("Agjy_|", 2, 8 * row, GFX_TEXT_NORMAL);
    lcd_fb_clear();
    memset(lcd_fb_page(5U), 0x5A, LCD_WIDTH);
    check_text("Settings > CW", 2, 8 * row, GFX_TEXT_INVERTED);
  }
}

static void test_text_covers_only_its_page(void) {
  uint8_t page;
  gfx_init();
  lcd_fb_clear();
  clear_all_dirty();
  (void)gfx_draw_text(2, 24, "Wg", GFX_TEXT_INVERTED);
  TEST_ASSERT_EQUAL(0x08U, lcd_fb_dirty_mask());
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    if (page != 3U) {
      TEST_ASSERT_EQUAL(0U, lcd_fb_page(page)[3]);
    }
  }
}

static void test_all_printable_characters(void) {
  char text[2] = {0, 0};
  uint8_t character;
  gfx_init();
  for (character = 0x20U; character <= 0x7EU; ++character) {
    text[0] = (char)character;
    lcd_fb_clear();
    check_text(text, 61, 33, GFX_TEXT_NORMAL);
  }
}

static void test_return_value_and_width(void) {
  gfx_init();
  lcd_fb_clear();
  TEST_ASSERT_EQUAL(32, gfx_draw_text(2, 0, "Agjy_|", GFX_TEXT_NORMAL));
  TEST_ASSERT_EQUAL(30U, gfx_text_width("Agjy_|"));
  TEST_ASSERT_EQUAL(0U, gfx_text_width(""));
  TEST_ASSERT_EQUAL(0U, gfx_text_width(NULL));
  TEST_ASSERT_EQUAL(-3, gfx_draw_text(-3, 0, "", GFX_TEXT_NORMAL));
  TEST_ASSERT_EQUAL(125U, gfx_text_width("1234567890123456789012345"));
}

static void test_missing_character_is_skipped(void) {
  static const char with_missing[] = {'A', 0x1F, 'B', 0};
  gfx_init();
  lcd_fb_clear();
  TEST_ASSERT_EQUAL(12, gfx_draw_text(2, 8, with_missing, GFX_TEXT_NORMAL));
  TEST_ASSERT_EQUAL(10U, gfx_text_width(with_missing));
  take_snapshot();
  lcd_fb_clear();
  (void)gfx_draw_text(2, 8, "AB", GFX_TEXT_NORMAL);
  TEST_ASSERT(is_fb_unchanged());
}

static void test_text_clipping(void) {
  static const int positions[][2] = {{-3, 0}, {-12, 8}, {120, 16}, {126, 24}, {2, -5},  {2, 60},  {2, -8},
                                     {2, 64}, {128, 0}, {-200, 0}, {2, 600},  {-7, -7}, {124, 59}};
  size_t i;
  gfx_init();
  for (i = 0U; i < sizeof(positions) / sizeof(positions[0]); ++i) {
    lcd_fb_clear();
    check_text("Wqx", positions[i][0], positions[i][1], GFX_TEXT_NORMAL);
    lcd_fb_clear();
    check_text("Wqx", positions[i][0], positions[i][1], GFX_TEXT_INVERTED);
  }
}

static void test_invalid_arguments(void) {
  gfx_init();
  lcd_fb_clear();
  take_snapshot();
  clear_all_dirty();
  TEST_ASSERT_EQUAL(4, gfx_draw_text(4, 0, NULL, GFX_TEXT_NORMAL));
  TEST_ASSERT_EQUAL(4, gfx_draw_text(4, 0, "A", 2U));
  TEST_ASSERT_EQUAL(4, gfx_draw_text(4, 0, "A", 0xFFU));
  TEST_ASSERT(is_fb_unchanged());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(!gfx_set_font(FONTS_COUNT));
  TEST_ASSERT(!gfx_set_font(0xFFU));
  TEST_ASSERT_EQUAL(5U, gfx_text_width("A"));
  TEST_ASSERT(gfx_set_font(FONTS_ID_8X16));
  TEST_ASSERT_EQUAL(8U, gfx_text_width("A"));
  TEST_ASSERT(gfx_set_font(FONTS_ID_5X8));
  TEST_ASSERT_EQUAL(5U, gfx_text_width("A"));
}

/* Expected pixel after a fill of [x0, x1] x [y0, y1] over the snapshot. */
static bool expected_fill_pixel(int x0, int y0, int x1, int y1, uint8_t color, int px, int py) {
  const bool is_inside = (px >= x0) && (px <= x1) && (py >= y0) && (py <= y1);
  const bool old_pixel = snapshot_pixel(px, py);
  if (!is_inside) {
    return old_pixel;
  }
  if (color == LCD_PIXEL_ON) {
    return true;
  }
  if (color == LCD_PIXEL_OFF) {
    return false;
  }
  return !old_pixel;
}

static void check_fill(int x, int y, int width, int height, uint8_t color) {
  int px;
  int py;
  bool is_ok = true;
  uint8_t expected_dirty = 0U;
  uint8_t page;

  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memset(lcd_fb_page(page), (int)(0x3CU ^ page), LCD_WIDTH);
  }
  take_snapshot();
  clear_all_dirty();
  gfx_fill_rect((int16_t)x, (int16_t)y, (int16_t)width, (int16_t)height, color);
  for (py = 0; py < (int)LCD_HEIGHT; ++py) {
    for (px = 0; px < (int)LCD_WIDTH; ++px) {
      const bool expected = expected_fill_pixel(x, y, x + width - 1, y + height - 1, color, px, py);
      if (lcd_fb_get_pixel((int16_t)px, (int16_t)py) != expected) {
        is_ok = false;
      }
      if (expected != snapshot_pixel(px, py)) {
        expected_dirty |= (uint8_t)(1U << ((unsigned)py >> 3));
      }
    }
  }
  if (!is_ok) {
    fprintf(stderr, "fill (%d, %d, %d, %d, %u) is wrong\n", x, y, width, height, color);
  }
  TEST_ASSERT(is_ok);
  TEST_ASSERT_EQUAL(expected_dirty, lcd_fb_dirty_mask());
}

static void test_fill_rect(void) {
  static const int rects[][4] = {
      {0, 0, 128, 64},      {0, 9, 128, 7},    {76, 25, 52, 7}, {37, 41, 20, 7}, {5, 3, 1, 1},
      {-10, -10, 20, 20},   {120, 60, 50, 50}, {-5, 10, 3, 5},  {130, 0, 5, 5},  {0, 64, 5, 5},
      {0, 0, 0, 5},         {0, 0, 5, 0},      {0, 0, -5, 5},   {0, 0, 5, -5},   {-32768, -32768, 32767, 32767},
      {32767, 0, 32767, 8}, {100, 7, 28, 2},   {0, 1, 128, 62}, {3, 8, 9, 8},    {-1, 63, 129, 1},
  };
  static const uint8_t colors[] = {LCD_PIXEL_ON, LCD_PIXEL_OFF, LCD_PIXEL_INVERT};
  size_t i;
  size_t c;
  for (i = 0U; i < sizeof(rects) / sizeof(rects[0]); ++i) {
    for (c = 0U; c < sizeof(colors); ++c) {
      check_fill(rects[i][0], rects[i][1], rects[i][2], rects[i][3], colors[c]);
    }
  }
}

static void test_fill_rect_unknown_color_and_no_change(void) {
  lcd_fb_clear();
  take_snapshot();
  clear_all_dirty();
  gfx_fill_rect(0, 0, 128, 64, 3U);
  TEST_ASSERT(is_fb_unchanged());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  gfx_fill_rect(0, 0, 128, 64, LCD_PIXEL_OFF);
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
}

static void check_format(uint16_t value, const char* expected) {
  char buffer[GFX_U16_TEXT_SIZE + 2U];
  memset(buffer, 'x', sizeof(buffer));
  TEST_ASSERT_EQUAL(strlen(expected), gfx_format_u16(value, buffer));
  TEST_ASSERT_EQUAL(0, strcmp(expected, buffer));
  TEST_ASSERT_EQUAL('x', buffer[GFX_U16_TEXT_SIZE]);
}

static void test_format_u16(void) {
  check_format(0U, "0");
  check_format(9U, "9");
  check_format(10U, "10");
  check_format(600U, "600");
  check_format(1000U, "1000");
  check_format(10009U, "10009");
  check_format(65535U, "65535");
}

int main(void) {
  TEST_RUN(test_no_font);
  TEST_RUN(test_text_rows_normal_and_inverted);
  TEST_RUN(test_text_covers_only_its_page);
  TEST_RUN(test_all_printable_characters);
  TEST_RUN(test_return_value_and_width);
  TEST_RUN(test_missing_character_is_skipped);
  TEST_RUN(test_text_clipping);
  TEST_RUN(test_invalid_arguments);
  TEST_RUN(test_fill_rect);
  TEST_RUN(test_fill_rect_unknown_color_and_no_change);
  TEST_RUN(test_format_u16);
  return TEST_FINISH();
}
