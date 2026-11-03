/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA scenarios of unlimited attempts and the long next skip in Sending (references/internal/training_spec.md D-T53,
 * D-T54, D-T57 to D-T60, section 12.3, AC-T52, AC-T53, Q8 fault cases F1 to F4, F7 and mutants M5, M7, M8, M10;
 * ui-ux.md UX-76 to UX-78, sections 5.20.1, 5.20.2 and 5.20.4). The real menu, buttons, engine and progress run with a
 * fake keyer and fake items. The oracles come from the documents: the status `Skipped` in column 9, row 5 with the
 * code of the character at the failed position, the bar that does not move, row 7 that does not change, a
 * half-keyed character dropped up to its CHAR_GAP, and the first character of the next item that counts.
 */

#include "cw/keyer_shared.h"
#include "cw/morse.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define LEVEL_KM 0U
#define LEVEL_WORDS_13 17U /* KMRSUAPTLOWI., words of 4, 10 items */
#define FB_SIZE ((size_t)LCD_PAGE_COUNT * LCD_WIDTH)
#define LONG_HOLD ((uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 100U))
#define NO_BAR (-1)

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

static void tx_open_items(uint16_t level, bool is_tx_unlimited, bool is_rx_unlimited, bool is_code_shown,
                          const char* const* items, size_t count) {
  size_t index;

  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_SENDING, 20U, level);
  }
  harness_power_on();
  (void)settings_set(SETTINGS_ID_TX_UNLIMITED, is_tx_unlimited ? 1U : 0U);
  (void)settings_set(SETTINGS_ID_RX_UNLIMITED, is_rx_unlimited ? 1U : 0U);
  (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, is_code_shown ? 1U : 0U);
  fake_koch_item_reset();
  for (index = 0U; index < count; ++index) {
    fake_koch_item_push(items[index]);
  }
  harness_open_mode(1U);
}

static void tx_open_word(const char* word, bool is_tx_unlimited, bool is_code_shown) {
  const char* const items[1] = {word};
  tx_open_items((strlen(word) > 1U) ? LEVEL_WORDS_13 : LEVEL_KM, is_tx_unlimited, false, is_code_shown, items, 1U);
}

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

/* The dot-dash text of @p character, from the morse table. */
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

