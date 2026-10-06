#include "modes/free_input.h"

#include <stdbool.h>
#include <stdint.h>

#include "buttons/buttons.h"
#include "cw/keyer.h"
#include "cw/keyer_shared.h"
#include "cw/morse.h"
#include "gfx/gfx.h"
#include "lcd/lcd_fb.h"
#include "mcu/compiler.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"

/* Text area of ui-ux.md section 5.9: rows 1-7 of 25 cells of 5 x 8 pixels from x = 2. */
#define FREE_INPUT_FIRST_ROW 1U
#define FREE_INPUT_LAST_ROW 7U
#define FREE_INPUT_COLUMNS 25U /* a cursor in this column means "line full" */
#define FREE_INPUT_GRID_X 2U
/* Delimiter: one line at y = 8r + 3 over the 25 cells, x 2-126. */
#define FREE_INPUT_DELIMITER_Y_OFFSET 3U
#define FREE_INPUT_DELIMITER_WIDTH (FREE_INPUT_COLUMNS * GFX_FONT_5X8_WIDTH)
/* The queue holds KEYER_QUEUE_SIZE events, and an overflow adds one. */
#define FREE_INPUT_EVENTS_PER_POLL (KEYER_QUEUE_SIZE + 1U)

#if (MORSE_STYLE_PUNCTUATION != SETTINGS_PROSIGN_PUNCTUATION) || (MORSE_STYLE_NAME != SETTINGS_PROSIGN_NAME) || \
    (MORSE_STYLE_BOTH != SETTINGS_PROSIGN_BOTH)
#error "The morse text styles must equal the settings prosign styles"
#endif

/* The framebuffer is the only text store; every cell at the cursor and after it is blank. */
static MEM_XDATA uint8_t free_input_row;
static MEM_XDATA uint8_t free_input_column;
static MEM_XDATA uint8_t free_input_pattern;
/* One glyph and its NUL for gfx_draw_text(). In xdata, so the helpers below need no parameter in internal RAM. */
static MEM_XDATA char free_input_glyph[2];
static MEM_XDATA uint8_t scroll_page;
/* Held in xdata across the settings_get() calls of free_input_enter(). */
static MEM_XDATA bool enter_is_swapped;
/* Read once at the mode start, so a settings change applies at the next start (keyer_spec.md D-P9). */
static MEM_XDATA uint8_t free_input_prosign_style;
static MEM_XDATA char free_input_text[MORSE_TEXT_SIZE];

static void free_input_reset_cursor(void) {
  free_input_row = FREE_INPUT_FIRST_ROW;
  free_input_column = 0U;
  free_input_pattern = MORSE_PATTERN_EMPTY;
}

/* Moves rows 2-7 up to rows 1-6 and clears row 7. Page 0, the title bar, is never written. */
static void free_input_scroll(void) {
  MEM_XDATA uint8_t* destination;
  MEM_XDATA uint8_t* source;
  uint8_t index;

  for (scroll_page = FREE_INPUT_FIRST_ROW; scroll_page < FREE_INPUT_LAST_ROW; ++scroll_page) {
    destination = lcd_fb_page(scroll_page);
    source = lcd_fb_page((uint8_t)(scroll_page + 1U));
    for (index = 0U; index < LCD_WIDTH; ++index) {
      destination[index] = source[index];
    }
  }
  destination = lcd_fb_page(FREE_INPUT_LAST_ROW);
  for (index = 0U; index < LCD_WIDTH; ++index) {
    destination[index] = 0U;
  }
  for (scroll_page = FREE_INPUT_FIRST_ROW; scroll_page <= FREE_INPUT_LAST_ROW; ++scroll_page) {
    lcd_fb_mark_dirty(scroll_page);
  }
}

static void free_input_new_line(void) {
  if (free_input_row < FREE_INPUT_LAST_ROW) {
    ++free_input_row;
  } else {
    free_input_scroll();
  }
  free_input_column = 0U;
}

