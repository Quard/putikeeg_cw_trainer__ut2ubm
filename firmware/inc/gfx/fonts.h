#ifndef GFX_FONTS_H
#define GFX_FONTS_H

/* u8g2 fonts in code memory. */

#include <stdint.h>

#include "mcu/compiler.h"

#define FONTS_ID_5X8 0U  /* u8g2_font_spleen5x8_mf, the UI font */
#define FONTS_ID_8X16 1U /* u8g2_font_spleen8x16_mf */
#define FONTS_COUNT 2U

/** Returns the u8g2 font data of @p font_id, or NULL for an unknown id. */
const uint8_t MEM_CODE* fonts_get(uint8_t font_id);

#endif /* GFX_FONTS_H */
