#include "modes/training_screen.h"

#include <stdbool.h>
#include <stdint.h>

#include "buttons/buttons.h"
#include "cw/morse.h"
#include "gfx/fonts.h"
#include "gfx/gfx.h"
#include "koch/koch.h"
#include "lcd/lcd_fb.h"
#include "mcu/compiler.h"
#include "modes/training.h"
#include "settings/settings.h"
#include "training_screen_internal.h"

/* Grid and layout of ui-ux.md sections 5.11, 5.15 and 6.10. */
#define SCREEN_GRID_X 2U
#define SCREEN_ROW_HEIGHT GFX_FONT_5X8_HEIGHT
#define SCREEN_SPEED_COLUMN 3U
#define SCREEN_LEVEL_COLUMN 9U
#define SCREEN_ITEM_Y 24        /* TRAINING_ITEM_Y, cell top of the 8 x 16 font */
#define SCREEN_SLOT_BASE_X 68   /* slot k of n characters: x = 68 - 8n + 16k */
#define SCREEN_CHAR_CENTER_X 64 /* an item of one character of m cells: x = 64 - 4m */
#define SCREEN_MARK_WIDTH 7
#define SCREEN_BLOCK_Y 26
#define SCREEN_BLOCK_HEIGHT 14
#define SCREEN_BAR_Y 38
#define SCREEN_BAR_HEIGHT 2
#define SCREEN_END_SAVE_ROW 1U
#define SCREEN_END_SCORE_ROW 2U
#define SCREEN_END_RESULT_ROW 3U
#define SCREEN_SELECTOR_ROW 4U
#define SCREEN_DESCRIPTION_ROW 5U
#define SCREEN_START_COLUMN 10U
#define SCREEN_START_BAND_X 47 /* cells 9-15, x 47-81 */
#define SCREEN_START_BAND_WIDTH 35
#define SCREEN_START_BAND_Y 57 /* y 56, the gap line, stays off */
#define SCREEN_START_BAND_HEIGHT 7
#define SCREEN_NUMBER_SIZE ((2U * GFX_U16_TEXT_SIZE) + 1U) /* "100/100" and room for two numbers */
_Static_assert(SCREEN_NUMBER_SIZE >= MORSE_TEXT_SIZE, "the number buffer also holds the text of one character");

MEM_XDATA char training_screen_text[TRAINING_SCREEN_COLUMNS + 1U];
static MEM_XDATA uint8_t screen_length;
/* Digits for an append, or the text of one character: never both at once. */
static MEM_XDATA char screen_number[SCREEN_NUMBER_SIZE];
#define screen_glyph_text screen_number
/* Held in xdata across the calls of training_screen_draw_header(). */
static MEM_XDATA bool screen_is_receiving;
/* A pixel x, or the item count of the header: one xdata work value, never both at once. */
static MEM_XDATA int16_t screen_x;
/* Sending: a word space inside a word ended the attempt (UX-52, D-T31). */
static const MEM_CODE char screen_pause_text[] = "pause too long";

void training_screen_compose(void) {
  screen_length = 0U;
  training_screen_text[0] = '\0';
}

void training_screen_append(const char* text) {
  while ((*text != '\0') && (screen_length < TRAINING_SCREEN_COLUMNS)) {
    training_screen_text[screen_length] = *text;
    ++screen_length;
    ++text;
  }
  training_screen_text[screen_length] = '\0';
}

void training_screen_append_u16(uint16_t value) {
  (void)gfx_format_u16(value, screen_number);
  training_screen_append(screen_number);
}

/* Appends spaces up to @p column. */
static void screen_pad(uint8_t column) {
  while (screen_length < column) {
    training_screen_append(" ");
  }
}

void training_screen_clear_rows(uint8_t first_row, MEM_XDATA uint8_t last_row) {
  gfx_fill_rect(0, (int16_t)(first_row * SCREEN_ROW_HEIGHT), (int16_t)LCD_WIDTH,
                (int16_t)((uint8_t)(last_row + 1U - first_row) * SCREEN_ROW_HEIGHT), LCD_PIXEL_OFF);
}

