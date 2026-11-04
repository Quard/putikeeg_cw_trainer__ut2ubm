/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of Free Training and the Callsigns configuration screen at the menu seam (training_spec.md sections
 * 23.5, 23.6 and 23.10 Q9: AC-T59, AC-T60, Q-C1 to Q-C3, Q-C6, F1, F4 to F6, M5, M10, M11, M13, M16, M17;
 * ui-ux.md 5.21 and 9.8: V-207 to V-215, V-217, V-223 to V-225). The expected frames are drawn here from the
 * layout of ui-ux.md 5.21.2 with the golden font, and every pixel of the framebuffer is compared.
 */

#include "koch/keyboard.h"
#include "koch/koch.h"
#include "menu_internal.h"
#include "modes/training.h"
#include "support/menu_frame.h"
#include "support/training_harness.h"

#define RX_WPM 32U /* the default RX Speed */
#define TX_WPM 20U /* the default TX Speed */
#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define FOCUS_MODE 0
#define FOCUS_CHARS 1
#define FOCUS_START 2
#define MODE_FIELD 2
#define CHARS_FIELD 14
#define FIRST_POSSIBLE_KEYS 18U /* learned characters at index 24 (shown L25) */
#define OPEN_PAGES ((uint8_t)(PAGE(0) | PAGE(1) | PAGE(2) | PAGE(7)))
#define MESSAGE_PAGES ((uint8_t)(PAGE(4) | PAGE(5)))

/* ---------- Expected frames (ui-ux.md 5.21.2, 5.21.3, 9.8) ---------- */

typedef struct {
  const char* mode;  /* "RX" or "TX" */
  const char* chars; /* "Learned L<n>" or "Full" */
  int focus;
  bool is_edit;
  bool has_message;
} config_t;

/* EDIT(r, fw) of ui-ux.md 9: the label in normal video, the band from x 106 - 5fw, the value and the markers. */
static void expect_edit_row(int r, const char* label, const char* value, int field_width, bool is_first) {
  const int band_x = 106 - 5 * field_width;
  frame_fill(band_x, 8 * r + 1, FRAME_WIDTH - band_x, 7, true);
  frame_text5(FRAME_CELL_X(0), 8 * r, label, false);
  frame_text5(FRAME_CELL_X(23 - (int)strlen(value)), 8 * r, value, true);
  if (is_first) {
    frame_text5(FRAME_CELL_X(24), 8 * r, ">", true);
  } else {
    frame_text5(FRAME_CELL_X(21 - field_width), 8 * r, "<", true);
  }
  frame_fill(band_x, 8 * r, FRAME_WIDTH - band_x, 1, false);
}

/* STARTBTN, or STARTTXT. */
static void expect_start(bool is_focused) {
  if (is_focused) {
    frame_fill(47, 57, 35, 7, true);
    frame_text5(FRAME_CELL_X(10), 56, "Start", true);
    frame_fill(47, 56, 35, 1, false);
  } else {
    frame_text5(FRAME_CELL_X(10), 56, "Start", false);
  }
}

/* CFGV(m, c, f) with the edit field and NODIGIT of ui-ux.md 9.8. */
static void expect_config(const config_t* config) {
  frame_clear();
  frame_title("Callsigns");
  if (config->is_edit && (config->focus == FOCUS_MODE)) {
    expect_edit_row(1, "Mode", config->mode, MODE_FIELD, strcmp(config->mode, "RX") == 0);
  } else {
    frame_list_row(1, "Mode", config->mode, false, config->focus == FOCUS_MODE);
  }
  if (config->is_edit && (config->focus == FOCUS_CHARS)) {
    expect_edit_row(2, "Chars", config->chars, CHARS_FIELD, strcmp(config->chars, "Full") != 0);
  } else {
    frame_list_row(2, "Chars", config->chars, false, config->focus == FOCUS_CHARS);
  }
  if (config->has_message) {
    frame_text5(FRAME_CELL_X(2), 32, "No digit learned yet", false);
    frame_text5(FRAME_CELL_X(2), 40, "Use Full or reach L25", false);
  }
  expect_start(config->focus == FOCUS_START);
}

