#include "modes/sending.h"

#include <stdbool.h>
#include <stdint.h>

#include "buttons/buttons.h"
#include "cw/keyer.h"
#include "cw/keyer_shared.h"
#include "cw/morse.h"
#include "gfx/gfx.h"
#include "lcd/lcd_fb.h"
#include "mcu/compiler.h"
#include "modes/training.h"
#include "modes/training_screen.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"
#include "tick/tick.h"
#include "training_screen_internal.h"
#include "util/rng.h"

/* Code rows of ui-ux.md section 5.13.3. */
#define TX_CODE_MAX_MARKS 12U
#define TX_CODE_ROW 7U
#define TX_ANSWER_ROW 5U
#define TX_CODE_X 2U
#define TX_MARK_Y_OFFSET 3U /* marks on y = 8r + 3 and 8r + 4 */
#define TX_MARK_HEIGHT 2
#define TX_DOT_WIDTH 2U
#define TX_DASH_WIDTH 6U
#define TX_MARK_GAP 3U
/* The queue holds KEYER_QUEUE_SIZE events, and an overflow adds one. */
#define TX_EVENTS_PER_POLL (KEYER_QUEUE_SIZE + 1U)

/* Settings read once at the mode start (UX-49). */
static MEM_XDATA uint8_t tx_key_type;
static MEM_XDATA bool tx_is_swapped;
static MEM_XDATA uint8_t tx_wpm;
static MEM_XDATA uint8_t tx_eff_wpm;
static MEM_XDATA uint8_t tx_max_word_len;
static MEM_XDATA bool tx_is_code_shown;
static MEM_XDATA uint8_t tx_prosign_style;

static MEM_XDATA uint8_t tx_pattern;
/* D-T15: drop every keyer event up to and including the next CHAR_GAP. */
static MEM_XDATA bool sending_is_dropping;
/* D-T31: the last event was a CHAR_GAP that gave PARTIAL, so a WORD_GAP now is a word space inside the word. */
static MEM_XDATA bool tx_is_word_gap_armed;
static MEM_XDATA uint8_t tx_mark_count;
static MEM_XDATA uint8_t tx_code_x;
static MEM_XDATA uint8_t tx_mark_x;
static MEM_XDATA uint8_t tx_shift;
static MEM_XDATA bool tx_is_dah;

static void tx_start_session(void) {
  buttons_set_enter_enabled(false);
  keyer_start(tx_key_type, tx_is_swapped, tx_wpm, tx_eff_wpm);
  sending_is_dropping = false;
  tx_is_word_gap_armed = false;
  tx_pattern = MORSE_PATTERN_EMPTY;
  training_start();
}

/* Draws one mark at tx_mark_x in row @p row and moves tx_mark_x past it and the gap. */
static void tx_draw_mark(uint8_t row, MEM_XDATA bool is_dah) {
  const uint8_t width = is_dah ? TX_DASH_WIDTH : TX_DOT_WIDTH;

  gfx_fill_rect((int16_t)tx_mark_x, (int16_t)((uint8_t)(row * GFX_FONT_5X8_HEIGHT) + TX_MARK_Y_OFFSET), (int16_t)width,
                TX_MARK_HEIGHT, LCD_PIXEL_ON);
  tx_mark_x = (uint8_t)(tx_mark_x + width + TX_MARK_GAP);
}

/* Row 5: the marks of @p pattern centered, x = (128 - w) / 2 (ui-ux.md section 5.13.3). */
static void tx_draw_answer_code(uint8_t pattern) {
  uint8_t width = 0U;

  tx_shift = 0U;
  while ((uint8_t)(pattern >> tx_shift) > MORSE_PATTERN_EMPTY) {
    width = (uint8_t)(width + ((((pattern >> tx_shift) & 1U) != 0U) ? TX_DASH_WIDTH : TX_DOT_WIDTH) + TX_MARK_GAP);
    ++tx_shift;
  }
  tx_mark_x = (uint8_t)((uint8_t)(LCD_WIDTH - (uint8_t)(width - TX_MARK_GAP)) / 2U);
  /* The first element is the highest bit below the start marker. */
  while (tx_shift != 0U) {
    --tx_shift;
    tx_is_dah = false;
    if (((pattern >> tx_shift) & 1U) != 0U) {
      tx_is_dah = true;
    }
    tx_draw_mark(TX_ANSWER_ROW, tx_is_dah);
  }
}

