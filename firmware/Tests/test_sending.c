/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Sending mode scenarios (references/internal/training_spec.md section 12.3, AC-T20; ui-ux.md section 9.4, V-110 to
 * V-119 and V-121). The real menu, buttons, engine and progress run with a fake keyer and fake items.
 */

#include "cw/morse.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define LEVEL_KM 0U
#define LEVEL_WORDS_13 17U
#define LEVEL_ALL_CHARS 61U
#define LEVEL_ALL_WORDS 62U
#define LEVEL_100_WORDS 153U
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

/* TX(set, kind, items). */
static void tx_open(uint16_t level, uint8_t prosign_style, bool is_code_shown, const char* const* items, size_t count) {
  size_t index;

  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_SENDING, 20U, level);
  }
  harness_power_on();
  (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, prosign_style);
  (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, is_code_shown ? 1U : 0U);
  fake_koch_item_reset();
  for (index = 0U; index < count; ++index) {
    fake_koch_item_push(items[index]);
  }
  harness_open_mode(1U);
}

static void tx_open_one(uint16_t level, const char* item) {
  const char* const items[1] = {item};
  tx_open(level, SETTINGS_PROSIGN_BOTH, true, items, 1U);
}

/* Runs passes from the next ms until the fake keyer FIFO is empty. */
static void drain(void) {
  unsigned guard = 0U;
  do {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  } while ((fake_keyer_pending_count() != 0U) && (guard < 100U));
}

static void element(bool is_dah) {
  fake_keyer_push_event(is_dah ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  drain();
}

static void char_gap(void) {
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  drain();
}

/* KEYP(p): the elements of @p pattern, then one CHAR_GAP. */
static void keyp(const char* pattern) {
  size_t index;
  for (index = 0U; pattern[index] != '\0'; ++index) {
    element(pattern[index] == '-');
  }
  char_gap();
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

/* KEY(s) */
static void key(const char* text) {
  size_t index;
  for (index = 0U; text[index] != '\0'; ++index) {
    keyp(code_of(text[index]));
  }
}

static void run_to_item(void) {
  unsigned guard = 0U;
  while ((training_phase() != TRAINING_PHASE_ITEM) && (guard < 20000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
}

static void expect_big(int x, const char* text, int bar_x) {
  item_expect_reset();
  item_expect(ITEM_PART_BIG, x, text);
  if (bar_x >= 0) {
    item_expect(ITEM_PART_BAR, bar_x, NULL);
  }
  TEST_ASSERT(item_line_is());
}

static void expect_word(const char* word, int bar_slot) {
  static char glyphs[8][2];
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
  TEST_ASSERT(item_line_is());
}

/* V-110 */
static void test_v110_start(void) {
  tx_open_one(LEVEL_KM, "K");
  harness_pins_for(BUTTON_BIT_ENTER, 999U);
  TEST_ASSERT(hdr("TX 20/20 L1          1/50"));
  TEST_ASSERT(body_off(1, 7));
  harness_pins_for(BUTTON_BIT_ENTER, 1U);
  expect_big(60, "K", -1);
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(body_off(5, 7));
  TEST_ASSERT(margins_off());
  harness_pins_for(BUTTON_BIT_ENTER, 1000U);
  harness_pins_for(0U, 100U);
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(KEYER_KEY_IAMBIC_B, fake_keyer_last_key_type());
  TEST_ASSERT(!fake_keyer_last_is_swapped());
  TEST_ASSERT_EQUAL(20U, fake_keyer_last_wpm());
  TEST_ASSERT_EQUAL(20U, fake_keyer_last_eff_wpm());
  TEST_ASSERT_EQUAL(1U, training_item_number());
  TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
  TEST_ASSERT_EQUAL(0U, fake_player_start_count());
  /* The mode makes one sidetone call: the tone at the start. */
  TEST_ASSERT_EQUAL(1U, fake_sidetone_call_count());
}

/* V-111 */
static void test_v111_correct(void) {
  const char* const items[2] = {"K", "K"};
  uint16_t t;

  tx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, true, items, 2U);
  run_to_item();
  key("K");
  t = harness_now;
  TEST_ASSERT(status("Correct"));
  expect_big(60, "K", -1);
  harness_run_to((uint16_t)(t + 600U));
  TEST_ASSERT(body_off(1, 7));
  TEST_ASSERT(hdr("TX 20/20 L1          2/50"));
  harness_run_to((uint16_t)(t + 899U));
  TEST_ASSERT(body_off(1, 7));
  harness_run_to((uint16_t)(t + 900U));
  expect_big(60, "K", -1);
  TEST_ASSERT_EQUAL(1U, training_score());
}

/* V-112, V-114 (row 7) */
static void check_wrong_then_correct(bool is_code_shown) {
  uint16_t t;

  tx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, is_code_shown, (const char* const[]){"K"}, 1U);
  run_to_item();
  element(true);
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-") : blank_row(7));
  element(false);
  char_gap();
  t = harness_now;
  TEST_ASSERT(status("Wrong: N"));
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-.") : blank_row(7));
  expect_big(60, "K", -1);
  TEST_ASSERT(blank_row(5));
  harness_run_to((uint16_t)(t + 1199U));
  TEST_ASSERT(status("Wrong: N"));
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-.") : blank_row(7));
  harness_run_to((uint16_t)(t + 1200U));
  TEST_ASSERT(status("Try 2 of 3"));
  /* UX-52: the retry screen has rows 5 and 7 off. */
  TEST_ASSERT(body_off(5, 7));
  expect_big(60, "K", -1);
  harness_run_to((uint16_t)(t + 1299U));
  element(true);
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-") : blank_row(7));
  element(false);
  element(true);
  char_gap();
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-.-") : blank_row(7));
  TEST_ASSERT_EQUAL(0U, training_score());
  TEST_ASSERT(margins_off());
}

static void test_v112_v114_code_row(void) {
  check_wrong_then_correct(true);
  check_wrong_then_correct(false);
}

/* V-113, V-114 (row 5) */
static void check_failed(bool is_code_shown) {
  const char* const items[2] = {"K", "M"};
  uint16_t t;

  size_t attempt;

  tx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, is_code_shown, items, 2U);
  run_to_item();
  keyp("-.");
  for (attempt = 0U; attempt < 2U; ++attempt) {
    /* UX-52: each retry turns row 7 off; the new attempt draws its marks from x 2. */
    run_to_item();
    TEST_ASSERT(body_off(5, 7));
    keyp("-.");
    TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-.") : blank_row(7));
  }
  t = harness_now;
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(status("Wrong: N"));
  TEST_ASSERT(marks_are(5, 54, "-.-"));
  expect_big(60, "K", -1);
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-.") : blank_row(7));
  harness_run_to((uint16_t)(t + 2499U));
  TEST_ASSERT(marks_are(5, 54, "-.-"));
  harness_run_to((uint16_t)(t + 2500U));
  TEST_ASSERT(body_off(1, 7));
  TEST_ASSERT(hdr("TX 20/20 L1          2/50"));
  harness_run_to((uint16_t)(t + 2799U));
  TEST_ASSERT(body_off(1, 7));
  harness_run_to((uint16_t)(t + 2800U));
  expect_big(60, "M", -1);
}

static void test_v113_v114_answer_code(void) {
  check_failed(true);
  check_failed(false);
}

static void word_gap(void) {
  fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
  drain();
}

static void clear_dirty_pages(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
}

/* Takes the screen and the attempt count before a step that must change nothing. */
static uint8_t expect_no_effect_before(void) {
  snapshot(snapshot_before);
  clear_dirty_pages();
  return training_wrong_attempts();
}

/* No status change, no attempt and no redraw since expect_no_effect_before(). */
static bool is_no_effect(uint8_t attempts_before) {
  return is_screen_unchanged() && (lcd_fb_dirty_mask() == 0U) && (training_wrong_attempts() == attempts_before);
}

#define TX_WRONG_CHAR 0U
#define TX_WRONG_UNKNOWN 1U
#define TX_WRONG_WORD_SPACE 2U

/* One wrong attempt of @p kind; a word item first keys its first character correctly. */
static void tx_wrong_attempt(uint8_t kind, bool is_word) {
  if (is_word) {
    key("K");
  }
  if (kind == TX_WRONG_CHAR) {
    keyp("-.");
  } else if (kind == TX_WRONG_UNKNOWN) {
    keyp("........");
  } else {
    word_gap();
  }
}

/* AC-T28 (1): the retry screen after each kind of wrong attempt, for a character item and a word item. */
static void check_retry_screen(uint8_t kind, bool is_word, bool is_code_shown) {
  static const char* const try_texts[2] = {"Try 2 of 3", "Try 3 of 3"};
  const char* const items[1] = {is_word ? "KMRS" : "K"};
  uint8_t attempt;
  uint16_t t;

  tx_open(is_word ? LEVEL_WORDS_13 : LEVEL_KM, SETTINGS_PROSIGN_BOTH, is_code_shown, items, 1U);
  run_to_item();
  for (attempt = 0U; attempt < 2U; ++attempt) {
    tx_wrong_attempt(kind, is_word);
    t = harness_now;
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
    TEST_ASSERT_EQUAL(attempt + 1U, training_wrong_attempts());
    /* During RESULT, row 7 keeps the marks of the keyed character. D2 decides row 7 for a word space. */
    if (kind == TX_WRONG_CHAR) {
      TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-.") : blank_row(7));
    } else if (kind == TX_WRONG_UNKNOWN) {
      TEST_ASSERT(is_code_shown ? marks_are(7, 2, "........") : blank_row(7));
    }
    harness_run_to((uint16_t)(t + TRAINING_WRONG_MS - 1U));
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    harness_run_to((uint16_t)(t + TRAINING_WRONG_MS));
    TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
    TEST_ASSERT(status(try_texts[attempt]));
    TEST_ASSERT(body_off(5, 7));
    if (is_word) {
      expect_word("KMRS", 0);
    } else {
      expect_big(60, "K", -1);
    }
    TEST_ASSERT(margins_off());
  }
  /* The first DIT after the retry draws its mark at x 2. */
  element(false);
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, ".") : blank_row(7));
  TEST_ASSERT_EQUAL(0U, training_answer_length());
}

