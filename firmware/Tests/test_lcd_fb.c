#include <limits.h>
#include <string.h>

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

static bool is_unchanged_since_snapshot(void) {
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

static void test_clear_zeroes_and_marks_all_dirty(void) {
  uint8_t page;
  size_t column;

  lcd_fb_page(3U)[17] = 0xFFU;
  clear_all_dirty();
  lcd_fb_clear();
  TEST_ASSERT_EQUAL(0xFFU, lcd_fb_dirty_mask());
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    for (column = 0U; column < LCD_WIDTH; ++column) {
      TEST_ASSERT_EQUAL(0U, lcd_fb_page(page)[column]);
    }
  }
}

static void test_pixel_layout(void) {
  lcd_fb_clear();
  lcd_fb_set_pixel(5, 13, LCD_PIXEL_ON);
  TEST_ASSERT_EQUAL(0x20U, lcd_fb_page(1U)[5]);
  lcd_fb_set_pixel(127, 63, LCD_PIXEL_ON);
  TEST_ASSERT_EQUAL(0x80U, lcd_fb_page(7U)[127]);
  lcd_fb_set_pixel(0, 0, LCD_PIXEL_ON);
  TEST_ASSERT_EQUAL(0x01U, lcd_fb_page(0U)[0]);
  TEST_ASSERT(lcd_fb_get_pixel(5, 13));
  TEST_ASSERT(!lcd_fb_get_pixel(5, 12));
  TEST_ASSERT(!lcd_fb_get_pixel(6, 13));
}

static void test_clipping(void) {
  static const int16_t xs[] = {INT16_MIN, -1, 0, 127, 128, INT16_MAX};
  static const int16_t ys[] = {INT16_MIN, -1, 0, 63, 64, INT16_MAX};
  size_t xi;
  size_t yi;

  for (xi = 0U; xi < sizeof(xs) / sizeof(xs[0]); ++xi) {
    for (yi = 0U; yi < sizeof(ys) / sizeof(ys[0]); ++yi) {
      const int16_t x = xs[xi];
      const int16_t y = ys[yi];
      const bool is_inside = (x >= 0) && (x <= 127) && (y >= 0) && (y <= 63);

      lcd_fb_clear();
      clear_all_dirty();
      take_snapshot();
      lcd_fb_set_pixel(x, y, LCD_PIXEL_ON);
      TEST_ASSERT_EQUAL(is_inside, lcd_fb_get_pixel(x, y));
      TEST_ASSERT_EQUAL(is_inside, !is_unchanged_since_snapshot());
      TEST_ASSERT_EQUAL(is_inside ? (1U << ((unsigned)y >> 3)) : 0U, lcd_fb_dirty_mask());
      lcd_fb_set_pixel(x, y, LCD_PIXEL_INVERT);
      lcd_fb_set_pixel(x, y, LCD_PIXEL_OFF);
      TEST_ASSERT(is_unchanged_since_snapshot());
    }
  }
}

static void test_get_pixel_outside_is_false(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memset(lcd_fb_page(page), 0xFF, LCD_WIDTH);
  }
  TEST_ASSERT(!lcd_fb_get_pixel(-1, 0));
  TEST_ASSERT(!lcd_fb_get_pixel(128, 0));
  TEST_ASSERT(!lcd_fb_get_pixel(0, -1));
  TEST_ASSERT(!lcd_fb_get_pixel(0, 64));
  TEST_ASSERT(!lcd_fb_get_pixel(INT16_MIN, INT16_MIN));
  TEST_ASSERT(!lcd_fb_get_pixel(INT16_MAX, INT16_MAX));
  TEST_ASSERT(lcd_fb_get_pixel(127, 63));
}

