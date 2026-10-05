#ifndef LCD_LCD_FB_H
#define LCD_LCD_FB_H

/*
 * 128 x 64 monochrome framebuffer in xdata with one dirty bit per 8-row page.
 * Pixel (x, y) is bit (y & 7) of byte [(y >> 3) * 128 + x]; bit 0 is the top row of the page.
 * Call these functions from the main loop only. They are not reentrant.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#define LCD_WIDTH 128U
#define LCD_HEIGHT 64U
#define LCD_PAGE_COUNT 8U

#define LCD_PIXEL_OFF 0U
#define LCD_PIXEL_ON 1U
#define LCD_PIXEL_INVERT 2U

/** Sets all pixels off and marks all pages dirty. */
void lcd_fb_clear(void);

/**
 * Sets pixel (@p x, @p y) to @p color (LCD_PIXEL_OFF, LCD_PIXEL_ON or LCD_PIXEL_INVERT).
 * Pixels outside the screen and unknown colors are ignored. Marks the page dirty only when the byte changes.
 */
void lcd_fb_set_pixel(int16_t x, int16_t y, uint8_t color);

/** Returns true when pixel (@p x, @p y) is on. Returns false outside the screen. */
bool lcd_fb_get_pixel(int16_t x, int16_t y);

/**
 * Returns the 128 bytes of @p page, or NULL when @p page >= LCD_PAGE_COUNT.
 * A caller that changes bytes through the pointer must call lcd_fb_mark_dirty().
 */
MEM_XDATA uint8_t* lcd_fb_page(uint8_t page);

/** Marks @p page dirty. Ignores @p page >= LCD_PAGE_COUNT. */
void lcd_fb_mark_dirty(uint8_t page);

/** Marks all pages dirty. */
void lcd_fb_mark_all_dirty(void);

/** Returns the dirty pages; bit n is page n. */
uint8_t lcd_fb_dirty_mask(void);

/** Clears the dirty bit of @p page. Ignores @p page >= LCD_PAGE_COUNT. */
void lcd_fb_clear_dirty(uint8_t page);

#endif /* LCD_LCD_FB_H */
