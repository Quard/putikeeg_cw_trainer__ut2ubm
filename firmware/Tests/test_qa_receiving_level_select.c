/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the level selection in Receiving, Sending and the main menu (references/internal/training_spec.md
 * D-T35 to D-T42, AC-T32 to AC-T35, task Q5; ui-ux.md sections 5.11.1, 5.15, 5.16, 6.1, 6.8, 6.9, 9.4).
 *
 * Golden screens: each expected screen is written here as the 25-column text of the ui-ux.md mockups. The QA
 * renderer turns that text into a 128 x 64 image with the golden 5 x 8 font (fixtures/font_golden.h), the header
 * band, the Start band and the menu band, and compares every pixel of the framebuffer. It does not use the
 * firmware layout code. The sessions run through the real menu, buttons, modes, engine and progress; the answers go
 * to the engine directly, so the end screen is the only screen under test there.
 */

#include "koch/koch.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define FB_SIZE ((size_t)LCD_PAGE_COUNT * LCD_WIDTH)
#define NO_ROW (-1)
#define STEP_MS 5U

/* ---------- QA golden renderer ---------- */

typedef struct {
  const char* rows[8]; /* NULL: blank row */
  bool has_start_band; /* ui-ux.md 5.16.1: band x 47-81, y 57-63, "Start" inverted in columns 10-14 */
  int selected_row;    /* main menu band x 0-127, y 8r+1..8r+7, or NO_ROW */
} golden_t;

static bool golden_text_ink(const char* text, int x, int y) {
  const int column = (x - 2) / 5;
  if ((text == NULL) || (x < 2) || (column >= 25) || (column >= (int)strlen(text))) {
    return false;
  }
  return golden_ink(text[column], (x - 2) % 5, y % 8);
}

static bool golden_pixel(const golden_t* golden, int x, int y) {
  const int row = y / 8;
  const int cell_y = y % 8;
  const char* text = (row == 7 && golden->has_start_band) ? "          Start" : golden->rows[row];
  const bool is_ink = golden_text_ink(text, x, y);

  if (row == 0) {
    return !is_ink; /* title bar or header: inverted over x 0-127, y 0-7 */
  }
  if (row == golden->selected_row) {
    return (cell_y != 0) && !is_ink;
  }
  if ((row == 7) && golden->has_start_band && (x >= 47) && (x <= 81)) {
    return (cell_y != 0) && !is_ink;
  }
  return is_ink;
}

static void print_rows(const golden_t* golden, int row) {
  int y;
  int x;
  for (y = 8 * row; y < 8 * row + 8; ++y) {
    fprintf(stderr, "  y%2d want ", y);
    for (x = 0; x < 128; ++x) {
      fputc(golden_pixel(golden, x, y) ? '#' : '.', stderr);
    }
    fprintf(stderr, "\n  y%2d got  ", y);
    for (x = 0; x < 128; ++x) {
      fputc(pixel(x, y) ? '#' : '.', stderr);
    }
    fputc('\n', stderr);
  }
}

static bool screen_is(const golden_t* golden, const char* name) {
  int row;
  bool is_ok = true;
  for (row = 0; row < 8; ++row) {
    int y;
    bool is_row_ok = true;
    for (y = 8 * row; (y < 8 * row + 8) && is_row_ok; ++y) {
      int x;
      for (x = 0; x < 128; ++x) {
        if (pixel(x, y) != golden_pixel(golden, x, y)) {
          is_row_ok = false;
          break;
        }
      }
    }
    if (!is_row_ok) {
      fprintf(stderr, "golden %s: row %d differs (want \"%s\")\n", name, row,
              (golden->rows[row] != NULL) ? golden->rows[row] : "");
      print_rows(golden, row);
      is_ok = false;
    }
  }
  return is_ok;
}

#define SCREEN_IS(golden) TEST_ASSERT(screen_is(&(golden), #golden))

/* ---------- Scenario helpers ---------- */

static void power_on(void) {
  harness_power_on();
  fake_koch_item_reset();
  fake_koch_item_push("K");
}

