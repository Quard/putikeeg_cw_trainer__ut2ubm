/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA scenarios of a Receiving free session of the Callsigns program (references/internal/training_spec.md sections
 * 23.4, 23.5 and 23.10, AC-T57, Q-C5, faults F2, F3, F4, F9 and mutant M15; ui-ux.md sections 5.21 and 9.8, V-216,
 * V-219 to V-221). The real menu, buttons, modes, engine, keyboard and callsign generator run with a fake player and
 * a fake Koch item source that fails the program at any call: a free session must never call koch_make_item().
 * Oracles: the screen texts, cells and pixel probes of ui-ux.md section 9, and the Koch order of section 5.6.
 */

#include "koch/keyboard.h"
#include "koch/koch.h"
#include "menu_internal.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define RX_WPM 32U /* the default RX Speed */
#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define PROBE_MS 40U
#define LONG_HOLD_MS ((uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 50U))
#define WRONG_FIRST '.' /* in the Full set, in no callsign */

/* ---------- Navigation ---------- */

static void step(button_event_t event) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(event);
}

static void press_pin(uint8_t pin, uint16_t duration) {
  harness_pins_for(pin, duration);
  harness_pins_for(0U, PROBE_MS);
}

/* Power-on with RX level @p level at RX Speed (0: an empty EEPROM). */
static void power_on(uint16_t level) {
  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_RECEIVING, RX_WPM, level);
  }
  harness_power_on();
  fake_koch_item_reset();
}

/* CFG of ui-ux.md 9.8: main menu row 3, enter, enter. */
static void open_config(void) {
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_FREE_TRAINING, menu_screen());
}

/* From the focus on Start: sets a value with the edit mode of UX-81, then the focus is on Start again. */
static void set_mode_rx(void) {
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
}

static void set_chars(bool is_full) {
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_PREV);
  if (is_full) {
    step(BUTTON_EVENT_NEXT);
  }
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
}

/* Power-on, CFG, Mode RX and the Chars value: the configuration screen with the focus on Start. */
static void open_rx(uint16_t level, bool is_full) {
  power_on(level);
  open_config();
  set_mode_rx();
  set_chars(is_full);
}

/* CFGV(m, c, START) of ui-ux.md 9.8. */
static bool is_config(const char* mode, const char* chars) {
  const row_text_t title = {0, "Callsigns", true};
  return (menu_screen() == MENU_SCREEN_FREE_TRAINING) && (training_phase() == TRAINING_PHASE_IDLE) &&
         row_matches(0, 0, 127, &title, 1U) && menu_row_is(1, "Mode", mode, false) &&
         menu_row_is(2, "Chars", chars, false) && body_off(3, 6) && start_button();
}

/* FTLIST of ui-ux.md 9.8. */
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

/* ---------- Answers through the group keyboard ---------- */

static const button_event_t group_taps[KEYBOARD_GROUP_COUNT] = {L_TAP, M_TAP, R_TAP, ENTER_EVENT};

/* The taps from the whole set down to the pick of @p character. */
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

static void pick_text(const char* text, bool is_wrong_first) {
  size_t index;
  for (index = 0U; text[index] != '\0'; ++index) {
    pick(((index == 0U) && is_wrong_first) ? WRONG_FIRST : text[index]);
  }
}

#define ANSWER_RIGHT 0U
#define ANSWER_WRONG_ONCE 1U   /* one whole wrong answer, then right */
#define ANSWER_WRONG_THRICE 2U /* three wrong answers, then the enter tap in the answer state */
#define ANSWER_SKIP 3U         /* the long next skip, then the enter tap */

