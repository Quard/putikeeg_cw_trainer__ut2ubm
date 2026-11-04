/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "modes/free_training.h"

#include <stdbool.h>
#include <stdint.h>

#include "buttons/buttons.h"
#include "callsign/callsign.h"
#include "gfx/gfx.h"
#include "koch/koch.h"
#include "lcd/lcd_fb.h"
#include "mcu/compiler.h"
#include "modes/receiving.h"
#include "modes/sending.h"
#include "modes/training.h"
#include "progress/progress.h"
#include "settings/settings.h"
#include "training_screen_internal.h"

/* The Mode value goes to progress_level() as it is, with the speed of its own mode (FT-3). */
#if (TRAINING_MODE_RECEIVING != PROGRESS_MODE_RECEIVING) || (TRAINING_MODE_SENDING != PROGRESS_MODE_SENDING)
#error "The training modes must equal the progress modes"
#endif
#define FT_SPEED_ID(mode) ((settings_id_t)(SETTINGS_ID_RX_WPM + (uint8_t)(2U * (mode))))
_Static_assert((FT_SPEED_ID(TRAINING_MODE_RECEIVING) == SETTINGS_ID_RX_WPM) &&
                   (FT_SPEED_ID(TRAINING_MODE_SENDING) == SETTINGS_ID_TX_WPM),
               "each mode must read its own speed");

/* Configuration screen of ui-ux.md section 5.21.2. Row r of Mode or Chars is focus place r - 1. */
#define FT_FOCUS_MODE 0U
#define FT_FOCUS_CHARS 1U
#define FT_FOCUS_START 2U
#define FT_FLAG_EDIT 0x01U
#define FT_FLAG_MESSAGE 0x02U
#define FT_TITLE_ROW 0U
#define FT_MODE_ROW 1U
#define FT_CHARS_ROW 2U
#define FT_MESSAGE_ROW 4U
#define FT_MESSAGE_LAST_ROW 5U
#define FT_MODE_FIELD_WIDTH 2U
#define FT_CHARS_FIELD_WIDTH 14U
/* Section 4.3: a value ends in column 22, '<' is in column 21 - fw, and '>' is in column 24. */
#define FT_VALUE_END_COLUMN 23U
#define FT_MARKER_COLUMN 21U
#define FT_LAST_COLUMN 24U
#define FT_GRID_X 2U
#define FT_ROW_HEIGHT GFX_FONT_5X8_HEIGHT
#define FT_BAND_HEIGHT 7

static const MEM_CODE char ft_text_title[] = "Callsigns";
static const MEM_CODE char ft_text_mode[] = "Mode";
static const MEM_CODE char ft_text_chars[] = "Chars";
static const MEM_CODE char ft_text_rx[] = "RX";
static const MEM_CODE char ft_text_tx[] = "TX";
static const MEM_CODE char ft_text_learned[] = "Learned L";
static const MEM_CODE char ft_text_full[] = "Full";
static const MEM_CODE char ft_text_start[] = "Start";
/*
 * Centered rows of 25 cells: column (25 - n) / 2 (ui-ux.md 5.21.2, 5.21.3). Every cell is written, so a redraw
 * changes no byte. UX-83: L25 is the first shown level whose learned characters hold the digit 0 (D-T67).
 */
static const MEM_CODE char ft_row_start[] = "          Start          ";
static const MEM_CODE char ft_row_no_digit[] = "  No digit learned yet   ";
static const MEM_CODE char ft_row_way_out[] = "  Use Full or reach L25  ";

/* Mode and Chars stay until a power-off (D-T65). The start-up value 0 gives RX and Learned. */
_Static_assert((TRAINING_MODE_RECEIVING == 0U) && (TRAINING_MODE_SENDING == 1U), "the Mode value is its value index");
static MEM_XDATA uint8_t free_training_mode;
static MEM_XDATA bool free_training_is_full;
/* Screen state (FT-1): the focus place and the FT_FLAG_* bits. */
static MEM_XDATA uint8_t ft_focus;
static MEM_XDATA uint8_t ft_flags;
/* Work values in xdata, so the calls below need no internal RAM to keep them. */
static MEM_XDATA uint8_t ft_row; /* the row that a draw helper draws */
/* The character count of a start action, the new focus of a focus step, or the band column of ft_draw_row(). */
static MEM_XDATA uint8_t ft_work;
#define FT_NO_BAND 0xFFU