static bool config_is(const char* mode, const char* chars, int focus, bool is_edit, bool has_message) {
  const config_t config = {mode, chars, focus, is_edit, has_message};
  expect_config(&config);
  return frame_matches("CFG") && (menu_screen() == MENU_SCREEN_FREE_TRAINING) &&
         (training_phase() == TRAINING_PHASE_IDLE);
}

/* FTLIST: the title and the one row, with no value and no marker (M10). */
static bool list_is(void) {
  frame_clear();
  frame_title("Free Training");
  frame_list_row(1, "Callsigns", NULL, false, true);
  return frame_matches("FTLIST") && (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_FREE_TRAINING) &&
         (menu_cursor() == 0U);
}

/* MAIN of ui-ux.md 5.21.1 with the levels @p rx and @p tx and the cursor on item @p selected. */
static bool main_is(const char* rx, const char* tx, int selected) {
  frame_clear();
  frame_title("CW Trainer");
  frame_list_row(1, "Receiving", rx, false, selected == 0);
  frame_list_row(2, "Sending", tx, false, selected == 1);
  frame_list_row(3, "Free Training", NULL, true, selected == 2);
  frame_list_row(4, "Free Input", NULL, false, selected == 3);
  frame_list_row(5, "Settings", NULL, true, selected == 4);
  frame_list_row(6, "About", NULL, false, selected == 5);
  return frame_matches("MAIN") && (menu_node() == MENU_NODE_MAIN) && (menu_cursor() == (uint8_t)selected);
}

/* ---------- Drivers ---------- */

static void step(button_event_t event) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(event);
}

/* A step that must dirty exactly @p mask, and @p mask must be the pages whose bytes changed (UX-70, UX-87). */
static bool step_dirty(button_event_t event, uint8_t mask) {
  bool is_ok;
  harness_mark_frame();
  step(event);
  is_ok = TEST_ASSERT(dirty_is(mask));
  return TEST_ASSERT(dirty_matches_change()) && is_ok;
}

static void open_list(void) {
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
}

static void open_config(void) {
  open_list();
  step(BUTTON_EVENT_ENTER);
}

/* From the focus on Start: sets Mode and Chars through the edit fields, then the focus is on Start again. */
static void set_values(bool is_tx, bool is_full) {
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(is_tx ? BUTTON_EVENT_NEXT : BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  step(is_full ? BUTTON_EVENT_NEXT : BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
}

/*
 * Power-on with the levels of EARN (0 means no record) and the default speeds. Mode and Chars keep their RAM values
 * across a host power-on, so @p is_tx and @p is_full set them, and the test starts on the main menu again.
 */
static void power_on(uint16_t rx_level, uint16_t tx_level, bool is_tx, bool is_full) {
  fake_iap_reset();
  if ((rx_level != 0U) || (tx_level != 0U)) {
    progress_fixture_write(PROGRESS_MODE_RECEIVING, RX_WPM, rx_level);
    progress_fixture_add(PROGRESS_MODE_SENDING, TX_WPM, tx_level);
  }
  harness_power_on();
  open_config();
  set_values(is_tx, is_full);
  harness_power_on();
  fake_koch_item_reset();
}

static bool has_no_iap_write(void) {
  return (fake_iap_command_count(CMD_PROGRAM) == 0U) && (fake_iap_command_count(CMD_ERASE) == 0U);
}

/* Passes, one per ms, until the engine is in @p phase. */
static bool run_to_phase(training_phase_t phase) {
  unsigned guard;
  for (guard = 0U; (training_phase() != phase) && (guard < 30000U); ++guard) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
  }
  return training_phase() == phase;
}

/* Tap mode is off: a next press changes the screen while the button is still held (tap mode acts at the release). */
static bool next_acts_at_press(void) {
  bool has_changed;
  harness_mark_frame();
  harness_pins_for(BUTTON_BIT_NEXT, 60U);
  has_changed = harness_changed_pages() != 0U;
  harness_pins_for(0U, 60U);
  return has_changed;
}

/* ---------- RX answers through the group keyboard ---------- */

static const button_event_t group_taps[KEYBOARD_GROUP_COUNT] = {L_TAP, M_TAP, R_TAP, ENTER_EVENT};

static void pick(char character) {
  uint8_t target = 0xFFU;
  uint8_t index;
  unsigned depth;
  for (index = 0U; index < keyboard_count(); ++index) {
    if (keyboard_char(index) == character) {
      target = index;
    }
  }
  if (!TEST_ASSERT(target != 0xFFU)) {
    return;
  }
  for (depth = 0U; depth < KEYBOARD_DEPTH_MAX; ++depth) {
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

/* Answers the current RX item right, at once. */
static void rx_answer_right(void) {
  char item[TRAINING_TEXT_SIZE];
  uint8_t index;
  if (!TEST_ASSERT(run_to_phase(TRAINING_PHASE_ITEM))) {
    return;
  }
  memcpy(item, (const char*)training_item_text(), sizeof(item));
  for (index = 0U; item[index] != '\0'; ++index) {
    pick(item[index]);
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
}

/* A whole RX free session: all items right, to the end screen. */
static void rx_session_to_end(void) {
  unsigned item;
  for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
    rx_answer_right();
  }
  TEST_ASSERT(run_to_phase(TRAINING_PHASE_END));
}

/* ---------- Tests ---------- */

/*
 * Q-C6, V-208: the first entry of the test process is the power-on case: RX and Learned. The step from the list
 * dirties exactly pages 0, 1, 2 and 7. Nothing runs: no player, keyer, sidetone or EEPROM access; tap mode is off.
 * It must run before any test changes Mode or Chars.
 */
static void test_first_entry_has_defaults(void) {
  fake_iap_reset();
  harness_power_on();
  TEST_ASSERT(main_is("L1", "L1", 0));
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT(main_is("L1", "L1", 2));
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(list_is());
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, OPEN_PAGES));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_START, false, false));
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_call_count());
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
  TEST_ASSERT(next_acts_at_press());
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_MODE, false, false));
}