/* Draws free_input_glyph[0] at the cursor. The wrap is lazy: a full line moves on only for the next glyph. */
static void free_input_draw_glyph(void) {
  if (free_input_column >= FREE_INPUT_COLUMNS) {
    free_input_new_line();
  }
  free_input_glyph[1] = '\0';
  (void)gfx_draw_text((int16_t)(FREE_INPUT_GRID_X + (uint8_t)(free_input_column * GFX_FONT_5X8_WIDTH)),
                      (int16_t)(free_input_row * GFX_FONT_5X8_HEIGHT), free_input_glyph, GFX_TEXT_NORMAL);
  ++free_input_column;
}

static void free_input_draw(char glyph) {
  free_input_glyph[0] = glyph;
  free_input_draw_glyph();
}

/* Draws the text of the pattern in progress, if any, and starts a new pattern. */
static void free_input_emit(void) {
  uint8_t length;
  uint8_t index;

  if (free_input_pattern == MORSE_PATTERN_EMPTY) {
    return;
  }
  length = morse_format(morse_decode(free_input_pattern), free_input_prosign_style, free_input_text);
  free_input_pattern = MORSE_PATTERN_EMPTY;
  /* Each glyph wraps on its own, so a text of several cells can split across two lines. */
  for (index = 0U; index < length; ++index) {
    free_input_draw(free_input_text[index]);
  }
}

/* A line never starts with a space, and the line end of a full line acts as the space. */
static void free_input_word_space(void) {
  if ((free_input_column != 0U) && (free_input_column < FREE_INPUT_COLUMNS)) {
    ++free_input_column;
  }
}

void free_input_enter(void) {
  buttons_set_enter_enabled(false);
  sidetone_set_tone_hz(settings_get(SETTINGS_ID_TONE_HZ));
  enter_is_swapped = false;
  if (settings_get(SETTINGS_ID_PADDLE_SWAP) != 0U) {
    enter_is_swapped = true;
  }
  keyer_start((uint8_t)settings_get(SETTINGS_ID_KEY_TYPE), enter_is_swapped, (uint8_t)settings_get(SETTINGS_ID_TX_WPM),
              (uint8_t)settings_get(SETTINGS_ID_TX_EFF_WPM));
  free_input_prosign_style = (uint8_t)settings_get(SETTINGS_ID_PROSIGN_STYLE);
  free_input_reset_cursor();
}

void free_input_poll(void) {
  uint8_t count;
  keyer_event_t event;

  for (count = 0U; count < FREE_INPUT_EVENTS_PER_POLL; ++count) {
    event = keyer_read_event();
    if (event == KEYER_EVENT_NONE) {
      return;
    }
    if ((event == KEYER_EVENT_DIT) || (event == KEYER_EVENT_DAH)) {
      free_input_pattern = morse_add_element(free_input_pattern, event == KEYER_EVENT_DAH);
    } else if (event == KEYER_EVENT_CHAR_GAP) {
      free_input_emit();
    } else if (event == KEYER_EVENT_WORD_GAP) {
      free_input_emit();
      free_input_word_space();
    } else if (event == KEYER_EVENT_OVERFLOW) {
      /* The lost events belong to this character, so it shows as unknown. */
      free_input_pattern = MORSE_PATTERN_INVALID;
    }
  }
}

void free_input_clear(void) {
  gfx_fill_rect(0, (int16_t)(FREE_INPUT_FIRST_ROW * GFX_FONT_5X8_HEIGHT), (int16_t)LCD_WIDTH,
                (int16_t)((FREE_INPUT_LAST_ROW + 1U - FREE_INPUT_FIRST_ROW) * GFX_FONT_5X8_HEIGHT), LCD_PIXEL_OFF);
  free_input_reset_cursor();
}

void free_input_delimiter(void) {
  if (free_input_column != 0U) {
    free_input_new_line();
  }
  gfx_fill_rect((int16_t)FREE_INPUT_GRID_X,
                (int16_t)((uint8_t)(free_input_row * GFX_FONT_5X8_HEIGHT) + FREE_INPUT_DELIMITER_Y_OFFSET),
                (int16_t)FREE_INPUT_DELIMITER_WIDTH, 1, LCD_PIXEL_ON);
  free_input_column = FREE_INPUT_COLUMNS;
}

void free_input_leave(void) {
  keyer_stop();
  buttons_set_enter_enabled(true);
}