static void elements(const char* pattern) {
  size_t index;
  for (index = 0U; pattern[index] != '\0'; ++index) {
    push((pattern[index] == '-') ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
}

static void key_char(char character) {
  elements(code_of(character));
  push(KEYER_EVENT_CHAR_GAP);
}

static void key(const char* text) {
  size_t index;
  for (index = 0U; text[index] != '\0'; ++index) {
    key_char(text[index]);
  }
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

static void skip_now(void) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(LONG_EVENT(R_TAP));
}

/* Item line of ui-ux.md 5.13.1: slot s of a word of n characters at x = 68 - 8n + 16s; the bar under @p bar_slot. */
static bool item_line_with_bar(const char* word, int bar_slot) {
  static char glyphs[KOCH_WORD_LEN_MAX][2];
  const int count = (int)strlen(word);
  int slot;
  item_expect_reset();
  for (slot = 0; slot < count; ++slot) {
    glyphs[slot][0] = word[slot];
    glyphs[slot][1] = '\0';
    item_expect(ITEM_PART_BIG, 68 - 8 * count + 16 * slot, glyphs[slot]);
  }
  if (bar_slot >= 0) {
    item_expect(ITEM_PART_BAR, 68 - 8 * count + 16 * bar_slot, NULL);
  }
  return item_line_is();
}

/* UX-78: the skip result for @p word with the reference character at @p slot; row 7 equals @p row7. */
static bool is_skip_result(const char* word, uint8_t slot, const uint8_t* row7) {
  const bool is_word = strlen(word) > 1U;
  return (training_phase() == TRAINING_PHASE_RESULT) && (training_last_check() == TRAINING_CHECK_FAILED) &&
         training_is_skipped() && (training_answer_length() == slot) && status("Skipped") &&
         code_is(code_of(word[slot])) && item_line_with_bar(word, is_word ? (int)slot : NO_BAR) &&
         (memcmp(row7, lcd_fb_page(7U), LCD_WIDTH) == 0);
}

/* The skip RESULT lasts TRAINING_FAILED_MS from @p skip_tick, then the blank moment. */
static void expect_failed_ms_from(uint16_t skip_tick) {
  harness_run_to((uint16_t)(skip_tick + TRAINING_FAILED_MS - 1U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  harness_run_to((uint16_t)(skip_tick + TRAINING_FAILED_MS));
  TEST_ASSERT(training_phase() != TRAINING_PHASE_RESULT);
}

/* AC-T52 (1), (5), N3, N4, M5: the four skip cases of UX-78 for each setting and each Show CW Code value. */
static void test_skip_screen_cases(void) {
  unsigned variant;
  for (variant = 0U; variant < 4U; ++variant) {
    const bool is_unlimited = (variant & 1U) != 0U;
    const bool is_code_shown = (variant & 2U) != 0U;
    uint8_t row7[LCD_WIDTH];
    uint16_t skip_tick;

    /* Skip in Item, nothing keyed: an empty answer, the reference is the first character. */
    tx_open_word("KMRS", is_unlimited, is_code_shown);
    run_to_item();
    memcpy(row7, lcd_fb_page(7U), LCD_WIDTH);
    skip_now();
    skip_tick = harness_now;
    TEST_ASSERT(is_skip_result("KMRS", 0U, row7));
    expect_failed_ms_from(skip_tick);

    /* Skip in Item after one accepted character. */
    tx_open_word("KMRS", is_unlimited, is_code_shown);
    run_to_item();
    key("K");
    memcpy(row7, lcd_fb_page(7U), LCD_WIDTH);
    skip_now();
    TEST_ASSERT(is_skip_result("KMRS", 1U, row7));

    /* Skip in a Wrong: <character> result: the bar stays under the wrong position. */
    tx_open_word("KMRS", is_unlimited, is_code_shown);
    run_to_item();
    key("KE");
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
    TEST_ASSERT(status("Wrong: E"));
    TEST_ASSERT(item_line_with_bar("KMRS", 1));
    memcpy(row7, lcd_fb_page(7U), LCD_WIDTH);
    skip_now();
    TEST_ASSERT(is_skip_result("KMRS", 1U, row7));
    TEST_ASSERT_EQUAL(1U, training_wrong_attempts());

    /* Skip in a Wrong: pause too long result: the bar stays under the next character. */
    tx_open_word("KMRS", is_unlimited, is_code_shown);
    run_to_item();
    key("KM");
    push(KEYER_EVENT_WORD_GAP);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
    TEST_ASSERT(status("Wrong: pause too long"));
    TEST_ASSERT(item_line_with_bar("KMRS", 2));
    memcpy(row7, lcd_fb_page(7U), LCD_WIDTH);
    skip_now();
    TEST_ASSERT(is_skip_result("KMRS", 2U, row7));

    /* Character item: no bar, the code of the item character. */
    tx_open_word("K", is_unlimited, is_code_shown);
    run_to_item();
    memcpy(row7, lcd_fb_page(7U), LCD_WIDTH);
    skip_now();
    TEST_ASSERT(is_skip_result("K", 0U, row7));
    tx_open_word("K", is_unlimited, is_code_shown);
    run_to_item();
    key("E");
    memcpy(row7, lcd_fb_page(7U), LCD_WIDTH);
    skip_now();
    TEST_ASSERT(is_skip_result("K", 0U, row7));
  }
}

/*
 * F2, F7, M7, M8, AC-T52 (2), (3): a half-keyed character at the skip, with its CHAR_GAP in a later pass or in the
 * same pass; a CHAR_GAP before the skip; an empty pattern and no keying. The first character of the next item counts
 * in each case, and no attempt is added.
 */
static void test_half_keyed_character(void) {
  unsigned variant;
  for (variant = 0U; variant < 4U; ++variant) {
    const char* const items[2] = {"KMRS", "MRSK"};
    tx_open_items(LEVEL_WORDS_13, false, false, true, items, 2U);
    run_to_item();
    if (variant == 0U) {
      /* The rest of the character after the skip, in later passes. */
      elements("-");
      skip_now();
      TEST_ASSERT(training_is_skipped());
      elements(".-");
      push(KEYER_EVENT_CHAR_GAP);
    } else if (variant == 1U) {
      /* The rest and the CHAR_GAP in the queue of the same pass as the skip. */
      elements("-");
      fake_keyer_push_event(KEYER_EVENT_DIT);
      fake_keyer_push_event(KEYER_EVENT_DAH);
      fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
      skip_now();
      TEST_ASSERT_EQUAL(0U, fake_keyer_pending_count());
      TEST_ASSERT(training_is_skipped());
    } else if (variant == 2U) {
      /* The CHAR_GAP comes before the skip: K is accepted, the pattern is empty at the skip. */
      key("K");
      TEST_ASSERT_EQUAL(1U, training_answer_length());
      skip_now();
      TEST_ASSERT_EQUAL(1U, training_answer_length());
    } else {
      /* An empty pattern and no keying until the next item. */
      skip_now();
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
    TEST_ASSERT(training_is_skipped());
    TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    TEST_ASSERT(status("Skipped"));
    run_to_item();
    TEST_ASSERT_EQUAL(2U, training_item_number());
    if (variant == 2U) {
      /* D-T57 (2): the skip cleared the armed flag of the accepted K, so a late WORD_GAP is no word space. */
      push(KEYER_EVENT_WORD_GAP);
      TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
      TEST_ASSERT_EQUAL(0U, training_answer_length());
    }
    key("M");
    if (!TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase())) {
      fprintf(stderr, "variant %u: the first character of the next item did not count\n", variant);
    }
    TEST_ASSERT_EQUAL(1U, training_answer_length());
    TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
    key("RSK");
    TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
    TEST_ASSERT_EQUAL(1U, training_score());
  }
}

/* AC-T52 (4), UX-77: the press and the repeats of a real next hold do nothing; the long event at 800 ms skips. */
static void test_real_hold_press_and_repeats(void) {
  tx_open_word("KMRS", true, true);
  run_to_item();
  key("K");
  mark_quiet_start();
  harness_pins_for(BUTTON_BIT_NEXT, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS - 20U));
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  harness_pins_for(BUTTON_BIT_NEXT, 60U);
  TEST_ASSERT(training_is_skipped());
  TEST_ASSERT(status("Skipped"));
  mark_quiet_start();
  harness_pins_for(BUTTON_BIT_NEXT, 500U);
  harness_pins_for(0U, (uint16_t)(BUTTONS_DEBOUNCE_MS + 10U));
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(1U, training_answer_length());
}

/* D-T57 (3), M6: a long next in the start pause, in GAP, after CORRECT, after FAILED and after a skip does nothing. */
static void test_long_next_ignored_states(void) {
  const char* const items[3] = {"K", "M", "K"};
  tx_open_items(LEVEL_KM, false, false, true, items, 3U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  mark_quiet_start();
  skip_now();
  TEST_ASSERT(is_quiet_since_mark());
  run_to_item();
  key("K");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  mark_quiet_start();
  skip_now();
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(1U, training_score());
  run_to_phase(TRAINING_PHASE_GAP);
  mark_quiet_start();
  skip_now();
  TEST_ASSERT(is_quiet_since_mark());
  run_to_item();
  key("E");
  run_to_item();
  key("E");
  run_to_item();
  key("E");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(!training_is_skipped());
  TEST_ASSERT(status("Wrong: E"));
  mark_quiet_start();
  skip_now();
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT(!training_is_skipped());
  run_to_item();
  skip_now();
  TEST_ASSERT(training_is_skipped());
  mark_quiet_start();
  skip_now();
  TEST_ASSERT(is_quiet_since_mark());
}

/* F3, F4: a skip on the last item; the end screen shows the score without it. A skip, then a long menu/back. */
static void test_skip_last_item_and_leave(void) {
  unsigned guard = 0U;
  uint8_t threshold;
  tx_open_word("KMRS", true, true);
  threshold = training_pass_threshold();
  TEST_ASSERT_EQUAL(10U, training_item_count());
  while ((training_phase() != TRAINING_PHASE_END) && (guard < 20U)) {
    run_to_item();
    if (training_item_number() < training_item_count()) {
      key("KMRS");
      TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
    } else {
      key("K");
      skip_now();
      TEST_ASSERT(training_is_skipped());
    }
    run_to_phase((training_item_number() < training_item_count()) ? TRAINING_PHASE_GAP : TRAINING_PHASE_END);
    ++guard;
  }
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(9U, training_score());
  TEST_ASSERT(end_screen_is(9U, 10U, threshold));
  TEST_ASSERT(!fake_keyer_is_running());

  /* F4 */
  tx_open_word("KMRS", false, true);
  run_to_item();
  elements("-");
  skip_now();
  TEST_ASSERT(training_is_skipped());
  harness_pins_for(BUTTON_BIT_MENU, LONG_HOLD);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
  TEST_ASSERT(!fake_keyer_is_running());
  TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
  harness_pins_for(0U, 30U);
  TEST_ASSERT(hdr("CW Trainer"));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
}

/* F1: a hold that starts in the Correct result skips the next item when its long event comes in ITEM. */
static void test_hold_across_items(void) {
  unsigned variant;
  for (variant = 0U; variant < 2U; ++variant) {
    const char* const items[2] = {"K", "M"};
    const uint16_t start_after_check = (variant == 0U) ? 200U : 20U;
    uint16_t check_tick;
    tx_open_items(LEVEL_KM, false, false, true, items, 2U);
    run_to_item();
    key("K");
    check_tick = harness_now;
    harness_run_to((uint16_t)(check_tick + start_after_check));
    harness_pins_for(BUTTON_BIT_NEXT, LONG_HOLD);
    harness_pins_for(0U, (uint16_t)(BUTTONS_DEBOUNCE_MS + 10U));
    TEST_ASSERT_EQUAL(1U, training_score());
    if (variant == 0U) {
      TEST_ASSERT_EQUAL(2U, training_item_number());
      TEST_ASSERT(training_is_skipped());
      TEST_ASSERT(status("Skipped"));
    } else {
      run_to_item();
      TEST_ASSERT_EQUAL(2U, training_item_number());
      TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
      TEST_ASSERT(!training_is_skipped());
    }
  }
}

/* M10, AC-T53, UX-76: TX Attempts acts only in Sending; the Try texts of both settings. */
static void test_setting_per_mode_and_try_text(void) {
  unsigned wrong;
  tx_open_items(LEVEL_KM, false, true, true, (const char* const[]){"K"}, 1U);
  TEST_ASSERT(!training_has_unlimited_attempts());
  run_to_item();
  key("E");
  run_to_item();
  TEST_ASSERT(status("Try 2 of 3"));
  key("E");
  run_to_item();
  TEST_ASSERT(status("Try 3 of 3"));
  key("E");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());

  tx_open_items(LEVEL_KM, true, false, true, (const char* const[]){"K"}, 1U);
  TEST_ASSERT(training_has_unlimited_attempts());
  (void)settings_set(SETTINGS_ID_TX_UNLIMITED, 0U);
  run_to_item();
  for (wrong = 1U; wrong <= 120U; ++wrong) {
    char text[16];
    key("E");
    if (!TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check())) {
      return;
    }
    run_to_item();
    if (wrong < 99U) {
      (void)snprintf(text, sizeof(text), "Try %u", wrong + 1U);
    } else {
      (void)snprintf(text, sizeof(text), "Try 99+");
    }
    if (!TEST_ASSERT(status(text))) {
      fprintf(stderr, "wrong %u\n", wrong);
      return;
    }
  }
  key("K");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  TEST_ASSERT_EQUAL(0U, training_score());
}

int main(void) {
  TEST_RUN(test_skip_screen_cases);
  TEST_RUN(test_half_keyed_character);
  TEST_RUN(test_real_hold_press_and_repeats);
  TEST_RUN(test_long_next_ignored_states);
  TEST_RUN(test_skip_last_item_and_leave);
  TEST_RUN(test_hold_across_items);
  TEST_RUN(test_setting_per_mode_and_try_text);
  return TEST_FINISH();
}