/*
 * V-207, M10, UX-79: the main menu with levels, the Free Training list with no level on Callsigns, the list ignores
 * prev, next and a held next, menu/back goes to the main menu on Free Training with no EEPROM access.
 */
static void test_menu_and_list(void) {
  power_on(30U, 5U, false, false);
  TEST_ASSERT(main_is("L31", "L6", 0));
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT(main_is("L31", "L6", 2));
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(list_is());
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, 0U));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, 0U));
  harness_mark_frame();
  harness_pins_for(BUTTON_BIT_NEXT, 1000U);
  harness_pins_for(0U, 100U);
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT(list_is());
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(main_is("L31", "L6", 2));
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/*
 * V-209, Q-C1, Q-C2, Q-C3: every focus step, the wrap from Start to Mode both ways, the exact frame (rows with x 0,
 * x 1, x 127 and the gap line) and the exact dirty pages. No band pixel stays after the focus leaves a row or Start.
 */
static void test_focus_steps(void) {
  power_on(0U, 0U, false, false);
  open_config();
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_START, false, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(1) | PAGE(7)));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_MODE, false, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(1) | PAGE(2)));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_CHARS, false, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(2) | PAGE(7)));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_START, false, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(2) | PAGE(7)));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_CHARS, false, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(1) | PAGE(2)));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_MODE, false, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(1) | PAGE(7)));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_START, false, false));
  /* The same with TX and Full: the value widths differ. */
  set_values(true, true);
  TEST_ASSERT(config_is("TX", "Full", FOCUS_START, false, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(2) | PAGE(7)));
  TEST_ASSERT(config_is("TX", "Full", FOCUS_CHARS, false, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(1) | PAGE(2)));
  TEST_ASSERT(config_is("TX", "Full", FOCUS_MODE, false, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(1) | PAGE(7)));
  TEST_ASSERT(config_is("TX", "Full", FOCUS_START, false, false));
  /* Long prev and long next do nothing (UX-24). */
  TEST_ASSERT(step_dirty(LONG_EVENT(BUTTON_EVENT_PREV), 0U));
  TEST_ASSERT(step_dirty(LONG_EVENT(BUTTON_EVENT_NEXT), 0U));
  TEST_ASSERT(config_is("TX", "Full", FOCUS_START, false, false));
}

