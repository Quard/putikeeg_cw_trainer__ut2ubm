/*
 * Receiving mode scenarios (references/internal/training_spec.md section 12.2, AC-T19; ui-ux.md section 9.4, V-90 to
 * V-109 and V-120). The real menu, buttons, engine, keyboard and progress run with a fake player and fake items.
 */

#include "cw/morse.h"
#include "koch/keyboard.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define LEVEL_KM 0U
#define LEVEL_KMR 1U
#define LEVEL_WORDS_13 17U /* KMRSUAPTLOWI., words of 4 */
#define LEVEL_ALL_CHARS 61U
#define LEVEL_ALL_WORDS 62U
#define AR_TEXT "\x02"
#define SK_TEXT "\x01"
#define BT_TEXT "\x03"
#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)

static uint8_t snapshot_before[FB_SIZE];

static void snapshot(uint8_t* buffer) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&buffer[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
}

static bool is_screen_unchanged(void) {
  uint8_t now_image[FB_SIZE];
  snapshot(now_image);
  return memcmp(snapshot_before, now_image, FB_SIZE) == 0;
}

/* RX(set, kind, items): the level gives the set and the kind (training_spec.md section 15.2). */
static void rx_open(uint16_t level, uint8_t prosign_style, const char* const* items, size_t count) {
  size_t index;

  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, level);
  }
  harness_power_on();
  (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, prosign_style);
  fake_koch_item_reset();
  for (index = 0U; index < count; ++index) {
    fake_koch_item_push(items[index]);
  }
  harness_open_mode(0U);
}

static void rx_open_one(uint16_t level, const char* item) {
  const char* const items[1] = {item};
  rx_open(level, SETTINGS_PROSIGN_BOTH, items, 1U);
}

/* One event at the next ms. */
static void tap(button_event_t event) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(event);
}

/* Passes until the engine is in phase ITEM. */
static void run_to_item(void) {
  unsigned guard = 0U;
  while ((training_phase() != TRAINING_PHASE_ITEM) && (guard < 20000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
}

/* The tap of each run, left to right (D-T27). */
static const button_event_t group_taps[KEYBOARD_GROUP_COUNT] = {L_TAP, M_TAP, R_TAP, ENTER_EVENT};

/* Taps the groups that lead to @p character of the keyboard list, up to the pick (the keyboard back at the top). */
static void pick(char character) {
  const uint8_t not_found = 0xFFU;
  uint8_t target = not_found;
  uint8_t index;
  unsigned guard;

  for (index = 0U; index < keyboard_count(); ++index) {
    if (keyboard_char(index) == character) {
      target = index;
    }
  }
  if (!TEST_ASSERT(target != not_found)) {
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
    tap(group_taps[group]);
    if (keyboard_depth() == 0U) {
      return;
    }
  }
}

/* HDR text of ui-ux.md section 5.11.1 for the RX speeds 32/32. */
static const char* rx_header(unsigned level, unsigned item, unsigned count) {
  static char text[32];
  char left[16];
  char right[16];
  (void)snprintf(left, sizeof(left), "RX 32/32 L%u", level);
  (void)snprintf(right, sizeof(right), "%u/%u", item, count);
  (void)snprintf(text, sizeof(text), "%-*s%s", 25 - (int)strlen(right), left, right);
  return text;
}

/* A real tap of @p pin: low for 30 ms, then released through the debounce. */
static void pin_tap(uint8_t pin) {
  harness_pins_for(pin, 30U);
  harness_pins_for(0U, 30U);
}

/* LONG(X) with real pins: held until the long event, then released. */
static void pin_long(uint8_t pin) {
  harness_pins_for(pin, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 10U));
  harness_pins_for(0U, 30U);
}

static void expect_item_line_block(int block_x) {
  item_expect_reset();
  item_expect(ITEM_PART_BLOCK, block_x, NULL);
  TEST_ASSERT(item_line_is());
  TEST_ASSERT(block_at(block_x));
}

static void expect_item_line_big(int x, const char* text) {
  item_expect_reset();
  item_expect(ITEM_PART_BIG, x, text);
  TEST_ASSERT(item_line_is());
}

/* V-90 */
static void test_v90_start(void) {
  rx_open_one(LEVEL_KM, "K");
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT(hdr("RX 32/32 L1          1/50"));
  harness_run_to(HARNESS_T0 + 999U);
  TEST_ASSERT(hdr("RX 32/32 L1          1/50"));
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(body_off(2, 5));
  TEST_ASSERT(keys("K", "M", "", ""));
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());
  TEST_ASSERT(margins_off());
  harness_run_to(HARNESS_T0 + 1000U);
  expect_item_line_block(60);
  TEST_ASSERT(status(NULL));
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0, strcmp("K", fake_player_last_text()));
  TEST_ASSERT_EQUAL(32U, fake_player_last_wpm());
  TEST_ASSERT_EQUAL(32U, fake_player_last_eff_wpm());
  TEST_ASSERT(margins_off());
  /* The mode makes one sidetone call: the tone at the start. */
  TEST_ASSERT_EQUAL(1U, fake_sidetone_call_count());
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_CALL_TONE, fake_sidetone_call_at(0U).kind);
  TEST_ASSERT_EQUAL(600U, fake_sidetone_call_at(0U).value);
}

/* V-91 */
static void test_v91_correct_and_gap(void) {
  const char* const items[2] = {"K", "M"};

  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
  harness_run_to(HARNESS_T0 + 1500U);
  fake_player_finish();
  harness_at(2000U, L_TAP);
  expect_item_line_big(60, "K");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT(keys("K", "M", "", ""));
  TEST_ASSERT_EQUAL(1U, training_score());
  snapshot(snapshot_before);
  harness_run_to(HARNESS_T0 + 2599U);
  TEST_ASSERT(is_screen_unchanged());
  harness_run_to(HARNESS_T0 + 2600U);
  TEST_ASSERT(body_off(1, 5));
  TEST_ASSERT(hdr("RX 32/32 L1          2/50"));
  TEST_ASSERT(keys("K", "M", "", ""));
  harness_run_to(HARNESS_T0 + 3099U);
  TEST_ASSERT(body_off(1, 5));
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  harness_run_to(HARNESS_T0 + 3100U);
  expect_item_line_block(60);
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0, strcmp("M", fake_player_last_text()));
  TEST_ASSERT(hdr("RX 32/32 L1          2/50"));
  TEST_ASSERT(margins_off());
}

/* V-92 */
static void test_v92_narrow_and_pick(void) {
  rx_open_one(LEVEL_ALL_CHARS, "M");
  TEST_ASSERT(hdr(rx_header(62U, 1U, 50U)));
  TEST_ASSERT_EQUAL(0, strcmp("RX 32/32 L1          1/50", rx_header(1U, 1U, 50U)));
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT(keys("A-K", "L-V", "W-6", "7-<AR>"));
  tap(M_TAP);
  TEST_ASSERT(keys("LMN", "OPQ", "RST", "UV"));
  tap(L_TAP);
  TEST_ASSERT(keys("L", "M", "N", ""));
  expect_item_line_block(60);
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  tap(M_TAP);
  expect_item_line_big(60, "M");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT(keys("A-K", "L-V", "W-6", "7-<AR>"));
  TEST_ASSERT(margins_off());
}