void training_screen_draw_text(uint8_t row, MEM_XDATA uint8_t column, const char* MEM_XDATA text) {
  (void)gfx_draw_text((int16_t)(SCREEN_GRID_X + (uint8_t)(column * GFX_FONT_5X8_WIDTH)),
                      (int16_t)(row * SCREEN_ROW_HEIGHT), text, GFX_TEXT_NORMAL);
}

void training_screen_draw_centered(uint8_t row) {
  training_screen_clear_rows(row, row);
  training_screen_draw_text(row, (uint8_t)((TRAINING_SCREEN_COLUMNS - screen_length) / 2U), training_screen_text);
}

static int16_t screen_slot_x(uint8_t slot, uint8_t count) {
  return (int16_t)(SCREEN_SLOT_BASE_X - (int16_t)(8U * count) + (int16_t)(16U * slot));
}

void training_screen_draw_item_text(const char MEM_XDATA* text, MEM_XDATA uint8_t style) {
  const uint8_t count = training_item_length();
  uint8_t slot;
  uint8_t length;

  training_screen_clear_rows(TRAINING_SCREEN_ITEM_FIRST_ROW, TRAINING_SCREEN_ITEM_LAST_ROW);
  (void)gfx_set_font(FONTS_ID_8X16);
  for (slot = 0U; text[slot] != '\0'; ++slot) {
    length = morse_format(text[slot], style, screen_glyph_text);
    screen_x = screen_slot_x(slot, count);
    if (count == 1U) {
      screen_x = (int16_t)(SCREEN_CHAR_CENTER_X - (int16_t)(4U * length));
    }
    (void)gfx_draw_text(screen_x, SCREEN_ITEM_Y, screen_glyph_text, GFX_TEXT_NORMAL);
  }
  (void)gfx_set_font(FONTS_ID_5X8);
}

void training_screen_draw_status(uint8_t style) {
  const training_check_t check = training_last_check();

  training_screen_compose();
  if (training_phase() == TRAINING_PHASE_RESULT) {
    if (check == TRAINING_CHECK_CORRECT) {
      training_screen_append("Correct");
    } else if (training_mode() == TRAINING_MODE_SENDING) {
      /* The glyph buffer holds the last answer character until its text replaces it. */
      screen_glyph_text[0] = training_answer_text()[training_answer_length() - 1U];
      training_screen_append("Wrong: ");
      if (screen_glyph_text[0] == TRAINING_CHAR_WORD_SPACE) {
        training_screen_append(screen_pause_text);
      } else {
        (void)morse_format(screen_glyph_text[0], style, screen_glyph_text);
        training_screen_append(screen_glyph_text);
      }
    } else if (check == TRAINING_CHECK_WRONG) {
      training_screen_append("Wrong");
    } else {
      training_screen_append("Wrong. The answer is:");
    }
  } else if ((training_phase() == TRAINING_PHASE_ITEM) && (training_wrong_attempts() != 0U)) {
    training_screen_append("Try ");
    training_screen_append_u16((uint16_t)(training_wrong_attempts() + 1U));
    training_screen_append(" of 3");
  }
  training_screen_draw_centered(TRAINING_SCREEN_STATUS_ROW);
}

void training_screen_draw_mark(uint8_t slot, MEM_XDATA uint8_t count, MEM_XDATA bool is_block) {
  const int16_t x = screen_slot_x(slot, count);

  if (is_block) {
    gfx_fill_rect(x, SCREEN_BLOCK_Y, SCREEN_MARK_WIDTH, SCREEN_BLOCK_HEIGHT, LCD_PIXEL_ON);
  } else {
    gfx_fill_rect(x, SCREEN_BAR_Y, SCREEN_MARK_WIDTH, SCREEN_BAR_HEIGHT, LCD_PIXEL_ON);
  }
}