/* V-210: a held next walks Mode -> Chars -> Start and stops; a held prev walks back to Mode and stops. */
static void test_repeat_stops_at_the_ends(void) {
  uint16_t start;
  power_on(0U, 0U, false, false);
  open_config();
  step(BUTTON_EVENT_NEXT);
  harness_run_to((uint16_t)(harness_now + 100U));
  start = harness_now;
  fake_button_pins_set(BUTTON_BIT_NEXT);
  harness_run_to((uint16_t)(start + 100U));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_CHARS, false, false));
  harness_run_to((uint16_t)(start + 470U));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_START, false, false));
  harness_mark_frame();
  harness_run_to((uint16_t)(start + 1000U));
  fake_button_pins_set(0U);
  harness_run_to((uint16_t)(start + 1100U));
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_START, false, false));
  harness_run_to((uint16_t)(start + 2000U));
  fake_button_pins_set(BUTTON_BIT_PREV);
  harness_run_to((uint16_t)(start + 2100U));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_CHARS, false, false));
  harness_run_to((uint16_t)(start + 2470U));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_MODE, false, false));
  harness_mark_frame();
  harness_run_to((uint16_t)(start + 3000U));
  fake_button_pins_set(0U);
  harness_run_to((uint16_t)(start + 3100U));
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_MODE, false, false));
}

/*
 * V-211, V-212, V-213, Q-C1, Q-C2: the edit fields of Mode (fw 2) and Chars (fw 14), the markers, the steps that stop
 * at the limits, the Mode step with two equal levels (page 1 only) and with two different levels (pages 1 and 2),
 * and menu/back that ends edit mode and stays on the screen.
 */
static void test_edit_fields(void) {
  power_on(0U, 0U, false, false);
  open_config();
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, PAGE(1)));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_MODE, true, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, 0U));
  TEST_ASSERT(step_dirty(REPEAT_EVENT(BUTTON_EVENT_PREV), 0U));
  /* RX and TX are both at index 0: the Learned level does not change, so page 2 stays clean. */
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(1)));
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_MODE, true, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, 0U));
  TEST_ASSERT(step_dirty(REPEAT_EVENT(BUTTON_EVENT_NEXT), 0U));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, PAGE(1)));
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_MODE, false, false));
  TEST_ASSERT(step_dirty(LONG_EVENT(BUTTON_EVENT_NEXT), 0U));

  /* V-213: the Chars field. */
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, PAGE(2)));
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_CHARS, true, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(2)));
  TEST_ASSERT(config_is("TX", "Full", FOCUS_CHARS, true, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, 0U));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(2)));
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_CHARS, true, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(2)));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_MENU, PAGE(2)));
  TEST_ASSERT(config_is("TX", "Full", FOCUS_CHARS, false, false));
  /* With Full, a Mode step changes only row 1. */
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(1)));
  TEST_ASSERT(config_is("RX", "Full", FOCUS_MODE, true, false));
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());

  /* V-212: RX index 30 and TX index 0 show different levels in the same redraw. */
  power_on(30U, 0U, false, false);
  open_config();
  TEST_ASSERT(config_is("RX", "Learned L31", FOCUS_START, false, false));
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(1) | PAGE(2)));
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_MODE, true, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(1) | PAGE(2)));
  TEST_ASSERT(config_is("RX", "Learned L31", FOCUS_MODE, true, false));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_MENU, PAGE(1)));
  TEST_ASSERT(config_is("RX", "Learned L31", FOCUS_MODE, false, false));
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/*
 * V-215, V-225 text, Q-C1: the blocked start shows the message on pages 4 and 5; a second blocked start changes
 * nothing; a focus step and the start of edit mode keep it; a value step clears it (Mode with equal levels: pages 1,
 * 4, 5; Mode with different levels: pages 1, 2, 4, 5; Chars: pages 2, 4, 5).
 */
