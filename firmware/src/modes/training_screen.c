/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

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
#define SCREEN_LABEL_COLUMN 8U  /* "New char " and "New chars": 9 cells, centered */
#define SCREEN_MARK_WIDTH 7
#define SCREEN_BLOCK_Y 26
#define SCREEN_BLOCK_HEIGHT 14
#define SCREEN_BAR_Y 38
#define SCREEN_BAR_HEIGHT 2
#define SCREEN_END_SAVE_ROW 1U
#define SCREEN_END_SCORE_ROW 2U
#define SCREEN_END_RESULT_ROW 3U
#define SCREEN_SELECTOR_ROW 1U
#define SCREEN_LABEL_ROW 2U
#define SCREEN_BLANK_ROW 6U
#define SCREEN_BAND_Y 57 /* y 56, the gap line, stays off */
#define SCREEN_BAND_HEIGHT 7
/* Code marks of ui-ux.md section 5.13.3: dits, dahs and mark gaps on y = 8 row + 3 and 8 row + 4. */
#define SCREEN_CODE_Y_OFFSET 3U
#define SCREEN_CODE_MARK_HEIGHT 2
#define SCREEN_CODE_DIT_WIDTH 2U
#define SCREEN_CODE_DAH_WIDTH 6U
#define SCREEN_CODE_MARK_GAP 3U
#define SCREEN_CODE_CHAR_GAP 9U /* 3 mark gaps, as 3 units between CW characters (ui-ux.md UX-67) */
#define SCREEN_NUMBER_SIZE ((2U * GFX_U16_TEXT_SIZE) + 1U) /* "100/100" and room for two numbers */
_Static_assert(SCREEN_NUMBER_SIZE >= MORSE_TEXT_SIZE, "the number buffer also holds the text of one character");

MEM_XDATA char training_screen_text[TRAINING_SCREEN_COLUMNS + 1U];
static MEM_XDATA uint8_t screen_length;
/* Digits for an append, or the text of one character: never both at once. */
static MEM_XDATA char screen_number[SCREEN_NUMBER_SIZE];
#define screen_glyph_text screen_number
/*
 * Cells of the big line that the start screen drew last. The start screen draws it only over a big line of the same
 * width or over blank rows, so it clears rows 3 and 4 only when the width changes (D-T49).
 */
static MEM_XDATA uint8_t screen_big_length;
/* A pixel x of the code group, or the item count of the header: one xdata work value, never both at once. */
static MEM_XDATA uint8_t screen_x;
/* Sending: a word space inside a word ended the attempt (UX-52, D-T31). */
static const MEM_CODE char screen_pause_text[] = "pause too long";
/* Label of a character level; level 0 draws 's' in its blank last cell (ui-ux.md 5.16.3). */
static const MEM_CODE char screen_new_char_text[] = "New char ";

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

/* Selects FONTS_ID_8X16, draws @p text in normal video from pixel (@p x, 24), then selects FONTS_ID_5X8 again. */
static void screen_draw_big_text(const char MEM_XDATA* text, MEM_XDATA int16_t x) {
  (void)gfx_set_font(FONTS_ID_8X16);
  (void)gfx_draw_text(x, SCREEN_ITEM_Y, text, GFX_TEXT_NORMAL);
  (void)gfx_set_font(FONTS_ID_5X8);
}

