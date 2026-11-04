/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Free Training and the Callsigns program at the menu seam (references/internal/training_spec.md sections 23.5,
 * 23.6 and 23.8, AC-T57 to AC-T60; ui-ux.md sections 5.21 and 9.8, V-207 to V-225). The real menu, buttons, modes,
 * engine, progress and callsign generator run with a fake keyer, a fake player and fake Koch items.
 */

#include "cw/morse.h"
#include "koch/keyboard.h"
#include "koch/koch.h"
#include "menu_internal.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define FOCUS_MODE 0
#define FOCUS_CHARS 1
#define FOCUS_START 2
#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define RX_WPM 32U /* the default RX Speed */
#define TX_WPM 20U /* the default TX Speed */

static void step(button_event_t event) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(event);
}

/*
 * Mode and Chars live in RAM until a power-off, and a host power-on keeps them. So each test first sets RX and
 * Learned through the configuration screen, then powers on again.
 */
static void set_default_values(void) {
  static const button_event_t steps[] = {
      BUTTON_EVENT_NEXT, BUTTON_EVENT_NEXT,  BUTTON_EVENT_ENTER, BUTTON_EVENT_ENTER, /* the configuration screen */
      BUTTON_EVENT_NEXT, BUTTON_EVENT_ENTER, BUTTON_EVENT_PREV,  BUTTON_EVENT_ENTER, /* Mode RX */
      BUTTON_EVENT_NEXT, BUTTON_EVENT_ENTER, BUTTON_EVENT_PREV,  BUTTON_EVENT_ENTER, /* Chars Learned */
  };
  size_t index;
  for (index = 0U; index < sizeof(steps) / sizeof(steps[0]); ++index) {
    step(steps[index]);
  }
}

/* Power-on with an empty EEPROM, or with RX level @p rx at RX Speed and TX level @p tx at TX Speed (EARN). */
static void power_on_with(uint16_t rx, uint16_t tx) {
  fake_iap_reset();
  if ((rx != 0U) || (tx != 0U)) {
    progress_fixture_write(PROGRESS_MODE_RECEIVING, RX_WPM, rx);
    progress_fixture_add(PROGRESS_MODE_SENDING, TX_WPM, tx);
  }
  harness_power_on();
  set_default_values();
  harness_power_on();
  fake_koch_item_reset();
}

/* A step that dirties exactly the pages whose bytes changed. */
static void step_dirty_free(button_event_t event) {
  harness_mark_frame();
  step(event);
  TEST_ASSERT(dirty_matches_change());
}

/* A step that must dirty exactly @p mask, which is also the set of the pages whose bytes changed. */
static void step_dirty(button_event_t event, uint8_t mask) {
  harness_mark_frame();
  step(event);
  TEST_ASSERT(dirty_matches_change());
  TEST_ASSERT(dirty_is(mask));
}

/* FTL: main menu row 3, then enter. */
static void open_list(void) {
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
}

/* CFG: FTL, then enter. */
static void open_config(void) {
  open_list();
  step(BUTTON_EVENT_ENTER);
}

/* MODE TX and CHARS FULL of ui-ux.md 9.8, from the focus on Start. Both end with the focus on Start. */
static void select_tx(void) {
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
}

static void select_full(void) {
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
}

/* FTLIST */
static bool is_list(void) {
  const row_text_t title = {0, "Free Training", true};
  return row_matches(0, 0, 127, &title, 1U) && menu_row_is(1, "Callsigns", NULL, true) && body_off(2, 7) &&
         (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_FREE_TRAINING) && (menu_cursor() == 0U);
}

/* The main menu row 3 with the cursor on Free Training: the label and '>' inverted in the band. */
static bool is_main_on_free_training(void) {
  const row_text_t texts[2] = {{0, "Free Training", true}, {24, ">", true}};
  return (menu_node() == MENU_NODE_MAIN) && (menu_cursor() == 2U) && row_matches(3, 0, 127, texts, 2U);
}

