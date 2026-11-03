/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA scenarios of unlimited attempts and the long next skip in Receiving (references/internal/training_spec.md
 * D-T53 to D-T56, D-T58 to D-T60, section 12.2, AC-T51, AC-T53, Q8 fault cases F1, F3 to F6 and mutants M3, M9,
 * M10; ui-ux.md UX-76 to UX-78, sections 5.20.1 to 5.20.3). The real menu, buttons, engine, keyboard and progress
 * run with a fake player and fake items. The oracles come from the documents: the status texts and their columns,
 * one answer playback per skip, row 7 with only the enter field, the labels of the whole set in the blank moment, an
 * rng_mix() at each acted event and none at an ignored one, and a score without the skipped item.
 */

#include "koch/keyboard.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "support/training_harness.h"
#include "util/rng.h"

#define LEVEL_KM 0U
#define LEVEL_WORDS_13 17U /* KMRSUAPTLOWI., words of 4, 10 items */
#define LEVEL_ALL_CHARS 61U
#define FB_SIZE ((size_t)LCD_PAGE_COUNT * LCD_WIDTH)
#define LONG_HOLD ((uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 100U))
#define RNG_PROBE_SEED 0x2468U

static uint8_t screen_before[FB_SIZE];

static void snapshot(uint8_t* buffer) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&buffer[(size_t)page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
}

static void mark_quiet_start(void) {
  snapshot(screen_before);
  harness_clear_dirty();
}

static bool is_quiet_since_mark(void) {
  uint8_t image[FB_SIZE];
  snapshot(image);
  return (memcmp(screen_before, image, FB_SIZE) == 0) && (lcd_fb_dirty_mask() == 0U);
}

static void rx_open_items(uint16_t level, bool is_rx_unlimited, bool is_tx_unlimited, const char* const* items,
                          size_t count) {
  size_t index;

  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, level);
  }
  harness_power_on();
  (void)settings_set(SETTINGS_ID_RX_UNLIMITED, is_rx_unlimited ? 1U : 0U);
  (void)settings_set(SETTINGS_ID_TX_UNLIMITED, is_tx_unlimited ? 1U : 0U);
  fake_koch_item_reset();
  for (index = 0U; index < count; ++index) {
    fake_koch_item_push(items[index]);
  }
  harness_open_mode(0U);
}

static void rx_open(bool is_rx_unlimited, const char* item) {
  const char* const items[1] = {item};
  rx_open_items(LEVEL_KM, is_rx_unlimited, false, items, 1U);
}

static void tap(button_event_t event) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(event);
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

static void run_to_item(void) { run_to_phase(TRAINING_PHASE_ITEM); }

static void pin_press(uint8_t pin, uint16_t duration) {
  harness_pins_for(pin, duration);
  harness_pins_for(0U, (uint16_t)(BUTTONS_DEBOUNCE_MS + 10U));
}

/* KM level: run 0 (menu/back) picks K, run 1 (prev) picks M. */
static button_event_t pick_of(char character) { return (character == 'K') ? L_TAP : M_TAP; }
static button_event_t wrong_pick_of(char character) { return (character == 'K') ? M_TAP : L_TAP; }

/* The skip result of UX-78: answer state, status, row 7 with only Next or Finish, the answer playing. */
static bool is_skip_answer_state(bool is_last_item) {
  return (training_phase() == TRAINING_PHASE_RESULT) && (training_last_check() == TRAINING_CHECK_FAILED) &&
         training_is_skipped() && status("Skipped. The answer is:") &&
         (is_last_item ? text_at(7, 19, "Finish") : text_at(7, 21, "Next")) && key_bars() && fake_player_is_running() &&
         (strcmp(training_item_text(), fake_player_last_text()) == 0);
}

/* The rng value after an event at @p tick: with rng_mix(tick) when @p is_mixed (D-T56 (1) and (2)). */
static uint16_t expected_rng(uint16_t tick, bool is_mixed) {
  rng_seed(RNG_PROBE_SEED);
  if (is_mixed) {
    rng_mix(tick);
  }
  return rng_next();
}

/* Sends @p event at the next ms with a known rng state; returns true when rng_mix() of that tick ran. */
static bool event_mixes_rng(button_event_t event) {
  uint16_t tick;
  uint16_t value;
  harness_set_time((uint16_t)(harness_now + 1U));
  tick = harness_now;
  rng_seed(RNG_PROBE_SEED);
  harness_event(event);
  value = rng_next();
  if (value == expected_rng(tick, true)) {
    return true;
  }
  TEST_ASSERT_EQUAL(expected_rng(tick, false), value);
  return false;
}