static bool ft_is_receiving(void) { return free_training_mode == TRAINING_MODE_RECEIVING; }

/* The value index of the focus place: 0 for the first value (RX, Learned), 1 for the second (TX, Full). */
static uint8_t ft_value(void) {
  return (ft_focus == FT_FOCUS_MODE) ? free_training_mode : (uint8_t)free_training_is_full;
}

/* The earned level of the Mode at the speed of that mode, from the progress image in RAM (UX-82). */
static uint16_t ft_level(void) {
  return progress_level(free_training_mode, (uint8_t)settings_get(FT_SPEED_ID(free_training_mode)));
}

/* FT-3: Learned is capped at the 40 word characters, so no prosign is in the set. */
static uint8_t ft_char_count(void) {
  uint8_t count = KOCH_WORD_ALPHABET_MAX;

  if (!free_training_is_full) {
    count = koch_learned_count(ft_level());
    if (count > KOCH_WORD_ALPHABET_MAX) {
      count = KOCH_WORD_ALPHABET_MAX;
    }
  }
  return count;
}

/*
 * Moves the value in training_screen_text to the end at column 22 in a row of 25 cells and blanks the other cells.
 * Every cell of the row is then written, so a redraw of the same row changes no byte. The copy runs from the right,
 * because the value only moves right.
 */
static void ft_place_value(void) {
  uint8_t length = 0U;
  uint8_t column;
  uint8_t index = TRAINING_SCREEN_COLUMNS;
  char character;

  while (training_screen_text[length] != '\0') {
    ++length;
  }
  column = (uint8_t)(FT_VALUE_END_COLUMN - length);
  training_screen_text[TRAINING_SCREEN_COLUMNS] = '\0';
  do {
    --index;
    character = ' ';
    if ((index >= column) && ((uint8_t)(index - column) < length)) {
      character = training_screen_text[(uint8_t)(index - column)];
    }
    training_screen_text[index] = character;
  } while (index != 0U);
}

/* Writes @p label over the first cells of the row in training_screen_text. */
static void ft_put_label(const char MEM_CODE* label) {
  uint8_t index;

  for (index = 0U; label[index] != '\0'; ++index) {
    training_screen_text[index] = label[index];
  }
}

/*
 * The band of the row ft_row from the band column ft_work to x 127: LCD_PIXEL_INVERT on y 8r+1 to 8r+7 turns the
 * normal text into inverted text on the band, and LCD_PIXEL_OFF clears the gap line y 8r (ui-ux.md UX-2). Column 0
 * gives x 0; an edit band starts one pixel before the cell of '<'.
 */
static void ft_fill_band(uint8_t color) {
  uint8_t x = 0U;
  uint8_t y = (uint8_t)(ft_row * FT_ROW_HEIGHT);
  uint8_t height = 1U;

  if (ft_work != 0U) {
    x = (uint8_t)((uint8_t)(ft_work * GFX_FONT_5X8_WIDTH) + (FT_GRID_X - 1U));
  }
  if (color == LCD_PIXEL_INVERT) {
    ++y;
    height = FT_BAND_HEIGHT;
  }
  gfx_fill_rect((int16_t)x, (int16_t)y, (int16_t)(uint8_t)(LCD_WIDTH - x), (int16_t)height, color);
}

/*
 * D-T92: the row ft_row (Mode or Chars) as a list row of ui-ux.md 5.2, or with a value in edit mode of 5.4. A row
 * without the focus writes each cell once with its final pixels, so its redraw changes no byte; ft_move_focus()
 * clears the margins of a row that loses the focus. A row with the focus gets the band: x 0-127 in browse mode,
 * from the cell before '<' in edit mode.
 */
