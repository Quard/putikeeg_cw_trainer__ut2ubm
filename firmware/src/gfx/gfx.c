/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "gfx/gfx.h"

#include <stddef.h>

#include "font_u8g2.h"
#include "gfx/fonts.h"
#include "lcd/lcd_fb.h"

#define GFX_DECIMAL_PLACES 4U

static const MEM_CODE uint16_t gfx_decimal_places[GFX_DECIMAL_PLACES] = {10000U, 1000U, 100U, 10U};

/* Working state in xdata: SDCC would put it in internal RAM. */
static MEM_XDATA int16_t text_x;
static MEM_XDATA uint8_t text_fg_color;
static MEM_XDATA uint8_t text_bg_color;
static MEM_XDATA uint8_t clip_first;
static MEM_XDATA uint8_t clip_last;
static MEM_XDATA uint8_t fill_left;
static MEM_XDATA uint8_t fill_width;
static MEM_XDATA uint8_t fill_mask;
static MEM_XDATA uint8_t fill_color;

void gfx_init(void) { (void)gfx_set_font(FONTS_ID_5X8); }

bool gfx_set_font(uint8_t font_id) {
  if (font_id >= FONTS_COUNT) {
    return false;
  }
  (void)font_u8g2_select(fonts_get(font_id));
  return true;
}

int16_t gfx_draw_text(int16_t x, MEM_XDATA int16_t y, const char* MEM_XDATA text, MEM_XDATA uint8_t color) {
  text_x = x;
  if ((text == NULL) || (color > GFX_TEXT_INVERTED)) {
    return text_x;
  }
  text_fg_color = LCD_PIXEL_ON;
  text_bg_color = LCD_PIXEL_OFF;
  if (color == GFX_TEXT_INVERTED) {
    text_fg_color = LCD_PIXEL_OFF;
    text_bg_color = LCD_PIXEL_ON;
  }
  while (*text != '\0') {
    text_x = (int16_t)(text_x + font_u8g2_draw_glyph(text_x, y, (uint8_t)*text, text_fg_color, text_bg_color));
    ++text;
  }
  return text_x;
}

uint16_t gfx_text_width(const char* text) {
  MEM_XDATA uint16_t width;
  MEM_XDATA int8_t pitch;

  if (text == NULL) {
    return 0U;
  }
  width = 0U;
  while (*text != '\0') {
    pitch = font_u8g2_glyph_pitch((uint8_t)*text);
    if (pitch > 0) {
      width = (uint16_t)(width + (uint8_t)pitch);
    }
    ++text;
  }
  return width;
}

/*
 * Clips [start, start + length) to [0, limit). Returns false when nothing stays visible, else sets clip_first and
 * clip_last (inclusive). start + length can overflow, so it is never computed.
 */
static bool gfx_clip(int16_t start, int16_t length, uint8_t limit) {
  if (start >= (int16_t)limit) {
    return false;
  }
  if (start < 0) {
    length = (int16_t)(length + start);
    start = 0;
  }
  if (length <= 0) {
    return false;
  }
  if (length > (int16_t)(limit - (uint8_t)start)) {
    length = (int16_t)(limit - (uint8_t)start);
  }
  clip_first = (uint8_t)start;
  clip_last = (uint8_t)((uint8_t)start + (uint8_t)length - 1U);
  return true;
}

/* Applies fill_color to the fill_mask bits of fill_width bytes from @p cell. Returns non-zero when a byte changed. */
static uint8_t gfx_fill_bytes(MEM_XDATA uint8_t* cell) {
  uint8_t remaining = fill_width;
  uint8_t changed = 0U;
  uint8_t value;

  do {
    value = *cell;
    if (fill_color == LCD_PIXEL_ON) {
      value |= fill_mask;
    } else if (fill_color == LCD_PIXEL_OFF) {
      value &= (uint8_t)~fill_mask;
    } else {
      value ^= fill_mask;
    }
    changed |= (uint8_t)(value ^ *cell);
    *cell = value;
    ++cell;
    --remaining;
  } while (remaining != 0U);
  return changed;
}

void gfx_fill_rect(int16_t x, MEM_XDATA int16_t y, MEM_XDATA int16_t width, MEM_XDATA int16_t height,
                   MEM_XDATA uint8_t color) {
  uint8_t page;

  if ((color > LCD_PIXEL_INVERT) || !gfx_clip(x, width, LCD_WIDTH)) {
    return;
  }
  fill_left = clip_first;
  fill_width = (uint8_t)(clip_last - clip_first + 1U);
  if (!gfx_clip(y, height, LCD_HEIGHT)) {
    return;
  }
  fill_color = color;
  for (page = (uint8_t)(clip_first >> 3); page <= (uint8_t)(clip_last >> 3); ++page) {
    fill_mask = 0xFFU;
    if (page == (uint8_t)(clip_first >> 3)) {
      fill_mask = (uint8_t)(fill_mask << (clip_first & 7U));
    }
    if (page == (uint8_t)(clip_last >> 3)) {
      fill_mask &= (uint8_t)(0xFFU >> (7U - (clip_last & 7U)));
    }
    if (gfx_fill_bytes(lcd_fb_page(page) + fill_left) != 0U) {
      lcd_fb_mark_dirty(page);
    }
  }
}

uint8_t gfx_format_u16(uint16_t value, char* buffer) {
  uint8_t place;
  uint8_t count = 0U;
  char digit;

  for (place = 0U; place < GFX_DECIMAL_PLACES; ++place) {
    digit = '0';
    while (value >= gfx_decimal_places[place]) {
      value = (uint16_t)(value - gfx_decimal_places[place]);
      ++digit;
    }
    if ((count != 0U) || (digit != '0')) {
      buffer[count] = digit;
      ++count;
    }
  }
  buffer[count] = (char)('0' + (uint8_t)value);
  ++count;
  buffer[count] = '\0';
  return count;
}