/* EDIT(r, fw) with the value and the markers of ui-ux.md 5.4. */
static bool edit_row_is(int r, const char* label, const char* value, int field_width, bool has_prev, bool has_next) {
  row_text_t texts[4] = {{0, label, false}, {23 - (int)strlen(value), value, true}};
  size_t count = 2U;
  if (has_prev) {
    texts[count].column = 21 - field_width;
    texts[count].text = "<";
    texts[count].is_inverted = true;
    ++count;
  }
  if (has_next) {
    texts[count].column = 24;
    texts[count].text = ">";
    texts[count].is_inverted = true;
    ++count;
  }
  return row_matches(r, 106 - 5 * field_width, 127, texts, count);
}

static bool is_message_shown(void) {
  return text_at(4, 2, "No digit learned yet") && text_at(5, 2, "Use Full or reach L25");
}

/* CFGV(m, c, f), with NODIGIT when @p has_message. */
static bool is_config(const char* mode, const char* chars, int focus, bool has_message) {
  const row_text_t title = {0, "Callsigns", true};
  bool is_ok = (menu_screen() == MENU_SCREEN_FREE_TRAINING) && (training_phase() == TRAINING_PHASE_IDLE);
  is_ok = is_ok && row_matches(0, 0, 127, &title, 1U);
  is_ok = is_ok && menu_row_is(1, "Mode", mode, focus == FOCUS_MODE);
  is_ok = is_ok && menu_row_is(2, "Chars", chars, focus == FOCUS_CHARS);
  is_ok = is_ok && blank(3) && blank(6);
  is_ok = is_ok && (has_message ? is_message_shown() : body_off(4, 5));
  return is_ok && ((focus == FOCUS_START) ? start_button() : text_at(7, 10, "Start"));
}

static bool has_no_iap_write(void) {
  return (fake_iap_command_count(CMD_PROGRAM) == 0U) && (fake_iap_command_count(CMD_ERASE) == 0U);
}

static bool is_callsign_item(void) {
  const uint8_t length = training_item_length();
  return training_is_free() && (length >= 3U) && (length <= 6U) && (training_item_text()[length] == '\0');
}