static void set_rx_speeds(uint16_t wpm, uint16_t eff) {
  (void)settings_set(SETTINGS_ID_RX_WPM, wpm);
  (void)settings_set(SETTINGS_ID_RX_EFF_WPM, eff);
}

static void set_tx_speeds(uint16_t wpm, uint16_t eff) {
  (void)settings_set(SETTINGS_ID_TX_WPM, wpm);
  (void)settings_set(SETTINGS_ID_TX_EFF_WPM, eff);
}

/* Fresh EEPROM with one earned level, then power-on. */
static void earn(uint8_t mode, uint8_t wpm, uint16_t level) {
  fake_iap_reset();
  progress_fixture_write(mode, wpm, level);
  power_on();
}

static void step_ms(uint16_t ms) {
  harness_set_time((uint16_t)(harness_now + ms));
  harness_pass();
}

static void until_phase(training_phase_t phase) {
  unsigned guard = 0U;
  while ((training_phase() != phase) && (guard < 60000U)) {
    step_ms(STEP_MS);
    ++guard;
  }
  TEST_ASSERT_EQUAL(phase, training_phase());
}

/* Answers the current item through the engine: right, or one wrong attempt first. */
static void answer_item(bool is_first_wrong) {
  uint8_t index;
  training_check_t check = TRAINING_CHECK_PARTIAL;

  if (is_first_wrong) {
    check = training_add_char((training_item_text()[0] == 'K') ? 'M' : 'K');
    for (index = 1U; (check == TRAINING_CHECK_PARTIAL) && (index < training_item_length()); ++index) {
      check = training_add_char(training_item_text()[index]);
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, check);
    until_phase(TRAINING_PHASE_ITEM);
  }
  for (index = 0U; index < training_item_length(); ++index) {
    check = training_add_char(training_item_text()[index]);
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, check);
}

/* Runs the session of the open mode up to the last answer, with @p first_correct first-attempt answers. */
static void run_to_last_answer(unsigned first_correct) {
  unsigned number;
  const unsigned count = training_item_count();
  for (number = 1U; number <= count; ++number) {
    until_phase(TRAINING_PHASE_ITEM);
    answer_item(number > first_correct);
  }
}

static void run_session(unsigned first_correct) {
  run_to_last_answer(first_correct);
  until_phase(TRAINING_PHASE_END);
}

static bool is_tap_mode_off_on_main_menu(void) {
  /* Main menu, prev: the cursor moves at the debounced press when tap mode is off (ui-ux.md 6.1). */
  const uint8_t cursor = menu_cursor();
  bool is_off;
  harness_pins_for(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + 5U));
  is_off = (menu_cursor() != cursor);
  harness_pins_for(0U, 40U);
  /* Put the cursor back. */
  if (is_off) {
    harness_event(BUTTON_EVENT_NEXT);
  }
  return is_off;
}

/* ---------- Golden start screens (AC-T35, ui-ux.md 5.16.1 to 5.16.3) ---------- */