/* Answers the current item in the way of @p kind. */
static void rx_answer(uint8_t kind) {
  char item[TRAINING_TEXT_SIZE];
  unsigned attempt;

  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  if (kind == ANSWER_SKIP) {
    step(LONG_EVENT(R_TAP));
    TEST_ASSERT(training_is_skipped());
    TEST_ASSERT(status("Skipped. The answer is:"));
    step(ENTER_EVENT);
    return;
  }
  if (kind == ANSWER_WRONG_THRICE) {
    for (attempt = 0U; attempt < TRAINING_ATTEMPTS; ++attempt) {
      run_to_phase(TRAINING_PHASE_ITEM);
      pick_text(item, true);
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
    TEST_ASSERT(status("Wrong. The answer is:"));
    step(ENTER_EVENT);
    return;
  }
  if (kind == ANSWER_WRONG_ONCE) {
    pick_text(item, true);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
    run_to_phase(TRAINING_PHASE_ITEM);
    TEST_ASSERT(status("Try 2 of 3"));
  }
  pick_text(item, false);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
}

/*
 * Tap mode on (ui-ux.md 6.8): an L press gives no event while it is held, and the tap event at the release. Call it
 * last in a test: the release narrows the keyboard.
 */
static bool is_tap_mode_on_in_item(void) {
  const uint8_t depth = keyboard_depth();
  bool is_quiet;
  harness_pins_for(BUTTON_BIT_MENU, PROBE_MS);
  is_quiet = (keyboard_depth() == depth) && (training_answer_length() == 0U);
  harness_pins_for(0U, PROBE_MS);
  return is_quiet && ((keyboard_depth() != depth) || (training_answer_length() != 0U));
}

/*
 * After a leave to the Free Training list: tap mode off (a menu/back press acts while held) and enter enabled (a P3.2
 * closure opens the list again). Ends on the configuration screen.
 */
static bool is_leave_clean(void) {
  bool is_ok;
  harness_pins_for(BUTTON_BIT_MENU, PROBE_MS);
  is_ok = (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_MAIN) && (menu_cursor() == 2U);
  harness_pins_for(0U, PROBE_MS);
  harness_pins_for(BUTTON_BIT_ENTER, PROBE_MS);
  is_ok = is_ok && (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_FREE_TRAINING);
  harness_pins_for(0U, PROBE_MS);
  step(BUTTON_EVENT_ENTER);
  if (!is_ok) {
    fprintf(stderr, "leave: tap mode or enter state is wrong\n");
  }
  return is_ok;
}

/* ---------- Scenarios ---------- */

/* AC-T57 (1), V-216, Q-C5: the start, the keyboard of the whole set, tap mode, the first item and its slots. */
static void test_start_and_first_item(void) {
  uint8_t index;
  uint8_t length;
  int slot;

  open_rx(0U, true);
  TEST_ASSERT(is_config("RX", "Full"));
  harness_set_time(HARNESS_T0);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(hdr("RX 32/32 Callsigns   1/20"));
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(body_off(2, 5));
  TEST_ASSERT(keys("A-J", "K-T", "U-3", "4-/"));
  TEST_ASSERT(margins_off());
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());
  TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, keyboard_count());
  for (index = 0U; index < KOCH_WORD_ALPHABET_MAX; ++index) {
    bool is_found = false;
    uint8_t k;
    for (k = 0U; k < keyboard_count(); ++k) {
      is_found = is_found || (keyboard_char(k) == koch_char(index));
    }
    TEST_ASSERT(is_found);
  }
  harness_run_to((uint16_t)(HARNESS_T0 + TRAINING_START_MS - 1U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());
  harness_run_to((uint16_t)(HARNESS_T0 + TRAINING_START_MS));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  TEST_ASSERT_EQUAL(RX_WPM, fake_player_last_wpm());
  TEST_ASSERT_EQUAL(RX_WPM, fake_player_last_eff_wpm());
  TEST_ASSERT_EQUAL(0, strcmp(fake_player_last_text(), (const char*)training_item_text()));
  TEST_ASSERT(hdr("RX 32/32 Callsigns   1/20"));
  length = training_item_length();
  TEST_ASSERT((length >= 3U) && (length <= 6U));
  /* Q-C5: one slot per callsign character, the block on the first (rx_count held 40 before the start). */
  item_expect_reset();
  for (slot = 0; slot < (int)length; ++slot) {
    item_expect((slot == 0) ? ITEM_PART_BLOCK : ITEM_PART_BAR, 68 - 8 * (int)length + 16 * slot, NULL);
  }
  TEST_ASSERT(item_line_is());
  TEST_ASSERT(block_at(68 - 8 * (int)length));
  TEST_ASSERT(is_tap_mode_on_in_item());
  TEST_ASSERT_EQUAL(0U, fake_koch_item_count());
}

/* Runs a whole session with the answers of @p kinds and checks the free end screen of V-219 for @p score. */
static void run_session_to_end(const uint8_t* kinds, unsigned score) {
  char text[16];
  unsigned item;

  harness_set_time(HARNESS_T0);
  harness_event(BUTTON_EVENT_ENTER);
  for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
    rx_answer(kinds[item]);
  }
  run_to_phase(TRAINING_PHASE_END);
  (void)snprintf(text, sizeof(text), "Score %u/20", score);
  TEST_ASSERT(hdr("RX 32/32 Callsigns  20/20"));
  TEST_ASSERT(status(NULL));
  if (!TEST_ASSERT(text_at(2, 7, text))) {
    fprintf(stderr, "expected %s, engine score %u\n", text, (unsigned)training_score());
  }
  TEST_ASSERT(body_off(3, 6));
  TEST_ASSERT(ok_button());
  TEST_ASSERT(margins_off());
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FREE, training_outcome());
  TEST_ASSERT(has_no_iap_write());
  TEST_ASSERT_EQUAL(0U, fake_koch_item_count());
}