static void test_no_digit_message(void) {
  power_on(0U, 0U, false, false);
  open_config();
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, MESSAGE_PAGES));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_START, false, true));
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, 0U));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(1) | PAGE(7)));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_MODE, false, true));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, PAGE(1)));
  TEST_ASSERT(config_is("RX", "Learned L1", FOCUS_MODE, true, true));
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(1) | MESSAGE_PAGES));
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_MODE, true, false));
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, MESSAGE_PAGES));
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_START, false, true));
  /* A Chars step clears the message. */
  step(BUTTON_EVENT_PREV);
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_CHARS, false, true));
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(step_dirty(BUTTON_EVENT_NEXT, PAGE(2) | MESSAGE_PAGES));
  TEST_ASSERT(config_is("TX", "Full", FOCUS_CHARS, true, false));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());

  /* The largest step: Mode with Learned, different levels and the message (4 pages, UX-87). */
  power_on(30U, 0U, true, false);
  open_config();
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_START, false, false));
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_START, false, true));
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(step_dirty(BUTTON_EVENT_PREV, PAGE(1) | PAGE(2) | MESSAGE_PAGES));
  TEST_ASSERT(config_is("RX", "Learned L31", FOCUS_MODE, true, false));
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/* V-224: the longest texts. Learned L65535 fills columns 9-22; in edit mode the band starts at x 36. */
static void test_longest_texts(void) {
  int x;
  int y;
  power_on(65534U, 0U, false, false);
  open_config();
  TEST_ASSERT(config_is("RX", "Learned L65535", FOCUS_START, false, false));
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(config_is("RX", "Learned L65535", FOCUS_CHARS, true, false));
  for (y = 16; y < 24; ++y) {
    for (x = 27; x <= 35; ++x) {
      TEST_ASSERT(!pixel(x, y));
    }
  }
}

/*
 * F1, V-217, V-225, M5, AC-T59 (3): Learned uses the level of the mode at the speed of that mode. RX Speed 25 and TX
 * Speed 15 differ from the defaults; the records at the default speeds hold other levels that must not count.
 */
static void test_learned_edge_at_each_speed(void) {
  unsigned run;
  for (run = 0U; run < 2U; ++run) {
    const uint16_t rx_level = (run == 0U) ? 23U : 24U;
    const uint16_t tx_level = (run == 0U) ? 24U : 23U;
    fake_iap_reset();
    progress_fixture_write(PROGRESS_MODE_RECEIVING, 25U, rx_level);
    progress_fixture_add(PROGRESS_MODE_RECEIVING, RX_WPM, (uint16_t)(47U - rx_level));
    progress_fixture_add(PROGRESS_MODE_SENDING, 15U, tx_level);
    progress_fixture_add(PROGRESS_MODE_SENDING, TX_WPM, (uint16_t)(47U - tx_level));
    harness_power_on();
    TEST_ASSERT(settings_set(SETTINGS_ID_RX_WPM, 25U));
    TEST_ASSERT(settings_set(SETTINGS_ID_TX_WPM, 15U));
    (void)settings_set(SETTINGS_ID_TX_EFF_WPM, 15U);
    open_config();
    set_values(false, false);
    TEST_ASSERT(config_is("RX", (run == 0U) ? "Learned L24" : "Learned L25", FOCUS_START, false, false));
    step(BUTTON_EVENT_ENTER);
    if (run == 0U) {
      TEST_ASSERT(config_is("RX", "Learned L24", FOCUS_START, false, true));
    } else {
      TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
      TEST_ASSERT_EQUAL(FIRST_POSSIBLE_KEYS, keyboard_count());
      TEST_ASSERT_EQUAL(FIRST_POSSIBLE_KEYS, training_alphabet_count());
      step(LONG_EVENT(BUTTON_EVENT_MENU));
      step(BUTTON_EVENT_ENTER);
    }
    set_values(true, false);
    TEST_ASSERT(config_is("TX", (run == 0U) ? "Learned L25" : "Learned L24", FOCUS_START, false, false));
    step(BUTTON_EVENT_ENTER);
    if (run == 1U) {
      TEST_ASSERT(config_is("TX", "Learned L24", FOCUS_START, false, true));
    } else {
      TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
      TEST_ASSERT_EQUAL(TRAINING_MODE_SENDING, training_mode());
      TEST_ASSERT_EQUAL(FIRST_POSSIBLE_KEYS, training_alphabet_count());
      step(LONG_EVENT(BUTTON_EVENT_MENU));
    }
  }
  /* Full starts at index 0 in both modes. The session start dirties exactly the pages whose bytes change. */
  power_on(0U, 0U, false, true);
  open_config();
  harness_mark_frame();
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(dirty_matches_change());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, training_alphabet_count());
  step(LONG_EVENT(BUTTON_EVENT_MENU));
  step(BUTTON_EVENT_ENTER);
  set_values(true, true);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, training_alphabet_count());
  step(LONG_EVENT(BUTTON_EVENT_MENU));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_PROGRAM));
}