static void test_golden_start_screens(void) {
  static const golden_t rx_first_use = {
      {"RX 32/32 L1          1/50", NULL, NULL, NULL, "         L1 of 1", "         K and M", NULL, NULL},
      true,
      NO_ROW};
  static const golden_t tx_l20 = {
      {"TX 20/20 L20         1/50", NULL, NULL, NULL, "     <  L20 of 20", "       New char J", NULL, NULL},
      true,
      NO_ROW};
  static const golden_t rx_words = {
      {"RX 32/32 L18         1/10", NULL, NULL, NULL, "     <  L18 of 18", "       Words of 4", NULL, NULL},
      true,
      NO_ROW};
  static const golden_t tx_final_top = {
      {"TX 20/20 L65         1/11", NULL, NULL, NULL, "     <  L65 of 65", "       Words of 5", NULL, NULL},
      true,
      NO_ROW};
  static const golden_t tx_final_below = {
      {"TX 20/20 L64         1/10", NULL, NULL, NULL, "     <  L64 of 65  >", "       Words of 5", NULL, NULL},
      true,
      NO_ROW};
  static const golden_t rx_l2_of_2 = {
      {"RX 32/32 L2          1/50", NULL, NULL, NULL, "      <  L2 of 2", "       New char R", NULL, NULL},
      true,
      NO_ROW};
  static const golden_t rx_l1_of_2 = {
      {"RX 32/32 L1          1/50", NULL, NULL, NULL, "         L1 of 2  >", "         K and M", NULL, NULL},
      true,
      NO_ROW};
  static const golden_t rx_top = {
      {"RX 50/50 L65535     1/100", NULL, NULL, NULL, "  <  L65535 of 65535", "       Words of 5", NULL, NULL},
      true,
      NO_ROW};
  static const golden_t rx_top_below = {
      {"RX 50/50 L65535     1/100", NULL, NULL, NULL, "  <  L65535 of 65535  >", "       Words of 5", NULL, NULL},
      true,
      NO_ROW};

  fake_iap_reset();
  power_on();
  harness_open_start_screen(0U);
  SCREEN_IS(rx_first_use);
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());

  fake_iap_reset();
  power_on();
  harness_open_start_screen(1U);
  TEST_ASSERT(screen_is(
      &(golden_t){{"TX 20/20 L1          1/50", NULL, NULL, NULL, "         L1 of 1", "         K and M", NULL, NULL},
                  true,
                  NO_ROW},
      "tx_first_use"));
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());

  earn(PROGRESS_MODE_SENDING, 20U, 19U);
  harness_open_start_screen(1U);
  SCREEN_IS(tx_l20);

  earn(PROGRESS_MODE_RECEIVING, 32U, 17U);
  harness_open_start_screen(0U);
  SCREEN_IS(rx_words);

  earn(PROGRESS_MODE_SENDING, 20U, 64U);
  harness_open_start_screen(1U);
  SCREEN_IS(tx_final_top);
  harness_event(M_TAP);
  SCREEN_IS(tx_final_below);

  earn(PROGRESS_MODE_RECEIVING, 32U, 1U);
  harness_open_start_screen(0U);
  SCREEN_IS(rx_l2_of_2);
  harness_event(M_TAP);
  SCREEN_IS(rx_l1_of_2);
  harness_event(R_TAP);
  SCREEN_IS(rx_l2_of_2);

  earn(PROGRESS_MODE_RECEIVING, 50U, 65535U);
  set_rx_speeds(50U, 50U);
  harness_open_start_screen(0U);
  SCREEN_IS(rx_top);
  harness_event(M_TAP);
  SCREEN_IS(rx_top_below);
  harness_event(R_TAP);
  SCREEN_IS(rx_top);
}

/* The description in the three Prosigns styles (ui-ux.md 5.16.3, V-140), as whole screens. */
static void test_golden_prosign_descriptions(void) {
  static const uint8_t styles[3] = {SETTINGS_PROSIGN_BOTH, SETTINGS_PROSIGN_NAME, SETTINGS_PROSIGN_PUNCTUATION};
  static const char* const bt_rows[3] = {"     New char <BT/=>", "      New char <BT>", "       New char ="};
  static const char* const ar_rows[3] = {"     New char <AR/+>", "      New char <AR>", "       New char +"};
  size_t index;

  for (index = 0U; index < 3U; ++index) {
    golden_t bt = {
        {"RX 32/32 L59         1/50", NULL, NULL, NULL, "     <  L59 of 59", NULL, NULL, NULL}, true, NO_ROW};
    golden_t ar = {
        {"RX 32/32 L62         1/50", NULL, NULL, NULL, "     <  L62 of 62", NULL, NULL, NULL}, true, NO_ROW};
    bt.rows[5] = bt_rows[index];
    ar.rows[5] = ar_rows[index];
    earn(PROGRESS_MODE_RECEIVING, 32U, 58U);
    (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, styles[index]);
    harness_open_start_screen(0U);
    TEST_ASSERT(screen_is(&bt, bt_rows[index]));
    earn(PROGRESS_MODE_RECEIVING, 32U, 61U);
    (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, styles[index]);
    harness_open_start_screen(0U);
    TEST_ASSERT(screen_is(&ar, ar_rows[index]));
  }
}