typedef struct {
  uint8_t style;
  const char* top_enter;
  const char* after_e[4];
  const char* after_ee[4];
  const char* ar_text;
  int ar_x;
  const char* after_er[4];
  const char* bt_text;
  int bt_x;
} v93_row_t;

/* V-93: the prosign tail of the enter run for each Prosigns value. A one-character label uses the short text. */
static void test_v93_tail_labels(void) {
  static const v93_row_t rows[3] = {
      {SETTINGS_PROSIGN_BOTH,
       "7-<AR>",
       {"789", ".,?", "/<BT>", "<SK><AR>"},
       {"<SK>", "<AR>", "", ""},
       "<AR/+>",
       40,
       {"/", "<BT>", "", ""},
       "<BT/=>",
       40},
      {SETTINGS_PROSIGN_NAME,
       "7-<AR>",
       {"789", ".,?", "/<BT>", "<SK><AR>"},
       {"<SK>", "<AR>", "", ""},
       "<AR>",
       48,
       {"/", "<BT>", "", ""},
       "<BT>",
       48},
      {SETTINGS_PROSIGN_PUNCTUATION,
       "7-+",
       {"789", ".,?", "/=", "<SK>+"},
       {"<SK>", "+", "", ""},
       "+",
       60,
       {"/", "=", "", ""},
       "=",
       60},
  };
  size_t index;

  for (index = 0U; index < 3U; ++index) {
    const v93_row_t* row = &rows[index];
    const char* const ar_items[1] = {AR_TEXT};
    const char* const sk_items[1] = {SK_TEXT};
    const char* const bt_items[1] = {BT_TEXT};

    rx_open(LEVEL_ALL_CHARS, row->style, ar_items, 1U);
    harness_run_to(HARNESS_T0 + 1000U);
    TEST_ASSERT(keys("A-K", "L-V", "W-6", row->top_enter));
    tap(ENTER_EVENT);
    TEST_ASSERT(keys(row->after_e[0], row->after_e[1], row->after_e[2], row->after_e[3]));
    tap(ENTER_EVENT);
    TEST_ASSERT(keys(row->after_ee[0], row->after_ee[1], row->after_ee[2], row->after_ee[3]));
    tap(M_TAP);
    expect_item_line_big(row->ar_x, row->ar_text);
    TEST_ASSERT(status("Correct"));

    rx_open(LEVEL_ALL_CHARS, row->style, sk_items, 1U);
    harness_run_to(HARNESS_T0 + 1000U);
    tap(ENTER_EVENT);
    tap(ENTER_EVENT);
    tap(L_TAP);
    expect_item_line_big(48, "<SK>");
    TEST_ASSERT(status("Correct"));

    rx_open(LEVEL_ALL_CHARS, row->style, bt_items, 1U);
    harness_run_to(HARNESS_T0 + 1000U);
    tap(ENTER_EVENT);
    tap(R_TAP);
    TEST_ASSERT(keys(row->after_er[0], row->after_er[1], row->after_er[2], row->after_er[3]));
    tap(M_TAP);
    expect_item_line_big(row->bt_x, row->bt_text);
    TEST_ASSERT(status("Correct"));
    TEST_ASSERT(margins_off());
  }
}

static void expect_word_slots(const char* picks, uint8_t length) {
  static const int xs[4] = {36, 52, 68, 84};
  static char glyphs[4][2];
  uint8_t slot;
  const uint8_t pick_count = (uint8_t)strlen(picks);

  item_expect_reset();
  for (slot = 0U; slot < length; ++slot) {
    if (slot < pick_count) {
      glyphs[slot][0] = picks[slot];
      glyphs[slot][1] = '\0';
      item_expect(ITEM_PART_BIG, xs[slot], glyphs[slot]);
    } else if (slot == pick_count) {
      item_expect(ITEM_PART_BLOCK, xs[slot], NULL);
    } else {
      item_expect(ITEM_PART_BAR, xs[slot], NULL);
    }
  }
  TEST_ASSERT(item_line_is());
}

/* V-94 */
static void test_v94_word_undo(void) {
  uint8_t before[FB_SIZE];

  /* Run 1: KMRSUAPTLOWI. in keyboard order is AIKLMOPRSTUW. (13 characters: 4/3/3/3). */
  rx_open_one(LEVEL_WORDS_13, "KMRS");
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT(keys("AIKL", "MOP", "RST", "UW."));
  expect_word_slots("", 4U);
  tap(L_TAP);
  TEST_ASSERT(keys("A", "I", "K", "L"));
  tap(R_TAP);
  expect_word_slots("K", 4U);
  TEST_ASSERT(keys("AIKL", "MOP", "RST", "UW."));
  /* First LONG(M): no change up to 800 ms, then the pick is undone. */
  snapshot(before);
  harness_pins_for(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS - 1U));
  memcpy(snapshot_before, before, FB_SIZE);
  TEST_ASSERT(is_screen_unchanged());
  harness_pins_for(BUTTON_BIT_PREV, 11U);
  harness_pins_for(0U, 30U);
  expect_word_slots("", 4U);
  TEST_ASSERT(keys("AIKL", "MOP", "RST", "UW."));
  tap(L_TAP);
  TEST_ASSERT(keys("A", "I", "K", "L"));
  pin_long(BUTTON_BIT_PREV);
  TEST_ASSERT(keys("AIKL", "MOP", "RST", "UW."));
  expect_word_slots("", 4U);
  snapshot(snapshot_before);
  pin_long(BUTTON_BIT_PREV);
  TEST_ASSERT(is_screen_unchanged());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(0U, fake_player_stop_count());
  TEST_ASSERT(status(NULL));

  /* Run 2: two narrowings, then three undo steps. */
  rx_open_one(LEVEL_ALL_CHARS, "M");
  harness_run_to(HARNESS_T0 + 1000U);
  tap(M_TAP);
  tap(L_TAP);
  TEST_ASSERT(keys("L", "M", "N", ""));
  pin_long(BUTTON_BIT_PREV);
  TEST_ASSERT(keys("LMN", "OPQ", "RST", "UV"));
  pin_long(BUTTON_BIT_PREV);
  TEST_ASSERT(keys("A-K", "L-V", "W-6", "7-<AR>"));
  snapshot(snapshot_before);
  pin_long(BUTTON_BIT_PREV);
  TEST_ASSERT(is_screen_unchanged());
  expect_item_line_block(60);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(0U, fake_player_stop_count());
}

/* V-95: a held prev is not a pick. */
static void test_v95_hold_prev(void) {
  uint16_t press;

  rx_open_one(LEVEL_KMR, "M");
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT(keys("K", "M", "R", ""));
  harness_pins_for(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + 1000U));
  press = (uint16_t)(harness_now - 1000U);
  harness_pins_for(0U, 30U);
  harness_run_to((uint16_t)(press + 3000U));
  expect_item_line_block(60);
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(keys("K", "M", "R", ""));
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  pin_tap(BUTTON_BIT_PREV);
  expect_item_line_big(60, "M");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_score());
}

