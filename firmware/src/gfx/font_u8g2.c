/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "font_u8g2.h"

#include <stddef.h>

#include "lcd/lcd_fb.h"

/* u8g2 font header (references/internal/menu_subsystem_spec.md section 4.2). */
#define FONT_HEADER_SIZE 23U
#define FONT_OFFSET_UPPER 17U
#define FONT_OFFSET_LOWER 19U

/* Header bytes 2..12 are kept in font_header; the indexes below are relative to byte 2. */
#define FONT_HEADER_FIRST 2U
#define FONT_HEADER_COUNT 11U
#define FONT_BITS_ZERO_RUN 0U
#define FONT_BITS_ONE_RUN 1U
#define FONT_BITS_GLYPH_FIELDS 2U /* bit widths of the glyph fields, in glyph_fields order */
#define FONT_BBOX_HEIGHT 8U
#define FONT_BBOX_Y_OFFSET 10U

/* Glyph header fields in bitstream order. Fields from GLYPH_X_OFFSET on are signed. */
#define GLYPH_WIDTH 0U
#define GLYPH_HEIGHT 1U
#define GLYPH_X_OFFSET 2U
#define GLYPH_Y_OFFSET 3U
#define GLYPH_PITCH 4U
#define GLYPH_FIELD_COUNT 5U

#define FONT_GLYPH_ENCODING 0U
#define FONT_GLYPH_SIZE 1U
#define FONT_GLYPH_BITSTREAM 2U

static const uint8_t MEM_CODE* MEM_XDATA font_data;
static MEM_XDATA uint8_t font_header[FONT_HEADER_COUNT];
/* Rows from the cell top to the baseline: bbox height + bbox y offset. */
static MEM_XDATA int8_t font_ascent;

/*
 * Bitstream read position, and 1 << count of the last read. The decoder uses them for every field, so they stay
 * in internal RAM (4 bytes).
 */
static const uint8_t MEM_CODE* font_bit_ptr;
static uint8_t font_bit_mask;
static uint8_t font_read_end_bit;

static MEM_XDATA int8_t glyph_fields[GLYPH_FIELD_COUNT];
/* Rows from the cell top to the glyph box top. */
static MEM_XDATA int8_t glyph_top;

/* Next pixel of the glyph box (internal RAM, 2 bytes: the pixel loop uses it) and the screen position of the box. */
static uint8_t cursor_column;
static uint8_t cursor_row;
static MEM_XDATA int16_t box_left;
static MEM_XDATA int16_t box_top;

static MEM_XDATA uint8_t run_zeros;
static MEM_XDATA uint8_t run_ones;

#define GLYPH_BOX_WIDTH ((uint8_t)glyph_fields[GLYPH_WIDTH])
#define GLYPH_BOX_HEIGHT ((uint8_t)glyph_fields[GLYPH_HEIGHT])

/* Reads @p count bits, least significant bit first. A field wider than 8 bits keeps only its low 8 bits. */
static uint8_t font_read_bits(uint8_t count) {
  uint8_t value = 0U;

  font_read_end_bit = 1U;
  while (count != 0U) {
    if ((*font_bit_ptr & font_bit_mask) != 0U) {
      value |= font_read_end_bit;
    }
    font_bit_mask <<= 1;
    if (font_bit_mask == 0U) {
      font_bit_mask = 1U;
      ++font_bit_ptr;
    }
    font_read_end_bit <<= 1;
    --count;
  }
  return value;
}

/* Moves the bitstream to glyph @p encoding. Returns false when the font has no such glyph. */
static bool font_find_glyph(uint8_t encoding) {
  const uint8_t MEM_CODE* glyph = font_data;
  uint8_t offset_index = 0U;
  uint8_t size;

  if ((encoding >= (uint8_t)'A') && (encoding <= (uint8_t)'Z')) {
    offset_index = FONT_OFFSET_UPPER;
  } else if ((encoding >= (uint8_t)'a') && (encoding <= (uint8_t)'z')) {
    offset_index = FONT_OFFSET_LOWER;
  }
  if (offset_index != 0U) {
    glyph += (uint16_t)(((uint16_t)glyph[offset_index] << 8) | glyph[(uint8_t)(offset_index + 1U)]);
  }
  glyph += FONT_HEADER_SIZE;
  for (;;) {
    size = glyph[FONT_GLYPH_SIZE];
    if (size == 0U) {
      return false;
    }
    if (glyph[FONT_GLYPH_ENCODING] == encoding) {
      break;
    }
    glyph += size;
  }
  font_bit_ptr = glyph + FONT_GLYPH_BITSTREAM;
  font_bit_mask = 1U;
  return true;
}