void training_screen_draw_item_text(const char MEM_XDATA* text, MEM_XDATA uint8_t style) {
  const uint8_t count = training_item_length();
  uint8_t slot;
  uint8_t length;
  int16_t x;

  training_screen_clear_rows(TRAINING_SCREEN_ITEM_FIRST_ROW, TRAINING_SCREEN_ITEM_LAST_ROW);
  for (slot = 0U; text[slot] != '\0'; ++slot) {
    length = morse_format(text[slot], style, screen_glyph_text);
    if (count == 1U) {
      x = (int16_t)(SCREEN_CHAR_CENTER_X - (int16_t)(4U * length));
    } else {
      x = screen_slot_x(slot, count);
    }
    screen_draw_big_text(screen_glyph_text, x);
  }
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

uint8_t training_screen_draw_code_mark(uint8_t x, MEM_XDATA uint8_t row, MEM_XDATA bool is_dah) {
  const uint8_t width = is_dah ? SCREEN_CODE_DAH_WIDTH : SCREEN_CODE_DIT_WIDTH;
  const uint8_t next_x = (uint8_t)(x + width + SCREEN_CODE_MARK_GAP);

  gfx_fill_rect((int16_t)x, (int16_t)((uint8_t)(row * SCREEN_ROW_HEIGHT) + SCREEN_CODE_Y_OFFSET), (int16_t)width,
                SCREEN_CODE_MARK_HEIGHT, LCD_PIXEL_ON);
  return next_x;
}

/* The pixel width of the marks of @p pattern, each with the mark gap after it. */
static uint8_t screen_code_width(uint8_t pattern) {
  uint8_t width = 0U;

  while (pattern > MORSE_PATTERN_EMPTY) {
    width = (uint8_t)(width + (((pattern & 1U) != 0U) ? SCREEN_CODE_DAH_WIDTH : SCREEN_CODE_DIT_WIDTH) +
                      SCREEN_CODE_MARK_GAP);
    pattern >>= 1U;
  }
  return width;
}

/* Draws the marks of @p pattern in the code group row from x screen_x, and moves screen_x past them and a mark gap. */
static void screen_draw_code(uint8_t pattern) {
  uint8_t shift = 0U;

  while ((uint8_t)(pattern >> shift) > MORSE_PATTERN_EMPTY) {
    ++shift;
  }
  /* The first element is the highest bit below the start marker. */
  while (shift != 0U) {
    --shift;
    /* A cast, not a compare: a bool from a compare needs a bit of internal RAM in SDCC. */
    screen_x =
        training_screen_draw_code_mark(screen_x, TRAINING_SCREEN_CODE_GROUP_ROW, (bool)((pattern >> shift) & 1U));
  }
}

/*
 * x = (128 - w) / 2 = (131 - the sum of the mark widths and gaps) / 2, because w has no gap after the last mark. An
 * empty second pattern has width 0 and draws nothing.
 */
void training_screen_draw_code_group(uint8_t first, MEM_XDATA uint8_t second) {
  uint8_t width = screen_code_width(second);

  if (width != 0U) {
    width = (uint8_t)(width + (SCREEN_CODE_CHAR_GAP - SCREEN_CODE_MARK_GAP));
  }
  width = (uint8_t)(width + screen_code_width(first));
  screen_x = (uint8_t)((uint8_t)((LCD_WIDTH + SCREEN_CODE_MARK_GAP) - width) / 2U);
  screen_draw_code(first);
  screen_x += SCREEN_CODE_CHAR_GAP - SCREEN_CODE_MARK_GAP;
  screen_draw_code(second);
}

/*
 * ui-ux.md section 5.11.1: tag, speeds from column 3, level from column 9, item right-aligned to column 24. In SELECT
 * the header is the one of the start pause at the selected level (SL-6), so a start does not redraw it.
 */
void training_screen_draw_header(void) {
  uint8_t length;

  training_screen_compose();
  training_screen_append((training_mode() == TRAINING_MODE_RECEIVING) ? "RX" : "TX");
  screen_pad(SCREEN_SPEED_COLUMN);
  training_screen_append_u16(training_wpm());
  training_screen_append("/");
  training_screen_append_u16(
      settings_get((training_mode() == TRAINING_MODE_RECEIVING) ? SETTINGS_ID_RX_EFF_WPM : SETTINGS_ID_TX_EFF_WPM));
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

/*
 * ui-ux.md sections 5.16.2 and 5.16.3 (SL-1, SL-2a to SL-2c): rows 1-5 for the selected level. The big line of m
 * cells starts at x = 64 - 4m. Only Sending shows the code row; level 0 shows the codes of K and M as one group.
 * Reads no session accessor (SL-8). The settings cannot change on a training screen.
 */
static void screen_draw_level(void) {
  screen_draw_selector();
  training_screen_clear_rows(TRAINING_SCREEN_CODE_GROUP_ROW, TRAINING_SCREEN_CODE_GROUP_ROW);
  training_screen_compose();
  if (koch_kind(training_selected_level()) == KOCH_KIND_WORD) {
    training_screen_clear_rows(SCREEN_LABEL_ROW, SCREEN_LABEL_ROW);
    training_screen_append("Words of ");
    training_screen_append_u16(
        koch_word_length(training_selected_level(), (uint8_t)settings_get(SETTINGS_ID_MAX_WORD_LEN)));
  } else {
    /* Both labels have 9 cells from column 8, so the opaque glyphs replace each other with no clear (D-T49). */
    training_screen_draw_text(SCREEN_LABEL_ROW, SCREEN_LABEL_COLUMN, screen_new_char_text);
    if (training_selected_level() == 0U) {
      training_screen_draw_text(SCREEN_LABEL_ROW, SCREEN_LABEL_COLUMN + 8U, "s");
      if (training_mode() == TRAINING_MODE_SENDING) {
        training_screen_draw_code_group(morse_encode(koch_char(0U)), morse_encode(koch_char(1U)));
      }
      training_screen_append("K M");
    } else {
      screen_glyph_text[0] = koch_char((uint8_t)(koch_learned_count(training_selected_level()) - 1U));
      if (training_mode() == TRAINING_MODE_SENDING) {
        training_screen_draw_code_group(morse_encode(screen_glyph_text[0]), MORSE_PATTERN_EMPTY);
      }
      (void)morse_format(screen_glyph_text[0], (uint8_t)settings_get(SETTINGS_ID_PROSIGN_STYLE), screen_glyph_text);
      training_screen_append(screen_glyph_text);
    }
  }
  /* At most 15 cells (SL-2b), so x = 64 - 4m stays in 4..64. */
  /* The glyphs are opaque boxes of a whole cell: a text of the same width rewrites only the bytes that change. */
  if (screen_length != screen_big_length) {
    screen_big_length = screen_length;
    training_screen_clear_rows(TRAINING_SCREEN_ITEM_FIRST_ROW, TRAINING_SCREEN_ITEM_LAST_ROW);
  }
  screen_draw_big_text(training_screen_text,
                       (int16_t)(uint8_t)((uint8_t)SCREEN_CHAR_CENTER_X - (uint8_t)(4U * screen_length)));
}

/*
 * ui-ux.md sections 5.15 and 5.16.1 (SL-5): @p label centered in row 7, inverted inside a band one cell wider on each
 * side, y 57-63. The gap line y 56 stays off across the band (ui-ux.md UX-2).
 */
static void screen_draw_band(const char* label) {
  uint8_t x;
  uint8_t width;

  training_screen_compose();
  training_screen_append(label);
  training_screen_draw_centered(TRAINING_SCREEN_LAST_ROW);
  /* Column c = (25 - n) / 2 from training_screen_draw_centered(); the band starts one cell before it. */
  x = (uint8_t)((uint8_t)(((TRAINING_SCREEN_COLUMNS - screen_length) / 2U) * GFX_FONT_5X8_WIDTH) -
                (GFX_FONT_5X8_WIDTH - SCREEN_GRID_X));
  width = (uint8_t)((uint8_t)(screen_length + 2U) * GFX_FONT_5X8_WIDTH);
  gfx_fill_rect((int16_t)x, SCREEN_BAND_Y, (int16_t)width, SCREEN_BAND_HEIGHT, LCD_PIXEL_INVERT);
  gfx_fill_rect((int16_t)x, (int16_t)(SCREEN_BAND_Y - 1), (int16_t)width, 1, LCD_PIXEL_OFF);
}

/* A step redraws rows 0-5; only the pages whose bytes change get dirty (SL-4, D-T49). */
void training_screen_draw_select(void) {
  training_screen_draw_header();
  screen_draw_level();
}

/* ui-ux.md section 5.16.1: the selector, the level rows, row 6 blank and the Start band. */
void training_screen_draw_start(void) {
  /* Rows 2-6 can hold the end screen; a step redraws rows 2-4 without a full clear. */
  training_screen_clear_rows(SCREEN_LABEL_ROW, SCREEN_BLANK_ROW);
  screen_draw_level();
  screen_draw_band("Start");
}

/* ui-ux.md section 5.15 (SL-7, D-T42 (3)). */
void training_screen_draw_end(void) {
  const training_outcome_t outcome = training_outcome();

  training_screen_clear_rows(TRAINING_SCREEN_STATUS_ROW, TRAINING_SCREEN_LAST_ROW);
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
  screen_draw_band("OK");
}

_Static_assert((TRAINING_OUTCOME_PASS_NOT_SAVED == (TRAINING_OUTCOME_PASS + 1U)),
               "the advancing passes must be one range");
_Static_assert(((BUTTON_EVENT_NEXT & 1U) != 0U) && ((BUTTON_EVENT_PREV & 1U) == 0U), "bit 0 must tell next from prev");

/*
 * ui-ux.md sections 5.15 and 5.16.4 (D-T38, D-T47). Tap mode is off, so a press acts at once; a long event never
 * acts. In END no step acts, and enter opens the start screen; so START comes only from SELECT.
 */
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
    if (training_reopen_select()) {
      training_screen_draw_header();
      training_screen_draw_start();
      return TRAINING_SELECT_NONE;
    }
    return TRAINING_SELECT_START;
  }
  if (event == BUTTON_EVENT_MENU) {
    return TRAINING_SELECT_LEAVE;
  }
  return TRAINING_SELECT_NONE;
}