/* AC-T51 (1), D-T56 (1), UX-78: a skip in ITEM: stop, one start of the item, the answer state, rng_mix(). */
static void test_skip_in_item_answer_state(void) {
  unsigned setting;
  for (setting = 0U; setting <= 1U; ++setting) {
    size_t calls;
    rx_open(setting == 1U, "K");
    run_to_item();
    calls = fake_player_call_count();
    TEST_ASSERT(event_mixes_rng(LONG_EVENT(R_TAP)));
    TEST_ASSERT_EQUAL(calls + 2U, fake_player_call_count());
    TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_STOP, fake_player_call_at(calls));
    TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_START, fake_player_call_at(calls + 1U));
    TEST_ASSERT(is_skip_answer_state(false));
    TEST_ASSERT_EQUAL(0U, training_score());
    TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
    /* No time limit; the answer plays only once by itself. */
    fake_player_finish();
    calls = fake_player_call_count();
    harness_run_to((uint16_t)(harness_now + TRAINING_FAILED_MS + 3000U));
    TEST_ASSERT_EQUAL(calls, fake_player_call_count());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    /* A long next, the other taps and a long prev in the answer state do nothing and mix nothing. */
    mark_quiet_start();
    TEST_ASSERT(!event_mixes_rng(LONG_EVENT(R_TAP)));
    TEST_ASSERT(!event_mixes_rng(L_TAP));
    TEST_ASSERT(!event_mixes_rng(M_TAP));
    TEST_ASSERT(!event_mixes_rng(R_TAP));
    TEST_ASSERT(!event_mixes_rng(LONG_EVENT(M_TAP)));
    TEST_ASSERT(is_quiet_since_mark());
    TEST_ASSERT_EQUAL(calls, fake_player_call_count());
    /* A long enter replays, an enter tap goes on to the blank moment. */
    TEST_ASSERT(event_mixes_rng(LONG_EVENT(ENTER_EVENT)));
    TEST_ASSERT_EQUAL(calls + 1U, fake_player_call_count());
    TEST_ASSERT(event_mixes_rng(ENTER_EVENT));
    TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  }
}

/* AC-T51 (2), F6: a long next at the tick where the WRONG delay ends, in both orders of event and poll. */
static void test_skip_at_end_of_wrong_delay(void) {
  unsigned order;
  for (order = 0U; order < 2U; ++order) {
    uint16_t wrong_tick;
    size_t calls;
    long stop_index;
    rx_open(false, "K");
    run_to_item();
    tap(wrong_pick_of('K'));
    wrong_tick = harness_now;
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
    TEST_ASSERT(status("Wrong"));
    harness_run_to((uint16_t)(wrong_tick + TRAINING_WRONG_MS - 1U));
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    harness_set_time((uint16_t)(wrong_tick + TRAINING_WRONG_MS));
    calls = fake_player_call_count();
    if (order == 1U) {
      /* The poll first: the retry starts its playback, then the skip acts in ITEM. */
      harness_pass();
      TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
      TEST_ASSERT_EQUAL(calls + 1U, fake_player_call_count());
    }
    harness_event(LONG_EVENT(R_TAP));
    TEST_ASSERT(is_skip_answer_state(false));
    TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
    stop_index = last_player_call(FAKE_PLAYER_CALL_STOP);
    TEST_ASSERT(stop_index >= (long)calls);
    TEST_ASSERT_EQUAL((size_t)stop_index + 2U, fake_player_call_count());
    TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_START, fake_player_call_at((size_t)stop_index + 1U));
    /* No retry follows: the next passes start nothing. */
    calls = fake_player_call_count();
    harness_run_to((uint16_t)(harness_now + TRAINING_WRONG_MS + 100U));
    TEST_ASSERT_EQUAL(calls, fake_player_call_count());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    TEST_ASSERT(training_is_skipped());
  }
}