/* The item in the 8 x 16 font; in a word item, the bar under the character that the user keys next. */
static void tx_draw_item_line(void) {
  const training_phase_t phase = training_phase();
  const uint8_t length = training_item_length();
  uint8_t bar_slot = training_answer_length();

  if ((phase != TRAINING_PHASE_ITEM) && (phase != TRAINING_PHASE_RESULT)) {
    training_screen_clear_rows(TRAINING_SCREEN_ITEM_FIRST_ROW, TRAINING_SCREEN_ITEM_LAST_ROW);
    return;
  }
  training_screen_draw_item_text(training_item_text(), tx_prosign_style);
  if (phase == TRAINING_PHASE_RESULT) {
    if (training_last_check() == TRAINING_CHECK_CORRECT) {
      return;
    }
    /* A wrong character: the bar stays under it. */
    --bar_slot;
  }
  if (length > 1U) {
    training_screen_draw_mark(bar_slot, length, false);
  }
}

/* The status, and row 5: the expected code after the third wrong attempt (ui-ux.md section 5.14). */
static void tx_draw_input(void) {
  training_screen_draw_status(tx_prosign_style);
  training_screen_clear_rows(TX_ANSWER_ROW, TX_ANSWER_ROW);
  if ((training_phase() == TRAINING_PHASE_RESULT) && (training_last_check() == TRAINING_CHECK_FAILED)) {
    tx_draw_answer_code(morse_encode(training_item_text()[training_answer_length() - 1U]));
  }
  tx_draw_item_line();
}

/* DIT or DAH (tx_is_dah) in phase ITEM: the first element of a character clears row 7. */
static void tx_add_element(void) {
  rng_mix(tick_now());
  if (tx_pattern == MORSE_PATTERN_EMPTY) {
    training_screen_clear_rows(TX_CODE_ROW, TX_CODE_ROW);
    tx_mark_count = 0U;
    tx_code_x = TX_CODE_X;
  }
  tx_pattern = morse_add_element(tx_pattern, tx_is_dah);
  if (tx_is_code_shown && (tx_mark_count < TX_CODE_MAX_MARKS)) {
    tx_mark_x = tx_code_x;
    tx_draw_mark(TX_CODE_ROW, tx_is_dah);
    tx_code_x = tx_mark_x;
    ++tx_mark_count;
  }
}

/*
 * Section 12.3: the drop flag first, then the phase. Every event clears the armed flag. A WORD_GAP right after a
 * PARTIAL CHAR_GAP is a word space inside the word (D-T31). One check path keeps the call chain as deep as before.
 */
static void tx_handle_keyer_event(keyer_event_t event) {
  char character;

  if ((event == KEYER_EVENT_WORD_GAP) && tx_is_word_gap_armed && (training_phase() == TRAINING_PHASE_ITEM)) {
    character = TRAINING_CHAR_WORD_SPACE;
  } else {
    tx_is_word_gap_armed = false;
    if (sending_is_dropping) {
      if (event == KEYER_EVENT_CHAR_GAP) {
        sending_is_dropping = false;
      }
      return;
    }
    if ((event == KEYER_EVENT_DIT) || (event == KEYER_EVENT_DAH)) {
      if (training_phase() != TRAINING_PHASE_ITEM) {
        sending_is_dropping = true;
        return;
      }
      tx_is_dah = false;
      if (event == KEYER_EVENT_DAH) {
        tx_is_dah = true;
      }
      tx_add_element();
      return;
    }
    if (training_phase() != TRAINING_PHASE_ITEM) {
      return;
    }
    if (event == KEYER_EVENT_OVERFLOW) {
      /* D-T16: a lost event uses no attempt. */
      tx_pattern = MORSE_PATTERN_EMPTY;
      sending_is_dropping = true;
      return;
    }
    if ((event != KEYER_EVENT_CHAR_GAP) || (tx_pattern == MORSE_PATTERN_EMPTY)) {
      return;
    }
    character = morse_decode(tx_pattern);
    tx_pattern = MORSE_PATTERN_EMPTY;
  }
  tx_is_word_gap_armed = false;
  if (training_add_char(character) == TRAINING_CHECK_PARTIAL) {
    tx_is_word_gap_armed = true;
    tx_draw_item_line();
  } else {
    tx_draw_input();
  }
}