/* Passes from the next ms until the engine is in @p phase. */
static void run_to_phase(training_phase_t phase) {
  unsigned guard = 0U;
  while ((training_phase() != phase) && (guard < 20000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
}

/* ---------- RX answers through the group keyboard ---------- */

static const button_event_t group_taps[KEYBOARD_GROUP_COUNT] = {L_TAP, M_TAP, R_TAP, ENTER_EVENT};

/* Taps the groups that lead to @p character, up to the pick. */
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

/* The current RX item: right at once, or one wrong pick first (a full wrong answer), then right. */
static void rx_answer(bool is_wrong_first) {
  char item[TRAINING_TEXT_SIZE];
  uint8_t index;

  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, training_item_text(), sizeof(item));
  if (is_wrong_first) {
    /* '.' is in the Full set and in no callsign. */
    for (index = 0U; item[index] != '\0'; ++index) {
      pick((index == 0U) ? '.' : item[index]);
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
    run_to_phase(TRAINING_PHASE_ITEM);
  }
  for (index = 0U; item[index] != '\0'; ++index) {
    pick(item[index]);
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
}

/* ---------- TX answers through the fake keyer ---------- */

static void drain(void) {
  unsigned guard = 0U;
  do {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  } while ((fake_keyer_pending_count() != 0U) && (guard < 100U));
}

/* The elements of @p character, then a CHAR_GAP. */
static void key_char(char character) {
  const uint8_t pattern = morse_encode(character);
  uint8_t shift = 0U;

  while ((uint8_t)(pattern >> shift) > 1U) {
    ++shift;
  }
  while (shift != 0U) {
    --shift;
    fake_keyer_push_event((((pattern >> shift) & 1U) != 0U) ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
    drain();
  }
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  drain();
}

static void tx_answer(void) {
  char item[TRAINING_TEXT_SIZE];
  uint8_t index;

  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, training_item_text(), sizeof(item));
  for (index = 0U; item[index] != '\0'; ++index) {
    key_char(item[index]);
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
}

/* ---------- Scenarios ---------- */

/* V-207, AC-T59 (1): the list, its no-op events, and the way back to the main menu. */
static void test_v207_list(void) {
  power_on_with(0U, 0U);
  TEST_ASSERT(menu_row_is(1, "Receiving", "L1", true));
  TEST_ASSERT(menu_row_is(2, "Sending", "L1", false));
  open_list();
  TEST_ASSERT(is_list());
  step_dirty(BUTTON_EVENT_PREV, 0U);
  step_dirty(BUTTON_EVENT_NEXT, 0U);
  harness_mark_frame();
  harness_pins_for(BUTTON_BIT_NEXT, 1000U);
  harness_pins_for(0U, 100U);
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT(is_list());
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(is_main_on_free_training());
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/* AC-T59 (2), D-T65: the C start-up values give RX and Learned. It runs before any test changes them. */
static void test_startup_values(void) {
  fake_iap_reset();
  harness_power_on();
  open_config();
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_START, false));
}

/* V-208, AC-T59 (2): the first opening, with the defaults RX and Learned. */
static void test_v208_open(void) {
  power_on_with(0U, 0U);
  open_list();
  step_dirty(BUTTON_EVENT_ENTER, (uint8_t)(PAGE(0) | PAGE(1) | PAGE(2) | PAGE(7)));
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_START, false));
  TEST_ASSERT(margins_off());
  TEST_ASSERT_EQUAL(0U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_call_count());
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/* V-209: the focus steps wrap at a press, with the pages of UX-87. */
static void test_v209_focus(void) {
  power_on_with(0U, 0U);
  open_config();
  step_dirty(BUTTON_EVENT_NEXT, (uint8_t)(PAGE(1) | PAGE(7)));
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_MODE, false));
  step_dirty(BUTTON_EVENT_NEXT, (uint8_t)(PAGE(1) | PAGE(2)));
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_CHARS, false));
  step_dirty(BUTTON_EVENT_NEXT, (uint8_t)(PAGE(2) | PAGE(7)));
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_START, false));
  step_dirty(BUTTON_EVENT_PREV, (uint8_t)(PAGE(2) | PAGE(7)));
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_CHARS, false));
  step_dirty(BUTTON_EVENT_PREV, (uint8_t)(PAGE(1) | PAGE(2)));
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_MODE, false));
  step_dirty(BUTTON_EVENT_PREV, (uint8_t)(PAGE(1) | PAGE(7)));
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_START, false));
  TEST_ASSERT(margins_off());
}

/* V-210: a repeat stops at Mode and at Start; the long event changes nothing. */
static void test_v210_repeat(void) {
  power_on_with(0U, 0U);
  open_config();
  step(BUTTON_EVENT_NEXT);
  /* The press comes after the debounce, the first repeat 400 ms after the press. */
  harness_pins_for(BUTTON_BIT_NEXT, 450U);
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_START, false));
  harness_mark_frame();
  harness_pins_for(BUTTON_BIT_NEXT, 650U);
  harness_pins_for(0U, 1000U);
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_START, false));
  harness_pins_for(BUTTON_BIT_PREV, 1100U);
  harness_pins_for(0U, 100U);
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_MODE, false));
}

/* V-211: edit mode of Mode with the markers; a step at a limit changes nothing. */
static void test_v211_mode_edit(void) {
  power_on_with(0U, 0U);
  open_config();
  step(BUTTON_EVENT_NEXT);
  step_dirty(BUTTON_EVENT_ENTER, PAGE(1));
  TEST_ASSERT(edit_row_is(1, "Mode", "RX", 2, false, true));
  step_dirty(BUTTON_EVENT_PREV, 0U);
  step_dirty(BUTTON_EVENT_NEXT, PAGE(1));
  TEST_ASSERT(edit_row_is(1, "Mode", "TX", 2, true, false));
  TEST_ASSERT(menu_row_is(2, "Chars", "Learned L1", false));
  step_dirty(BUTTON_EVENT_NEXT, 0U);
  step_dirty(BUTTON_EVENT_ENTER, PAGE(1));
  TEST_ASSERT(is_config("TX", "Learned L1", FOCUS_MODE, false));
}

