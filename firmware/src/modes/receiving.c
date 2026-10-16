/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "modes/receiving.h"

#include <stdbool.h>
#include <stdint.h>

#include "buttons/buttons.h"
#include "cw/morse.h"
#include "cw/player.h"
#include "gfx/gfx.h"
#include "koch/keyboard.h"
#include "lcd/lcd_fb.h"
#include "mcu/compiler.h"
#include "modes/training.h"
#include "modes/training_screen.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"
#include "tick/tick.h"
#include "training_screen_internal.h"
#include "util/rng.h"

/* Layout of ui-ux.md sections 5.11 and 5.12.1 (UX-51, D-T29). */
#define RX_KEY_BAR_Y 55
#define RX_LABEL_ROW 7U
#define RX_LABEL_Y 56
#define RX_FIELD_CELLS 5U       /* menu/back, prev and next */
#define RX_ENTER_FIELD_CELLS 8U /* enter: only its run can hold two prosigns */
/* A list can pass the field by one short text before the range replaces it; morse_format() then needs its size. */
#define RX_LABEL_SIZE (RX_ENTER_FIELD_CELLS + MORSE_TEXT_SIZE + 1U)
_Static_assert(RX_LABEL_SIZE <= sizeof(training_screen_text), "the label is built in training_screen_text");
/* No text is composed while a label is built. */
#define rx_label training_screen_text
#define RX_RANGE_MARK '-'

/* Button to run, left to right (D-T27): MENU 0, PREV 1, NEXT 2, ENTER 3. ENTER gives (3 - 4) & 3 = 3. */
#define RX_GROUP_OF(code) ((uint8_t)((uint8_t)(BUTTON_EVENT_MENU - (code)) & (KEYBOARD_GROUP_COUNT - 1U)))
_Static_assert((RX_GROUP_OF(BUTTON_EVENT_MENU) == KEYBOARD_GROUP_MENU) &&
                   (RX_GROUP_OF(BUTTON_EVENT_PREV) == KEYBOARD_GROUP_PREV) &&
                   (RX_GROUP_OF(BUTTON_EVENT_NEXT) == KEYBOARD_GROUP_NEXT) &&
                   (RX_GROUP_OF(BUTTON_EVENT_ENTER) == KEYBOARD_GROUP_ENTER),
               "RX_GROUP_OF() must map each button to the run under it");

/* Left pixel x of the label field and the key bar of each run: x 2-26, 30-54, 58-82 and 87-126. */
static const MEM_CODE uint8_t rx_field_x[KEYBOARD_GROUP_COUNT] = {2U, 30U, 58U, 87U};

/* The enter label of the answer state (UX-64). */
static const MEM_CODE char rx_text_next[] = "Next";
static const MEM_CODE char rx_text_finish[] = "Finish";

/* Settings read once at the mode start (UX-49). */
static MEM_XDATA uint8_t rx_wpm;
static MEM_XDATA uint8_t rx_eff_wpm;
static MEM_XDATA uint8_t rx_max_word_len;
static MEM_XDATA uint8_t rx_prosign_style;

static MEM_XDATA uint8_t rx_label_length;
/* Work values in xdata, so the calls in the loops need no internal RAM to keep them. */
static MEM_XDATA uint8_t rx_group;
static MEM_XDATA uint8_t rx_index;
static MEM_XDATA uint8_t rx_count;
static MEM_XDATA uint8_t rx_code;
static MEM_XDATA bool rx_is_long;
static MEM_XDATA bool rx_is_block;

static void rx_play_item(void) { (void)player_start(training_item_text(), training_item_length(), rx_wpm, rx_eff_wpm); }

/* The answer state of D-T44: the answer after the third wrong attempt shows until an enter tap. */
static bool rx_is_answer_shown(void) {
  return (training_phase() == TRAINING_PHASE_RESULT) && (training_last_check() == TRAINING_CHECK_FAILED);
}

/* Tap mode is on only during a session (D-T39); the switch drops the press that starts it. */
static void rx_start_session(void) {
  buttons_set_tap_mode(true);
  training_start();
  keyboard_reset(training_alphabet_count());
}

/* In Prosign and Both, a prosign has the short text <BT>, <SK> or <AR> in a list or a range. */
static uint8_t rx_short_style(void) {
  return (rx_prosign_style == MORSE_STYLE_PUNCTUATION) ? MORSE_STYLE_PUNCTUATION : MORSE_STYLE_NAME;
}

/* Appends the short text of list character @p index to rx_label. */
static void rx_append_short(uint8_t index) {
  rx_label_length =
      (uint8_t)(rx_label_length + morse_format(keyboard_char(index), rx_short_style(), &rx_label[rx_label_length]));
}

