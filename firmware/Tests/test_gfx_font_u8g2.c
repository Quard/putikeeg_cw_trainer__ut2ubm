#include <string.h>

#include "fixtures/font_golden.h"
#include "font_u8g2.h"
#include "gfx/fonts.h"
#include "lcd/lcd_fb.h"
#include "support/test_assert.h"

#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)
#define MISSING_ENCODING 0x1FU

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

static void clear_all_dirty(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
}

/* Returns the golden 5x8 pixel at cell (column, row) for @p encoding. */
static bool golden_5x8_pixel(uint8_t encoding, int column, int row) {
  const uint8_t bits = font_golden_5x8[encoding - FONT_GOLDEN_5X8_FIRST].rows[row];
  return (bits & (1U << (FONT_GOLDEN_5X8_WIDTH - 1U - (unsigned)column))) != 0U;
}

/*
 * Checks that every on-screen pixel of the 5x8 cell at (x, y) is ink XOR @p is_inverted, and every pixel outside
 * the cell is unchanged since the snapshot.
 */
static bool is_cell_drawn(uint8_t encoding, int x, int y, bool is_inverted) {
  int px;
  int py;
  for (py = 0; py < (int)LCD_HEIGHT; ++py) {
    for (px = 0; px < (int)LCD_WIDTH; ++px) {
      const int column = px - x;
      const int row = py - y;
      const bool is_in_cell = (column >= 0) && (column < 5) && (row >= 0) && (row < 8);
      bool expected = snapshot_pixel(px, py);
      if (is_in_cell) {
        expected = golden_5x8_pixel(encoding, column, row) != is_inverted;
      }
      if (lcd_fb_get_pixel((int16_t)px, (int16_t)py) != expected) {
        fprintf(stderr, "glyph 0x%02X at (%d, %d): pixel (%d, %d) is wrong\n", encoding, x, y, px, py);
        return false;
      }
    }
  }
  return true;
}

static void select_5x8(void) { TEST_ASSERT(font_u8g2_select(fonts_get(FONTS_ID_5X8))); }

static void test_every_5x8_glyph_matches_golden(void) {
  uint8_t encoding;
  select_5x8();
  for (encoding = FONT_GOLDEN_5X8_FIRST; encoding <= FONT_GOLDEN_5X8_LAST; ++encoding) {
    int8_t pitch;
    lcd_fb_clear();
    take_snapshot();
    pitch = font_u8g2_draw_glyph(0, 0, encoding, LCD_PIXEL_ON, LCD_PIXEL_OFF);
    TEST_ASSERT_EQUAL(font_golden_5x8[encoding - FONT_GOLDEN_5X8_FIRST].pitch, pitch);
    TEST_ASSERT(is_cell_drawn(encoding, 0, 0, false));
  }
}

static void test_every_5x8_glyph_inverted(void) {
  uint8_t encoding;
  select_5x8();
  for (encoding = FONT_GOLDEN_5X8_FIRST; encoding <= FONT_GOLDEN_5X8_LAST; ++encoding) {
    memset(lcd_fb_page(0U), 0x5A, LCD_WIDTH);
    take_snapshot();
    (void)font_u8g2_draw_glyph(0, 0, encoding, LCD_PIXEL_OFF, LCD_PIXEL_ON);
    TEST_ASSERT(is_cell_drawn(encoding, 0, 0, true));
  }
}

static void test_invert_color_gives_xor(void) {
  uint8_t encoding;
  int column;
  int row;
  select_5x8();
  for (encoding = FONT_GOLDEN_5X8_FIRST; encoding <= FONT_GOLDEN_5X8_LAST; ++encoding) {
    lcd_fb_clear();
    memset(lcd_fb_page(1U), 0xA5, LCD_WIDTH);
    take_snapshot();
    (void)font_u8g2_draw_glyph(3, 8, encoding, LCD_PIXEL_INVERT, LCD_PIXEL_OFF);
    for (row = 0; row < 8; ++row) {
      for (column = 0; column < 5; ++column) {
        const bool is_ink = golden_5x8_pixel(encoding, column, row);
        const bool expected = is_ink ? !snapshot_pixel(3 + column, 8 + row) : false;
        TEST_ASSERT_EQUAL(expected, lcd_fb_get_pixel((int16_t)(3 + column), (int16_t)(8 + row)));
      }
    }
  }
}