/* V-212, UX-82: Learned shows the level of the mode at its own speed and follows a Mode step. */
static void test_v212_learned_follows_mode(void) {
  power_on_with(30U, 0U);
  open_config();
  TEST_ASSERT(is_config("RX", "Learned L31", FOCUS_START, false));
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step_dirty(BUTTON_EVENT_NEXT, (uint8_t)(PAGE(1) | PAGE(2)));
  TEST_ASSERT(edit_row_is(1, "Mode", "TX", 2, true, false));
  TEST_ASSERT(menu_row_is(2, "Chars", "Learned L1", false));
  step_dirty(BUTTON_EVENT_PREV, (uint8_t)(PAGE(1) | PAGE(2)));
  TEST_ASSERT(menu_row_is(2, "Chars", "Learned L31", false));
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(is_config("RX", "Learned L31", FOCUS_MODE, false));
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/* V-213: edit mode of Chars (fw 14); a Mode step keeps Full. */
static void test_v213_chars_edit(void) {
  power_on_with(0U, 0U);
  open_config();
  step(BUTTON_EVENT_PREV);
  step_dirty(BUTTON_EVENT_ENTER, PAGE(2));
  TEST_ASSERT(edit_row_is(2, "Chars", "Learned L1", 14, false, true));
  step_dirty(BUTTON_EVENT_NEXT, PAGE(2));
  TEST_ASSERT(edit_row_is(2, "Chars", "Full", 14, true, false));
  step_dirty(BUTTON_EVENT_NEXT, 0U);
  step(BUTTON_EVENT_PREV);
  TEST_ASSERT(edit_row_is(2, "Chars", "Learned L1", 14, false, true));
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_config("RX", "Full", FOCUS_CHARS, false));
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step_dirty(BUTTON_EVENT_NEXT, PAGE(1));
  TEST_ASSERT(menu_row_is(2, "Chars", "Full", false));
}

/*
 * V-214, AC-T59 (2), D-T65: Mode and Chars stay in RAM across a leave and a long menu/back. A host power-on keeps
 * the RAM, so test_v208_open() checks the start-up values, before any test changes them.
 */
static void test_v214_values_kept(void) {
  power_on_with(0U, 0U);
  open_config();
  select_tx();
  select_full();
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(is_list());
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_config("TX", "Full", FOCUS_START, false));
  step(LONG_EVENT(BUTTON_EVENT_MENU));
  TEST_ASSERT(is_list());
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_config("TX", "Full", FOCUS_START, false));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE));
}

/* V-215, UX-83: the message, its redraw rules and its clear by a value step. */
static void test_v215_no_digit(void) {
  power_on_with(0U, 0U);
  open_config();
  step_dirty(BUTTON_EVENT_ENTER, (uint8_t)(PAGE(4) | PAGE(5)));
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_START, true));
  TEST_ASSERT_EQUAL(0U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  step_dirty(BUTTON_EVENT_ENTER, 0U);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_message_shown());
  step_dirty(BUTTON_EVENT_NEXT, (uint8_t)(PAGE(1) | PAGE(4) | PAGE(5)));
  TEST_ASSERT(body_off(4, 5));
  TEST_ASSERT(menu_row_is(2, "Chars", "Learned L1", false));
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_config("TX", "Learned L1", FOCUS_START, true));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
}

/* V-216, AC-T57 (1): an RX session with Full: the header, the keyboard of 40 characters, the first item. */
static void test_v216_rx_start(void) {
  power_on_with(0U, 0U);
  open_config();
  select_full();
  harness_set_time(HARNESS_T0);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(hdr("RX 32/32 Callsigns   1/20"));
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(body_off(2, 5));
  TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, keyboard_count());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  harness_run_to(HARNESS_T0 + TRAINING_START_MS);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT(is_callsign_item());
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  TEST_ASSERT_EQUAL(32U, fake_player_last_wpm());
  TEST_ASSERT_EQUAL(32U, fake_player_last_eff_wpm());
  TEST_ASSERT_EQUAL(0, strcmp(fake_player_last_text(), training_item_text()));
  TEST_ASSERT(hdr("RX 32/32 Callsigns   1/20"));
  TEST_ASSERT_EQUAL(0U, fake_koch_item_count());
}