static uint8_t rx_field_cells(void) {
  return (rx_group == KEYBOARD_GROUP_ENTER) ? RX_ENTER_FIELD_CELLS : RX_FIELD_CELLS;
}

/*
 * ui-ux.md section 5.12.1: blank, a list of short texts (one character is a list of one), or a range. The answer state
 * has only the enter label (UX-64).
 */
static void rx_build_label(void) {
  uint8_t size;
  uint8_t field;
  uint8_t first;

  rx_label_length = 0U;
  rx_label[0] = '\0';
  if (rx_is_answer_shown()) {
    if (rx_group == KEYBOARD_GROUP_ENTER) {
      training_screen_compose();
      if (training_item_number() == training_item_count()) {
        training_screen_append(rx_text_finish);
        rx_label_length = (uint8_t)(sizeof(rx_text_finish) - 1U);
      } else {
        training_screen_append(rx_text_next);
        rx_label_length = (uint8_t)(sizeof(rx_text_next) - 1U);
      }
    }
    return;
  }
  size = keyboard_group_size(rx_group);
  field = rx_field_cells();
  first = keyboard_group_first(rx_group);
  for (rx_index = 0U; (rx_index < size) && (rx_label_length <= field); ++rx_index) {
    rx_append_short((uint8_t)(first + rx_index));
  }
  if (rx_label_length > field) {
    rx_label_length = 0U;
    rx_append_short(first);
    rx_label[rx_label_length] = RX_RANGE_MARK;
    ++rx_label_length;
    rx_append_short((uint8_t)(first + size - 1U));
  }
}

/*
 * Writes row 7 only (LC-7). The free pixels of a field are 5 * (cells - n): menu/back puts none on the left, prev and
 * next half (centered, rounded down), enter all (right-aligned, x = 127 - 5n). The D1 check proves n <= cells.
 */
static void rx_draw_labels(void) {
  training_screen_clear_rows(RX_LABEL_ROW, RX_LABEL_ROW);
  for (rx_group = KEYBOARD_GROUP_MENU; rx_group < KEYBOARD_GROUP_COUNT; ++rx_group) {
    rx_build_label();
    rx_index = (uint8_t)((uint8_t)(rx_field_cells() - rx_label_length) * GFX_FONT_5X8_WIDTH);
    if (rx_group == KEYBOARD_GROUP_MENU) {
      rx_index = 0U;
    } else if (rx_group != KEYBOARD_GROUP_ENTER) {
      rx_index = (uint8_t)(rx_index / 2U);
    }
    (void)gfx_draw_text((int16_t)(uint8_t)(rx_field_x[rx_group] + rx_index), RX_LABEL_Y, rx_label, GFX_TEXT_NORMAL);
  }
}

/* One segment above each label field, in a full render only (D-T29). */
static void rx_draw_key_bars(void) {
  for (rx_group = KEYBOARD_GROUP_MENU; rx_group < KEYBOARD_GROUP_COUNT; ++rx_group) {
    gfx_fill_rect((int16_t)rx_field_x[rx_group], RX_KEY_BAR_Y,
                  (int16_t)(uint8_t)(rx_field_cells() * GFX_FONT_5X8_WIDTH), 1, LCD_PIXEL_ON);
  }
}

/* ITEM: the picks, the block and the bars. RESULT: the picks, or the answer after the third wrong attempt. */
static void rx_draw_item_line(void) {
  const training_phase_t phase = training_phase();

  if (rx_is_answer_shown()) {
    training_screen_draw_item_text(training_item_text(), rx_prosign_style);
    return;
  }
  /* The answer is empty outside ITEM and RESULT. */
  training_screen_draw_item_text(training_answer_text(), rx_prosign_style);
  if (phase == TRAINING_PHASE_ITEM) {
    rx_count = training_item_length();
    rx_is_block = true;
    for (rx_index = training_answer_length(); rx_index < rx_count; ++rx_index) {
      training_screen_draw_mark(rx_index, rx_count, rx_is_block);
      rx_is_block = false;
    }
  }
}

static void rx_draw_input(void) {
  training_screen_draw_status(rx_prosign_style);
  rx_draw_item_line();
  rx_draw_labels();
}

void receiving_enter(void) {
  rng_seed(tick_now());
  rx_wpm = (uint8_t)settings_get(SETTINGS_ID_RX_WPM);
  rx_eff_wpm = (uint8_t)settings_get(SETTINGS_ID_RX_EFF_WPM);
  rx_max_word_len = (uint8_t)settings_get(SETTINGS_ID_MAX_WORD_LEN);
  rx_prosign_style = (uint8_t)settings_get(SETTINGS_ID_PROSIGN_STYLE);
  sidetone_set_tone_hz(settings_get(SETTINGS_ID_TONE_HZ));
  training_prepare(TRAINING_MODE_RECEIVING, rx_wpm, rx_max_word_len);
}

