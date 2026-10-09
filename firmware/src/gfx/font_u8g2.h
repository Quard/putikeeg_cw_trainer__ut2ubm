/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef GFX_FONT_U8G2_H
#define GFX_FONT_U8G2_H

/*
 * Decoder for u8g2 fonts in code memory. Private to gfx. Call from the main loop only; not reentrant.
 * Parameters after the first are in xdata on the target, so that the decoder needs almost no internal RAM.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

/** Selects the u8g2 font at @p font and reads its header. Returns false for NULL and keeps the current font. */
bool font_u8g2_select(const uint8_t MEM_CODE* font);

/** Returns the cell height of the selected font (bounding box height), or 0 with no font. */
uint8_t font_u8g2_cell_height(void);

/**
 * Draws glyph @p encoding with its cell top-left at (@p x, @p y). Glyph pixels get @p fg_color, the other pixels
 * of the glyph box get @p bg_color (LCD_PIXEL_OFF, LCD_PIXEL_ON or LCD_PIXEL_INVERT).
 * Returns the glyph pitch. Returns 0 and draws nothing when no font is selected or the font has no such glyph.
 */
int8_t font_u8g2_draw_glyph(int16_t x, MEM_XDATA int16_t y, MEM_XDATA uint8_t encoding, MEM_XDATA uint8_t fg_color,
                            MEM_XDATA uint8_t bg_color);

/** Returns the pitch of glyph @p encoding without drawing it. Returns 0 for a missing glyph or no font. */
int8_t font_u8g2_glyph_pitch(uint8_t encoding);

#endif /* GFX_FONT_U8G2_H */