/* V-217, V-225, AC-T59 (3): Learned starts from level index 24 (L25) and blocks at 23, at the speed of each mode. */
static void check_learned_edge(bool is_tx, uint16_t level, bool is_started) {
  char text[16];

  power_on_with(is_tx ? 0U : level, is_tx ? level : 0U);
  open_config();
  if (is_tx) {
    select_tx();
  }
  (void)snprintf(text, sizeof(text), "Learned L%u", (unsigned)(level + 1U));
  TEST_ASSERT(menu_row_is(2, "Chars", text, false));
  step(BUTTON_EVENT_ENTER);
  if (is_started) {
    TEST_ASSERT(training_phase() == TRAINING_PHASE_GAP);
    TEST_ASSERT_EQUAL(18U, training_alphabet_count());
    TEST_ASSERT(hdr(is_tx ? "TX 20/20 Callsigns   1/20" : "RX 32/32 Callsigns   1/20"));
  } else {
    TEST_ASSERT(is_config(is_tx ? "TX" : "RX", text, FOCUS_START, true));
  }
}

static void test_v217_v225_learned_edge(void) {
  check_learned_edge(false, 24U, true);
  check_learned_edge(false, 23U, false);
  check_learned_edge(true, 24U, true);
  check_learned_edge(true, 23U, false);
  /* RX index 30 and TX index 0: RX starts, TX blocks. */
  power_on_with(30U, 0U);
  open_config();
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(koch_learned_count(30U), training_alphabet_count());
  step(LONG_EVENT(BUTTON_EVENT_MENU));
  step(BUTTON_EVENT_ENTER);
  select_tx();
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_config("TX", "Learned L1", FOCUS_START, true));
  /* Full starts at index 0. */
  select_full();
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, training_alphabet_count());
}

/* F2: level 65535 gives 43 learned characters; the set is the 40 word characters. */
static void test_learned_cap(void) {
  power_on_with(65534U, 0U);
  open_config();
  TEST_ASSERT(menu_row_is(2, "Chars", "Learned L65535", false));
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(edit_row_is(2, "Chars", "Learned L65535", 14, false, true));
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, training_alphabet_count());
  TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, keyboard_count());
}

/* V-218, AC-T58 (1): a TX session starts the keyer once with the TX settings and shows the TX header. */
static void test_v218_tx_start(void) {
  power_on_with(0U, 0U);
  open_config();
  select_tx();
  select_full();
  harness_set_time(HARNESS_T0);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(hdr("TX 20/20 Callsigns   1/20"));
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(20U, fake_keyer_last_wpm());
  TEST_ASSERT_EQUAL(20U, fake_keyer_last_eff_wpm());
  TEST_ASSERT_EQUAL(SETTINGS_KEY_IAMBIC_B, fake_keyer_last_key_type());
  TEST_ASSERT(body_off(7, 7));
}

/* V-219, V-220, AC-T57 (2) to (4), AC-T60: an RX session to the free end screen, then enter. */
static void test_v219_v220_rx_session(void) {
  unsigned item;

  power_on_with(0U, 0U);
  open_config();
  select_full();
  step(BUTTON_EVENT_ENTER);
  for (item = 1U; item <= TRAINING_FREE_ITEMS; ++item) {
    rx_answer(item > 17U);
    TEST_ASSERT(is_callsign_item());
  }
  run_to_phase(TRAINING_PHASE_END);
  TEST_ASSERT(hdr("RX 32/32 Callsigns  20/20"));
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(text_at(2, 7, "Score 17/20"));
  TEST_ASSERT(body_off(3, 6));
  TEST_ASSERT(ok_button());
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FREE, training_outcome());
  TEST_ASSERT(has_no_iap_write());
  TEST_ASSERT_EQUAL(0U, fake_koch_item_count());
  /* V-220 run 1: the other events change nothing. */
  step_dirty(BUTTON_EVENT_PREV, 0U);
  step_dirty(REPEAT_EVENT(BUTTON_EVENT_NEXT), 0U);
  step_dirty(LONG_EVENT(BUTTON_EVENT_PREV), 0U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  step_dirty(BUTTON_EVENT_ENTER, (uint8_t)(PAGE(0) | PAGE(1) | PAGE(2) | PAGE(7)));
  TEST_ASSERT(is_config("RX", "Full", FOCUS_START, false));
  TEST_ASSERT(margins_off());
  TEST_ASSERT(!fake_player_is_running());
  /* The next session starts from the configuration screen. */
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
}