/* F2 count, M5 cap: Learned at levels 60, 61 and 65535 (42, 43 learned characters) gives the count 40. */
static void test_learned_cap(void) {
  static const uint16_t levels[] = {59U, 60U, 61U, 65535U};
  size_t index;
  for (index = 0U; index < sizeof(levels) / sizeof(levels[0]); ++index) {
    power_on(levels[index], levels[index], false, false);
    open_config();
    step(BUTTON_EVENT_ENTER);
    TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
    TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, training_alphabet_count());
    TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, keyboard_count());
    step(LONG_EVENT(BUTTON_EVENT_MENU));
    step(BUTTON_EVENT_ENTER);
    set_values(true, false);
    step(BUTTON_EVENT_ENTER);
    TEST_ASSERT_EQUAL(TRAINING_MODE_SENDING, training_mode());
    TEST_ASSERT_EQUAL(KOCH_WORD_ALPHABET_MAX, training_alphabet_count());
    step(LONG_EVENT(BUTTON_EVENT_MENU));
  }
}

/*
 * M11, M16, M17, C-7, AC-T59 (2) (4) (7), V-214, F4 (configuration states): every opening has the focus on Start,
 * no edit mode and no message, keeps Mode and Chars, and dirties exactly pages 0, 1, 2 and 7. The openings: the list
 * after menu/back, after a long menu/back in browse and in edit mode, and the enter press on the free end screen
 * (not the Koch start screen: M13).
 */
static void test_openings_reset_the_screen_state(void) {
  power_on(0U, 0U, true, true);
  open_config();
  /* menu/back from a Chars focus with the message: the list, then a clean opening. */
  set_values(true, false);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_PREV);
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_CHARS, false, true));
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(list_is());
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, OPEN_PAGES));
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_START, false, false));

  /* A long menu/back in browse mode leaves at the press. */
  step(BUTTON_EVENT_NEXT);
  harness_pins_for(BUTTON_BIT_MENU, 60U);
  TEST_ASSERT(list_is());
  harness_pins_for(BUTTON_BIT_MENU, 1000U);
  harness_pins_for(0U, 60U);
  TEST_ASSERT(list_is());
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, OPEN_PAGES));
  TEST_ASSERT(config_is("TX", "Learned L1", FOCUS_START, false, false));

  /* In edit mode the press ends edit mode, and the long event leaves (C-4). */
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT(config_is("TX", "Full", FOCUS_CHARS, true, false));
  harness_pins_for(BUTTON_BIT_MENU, 60U);
  TEST_ASSERT(config_is("TX", "Full", FOCUS_CHARS, false, false));
  harness_pins_for(BUTTON_BIT_MENU, 900U);
  TEST_ASSERT(list_is());
  harness_pins_for(0U, 60U);
  TEST_ASSERT(list_is());
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, OPEN_PAGES));
  TEST_ASSERT(config_is("TX", "Full", FOCUS_START, false, false));
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());

  /* The end screen: enter opens the configuration screen, not the Koch start screen. */
  set_values(false, true);
  step(BUTTON_EVENT_ENTER);
  rx_session_to_end();
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FREE, training_outcome());
  TEST_ASSERT(step_dirty(BUTTON_EVENT_ENTER, OPEN_PAGES));
  TEST_ASSERT(config_is("RX", "Full", FOCUS_START, false, false));
  TEST_ASSERT_EQUAL(0U, fake_koch_item_count());
  TEST_ASSERT(has_no_iap_write());
  /* menu/back on the configuration screen: the list with the cursor on Callsigns (AC-T59 (4)). */
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(list_is());
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(main_is("L1", "L1", 2));
}

/*
 * F6, F5, AC-T60, V-223: a Koch Receiving session that passes saves L31 -> L32; free sessions in both modes after it
 * keep every level and write nothing; Receiving from the main menu then shows its Koch start screen and takes Koch
 * items. Reset Statistics keeps Mode and Chars.
 */