/* AC-T57 (2) and (3), V-219: the scores 17, 0 and 20, and F3: a skip of item 20 does not count. */
static void test_end_screen_scores(void) {
  uint8_t kinds[TRAINING_FREE_ITEMS];
  unsigned item;

  for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
    kinds[item] = (item < 17U) ? ANSWER_RIGHT : ANSWER_WRONG_ONCE;
  }
  open_rx(0U, true);
  run_session_to_end(kinds, 17U);

  memset(kinds, ANSWER_WRONG_THRICE, sizeof(kinds));
  open_rx(0U, true);
  run_session_to_end(kinds, 0U);

  memset(kinds, ANSWER_RIGHT, sizeof(kinds));
  open_rx(0U, true);
  run_session_to_end(kinds, 20U);

  kinds[TRAINING_FREE_ITEMS - 1U] = ANSWER_SKIP;
  open_rx(0U, true);
  run_session_to_end(kinds, 19U);
}

/* AC-T57 (2): unlimited RX Attempts give Try 4 after three wrong answers; a long enter replays the answer. */
static void test_attempts_and_replay(void) {
  char item[TRAINING_TEXT_SIZE];
  unsigned attempt;
  size_t starts;

  open_rx(0U, true);
  (void)settings_set(SETTINGS_ID_RX_UNLIMITED, 1U);
  step(BUTTON_EVENT_ENTER);
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  for (attempt = 0U; attempt < 3U; ++attempt) {
    run_to_phase(TRAINING_PHASE_ITEM);
    pick_text(item, true);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  }
  run_to_phase(TRAINING_PHASE_ITEM);
  TEST_ASSERT(status("Try 4"));
  pick_text(item, false);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  TEST_ASSERT_EQUAL(0U, training_score());
  (void)settings_set(SETTINGS_ID_RX_UNLIMITED, 0U);

  /* 3 attempts: the answer state waits for an enter tap; a long enter replays the answer. */
  open_rx(0U, true);
  step(BUTTON_EVENT_ENTER);
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  for (attempt = 0U; attempt < TRAINING_ATTEMPTS; ++attempt) {
    run_to_phase(TRAINING_PHASE_ITEM);
    pick_text(item, true);
  }
  TEST_ASSERT(status("Wrong. The answer is:"));
  TEST_ASSERT(keys("", "", "", "Next"));
  TEST_ASSERT(fake_player_is_running());
  TEST_ASSERT_EQUAL(0, strcmp(item, fake_player_last_text()));
  fake_player_finish();
  harness_run_to((uint16_t)(harness_now + 5000U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  starts = fake_player_start_count();
  step(LONG_EVENT(ENTER_EVENT));
  TEST_ASSERT_EQUAL(starts + 1U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0, strcmp(item, fake_player_last_text()));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  step(ENTER_EVENT);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT(hdr("RX 32/32 Callsigns   2/20"));
}

/* AC-T57 (4), V-220: on the end screen only enter and menu/back act; enter reopens the configuration screen. */
static void test_end_screen_controls(void) {
  uint8_t kinds[TRAINING_FREE_ITEMS];
  size_t calls;

  memset(kinds, ANSWER_RIGHT, sizeof(kinds));
  open_rx(0U, true);
  run_session_to_end(kinds, 20U);
  harness_mark_frame();
  press_pin(BUTTON_BIT_PREV, PROBE_MS);
  press_pin(BUTTON_BIT_NEXT, PROBE_MS);
  press_pin(BUTTON_BIT_NEXT, 1000U);
  press_pin(BUTTON_BIT_PREV, 1000U);
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT_EQUAL(0U, harness_changed_pages());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  calls = fake_player_call_count();
  harness_mark_frame();
  harness_pins_for(BUTTON_BIT_ENTER, PROBE_MS);
  TEST_ASSERT(is_config("RX", "Full"));
  TEST_ASSERT(dirty_is((uint8_t)(PAGE(0) | PAGE(1) | PAGE(2) | PAGE(7))));
  TEST_ASSERT(dirty_matches_change());
  /* FT-6: the press runs receiving_leave(), so at most one stop() and no start. */
  TEST_ASSERT(fake_player_call_count() <= calls + 1U);
  if (fake_player_call_count() > calls) {
    TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_STOP, fake_player_call_at(calls));
  }
  calls = fake_player_call_count();
  harness_pins_for(0U, 2000U);
  TEST_ASSERT(is_config("RX", "Full"));
  TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  TEST_ASSERT(!fake_player_is_running());

  /* Run 2: a menu/back press opens the list. */
  open_rx(0U, true);
  run_session_to_end(kinds, 20U);
  press_pin(BUTTON_BIT_MENU, PROBE_MS);
  TEST_ASSERT(is_list());

  /* Run 3: a held menu/back opens the list at the press; the long event changes nothing. */
  open_rx(0U, true);
  run_session_to_end(kinds, 20U);
  harness_pins_for(BUTTON_BIT_MENU, PROBE_MS);
  TEST_ASSERT(is_list());
  harness_mark_frame();
  harness_pins_for(BUTTON_BIT_MENU, 1000U);
  harness_pins_for(0U, PROBE_MS);
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT(is_list());
  TEST_ASSERT(has_no_iap_write());
}

/* F9 RX: enter held from the last result across END opens nothing; each new press acts once. */
static void test_enter_held_across_end(void) {
  uint8_t kinds[TRAINING_FREE_ITEMS];
  unsigned item;
  char last[TRAINING_TEXT_SIZE];

  memset(kinds, ANSWER_RIGHT, sizeof(kinds));
  open_rx(0U, true);
  step(BUTTON_EVENT_ENTER);
  for (item = 0U; item + 1U < TRAINING_FREE_ITEMS; ++item) {
    rx_answer(ANSWER_RIGHT);
  }
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(last, (const char*)training_item_text(), sizeof(last));
  pick_text(last, false);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  harness_pins_for(BUTTON_BIT_ENTER, 100U);
  run_to_phase(TRAINING_PHASE_END);
  harness_pins_for(BUTTON_BIT_ENTER, 2000U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT(text_at(2, 7, "Score 20/20"));
  TEST_ASSERT(ok_button());
  harness_pins_for(0U, 100U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  press_pin(BUTTON_BIT_ENTER, PROBE_MS);
  TEST_ASSERT(is_config("RX", "Full"));
  harness_pins_for(BUTTON_BIT_ENTER, PROBE_MS);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT(hdr("RX 32/32 Callsigns   1/20"));
  harness_pins_for(0U, PROBE_MS);
}

#define STATE_START_PAUSE 0U
#define STATE_ITEM 1U
#define STATE_WRONG 2U
#define STATE_ANSWER 3U
#define STATE_BLANK 4U
#define STATE_END 5U
#define STATE_COUNT 6U

/* Leads a new RX session into @p state. */
static void enter_state(uint8_t state) {
  char item[TRAINING_TEXT_SIZE];
  unsigned number;
  unsigned attempt;

  open_rx(0U, true);
  step(BUTTON_EVENT_ENTER);
  if (state == STATE_START_PAUSE) {
    return;
  }
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  if (state == STATE_WRONG) {
    pick_text(item, true);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  } else if (state == STATE_ANSWER) {
    for (attempt = 0U; attempt < TRAINING_ATTEMPTS; ++attempt) {
      run_to_phase(TRAINING_PHASE_ITEM);
      pick_text(item, true);
    }
    TEST_ASSERT(status("Wrong. The answer is:"));
  } else if (state == STATE_BLANK) {
    pick_text(item, false);
    run_to_phase(TRAINING_PHASE_GAP);
    TEST_ASSERT(hdr("RX 32/32 Callsigns   2/20"));
  } else if (state == STATE_END) {
    pick_text(item, false);
    for (number = 1U; number < TRAINING_FREE_ITEMS; ++number) {
      rx_answer(ANSWER_RIGHT);
    }
    run_to_phase(TRAINING_PHASE_END);
  }
}

/* F4 RX, V-221: a long menu/back in every state opens the list, stops the player and gives the buttons back. */
static void test_long_back_every_state(void) {
  uint8_t state;

  for (state = 0U; state < STATE_COUNT; ++state) {
    enter_state(state);
    if (state == STATE_ITEM) {
      /* A real hold in tap mode: only the long event acts. */
      TEST_ASSERT(fake_player_is_running());
      harness_pins_for(BUTTON_BIT_MENU, PROBE_MS);
      TEST_ASSERT_EQUAL(MENU_SCREEN_FREE_TRAINING, menu_screen());
      harness_pins_for(BUTTON_BIT_MENU, LONG_HOLD_MS);
      harness_pins_for(0U, PROBE_MS);
    } else if (state == STATE_END) {
      /* Tap mode is off on the end screen: the press already leaves. */
      harness_pins_for(BUTTON_BIT_MENU, PROBE_MS);
      TEST_ASSERT(is_list());
      harness_pins_for(BUTTON_BIT_MENU, LONG_HOLD_MS);
      harness_pins_for(0U, PROBE_MS);
    } else {
      step(LONG_EVENT(BUTTON_EVENT_MENU));
    }
    if (!TEST_ASSERT(is_list())) {
      fprintf(stderr, "state %u\n", (unsigned)state);
    }
    TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
    TEST_ASSERT(!fake_player_is_running());
    if (fake_player_call_count() != 0U) {
      TEST_ASSERT((state == STATE_END) ||
                  (fake_player_call_at(fake_player_call_count() - 1U) == FAKE_PLAYER_CALL_STOP));
    }
    TEST_ASSERT(has_no_iap_write());
    TEST_ASSERT(is_leave_clean());
    if (!TEST_ASSERT(is_config("RX", "Full"))) {
      fprintf(stderr, "state %u: no configuration screen after the new entry\n", (unsigned)state);
    }
    TEST_ASSERT_EQUAL(0U, fake_koch_item_count());
  }
}

/* F2: level index 65535 (43 learned characters): the set is the 40 word characters, no prosign anywhere. */
static void test_level_65535(void) {
  unsigned item;
  const char* text;

  open_rx(65535U, false);
  TEST_ASSERT_EQUAL(43U, koch_learned_count(65535U));
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, training_alphabet_count());
  TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, keyboard_count());
  TEST_ASSERT(keys("A-J", "K-T", "U-3", "4-/"));
  for (item = 0U; item < keyboard_count(); ++item) {
    TEST_ASSERT(koch_index_of(keyboard_char((uint8_t)item)) < KOCH_WORD_ALPHABET_MAX);
  }
  for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
    size_t index;
    run_to_phase(TRAINING_PHASE_ITEM);
    text = (const char*)training_item_text();
    for (index = 0U; text[index] != '\0'; ++index) {
      TEST_ASSERT(((text[index] >= 'A') && (text[index] <= 'Z')) || ((text[index] >= '0') && (text[index] <= '9')));
    }
    rx_answer(ANSWER_RIGHT);
  }
  run_to_phase(TRAINING_PHASE_END);
  TEST_ASSERT(text_at(2, 7, "Score 20/20"));
}