/* ---------- Golden end screens (AC-T35, ui-ux.md 5.15) ---------- */

static void test_golden_end_pass_fail_not_saved(void) {
  static const golden_t pass = {{"RX 32/32 L1         50/50", NULL, "       Score 45/50", "    Passed, new level",
                                 "      <  L2 of 2", "       New char R", NULL, NULL},
                                true,
                                NO_ROW};
  static const golden_t fail = {{"RX 32/32 L1         50/50", NULL, "       Score 44/50", "   Not passed, need 45",
                                 "         L1 of 1", "         K and M", NULL, NULL},
                                true,
                                NO_ROW};
  static const golden_t not_saved = {{"RX 32/32 L1         50/50", "        Not saved", "       Score 50/50",
                                      "    Passed, new level", "      <  L2 of 2", "       New char R", NULL, NULL},
                                     true,
                                     NO_ROW};
  static const golden_t pass_stepped = {
      {"RX 32/32 L1         50/50", NULL, "       Score 45/50", "    Passed, new level", "         L1 of 2  >",
       "         K and M", NULL, NULL},
      true,
      NO_ROW};

  fake_iap_reset();
  power_on();
  harness_open_mode(0U);
  run_session(45U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  SCREEN_IS(pass);
  /* SL-6: the end screen header keeps the session that ended while the selection changes. */
  harness_event(M_TAP);
  SCREEN_IS(pass_stepped);
  harness_event(R_TAP);
  SCREEN_IS(pass);

  fake_iap_reset();
  power_on();
  harness_open_mode(0U);
  run_session(44U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
  SCREEN_IS(fail);

  fake_iap_reset();
  power_on();
  harness_open_mode(0U);
  fake_iap_set_fail_operation(1U); /* the erase fails */
  run_session(50U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_NOT_SAVED, training_outcome());
  SCREEN_IS(not_saved);
}

static void test_golden_end_replay(void) {
  static const golden_t rx_pass = {{"RX 32/32 L5         50/50", NULL, "       Score 50/50", "         Passed",
                                    "     <  L6 of 20  >", "       Words of 2", NULL, NULL},
                                   true,
                                   NO_ROW};
  static const golden_t rx_fail = {{"RX 32/32 L5         50/50", NULL, "       Score 44/50", "   Not passed, need 45",
                                    "     <  L5 of 20  >", "       New char U", NULL, NULL},
                                   true,
                                   NO_ROW};
  static const golden_t tx_pass = {{"TX 20/20 L1         50/50", NULL, "       Score 47/50", "         Passed",
                                    "     <  L2 of 20  >", "       New char R", NULL, NULL},
                                   true,
                                   NO_ROW};
  unsigned index;

  earn(PROGRESS_MODE_RECEIVING, 32U, 19U);
  harness_open_start_screen(0U);
  for (index = 0U; index < 15U; ++index) {
    harness_event(M_TAP);
  }
  harness_event(ENTER_EVENT);
  run_session(50U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
  SCREEN_IS(rx_pass);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  /* Start at L6 from the end screen, back down to L5, fail. */
  harness_event(M_TAP);
  harness_event(ENTER_EVENT);
  TEST_ASSERT_EQUAL(4U, training_level());
  run_session(44U);
  SCREEN_IS(rx_fail);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));

  earn(PROGRESS_MODE_SENDING, 20U, 19U);
  harness_open_start_screen(1U);
  for (index = 0U; index < 19U; ++index) {
    harness_event(M_TAP);
  }
  harness_event(ENTER_EVENT);
  run_session(47U);
  SCREEN_IS(tx_pass);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  TEST_ASSERT_EQUAL(19U, progress_level(PROGRESS_MODE_SENDING, 20U));
}

static void test_golden_end_top(void) {
  static const golden_t top = {{"RX 50/50 L65535   100/100", NULL, "      Score 80/100", "         Passed",
                                "  <  L65535 of 65535", "       Words of 5", NULL, NULL},
                               true,
                               NO_ROW};
  earn(PROGRESS_MODE_RECEIVING, 50U, 65535U);
  set_rx_speeds(50U, 50U);
  harness_open_mode(0U);
  run_session(80U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_TOP, training_outcome());
  SCREEN_IS(top);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
}

/* ---------- Golden main menu (AC-T34, ui-ux.md 5.16.6, V-01, V-149) ---------- */

static void test_golden_main_menu(void) {
  static const golden_t empty = {{"CW Trainer", "Receiving            L1", "Sending              L1", "Free Input",
                                  "Settings                >", "About", NULL, NULL},
                                 false,
                                 1};
  static const golden_t levels = {{"CW Trainer", "Receiving           L12", "Sending              L1", "Free Input",
                                   "Settings                >", "About", NULL, NULL},
                                  false,
                                  1};
  static const golden_t top_on_sending = {{"CW Trainer", "Receiving        L65535", "Sending          L65535",
                                           "Free Input", "Settings                >", "About", NULL, NULL},
                                          false,
                                          2};
  static const golden_t on_free_input = {{"CW Trainer", "Receiving        L65535", "Sending          L65535",
                                          "Free Input", "Settings                >", "About", NULL, NULL},
                                         false,
                                         3};

  fake_iap_reset();
  power_on();
  SCREEN_IS(empty);

  fake_iap_reset();
  progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, 11U);
  power_on();
  SCREEN_IS(levels);

  fake_iap_reset();
  progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, 65534U);
  progress_fixture_add(PROGRESS_MODE_SENDING, 20U, 65535U);
  power_on();
  harness_event(BUTTON_EVENT_NEXT);
  SCREEN_IS(top_on_sending);
  harness_event(BUTTON_EVENT_NEXT);
  SCREEN_IS(on_free_input);
}

