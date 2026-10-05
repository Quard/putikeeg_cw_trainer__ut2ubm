/*
 * u8g2_font_render.h
 *
 *  Created on: Nov 27, 2020
 *      Author: quard
 */

#ifndef INC_U8G2_FONT_RENDER_H_
#define INC_U8G2_FONT_RENDER_H_

#include <stdint.h>


#define U8G2_FONT_HEADER_SIZE 23

#define U8G2FontRender_OK  0x01
#define U8G2FontRender_ERR 0x02


#define pgm_read(adr) (*(const uint8_t *)(adr))

typedef void (*fnDrawPixel)(void *context, int16_t x, int16_t y);

typedef struct {
  uint8_t number_of_glyphs;
  uint8_t bounding_box_mode;
  uint8_t zero_bit_width;
  uint8_t one_bit_width;

  uint8_t glyph_width;
  uint8_t glyph_height;
  uint8_t glyph_x_offset;
  uint8_t glyph_y_offset;
  uint8_t glyph_pitch;

  int8_t bounding_box_width;
  int8_t bounding_box_height;
  int8_t bounding_box_x_offset;
  int8_t bounding_box_y_offset;

  int8_t ascent_A;
  int8_t descent_g;
  int8_t ascent_parentheses;
  int8_t descent_parentheses;

  uint16_t offset_A;
  uint16_t offset_a;
  uint16_t offset_0x100;
} U8G2FontHeader_t;

typedef struct {
  uint8_t character;
  uint8_t next_glypth;

  uint8_t width;
  uint8_t height;
  int8_t x_offset;
  int8_t y_offset;
  int8_t pitch;

  uint8_t bit_pos;
  const uint8_t *data;
} U8G2FontGlyph_t;

typedef struct {
    U8G2FontHeader_t header;

    const uint8_t *data;

    fnDrawPixel drawFgPixel;
    fnDrawPixel drawBgPixel;
    void *drawContext;
} U8G2FontRender_t;


U8G2FontRender_t U8G2FontRender(const uint8_t *data, fnDrawPixel drawFgPixel,
                                fnDrawPixel drawBgPixel, void *drawContext);
U8G2FontHeader_t U8G2FontRender_ParseHeader(const U8G2FontRender_t *font);
void U8G2FontRender_PrintChar(U8G2FontRender_t *font, int16_t *x, int16_t y,
                              char chr);
void U8G2FontRender_Print(U8G2FontRender_t *font, int16_t x, int16_t y,
                          const char *str);
uint16_t U8G2FontRender_GetWidth(U8G2FontRender_t *font, const char *str);


#endif /* INC_U8G2_FONT_RENDER_H_ */