static void test_ac_t28_retry_screen(void) {
  uint8_t kind;
  size_t shown;

  for (shown = 0U; shown < 2U; ++shown) {
    for (kind = TX_WRONG_CHAR; kind <= TX_WRONG_WORD_SPACE; ++kind) {
      check_retry_screen(kind, true, shown != 0U);
      if (kind != TX_WRONG_WORD_SPACE) {
        check_retry_screen(kind, false, shown != 0U);
      }
    }
  }
}

/* AC-T28 (3): a WORD_GAP in each of these states changes nothing. */
static void test_ac_t28_word_gap_without_effect(void) {
  const char* const two_words[2] = {"KMRS", "KMRS"};
  const char* const two_chars[2] = {"K", "K"};
  uint8_t attempts;

  /* In the start pause, before the first character, and before the first character after a retry. */
  tx_open(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, true, two_words, 2U);
  harness_run_to(HARNESS_T0 + 500U);
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  run_to_item();
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  key("K");
  keyp("-.");
  /* In RESULT after a wrong character, armed or not. */
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  run_to_item();
  TEST_ASSERT(status("Try 2 of 3"));
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  /* After the last character: the item is complete and the phase is RESULT. */
  key("KMRS");
  TEST_ASSERT(status("Correct"));
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  /* In the blank moment (GAP). */
  harness_run_to((uint16_t)(harness_now + TRAINING_CORRECT_MS + 10U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));

  /* After OVERFLOW with the drop flag set, and after the CHAR_GAP that clears it (D-T16). */
  run_to_item();
  key("K");
  attempts = expect_no_effect_before();
  fake_keyer_push_event(KEYER_EVENT_OVERFLOW);
  drain();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  char_gap();
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  TEST_ASSERT(status(NULL));
  key("M");
  TEST_ASSERT_EQUAL(2U, training_answer_length());

  /* After a DIT that follows the PARTIAL CHAR_GAP. */
  key("R");
  element(false);
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());

  /* After a character dropped by D-T15: a DIT in RESULT, then the rest of that character after the retry. */
  tx_open(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, true, two_words, 1U);
  run_to_item();
  key("K");
  keyp("-.");
  element(true);
  run_to_item();
  element(false);
  char_gap();
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  key("K");
  TEST_ASSERT_EQUAL(1U, training_answer_length());

  /* A character session: never armed in ITEM. */
  tx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, true, two_chars, 2U);
  run_to_item();
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  key("K");
  TEST_ASSERT(status("Correct"));
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  TEST_ASSERT_EQUAL(1U, training_score());
}