/* V-96: a held next does nothing; a held menu/back below the long time is a tap at its release. */
static void test_v96_holds(void) {
  rx_open_one(LEVEL_ALL_CHARS, "K");
  harness_run_to(HARNESS_T0 + 1000U);
  snapshot(snapshot_before);
  harness_pins_for(BUTTON_BIT_NEXT, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2000U));
  harness_pins_for(0U, 100U);
  TEST_ASSERT(is_screen_unchanged());
  harness_pins_for(BUTTON_BIT_MENU, 790U);
  TEST_ASSERT(is_screen_unchanged());
  harness_pins_for(0U, 30U);
  TEST_ASSERT(keys("ABC", "DEF", "GHI", "JK"));
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
}

/* Sets the pins to @p mask so that the debounced change comes at HARNESS_T0 + @p t; returns at that time. */
static void pins_at(uint8_t mask, uint16_t t) {
  harness_run_to((uint16_t)(HARNESS_T0 + t - BUTTONS_DEBOUNCE_MS - 1U));
  harness_pins_for(mask, (uint16_t)(BUTTONS_DEBOUNCE_MS + 1U));
}

/* TIP(t, d) of ui-ux.md section 9.4: P3.2 low from t for d ms, then high and debounced. */
static void tip(uint16_t t, uint16_t d) {
  harness_run_to((uint16_t)(HARNESS_T0 + t - 1U));
  harness_pins_for(BUTTON_BIT_ENTER, d);
  harness_pins_for(0U, BUTTONS_DEBOUNCE_MS);
}

/* Screen of a KM item before any pick: the block, the labels and no status. */
static bool is_km_item_waiting(void) {
  expect_item_line_block(60);
  return keys("K", "M", "", "") && status(NULL);
}

/* V-97: a long enter replays; the tap of the empty enter run, the releases and P5.5 do nothing. */
static void test_v97_replay(void) {
  rx_open_one(LEVEL_KM, "K");
  harness_at(500U, ENTER_EVENT);
  TEST_ASSERT_EQUAL(0U, fake_player_start_count());
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  /* Hold from 1200 (playing) for 1000 ms: the long event at 2000 restarts the playback. */
  pins_at(BUTTON_BIT_ENTER, 1200U);
  harness_run_to(HARNESS_T0 + 1999U);
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  harness_run_to(HARNESS_T0 + 2000U);
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0U, fake_player_stop_count());
  TEST_ASSERT(is_km_item_waiting());
  pins_at(0U, 2200U);
  harness_run_to(HARNESS_T0 + 2600U);
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
  fake_player_finish();
  /* Hold from 3000 (waiting) for 900 ms: one replay at 3800, nothing at the release at 3900. */
  pins_at(BUTTON_BIT_ENTER, 3000U);
  harness_run_to(HARNESS_T0 + 3799U);
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
  harness_run_to(HARNESS_T0 + 3800U);
  TEST_ASSERT_EQUAL(3U, fake_player_start_count());
  pins_at(0U, 3900U);
  harness_run_to(HARNESS_T0 + 4400U);
  TEST_ASSERT_EQUAL(3U, fake_player_start_count());
  fake_player_finish();
  /* P5.5 is not a button of this harness; the fake keyer is off in Receiving. */
  harness_run_to(HARNESS_T0 + 4999U);
  TEST_ASSERT(is_km_item_waiting());
  harness_at(5000U, L_TAP);
  expect_item_line_big(60, "K");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(3U, fake_player_start_count());
  TEST_ASSERT_EQUAL(1U, fake_sidetone_call_count());
  TEST_ASSERT(!fake_keyer_is_running());
}

/* V-98, V-99 */
static void test_v98_v99_wrong_attempts(void) {
  const char* const items[2] = {"K", "M"};
  uint16_t t;
  long stop_index;

  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
  harness_run_to(HARNESS_T0 + 1000U);
  harness_at(1300U, M_TAP);
  expect_item_line_big(60, "M");
  TEST_ASSERT(status("Wrong"));
  TEST_ASSERT_EQUAL(1U, fake_player_stop_count());
  harness_run_to(HARNESS_T0 + 2499U);
  TEST_ASSERT(status("Wrong"));
  harness_run_to(HARNESS_T0 + 2500U);
  TEST_ASSERT(status("Try 2 of 3"));
  expect_item_line_block(60);
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0, strcmp("K", fake_player_last_text()));
  TEST_ASSERT(hdr("RX 32/32 L1          1/50"));

  harness_at(2600U, M_TAP);
  TEST_ASSERT(status("Wrong"));
  harness_run_to(HARNESS_T0 + 3800U);
  TEST_ASSERT(status("Try 3 of 3"));
  harness_at(4000U, M_TAP);
  TEST_ASSERT(status("Wrong. The answer is:"));
  expect_item_line_big(60, "K");
  TEST_ASSERT_EQUAL(4U, fake_player_start_count());
  stop_index = last_player_call(FAKE_PLAYER_CALL_STOP);
  TEST_ASSERT(stop_index >= 0);
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_START, fake_player_call_at((size_t)stop_index + 1U));
  TEST_ASSERT(fake_player_is_running());
  /* The answer plays for 5 s; the delay counts from its end. */
  harness_run_to(HARNESS_T0 + 9000U);
  TEST_ASSERT(status("Wrong. The answer is:"));
  fake_player_finish();
  t = harness_now;
  harness_run_to((uint16_t)(t + 2499U));
  TEST_ASSERT(status("Wrong. The answer is:"));
  expect_item_line_big(60, "K");
  harness_run_to((uint16_t)(t + 2501U));
  TEST_ASSERT(body_off(1, 5));
  TEST_ASSERT(hdr("RX 32/32 L1          2/50"));
  harness_run_to((uint16_t)(t + 3001U));
  expect_item_line_block(60);
  TEST_ASSERT_EQUAL(5U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0, strcmp("M", fake_player_last_text()));
  TEST_ASSERT_EQUAL(0U, training_score());
}

/* V-100 */
static void test_v100_retry_not_scored(void) {
  const char* const items[2] = {"K", "K"};

  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
  harness_run_to(HARNESS_T0 + 1000U);
  tap(M_TAP);
  TEST_ASSERT(status("Wrong"));
  run_to_item();
  TEST_ASSERT(status("Try 2 of 3"));
  tap(L_TAP);
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(0U, training_score());
  run_to_item();
  TEST_ASSERT_EQUAL(2U, training_item_number());
  expect_item_line_block(60);
}

/* V-101 */
static void test_v101_word_picks(void) {
  rx_open_one(LEVEL_WORDS_13, "KMRS");
  harness_run_to(HARNESS_T0 + 1000U);
  pick('K');
  expect_word_slots("K", 4U);
  pick('M');
  expect_word_slots("KM", 4U);
  pick('R');
  expect_word_slots("KMR", 4U);
  TEST_ASSERT(status(NULL));
  pick('S');
  item_expect_reset();
  item_expect(ITEM_PART_BIG, 36, "K");
  item_expect(ITEM_PART_BIG, 52, "M");
  item_expect(ITEM_PART_BIG, 68, "R");
  item_expect(ITEM_PART_BIG, 84, "S");
  TEST_ASSERT(item_line_is());
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_score());
}