/*
 * ui-ux.md section 5.11.1: tag, speeds from column 3, level from column 9, item right-aligned to column 24. In SELECT
 * the header is the one of the start pause at the selected level (SL-6), so a start does not redraw it.
 */
void training_screen_draw_header(void) {
  uint8_t length;

  screen_is_receiving = (training_mode() == TRAINING_MODE_RECEIVING);
  training_screen_compose();
  training_screen_append(screen_is_receiving ? "RX" : "TX");
  screen_pad(SCREEN_SPEED_COLUMN);
  training_screen_append_u16(training_wpm());
  training_screen_append("/");
  training_screen_append_u16(settings_get(screen_is_receiving ? SETTINGS_ID_RX_EFF_WPM : SETTINGS_ID_TX_EFF_WPM));
  screen_pad(SCREEN_LEVEL_COLUMN);
  training_screen_append("L");
  if (training_phase() == TRAINING_PHASE_SELECT) {
    training_screen_append_u16(training_new_shown_level());
    screen_number[0] = '1';
    length = 1U;
    screen_x = koch_item_count(training_selected_level());
  } else {
    training_screen_append_u16(training_shown_level());
    length = gfx_format_u16(training_shown_item_number(), screen_number);
    screen_x = training_item_count();
  }
  screen_number[length] = '/';
  ++length;
  length = (uint8_t)(length + gfx_format_u16((uint16_t)screen_x, &screen_number[length]));
  screen_pad((uint8_t)(TRAINING_SCREEN_COLUMNS - length));
  training_screen_append(screen_number);
  gfx_fill_rect(0, 0, (int16_t)LCD_WIDTH, (int16_t)SCREEN_ROW_HEIGHT, LCD_PIXEL_ON);
  (void)gfx_draw_text((int16_t)SCREEN_GRID_X, 0, training_screen_text, GFX_TEXT_INVERTED);
}

/*
 * ui-ux.md section 5.16.2: '<', two blanks, "L<s> of <e>", two blanks, '>'. A marker shows only when a step is
 * possible; a blank keeps its cell, so the centered line starts in column floor((19 - len) / 2).
 */
static void screen_draw_selector(void) {
  training_screen_compose();
  training_screen_append("   L");
  training_screen_append_u16(training_new_shown_level());
  training_screen_append(" of ");
  training_screen_append_u16(koch_shown_level(training_earned_level()));
  training_screen_append("   ");
  if (training_selected_level() != 0U) {
    training_screen_text[0] = '<';
  }
  if (training_selected_level() < training_earned_level()) {
    training_screen_text[screen_length - 1U] = '>';
  }
  training_screen_draw_centered(SCREEN_SELECTOR_ROW);
}

/* ui-ux.md section 5.16.3 (SL-2): what the selected level adds. The settings cannot change on a training screen. */
static void screen_draw_description(void) {
  training_screen_compose();
  if (training_selected_level() == 0U) {
    training_screen_append("K and M");
  } else if (koch_kind(training_selected_level()) == KOCH_KIND_CHAR) {
    training_screen_append("New char ");
    (void)morse_format(koch_char((uint8_t)(koch_learned_count(training_selected_level()) - 1U)),
                       (uint8_t)settings_get(SETTINGS_ID_PROSIGN_STYLE), screen_glyph_text);
    training_screen_append(screen_glyph_text);
  } else {
    training_screen_append("Words of ");
    training_screen_append_u16(
        koch_word_length(training_selected_level(), (uint8_t)settings_get(SETTINGS_ID_MAX_WORD_LEN)));
  }
  training_screen_draw_centered(SCREEN_DESCRIPTION_ROW);
}

void training_screen_draw_select(void) {
  if (training_phase() == TRAINING_PHASE_SELECT) {
    training_screen_draw_header();
  }
  screen_draw_selector();
  screen_draw_description();
}