/*
 * Q5 menu case: the level key is the base speed of the mode itself, never the effective speed and never the speed
 * of the other mode; the mode entry and the menu agree. A speed change shows at the next render and entry.
 */
static void test_level_key_is_own_base_speed(void) {
  fake_iap_reset();
  progress_fixture_write(PROGRESS_MODE_RECEIVING, 20U, 4U);
  progress_fixture_add(PROGRESS_MODE_RECEIVING, 15U, 30U); /* RX Eff. Speed */
  progress_fixture_add(PROGRESS_MODE_RECEIVING, 25U, 2U);  /* TX Speed */
  progress_fixture_add(PROGRESS_MODE_SENDING, 25U, 9U);
  progress_fixture_add(PROGRESS_MODE_SENDING, 20U, 40U); /* RX Speed */
  progress_fixture_add(PROGRESS_MODE_SENDING, 15U, 50U);
  power_on();
  set_rx_speeds(20U, 15U);
  set_tx_speeds(25U, 15U);
  menu_render();
  TEST_ASSERT(menu_row_is(1, "Receiving", "L5", true));
  TEST_ASSERT(menu_row_is(2, "Sending", "L10", false));
  harness_open_start_screen(0U);
  TEST_ASSERT(hdr("RX 20/15 L5          1/50"));
  TEST_ASSERT(lvl(5U, 5U));
  harness_event(L_TAP);
  harness_open_start_screen(1U);
  TEST_ASSERT(hdr("TX 25/15 L10         1/50"));
  TEST_ASSERT(lvl(10U, 10U));
  harness_event(L_TAP);
  /* Speed change between the menu and the mode. */
  set_rx_speeds(25U, 25U);
  set_tx_speeds(20U, 20U);
  menu_render();
  TEST_ASSERT(menu_row_is(1, "Receiving", "L3", false));
  TEST_ASSERT(menu_row_is(2, "Sending", "L41", true));
  harness_event(BUTTON_EVENT_PREV);
  harness_open_start_screen(0U);
  TEST_ASSERT(hdr("RX 25/25 L3          1/10"));
  TEST_ASSERT(lvl(3U, 3U));
}