void sending_enter(void) {
  rng_seed(tick_now());
  tx_key_type = (uint8_t)settings_get(SETTINGS_ID_KEY_TYPE);
  /* Plain stores: a bool from a compare needs a bit of internal RAM in SDCC. */
  tx_is_swapped = false;
  if (settings_get(SETTINGS_ID_PADDLE_SWAP) != 0U) {
    tx_is_swapped = true;
  }
  tx_wpm = (uint8_t)settings_get(SETTINGS_ID_TX_WPM);
  tx_eff_wpm = (uint8_t)settings_get(SETTINGS_ID_TX_EFF_WPM);
  tx_max_word_len = (uint8_t)settings_get(SETTINGS_ID_MAX_WORD_LEN);
  tx_is_code_shown = false;
  if (settings_get(SETTINGS_ID_SHOW_CW_CODE) != 0U) {
    tx_is_code_shown = true;
  }
  tx_prosign_style = (uint8_t)settings_get(SETTINGS_ID_PROSIGN_STYLE);
  sidetone_set_tone_hz(settings_get(SETTINGS_ID_TONE_HZ));
  /* The keyer starts with the session, so P3.2 stays enter on the start screen (D-T40). */
  training_prepare(TRAINING_MODE_SENDING, tx_wpm, tx_max_word_len);
}

void sending_render(void) {
  const training_phase_t phase = training_phase();

  if (phase == TRAINING_PHASE_SELECT) {
    training_screen_draw_start();
  } else if (phase == TRAINING_PHASE_END) {
    training_screen_draw_end();
  } else {
    tx_draw_input();
  }
  training_screen_draw_header();
}

void sending_poll(void) {
  uint8_t count;
  keyer_event_t keyer_event;
  training_event_t event;

  for (count = 0U; count < TX_EVENTS_PER_POLL; ++count) {
    keyer_event = keyer_read_event();
    if (keyer_event == KEYER_EVENT_NONE) {
      break;
    }
    tx_handle_keyer_event(keyer_event);
  }
  event = training_poll();
  if ((event == TRAINING_EVENT_ITEM) || (event == TRAINING_EVENT_RETRY)) {
    /* D-T30: a retry looks like a new item. Row 6 is always off, so one clear covers rows 5 and 7. */
    training_screen_clear_rows(TX_ANSWER_ROW, TX_CODE_ROW);
    tx_draw_input();
  } else if (event == TRAINING_EVENT_GAP) {
    training_screen_clear_rows(TRAINING_SCREEN_STATUS_ROW, TRAINING_SCREEN_LAST_ROW);
    training_screen_draw_header();
  } else if (event == TRAINING_EVENT_END) {
    keyer_stop();
    buttons_set_enter_enabled(true);
    /* Drops a press held across END, so its repeats cannot step the selector (D-T43). */
    buttons_set_tap_mode(false);
    training_screen_draw_header();
    training_screen_draw_end();
  }
}

/* Only the start screen and the end screen act (ui-ux.md sections 5.16.4 and 6.9). */
bool sending_handle(button_event_t event) {
  const training_phase_t phase = training_phase();
  training_select_action_t action;

  if ((phase != TRAINING_PHASE_SELECT) && (phase != TRAINING_PHASE_END)) {
    return false;
  }
  action = training_screen_select_input(event);
  if (action == TRAINING_SELECT_LEAVE) {
    return true;
  }
  if (action == TRAINING_SELECT_START) {
    rng_mix(tick_now());
    tx_start_session();
    training_screen_clear_rows(TRAINING_SCREEN_STATUS_ROW, TRAINING_SCREEN_LAST_ROW);
    /* The start screen header already shows the start pause of the selected level (D-T42 (4)). */
    if (phase == TRAINING_PHASE_END) {
      training_screen_draw_header();
    }
    tx_draw_input();
  }
  return false;
}

_Static_assert((TRAINING_PHASE_ITEM == (TRAINING_PHASE_GAP + 1U)) &&
                   (TRAINING_PHASE_RESULT == (TRAINING_PHASE_GAP + 2U)),
               "GAP, ITEM and RESULT must be one range");

void sending_leave(void) {
  /* The keyer runs only in GAP, ITEM and RESULT (D-T40). */
  if ((uint8_t)(training_phase() - TRAINING_PHASE_GAP) <= (uint8_t)(TRAINING_PHASE_RESULT - TRAINING_PHASE_GAP)) {
    keyer_stop();
    buttons_set_enter_enabled(true);
  }
  training_stop();
}
