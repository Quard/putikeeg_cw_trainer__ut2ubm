/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GFX_GFX_H
#define GFX_GFX_H

/*
 * Text and filled rectangles in the lcd_fb framebuffer. Call from the main loop only; not reentrant.
 * Parameters after the first are in xdata on the target, so that the module needs almost no internal RAM.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#define GFX_TEXT_NORMAL 0U   /* glyph pixels on, cell background off */
#define GFX_TEXT_INVERTED 1U /* glyph pixels off, cell background on */
#define GFX_U16_TEXT_SIZE 6U /* "65535" plus NUL */
#define GFX_FONT_5X8_WIDTH 5U
#define GFX_FONT_5X8_HEIGHT 8U

/** Selects the 5 x 8 UI font. */
void gfx_init(void);

/** Selects font @p font_id (FONTS_ID_*). Returns false and keeps the current font for an unknown id. */
bool gfx_set_font(uint8_t font_id);

/**
 * Draws @p text with its cell top-left at (@p x, @p y) in GFX_TEXT_NORMAL or GFX_TEXT_INVERTED video and returns
 * the x after the last character. Pixels outside the screen are dropped; a character without a glyph does not
 * move x. Draws nothing and returns @p x with no font, a NULL @p text or an unknown @p color.
 */
int16_t gfx_draw_text(int16_t x, MEM_XDATA int16_t y, const char* MEM_XDATA text, MEM_XDATA uint8_t color);

/** Returns the sum of the positive glyph pitches of @p text, or 0 with no font or a NULL @p text. */
uint16_t gfx_text_width(const char* text);

/**
 * Sets the pixels of the rectangle to @p color (LCD_PIXEL_OFF, LCD_PIXEL_ON or LCD_PIXEL_INVERT), clipped to the
 * screen. Does nothing for an unknown color or a width or height <= 0. Marks each changed page dirty.
 */
void gfx_fill_rect(int16_t x, MEM_XDATA int16_t y, MEM_XDATA int16_t width, MEM_XDATA int16_t height,
                   MEM_XDATA uint8_t color);

/**
 * Writes the decimal digits of @p value without leading zeros and a NUL into @p buffer, which has at least
 * GFX_U16_TEXT_SIZE bytes. Returns the digit count, 1..5.
 */
uint8_t gfx_format_u16(uint16_t value, char* buffer);

#endif /* GFX_GFX_H */