static void test_colors(void) {
  lcd_fb_clear();
  lcd_fb_set_pixel(10, 20, LCD_PIXEL_ON);
  TEST_ASSERT(lcd_fb_get_pixel(10, 20));
  lcd_fb_set_pixel(10, 20, LCD_PIXEL_OFF);
  TEST_ASSERT(!lcd_fb_get_pixel(10, 20));
  lcd_fb_set_pixel(10, 20, LCD_PIXEL_INVERT);
  TEST_ASSERT(lcd_fb_get_pixel(10, 20));
  lcd_fb_set_pixel(10, 20, LCD_PIXEL_INVERT);
  TEST_ASSERT(!lcd_fb_get_pixel(10, 20));
  /* Neighbour bits in the same byte stay unchanged. */
  lcd_fb_page(2U)[10] = 0xEFU;
  lcd_fb_set_pixel(10, 20, LCD_PIXEL_ON);
  TEST_ASSERT_EQUAL(0xFFU, lcd_fb_page(2U)[10]);
  lcd_fb_set_pixel(10, 20, LCD_PIXEL_OFF);
  TEST_ASSERT_EQUAL(0xEFU, lcd_fb_page(2U)[10]);
}

static void test_unknown_color_is_ignored(void) {
  static const uint8_t colors[] = {3U, 0x7FU, 0xFFU};
  size_t i;

  lcd_fb_clear();
  lcd_fb_set_pixel(4, 4, LCD_PIXEL_ON);
  clear_all_dirty();
  take_snapshot();
  for (i = 0U; i < sizeof(colors); ++i) {
    lcd_fb_set_pixel(4, 4, colors[i]);
    lcd_fb_set_pixel(5, 5, colors[i]);
  }
  TEST_ASSERT(is_unchanged_since_snapshot());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
}

static void test_dirty_only_on_change(void) {
  lcd_fb_clear();
  clear_all_dirty();
  lcd_fb_set_pixel(1, 40, LCD_PIXEL_OFF);
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  lcd_fb_set_pixel(1, 40, LCD_PIXEL_ON);
  TEST_ASSERT_EQUAL(1U << 5, lcd_fb_dirty_mask());
  lcd_fb_clear_dirty(5U);
  lcd_fb_set_pixel(1, 40, LCD_PIXEL_ON);
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  lcd_fb_set_pixel(1, 40, LCD_PIXEL_INVERT);
  TEST_ASSERT_EQUAL(1U << 5, lcd_fb_dirty_mask());
}

static void test_page_bounds(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    TEST_ASSERT(lcd_fb_page(page) != NULL);
    TEST_ASSERT(lcd_fb_page(page) == lcd_fb_page(0U) + ((size_t)page * LCD_WIDTH));
  }
  TEST_ASSERT(lcd_fb_page(8U) == NULL);
  TEST_ASSERT(lcd_fb_page(9U) == NULL);
  TEST_ASSERT(lcd_fb_page(255U) == NULL);
}

static void test_dirty_marking_bounds(void) {
  clear_all_dirty();
  lcd_fb_mark_dirty(8U);
  lcd_fb_mark_dirty(255U);
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  lcd_fb_mark_dirty(0U);
  lcd_fb_mark_dirty(7U);
  TEST_ASSERT_EQUAL(0x81U, lcd_fb_dirty_mask());
  lcd_fb_clear_dirty(8U);
  lcd_fb_clear_dirty(255U);
  TEST_ASSERT_EQUAL(0x81U, lcd_fb_dirty_mask());
  lcd_fb_clear_dirty(7U);
  TEST_ASSERT_EQUAL(0x01U, lcd_fb_dirty_mask());
  lcd_fb_mark_all_dirty();
  TEST_ASSERT_EQUAL(0xFFU, lcd_fb_dirty_mask());
}

int main(void) {
  TEST_RUN(test_clear_zeroes_and_marks_all_dirty);
  TEST_RUN(test_pixel_layout);
  TEST_RUN(test_clipping);
  TEST_RUN(test_get_pixel_outside_is_false);
  TEST_RUN(test_colors);
  TEST_RUN(test_unknown_color_is_ignored);
  TEST_RUN(test_dirty_only_on_change);
  TEST_RUN(test_page_bounds);
  TEST_RUN(test_dirty_marking_bounds);
  return TEST_FINISH();
}