/* V-220 runs 2 and 3, UX-86: menu/back on the end screen opens the list; its long event does nothing more. */
static void test_v220_end_menu(void) {
  unsigned item;

  power_on_with(0U, 0U);
  open_config();
  select_tx();
  select_full();
  step(BUTTON_EVENT_ENTER);
  for (item = 1U; item <= TRAINING_FREE_ITEMS; ++item) {
    tx_answer();
  }
  run_to_phase(TRAINING_PHASE_END);
  TEST_ASSERT(hdr("TX 20/20 Callsigns  20/20"));
  TEST_ASSERT(text_at(2, 7, "Score 20/20"));
  TEST_ASSERT(ok_button());
  TEST_ASSERT(!fake_keyer_is_running());
  TEST_ASSERT(has_no_iap_write());
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(is_list());
  step_dirty(LONG_EVENT(BUTTON_EVENT_MENU), 0U);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_config("TX", "Full", FOCUS_START, false));
}

/* V-222, FT-9, F9: a contact held across the session end opens nothing; each new press acts once. */
static void test_v222_held_contact(void) {
  char item[TRAINING_TEXT_SIZE];
  unsigned number;
  uint8_t index;

  power_on_with(0U, 0U);
  open_config();
  select_tx();
  select_full();
  step(BUTTON_EVENT_ENTER);
  for (number = 1U; number < TRAINING_FREE_ITEMS; ++number) {
    tx_answer();
  }
  run_to_phase(TRAINING_PHASE_ITEM);
  memcpy(item, training_item_text(), sizeof(item));
  for (index = 0U; item[index + 1U] != '\0'; ++index) {
    key_char(item[index]);
  }
  harness_pins = BUTTON_BIT_ENTER;
  fake_button_pins_set(harness_pins);
  key_char(item[index]);
  run_to_phase(TRAINING_PHASE_END);
  harness_pins_for(BUTTON_BIT_ENTER, 2000U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT(ok_button());
  harness_pins_for(0U, 100U);
  harness_pins_for(BUTTON_BIT_ENTER, 100U);
  TEST_ASSERT(is_config("TX", "Full", FOCUS_START, false));
  harness_pins_for(0U, 100U);
  harness_pins_for(BUTTON_BIT_ENTER, 50U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT(hdr("TX 20/20 Callsigns   1/20"));
}

/* V-221, AC-T59 (5), D-T87: a long menu/back in each session state opens the list and stops the session. */
static void check_long_back(bool is_tx, training_phase_t phase) {
  power_on_with(0U, 0U);
  open_config();
  if (is_tx) {
    select_tx();
  }
  select_full();
  step(BUTTON_EVENT_ENTER);
  if (phase != TRAINING_PHASE_GAP) {
    run_to_phase(TRAINING_PHASE_ITEM);
  }
  if (phase == TRAINING_PHASE_RESULT) {
    if (is_tx) {
      key_char('.');
    } else {
      char item[TRAINING_TEXT_SIZE];
      uint8_t index;

      memcpy(item, training_item_text(), sizeof(item));
      for (index = 0U; item[index] != '\0'; ++index) {
        pick((index == 0U) ? '.' : item[index]);
      }
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  }
  TEST_ASSERT_EQUAL(phase, training_phase());
  step(LONG_EVENT(BUTTON_EVENT_MENU));
  TEST_ASSERT(is_list());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
  TEST_ASSERT(!fake_player_is_running());
  TEST_ASSERT(!fake_keyer_is_running());
  TEST_ASSERT(has_no_iap_write());
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_config(is_tx ? "TX" : "RX", "Full", FOCUS_START, false));
}

static void test_v221_long_back(void) {
  check_long_back(false, TRAINING_PHASE_GAP);
  check_long_back(false, TRAINING_PHASE_ITEM);
  check_long_back(false, TRAINING_PHASE_RESULT);
  check_long_back(true, TRAINING_PHASE_GAP);
  check_long_back(true, TRAINING_PHASE_ITEM);
  check_long_back(true, TRAINING_PHASE_RESULT);
  TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
  /* Edit mode: the press ends it, the long event leaves. */
  power_on_with(0U, 0U);
  open_config();
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_MODE, false));
  step(LONG_EVENT(BUTTON_EVENT_MENU));
  TEST_ASSERT(is_list());
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_config("RX", "Learned L1", FOCUS_START, false));
}