/* D-T56 (2), AC-T51 (4): a long next in the start pause, in GAP and after CORRECT does nothing. */
static void test_long_next_ignored_in_gap_and_correct(void) {
  const char* const items[2] = {"K", "M"};
  size_t calls;
  rx_open_items(LEVEL_KM, true, false, items, 2U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  calls = fake_player_call_count();
  mark_quiet_start();
  TEST_ASSERT(!event_mixes_rng(LONG_EVENT(R_TAP)));
  TEST_ASSERT(is_quiet_since_mark());
  run_to_item();
  tap(pick_of('K'));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  calls = fake_player_call_count();
  mark_quiet_start();
  TEST_ASSERT(!event_mixes_rng(LONG_EVENT(R_TAP)));
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  TEST_ASSERT_EQUAL(1U, training_score());
  run_to_phase(TRAINING_PHASE_GAP);
  mark_quiet_start();
  TEST_ASSERT(!event_mixes_rng(LONG_EVENT(R_TAP)));
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  run_to_item();
  TEST_ASSERT(!training_is_skipped());
}

/*
 * F1, UX-77 accepted risk: a real next hold that starts in the Correct result skips the next item when its long event
 * comes in ITEM, and does nothing when the long event comes in the blank moment.
 */
static void test_hold_across_items(void) {
  unsigned variant;
  for (variant = 0U; variant < 2U; ++variant) {
    const char* const items[2] = {"K", "M"};
    const uint16_t start_after_check = (variant == 0U) ? 400U : 50U;
    uint16_t check_tick;
    rx_open_items(LEVEL_KM, false, false, items, 2U);
    run_to_item();
    tap(pick_of('K'));
    check_tick = harness_now;
    harness_run_to((uint16_t)(check_tick + start_after_check));
    pin_press(BUTTON_BIT_NEXT, LONG_HOLD);
    TEST_ASSERT_EQUAL(1U, training_score());
    if (variant == 0U) {
      TEST_ASSERT_EQUAL(2U, training_item_number());
      TEST_ASSERT(is_skip_answer_state(false));
      TEST_ASSERT_EQUAL(0U, training_answer_length());
    } else {
      run_to_item();
      TEST_ASSERT_EQUAL(2U, training_item_number());
      TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
      TEST_ASSERT(!training_is_skipped());
    }
  }
}

/* M9, D-T56 (1): a skip inside an open group drops the group; the blank moment shows the labels of the whole set. */
static void test_skip_in_open_group_labels(void) {
  uint8_t row7_whole_set[LCD_WIDTH];
  rx_open_items(LEVEL_ALL_CHARS, false, false, (const char* const[]){"K", "M"}, 2U);
  run_to_item();
  memcpy(row7_whole_set, lcd_fb_page(7U), LCD_WIDTH);
  tap(L_TAP);
  TEST_ASSERT_EQUAL(1U, keyboard_depth());
  TEST_ASSERT(memcmp(row7_whole_set, lcd_fb_page(7U), LCD_WIDTH) != 0);
  tap(LONG_EVENT(R_TAP));
  TEST_ASSERT(is_skip_answer_state(false));
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  tap(ENTER_EVENT);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(0, memcmp(row7_whole_set, lcd_fb_page(7U), LCD_WIDTH));
  run_to_item();
  TEST_ASSERT_EQUAL(0, memcmp(row7_whole_set, lcd_fb_page(7U), LCD_WIDTH));
}

/* Word item with picks after a wrong attempt: the skip after WRONG shows the answer state; the answer is the item. */
static void test_skip_after_wrong_word(void) {
  rx_open_items(LEVEL_WORDS_13, true, false, (const char* const[]){"KMRS"}, 1U);
  run_to_item();
  tap(pick_of('M'));
  tap(L_TAP);
  tap(L_TAP);
  tap(L_TAP);
  if (training_phase() == TRAINING_PHASE_ITEM) {
    /* The keyboard of this level opens groups; fill the answer with any picks until the check. */
    unsigned guard = 0U;
    while ((training_phase() == TRAINING_PHASE_ITEM) && (guard < 40U)) {
      tap(L_TAP);
      ++guard;
    }
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  TEST_ASSERT_EQUAL(4U, training_answer_length());
  tap(LONG_EVENT(R_TAP));
  TEST_ASSERT(is_skip_answer_state(false));
  TEST_ASSERT_EQUAL(3U, training_answer_length());
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
}

/* F3, F4, D-T58: a skip on the last item; the end screen shows the score without it. A skip, then a long menu/back. */
static void test_skip_last_item_and_leave(void) {
  unsigned guard = 0U;
  uint8_t threshold;
  rx_open_items(LEVEL_WORDS_13, true, false, (const char* const[]){"KMRS"}, 1U);
  threshold = training_pass_threshold();
  TEST_ASSERT_EQUAL(10U, training_item_count());
  while ((training_phase() != TRAINING_PHASE_END) && (guard < 20U)) {
    run_to_item();
    tap(LONG_EVENT(R_TAP));
    TEST_ASSERT(is_skip_answer_state(training_item_number() == training_item_count()));
    tap(ENTER_EVENT);
    ++guard;
  }
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT(end_screen_is(0U, 10U, threshold));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));

  /* F4: a skip, then a long menu/back: the normal leave calls, no write, the main menu. */
  rx_open(true, "K");
  run_to_item();
  tap(LONG_EVENT(R_TAP));
  TEST_ASSERT(is_skip_answer_state(false));
  harness_pins_for(BUTTON_BIT_MENU, LONG_HOLD);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
  TEST_ASSERT(!fake_player_is_running());
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_STOP, fake_player_call_at(fake_player_call_count() - 1U));
  harness_pins_for(0U, 30U);
  TEST_ASSERT(hdr("CW Trainer"));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  TEST_ASSERT_EQUAL(0U, progress_level(PROGRESS_MODE_RECEIVING, 32U));
}