/* M15: the entry seeds the RNG with the tick of the press, so starts at different ticks give different callsigns. */
static void test_first_callsign_follows_the_tick(void) {
  static const uint16_t ticks[5] = {11000U, 12345U, 23456U, 34567U, 45678U};
  char firsts[5][TRAINING_TEXT_SIZE];
  unsigned run;
  unsigned distinct = 0U;

  for (run = 0U; run < 5U; ++run) {
    open_rx(0U, true);
    harness_set_time(ticks[run]);
    harness_event(BUTTON_EVENT_ENTER);
    run_to_phase(TRAINING_PHASE_ITEM);
    memcpy(firsts[run], (const char*)training_item_text(), TRAINING_TEXT_SIZE);
  }
  TEST_ASSERT(strcmp(firsts[0], firsts[1]) != 0);
  for (run = 0U; run < 5U; ++run) {
    unsigned other;
    bool is_new = true;
    for (other = 0U; other < run; ++other) {
      is_new = is_new && (strcmp(firsts[run], firsts[other]) != 0);
    }
    if (is_new) {
      ++distinct;
    }
  }
  printf("first callsigns: %s %s %s %s %s\n", firsts[0], firsts[1], firsts[2], firsts[3], firsts[4]);
  TEST_ASSERT(distinct >= 4U);
}

int main(void) {
  TEST_RUN(test_start_and_first_item);
  TEST_RUN(test_end_screen_scores);
  TEST_RUN(test_attempts_and_replay);
  TEST_RUN(test_end_screen_controls);
  TEST_RUN(test_enter_held_across_end);
  TEST_RUN(test_long_back_every_state);
  TEST_RUN(test_level_65535);
  TEST_RUN(test_first_callsign_follows_the_tick);
  return TEST_FINISH();
}