static void ft_draw_row(void) {
  training_screen_compose();
  if (ft_row == FT_MODE_ROW) {
    training_screen_append(ft_is_receiving() ? ft_text_rx : ft_text_tx);
  } else if (free_training_is_full) {
    training_screen_append(ft_text_full);
  } else {
    training_screen_append(ft_text_learned);
    training_screen_append_u16(koch_shown_level(ft_level()));
  }
  ft_place_value();
  ft_put_label((ft_row == FT_MODE_ROW) ? ft_text_mode : ft_text_chars);
  ft_work = FT_NO_BAND;
  if ((uint8_t)(ft_focus + 1U) == ft_row) {
    ft_work = 0U;
    if ((ft_flags & FT_FLAG_EDIT) != 0U) {
      ft_work = (uint8_t)(FT_MARKER_COLUMN - ((ft_row == FT_MODE_ROW) ? FT_MODE_FIELD_WIDTH : FT_CHARS_FIELD_WIDTH));
      /* Two values each: '<' shows on the second value, '>' on the first (UX-4). */
      if (ft_value() != 0U) {
        training_screen_text[ft_work] = '<';
      } else {
        training_screen_text[FT_LAST_COLUMN] = '>';
      }
    }
    /* A row with the focus changes at each redraw, so a clear costs no extra dirty page. */
    training_screen_clear_rows(ft_row, ft_row);
  }
  training_screen_draw_text(ft_row, 0U, training_screen_text);
  if (ft_work != FT_NO_BAND) {
    ft_fill_band(LCD_PIXEL_INVERT);
    ft_fill_band(LCD_PIXEL_OFF);
  }
}

/* The title bar of ui-ux.md 5.1. The title never changes, so only an opening draws it, after a clear. */
static void ft_draw_title(void) {
  gfx_fill_rect(0, 0, (int16_t)LCD_WIDTH, (int16_t)FT_ROW_HEIGHT, LCD_PIXEL_ON);
  (void)gfx_draw_text((int16_t)FT_GRID_X, 0, ft_text_title, GFX_TEXT_INVERTED);
}

/* ui-ux.md 5.21.3. */
static void ft_draw_message(void) {
  training_screen_draw_text(FT_MESSAGE_ROW, 0U, ft_row_no_digit);
  training_screen_draw_text(FT_MESSAGE_LAST_ROW, 0U, ft_row_way_out);
}

/* The Start band of the training start screen with the focus (D-T91), else plain text. */
static void ft_draw_start(void) {
  if (ft_focus == FT_FOCUS_START) {
    training_screen_draw_band(ft_text_start);
  } else {
    training_screen_draw_text(TRAINING_SCREEN_LAST_ROW, 0U, ft_row_start);
  }
}

/*
 * UX-87: rows 1-7 of the configuration screen. Only the pages whose bytes change get dirty: every part without the
 * focus writes its final pixels once, and the part with the focus changes at each step that redraws it.
 */
static void ft_draw_screen(void) {
  ft_row = FT_MODE_ROW;
  ft_draw_row();
  ft_row = FT_CHARS_ROW;
  ft_draw_row();
  if ((ft_flags & FT_FLAG_MESSAGE) != 0U) {
    ft_draw_message();
  } else {
    training_screen_clear_rows(FT_MESSAGE_ROW, FT_MESSAGE_LAST_ROW);
  }
  ft_draw_start();
}

/* FT-1, C-7: each opening has the focus on Start, no edit mode and no message. */
static void ft_open(void) {
  ft_focus = FT_FOCUS_START;
  ft_flags = 0U;
}

/* An opening draws on blank rows: the clear of menu_render() or of the end screen enter (FT-11). */
static void ft_draw_opening(void) {
  ft_draw_title();
  ft_draw_screen();
}

/* FT-4: a session starts, or the message of UX-83 shows. A blocked start with the message shown redraws nothing. */
static void ft_start(void) {
  ft_work = ft_char_count();
  if (!callsign_is_possible(ft_work)) {
    if ((ft_flags & FT_FLAG_MESSAGE) == 0U) {
      ft_flags |= FT_FLAG_MESSAGE;
      ft_draw_message();
    }
    return;
  }
  /* The entry seeds the RNG with the tick of this press, so no rng_mix() follows. */
  if (ft_is_receiving()) {
    receiving_start_free(ft_work);
  } else {
    sending_start_free(ft_work);
  }
  training_screen_clear_rows(FT_TITLE_ROW, TRAINING_SCREEN_LAST_ROW);
  free_training_render();
}