/* V-102 */
static void test_v102_word_keyboard(void) {
  rx_open_one(LEVEL_ALL_WORDS, "KM");
  TEST_ASSERT(keys("A-J", "K-T", "U-3", "4-/"));
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT(keys("A-J", "K-T", "U-3", "4-/"));
  TEST_ASSERT_EQUAL(40U, keyboard_count());
  tap(ENTER_EVENT);
  TEST_ASSERT(keys("456", "789", ".,", "?/"));
  TEST_ASSERT(margins_off());
}

/* V-103: events in the result and the blank moment change nothing. */
static void test_v103_ignored_in_delays(void) {
  const char* const items[2] = {"K", "K"};
  uint16_t t;

  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
  harness_run_to(HARNESS_T0 + 1000U);
  tap(L_TAP);
  t = harness_now;
  snapshot(snapshot_before);
  harness_run_to((uint16_t)(t + 100U));
  harness_event(L_TAP);
  harness_event(M_TAP);
  harness_event(R_TAP);
  harness_event(ENTER_EVENT);
  harness_event(LONG_EVENT(M_TAP));
  harness_event(LONG_EVENT(ENTER_EVENT));
  TEST_ASSERT(is_screen_unchanged());
  harness_run_to((uint16_t)(t + 700U));
  snapshot(snapshot_before);
  harness_event(L_TAP);
  harness_event(M_TAP);
  harness_event(R_TAP);
  harness_event(ENTER_EVENT);
  harness_event(LONG_EVENT(M_TAP));
  harness_event(LONG_EVENT(ENTER_EVENT));
  TEST_ASSERT(is_screen_unchanged());
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  TEST_ASSERT_EQUAL(1U, fake_player_stop_count());
  TEST_ASSERT_EQUAL(2U, fake_player_call_count());
  harness_run_to((uint16_t)(t + 1100U));
  expect_item_line_block(60);
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
}

/* Main menu with the cursor on @p row and tap mode off: a NEXT press acts at the press. */
static void expect_main_menu_after_leave(uint8_t row) {
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(row, menu_cursor());
  TEST_ASSERT(hdr("CW Trainer"));
  harness_pins_for(BUTTON_BIT_NEXT, (uint16_t)(BUTTONS_DEBOUNCE_MS + 1U));
  TEST_ASSERT_EQUAL((uint8_t)(row + 1U), menu_cursor());
  harness_pins_for(0U, 30U);
}

/* V-104 */
static void test_v104_leave_with_long_menu(void) {
  uint8_t before[FB_SIZE];

  rx_open_one(LEVEL_KM, "K");
  harness_run_to(HARNESS_T0 + 1179U);
  harness_pins_for(BUTTON_BIT_MENU, 21U);
  snapshot(before);
  harness_pins_for(BUTTON_BIT_MENU, 799U);
  memcpy(snapshot_before, before, FB_SIZE);
  TEST_ASSERT(is_screen_unchanged());
  TEST_ASSERT_EQUAL((uint16_t)(HARNESS_T0 + 1999U), harness_now);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  harness_pins_for(BUTTON_BIT_MENU, 1U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_STOP, fake_player_call_at(fake_player_call_count() - 1U));
  TEST_ASSERT(!fake_player_is_running());
  harness_pins_for(0U, 30U);
  expect_main_menu_after_leave(0U);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(3U));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U));
  TEST_ASSERT_EQUAL(0U, progress_level(PROGRESS_MODE_RECEIVING, 32U));

  /* D-T24: a hold of menu/back from 100 ms (start pause) leaves at 900 ms, before the first item plays. */
  rx_open_one(LEVEL_KM, "K");
  harness_run_to(HARNESS_T0 + 79U);
  harness_pins_for(BUTTON_BIT_MENU, 820U);
  TEST_ASSERT_EQUAL((uint16_t)(HARNESS_T0 + 899U), harness_now);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  harness_pins_for(BUTTON_BIT_MENU, 1U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(0U, fake_player_start_count());
  harness_pins_for(0U, 30U);
  expect_main_menu_after_leave(0U);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(3U));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U));

  /* A hold from 300 ms: the first item plays at 1000 ms, and the leave at 1100 ms stops it. */
  rx_open_one(LEVEL_KM, "K");
  harness_run_to(HARNESS_T0 + 279U);
  harness_pins_for(BUTTON_BIT_MENU, 721U);
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_START, fake_player_call_at(0U));
  TEST_ASSERT_EQUAL(0, strcmp("K", fake_player_last_text()));
  harness_pins_for(BUTTON_BIT_MENU, 99U);
  TEST_ASSERT_EQUAL((uint16_t)(HARNESS_T0 + 1099U), harness_now);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT_EQUAL(1U, fake_player_call_count());
  harness_pins_for(BUTTON_BIT_MENU, 1U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(2U, fake_player_call_count());
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_STOP, fake_player_call_at(1U));
  TEST_ASSERT(!fake_player_is_running());
  harness_pins_for(0U, 30U);
  expect_main_menu_after_leave(0U);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(3U));
}

/* Answers the current item: the right pick, after one wrong pick when @p is_first_wrong. Then runs to the next. */
static void answer_km_item(bool is_first_wrong) {
  const char item = training_item_text()[0];
  const button_event_t right = (item == 'K') ? L_TAP : M_TAP;
  const button_event_t wrong = (item == 'K') ? M_TAP : L_TAP;

  fake_player_finish();
  if (is_first_wrong) {
    tap(wrong);
    run_to_item();
  }
  tap(right);
}

/* Runs a KM session of 50 items with @p first_correct first-attempt answers, up to its end screen. */
static void run_km_session(uint8_t first_correct) {
  uint8_t number;
  unsigned guard = 0U;

  for (number = 1U; number <= 50U; ++number) {
    run_to_item();
    answer_km_item(number > first_correct);
  }
  while ((training_phase() != TRAINING_PHASE_END) && (guard < 2000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
}

static void open_km_50(void) {
  const char* const items[2] = {"K", "M"};
  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
}

/* V-105 */
static void test_v105_pass(void) {
  open_km_50();
  run_km_session(45U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT(hdr("RX 32/32 L1         50/50"));
  TEST_ASSERT(text_at(2, 7, "Score 45/50"));
  TEST_ASSERT(text_at(3, 9, "Passed"));
  TEST_ASSERT(text_at(4, 6, "New level L2"));
  TEST_ASSERT(blank_row(1));
  TEST_ASSERT(blank_row(5));
  TEST_ASSERT(blank_row(6));
  TEST_ASSERT(next_button());
  TEST_ASSERT(margins_off());
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(1U));
  TEST_ASSERT_EQUAL(0U, fake_iap_erase_count(0U));
  TEST_ASSERT_EQUAL(1U, fake_iap_program_count(0x0200U));
  /* No blank moment before the end screen: 50 GAP events only before items 2..50. */
  TEST_ASSERT_EQUAL(50U, training_item_number());
  snapshot(snapshot_before);
  harness_pins_for(BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 30U));
  TEST_ASSERT(is_screen_unchanged());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  harness_pins_for(0U, BUTTONS_DEBOUNCE_MS);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  harness_pins_for(0U, 1U);
  TEST_ASSERT(hdr("RX 32/32 L2          1/50"));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(body_off(2, 5));
  TEST_ASSERT(keys("K", "M", "R", ""));
  TEST_ASSERT(margins_off());
}

/* V-106 */
static void test_v106_fail(void) {
  open_km_50();
  run_km_session(44U);
  TEST_ASSERT(text_at(2, 7, "Score 44/50"));
  TEST_ASSERT(text_at(3, 3, "Not passed, need 45"));
  TEST_ASSERT(text_at(4, 5, "Repeat level L1"));
  TEST_ASSERT(blank_row(5));
  TEST_ASSERT(next_button());
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(3U));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U));
  tap(ENTER_EVENT);
  TEST_ASSERT(hdr("RX 32/32 L1          1/50"));
}

