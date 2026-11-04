/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA scenarios of a Sending free session of the Callsigns program (references/internal/training_spec.md sections
 * 23.4, 23.5 and 23.10, AC-T58, Q-C5, Q-C8, faults F3, F4 and F9; ui-ux.md sections 5.21 and 9.8, V-218, V-221,
 * V-222). The real menu, buttons, modes, engine and callsign generator run with a fake keyer, a fake player and a
 * fake Koch item source. Oracles: the screen texts, cells and pixel probes of ui-ux.md section 9.
 */

#include "cw/morse.h"
#include "koch/keyboard.h"
#include "koch/koch.h"
#include "menu_internal.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define RX_WPM 32U
#define TX_WPM 20U
#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define PROBE_MS 40U
#define LONG_HOLD_MS ((uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 50U))
#define CODE_MARKS_MAX 12U /* ui-ux.md 5.13.3: row 7 shows at most 12 marks of a character */

/* ---------- Navigation ---------- */

static void step(button_event_t event) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(event);
}

static void press_pin(uint8_t pin, uint16_t duration) {
  harness_pins_for(pin, duration);
  harness_pins_for(0U, PROBE_MS);
}

static void power_on(void) {
  fake_iap_reset();
  harness_power_on();
  fake_koch_item_reset();
  (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, 1U);
}

static void open_config(void) {
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_FREE_TRAINING, menu_screen());
}

/* From the focus on Start: Mode TX and Chars Full with the edit mode of UX-81; the focus ends on Start. */
static void set_tx_full(void) {
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
}

static void set_rx_full(void) {
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
}

static void open_tx(void) {
  power_on();
  open_config();
  set_tx_full();
}

/* CFGV(m, c, START) of ui-ux.md 9.8. */
static bool is_config(const char* mode, const char* chars) {
  const row_text_t title = {0, "Callsigns", true};
  return (menu_screen() == MENU_SCREEN_FREE_TRAINING) && (training_phase() == TRAINING_PHASE_IDLE) &&
         row_matches(0, 0, 127, &title, 1U) && menu_row_is(1, "Mode", mode, false) &&
         menu_row_is(2, "Chars", chars, false) && body_off(3, 6) && start_button();
}

static bool is_list(void) {
  const row_text_t title = {0, "Free Training", true};
  return (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_FREE_TRAINING) && (menu_cursor() == 0U) &&
         row_matches(0, 0, 127, &title, 1U) && menu_row_is(1, "Callsigns", NULL, true) && body_off(2, 7);
}

static bool has_no_iap_write(void) {
  return (fake_iap_command_count(CMD_PROGRAM) == 0U) && (fake_iap_command_count(CMD_ERASE) == 0U);
}

