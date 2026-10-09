/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LCD_LCD_H
#define LCD_LCD_H

/*
 * 128 x 64 LCD controller driver (probably ST7567) on top of lcd_spi and lcd_fb.
 * Call these functions from the main loop only. They are not reentrant.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#define LCD_ORIENTATION_NORMAL 0U
#define LCD_ORIENTATION_ROTATED 1U

/**
 * Resets and configures the controller with the stored settings, clears the controller RAM and turns the
 * display on. Keeps the framebuffer, marks all pages dirty and cancels a flush in progress.
 * Safe to call again to recover the display. Blocks for the reset and power-up delays plus the RAM clear transfer.
 */
void lcd_init(void);

/** Turns the display on (true) or off (false). */
void lcd_set_display_on(bool is_on);

/** Sets the contrast. Values above 63 become 63. The value is stored for lcd_init(). */
void lcd_set_contrast(uint8_t level);

/** Shows inverted (true) or normal (false) pixels. The value is stored for lcd_init(). */
void lcd_set_inverted(bool is_inverted);

/** Turns all pixels on (true) or shows the RAM (false). The value is stored for lcd_init() and lcd_wake(). */
void lcd_set_all_pixels_on(bool is_all_on);

/**
 * Sets LCD_ORIENTATION_NORMAL or LCD_ORIENTATION_ROTATED and marks all pages dirty.
 * Ignores unknown values. The value is stored for lcd_init().
 */
void lcd_set_orientation(uint8_t orientation);

/** Turns the display off and puts the controller into power save. */
void lcd_sleep(void);

/** Leaves power save, restores the stored all-pixels-on setting and turns the display on. */
void lcd_wake(void);

/**
 * Sends @p length bytes to controller RAM at @p page and @p column, bypassing the framebuffer.
 * Ignores @p page >= 8 and @p column >= 128. Cuts @p length to 128 - @p column. A length of 0 sends only the
 * address. Does not change the framebuffer or the dirty pages.
 */
void lcd_write(uint8_t page, uint8_t column, const MEM_XDATA uint8_t* data, uint8_t length);

/**
 * Sends at most one chunk (LCD_FLUSH_CHUNK_COLUMNS columns of one page) of dirty framebuffer data.
 * Pages are served round-robin. Does nothing when no page is pending.
 */
void lcd_flush_step(void);

/** Returns true while a page is dirty or a page transfer is in progress. */
bool lcd_has_pending_flush(void);

/** Calls lcd_flush_step() until nothing is pending. */
void lcd_flush(void);

#endif /* LCD_LCD_H */