/* V-107 */
static void test_v107_not_saved(void) {
  open_km_50();
  fake_iap_set_fail_operation(2U);
  run_km_session(50U);
  TEST_ASSERT(text_at(4, 6, "New level L2"));
  TEST_ASSERT(text_at(5, 8, "Not saved"));
  TEST_ASSERT(next_button());
  tap(ENTER_EVENT);
  TEST_ASSERT(hdr("RX 32/32 L2          1/50"));
  /* Re-init from the fake EEPROM. */
  fake_iap_clear_counters();
  harness_power_on();
  fake_koch_item_reset();
  fake_koch_item_push("K");
  harness_open_mode(0U);
  TEST_ASSERT(hdr("RX 32/32 L1          1/50"));
}

/* V-108 (Receiving part): the end screen ignores all but enter and menu/back. */
static void test_v108_end_screen_events(void) {
  size_t calls;

  open_km_50();
  run_km_session(45U);
  calls = fake_player_call_count();
  snapshot(snapshot_before);
  tap(M_TAP);
  tap(R_TAP);
  tap(REPEAT_EVENT(R_TAP));
  tap(LONG_EVENT(M_TAP));
  tap(LONG_EVENT(R_TAP));
  tap(LONG_EVENT(ENTER_EVENT));
  harness_pins_for(BUTTON_BIT_NEXT, 2000U);
  harness_pins_for(0U, 30U);
  /* TIP(t, 1000): nothing at 800 ms and nothing at the release. */
  harness_pins_for(BUTTON_BIT_ENTER, 1000U);
  harness_pins_for(0U, 30U);
  TEST_ASSERT(is_screen_unchanged());
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  pin_tap(BUTTON_BIT_MENU);
  expect_main_menu_after_leave(0U);

  /* A long menu/back on the end screen also leaves. */
  open_km_50();
  run_km_session(45U);
  pin_long(BUTTON_BIT_MENU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
}

/* V-109 */
static void test_v109_top_levels(void) {
  static const uint16_t levels[2] = {65534U, 65535U};
  size_t index;

  for (index = 0U; index < 2U; ++index) {
    uint8_t number;

    fake_iap_reset();
    progress_fixture_write(PROGRESS_MODE_RECEIVING, 50U, levels[index]);
    harness_power_on();
    (void)settings_set(SETTINGS_ID_RX_WPM, 50U);
    (void)settings_set(SETTINGS_ID_RX_EFF_WPM, 50U);
    fake_koch_item_reset();
    fake_koch_item_push("KMRSU");
    harness_open_mode(0U);
    TEST_ASSERT(hdr("RX 50/50 L65535     1/100"));
    for (number = 0U; number < 6U; ++number) {
      run_to_item();
      fake_player_finish();
      pick('K');
      pick('M');
      pick('R');
      pick('S');
      pick('U');
    }
    harness_run_to((uint16_t)(harness_now + 800U));
    TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
    TEST_ASSERT(hdr("RX 50/50 L65535     7/100"));
    TEST_ASSERT_EQUAL(6U, training_score());
  }
}

/* Character level index with @p learned characters (training_spec.md section 6). */
static uint16_t char_level(uint8_t learned) {
  return ((learned % 2U) == 0U) ? (uint16_t)(3U * ((learned - 2U) / 2U)) : (uint16_t)(3U * ((learned - 3U) / 2U) + 1U);
}

/* A tap per letter of @p taps: L, M, R, E of ui-ux.md section 9.4. */
static void tap_letters(const char* taps) {
  for (; *taps != '\0'; ++taps) {
    tap((*taps == 'L') ? L_TAP : ((*taps == 'M') ? M_TAP : ((*taps == 'R') ? R_TAP : ENTER_EVENT)));
  }
}

typedef struct {
  uint16_t level;
  uint8_t style;
  const char* taps;
  const char* labels[4];
} label_row_t;

#define PUNCT SETTINGS_PROSIGN_PUNCTUATION
#define NAME SETTINGS_PROSIGN_NAME
#define BOTH SETTINGS_PROSIGN_BOTH

/* The label tables and the longest labels of ui-ux.md section 5.12.1 (UX-51). */
static void test_d1_label_tables(void) {
  static const label_row_t rows[] = {
      {LEVEL_ALL_CHARS, BOTH, "", {"A-K", "L-V", "W-6", "7-<AR>"}},
      {LEVEL_ALL_CHARS, BOTH, "L", {"ABC", "DEF", "GHI", "JK"}},
      {LEVEL_ALL_CHARS, BOTH, "M", {"LMN", "OPQ", "RST", "UV"}},
      {LEVEL_ALL_CHARS, BOTH, "R", {"WXY", "Z01", "234", "56"}},
      {LEVEL_ALL_CHARS, NAME, "E", {"789", ".,?", "/<BT>", "<SK><AR>"}},
      {LEVEL_ALL_CHARS, NAME, "ER", {"/", "<BT>", "", ""}},
      {LEVEL_ALL_CHARS, NAME, "EE", {"<SK>", "<AR>", "", ""}},
      {LEVEL_ALL_CHARS, PUNCT, "", {"A-K", "L-V", "W-6", "7-+"}},
      {LEVEL_ALL_CHARS, PUNCT, "E", {"789", ".,?", "/=", "<SK>+"}},
      {LEVEL_ALL_CHARS, PUNCT, "ER", {"/", "=", "", ""}},
      {LEVEL_ALL_CHARS, PUNCT, "EE", {"<SK>", "+", "", ""}},
      {LEVEL_ALL_WORDS, PUNCT, "", {"A-J", "K-T", "U-3", "4-/"}},
      {LEVEL_ALL_WORDS, BOTH, "L", {"ABC", "DEF", "GH", "IJ"}},
      {LEVEL_ALL_WORDS, BOTH, "M", {"KLM", "NOP", "QR", "ST"}},
      {LEVEL_ALL_WORDS, BOTH, "R", {"UVW", "XYZ", "01", "23"}},
      {LEVEL_ALL_WORDS, NAME, "E", {"456", "789", ".,", "?/"}},
      {58U, BOTH, "", {"A-K", "L-U", "V-4", "5-<BT>"}},
      {58U, PUNCT, "", {"A-K", "L-U", "V-4", "5-="}},
      {58U, BOTH, "E", {"567", "89.", ",?", "/<BT>"}},
      {58U, PUNCT, "E", {"567", "89.", ",?", "/="}},
      {58U, BOTH, "EE", {"/", "<BT>", "", ""}},
      {58U, PUNCT, "EE", {"/", "=", "", ""}},
      {60U, NAME, "", {"A-K", "L-V", "W-5", "6-<SK>"}},
      {60U, BOTH, "E", {"678", "9.,", "?/", "<BT><SK>"}},
      {60U, PUNCT, "E", {"678", "9.,", "?/", "=<SK>"}},
      {60U, BOTH, "EE", {"<BT>", "<SK>", "", ""}},
      {60U, PUNCT, "EE", {"=", "<SK>", "", ""}},
  };
  static const struct {
    uint8_t learned;
    const char* labels[4];
  } longest[] = {
      {17U, {"AEFIJ", "KLMN", "OPRS", "TUW."}},   {18U, {"AEFIJ", "KLMNO", "PRST", "UW0."}},
      {19U, {"AEFIJ", "KLMNO", "PRSTU", "WY0."}}, {28U, {"A-J", "K-Q", "R-Y", "Z059.,/"}},
      {32U, {"A-J", "K-R", "S-0", "3589.,?/"}},
  };
  const char* const items[1] = {"K"};
  size_t index;

  for (index = 0U; index < sizeof(rows) / sizeof(rows[0]); ++index) {
    const label_row_t* row = &rows[index];

    rx_open(row->level, row->style, items, 1U);
    harness_run_to(HARNESS_T0 + 1000U);
    tap_letters(row->taps);
    if (!TEST_ASSERT(keys(row->labels[0], row->labels[1], row->labels[2], row->labels[3]))) {
      fprintf(stderr, "label row %zu\n", index);
    }
    TEST_ASSERT(margins_off());
  }
  for (index = 0U; index < sizeof(longest) / sizeof(longest[0]); ++index) {
    rx_open(char_level(longest[index].learned), BOTH, items, 1U);
    TEST_ASSERT_EQUAL(longest[index].learned, keyboard_count());
    TEST_ASSERT(
        keys(longest[index].labels[0], longest[index].labels[1], longest[index].labels[2], longest[index].labels[3]));
  }
}

#define LABEL_TEXT_MAX 64U

/* The label rule of ui-ux.md section 5.12.1 for run @p run of the current range. Returns the cells of the label. */
static size_t model_label(uint8_t run, uint8_t style, char* label) {
  const uint8_t short_style = (style == SETTINGS_PROSIGN_PUNCTUATION) ? MORSE_STYLE_PUNCTUATION : MORSE_STYLE_NAME;
  const uint8_t first = keyboard_group_first(run);
  const uint8_t size = keyboard_group_size(run);
  const size_t cells = (run == KEYBOARD_GROUP_ENTER) ? 8U : 5U;
  char text[MORSE_TEXT_SIZE];
  uint8_t index;

  label[0] = '\0';
  for (index = 0U; index < size; ++index) {
    (void)morse_format(keyboard_char((uint8_t)(first + index)), short_style, text);
    strcat(label, text);
  }
  if (strlen(label) > cells) {
    (void)morse_format(keyboard_char(first), short_style, text);
    strcpy(label, text);
    strcat(label, "-");
    (void)morse_format(keyboard_char((uint8_t)(first + size - 1U)), short_style, text);
    strcat(label, text);
  }
  return strlen(label);
}

static unsigned walk_screens;

/* Checks the labels of the current range, then narrows into every run of 2 or more and steps back with LONG(M). */
static void walk_ranges(uint8_t style) {
  char labels[4][LABEL_TEXT_MAX];
  uint8_t run;

  for (run = 0U; run < KEYBOARD_GROUP_COUNT; ++run) {
    const size_t cells = (run == KEYBOARD_GROUP_ENTER) ? 8U : 5U;
    TEST_ASSERT(model_label(run, style, labels[run]) <= cells);
  }
  if (!TEST_ASSERT(keys(labels[0], labels[1], labels[2], labels[3]) && margins_off())) {
    fprintf(stderr, "labels %s|%s|%s|%s, style %u, %u learned\n", labels[0], labels[1], labels[2], labels[3],
            (unsigned)style, (unsigned)keyboard_count());
  }
  ++walk_screens;
  for (run = 0U; run < KEYBOARD_GROUP_COUNT; ++run) {
    if (keyboard_group_size(run) >= 2U) {
      tap(group_taps[run]);
      walk_ranges(style);
      tap(LONG_EVENT(M_TAP));
    }
  }
}

/* V-121, LC-6: every label of every reachable range, every level kind and every Prosigns value fits its field. */
static void test_v121_labels_fit_every_set(void) {
  static const uint8_t styles[3] = {SETTINGS_PROSIGN_PUNCTUATION, SETTINGS_PROSIGN_NAME, SETTINGS_PROSIGN_BOTH};
  const char* const items[1] = {"K"};
  uint16_t level;
  size_t style;

  walk_screens = 0U;
  for (level = 0U; level <= KOCH_FINAL_LEVEL; ++level) {
    for (style = 0U; style < 3U; ++style) {
      rx_open(level, styles[style], items, 1U);
      run_to_item();
      walk_ranges(styles[style]);
      TEST_ASSERT_EQUAL(0U, training_answer_length());
      TEST_ASSERT_EQUAL(0U, keyboard_depth());
    }
  }
  TEST_ASSERT(walk_screens > 1000U);
}

/* V-120: the Prosigns setting is read once at the mode start. */
static void test_v120_prosigns_read_once(void) {
  rx_open_one(LEVEL_ALL_CHARS, "K");
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT(keys("A-K", "L-V", "W-6", "7-<AR>"));
  (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, SETTINGS_PROSIGN_PUNCTUATION);
  tap(ENTER_EVENT);
  TEST_ASSERT(keys("789", ".,?", "/<BT>", "<SK><AR>"));
  pin_long(BUTTON_BIT_PREV);
  TEST_ASSERT(keys("A-K", "L-V", "W-6", "7-<AR>"));
  pin_long(BUTTON_BIT_MENU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  harness_set_time(HARNESS_T0);
  harness_event(ENTER_EVENT);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT(keys("A-K", "L-V", "W-6", "7-+"));
}

/* V-122: a long enter replays a word; the pick and the current run stay. */
static void test_v122_word_replay(void) {
  rx_open_one(LEVEL_WORDS_13, "KMRS");
  harness_run_to(HARNESS_T0 + 1000U);
  tap(L_TAP);
  tap(R_TAP);
  tap(L_TAP);
  TEST_ASSERT(keys("A", "I", "K", "L"));
  /* Hold while the item plays: a restart at 800 ms of the hold, nothing at the release. */
  pins_at(BUTTON_BIT_ENTER, 1100U);
  harness_run_to(HARNESS_T0 + 1899U);
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  harness_run_to(HARNESS_T0 + 1900U);
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0, strcmp("KMRS", fake_player_last_text()));
  pins_at(0U, 2100U);
  harness_run_to(HARNESS_T0 + 2200U);
  fake_player_finish();
  /* Hold while waiting. */
  pins_at(BUTTON_BIT_ENTER, 2500U);
  harness_run_to(HARNESS_T0 + 3300U);
  TEST_ASSERT_EQUAL(3U, fake_player_start_count());
  pins_at(0U, 3500U);
  harness_run_to(HARNESS_T0 + 3600U);
  TEST_ASSERT_EQUAL(3U, fake_player_start_count());
  TEST_ASSERT_EQUAL(0U, fake_player_stop_count());
  expect_word_slots("K", 4U);
  TEST_ASSERT(keys("A", "I", "K", "L"));
  TEST_ASSERT(status(NULL));
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  TEST_ASSERT_EQUAL(1U, keyboard_depth());
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
}

#define LEVEL_KMRS 3U

/* Checks the pick of S by the enter run at HARNESS_T0 + @p t: the result, and the player started once and stopped. */
static void expect_s_picked_at(uint16_t t) {
  TEST_ASSERT_EQUAL((uint16_t)(HARNESS_T0 + t), harness_now);
  expect_item_line_big(60, "S");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(2U, fake_player_call_count());
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_START, fake_player_call_at(0U));
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_STOP, fake_player_call_at(1U));
}