/*
 * ui-ux.md section 5.16.1: rows 1-3 and 6 blank, the selector, the description and the Start band. The end screen
 * draws its rows 1-3 over it, so both screens share one band (SL-5).
 */
void training_screen_draw_start(void) {
  training_screen_clear_rows(TRAINING_SCREEN_STATUS_ROW, TRAINING_SCREEN_LAST_ROW);
  screen_draw_selector();
  screen_draw_description();
  gfx_fill_rect(SCREEN_START_BAND_X, SCREEN_START_BAND_Y, SCREEN_START_BAND_WIDTH, SCREEN_START_BAND_HEIGHT,
                LCD_PIXEL_ON);
  (void)gfx_draw_text((int16_t)(SCREEN_GRID_X + (SCREEN_START_COLUMN * GFX_FONT_5X8_WIDTH)),
                      (int16_t)(TRAINING_SCREEN_LAST_ROW * SCREEN_ROW_HEIGHT), "Start", GFX_TEXT_INVERTED);
  /* Inverted text paints the gap line y 56; the band leaves it off (ui-ux.md UX-2). */
  gfx_fill_rect(SCREEN_START_BAND_X, (int16_t)(SCREEN_START_BAND_Y - 1), SCREEN_START_BAND_WIDTH, 1, LCD_PIXEL_OFF);
}

/* ui-ux.md section 5.15 (SL-7, D-T42 (3)). */
void training_screen_draw_end(void) {
  const training_outcome_t outcome = training_outcome();

  training_screen_draw_start();
  if (outcome == TRAINING_OUTCOME_PASS_NOT_SAVED) {
    training_screen_compose();
    training_screen_append("Not saved");
    training_screen_draw_centered(SCREEN_END_SAVE_ROW);
  }

  training_screen_compose();
  training_screen_append("Score ");
  training_screen_append_u16(training_score());
  training_screen_append("/");
  training_screen_append_u16(training_item_count());
  training_screen_draw_centered(SCREEN_END_SCORE_ROW);

  training_screen_compose();
  if (outcome == TRAINING_OUTCOME_FAIL) {
    training_screen_append("Not passed, need ");
    training_screen_append_u16(training_pass_threshold());
  } else {
    training_screen_append("Passed");
    if ((uint8_t)(outcome - TRAINING_OUTCOME_PASS) <= (TRAINING_OUTCOME_PASS_NOT_SAVED - TRAINING_OUTCOME_PASS)) {
      training_screen_append(", new level");
    }
  }
  training_screen_draw_centered(SCREEN_END_RESULT_ROW);
}

_Static_assert((TRAINING_OUTCOME_PASS_NOT_SAVED == (TRAINING_OUTCOME_PASS + 1U)),
               "the advancing passes must be one range");
_Static_assert(((BUTTON_EVENT_NEXT & 1U) != 0U) && ((BUTTON_EVENT_PREV & 1U) == 0U), "bit 0 must tell next from prev");

/* ui-ux.md section 5.16.4 (D-T38). Tap mode is off, so a press acts at once; a long event never acts. */
training_select_action_t training_screen_select_input(button_event_t event) {
  const uint8_t code = BUTTON_EVENT_CODE(event);

  if ((event & BUTTON_EVENT_LONG_FLAG) != 0U) {
    return TRAINING_SELECT_NONE;
  }
  if ((code == BUTTON_EVENT_PREV) || (code == BUTTON_EVENT_NEXT)) {
    /* Bit 0 tells next from prev; a bool from a compare would need a bit of internal RAM in SDCC. */
    if (training_select_step((bool)(code & 1U))) {
      training_screen_draw_select();
    }
    return TRAINING_SELECT_NONE;
  }
  /* A whole-event compare also rejects a repeat. */
  if (event == BUTTON_EVENT_ENTER) {
    return TRAINING_SELECT_START;
  }
  if (event == BUTTON_EVENT_MENU) {
    return TRAINING_SELECT_LEAVE;
  }
  return TRAINING_SELECT_NONE;
}