static void test_letter_a_hand_checked(void) {
  static const char* const rows[8] = {".....", ".##..", "#..#.", "#..#.", "####.", "#..#.", "#..#.", "....."};
  int column;
  int row;
  int offset;
  static const int origins[2][2] = {{0, 0}, {10, 16}};

  select_5x8();
  for (offset = 0; offset < 2; ++offset) {
    const int x = origins[offset][0];
    const int y = origins[offset][1];
    lcd_fb_clear();
    TEST_ASSERT_EQUAL(5, font_u8g2_draw_glyph((int16_t)x, (int16_t)y, (uint8_t)'A', LCD_PIXEL_ON, LCD_PIXEL_OFF));
    for (row = 0; row < 8; ++row) {
      for (column = 0; column < 5; ++column) {
        TEST_ASSERT_EQUAL(rows[row][column] == '#', lcd_fb_get_pixel((int16_t)(x + column), (int16_t)(y + row)));
      }
    }
    for (column = 0; column <= 5; ++column) {
      TEST_ASSERT(!lcd_fb_get_pixel((int16_t)(x + column), (int16_t)(y + 8)));
    }
    for (row = 0; row < 8; ++row) {
      TEST_ASSERT(!lcd_fb_get_pixel((int16_t)(x + 5), (int16_t)(y + row)));
    }
  }
}

static void test_descenders_stay_in_cell(void) {
  static const char descenders[] = "gjpqy";
  size_t i;
  int column;
  select_5x8();
  for (i = 0U; i < sizeof(descenders) - 1U; ++i) {
    bool has_row_7 = false;
    lcd_fb_clear();
    (void)font_u8g2_draw_glyph(0, 8, (uint8_t)descenders[i], LCD_PIXEL_ON, LCD_PIXEL_OFF);
    for (column = 0; column < 5; ++column) {
      has_row_7 = has_row_7 || lcd_fb_get_pixel((int16_t)column, 15);
      TEST_ASSERT(!lcd_fb_get_pixel((int16_t)column, 16));
      TEST_ASSERT(!lcd_fb_get_pixel((int16_t)column, 7));
    }
    TEST_ASSERT(has_row_7);
  }
}

static void test_8x16_glyphs_match_golden(void) {
  size_t i;
  int column;
  int row;
  TEST_ASSERT(font_u8g2_select(fonts_get(FONTS_ID_8X16)));
  TEST_ASSERT_EQUAL(FONT_GOLDEN_8X16_CELL_HEIGHT, font_u8g2_cell_height());
  for (i = 0U; i < FONT_GOLDEN_8X16_COUNT; ++i) {
    const font_golden_8x16_t* golden = &font_golden_8x16[i];
    lcd_fb_clear();
    TEST_ASSERT_EQUAL(golden->pitch, font_u8g2_draw_glyph(0, 0, golden->encoding, LCD_PIXEL_ON, LCD_PIXEL_OFF));
    TEST_ASSERT_EQUAL(golden->pitch, font_u8g2_glyph_pitch(golden->encoding));
    for (row = 0; row < (int)FONT_GOLDEN_8X16_ROWS; ++row) {
      for (column = 0; column < (int)FONT_GOLDEN_8X16_WIDTH; ++column) {
        const bool expected = (golden->rows[row] & (0x80U >> (unsigned)column)) != 0U;
        TEST_ASSERT_EQUAL(expected, lcd_fb_get_pixel((int16_t)column, (int16_t)row));
      }
    }
  }
  select_5x8();
  TEST_ASSERT_EQUAL(FONT_GOLDEN_5X8_CELL_HEIGHT, font_u8g2_cell_height());
}

static void test_missing_glyph(void) {
  select_5x8();
  lcd_fb_clear();
  memset(lcd_fb_page(0U), 0x3C, LCD_WIDTH);
  take_snapshot();
  clear_all_dirty();
  TEST_ASSERT_EQUAL(0, font_u8g2_draw_glyph(0, 0, MISSING_ENCODING, LCD_PIXEL_ON, LCD_PIXEL_OFF));
  TEST_ASSERT_EQUAL(0, font_u8g2_glyph_pitch(MISSING_ENCODING));
  TEST_ASSERT_EQUAL(0, memcmp(snapshot, lcd_fb_page(0U), LCD_WIDTH));
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
}

static void test_pitch_of_every_glyph(void) {
  uint8_t encoding;
  select_5x8();
  for (encoding = FONT_GOLDEN_5X8_FIRST; encoding <= FONT_GOLDEN_5X8_LAST; ++encoding) {
    TEST_ASSERT_EQUAL(font_golden_5x8[encoding - FONT_GOLDEN_5X8_FIRST].pitch, font_u8g2_glyph_pitch(encoding));
  }
  /* The font has glyphs above 0x7F, but they are outside the UI character set; 0x00 is not in the font. */
  TEST_ASSERT_EQUAL(0, font_u8g2_glyph_pitch(0x00U));
}