/* Screen of a KMRS item before any pick. */
static bool is_kmrs_item_waiting(void) {
  expect_item_line_block(60);
  return keys("K", "M", "R", "S") && status(NULL) && (training_answer_length() == 0U);
}

/* V-123: a short tip closure is an enter tap: it selects the enter run at the release. */
static void test_v123_short_tip_selects(void) {
  rx_open_one(LEVEL_KMRS, "S");
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT(keys("K", "M", "R", "S"));
  harness_run_to(HARNESS_T0 + 1499U);
  harness_pins_for(BUTTON_BIT_ENTER, 100U);
  TEST_ASSERT(is_kmrs_item_waiting());
  harness_pins_for(0U, BUTTONS_DEBOUNCE_MS - 1U);
  TEST_ASSERT(is_kmrs_item_waiting());
  harness_run_to(HARNESS_T0 + 1620U);
  expect_s_picked_at(1620U);

  rx_open_one(LEVEL_KMRS, "S");
  harness_at(1500U, ENTER_EVENT);
  expect_s_picked_at(1500U);
}

/* V-124: a tip closure of 800 ms replays; a closure of 700 ms then selects. */
static void test_v124_tip_replay(void) {
  rx_open_one(LEVEL_KMRS, "S");
  harness_run_to(HARNESS_T0 + 1499U);
  harness_pins_for(BUTTON_BIT_ENTER, 800U);
  harness_pins_for(0U, BUTTONS_DEBOUNCE_MS - 1U);
  TEST_ASSERT_EQUAL(1U, fake_player_start_count());
  harness_run_to(HARNESS_T0 + 2320U);
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
  TEST_ASSERT(is_kmrs_item_waiting());
  harness_run_to(HARNESS_T0 + 3000U);
  fake_player_finish();
  tip(3500U, 700U);
  harness_run_to(HARNESS_T0 + 4219U);
  TEST_ASSERT(is_kmrs_item_waiting());
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
  harness_run_to(HARNESS_T0 + 4220U);
  expect_item_line_big(60, "S");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(2U, fake_player_start_count());
}