/* AC-T28 (4): a PARTIAL CHAR_GAP and a WORD_GAP read in one poll give the wrong attempt. */
static void test_ac_t28_char_and_word_gap_in_one_poll(void) {
  tx_open_one(LEVEL_WORDS_13, "KMRS");
  run_to_item();
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_DIT);
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_pass();
  TEST_ASSERT_EQUAL(0U, fake_keyer_pending_count());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(0, strcmp("K ", training_answer_text()));
  /* The bar stays under the character expected next. */
  expect_word("KMRS", 1);
}

/* HDR text of ui-ux.md section 5.11.1 at TX 20/20 for level index LEVEL_WORDS_13. */
static const char* tx_header(unsigned item, unsigned count) {
  static char text[32];
  char right[16];
  (void)snprintf(right, sizeof(right), "%u/%u", item, count);
  (void)snprintf(text, sizeof(text), "%-*s%s", 25 - (int)strlen(right), "TX 20/20 L18", right);
  return text;
}

/* V-128 and AC-T28 (2): a pause after character k of KMRS (k = 2, then k = length - 1 = 3). */
static void check_pause_error(const char* keyed, int bar_slot, const char* row7) {
  uint8_t header[LCD_WIDTH];
  uint16_t t;

  tx_open_one(LEVEL_WORDS_13, "KMRS");
  run_to_item();
  key(keyed);
  expect_word("KMRS", bar_slot);
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(marks_are(7, 2, row7));
  memcpy(header, lcd_fb_page(0U), LCD_WIDTH);
  word_gap();
  t = harness_now;
  TEST_ASSERT(status("Wrong: pause too long"));
  TEST_ASSERT(text_at(1, 2, "Wrong: pause too long"));
  expect_word("KMRS", bar_slot);
  TEST_ASSERT(body_off(5, 5));
  TEST_ASSERT(marks_are(7, 2, row7));
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  snapshot(snapshot_before);
  harness_run_to((uint16_t)(t + 1199U));
  TEST_ASSERT(is_screen_unchanged());
  harness_run_to((uint16_t)(t + 1200U));
  TEST_ASSERT(status("Try 2 of 3"));
  expect_word("KMRS", 0);
  TEST_ASSERT(body_off(5, 7));
  TEST_ASSERT_EQUAL(0, memcmp(header, lcd_fb_page(0U), LCD_WIDTH));
  key("KMRS");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(0U, training_score());
  TEST_ASSERT(margins_off());
}

static void test_v128_pause_error(void) {
  check_pause_error("KM", 2, "--");
  check_pause_error("KMR", 3, ".-.");
}

/* V-129: a WORD_GAP before the first character of the item, also after a retry, does nothing. */
static void test_v129_word_gap_before_first_char(void) {
  uint8_t attempts;
  uint16_t t;

  tx_open_one(LEVEL_WORDS_13, "KMRS");
  run_to_item();
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  TEST_ASSERT(status(NULL));
  expect_word("KMRS", 0);
  key("KMRS");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_score());

  tx_open_one(LEVEL_WORDS_13, "KMRS");
  run_to_item();
  key("K");
  key("N");
  t = harness_now;
  harness_run_to((uint16_t)(t + 1200U));
  TEST_ASSERT(status("Try 2 of 3"));
  expect_word("KMRS", 0);
  harness_run_to((uint16_t)(t + 1299U));
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT_EQUAL((uint16_t)(t + 1300U), harness_now);
  TEST_ASSERT(is_no_effect(attempts));
  key("KMRS");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(0U, training_score());
}

/* V-130: a WORD_GAP after the last character falls in the result delay and does nothing. */
static void test_v130_word_gap_after_last_char(void) {
  const char* const items[2] = {"KMRS", "KMRS"};
  uint8_t attempts;
  uint16_t t;

  tx_open(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, true, items, 2U);
  run_to_item();
  key("KMRS");
  t = harness_now;
  TEST_ASSERT(status("Correct"));
  expect_word("KMRS", -1);
  harness_run_to((uint16_t)(t + 179U));
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  harness_run_to((uint16_t)(t + 600U));
  TEST_ASSERT(body_off(1, 7));
  TEST_ASSERT(hdr(tx_header(2U, training_item_count())));
  harness_run_to((uint16_t)(t + 900U));
  expect_word("KMRS", 0);
  TEST_ASSERT(status(NULL));
  TEST_ASSERT_EQUAL(1U, training_score());
}

