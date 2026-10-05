#include "lcd/lcd_fb.h"

#include <stddef.h>

#define LCD_FB_SIZE ((uint16_t)(LCD_PAGE_COUNT * LCD_WIDTH))
#define LCD_FB_ALL_PAGES 0xFFU

static MEM_XDATA uint8_t lcd_fb_pixels[LCD_FB_SIZE];
static uint8_t lcd_fb_dirty_pages;

/* A table lookup is shorter and faster than a variable shift on the 8051. */
static const MEM_CODE uint8_t lcd_fb_bit_masks[8] = {0x01U, 0x02U, 0x04U, 0x08U, 0x10U, 0x20U, 0x40U, 0x80U};

/*
 * Macros, not functions: SDCC overlays the parameters of functions that call nothing, which saves internal RAM.
 * Negative coordinates wrap to large unsigned values, so one compare checks both ends of the range.
 */
#define LCD_FB_IS_ON_SCREEN(x, y) (((uint16_t)(x) < LCD_WIDTH) && ((uint16_t)(y) < LCD_HEIGHT))
#define LCD_FB_CELL(x, y) (&lcd_fb_pixels[((uint16_t)((uint8_t)(y) >> 3) * LCD_WIDTH) + (uint8_t)(x)])

void lcd_fb_clear(void) {
  MEM_XDATA uint8_t* cell = lcd_fb_pixels;
  do {
    *cell = 0U;
    ++cell;
  } while (cell != &lcd_fb_pixels[LCD_FB_SIZE]);
  lcd_fb_dirty_pages = LCD_FB_ALL_PAGES;
}

void lcd_fb_set_pixel(int16_t x, int16_t y, uint8_t color) {
  MEM_XDATA uint8_t* cell;
  uint8_t mask;
  uint8_t old_value;
  uint8_t new_value;

  if (!LCD_FB_IS_ON_SCREEN(x, y)) {
    return;
  }
  cell = LCD_FB_CELL(x, y);
  mask = lcd_fb_bit_masks[(uint8_t)y & 7U];
  old_value = *cell;
  if (color == LCD_PIXEL_ON) {
    new_value = (uint8_t)(old_value | mask);
  } else if (color == LCD_PIXEL_OFF) {
    new_value = (uint8_t)(old_value & (uint8_t)~mask);
  } else if (color == LCD_PIXEL_INVERT) {
    new_value = (uint8_t)(old_value ^ mask);
  } else {
    return;
  }
  if (new_value != old_value) {
    *cell = new_value;
    lcd_fb_dirty_pages |= lcd_fb_bit_masks[(uint8_t)y >> 3];
  }
}

bool lcd_fb_get_pixel(int16_t x, int16_t y) {
  if (!LCD_FB_IS_ON_SCREEN(x, y)) {
    return false;
  }
  return (bool)(*LCD_FB_CELL(x, y) & lcd_fb_bit_masks[(uint8_t)y & 7U]);
}

MEM_XDATA uint8_t* lcd_fb_page(uint8_t page) {
  if (page >= LCD_PAGE_COUNT) {
    return NULL;
  }
  return &lcd_fb_pixels[(uint16_t)((uint16_t)page * LCD_WIDTH)];
}

void lcd_fb_mark_dirty(uint8_t page) {
  if (page < LCD_PAGE_COUNT) {
    lcd_fb_dirty_pages |= lcd_fb_bit_masks[page];
  }
}

void lcd_fb_mark_all_dirty(void) { lcd_fb_dirty_pages = LCD_FB_ALL_PAGES; }

uint8_t lcd_fb_dirty_mask(void) { return lcd_fb_dirty_pages; }

void lcd_fb_clear_dirty(uint8_t page) {
  if (page < LCD_PAGE_COUNT) {
    lcd_fb_dirty_pages &= (uint8_t)~lcd_fb_bit_masks[page];
  }
}