/* Rows 1-7 of a session. */
static void rx_draw_session(void) {
  rx_draw_input();
  rx_draw_key_bars();
}

void receiving_render(void) {
  const training_phase_t phase = training_phase();

  if (phase == TRAINING_PHASE_SELECT) {
    training_screen_draw_start();
  } else if (phase == TRAINING_PHASE_END) {
    training_screen_draw_end();
  } else {
    rx_draw_session();
  }
  training_screen_draw_header();
}

void receiving_poll(void) {
  const training_event_t event = training_poll();

  if ((event == TRAINING_EVENT_ITEM) || (event == TRAINING_EVENT_RETRY)) {
    keyboard_top();
    rx_draw_input();
    rx_play_item();
  } else if (event == TRAINING_EVENT_GAP) {
    training_screen_clear_rows(TRAINING_SCREEN_STATUS_ROW, TRAINING_SCREEN_ITEM_LAST_ROW + 1U);
    training_screen_draw_header();
    /* Row 7 can hold the label of the answer state (D-T44). */
    rx_draw_labels();
  } else if (event == TRAINING_EVENT_END) {
    buttons_set_tap_mode(false);
    training_screen_draw_header();
    training_screen_draw_end();
  }
}

/* A pick goes to the engine; a check stops the playback (ui-ux.md section 5.12.2). */
static void rx_pick(char character) {
  const training_check_t check = training_add_char(character);

  if (check != TRAINING_CHECK_PARTIAL) {
    player_stop();
    if (check == TRAINING_CHECK_FAILED) {
      /* The answer plays once more and stays until an enter tap (D-T44). */
      rx_play_item();
      training_hold_result(true);
    }
  }
  rx_draw_input();
}

/* Applies rx_code and rx_is_long in phase ITEM. Returns true when the event acted. */
static bool rx_handle_item(void) {
  char picked;

  if (rx_is_long) {
    if (rx_code == BUTTON_EVENT_ENTER) {
      rx_play_item();
      return true;
    }
    if (rx_code != BUTTON_EVENT_PREV) {
      return false;
    }
    if (keyboard_back()) {
      rx_draw_labels();
    } else if (training_undo_char()) {
      rx_draw_item_line();
    } else {
      return false;
    }
    return true;
  }
  rx_group = RX_GROUP_OF(rx_code);
  if (keyboard_group_size(rx_group) == 0U) {
    return false;
  }
  picked = keyboard_select(rx_group);
  if (picked == KEYBOARD_NO_PICK) {
    rx_draw_labels();
  } else {
    rx_pick(picked);
  }
  return true;
}

/* Applies rx_code and rx_is_long in the answer state (D-T44). Returns true when the event acted. */
static bool rx_handle_answer(void) {
  if (rx_code != BUTTON_EVENT_ENTER) {
    return false;
  }
  if (rx_is_long) {
    rx_play_item();
  } else {
    player_stop();
    training_hold_result(false);
  }
  return true;
}

bool receiving_handle(button_event_t event) {
  const training_phase_t phase = training_phase();
  training_select_action_t action;

  if ((phase == TRAINING_PHASE_SELECT) || (phase == TRAINING_PHASE_END)) {
    action = training_screen_select_input(event);
    if (action == TRAINING_SELECT_LEAVE) {
      return true;
    }
    if (action == TRAINING_SELECT_START) {
      rng_mix(tick_now());
      rx_start_session();
      training_screen_clear_rows(TRAINING_SCREEN_STATUS_ROW, TRAINING_SCREEN_LAST_ROW);
      /* The start screen header already shows the start pause of the selected level (D-T42 (4)). */
      rx_draw_session();
    }
    return false;
  }
  if ((event & BUTTON_EVENT_REPEAT_FLAG) != 0U) {
    return false;
  }
  rx_code = BUTTON_EVENT_CODE(event);
  rx_is_long = false;
  if ((event & BUTTON_EVENT_LONG_FLAG) != 0U) {
    rx_is_long = true;
  }
  if (((phase == TRAINING_PHASE_ITEM) && rx_handle_item()) || (rx_is_answer_shown() && rx_handle_answer())) {
    rng_mix(tick_now());
  }
  return false;
}

void receiving_leave(void) {
  player_stop();
  training_stop();
  buttons_set_tap_mode(false);
}