/* Opens Receiving at level KMRS with a real enter press whose debounced press comes at HARNESS_T0. */
static void rx_open_by_enter_pin(void) {
  fake_iap_reset();
  progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, LEVEL_KMRS);
  harness_power_on();
  fake_koch_item_reset();
  fake_koch_item_push("S");
  harness_set_time((uint16_t)(HARNESS_T0 - BUTTONS_DEBOUNCE_MS - 1U));
  harness_pins_for(BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 1U));
}

/* V-125: the enter press that opens Receiving gives nothing at its release and no long event. */
static void test_v125_opening_enter(void) {
  static const uint16_t hold_ends[2] = {300U, 1500U};
  size_t run;

  for (run = 0U; run < 2U; ++run) {
    rx_open_by_enter_pin();
    TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
    TEST_ASSERT_EQUAL((uint16_t)HARNESS_T0, harness_now);
    harness_pins_for(BUTTON_BIT_ENTER, hold_ends[run]);
    harness_pins_for(0U, 100U);
    harness_run_to(HARNESS_T0 + 1999U);
    TEST_ASSERT_EQUAL(1U, fake_player_call_count());
    TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_START, fake_player_call_at(0U));
    TEST_ASSERT(is_kmrs_item_waiting());
    harness_at(2000U, ENTER_EVENT);
    expect_item_line_big(60, "S");
    TEST_ASSERT(status("Correct"));
  }
}

/* V-126: a long enter in the start pause, the blank moment and a result does nothing. */
static void test_v126_long_enter_outside_item(void) {
  const char* const items[2] = {"K", "K"};

  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
  pins_at(BUTTON_BIT_ENTER, 100U);
  harness_run_to(HARNESS_T0 + 999U);
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT_EQUAL(1U, fake_player_call_count());
  pins_at(0U, 1100U);
  harness_at(1500U, L_TAP);
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(2U, fake_player_call_count());
  /* Long event at 2320, in the blank moment. */
  pins_at(BUTTON_BIT_ENTER, 1520U);
  harness_run_to(HARNESS_T0 + 2320U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT(body_off(1, 5));
  TEST_ASSERT_EQUAL(2U, fake_player_call_count());
  pins_at(0U, 2520U);
  harness_run_to(HARNESS_T0 + 2600U);
  TEST_ASSERT_EQUAL(3U, fake_player_call_count());
  harness_at(3000U, M_TAP);
  TEST_ASSERT(status("Wrong"));
  /* Long event at 3900, in the result. */
  pins_at(BUTTON_BIT_ENTER, 3100U);
  harness_run_to(HARNESS_T0 + 3900U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  TEST_ASSERT(status("Wrong"));
  TEST_ASSERT_EQUAL(4U, fake_player_call_count());
  pins_at(0U, 4100U);
  harness_run_to(HARNESS_T0 + 4200U);
  TEST_ASSERT(status("Try 2 of 3"));
  TEST_ASSERT_EQUAL(5U, fake_player_call_count());
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_START, fake_player_call_at(0U));
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_STOP, fake_player_call_at(1U));
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_START, fake_player_call_at(2U));
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_STOP, fake_player_call_at(3U));
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_START, fake_player_call_at(4U));
}

static void clear_dirty_pages(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
}

/* V-127: a tap of an empty run changes nothing: no redraw, no dirty page, no player call. */
static void test_v127_empty_runs(void) {
  size_t calls;

  rx_open_one(LEVEL_KM, "K");
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT(keys("K", "M", "", ""));
  calls = fake_player_call_count();
  snapshot(snapshot_before);
  clear_dirty_pages();
  tap(ENTER_EVENT);
  tap(R_TAP);
  TEST_ASSERT(is_screen_unchanged());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  tap(L_TAP);
  expect_item_line_big(60, "K");
  TEST_ASSERT(status("Correct"));

  rx_open_one(LEVEL_KMR, "K");
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT(keys("K", "M", "R", ""));
  calls = fake_player_call_count();
  snapshot(snapshot_before);
  clear_dirty_pages();
  tap(ENTER_EVENT);
  TEST_ASSERT(is_screen_unchanged());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  tap(L_TAP);
  expect_item_line_big(60, "K");
  TEST_ASSERT(status("Correct"));
}