/* V-131: three pause errors give the failed path with the code of the character under the bar in row 5. */
static void check_three_pause_errors(bool is_code_shown) {
  const char* const items[2] = {"KMRS", "UAPT"};
  uint8_t attempts;
  uint16_t t;

  tx_open(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, is_code_shown, items, 2U);
  run_to_item();
  key("K");
  word_gap();
  TEST_ASSERT(status("Wrong: pause too long"));
  expect_word("KMRS", 1);
  TEST_ASSERT(body_off(5, 5));
  run_to_item();
  TEST_ASSERT(status("Try 2 of 3"));
  expect_word("KMRS", 0);
  TEST_ASSERT(body_off(7, 7));
  key("K");
  word_gap();
  TEST_ASSERT(status("Wrong: pause too long"));
  expect_word("KMRS", 1);
  TEST_ASSERT(body_off(5, 5));
  run_to_item();
  TEST_ASSERT(status("Try 3 of 3"));
  expect_word("KMRS", 0);
  TEST_ASSERT(body_off(7, 7));
  key("KM");
  word_gap();
  t = harness_now;
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(status("Wrong: pause too long"));
  expect_word("KMRS", 2);
  TEST_ASSERT(marks_are(5, 56, ".-."));
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "--") : blank_row(7));
  harness_run_to((uint16_t)(t + 999U));
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  harness_run_to((uint16_t)(t + 2499U));
  TEST_ASSERT(is_screen_unchanged());
  harness_run_to((uint16_t)(t + 2500U));
  TEST_ASSERT(body_off(1, 7));
  TEST_ASSERT(hdr(tx_header(2U, training_item_count())));
  harness_run_to((uint16_t)(t + 2800U));
  expect_word("UAPT", 0);
  TEST_ASSERT_EQUAL(0U, training_score());
  TEST_ASSERT(margins_off());
}

static void test_v131_three_pause_errors(void) {
  check_three_pause_errors(true);
  check_three_pause_errors(false);
}

/* V-132: WORD_GAP events in the start pause, the wrong delay and the blank moment do nothing. */
static void test_v132_word_gap_in_delays(void) {
  const char* const items[2] = {"KMRS", "KMRS"};
  uint8_t attempts;
  uint16_t t;
  uint16_t u;

  tx_open(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, true, items, 2U);
  harness_run_to(HARNESS_T0 + 499U);
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  harness_run_to(HARNESS_T0 + 1000U);
  expect_word("KMRS", 0);
  TEST_ASSERT(status(NULL));
  key("K");
  key("N");
  t = harness_now;
  TEST_ASSERT(status("Wrong: N"));
  harness_run_to((uint16_t)(t + 179U));
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  TEST_ASSERT(status("Wrong: N"));
  harness_run_to((uint16_t)(t + 1200U));
  TEST_ASSERT(status("Try 2 of 3"));
  key("KMRS");
  u = harness_now;
  TEST_ASSERT(status("Correct"));
  harness_run_to((uint16_t)(u + 699U));
  attempts = expect_no_effect_before();
  word_gap();
  TEST_ASSERT(is_no_effect(attempts));
  TEST_ASSERT(body_off(1, 7));
  harness_run_to((uint16_t)(u + 900U));
  expect_word("KMRS", 0);
  TEST_ASSERT(status(NULL));
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
}

/* V-133 part A: characters keyed with no WORD_GAP between them move the bar and use no attempt. */
static void test_v133_no_word_gap(void) {
  tx_open_one(LEVEL_WORDS_13, "KMRS");
  run_to_item();
  key("K");
  expect_word("KMRS", 1);
  TEST_ASSERT(status(NULL));
  key("M");
  expect_word("KMRS", 2);
  TEST_ASSERT(status(NULL));
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
}

/* V-134: row 7 keeps the wrong code through the delay, also after a dropped DIT, and goes off at the retry. */
static void check_dropped_after_wrong(bool is_code_shown) {
  uint16_t t;

  tx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, is_code_shown, (const char* const[]){"K"}, 1U);
  run_to_item();
  keyp("--");
  t = harness_now;
  TEST_ASSERT(status("Wrong: M"));
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "--") : blank_row(7));
  harness_run_to((uint16_t)(t + 1099U));
  element(false);
  harness_run_to((uint16_t)(t + 1199U));
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "--") : blank_row(7));
  harness_run_to((uint16_t)(t + 1200U));
  TEST_ASSERT(status("Try 2 of 3"));
  expect_big(60, "K", -1);
  TEST_ASSERT(body_off(5, 7));
  harness_run_to((uint16_t)(t + 1299U));
  element(true);
  TEST_ASSERT(body_off(5, 7));
  TEST_ASSERT(status("Try 2 of 3"));
  harness_run_to((uint16_t)(t + 1499U));
  char_gap();
  TEST_ASSERT(body_off(5, 7));
  TEST_ASSERT(status("Try 2 of 3"));
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  element(true);
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-") : blank_row(7));
  element(false);
  element(true);
  char_gap();
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT(is_code_shown ? marks_are(7, 2, "-.-") : blank_row(7));
  TEST_ASSERT(body_off(5, 5));
}

static void test_v134_dropped_after_wrong(void) {
  check_dropped_after_wrong(true);
  check_dropped_after_wrong(false);
}

/* V-135: a WORD_GAP after OVERFLOW never counts (D-T16, D-T31). */
static void test_v135_overflow_then_word_gap(void) {
  size_t run;

  for (run = 0U; run < 2U; ++run) {
    tx_open_one(LEVEL_WORDS_13, "KMRS");
    run_to_item();
    key("K");
    element(false);
    fake_keyer_push_event(KEYER_EVENT_OVERFLOW);
    drain();
    if (run == 0U) {
      char_gap();
      word_gap();
    } else {
      word_gap();
      key("M"); /* dropped up to its CHAR_GAP */
    }
    TEST_ASSERT(status(NULL));
    expect_word("KMRS", 1);
    TEST_ASSERT_EQUAL(1U, training_answer_length());
    TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
    key("MRS");
    TEST_ASSERT(status("Correct"));
    TEST_ASSERT_EQUAL(1U, training_score());
    TEST_ASSERT(margins_off());
  }
}