/* V-223, AC-T60: free sessions never change the levels; Reset Statistics keeps Mode and Chars. */
static void test_v223_no_progress_change(void) {
  unsigned item;
  uint8_t record[PROGRESS_RECORD_SIZE];

  power_on_with(30U, 5U);
  memcpy(record, &fake_iap_memory()[0x0200U], sizeof(record));
  open_config();
  step(BUTTON_EVENT_ENTER);
  for (item = 1U; item <= TRAINING_FREE_ITEMS; ++item) {
    run_to_phase(TRAINING_PHASE_ITEM);
    step(LONG_EVENT(BUTTON_EVENT_NEXT));
    step(BUTTON_EVENT_ENTER);
  }
  run_to_phase(TRAINING_PHASE_END);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FREE, training_outcome());
  TEST_ASSERT(text_at(2, 7, "Score 0/20"));
  step(BUTTON_EVENT_ENTER);
  select_tx();
  select_full();
  step(BUTTON_EVENT_ENTER);
  for (item = 1U; item <= TRAINING_FREE_ITEMS; ++item) {
    tx_answer();
  }
  run_to_phase(TRAINING_PHASE_END);
  TEST_ASSERT(text_at(2, 7, "Score 20/20"));
  step(BUTTON_EVENT_MENU);
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(menu_row_is(1, "Receiving", "L31", false));
  TEST_ASSERT(menu_row_is(2, "Sending", "L6", false));
  TEST_ASSERT(is_main_on_free_training());
  TEST_ASSERT(has_no_iap_write());
  TEST_ASSERT_EQUAL(0, memcmp(record, &fake_iap_memory()[0x0200U], sizeof(record)));
  TEST_ASSERT_EQUAL(30U, progress_level(PROGRESS_MODE_RECEIVING, RX_WPM));
  TEST_ASSERT_EQUAL(5U, progress_level(PROGRESS_MODE_SENDING, TX_WPM));
  /* The menu tests link a fake stats_reset(); the real one is progress_reset(). */
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_reset());
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_config("TX", "Full", FOCUS_START, false));
}

/* D-T86, M10: only Receiving and Sending show a level; Callsigns shows none in its list. */
static void test_tree(void) {
  const menu_node_t MEM_CODE* main_node = &menu_nodes[MENU_NODE_MAIN];
  const menu_node_t MEM_CODE* free_node = &menu_nodes[MENU_NODE_FREE_TRAINING];

  TEST_ASSERT_EQUAL(6U, MENU_NODE_COUNT);
  TEST_ASSERT_EQUAL(6U, main_node->item_count);
  TEST_ASSERT_EQUAL(0, strcmp(main_node->items[2].label, "Free Training"));
  TEST_ASSERT_EQUAL(MENU_KIND_SUBMENU, main_node->items[2].kind);
  TEST_ASSERT_EQUAL(MENU_NODE_FREE_TRAINING, main_node->items[2].target);
  TEST_ASSERT_EQUAL(0, strcmp(main_node->items[3].label, "Free Input"));
  TEST_ASSERT_EQUAL(0, strcmp(main_node->items[4].label, "Settings"));
  TEST_ASSERT_EQUAL(0, strcmp(main_node->items[5].label, "About"));
  TEST_ASSERT_EQUAL(0, strcmp(free_node->title, "Free Training"));
  TEST_ASSERT_EQUAL(1U, free_node->item_count);
  TEST_ASSERT(!free_node->is_save_on_exit);
  TEST_ASSERT_EQUAL(0, strcmp(free_node->items[0].label, "Callsigns"));
  TEST_ASSERT_EQUAL(MENU_KIND_MODE, free_node->items[0].kind);
  TEST_ASSERT_EQUAL(MENU_MODE_CALLSIGNS, free_node->items[0].target);
}

