/*
 * u8g2_font_render.c
 *
 *  Created on: Nov 27, 2020
 *      Author: quard
 */
#include "u8g2_font_render.h"


static uint8_t font_get_unsigned_bits(U8G2FontGlyph_t *glyph, uint8_t count);
static int8_t font_get_signed_bits(U8G2FontGlyph_t *glyph, uint8_t count);
static uint16_t font_get_start_symbol_search_postition(const U8G2FontRender_t *font, char chr);
static int8_t font_get_glyph(const U8G2FontRender_t *font, U8G2FontGlyph_t *glyph, uint16_t search_position, char chr);
static void font_parse_glyph_header(const U8G2FontRender_t *font, U8G2FontGlyph_t *glyph);
static int16_t font_draw_start_x_position(const U8G2FontGlyph_t *glyph);
static int16_t font_draw_start_y_position(const U8G2FontRender_t *font, const U8G2FontGlyph_t *glyph);
static void font_render_glyph(U8G2FontRender_t *font, U8G2FontGlyph_t *glyph, int16_t x, int16_t y);


U8G2FontRender_t U8G2FontRender(const uint8_t *data, fnDrawPixel drawFgPixel,
                                fnDrawPixel drawBgPixel, void *drawContext) {
    U8G2FontRender_t font = {
            .data = data,
            .drawFgPixel = drawFgPixel,
            .drawBgPixel = drawBgPixel,
            .drawContext = drawContext,
    };

    font.header = U8G2FontRender_ParseHeader(&font);

    return font;
}

U8G2FontHeader_t U8G2FontRender_ParseHeader(const U8G2FontRender_t *font) {
    U8G2FontHeader_t header = {0};

    header.number_of_glyphs = font->data[0];
    header.bounding_box_mode = font->data[1];
    header.zero_bit_width = font->data[2];
    header.one_bit_width = font->data[3];
    header.glyph_width = font->data[4];
    header.glyph_height = font->data[5];
    header.glyph_x_offset = font->data[6];
    header.glyph_y_offset = font->data[7];
    header.glyph_pitch = font->data[8];
    header.bounding_box_width = (int8_t)font->data[9];
    header.bounding_box_height = (int8_t)font->data[10];
    header.bounding_box_x_offset = (int8_t)font->data[11];
    header.bounding_box_y_offset = (int8_t)font->data[12];
    header.ascent_A = (int8_t)font->data[13];
    header.descent_g = (int8_t)font->data[14];
    header.ascent_parentheses = (int8_t)font->data[15];
    header.descent_parentheses = (int8_t)font->data[16];
    header.offset_A = (uint16_t)(U8G2_FONT_HEADER_SIZE + ((uint16_t)font->data[17] << 8U) + font->data[18]);
    header.offset_a = (uint16_t)(U8G2_FONT_HEADER_SIZE + ((uint16_t)font->data[19] << 8U) + font->data[20]);
    header.offset_0x100 = (uint16_t)(U8G2_FONT_HEADER_SIZE + ((uint16_t)font->data[21] << 8U) + font->data[22]);

    return header;
}

void U8G2FontRender_PrintChar(U8G2FontRender_t *font, int16_t *x, int16_t y, char chr) {
    uint16_t search_position = font_get_start_symbol_search_postition(font, chr);

    U8G2FontGlyph_t glyph;
    if (font_get_glyph(font, &glyph, search_position, chr) != U8G2FontRender_OK) {
        return;
    }
    font_parse_glyph_header(font, &glyph);

    font_render_glyph(font, &glyph, *x, y);

    *x = (int16_t)(*x + glyph.pitch);
}

void U8G2FontRender_Print(U8G2FontRender_t *font, int16_t x, int16_t y, const char *str) {
    while (*str) {
        U8G2FontRender_PrintChar(font, &x, y, *str++);
    }
}

uint16_t U8G2FontRender_GetWidth(U8G2FontRender_t *font, const char *str) {
  uint16_t width = 0;
  U8G2FontGlyph_t glyph;
  uint16_t search_position;

  while (*str) {
    char chr = *str++;

    search_position = font_get_start_symbol_search_postition(font, chr);
    if (font_get_glyph(font, &glyph, search_position, chr) != U8G2FontRender_OK) {
        continue;
    }
    font_parse_glyph_header(font, &glyph);
    if (glyph.pitch > 0) {
        width = (uint16_t)(width + (uint16_t)glyph.pitch);
    }
  }

  return width;
}