/* Reads the glyph header fields. Returns false with no font or no such glyph. */
static bool font_load_glyph(uint8_t encoding) {
  uint8_t field;

  if (font_data == NULL) {
    return false;
  }
  if (!font_find_glyph(encoding)) {
    return false;
  }
  for (field = 0U; field < GLYPH_FIELD_COUNT; ++field) {
    const uint8_t count = font_header[(uint8_t)(FONT_BITS_GLYPH_FIELDS + field)];
    uint8_t value = font_read_bits(count);
    /* u8g2 stores a signed field as the raw value minus 1 << (count - 1). The end bit is 0 after 8 bits. */
    if (field >= GLYPH_X_OFFSET) {
      value = (uint8_t)(value - ((font_read_end_bit == 0U) ? 0x80U : (uint8_t)(font_read_end_bit >> 1)));
    }
    glyph_fields[field] = (int8_t)value;
  }
  glyph_top = (int8_t)(font_ascent - glyph_fields[GLYPH_HEIGHT] - glyph_fields[GLYPH_Y_OFFSET]);
  return true;
}

/* Paints the next @p count pixels of the glyph box row by row. Pixels after the last row are dropped. */
static void font_put_pixels(uint8_t count, MEM_XDATA uint8_t color) {
  while ((count != 0U) && (cursor_row != GLYPH_BOX_HEIGHT)) {
    lcd_fb_set_pixel((int16_t)(box_left + cursor_column), (int16_t)(box_top + cursor_row), color);
    ++cursor_column;
    if (cursor_column == GLYPH_BOX_WIDTH) {
      cursor_column = 0U;
      ++cursor_row;
    }
    --count;
  }
}

bool font_u8g2_select(const uint8_t MEM_CODE* font) {
  uint8_t index;

  if (font == NULL) {
    return false;
  }
  for (index = 0U; index < FONT_HEADER_COUNT; ++index) {
    font_header[index] = font[(uint8_t)(FONT_HEADER_FIRST + index)];
  }
  font_ascent = (int8_t)(font_header[FONT_BBOX_HEIGHT] + font_header[FONT_BBOX_Y_OFFSET]);
  font_data = font;
  return true;
}

uint8_t font_u8g2_cell_height(void) {
  if (font_data == NULL) {
    return 0U;
  }
  return font_header[FONT_BBOX_HEIGHT];
}

int8_t font_u8g2_glyph_pitch(uint8_t encoding) {
  if (!font_load_glyph(encoding)) {
    return 0;
  }
  return glyph_fields[GLYPH_PITCH];
}

int8_t font_u8g2_draw_glyph(int16_t x, MEM_XDATA int16_t y, MEM_XDATA uint8_t encoding, MEM_XDATA uint8_t fg_color,
                            MEM_XDATA uint8_t bg_color) {
  if (!font_load_glyph(encoding)) {
    return 0;
  }
  box_left = (int16_t)(x + glyph_fields[GLYPH_X_OFFSET]);
  box_top = (int16_t)(y + glyph_top);
  cursor_column = 0U;
  cursor_row = 0U;
  if (GLYPH_BOX_WIDTH == 0U) {
    return glyph_fields[GLYPH_PITCH];
  }
  /* Each run pair adds at least one pixel, and a full box stops the repeats, so the loop is bounded. */
  while (cursor_row != GLYPH_BOX_HEIGHT) {
    run_zeros = font_read_bits(font_header[FONT_BITS_ZERO_RUN]);
    run_ones = font_read_bits(font_header[FONT_BITS_ONE_RUN]);
    if ((run_zeros == 0U) && (run_ones == 0U)) {
      break;
    }
    do {
      font_put_pixels(run_zeros, bg_color);
      font_put_pixels(run_ones, fg_color);
    } while ((cursor_row != GLYPH_BOX_HEIGHT) && (font_read_bits(1U) != 0U));
  }
  return glyph_fields[GLYPH_PITCH];
}