static void run_to_phase(training_phase_t phase) {
  unsigned guard = 0U;
  while ((training_phase() != phase) && (guard < 20000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
  TEST_ASSERT_EQUAL(phase, training_phase());
}

/* ---------- Keying through the fake keyer ---------- */

static void drain(void) {
  unsigned guard = 0U;
  do {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  } while ((fake_keyer_pending_count() != 0U) && (guard < 100U));
}

static void push(keyer_event_t event) {
  fake_keyer_push_event(event);
  drain();
}

static const char* code_of(char character) {
  static char text[8];
  const uint8_t pattern = morse_encode(character);
  uint8_t shift = 0U;
  size_t length = 0U;

  while ((uint8_t)(pattern >> shift) > 1U) {
    ++shift;
  }
  while (shift != 0U) {
    --shift;
    text[length] = (((pattern >> shift) & 1U) != 0U) ? '-' : '.';
    ++length;
  }
  text[length] = '\0';
  return text;
}

/* The elements of @p character without the CHAR_GAP. */
static void key_elements(char character) {
  const char* code = code_of(character);
  size_t index;
  for (index = 0U; code[index] != '\0'; ++index) {
    push((code[index] == '-') ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
}

static void key_char(char character) {
  key_elements(character);
  push(KEYER_EVENT_CHAR_GAP);
}

/* A character that is not @p expected: E, or T for an expected E. */
static char other_than(char expected) { return (expected == 'E') ? 'T' : 'E'; }

static void key_item_correct(void) {
  char item[TRAINING_TEXT_SIZE];
  size_t index;
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  for (index = 0U; item[index] != '\0'; ++index) {
    key_char(item[index]);
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
}

/* Enter disabled: a P3.2 closure gives no menu event. One pass of the main loop with an event log. */
static bool pass_logs_enter(uint16_t duration) {
  bool has_enter = false;
  uint16_t elapsed;
  for (elapsed = 0U; elapsed < duration; ++elapsed) {
    button_event_t event;
    unsigned count;
    harness_set_time((uint16_t)(harness_now + 1U));
    fake_tick_set_now(harness_now);
    for (count = 0U; count < HARNESS_EVENTS_PER_PASS; ++count) {
      event = buttons_poll();
      if (event == BUTTON_EVENT_NONE) {
        break;
      }
      has_enter = has_enter || (BUTTON_EVENT_CODE(event) == BUTTON_EVENT_ENTER);
      menu_handle(event);
    }
    menu_poll();
    if (menu_needs_redraw()) {
      menu_render();
    }
  }
  return has_enter;
}

/* After a leave to the list: enter enabled (a P3.2 closure opens the list again). Ends on the configuration screen. */
static bool is_leave_clean(void) {
  bool is_ok;
  press_pin(BUTTON_BIT_MENU, PROBE_MS);
  is_ok = (menu_node() == MENU_NODE_MAIN) && (menu_cursor() == 2U);
  harness_pins_for(BUTTON_BIT_ENTER, PROBE_MS);
  is_ok = is_ok && (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_FREE_TRAINING);
  harness_pins_for(0U, PROBE_MS);
  step(BUTTON_EVENT_ENTER);
  return is_ok;
}

/* ---------- Scenarios ---------- */

/* AC-T58 (1), V-218: the keyer starts once with the TX settings, enter is disabled, the header shows the item. */
static void test_start_header_and_keyer(void) {
  unsigned item;

  open_tx();
  (void)settings_set(SETTINGS_ID_KEY_TYPE, SETTINGS_KEY_IAMBIC_A);
  (void)settings_set(SETTINGS_ID_PADDLE_SWAP, 1U);
  (void)settings_set(SETTINGS_ID_TX_EFF_WPM, 15U);
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  harness_set_time(HARNESS_T0);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(hdr("TX 20/15 Callsigns   1/20"));
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(TX_WPM, fake_keyer_last_wpm());
  TEST_ASSERT_EQUAL(15U, fake_keyer_last_eff_wpm());
  TEST_ASSERT_EQUAL(SETTINGS_KEY_IAMBIC_A, fake_keyer_last_key_type());
  TEST_ASSERT(fake_keyer_last_is_swapped());
  TEST_ASSERT(body_off(1, 7));
  run_to_phase(TRAINING_PHASE_ITEM);
  TEST_ASSERT(blank_row(7));
  TEST_ASSERT(!pass_logs_enter(PROBE_MS));
  harness_pins = BUTTON_BIT_ENTER;
  fake_button_pins_set(harness_pins);
  TEST_ASSERT(!pass_logs_enter(PROBE_MS));
  harness_pins = 0U;
  fake_button_pins_set(harness_pins);
  TEST_ASSERT(!pass_logs_enter(PROBE_MS));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  for (item = 1U; item <= 11U; ++item) {
    key_item_correct();
  }
  run_to_phase(TRAINING_PHASE_ITEM);
  TEST_ASSERT(hdr("TX 20/15 Callsigns  12/20"));
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(0U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0U, fake_koch_item_count());

  open_tx();
  (void)settings_set(SETTINGS_ID_TX_WPM, 5U);
  (void)settings_set(SETTINGS_ID_TX_EFF_WPM, 5U);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(hdr("TX 5/5   Callsigns   1/20"));
  TEST_ASSERT_EQUAL(5U, fake_keyer_last_wpm());
}

/* AC-T58 (2): normal character gaps give CORRECT; a WORD_GAP after a PARTIAL CHAR_GAP is the word-space error. */
static void test_word_space_inside_a_callsign(void) {
  char item[TRAINING_TEXT_SIZE];

  open_tx();
  step(BUTTON_EVENT_ENTER);
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  key_char(item[0]);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  push(KEYER_EVENT_WORD_GAP);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  TEST_ASSERT(text_at(1, 2, "Wrong: pause too long"));
  run_to_phase(TRAINING_PHASE_ITEM);
  TEST_ASSERT(status("Try 2 of 3"));
  key_item_correct();
  TEST_ASSERT_EQUAL(0U, training_score());
}

/* AC-T58 (3): three attempts, unlimited attempts, the skip and Show CW Code Off. */
static void test_attempts_skip_and_code(void) {
  char item[TRAINING_TEXT_SIZE];
  unsigned attempt;

  open_tx();
  step(BUTTON_EVENT_ENTER);
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  for (attempt = 1U; attempt <= TRAINING_ATTEMPTS; ++attempt) {
    run_to_phase(TRAINING_PHASE_ITEM);
    key_char(other_than(item[0]));
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(code_is(code_of(item[0])));
  run_to_phase(TRAINING_PHASE_GAP);
  TEST_ASSERT(hdr("TX 20/20 Callsigns   2/20"));

  open_tx();
  (void)settings_set(SETTINGS_ID_TX_UNLIMITED, 1U);
  step(BUTTON_EVENT_ENTER);
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  for (attempt = 1U; attempt <= 3U; ++attempt) {
    run_to_phase(TRAINING_PHASE_ITEM);
    key_char(other_than(item[0]));
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  }
  run_to_phase(TRAINING_PHASE_ITEM);
  TEST_ASSERT(status("Try 4"));
  key_item_correct();
  TEST_ASSERT_EQUAL(0U, training_score());
  (void)settings_set(SETTINGS_ID_TX_UNLIMITED, 0U);

  /* The long next skip after one keyed character: the code of the next character shows. */
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  key_char(item[0]);
  step(LONG_EVENT(BUTTON_EVENT_NEXT));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(training_is_skipped());
  TEST_ASSERT(status("Skipped"));
  TEST_ASSERT(code_is(code_of(item[1])));

  open_tx();
  (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, 0U);
  step(BUTTON_EVENT_ENTER);
  run_to_phase(TRAINING_PHASE_ITEM);
  push(KEYER_EVENT_DIT);
  TEST_ASSERT(blank_row(7));
}

/*
 * Q-C5 TX: before the start, tx_mark_count held the character count (40). The first DIT of the first item draws at
 * x 2, and 13 DITs show exactly 12 marks.
 */
static void test_first_marks_of_a_free_session(void) {
  unsigned dit;

  open_tx();
  step(BUTTON_EVENT_ENTER);
  run_to_phase(TRAINING_PHASE_ITEM);
  push(KEYER_EVENT_DIT);
  TEST_ASSERT(marks_are(7, 2, "."));
  for (dit = 2U; dit <= CODE_MARKS_MAX + 1U; ++dit) {
    push(KEYER_EVENT_DIT);
  }
  TEST_ASSERT(marks_are(7, 2, "............"));
}

/* Runs a TX session with all items correct, the last one with @p is_last_skipped, to the end screen. */
static void run_tx_session(bool is_last_skipped) {
  unsigned item;
  for (item = 1U; item < TRAINING_FREE_ITEMS; ++item) {
    key_item_correct();
  }
  run_to_phase(TRAINING_PHASE_ITEM);
  if (is_last_skipped) {
    step(LONG_EVENT(BUTTON_EVENT_NEXT));
    TEST_ASSERT(training_is_skipped());
  } else {
    key_item_correct();
  }
  run_to_phase(TRAINING_PHASE_END);
}

/* AC-T58 (4), F3 TX: the end screen, the keyer stops, enter is enabled and opens the configuration screen. */
static void test_end_screen(void) {
  size_t stops;

  open_tx();
  step(BUTTON_EVENT_ENTER);
  stops = fake_keyer_stop_count();
  run_tx_session(false);
  TEST_ASSERT(hdr("TX 20/20 Callsigns  20/20"));
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(text_at(2, 7, "Score 20/20"));
  TEST_ASSERT(body_off(3, 6));
  TEST_ASSERT(ok_button());
  TEST_ASSERT(margins_off());
  TEST_ASSERT(!fake_keyer_is_running());
  TEST_ASSERT_EQUAL(stops + 1U, fake_keyer_stop_count());
  TEST_ASSERT(has_no_iap_write());
  harness_mark_frame();
  harness_pins_for(BUTTON_BIT_ENTER, PROBE_MS);
  TEST_ASSERT(is_config("TX", "Full"));
  TEST_ASSERT(dirty_is((uint8_t)(PAGE(0) | PAGE(1) | PAGE(2) | PAGE(7))));
  TEST_ASSERT(dirty_matches_change());
  harness_pins_for(0U, PROBE_MS);
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());

  open_tx();
  step(BUTTON_EVENT_ENTER);
  run_tx_session(true);
  TEST_ASSERT(text_at(2, 7, "Score 19/20"));
  TEST_ASSERT(body_off(3, 6));
  TEST_ASSERT(ok_button());
  TEST_ASSERT_EQUAL(0U, fake_koch_item_count());
}

/* V-222, F9 TX: P3.2 held across the end opens nothing; each new press acts once. */
static void test_contact_held_across_end(void) {
  char item[TRAINING_TEXT_SIZE];
  unsigned number;
  size_t index;
  size_t starts;

  open_tx();
  step(BUTTON_EVENT_ENTER);
  for (number = 1U; number < TRAINING_FREE_ITEMS; ++number) {
    key_item_correct();
  }
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  for (index = 0U; item[index + 1U] != '\0'; ++index) {
    key_char(item[index]);
  }
  key_elements(item[index]);
  harness_pins_for(BUTTON_BIT_ENTER, 100U);
  push(KEYER_EVENT_CHAR_GAP);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  run_to_phase(TRAINING_PHASE_END);
  harness_pins_for(BUTTON_BIT_ENTER, 2000U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT(text_at(2, 7, "Score 20/20"));
  TEST_ASSERT(ok_button());
  harness_pins_for(0U, 100U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  press_pin(BUTTON_BIT_ENTER, 100U);
  TEST_ASSERT(is_config("TX", "Full"));
  starts = fake_keyer_start_count();
  harness_pins_for(BUTTON_BIT_ENTER, 100U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT(hdr("TX 20/20 Callsigns   1/20"));
  TEST_ASSERT_EQUAL(starts + 1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(0U, fake_keyer_pending_count());
  TEST_ASSERT(blank_row(7));
  harness_pins_for(0U, PROBE_MS);
}

#define STATE_START_PAUSE 0U
#define STATE_KEYING 1U
#define STATE_WRONG 2U
#define STATE_FAILED 3U
#define STATE_BLANK 4U
#define STATE_END 5U
#define STATE_COUNT 6U

static void enter_state(uint8_t state) {
  char item[TRAINING_TEXT_SIZE];
  unsigned attempt;

  open_tx();
  step(BUTTON_EVENT_ENTER);
  if (state == STATE_START_PAUSE) {
    return;
  }
  if (state == STATE_END) {
    run_tx_session(false);
    return;
  }
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  if (state == STATE_KEYING) {
    push(KEYER_EVENT_DIT);
  } else if (state == STATE_WRONG) {
    key_char(other_than(item[0]));
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  } else if (state == STATE_FAILED) {
    for (attempt = 0U; attempt < TRAINING_ATTEMPTS; ++attempt) {
      run_to_phase(TRAINING_PHASE_ITEM);
      key_char(other_than(item[0]));
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  } else {
    key_item_correct();
    run_to_phase(TRAINING_PHASE_GAP);
  }
}

/* F4 TX, V-221: a long menu/back in every state opens the list, stops the keyer once and enables enter. */
static void test_long_back_every_state(void) {
  uint8_t state;

  for (state = 0U; state < STATE_COUNT; ++state) {
    size_t stops;
    enter_state(state);
    stops = fake_keyer_stop_count();
    if (state == STATE_END) {
      harness_pins_for(BUTTON_BIT_MENU, PROBE_MS);
      TEST_ASSERT(is_list());
      harness_pins_for(BUTTON_BIT_MENU, LONG_HOLD_MS);
      harness_pins_for(0U, PROBE_MS);
      TEST_ASSERT_EQUAL(stops, fake_keyer_stop_count());
    } else {
      step(LONG_EVENT(BUTTON_EVENT_MENU));
      TEST_ASSERT_EQUAL(stops + 1U, fake_keyer_stop_count());
    }
    if (!TEST_ASSERT(is_list())) {
      fprintf(stderr, "state %u\n", (unsigned)state);
    }
    TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
    TEST_ASSERT(!fake_keyer_is_running());
    TEST_ASSERT(has_no_iap_write());
    TEST_ASSERT(is_leave_clean());
    if (!TEST_ASSERT(is_config("TX", "Full"))) {
      fprintf(stderr, "state %u: no configuration screen after the new entry\n", (unsigned)state);
    }
    TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  }
}

/* ---------- Q-C8: Koch sessions after the band export ---------- */

static const button_event_t group_taps[KEYBOARD_GROUP_COUNT] = {L_TAP, M_TAP, R_TAP, ENTER_EVENT};

static void pick(char character) {
  uint8_t target = 0xFFU;
  uint8_t index;
  unsigned guard;
  for (index = 0U; index < keyboard_count(); ++index) {
    if (keyboard_char(index) == character) {
      target = index;
    }
  }
  if (!TEST_ASSERT(target != 0xFFU)) {
    return;
  }
  for (guard = 0U; guard < KEYBOARD_DEPTH_MAX; ++guard) {
    uint8_t group;
    for (group = 0U; group < KEYBOARD_GROUP_COUNT; ++group) {
      const uint8_t first = keyboard_group_first(group);
      if ((target >= first) && (target < (uint8_t)(first + keyboard_group_size(group)))) {
        break;
      }
    }
    if (!TEST_ASSERT(group < KEYBOARD_GROUP_COUNT)) {
      return;
    }
    step(group_taps[group]);
    if (keyboard_depth() == 0U) {
      return;
    }
  }
}

/*
 * Q-C8, F5: a free session calls no koch_make_item(). Then Receiving and Sending from the main menu show the Koch
 * header, the Start band and, after a pass, the OK band, bit-exactly; their items come from koch_make_item().
 */
static void test_koch_sessions_after_free(void) {
  unsigned item;
  uint8_t mode;

  for (mode = 0U; mode < 2U; ++mode) {
    power_on();
    open_config();
    if (mode == 1U) {
      set_tx_full();
    } else {
      set_rx_full();
    }
    step(BUTTON_EVENT_ENTER);
    TEST_ASSERT(hdr((mode == 1U) ? "TX 20/20 Callsigns   1/20" : "RX 32/32 Callsigns   1/20"));
    for (item = 1U; item <= TRAINING_FREE_ITEMS; ++item) {
      run_to_phase(TRAINING_PHASE_ITEM);
      if (mode == 1U) {
        key_item_correct();
      } else {
        char text[TRAINING_TEXT_SIZE];
        size_t index;
        memcpy(text, (const char*)training_item_text(), sizeof(text));
        for (index = 0U; text[index] != '\0'; ++index) {
          pick(text[index]);
        }
      }
    }
    run_to_phase(TRAINING_PHASE_END);
    TEST_ASSERT_EQUAL(0U, fake_koch_item_count());
    step(BUTTON_EVENT_MENU);
    step(BUTTON_EVENT_MENU);
    TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
    TEST_ASSERT_EQUAL(2U, menu_cursor());
    step(BUTTON_EVENT_PREV);
    step(BUTTON_EVENT_PREV);
    if (mode == 1U) {
      step(BUTTON_EVENT_NEXT);
    }
    fake_koch_item_push("K");
    harness_set_time(HARNESS_T0);
    harness_event(ENTER_EVENT);
    TEST_ASSERT_EQUAL((mode == 1U) ? MENU_SCREEN_SENDING : MENU_SCREEN_RECEIVING, menu_screen());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
    TEST_ASSERT(!training_is_free());
    TEST_ASSERT(hdr((mode == 1U) ? "TX 20/20 L1          1/50" : "RX 32/32 L1          1/50"));
    TEST_ASSERT(start_button());
    harness_event(ENTER_EVENT);
    for (item = 1U; item <= 50U; ++item) {
      run_to_phase(TRAINING_PHASE_ITEM);
      if (mode == 1U) {
        key_char('K');
      } else {
        step(L_TAP);
      }
      TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
    }
    run_to_phase(TRAINING_PHASE_END);
    TEST_ASSERT_EQUAL(50U, fake_koch_item_count());
    TEST_ASSERT(hdr((mode == 1U) ? "TX 20/20 L1         50/50" : "RX 32/32 L1         50/50"));
    TEST_ASSERT(end_screen_is(50U, 50U, 45U));
    TEST_ASSERT(ok_button());
  }
}

int main(void) {
  TEST_RUN(test_start_header_and_keyer);
  TEST_RUN(test_word_space_inside_a_callsign);
  TEST_RUN(test_attempts_skip_and_code);
  TEST_RUN(test_first_marks_of_a_free_session);
  TEST_RUN(test_end_screen);
  TEST_RUN(test_contact_held_across_end);
  TEST_RUN(test_long_back_every_state);
  TEST_RUN(test_koch_sessions_after_free);
  return TEST_FINISH();
}