/* AC-T27: each button selects the run under it (menu/back 0, prev 1, next 2, enter 3), with the real pins. */
static void test_button_to_run_mapping(void) {
  static const uint8_t pins[4] = {BUTTON_BIT_MENU, BUTTON_BIT_PREV, BUTTON_BIT_NEXT, BUTTON_BIT_ENTER};
  const char* const items[4] = {"K", "M", "R", "S"};
  size_t index;

  rx_open(LEVEL_KMRS, SETTINGS_PROSIGN_BOTH, items, 4U);
  for (index = 0U; index < 4U; ++index) {
    char picked[2];

    run_to_item();
    picked[0] = training_item_text()[0];
    picked[1] = '\0';
    TEST_ASSERT_EQUAL((uint8_t)keyboard_char((uint8_t)index), (uint8_t)picked[0]);
    pin_tap(pins[index]);
    expect_item_line_big(60, picked);
    TEST_ASSERT(status("Correct"));
  }
  TEST_ASSERT_EQUAL(4U, training_score());
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
}

/* LC-7: a narrowing writes row 7 only; the key bars and the other rows stay. */
static void test_label_redraw_writes_row_7_only(void) {
  rx_open_one(LEVEL_ALL_CHARS, "K");
  harness_run_to(HARNESS_T0 + 1000U);
  clear_dirty_pages();
  tap(ENTER_EVENT);
  TEST_ASSERT_EQUAL(0x80U, lcd_fb_dirty_mask());
  TEST_ASSERT(keys("789", ".,?", "/<BT>", "<SK><AR>"));
  clear_dirty_pages();
  pin_long(BUTTON_BIT_PREV);
  TEST_ASSERT_EQUAL(0x80U, lcd_fb_dirty_mask());
  TEST_ASSERT(keys("A-K", "L-V", "W-6", "7-<AR>"));
}

#define LEVEL_FIRST_WORDS 2U /* word session 1: 10 words of 2, K M R learned */
#define LEVEL_100_WORDS 153U /* final level of 100 words */

/* Answers a KM word item: right on the first attempt, or wrong (MM) in all three attempts. */
static void rx_answer_km_word(bool is_right) {
  uint8_t attempt;

  fake_player_finish();
  if (is_right) {
    pick('K');
    pick('M');
    return;
  }
  for (attempt = 0U; attempt < TRAINING_ATTEMPTS; ++attempt) {
    run_to_item();
    fake_player_finish();
    pick('M');
    pick('M');
  }
  /* The answer playback holds the result (D-T14). */
  fake_player_finish();
}

/* A Receiving session of KM words at @p level with @p first_correct items right on the first attempt. */
static void rx_run_km_words(uint16_t level, uint8_t first_correct) {
  const char* const items[1] = {"KM"};
  uint8_t number;
  uint8_t count;
  unsigned guard = 0U;

  rx_open(level, SETTINGS_PROSIGN_BOTH, items, 1U);
  count = training_item_count();
  for (number = 1U; number <= count; ++number) {
    run_to_item();
    rx_answer_km_word(number <= first_correct);
  }
  while ((training_phase() != TRAINING_PHASE_END) && (guard < 5000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
}

/* V-136, AC-T29 (2): word sessions pass at 80 % (D-T33). */
static void test_v136_word_pass_rate(void) {
  rx_run_km_words(LEVEL_FIRST_WORDS, 7U);
  TEST_ASSERT(text_at(2, 7, "Score 7/10"));
  TEST_ASSERT(text_at(3, 3, "Not passed, need 8"));
  TEST_ASSERT(end_screen_is(7U, 10U, 8U, 3U));
  TEST_ASSERT_EQUAL(LEVEL_FIRST_WORDS, progress_level(PROGRESS_MODE_RECEIVING, 32U));

  rx_run_km_words(LEVEL_FIRST_WORDS, 8U);
  TEST_ASSERT(text_at(3, 9, "Passed"));
  TEST_ASSERT(end_screen_is(8U, 10U, 8U, 3U));
  TEST_ASSERT_EQUAL(LEVEL_FIRST_WORDS + 1U, progress_level(PROGRESS_MODE_RECEIVING, 32U));

  rx_run_km_words(LEVEL_100_WORDS, 79U);
  TEST_ASSERT(text_at(2, 6, "Score 79/100"));
  TEST_ASSERT(text_at(3, 3, "Not passed, need 80"));
  TEST_ASSERT(end_screen_is(79U, 100U, 80U, 154U));

  rx_run_km_words(LEVEL_100_WORDS, 80U);
  TEST_ASSERT(text_at(3, 9, "Passed"));
  TEST_ASSERT(end_screen_is(80U, 100U, 80U, 154U));
}

/* AC-T19: a long menu/back leaves from every phase; the player stops and tap mode goes off each time. */
static void test_leave_in_each_phase(void) {
  const char* const items[2] = {"K", "K"};
  size_t stops;

  /* RESULT */
  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
  run_to_item();
  tap(L_TAP);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  stops = fake_player_stop_count();
  harness_event(LONG_EVENT(L_TAP));
  TEST_ASSERT_EQUAL(stops + 1U, fake_player_stop_count());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
  expect_main_menu_after_leave(0U);

  /* RESULT of the third wrong attempt, during the answer playback */
  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
  run_to_item();
  tap(M_TAP);
  run_to_item();
  tap(M_TAP);
  run_to_item();
  tap(M_TAP);
  TEST_ASSERT(fake_player_is_running());
  harness_event(LONG_EVENT(L_TAP));
  TEST_ASSERT(!fake_player_is_running());
  expect_main_menu_after_leave(0U);

  /* Blank moment */
  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
  run_to_item();
  tap(L_TAP);
  harness_run_to((uint16_t)(harness_now + 700U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  harness_event(LONG_EVENT(L_TAP));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  expect_main_menu_after_leave(0U);
  TEST_ASSERT_EQUAL(0U, harness_overlap_count);
  TEST_ASSERT(!fake_keyer_is_running());
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
}

int main(void) {
  TEST_RUN(test_v90_start);
  TEST_RUN(test_v91_correct_and_gap);
  TEST_RUN(test_v92_narrow_and_pick);
  TEST_RUN(test_v93_tail_labels);
  TEST_RUN(test_v94_word_undo);
  TEST_RUN(test_v95_hold_prev);
  TEST_RUN(test_v96_holds);
  TEST_RUN(test_v97_replay);
  TEST_RUN(test_v98_v99_wrong_attempts);
  TEST_RUN(test_v100_retry_not_scored);
  TEST_RUN(test_v101_word_picks);
  TEST_RUN(test_v102_word_keyboard);
  TEST_RUN(test_v103_ignored_in_delays);
  TEST_RUN(test_v104_leave_with_long_menu);
  TEST_RUN(test_v105_pass);
  TEST_RUN(test_v106_fail);
  TEST_RUN(test_v107_not_saved);
  TEST_RUN(test_v108_end_screen_events);
  TEST_RUN(test_v109_top_levels);
  TEST_RUN(test_d1_label_tables);
  TEST_RUN(test_v121_labels_fit_every_set);
  TEST_RUN(test_v120_prosigns_read_once);
  TEST_RUN(test_v122_word_replay);
  TEST_RUN(test_v123_short_tip_selects);
  TEST_RUN(test_v124_tip_replay);
  TEST_RUN(test_v125_opening_enter);
  TEST_RUN(test_v126_long_enter_outside_item);
  TEST_RUN(test_v127_empty_runs);
  TEST_RUN(test_button_to_run_mapping);
  TEST_RUN(test_label_redraw_writes_row_7_only);
  TEST_RUN(test_leave_in_each_phase);
  TEST_RUN(test_v136_word_pass_rate);
  return TEST_FINISH();
}