/* ---------- Tap mode, held buttons and the keyer (Q5 mode cases) ---------- */

/* AC-T32 (5), D-T39: after a leave from each phase, tap mode is off on the main menu and on the next start screen. */
static void test_rx_tap_mode_off_after_leave_from_each_phase(void) {
  static const training_phase_t phases[5] = {TRAINING_PHASE_SELECT, TRAINING_PHASE_GAP, TRAINING_PHASE_ITEM,
                                             TRAINING_PHASE_RESULT, TRAINING_PHASE_END};
  size_t index;

  for (index = 0U; index < 5U; ++index) {
    earn(PROGRESS_MODE_RECEIVING, 32U, 5U);
    harness_open_start_screen(0U);
    if (phases[index] != TRAINING_PHASE_SELECT) {
      harness_event(ENTER_EVENT);
    }
    if (phases[index] == TRAINING_PHASE_ITEM) {
      until_phase(TRAINING_PHASE_ITEM);
    } else if (phases[index] == TRAINING_PHASE_RESULT) {
      until_phase(TRAINING_PHASE_ITEM);
      answer_item(false);
    } else if (phases[index] == TRAINING_PHASE_END) {
      run_session(50U);
    }
    TEST_ASSERT_EQUAL(phases[index], training_phase());
    if ((phases[index] == TRAINING_PHASE_SELECT) || (phases[index] == TRAINING_PHASE_END)) {
      /* A real menu/back press leaves at the press; its long event later does nothing. */
      harness_pins_for(BUTTON_BIT_MENU, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
      TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
      harness_pins_for(BUTTON_BIT_MENU, 1000U);
    } else {
      /* A real menu/back hold: the long event leaves; the release then gives nothing on the main menu. */
      harness_pins_for(BUTTON_BIT_MENU, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 5U));
      TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    }
    harness_pins_for(0U, 50U);
    TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
    TEST_ASSERT_EQUAL(0U, menu_cursor());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
    TEST_ASSERT(!fake_player_is_running());
    TEST_ASSERT(is_tap_mode_off_on_main_menu());
    /* The next start screen: a prev press steps at the press. */
    harness_open_start_screen(0U);
    harness_pins_for(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
    /* The END run passed at the earned level, so its earned level is one higher. */
    TEST_ASSERT(lvl(training_earned_level(), training_earned_level() + 1U));
    TEST_ASSERT_EQUAL((phases[index] == TRAINING_PHASE_END) ? 6U : 5U, training_earned_level());
    harness_pins_for(0U, 50U);
    harness_event(L_TAP);
  }
}

/* D-T39, ui-ux.md 5.16.4: prev held with repeats across the session start gives no event in the session. */
static void test_rx_prev_held_across_start(void) {
  uint16_t level;

  earn(PROGRESS_MODE_RECEIVING, 32U, 19U);
  harness_open_start_screen(0U);
  harness_pins_for(BUTTON_BIT_PREV, 500U); /* the press and one repeat */
  TEST_ASSERT_EQUAL(17U, training_selected_level());
  harness_pins_for(BUTTON_BIT_PREV | BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  level = training_selected_level();
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(level, training_level());
  /* Hold both through the start pause and into ITEM, past the long-press time; then release in ITEM. */
  harness_pins_for(BUTTON_BIT_PREV | BUTTON_BIT_ENTER, 1500U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  harness_pins_for(0U, 60U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(level, training_level());
  /* One play of item 1 only: no replay from the held enter. */
  TEST_ASSERT_EQUAL(1U, fake_player_call_count());
}

/* AC-T32 (6): menu/back held from the last RESULT across the END event gives no event, also no long event. */
static void test_rx_menu_held_across_end(void) {
  static const golden_t end = {{"RX 32/32 L6         10/10", NULL, "       Score 10/10", "    Passed, new level",
                                "      <  L7 of 7", "       New char A", NULL, NULL},
                               true,
                               NO_ROW};
  earn(PROGRESS_MODE_RECEIVING, 32U, 5U);
  harness_open_mode(0U);
  run_to_last_answer(10U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  harness_pins_for(BUTTON_BIT_MENU, 1500U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  harness_pins_for(0U, 60U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  SCREEN_IS(end);
  harness_pins_for(BUTTON_BIT_MENU, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  harness_pins_for(0U, 60U);
}

/*
 * D-T40, AC-T33 (1), (5): the keyer runs only in a Sending session. A leave from SELECT or END does not touch the
 * keyer and leaves enter with the menu; a long menu/back from GAP, ITEM or RESULT stops it once and gives P3.2 back.
 */
static void test_tx_keyer_ownership_on_leave(void) {
  static const training_phase_t phases[5] = {TRAINING_PHASE_SELECT, TRAINING_PHASE_GAP, TRAINING_PHASE_ITEM,
                                             TRAINING_PHASE_RESULT, TRAINING_PHASE_END};
  size_t index;

  for (index = 0U; index < 5U; ++index) {
    size_t stops;
    earn(PROGRESS_MODE_SENDING, 20U, 2U);
    harness_open_start_screen(1U);
    TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
    if (phases[index] != TRAINING_PHASE_SELECT) {
      harness_event(ENTER_EVENT);
      TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
    }
    if (phases[index] == TRAINING_PHASE_ITEM) {
      until_phase(TRAINING_PHASE_ITEM);
    } else if (phases[index] == TRAINING_PHASE_RESULT) {
      until_phase(TRAINING_PHASE_ITEM);
      answer_item(false);
    } else if (phases[index] == TRAINING_PHASE_END) {
      run_session(10U);
      TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
    }
    TEST_ASSERT_EQUAL(phases[index], training_phase());
    stops = fake_keyer_stop_count();
    if ((phases[index] == TRAINING_PHASE_SELECT) || (phases[index] == TRAINING_PHASE_END)) {
      harness_event(L_TAP);
      TEST_ASSERT_EQUAL(stops, fake_keyer_stop_count());
    } else {
      harness_event(LONG_EVENT(L_TAP));
      TEST_ASSERT_EQUAL(stops + 1U, fake_keyer_stop_count());
    }
    TEST_ASSERT(!fake_keyer_is_running());
    TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    TEST_ASSERT_EQUAL(1U, menu_cursor());
    /* P3.2 is enter on the main menu: a real enter press opens Sending again. */
    harness_pins_for(BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
    harness_pins_for(0U, 40U);
    TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
    TEST_ASSERT(!fake_keyer_is_running());
    harness_event(L_TAP);
  }
}

/* Re-entry always preselects the earned level; a selection made on the end screen is lost (ui-ux.md 5.16.4). */
static void test_reentry_preselects_earned(void) {
  earn(PROGRESS_MODE_RECEIVING, 32U, 9U);
  harness_open_start_screen(0U);
  harness_event(M_TAP);
  harness_event(M_TAP);
  harness_event(ENTER_EVENT);
  run_session(50U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
  TEST_ASSERT(lvl(9U, 10U));
  harness_event(M_TAP);
  harness_event(L_TAP);
  TEST_ASSERT(menu_row_is(1, "Receiving", "L10", true));
  harness_open_start_screen(0U);
  TEST_ASSERT(lvl(10U, 10U));
  TEST_ASSERT(hdr("RX 32/32 L10         1/50"));
}

int main(void) {
  TEST_RUN(test_golden_start_screens);
  TEST_RUN(test_golden_prosign_descriptions);
  TEST_RUN(test_golden_end_pass_fail_not_saved);
  TEST_RUN(test_golden_end_replay);
  TEST_RUN(test_golden_end_top);
  TEST_RUN(test_golden_main_menu);
  TEST_RUN(test_level_key_is_own_base_speed);
  TEST_RUN(test_rx_tap_mode_off_after_leave_from_each_phase);
  TEST_RUN(test_rx_prev_held_across_start);
  TEST_RUN(test_rx_menu_held_across_end);
  TEST_RUN(test_tx_keyer_ownership_on_leave);
  TEST_RUN(test_reentry_preselects_earned);
  return TEST_FINISH();
}