/*
 * UX-87, C-1: a random walk on the configuration screen. After each event, exactly the pages whose bytes changed are
 * dirty, and the frame equals a full render of the same state.
 */
static void test_config_walk(void) {
  static const button_event_t events[] = {
      BUTTON_EVENT_PREV,
      BUTTON_EVENT_NEXT,
      BUTTON_EVENT_ENTER,
      BUTTON_EVENT_MENU,
      REPEAT_EVENT(BUTTON_EVENT_PREV),
      REPEAT_EVENT(BUTTON_EVENT_NEXT),
      LONG_EVENT(BUTTON_EVENT_PREV),
      LONG_EVENT(BUTTON_EVENT_NEXT),
  };
  static uint8_t incremental[LCD_PAGE_COUNT * LCD_WIDTH];
  unsigned walk_step;
  unsigned seed = 77U;
  uint8_t page;

  power_on_with(0U, 0U);
  open_config();
  for (walk_step = 0U; walk_step < 3000U; ++walk_step) {
    seed = (seed * 1103515245U) + 12345U;
    step_dirty_free(events[(seed >> 16) % (sizeof(events) / sizeof(events[0]))]);
    if (menu_screen() != MENU_SCREEN_FREE_TRAINING) {
      step(BUTTON_EVENT_ENTER);
      continue;
    }
    if (training_phase() != TRAINING_PHASE_IDLE) {
      step(LONG_EVENT(BUTTON_EVENT_MENU));
      step(BUTTON_EVENT_ENTER);
      continue;
    }
    for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
      memcpy(&incremental[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
    }
    menu_render();
    for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
      if (!TEST_ASSERT_EQUAL(0, memcmp(&incremental[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH))) {
        fprintf(stderr, "walk step %u page %u\n", walk_step, (unsigned)page);
      }
    }
  }
  TEST_ASSERT(has_no_iap_write());
}

/* F5: Receiving from the main menu after a free session gets Koch items and the start screen. */
static void test_koch_after_free(void) {
  power_on_with(0U, 0U);
  open_config();
  select_full();
  step(BUTTON_EVENT_ENTER);
  run_to_phase(TRAINING_PHASE_ITEM);
  step(LONG_EVENT(BUTTON_EVENT_MENU));
  step(BUTTON_EVENT_MENU);
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_PREV);
  fake_koch_item_push("K");
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT(!training_is_free());
  TEST_ASSERT(hdr("RX 32/32 L1          1/50"));
}

int main(void) {
  TEST_RUN(test_startup_values);
  TEST_RUN(test_tree);
  TEST_RUN(test_v207_list);
  TEST_RUN(test_v208_open);
  TEST_RUN(test_v209_focus);
  TEST_RUN(test_v210_repeat);
  TEST_RUN(test_v211_mode_edit);
  TEST_RUN(test_v212_learned_follows_mode);
  TEST_RUN(test_v213_chars_edit);
  TEST_RUN(test_v214_values_kept);
  TEST_RUN(test_v215_no_digit);
  TEST_RUN(test_v216_rx_start);
  TEST_RUN(test_v217_v225_learned_edge);
  TEST_RUN(test_learned_cap);
  TEST_RUN(test_v218_tx_start);
  TEST_RUN(test_v219_v220_rx_session);
  TEST_RUN(test_v220_end_menu);
  TEST_RUN(test_v222_held_contact);
  TEST_RUN(test_v221_long_back);
  TEST_RUN(test_v223_no_progress_change);
  TEST_RUN(test_config_walk);
  TEST_RUN(test_koch_after_free);
  return TEST_FINISH();
}