/*
 * A press wraps; a repeat stops at Mode or Start (UX-80, UX-13). The row that loses the focus changes anyway, so its
 * clear takes the band and its margins away with no extra dirty page.
 */
static void ft_move_focus(button_event_t event) {
  ft_work = (uint8_t)(ft_focus + ((BUTTON_EVENT_CODE(event) == BUTTON_EVENT_NEXT) ? 1U : 0xFFU));
  if (ft_work > FT_FOCUS_START) {
    if ((event & BUTTON_EVENT_REPEAT_FLAG) != 0U) {
      return;
    }
    ft_work = (BUTTON_EVENT_CODE(event) == BUTTON_EVENT_NEXT) ? FT_FOCUS_MODE : FT_FOCUS_START;
  }
  if (ft_focus != FT_FOCUS_START) {
    training_screen_clear_rows((uint8_t)(ft_focus + 1U), (uint8_t)(ft_focus + 1U));
  }
  ft_focus = ft_work;
  ft_draw_screen();
}

/* Edit mode (UX-81): prev gives the first value, next the second; a step at a limit does nothing. */
static void ft_step_value(uint8_t code) {
  ft_work = (code == BUTTON_EVENT_NEXT) ? 1U : 0U;
  if (ft_value() == ft_work) {
    return;
  }
  if (ft_focus == FT_FOCUS_MODE) {
    free_training_mode = ft_work;
  } else {
    free_training_is_full = (bool)ft_work;
  }
  /* The reason of the message can change with the value (UX-83). */
  ft_flags &= (uint8_t)~FT_FLAG_MESSAGE;
  ft_draw_screen();
}

/* The configuration screen (UX-80, UX-81). Every event acts at the press; long events do nothing. */
static bool ft_handle_config(button_event_t event) {
  const uint8_t code = BUTTON_EVENT_CODE(event);

  if ((event & BUTTON_EVENT_LONG_FLAG) != 0U) {
    return false;
  }
  if ((ft_flags & FT_FLAG_EDIT) != 0U) {
    if ((code == BUTTON_EVENT_PREV) || (code == BUTTON_EVENT_NEXT)) {
      ft_step_value(code);
    } else {
      ft_flags &= (uint8_t)~FT_FLAG_EDIT;
      ft_draw_screen();
    }
    return false;
  }
  if ((code == BUTTON_EVENT_PREV) || (code == BUTTON_EVENT_NEXT)) {
    ft_move_focus(event);
  } else if (code == BUTTON_EVENT_MENU) {
    return true;
  } else if (ft_focus == FT_FOCUS_START) {
    ft_start();
  } else {
    ft_flags |= FT_FLAG_EDIT;
    ft_draw_screen();
  }
  return false;
}

void free_training_enter(void) {
  training_stop();
  ft_open();
}

void free_training_render(void) {
  const training_phase_t phase = training_phase();

  if (phase == TRAINING_PHASE_IDLE) {
    ft_draw_opening();
  } else if (ft_is_receiving()) {
    receiving_render();
  } else {
    sending_render();
  }
}

void free_training_poll(void) {
  if (training_phase() == TRAINING_PHASE_IDLE) {
    return;
  }
  if (ft_is_receiving()) {
    receiving_poll();
  } else {
    sending_poll();
  }
}

/*
 * FT-5, FT-6. The end screen handles its events here and never passes them to the session code. A whole-event
 * compare also rejects a repeat.
 */
bool free_training_handle(button_event_t event) {
  const training_phase_t phase = training_phase();

  if (phase == TRAINING_PHASE_IDLE) {
    return ft_handle_config(event);
  }
  if (phase == TRAINING_PHASE_END) {
    if (event == BUTTON_EVENT_MENU) {
      return true;
    }
    if (event == BUTTON_EVENT_ENTER) {
      free_training_leave();
      ft_open();
      training_screen_clear_rows(FT_TITLE_ROW, TRAINING_SCREEN_LAST_ROW);
      ft_draw_opening();
    }
    return false;
  }
  if (ft_is_receiving()) {
    return receiving_handle(event);
  }
  return sending_handle(event);
}

void free_training_leave(void) {
  if (training_phase() == TRAINING_PHASE_IDLE) {
    return;
  }
  if (ft_is_receiving()) {
    receiving_leave();
  } else {
    sending_leave();
  }
}