static void test_free_sessions_keep_progress(void) {
  uint8_t record[PROGRESS_RECORD_SIZE];
  size_t koch_items;
  unsigned item;

  power_on(30U, 5U, false, false);
  fake_koch_item_push("K");
  harness_open_start_screen(0U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  step(BUTTON_EVENT_ENTER);
  while (run_to_phase(TRAINING_PHASE_ITEM)) {
    pick('K');
  }
  TEST_ASSERT(run_to_phase(TRAINING_PHASE_END));
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  TEST_ASSERT_EQUAL(31U, progress_level(PROGRESS_MODE_RECEIVING, RX_WPM));
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(main_is("L32", "L6", 0));
  koch_items = fake_koch_item_count();
  fake_iap_clear_counters();
  memcpy(record, &fake_iap_memory()[0x0200U], sizeof(record));

  /* RX Learned, all right. */
  open_config();
  step(BUTTON_EVENT_ENTER);
  rx_session_to_end();
  TEST_ASSERT(text_at(2, 7, "Score 20/20"));
  /* RX Full, all skipped with a long next, then an enter tap for each answer state. */
  step(BUTTON_EVENT_ENTER);
  set_values(false, true);
  step(BUTTON_EVENT_ENTER);
  for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
    TEST_ASSERT(run_to_phase(TRAINING_PHASE_ITEM));
    step(LONG_EVENT(BUTTON_EVENT_NEXT));
    step(BUTTON_EVENT_ENTER);
  }
  TEST_ASSERT(run_to_phase(TRAINING_PHASE_END));
  TEST_ASSERT(text_at(2, 7, "Score 0/20"));
  /* TX Learned (TX level 5 at 20 WPM blocks: the message), then TX Full, all skipped. */
  step(BUTTON_EVENT_ENTER);
  set_values(true, false);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(config_is("TX", "Learned L6", FOCUS_START, false, true));
  set_values(true, true);
  step(BUTTON_EVENT_ENTER);
  for (item = 0U; item < TRAINING_FREE_ITEMS; ++item) {
    TEST_ASSERT(run_to_phase(TRAINING_PHASE_ITEM));
    step(LONG_EVENT(BUTTON_EVENT_NEXT));
  }
  TEST_ASSERT(run_to_phase(TRAINING_PHASE_END));
  TEST_ASSERT(text_at(2, 7, "Score 0/20"));
  TEST_ASSERT_EQUAL(koch_items, fake_koch_item_count());
  step(BUTTON_EVENT_MENU);
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(main_is("L32", "L6", 2));
  TEST_ASSERT(has_no_iap_write());
  TEST_ASSERT_EQUAL(0, memcmp(record, &fake_iap_memory()[0x0200U], sizeof(record)));

  /* F5: Receiving from the main menu is a Koch session again. */
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT(!training_is_free());
  TEST_ASSERT(start_button());
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(run_to_phase(TRAINING_PHASE_ITEM));
  TEST_ASSERT_EQUAL(koch_items + 1U, fake_koch_item_count());
  TEST_ASSERT_EQUAL(koch_item_count(31U), training_item_count());
  step(LONG_EVENT(BUTTON_EVENT_MENU));

  /* Reset Statistics: Settings > Reset Statistics > Yes, then OK. Mode and Chars stay (D-T81). */
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_PREV);
  TEST_ASSERT_EQUAL(4U, menu_cursor());
  step(BUTTON_EVENT_ENTER);
  while (menu_cursor() != 4U) {
    step(BUTTON_EVENT_NEXT);
  }
  TEST_ASSERT_EQUAL(0, strcmp("Reset Statistics", menu_nodes[menu_node()].items[menu_cursor()].label));
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_DIALOG, menu_screen());
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(1U, fake_stats_reset_count());
  step(BUTTON_EVENT_ENTER);
  while (menu_node() != MENU_NODE_MAIN) {
    step(BUTTON_EVENT_MENU);
  }
  while (menu_cursor() != 2U) {
    step(BUTTON_EVENT_PREV);
  }
  step(BUTTON_EVENT_ENTER);
  step(BUTTON_EVENT_ENTER);
  TEST_ASSERT(config_is("TX", "Full", FOCUS_START, false, false));
}

int main(void) {
  TEST_RUN(test_first_entry_has_defaults);
  TEST_RUN(test_menu_and_list);
  TEST_RUN(test_focus_steps);
  TEST_RUN(test_repeat_stops_at_the_ends);
  TEST_RUN(test_edit_fields);
  TEST_RUN(test_no_digit_message);
  TEST_RUN(test_longest_texts);
  TEST_RUN(test_learned_edge_at_each_speed);
  TEST_RUN(test_learned_cap);
  TEST_RUN(test_openings_reset_the_screen_state);
  TEST_RUN(test_free_sessions_keep_progress);
  return TEST_FINISH();
}