static void test_dirty_pages_follow_changes(void) {
  select_5x8();
  lcd_fb_clear();
  clear_all_dirty();
  (void)font_u8g2_draw_glyph(0, 4, (uint8_t)'A', LCD_PIXEL_ON, LCD_PIXEL_OFF);
  TEST_ASSERT_EQUAL(0x03U, lcd_fb_dirty_mask());
  clear_all_dirty();
  (void)font_u8g2_draw_glyph(0, 4, (uint8_t)'A', LCD_PIXEL_ON, LCD_PIXEL_OFF);
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  (void)font_u8g2_draw_glyph(0, 4, (uint8_t)' ', LCD_PIXEL_ON, LCD_PIXEL_OFF);
  TEST_ASSERT_EQUAL(0x03U, lcd_fb_dirty_mask());
}

static void check_clipped(uint8_t encoding, int x, int y) {
  lcd_fb_clear();
  take_snapshot();
  TEST_ASSERT_EQUAL(5, font_u8g2_draw_glyph((int16_t)x, (int16_t)y, encoding, LCD_PIXEL_ON, LCD_PIXEL_OFF));
  TEST_ASSERT(is_cell_drawn(encoding, x, y, false));
}

static void test_clipping(void) {
  static const int positions[][2] = {{-3, 0},    {125, 0},   {0, -5},     {0, 60},       {-3, -5},  {125, 60},
                                     {-5, 0},    {128, 0},   {0, -8},     {0, 64},       {-200, 0}, {300, 30},
                                     {10, -300}, {10, 3000}, {-32768, 0}, {32767, 32767}};
  size_t i;
  select_5x8();
  for (i = 0U; i < sizeof(positions) / sizeof(positions[0]); ++i) {
    check_clipped((uint8_t)'W', positions[i][0], positions[i][1]);
    check_clipped((uint8_t)'#', positions[i][0], positions[i][1]);
  }
}

static void test_inverted_clipping_touches_screen_only(void) {
  select_5x8();
  lcd_fb_clear();
  (void)font_u8g2_draw_glyph(125, 60, (uint8_t)' ', LCD_PIXEL_OFF, LCD_PIXEL_ON);
  TEST_ASSERT(lcd_fb_get_pixel(125, 60));
  TEST_ASSERT(lcd_fb_get_pixel(127, 63));
  TEST_ASSERT(!lcd_fb_get_pixel(124, 60));
  TEST_ASSERT(!lcd_fb_get_pixel(125, 59));
}

static void test_select_null_keeps_font(void) {
  select_5x8();
  TEST_ASSERT(!font_u8g2_select(NULL));
  TEST_ASSERT_EQUAL(8U, font_u8g2_cell_height());
  TEST_ASSERT_EQUAL(5, font_u8g2_glyph_pitch((uint8_t)'A'));
}

/* Runs first: the decoder state is static, so no font is selected yet. */
static void test_no_font_draws_nothing(void) {
  lcd_fb_clear();
  take_snapshot();
  clear_all_dirty();
  TEST_ASSERT_EQUAL(0U, font_u8g2_cell_height());
  TEST_ASSERT_EQUAL(0, font_u8g2_draw_glyph(0, 0, (uint8_t)'A', LCD_PIXEL_ON, LCD_PIXEL_OFF));
  TEST_ASSERT_EQUAL(0, font_u8g2_glyph_pitch((uint8_t)'A'));
  TEST_ASSERT_EQUAL(0, memcmp(snapshot, lcd_fb_page(0U), LCD_WIDTH));
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(!font_u8g2_select(NULL));
  TEST_ASSERT_EQUAL(0, font_u8g2_glyph_pitch((uint8_t)'A'));
}

static void test_fonts_get(void) {
  TEST_ASSERT(fonts_get(FONTS_ID_5X8) != NULL);
  TEST_ASSERT(fonts_get(FONTS_ID_8X16) != NULL);
  TEST_ASSERT(fonts_get(FONTS_COUNT) == NULL);
  TEST_ASSERT(fonts_get(0xFFU) == NULL);
}

int main(void) {
  TEST_RUN(test_no_font_draws_nothing);
  TEST_RUN(test_fonts_get);
  TEST_RUN(test_every_5x8_glyph_matches_golden);
  TEST_RUN(test_every_5x8_glyph_inverted);
  TEST_RUN(test_invert_color_gives_xor);
  TEST_RUN(test_letter_a_hand_checked);
  TEST_RUN(test_descenders_stay_in_cell);
  TEST_RUN(test_8x16_glyphs_match_golden);
  TEST_RUN(test_missing_glyph);
  TEST_RUN(test_pitch_of_every_glyph);
  TEST_RUN(test_dirty_pages_follow_changes);
  TEST_RUN(test_clipping);
  TEST_RUN(test_inverted_clipping_touches_screen_only);
  TEST_RUN(test_select_null_keeps_font);
  return TEST_FINISH();
}