static uint8_t font_get_unsigned_bits(U8G2FontGlyph_t *glyph, uint8_t count) {
  uint8_t val;
  uint8_t start = glyph->bit_pos;
  uint8_t end = start + count;

  val = pgm_read(glyph->data);
  val >>= start;

  if (end >= 8) {
    uint8_t cnt = 8 - start;
    glyph->data++;

    val |= pgm_read(glyph->data) << (cnt);

    end -= 8;
  }

  glyph->bit_pos = end;

  val &= (1U << count) - 1;

  return val;
}

static int8_t font_get_signed_bits(U8G2FontGlyph_t *glyph, uint8_t count) {
  int8_t val = (int8_t)font_get_unsigned_bits(glyph, count);
  val -= 1 << (count - 1);

  return val;
}

static uint16_t font_get_start_symbol_search_postition(const U8G2FontRender_t *font, char chr) {
    uint16_t search_position = U8G2_FONT_HEADER_SIZE;
    if (chr >= 65 && chr <= 90) {
        search_position = font->header.offset_A;
    } else if (chr >= 97 && chr <= 122) {
        search_position = font->header.offset_a;
    }

    return search_position;
}

static int8_t font_get_glyph(const U8G2FontRender_t *font, U8G2FontGlyph_t *glyph, uint16_t search_position, char chr) {
    while (1) {
        glyph->character = font->data[search_position];
        glyph->next_glypth = font->data[search_position + 1U];
        if (glyph->character == chr) {
            glyph->data = font->data + search_position + 2;
            glyph->bit_pos = 0;

            return U8G2FontRender_OK;
        }

        search_position += glyph->next_glypth;
        if (glyph->next_glypth == 0) {
            break;
        }
    }

    return U8G2FontRender_ERR;
}

static void font_parse_glyph_header(const U8G2FontRender_t *font, U8G2FontGlyph_t *glyph) {
    glyph->width = font_get_unsigned_bits(glyph, font->header.glyph_width);
    glyph->height = font_get_unsigned_bits(glyph, font->header.glyph_height);
    glyph->x_offset = font_get_signed_bits(glyph, font->header.glyph_x_offset);
    glyph->y_offset = font_get_signed_bits(glyph, font->header.glyph_y_offset);
    glyph->pitch = font_get_signed_bits(glyph, font->header.glyph_pitch);
}

static int16_t font_draw_start_x_position(const U8G2FontGlyph_t *glyph) {
    return glyph->x_offset;
}

static int16_t font_draw_start_y_position(const U8G2FontRender_t *font, const U8G2FontGlyph_t *glyph) {
    return font->header.bounding_box_height - glyph->height - glyph->y_offset;
}

static void font_render_glyph(U8G2FontRender_t *font, U8G2FontGlyph_t *glyph, int16_t x, int16_t y) {
    uint32_t pixels = 0;
    int16_t y_pos = y + font_draw_start_y_position(font, glyph);
    int16_t x_pos = x + font_draw_start_x_position(glyph);
    while (1) {
        uint8_t zeros = font_get_unsigned_bits(glyph, font->header.zero_bit_width);
        uint8_t ones = font_get_unsigned_bits(glyph, font->header.one_bit_width);
        int8_t repeat = 0;

        while (font_get_unsigned_bits(glyph, 1) == 1) {
            repeat++;
        }

        for (; repeat>=0; repeat--) {
            for (uint8_t i=0; i<zeros + ones; i++) {
                if (i <= zeros - 1) {
                    font->drawBgPixel(font->drawContext, x_pos, y_pos);
                } else {
                    font->drawFgPixel(font->drawContext, x_pos, y_pos);
                }
                x_pos++;

                pixels++;
                if (pixels % glyph->width == 0) {
                    y_pos++;
                    x_pos = x + font_draw_start_x_position(glyph);
                }
            }
        }

        if (pixels >= glyph->width * glyph->height) {
            break;
        }
    }
}