/* The status of the retry after @p wrong wrong attempts with unlimited attempts (UX-76). */
static void expected_try(unsigned wrong, char* text, size_t size) {
  if (wrong < 99U) {
    (void)snprintf(text, size, "Try %u", wrong + 1U);
  } else {
    (void)snprintf(text, size, "Try 99+");
  }
}

/* AC-T53, F5, UX-76: 260 wrong attempts: each retry shows Try N to Try 99, then Try 99+; no FAILED; no point. */
static void test_try_texts_and_cap(void) {
  unsigned wrong;
  rx_open(true, "K");
  run_to_item();
  TEST_ASSERT(status(NULL));
  for (wrong = 1U; wrong <= 260U; ++wrong) {
    char text[16];
    tap(wrong_pick_of('K'));
    if (!TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check())) {
      return;
    }
    TEST_ASSERT(status("Wrong"));
    run_to_item();
    expected_try(wrong, text, sizeof(text));
    if (!TEST_ASSERT(status(text))) {
      fprintf(stderr, "wrong %u: %s\n", wrong, text);
      return;
    }
  }
  TEST_ASSERT_EQUAL(255U, training_wrong_attempts());
  tap(pick_of('K'));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  TEST_ASSERT_EQUAL(0U, training_score());

  /* 3 attempts: Try 2 of 3, Try 3 of 3, then the answer with the Wrong text. */
  rx_open(false, "K");
  run_to_item();
  tap(wrong_pick_of('K'));
  run_to_item();
  TEST_ASSERT(status("Try 2 of 3"));
  tap(wrong_pick_of('K'));
  run_to_item();
  TEST_ASSERT(status("Try 3 of 3"));
  tap(wrong_pick_of('K'));
  TEST_ASSERT(status("Wrong. The answer is:"));
  TEST_ASSERT(!training_is_skipped());
}

/* M10, D-T60, UX-49: RX Attempts acts only in Receiving and only from the next mode start. */
static void test_setting_per_mode_read_once(void) {
  rx_open_items(LEVEL_KM, false, true, (const char* const[]){"K"}, 1U);
  TEST_ASSERT(!training_has_unlimited_attempts());
  (void)settings_set(SETTINGS_ID_RX_UNLIMITED, 1U);
  run_to_item();
  tap(wrong_pick_of('K'));
  run_to_item();
  tap(wrong_pick_of('K'));
  run_to_item();
  tap(wrong_pick_of('K'));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());

  rx_open_items(LEVEL_KM, true, false, (const char* const[]){"K"}, 1U);
  TEST_ASSERT(training_has_unlimited_attempts());
  (void)settings_set(SETTINGS_ID_RX_UNLIMITED, 0U);
  run_to_item();
  tap(wrong_pick_of('K'));
  run_to_item();
  tap(wrong_pick_of('K'));
  run_to_item();
  tap(wrong_pick_of('K'));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  run_to_item();
  TEST_ASSERT(status("Try 4"));
}

int main(void) {
  TEST_RUN(test_skip_in_item_answer_state);
  TEST_RUN(test_skip_at_end_of_wrong_delay);
  TEST_RUN(test_long_next_ignored_in_gap_and_correct);
  TEST_RUN(test_hold_across_items);
  TEST_RUN(test_skip_in_open_group_labels);
  TEST_RUN(test_skip_after_wrong_word);
  TEST_RUN(test_skip_last_item_and_leave);
  TEST_RUN(test_try_texts_and_cap);
  TEST_RUN(test_setting_per_mode_read_once);
  return TEST_FINISH();
}
