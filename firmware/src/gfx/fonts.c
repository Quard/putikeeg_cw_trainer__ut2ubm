#include "gfx/fonts.h"

#include <stddef.h>
#include <stdint.h>

/* The fonts stay in the default constant area: code memory on the target. */
#define U8G2_FONT_SECTION(name)
#include "u8g2_fonts.h"

const uint8_t MEM_CODE* fonts_get(uint8_t font_id) {
  if (font_id == FONTS_ID_5X8) {
    return (const uint8_t MEM_CODE*)u8g2_font_spleen5x8_mf;
  }
  if (font_id == FONTS_ID_8X16) {
    return (const uint8_t MEM_CODE*)u8g2_font_spleen8x16_mf;
  }
  return NULL;
}