/* V-115: dropped characters and the overflow use no attempt. */
static void test_v115_dropped_characters(void) {
  const char* const items[2] = {"K", "M"};
  uint16_t t;

  tx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, true, items, 2U);
  run_to_item();
  key("K");
  t = harness_now;
  harness_run_to((uint16_t)(t + 99U));
  element(false);
  harness_run_to((uint16_t)(t + 399U));
  element(false);
  harness_run_to((uint16_t)(t + 900U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  expect_big(60, "M", -1);
  harness_run_to((uint16_t)(t + 999U));
  element(true);
  harness_run_to((uint16_t)(t + 1099U));
  char_gap();
  TEST_ASSERT(status(NULL));
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT(blank_row(7));
  /* A character item: the WORD_GAP does nothing (UX-52). */
  fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
  drain();
  TEST_ASSERT(status(NULL));
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  element(false);
  fake_keyer_push_event(KEYER_EVENT_OVERFLOW);
  drain();
  char_gap();
  TEST_ASSERT(status(NULL));
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  key("M");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(2U, training_score());
}

/* V-115: a DIT in the start pause drops its character after the item shows; an unknown pattern is wrong. */
static void test_drop_from_start_pause_and_unknown(void) {
  tx_open_one(LEVEL_KM, "K");
  harness_run_to(HARNESS_T0 + 900U);
  element(true);
  run_to_item();
  element(false);
  element(true);
  char_gap();
  TEST_ASSERT(status(NULL));
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  keyp("........");
  TEST_ASSERT(status("Wrong: *"));
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
}

/* V-116 */
static void test_v116_word(void) {
  uint16_t t;

  tx_open_one(LEVEL_WORDS_13, "KMRS");
  run_to_item();
  expect_word("KMRS", 0);
  key("K");
  expect_word("KMRS", 1);
  TEST_ASSERT(status(NULL));
  key("N");
  t = harness_now;
  TEST_ASSERT(status("Wrong: N"));
  expect_word("KMRS", 1);
  TEST_ASSERT(marks_are(7, 2, "-."));
  harness_run_to((uint16_t)(t + 1199U));
  TEST_ASSERT(marks_are(7, 2, "-."));
  harness_run_to((uint16_t)(t + 1200U));
  TEST_ASSERT(status("Try 2 of 3"));
  expect_word("KMRS", 0);
  TEST_ASSERT(blank_row(7));
  key("KMR");
  expect_word("KMRS", 3);
  key("S");
  TEST_ASSERT(status("Correct"));
  expect_word("KMRS", -1);
  TEST_ASSERT_EQUAL(0U, training_score());
}

typedef struct {
  uint8_t style;
  int ar_x;
  const char* ar_text;
  const char* wrong_text;
} v117_row_t;

/* V-117 */
static void test_v117_prosign_texts(void) {
  static const v117_row_t rows[3] = {
      {SETTINGS_PROSIGN_BOTH, 40, "<AR/+>", "Wrong: <BT/=>"},
      {SETTINGS_PROSIGN_NAME, 48, "<AR>", "Wrong: <BT>"},
      {SETTINGS_PROSIGN_PUNCTUATION, 60, "+", "Wrong: ="},
  };
  size_t index;

  for (index = 0U; index < 3U; ++index) {
    tx_open(LEVEL_ALL_CHARS, rows[index].style, true, (const char* const[]){"\x02"}, 1U);
    run_to_item();
    expect_big(rows[index].ar_x, rows[index].ar_text, -1);
    keyp("-...-");
    TEST_ASSERT(status(rows[index].wrong_text));
    run_to_item();
    keyp(".-.-.");
    TEST_ASSERT(status("Correct"));
  }
}

/* V-118: P3.2 held into the end screen gives no enter until it opens and closes again. */
static void test_v118_end_and_enter(void) {
  uint8_t number;
  size_t stops;

  tx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, true, (const char* const[]){"K"}, 1U);
  for (number = 1U; number <= 50U; ++number) {
    run_to_item();
    keyp("-.-");
    if (number == 49U) {
      TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
    }
  }
  /* The last CHAR_GAP came; P3.2 goes low before the end screen. */
  stops = fake_keyer_stop_count();
  harness_pins_for(BUTTON_BIT_ENTER, 700U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(stops + 1U, fake_keyer_stop_count());
  TEST_ASSERT(!fake_keyer_is_running());
  TEST_ASSERT(start_button());
  harness_pins_for(BUTTON_BIT_ENTER, 2000U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  harness_pins_for(0U, 100U);
  harness_pins_for(BUTTON_BIT_ENTER, 30U);
  harness_pins_for(0U, 30U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(2U, fake_keyer_start_count());
  TEST_ASSERT(fake_keyer_is_running());
  TEST_ASSERT(hdr("TX 20/20 L2          1/50"));
  TEST_ASSERT(body_off(1, 7));
}

/* V-119 */
static void test_v119_ignored_events_and_leave(void) {
  uint8_t page;

  tx_open_one(LEVEL_KM, "K");
  run_to_item();
  snapshot(snapshot_before);
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
  harness_render_count = 0U;
  harness_event(L_TAP);
  harness_event(M_TAP);
  harness_event(R_TAP);
  harness_event(REPEAT_EVENT(R_TAP));
  harness_event(REPEAT_EVENT(R_TAP));
  harness_event(LONG_EVENT(M_TAP));
  harness_event(LONG_EVENT(R_TAP));
  harness_pins_for(BUTTON_BIT_NEXT, 1500U);
  harness_pins_for(0U, 30U);
  TEST_ASSERT(is_screen_unchanged());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT_EQUAL(0U, harness_render_count);
  TEST_ASSERT_EQUAL(0U, fake_keyer_stop_count());
  harness_event(LONG_EVENT(L_TAP));
  TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(3U));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
}

/* AC-T20: a long menu/back leaves from every phase and gives P3.2 back. */
static void test_leave_in_each_phase(void) {
  static const uint8_t phases[4] = {TRAINING_PHASE_GAP, TRAINING_PHASE_ITEM, TRAINING_PHASE_RESULT, TRAINING_PHASE_END};
  size_t index;

  for (index = 0U; index < 4U; ++index) {
    tx_open_one(LEVEL_KM, "K");
    if (phases[index] != TRAINING_PHASE_GAP) {
      run_to_item();
    }
    if (phases[index] == TRAINING_PHASE_RESULT) {
      key("K");
    }
    if (phases[index] == TRAINING_PHASE_END) {
      uint8_t number;
      for (number = 1U; number <= 50U; ++number) {
        run_to_item();
        key("K");
      }
      harness_run_to((uint16_t)(harness_now + 700U));
    }
    TEST_ASSERT_EQUAL(phases[index], training_phase());
    harness_event(LONG_EVENT(L_TAP));
    TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    TEST_ASSERT_EQUAL(1U, menu_cursor());
    TEST_ASSERT(!fake_keyer_is_running());
    TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
    /* P3.2 is the menu enter again: a press opens Sending. */
    harness_pins_for(BUTTON_BIT_ENTER, 25U);
    harness_pins_for(0U, 25U);
    TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
    TEST_ASSERT_EQUAL(0U, harness_overlap_count);
    TEST_ASSERT_EQUAL(0U, fake_player_start_count());
  }
  /* The end screen: a short menu/back leaves too. */
  tx_open_one(LEVEL_KM, "K");
  harness_event(LONG_EVENT(L_TAP));
  harness_set_time(HARNESS_T0);
  harness_event(ENTER_EVENT);
  harness_event(ENTER_EVENT);
  {
    uint8_t number;
    for (number = 1U; number <= 50U; ++number) {
      run_to_item();
      key("K");
    }
  }
  harness_run_to((uint16_t)(harness_now + 700U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  harness_event(M_TAP);
  harness_event(R_TAP);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
  harness_event(L_TAP);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(1U, menu_cursor());
}

/* V-121 (Sending part): the longest texts stay inside their areas. */
static void test_v121_longest_texts(void) {
  int x;
  int y;

  /* Wrong: <KN/(> */
  tx_open_one(LEVEL_KM, "K");
  run_to_item();
  keyp("-.--.");
  TEST_ASSERT(status("Wrong: <KN/(>"));
  TEST_ASSERT(margins_off());
  /* 12 dashes: marks up to x 106; the 13th is not drawn. */
  run_to_item();
  {
    unsigned index;
    for (index = 0U; index < 13U; ++index) {
      element(true);
    }
  }
  TEST_ASSERT(marks_are(7, 2, "------------"));
  char_gap();
  TEST_ASSERT(status("Wrong: *"));
  /* Wrong: pause too long (21 cells, start column 2). */
  tx_open_one(LEVEL_WORDS_13, "KMRS");
  run_to_item();
  key("K");
  word_gap();
  TEST_ASSERT(text_at(1, 2, "Wrong: pause too long"));
  TEST_ASSERT(margins_off());
  /* An 8-character word: x 4-123. */
  {
    const char* const words[1] = {"ABCDEFGH"};
    tx_open(LEVEL_ALL_WORDS, SETTINGS_PROSIGN_BOTH, true, words, 1U);
  }
  run_to_item();
  expect_word("ABCDEFGH", 0);
  for (y = 24; y < 40; ++y) {
    for (x = 0; x < 128; ++x) {
      if ((x < 4) || (x > 123)) {
        TEST_ASSERT(!pixel(x, y));
      }
    }
  }
  TEST_ASSERT(margins_off());
}

#define LEVEL_FIRST_WORDS 2U /* word session 1: 10 words of 2 */

/* A Sending session of KM words at @p level: @p first_correct right on the first attempt, the rest wrong 3 times. */
static void tx_run_km_words(uint16_t level, uint8_t first_correct) {
  const char* const items[1] = {"KM"};
  uint8_t number;
  uint8_t attempt;
  uint8_t count;

  tx_open(level, SETTINGS_PROSIGN_BOTH, true, items, 1U);
  count = training_item_count();
  for (number = 1U; number <= count; ++number) {
    run_to_item();
    if (number <= first_correct) {
      key("KM");
      continue;
    }
    for (attempt = 0U; attempt < TRAINING_ATTEMPTS; ++attempt) {
      run_to_item();
      key("N");
    }
  }
  harness_run_to((uint16_t)(harness_now + TRAINING_FAILED_MS + 10U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
}

/* V-136 (Sending part), AC-T29 (2): a word session of 10 passes at 8 (D-T33). */
static void test_v136_word_pass_rate(void) {
  tx_run_km_words(LEVEL_FIRST_WORDS, 7U);
  TEST_ASSERT(text_at(2, 7, "Score 7/10"));
  TEST_ASSERT(text_at(3, 3, "Not passed, need 8"));
  TEST_ASSERT(end_screen_is(7U, 10U, 8U, 3U));
  TEST_ASSERT_EQUAL(LEVEL_FIRST_WORDS, progress_level(PROGRESS_MODE_SENDING, 20U));

  tx_run_km_words(LEVEL_FIRST_WORDS, 8U);
  TEST_ASSERT(text_at(3, 4, "Passed, new level"));
  TEST_ASSERT(end_screen_is(8U, 10U, 8U, 3U));
  TEST_ASSERT_EQUAL(LEVEL_FIRST_WORDS + 1U, progress_level(PROGRESS_MODE_SENDING, 20U));

  tx_run_km_words(LEVEL_100_WORDS, 79U);
  TEST_ASSERT(text_at(3, 3, "Not passed, need 80"));
  TEST_ASSERT(end_screen_is(79U, 100U, 80U, 154U));
  tx_run_km_words(LEVEL_100_WORDS, 80U);
  TEST_ASSERT(end_screen_is(80U, 100U, 80U, 154U));

  /* A character session still needs 45 of 50. */
  tx_run_km_words(LEVEL_KM, 44U);
  TEST_ASSERT(end_screen_is(44U, 50U, 45U, 1U));
}

/* V-121: Not passed, need 80 on a session of 100 words (D-T33). */
static void test_v121_not_passed_80(void) {
  unsigned number;

  tx_open(LEVEL_100_WORDS, SETTINGS_PROSIGN_BOTH, true, (const char* const[]){"E"}, 1U);
  TEST_ASSERT_EQUAL(100U, training_item_count());
  for (number = 0U; number < 100U; ++number) {
    run_to_item();
    key("T");
    run_to_item();
    key("E");
  }
  harness_run_to((uint16_t)(harness_now + 700U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT(text_at(2, 7, "Score 0/100"));
  TEST_ASSERT(text_at(3, 3, "Not passed, need 80"));
  TEST_ASSERT(lvl(154U, 154U));
  TEST_ASSERT(start_button());
  TEST_ASSERT(margins_off());
}

/* ---------- Level selection (ui-ux.md 5.16, V-108, V-137, V-142, V-143, V-147, AC-T33) ---------- */

/* EARN(TX, @p level) at TX Speed 20, then OPEN(TX): the start screen shows. */
static void tx_earn_open(uint16_t level) {
  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_SENDING, 20U, level);
  }
  harness_power_on();
  fake_koch_item_reset();
  fake_koch_item_push("K");
  harness_open_start_screen(1U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
}

/* Keys every item right until the end screen. */
static void tx_pass_session(void) {
  unsigned guard = 0U;
  while ((training_phase() != TRAINING_PHASE_END) && (guard < 200U)) {
    run_to_item();
    key(training_item_text());
    while ((training_phase() == TRAINING_PHASE_RESULT) || (training_phase() == TRAINING_PHASE_GAP)) {
      harness_set_time((uint16_t)(harness_now + 1U));
      harness_pass();
    }
    ++guard;
  }
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
}

/* V-137 (Sending), AC-T33 (1): the start screen; the keyer does not start; a limit redraws nothing. */
static void test_v137_start_screen(void) {
  tx_earn_open(0U);
  TEST_ASSERT(hdr("TX 20/20 L1          1/50"));
  TEST_ASSERT(body_off(1, 3));
  TEST_ASSERT(lvl(1U, 1U));
  TEST_ASSERT(desc("K and M"));
  TEST_ASSERT(blank_row(6));
  TEST_ASSERT(start_button());
  TEST_ASSERT(margins_off());
  snapshot(snapshot_before);
  harness_clear_dirty();
  harness_event(M_TAP);
  harness_event(R_TAP);
  harness_pins_for(BUTTON_BIT_NEXT, 1000U);
  harness_pins_for(0U, 30U);
  harness_pins_for(BUTTON_BIT_PREV, 1000U);
  harness_pins_for(0U, 30U);
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT(is_screen_unchanged());
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  TEST_ASSERT(!fake_keyer_is_running());
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(3U));
}

/* V-142, AC-T33 (3): P3.2 is enter on the start screen; its press starts the keyer once and the session. */
static void test_v142_tip_starts_keyer_once(void) {
  uint16_t start;

  tx_earn_open(19U);
  TEST_ASSERT(hdr("TX 20/20 L20         1/50"));
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  harness_clear_dirty();
  harness_event(M_TAP);
  TEST_ASSERT(dirty_is(PAGE(0) | PAGE(4) | PAGE(5)));
  TEST_ASSERT(hdr("TX 20/20 L19         1/50"));
  TEST_ASSERT(lvl(19U, 20U));
  harness_clear_dirty();
  harness_pins_for(BUTTON_BIT_ENTER, BUTTONS_DEBOUNCE_MS);
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  harness_pins_for(BUTTON_BIT_ENTER, 1U);
  start = harness_now;
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(18U, training_level());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask() & PAGE(0));
  TEST_ASSERT(hdr("TX 20/20 L19         1/50"));
  TEST_ASSERT(body_off(1, 7));
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT(fake_keyer_is_running());
  TEST_ASSERT_EQUAL(KEYER_KEY_IAMBIC_B, fake_keyer_last_key_type());
  TEST_ASSERT(!fake_keyer_last_is_swapped());
  TEST_ASSERT_EQUAL(20U, fake_keyer_last_wpm());
  TEST_ASSERT_EQUAL(20U, fake_keyer_last_eff_wpm());
  /* The contact stays closed to 2000 ms: enter is disabled in the session, so nothing more comes. */
  harness_pins_for(BUTTON_BIT_ENTER, (uint16_t)(2000U - BUTTONS_DEBOUNCE_MS - 1U));
  harness_pins_for(0U, 30U);
  harness_run_to((uint16_t)(start + 2100U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(1U, training_item_number());
  harness_pins_for(BUTTON_BIT_ENTER, 50U);
  harness_pins_for(0U, 50U);
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
  TEST_ASSERT_EQUAL(0U, harness_overlap_count);
}

/* V-143 (Sending), AC-T33 (5): a leave from the start screen stops no keyer and leaves P3.2 with the menu. */
static void test_v143_leave_start_screen(void) {
  uint8_t menu_image[FB_SIZE];

  tx_earn_open(19U);
  harness_event(M_TAP);
  harness_event(M_TAP);
  harness_event(M_TAP);
  harness_pins_for(BUTTON_BIT_MENU, (uint16_t)(BUTTONS_DEBOUNCE_MS + 1U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  TEST_ASSERT(menu_row_is(2, "Sending", "L20", true));
  snapshot(menu_image);
  harness_pins_for(BUTTON_BIT_MENU, 1000U);
  snapshot(snapshot_before);
  TEST_ASSERT_EQUAL(0, memcmp(menu_image, snapshot_before, FB_SIZE));
  harness_pins_for(0U, 30U);
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(0U, fake_keyer_stop_count());
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(3U));
  /* P3.2 is still the menu enter: a press opens Sending again at the earned level. */
  harness_pins_for(BUTTON_BIT_ENTER, 25U);
  harness_pins_for(0U, 25U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
  TEST_ASSERT(lvl(20U, 20U));
}

/* V-108 runs 4 and 5, AC-T33 (4), (5): the Sending end screen steps, leaves and starts at the press. */
static void test_v108_end_screen(void) {
  tx_earn_open(0U);
  harness_event(ENTER_EVENT);
  tx_pass_session();
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  TEST_ASSERT(text_at(3, 4, "Passed, new level"));
  TEST_ASSERT(lvl(2U, 2U));
  TEST_ASSERT(desc("New char R"));
  TEST_ASSERT(start_button());
  TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
  TEST_ASSERT(!fake_keyer_is_running());
  harness_clear_dirty();
  harness_event(M_TAP);
  TEST_ASSERT(lvl(1U, 2U));
  TEST_ASSERT(dirty_is(PAGE(4) | PAGE(5)));
  harness_pins_for(BUTTON_BIT_MENU, (uint16_t)(BUTTONS_DEBOUNCE_MS + 1U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
  harness_pins_for(0U, 30U);

  /* Run 5: P3.2 low for 100 ms starts a new session at its press; the keyer starts once. */
  harness_pins_for(BUTTON_BIT_ENTER, 25U);
  harness_pins_for(0U, 25U);
  harness_event(ENTER_EVENT);
  tx_pass_session();
  TEST_ASSERT_EQUAL(2U, fake_keyer_start_count());
  harness_pins_for(BUTTON_BIT_ENTER, BUTTONS_DEBOUNCE_MS);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  harness_pins_for(BUTTON_BIT_ENTER, 1U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT(hdr("TX 20/20 L3          1/10"));
  TEST_ASSERT_EQUAL(3U, fake_keyer_start_count());
  harness_pins_for(BUTTON_BIT_ENTER, (uint16_t)(100U - BUTTONS_DEBOUNCE_MS - 1U));
  harness_pins_for(0U, 30U);
  TEST_ASSERT_EQUAL(3U, fake_keyer_start_count());
  TEST_ASSERT(fake_keyer_is_running());
  /* A long menu/back in the session stops the keyer once and gives P3.2 back. */
  harness_event(LONG_EVENT(L_TAP));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(3U, fake_keyer_stop_count());
  TEST_ASSERT_EQUAL(0U, harness_overlap_count);
}

/* V-147: a replay pass at L1 below L20 stores nothing; a pass at L20 stores L21. */
static void test_v147_replay_and_pass(void) {
  tx_earn_open(19U);
  while (training_selected_level() != 0U) {
    harness_event(REPEAT_EVENT(M_TAP));
  }
  TEST_ASSERT(lvl(1U, 20U));
  harness_event(ENTER_EVENT);
  tx_pass_session();
  TEST_ASSERT(text_at(3, 9, "Passed"));
  TEST_ASSERT(lvl(2U, 20U));
  TEST_ASSERT(desc("New char R"));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(3U));

  tx_earn_open(19U);
  (void)settings_set(SETTINGS_ID_MAX_WORD_LEN, 5U);
  harness_event(ENTER_EVENT);
  tx_pass_session();
  TEST_ASSERT(hdr("TX 20/20 L20        50/50"));
  TEST_ASSERT(text_at(3, 4, "Passed, new level"));
  TEST_ASSERT(lvl(21U, 21U));
  TEST_ASSERT(desc("Words of 5"));
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(1U));
  TEST_ASSERT_EQUAL(1U, fake_iap_program_count(0x0200U));
}

/*
 * V-144 (Sending), D-T43: prev or next pressed 100 ms after the last keyed character and held 2000 ms through the
 * session end gives no step, no repeat and no long event on the end screen. A new press then steps once.
 */
static void test_v144_held_through_end(void) {
  static const uint8_t pins[2] = {BUTTON_BIT_NEXT, BUTTON_BIT_PREV};
  size_t run;

  for (run = 0U; run < 2U; ++run) {
    unsigned guard = 0U;

    tx_earn_open(19U);
    while (training_selected_level() != 9U) {
      harness_event(M_TAP);
    }
    harness_event(ENTER_EVENT);
    do {
      run_to_item();
      key(training_item_text());
      ++guard;
    } while ((training_item_number() < training_item_count()) && (guard < 100U));
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    harness_run_to((uint16_t)(harness_now + 100U));
    harness_pins_for(pins[run], 600U);
    TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
    TEST_ASSERT(lvl(11U, 20U));
    harness_clear_dirty();
    harness_pins_for(pins[run], 1400U);
    harness_pins_for(0U, 30U);
    TEST_ASSERT(dirty_is(0U));
    TEST_ASSERT(lvl(11U, 20U));
    harness_pins_for(pins[run], 30U);
    harness_pins_for(0U, 30U);
    TEST_ASSERT(dirty_is(PAGE(4) | PAGE(5)));
    TEST_ASSERT(lvl((run == 0U) ? 12U : 10U, 20U));
  }
}

int main(void) {
  TEST_RUN(test_v110_start);
  TEST_RUN(test_v111_correct);
  TEST_RUN(test_v112_v114_code_row);
  TEST_RUN(test_v113_v114_answer_code);
  TEST_RUN(test_v115_dropped_characters);
  TEST_RUN(test_drop_from_start_pause_and_unknown);
  TEST_RUN(test_v116_word);
  TEST_RUN(test_ac_t28_retry_screen);
  TEST_RUN(test_ac_t28_word_gap_without_effect);
  TEST_RUN(test_ac_t28_char_and_word_gap_in_one_poll);
  TEST_RUN(test_v128_pause_error);
  TEST_RUN(test_v129_word_gap_before_first_char);
  TEST_RUN(test_v130_word_gap_after_last_char);
  TEST_RUN(test_v131_three_pause_errors);
  TEST_RUN(test_v132_word_gap_in_delays);
  TEST_RUN(test_v133_no_word_gap);
  TEST_RUN(test_v134_dropped_after_wrong);
  TEST_RUN(test_v135_overflow_then_word_gap);
  TEST_RUN(test_v117_prosign_texts);
  TEST_RUN(test_v118_end_and_enter);
  TEST_RUN(test_v119_ignored_events_and_leave);
  TEST_RUN(test_leave_in_each_phase);
  TEST_RUN(test_v121_longest_texts);
  TEST_RUN(test_v121_not_passed_80);
  TEST_RUN(test_v136_word_pass_rate);
  TEST_RUN(test_v137_start_screen);
  TEST_RUN(test_v142_tip_starts_keyer_once);
  TEST_RUN(test_v143_leave_start_screen);
  TEST_RUN(test_v108_end_screen);
  TEST_RUN(test_v147_replay_and_pass);
  TEST_RUN(test_v144_held_through_end);
  return TEST_FINISH();
}
