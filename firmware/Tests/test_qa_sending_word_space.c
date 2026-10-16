/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA scenarios of the Sending retry screen and the word-space error (references/internal/training_spec.md D-T30,
 * D-T31, D-T45 to D-T47, sections 12.3, 12.6 and 16 AC-T17, AC-T28, AC-T43 to AC-T45; ui-ux.md UX-52, UX-67, UX-69,
 * sections 5.13.1 to 5.13.3, 5.14, 5.15 and 5.16.3). The real menu, buttons, engine and progress run with a fake
 * keyer and fake items.
 */

#include "cw/keyer_shared.h"
#include "cw/morse.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "qa/qa_training_screens.h"
#include "support/training_harness.h"

#define LEVEL_KM 0U
#define LEVEL_WORDS_13 17U /* a word session (koch_kind() gives KOCH_KIND_WORD) */
#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)
#define PAUSE_TEXT "Wrong: pause too long"
#define EVENTS_PER_POLL (KEYER_QUEUE_SIZE + 1U)

/* Kinds of wrong attempt. */
#define WRONG_CHAR 0U    /* a keyed character that is not the expected one */
#define WRONG_UNKNOWN 1U /* 8 dits: `*` */
#define WRONG_PAUSE 2U   /* WORD_GAP after a PARTIAL CHAR_GAP */
#define WRONG_PROSIGN 3U /* BT for a letter */
#define WRONG_KIND_COUNT 4U

static uint8_t screen_before[FB_SIZE];

/* ---------- Setup and stimulus ---------- */

static void tx_open_full(uint16_t level, uint8_t style, bool is_code_shown, uint8_t eff_wpm, const char* const* items,
                         size_t count) {
  size_t index;

  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_SENDING, 20U, level);
  }
  harness_power_on();
  (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, style);
  (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, is_code_shown ? 1U : 0U);
  (void)settings_set(SETTINGS_ID_TX_EFF_WPM, eff_wpm);
  fake_koch_item_reset();
  for (index = 0U; index < count; ++index) {
    fake_koch_item_push(items[index]);
  }
  harness_open_mode(1U);
}

static void tx_open(uint16_t level, bool is_code_shown, const char* const* items, size_t count) {
  tx_open_full(level, SETTINGS_PROSIGN_BOTH, is_code_shown, 20U, items, count);
}

static void tx_open_word(const char* word, bool is_code_shown) {
  const char* const items[1] = {word};
  tx_open(LEVEL_WORDS_13, is_code_shown, items, 1U);
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

static void push(keyer_event_t event) {
  fake_keyer_push_event(event);
  drain();
}

static void word_gap(void) { push(KEYER_EVENT_WORD_GAP); }

/* KEYP(p): the elements of @p pattern, then one CHAR_GAP. */
static void keyp(const char* pattern) {
  size_t index;
  for (index = 0U; pattern[index] != '\0'; ++index) {
    push((pattern[index] == '-') ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
  push(KEYER_EVENT_CHAR_GAP);
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

/* KEY(s) */
static void key(const char* text) {
  size_t index;
  for (index = 0U; text[index] != '\0'; ++index) {
    keyp(code_of(text[index]));
  }
}

static void run_to_phase(training_phase_t phase) {
  unsigned guard = 0U;
  while ((training_phase() != phase) && (guard < 20000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
}

static void run_to_item(void) { run_to_phase(TRAINING_PHASE_ITEM); }

/* ---------- Probes ---------- */

static void snapshot(uint8_t* buffer) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&buffer[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
}

static bool is_screen_unchanged(void) {
  uint8_t image[FB_SIZE];
  snapshot(image);
  return memcmp(screen_before, image, FB_SIZE) == 0;
}

typedef struct {
  uint8_t attempts;
  uint8_t answer_length;
  uint8_t score;
  training_phase_t phase;
  training_check_t check;
} engine_state_t;

static engine_state_t engine_state(void) {
  engine_state_t state;
  state.attempts = training_wrong_attempts();
  state.answer_length = training_answer_length();
  state.score = training_score();
  state.phase = training_phase();
  state.check = training_last_check();
  return state;
}

static engine_state_t state_before;

/* Takes the screen and the engine state before a step that must change nothing. */
static void no_effect_begin(void) {
  uint8_t page;
  snapshot(screen_before);
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
  state_before = engine_state();
}

/* No pixel, no dirty page, no attempt, no answer change, no phase change since no_effect_begin(). */
static bool is_no_effect(void) {
  const engine_state_t now = engine_state();
  return is_screen_unchanged() && (lcd_fb_dirty_mask() == 0U) && (now.attempts == state_before.attempts) &&
         (now.answer_length == state_before.answer_length) && (now.phase == state_before.phase) &&
         (now.check == state_before.check) && (now.score == state_before.score);
}

static void expect_word(const char* word, int bar_slot) {
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
  TEST_ASSERT(item_line_is());
}

/* Row 5 FAILED code: the marks of @p character centered, x0 = (128 - w) / 2. */
static bool answer_code_is(char character) {
  const char* code = code_of(character);
  int width = 0;
  size_t index;
  for (index = 0U; code[index] != '\0'; ++index) {
    width += ((code[index] == '-') ? 6 : 2) + 3;
  }
  width -= 3;
  return marks_are(5, (128 - width) / 2, code);
}

static bool row7_is(bool is_code_shown, const char* pattern) {
  return is_code_shown ? marks_are(7, 2, pattern) : blank_row(7);
}

/* ---------- Wrong attempts ---------- */

/* The status text of a wrong attempt of @p kind. */
static const char* wrong_status(uint8_t kind) {
  if (kind == WRONG_CHAR) {
    return "Wrong: N";
  }
  if (kind == WRONG_UNKNOWN) {
    return "Wrong: *";
  }
  if (kind == WRONG_PROSIGN) {
    return "Wrong: <BT/=>";
  }
  return PAUSE_TEXT;
}

/* The marks that row 7 keeps during RESULT after a wrong attempt of @p kind (UX-52: a pause keeps the last char). */
static const char* wrong_row7(uint8_t kind, const char* last_correct) {
  if (kind == WRONG_CHAR) {
    return "-.";
  }
  if (kind == WRONG_UNKNOWN) {
    return "........";
  }
  if (kind == WRONG_PROSIGN) {
    return "-...-";
  }
  return code_of(last_correct[0]);
}

static void wrong_attempt(uint8_t kind) {
  if (kind == WRONG_CHAR) {
    keyp("-.");
  } else if (kind == WRONG_UNKNOWN) {
    keyp("........");
  } else if (kind == WRONG_PROSIGN) {
    keyp("-...-");
  } else {
    word_gap();
  }
}

/*
 * AC-T28 (1), WS-6, ui-ux.md 5.14 retry table: every kind of wrong attempt, both Show CW Code values, a character
 * item and words with the error at different positions. Checks the RESULT screen for the full 1200 ms and the retry
 * redraw at exactly TRAINING_WRONG_MS.
 */
static void check_retry_screen(uint8_t kind, const char* item, size_t prefix, bool is_code_shown) {
  static const char* const try_texts[2] = {"Try 2 of 3", "Try 3 of 3"};
  const bool is_word = strlen(item) > 1U;
  char keyed[KOCH_WORD_LEN_MAX + 1U];
  uint8_t header[LCD_WIDTH];
  uint8_t attempt;
  uint16_t t;

  memcpy(keyed, item, prefix);
  keyed[prefix] = '\0';
  if (is_word) {
    tx_open_word(item, is_code_shown);
  } else {
    const char* const items[1] = {item};
    tx_open(LEVEL_KM, is_code_shown, items, 1U);
  }
  run_to_item();
  memcpy(header, lcd_fb_page(0U), LCD_WIDTH);
  for (attempt = 0U; attempt < 2U; ++attempt) {
    key(keyed);
    wrong_attempt(kind);
    t = harness_now;
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
    TEST_ASSERT_EQUAL(attempt + 1U, training_wrong_attempts());
    TEST_ASSERT(status(wrong_status(kind)));
    TEST_ASSERT(blank_row(5));
    TEST_ASSERT(blank_row(6));
    TEST_ASSERT(row7_is(is_code_shown, wrong_row7(kind, (prefix > 0U) ? &item[prefix - 1U] : "")));
    if (is_word) {
      expect_word(item, (int)prefix);
    }
    snapshot(screen_before);
    harness_run_to((uint16_t)(t + TRAINING_WRONG_MS - 1U));
    TEST_ASSERT(is_screen_unchanged());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    harness_run_to((uint16_t)(t + TRAINING_WRONG_MS));
    TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
    TEST_ASSERT(status(try_texts[attempt]));
    TEST_ASSERT(body_off(5, 7));
    TEST_ASSERT(body_off(2, 2));
    if (is_word) {
      expect_word(item, 0);
    } else {
      item_expect_reset();
      item_expect(ITEM_PART_BIG, 60, item);
      TEST_ASSERT(item_line_is());
    }
    TEST_ASSERT_EQUAL(0, memcmp(header, lcd_fb_page(0U), LCD_WIDTH));
    TEST_ASSERT(margins_off());
    TEST_ASSERT_EQUAL(0U, training_answer_length());
  }
  /* The first element of the new attempt draws its mark from x 2. */
  push(KEYER_EVENT_DAH);
  TEST_ASSERT(row7_is(is_code_shown, "-"));
  push(KEYER_EVENT_DIT);
  TEST_ASSERT(row7_is(is_code_shown, "-."));
  TEST_ASSERT(blank_row(5));
}

static void test_retry_screen_every_kind(void) {
  static const char* const words[3] = {"KM", "KMRS", "KMRSUAPT"};
  uint8_t kind;
  size_t shown;
  size_t word;

  for (shown = 0U; shown < 2U; ++shown) {
    for (kind = 0U; kind < WRONG_KIND_COUNT; ++kind) {
      if (kind != WRONG_PAUSE) {
        check_retry_screen(kind, "K", 0U, shown != 0U);
        check_retry_screen(kind, "KMRS", 0U, shown != 0U);
      }
      for (word = 0U; word < 3U; ++word) {
        const size_t length = strlen(words[word]);
        check_retry_screen(kind, words[word], 1U, shown != 0U);
        check_retry_screen(kind, words[word], length - 1U, shown != 0U);
      }
    }
  }
}

/*
 * D-T30 (1): row 7 keeps the keyed code for the whole RESULT, also when the dropped events of a new character, a
 * CHAR_GAP, a WORD_GAP or an OVERFLOW come in the delay. It goes off only at the retry.
 */
static void check_row7_kept_in_result(uint8_t kind) {
  uint16_t t;

  tx_open_word("KMRS", true);
  run_to_item();
  key("KM");
  wrong_attempt(kind);
  t = harness_now;
  TEST_ASSERT(marks_are(7, 2, wrong_row7(kind, "M")));
  no_effect_begin();
  harness_run_to((uint16_t)(t + 200U));
  push(KEYER_EVENT_WORD_GAP);
  push(KEYER_EVENT_OVERFLOW);
  push(KEYER_EVENT_DAH);
  push(KEYER_EVENT_DIT);
  push(KEYER_EVENT_CHAR_GAP);
  push(KEYER_EVENT_WORD_GAP);
  push(KEYER_EVENT_DIT);
  harness_run_to((uint16_t)(t + TRAINING_WRONG_MS - 1U));
  TEST_ASSERT(is_no_effect());
  TEST_ASSERT(marks_are(7, 2, wrong_row7(kind, "M")));
  harness_run_to((uint16_t)(t + TRAINING_WRONG_MS));
  TEST_ASSERT(body_off(5, 7));
  TEST_ASSERT(status("Try 2 of 3"));
  /* The DIT in the delay started a character: its rest is dropped up to its CHAR_GAP (D-T15). */
  push(KEYER_EVENT_DAH);
  TEST_ASSERT(body_off(5, 7));
  push(KEYER_EVENT_CHAR_GAP);
  TEST_ASSERT(body_off(5, 7));
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  key("KMRS");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(0U, training_score());
}

static void test_row7_kept_during_result(void) {
  uint8_t kind;
  for (kind = 0U; kind < WRONG_KIND_COUNT; ++kind) {
    check_row7_kept_in_result(kind);
  }
}

/*
 * D-T30 (2), V-128, AC-T17: a pause after character k, for k = 1 .. length - 1 of words of 2, 4 and 8 characters.
 * Status, bar under character k + 1 (slot k), row 5 blank, row 7 the marks of character k, the answer ends with ' '.
 */
static void check_pause_at(const char* word, size_t k, uint8_t style) {
  const char* const items[2] = {word, "UAPT"};
  char keyed[KOCH_WORD_LEN_MAX + 2U];
  uint16_t t;

  memcpy(keyed, word, k);
  keyed[k] = '\0';
  tx_open_full(LEVEL_WORDS_13, style, true, 20U, items, 2U);
  run_to_item();
  key(keyed);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT(status(NULL));
  word_gap();
  t = harness_now;
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(k + 1U, training_answer_length());
  keyed[k] = TRAINING_CHAR_WORD_SPACE;
  keyed[k + 1U] = '\0';
  TEST_ASSERT_EQUAL(0, strcmp(keyed, training_answer_text()));
  /* WS-1: 21 cells from column 2, the rest of row 1 blank. */
  TEST_ASSERT(text_at(1, 2, PAUSE_TEXT));
  TEST_ASSERT_EQUAL(21U, strlen(PAUSE_TEXT));
  expect_word(word, (int)k);
  TEST_ASSERT(blank_row(5));
  TEST_ASSERT(blank_row(6));
  TEST_ASSERT(marks_are(7, 2, code_of(word[k - 1U])));
  TEST_ASSERT(margins_off());
  harness_run_to((uint16_t)(t + TRAINING_WRONG_MS - 1U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  harness_run_to((uint16_t)(t + TRAINING_WRONG_MS));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  key(word);
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(0U, training_score());
}

static void test_pause_error_positions(void) {
  static const char* const words[3] = {"KM", "KMRS", "KMRSUAPT"};
  static const uint8_t styles[3] = {SETTINGS_PROSIGN_PUNCTUATION, SETTINGS_PROSIGN_NAME, SETTINGS_PROSIGN_BOTH};
  size_t word;
  size_t k;
  size_t style;

  for (word = 0U; word < 3U; ++word) {
    for (k = 1U; k < strlen(words[word]); ++k) {
      for (style = 0U; style < 3U; ++style) {
        check_pause_at(words[word], k, styles[style]);
      }
    }
  }
}

/* Three pause errors give FAILED at TRAINING_FAILED_MS, row 5 the code of the character under the bar. */
static void check_three_pauses(const char* word, size_t k3, bool is_code_shown) {
  const char* const items[2] = {word, "UAPT"};
  char keyed[KOCH_WORD_LEN_MAX + 1U];
  uint16_t t;

  tx_open(LEVEL_WORDS_13, is_code_shown, items, 2U);
  run_to_item();
  key("K");
  word_gap();
  run_to_item();
  key("K");
  word_gap();
  run_to_item();
  TEST_ASSERT(status("Try 3 of 3"));
  memcpy(keyed, word, k3);
  keyed[k3] = '\0';
  key(keyed);
  word_gap();
  t = harness_now;
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT_EQUAL(3U, training_wrong_attempts());
  TEST_ASSERT(status(PAUSE_TEXT));
  expect_word(word, (int)k3);
  TEST_ASSERT(answer_code_is(word[k3]));
  TEST_ASSERT(blank_row(6));
  TEST_ASSERT(row7_is(is_code_shown, code_of(word[k3 - 1U])));
  snapshot(screen_before);
  harness_run_to((uint16_t)(t + TRAINING_FAILED_MS - 1U));
  TEST_ASSERT(is_screen_unchanged());
  harness_run_to((uint16_t)(t + TRAINING_FAILED_MS));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT(body_off(1, 7));
  harness_run_to((uint16_t)(t + TRAINING_FAILED_MS + TRAINING_SENDING_GAP_MS));
  expect_word("UAPT", 0);
  TEST_ASSERT(status(NULL));
  TEST_ASSERT(body_off(5, 7));
  TEST_ASSERT_EQUAL(0U, training_score());
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
}

static void test_three_pauses_failed(void) {
  size_t shown;
  for (shown = 0U; shown < 2U; ++shown) {
    check_three_pauses("KMRS", 1U, shown != 0U);
    check_three_pauses("KMRS", 3U, shown != 0U);
    check_three_pauses("KMRSUAPT", 7U, shown != 0U);
    check_three_pauses("KM", 1U, shown != 0U);
  }
}

/*
 * Mixed wrong attempts in one item: the third error decides the status and row 5, the earlier kind does not leak.
 * Runs: pause, wrong, pause; wrong, pause, wrong; unknown, pause, pause; pause, pause, prosign.
 */
static void test_mixed_errors_one_item(void) {
  const char* const items[2] = {"KMRS", "UAPT"};

  /* Run 1: pause after K, N for R after KM, pause after KMR. */
  tx_open(LEVEL_WORDS_13, true, items, 2U);
  run_to_item();
  key("K");
  word_gap();
  TEST_ASSERT(status(PAUSE_TEXT));
  run_to_item();
  TEST_ASSERT(status("Try 2 of 3"));
  key("KM");
  keyp("-.");
  TEST_ASSERT(status("Wrong: N"));
  expect_word("KMRS", 2);
  TEST_ASSERT(marks_are(7, 2, "-."));
  run_to_item();
  TEST_ASSERT(status("Try 3 of 3"));
  TEST_ASSERT(body_off(5, 7));
  key("KMR");
  word_gap();
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(status(PAUSE_TEXT));
  expect_word("KMRS", 3);
  TEST_ASSERT(answer_code_is('S'));
  TEST_ASSERT(marks_are(7, 2, ".-."));

  /* Run 2: E for M, pause after KMR, E for M again: the third status is the keyed character. */
  tx_open(LEVEL_WORDS_13, true, items, 2U);
  run_to_item();
  key("K");
  keyp(".");
  TEST_ASSERT(status("Wrong: E"));
  run_to_item();
  key("KMR");
  word_gap();
  TEST_ASSERT(status(PAUSE_TEXT));
  expect_word("KMRS", 3);
  TEST_ASSERT(blank_row(5));
  run_to_item();
  key("K");
  keyp(".");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(status("Wrong: E"));
  expect_word("KMRS", 1);
  TEST_ASSERT(answer_code_is('M'));
  TEST_ASSERT(marks_are(7, 2, "."));

  /* Run 3: `*` as the first character, then two pauses after K. */
  tx_open(LEVEL_WORDS_13, false, items, 2U);
  run_to_item();
  keyp("........");
  TEST_ASSERT(status("Wrong: *"));
  expect_word("KMRS", 0);
  run_to_item();
  key("K");
  word_gap();
  run_to_item();
  key("KM");
  word_gap();
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(status(PAUSE_TEXT));
  TEST_ASSERT(answer_code_is('R'));
  TEST_ASSERT(blank_row(7));

  /* Run 4: two pauses, then BT for K. */
  tx_open(LEVEL_WORDS_13, true, items, 2U);
  run_to_item();
  key("KMR");
  word_gap();
  run_to_item();
  key("KM");
  word_gap();
  run_to_item();
  keyp("-...-");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(status("Wrong: <BT/=>"));
  TEST_ASSERT(answer_code_is('K'));
  expect_word("KMRS", 0);
  run_to_item();
  expect_word("UAPT", 0);
  TEST_ASSERT_EQUAL(0U, training_score());

  /* Run 5: one poll with a wrong CHAR_GAP and a WORD_GAP: the WORD_GAP falls in RESULT. Status of the character. */
  tx_open(LEVEL_WORDS_13, true, items, 2U);
  run_to_item();
  key("K");
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_DIT);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_pass();
  TEST_ASSERT_EQUAL(0U, fake_keyer_pending_count());
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  TEST_ASSERT(status("Wrong: N"));
  TEST_ASSERT_EQUAL(0, strcmp("KN", training_answer_text()));

  /* Run 6: one poll: K, PARTIAL CHAR_GAP, WORD_GAP, then the elements of M: the M is dropped (RESULT, D-T15). */
  tx_open(LEVEL_WORDS_13, true, items, 2U);
  run_to_item();
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_DIT);
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  TEST_ASSERT(fake_keyer_pending_count() <= EVENTS_PER_POLL);
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_pass();
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  TEST_ASSERT(status(PAUSE_TEXT));
  TEST_ASSERT(marks_are(7, 2, "-.-"));
  expect_word("KMRS", 1);
  run_to_item();
  key("KMRS");
  TEST_ASSERT(status("Correct"));
}

/* ---------- No-effect cases (AC-T28 (3), D-T30, D-T31, section 12.3 edge-case table) ---------- */

static void expect_word_gap_no_effect(void) {
  no_effect_begin();
  word_gap();
  TEST_ASSERT(is_no_effect());
}

static void test_no_effect_states(void) {
  const char* const words[2] = {"KMRS", "KMRS"};
  const char* const chars[3] = {"K", "M", "K"};

  /* Start pause, before the first character, after the last character, in GAP. */
  tx_open(LEVEL_WORDS_13, true, words, 2U);
  harness_run_to(HARNESS_T0 + 100U);
  expect_word_gap_no_effect();
  run_to_item();
  expect_word_gap_no_effect();
  expect_word_gap_no_effect();
  key("KMRS");
  expect_word_gap_no_effect();
  run_to_phase(TRAINING_PHASE_GAP);
  expect_word_gap_no_effect();
  run_to_item();
  /* After a wrong first character (RESULT) and before the first character of the retry. */
  keyp("-.");
  expect_word_gap_no_effect();
  run_to_item();
  expect_word_gap_no_effect();
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  key("KMRS");
  TEST_ASSERT_EQUAL(1U, training_score());

  /* Character session: before the character, after a correct one, after a wrong one, at the retry. */
  tx_open(LEVEL_KM, true, chars, 3U);
  run_to_item();
  expect_word_gap_no_effect();
  key("K");
  expect_word_gap_no_effect();
  run_to_item();
  key("K");
  TEST_ASSERT(status("Wrong: K"));
  expect_word_gap_no_effect();
  run_to_item();
  expect_word_gap_no_effect();
  key("M");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_score());
}

/*
 * OVERFLOW cases (D-T16, D-T31, V-135). The key case for the answer-length mutation: after K, OVERFLOW, then the
 * CHAR_GAP that clears the drop flag, then the WORD_GAP of the same space: the answer has 1 character, the phase is
 * ITEM, but the WORD_GAP follows a lost character and must do nothing.
 */
static void test_no_effect_after_overflow(void) {
  /* a) K, DIT, OVERFLOW, CHAR_GAP, WORD_GAP. */
  tx_open_word("KMRS", true);
  run_to_item();
  key("K");
  push(KEYER_EVENT_DIT);
  push(KEYER_EVENT_OVERFLOW);
  push(KEYER_EVENT_CHAR_GAP);
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  expect_word_gap_no_effect();
  key("MRS");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_score());

  /* b) K, OVERFLOW right after the PARTIAL CHAR_GAP, CHAR_GAP, WORD_GAP. */
  tx_open_word("KMRS", true);
  run_to_item();
  key("K");
  push(KEYER_EVENT_OVERFLOW);
  push(KEYER_EVENT_CHAR_GAP);
  expect_word_gap_no_effect();
  key("MRS");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_score());

  /* c) K, OVERFLOW, WORD_GAP with the drop flag set: dropped; the next character is lost up to its CHAR_GAP. */
  tx_open_word("KMRS", true);
  run_to_item();
  key("K");
  push(KEYER_EVENT_OVERFLOW);
  expect_word_gap_no_effect();
  key("M"); /* dropped */
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  TEST_ASSERT(status(NULL));
  expect_word_gap_no_effect();
  key("MRS");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_score());

  /* d) The same three in one poll each: K CHAR_GAP OVERFLOW CHAR_GAP WORD_GAP. */
  tx_open_word("KMRS", true);
  run_to_item();
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_DIT);
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  fake_keyer_push_event(KEYER_EVENT_OVERFLOW);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_pass();
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  TEST_ASSERT(status(NULL));
}

/* A DIT after the PARTIAL CHAR_GAP clears the armed flag; a dropped character (D-T15) too. */
static void test_no_effect_after_dit_and_dropped(void) {
  tx_open_word("KMRS", true);
  run_to_item();
  key("K");
  push(KEYER_EVENT_DAH);
  expect_word_gap_no_effect();
  push(KEYER_EVENT_DAH);
  push(KEYER_EVENT_CHAR_GAP);
  TEST_ASSERT_EQUAL(2U, training_answer_length());
  /* Now armed again: the pause counts. */
  word_gap();
  TEST_ASSERT(status(PAUSE_TEXT));
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());

  /* D-T15: a DIT in RESULT, the rest of that character after the retry. */
  tx_open_word("KMRS", true);
  run_to_item();
  key("K");
  keyp("-.");
  push(KEYER_EVENT_DIT);
  run_to_item();
  push(KEYER_EVENT_DAH);
  push(KEYER_EVENT_CHAR_GAP);
  expect_word_gap_no_effect();
  key("K");
  push(KEYER_EVENT_DIT);
  push(KEYER_EVENT_OVERFLOW);
  push(KEYER_EVENT_CHAR_GAP);
  expect_word_gap_no_effect();
  key("MRS");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
}

/*
 * D-T31 (2): the WORD_GAP itself clears the armed flag. The fake gives a second WORD_GAP with no event between: in
 * RESULT it is dropped, and after the retry it does nothing.
 */
static void test_word_gap_clears_itself(void) {
  tx_open_word("KMRS", true);
  run_to_item();
  key("KM");
  word_gap();
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  run_to_item();
  TEST_ASSERT(status("Try 2 of 3"));
  expect_word_gap_no_effect();
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());

  /* The same with both WORD_GAP events in one poll. */
  tx_open_word("KMRS", true);
  run_to_item();
  key("K");
  fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
  fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_pass();
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  run_to_item();
  expect_word_gap_no_effect();
}

/*
 * Section 12.3, "Late WORD_GAP of an earlier space": at 20/5 the WORD_GAP comes about 2669 ms after the last mark,
 * after TRAINING_CORRECT_MS + TRAINING_SENDING_GAP_MS (900 ms) and after TRAINING_WRONG_MS (1200 ms). It then falls in
 * ITEM of the next item or of the retry, before its first character. CHAR_GAP at 2 dits = 120 ms at 20 WPM.
 */
#define LATE_WORD_GAP_MS (2669U - 120U)

static void test_late_word_gap_low_eff_speed(void) {
  const char* const items[2] = {"KMRS", "UAPT"};
  uint16_t t;

  /* Into the next item. */
  tx_open_full(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, true, 5U, items, 2U);
  TEST_ASSERT_EQUAL(5U, fake_keyer_last_eff_wpm());
  TEST_ASSERT_EQUAL(20U, fake_keyer_last_wpm());
  run_to_item();
  key("KMRS");
  t = harness_now;
  TEST_ASSERT(status("Correct"));
  harness_run_to((uint16_t)(t + LATE_WORD_GAP_MS - 1U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(2U, training_item_number());
  expect_word_gap_no_effect();
  TEST_ASSERT_EQUAL((uint16_t)(t + LATE_WORD_GAP_MS), harness_now);
  key("UAPT");
  TEST_ASSERT_EQUAL(2U, training_score());

  /* Into the retry, after a wrong character. */
  tx_open_full(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, true, 5U, items, 2U);
  run_to_item();
  key("KM");
  keyp("-.");
  t = harness_now;
  harness_run_to((uint16_t)(t + LATE_WORD_GAP_MS - 1U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT(status("Try 2 of 3"));
  expect_word_gap_no_effect();
  key("KMRS");
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());

  /* Into the retry, after a pause error: the space after the WORD_GAP gives no second WORD_GAP; a fake one. */
  tx_open_full(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, true, 5U, items, 2U);
  run_to_item();
  key("K");
  word_gap();
  t = harness_now;
  harness_run_to((uint16_t)(t + 1500U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  expect_word_gap_no_effect();

  /* Into the blank moment after FAILED (2500 + 300 ms). */
  tx_open_full(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, true, 5U, items, 2U);
  run_to_item();
  keyp("-.");
  run_to_item();
  keyp("-.");
  run_to_item();
  key("KMR");
  keyp("-.");
  t = harness_now;
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  harness_run_to((uint16_t)(t + LATE_WORD_GAP_MS - 1U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  expect_word_gap_no_effect();
  run_to_item();
  expect_word_gap_no_effect();
  key("UAPT");
  TEST_ASSERT_EQUAL(1U, training_score());
}

/* tx_start_session() clears the armed flag: leave the mode armed, open it again, a first WORD_GAP does nothing. */
static void test_reentry_clears_armed(void) {
  tx_open_word("KMRS", true);
  run_to_item();
  key("KM");
  TEST_ASSERT_EQUAL(2U, training_answer_length());
  harness_event(LONG_EVENT(BUTTON_EVENT_MENU));
  TEST_ASSERT(menu_screen() != MENU_SCREEN_SENDING);
  harness_set_time((uint16_t)(harness_now + 10U));
  harness_event(ENTER_EVENT);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
  harness_event(ENTER_EVENT); /* the start screen (D-T35) */
  run_to_item();
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  expect_word_gap_no_effect();
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
}

/* ---------- Static facts behind D-T31 ---------- */

/* No morse_decode() result and no Koch character is ' ', so the word space never matches an item character. */
static void test_word_space_never_matches(void) {
  unsigned pattern;
  uint8_t index;

  for (pattern = 0U; pattern < 256U; ++pattern) {
    TEST_ASSERT(morse_decode((uint8_t)pattern) != TRAINING_CHAR_WORD_SPACE);
  }
  for (index = 0U; index < KOCH_CHAR_COUNT; ++index) {
    TEST_ASSERT(koch_char(index) != TRAINING_CHAR_WORD_SPACE);
  }
  TEST_ASSERT_EQUAL(' ', TRAINING_CHAR_WORD_SPACE);
}

/* ---------- Random walk with a reference model of section 12.3 ---------- */

typedef struct {
  bool is_dropping;
  bool is_armed;
  uint8_t pattern;
  uint8_t attempts;
  uint8_t answer_length;
  uint8_t score;
  bool is_last_space;
  training_phase_t phase;
} model_t;

static model_t model;
static uint32_t rng_state;
static unsigned long walk_pause_errors;
static unsigned long walk_failed;
static unsigned long walk_retries;

static uint32_t walk_random(void) {
  rng_state = (rng_state * 1103515245UL) + 12345UL;
  return (rng_state >> 16) & 0x7FFFU;
}

static void model_reset_session(void) {
  memset(&model, 0, sizeof(model));
  model.pattern = MORSE_PATTERN_EMPTY;
  model.phase = training_phase();
}

static void model_check(char character) {
  const char* item = training_item_text();
  const uint8_t length = training_item_length();

  model.is_last_space = (character == TRAINING_CHAR_WORD_SPACE);
  if (character == item[model.answer_length]) {
    ++model.answer_length;
    if (model.answer_length < length) {
      model.is_armed = true;
      return;
    }
    if (model.attempts == 0U) {
      ++model.score;
    }
  } else {
    ++model.answer_length;
    ++model.attempts;
    if (model.is_last_space) {
      ++walk_pause_errors;
    }
    if (model.attempts == TRAINING_ATTEMPTS) {
      ++walk_failed;
    }
  }
  model.phase = TRAINING_PHASE_RESULT;
}

/* One keyer event, read in the order of the section 12.3 table. */
static void model_event(keyer_event_t event) {
  const bool was_armed = model.is_armed;

  model.is_armed = false;
  if ((event == KEYER_EVENT_WORD_GAP) && was_armed && (model.phase == TRAINING_PHASE_ITEM)) {
    TEST_ASSERT(!model.is_dropping);
    model_check(TRAINING_CHAR_WORD_SPACE);
    return;
  }
  if (model.is_dropping) {
    if (event == KEYER_EVENT_CHAR_GAP) {
      model.is_dropping = false;
    }
    return;
  }
  if ((event == KEYER_EVENT_DIT) || (event == KEYER_EVENT_DAH)) {
    if (model.phase != TRAINING_PHASE_ITEM) {
      model.is_dropping = true;
    } else {
      model.pattern = morse_add_element(model.pattern, event == KEYER_EVENT_DAH);
    }
    return;
  }
  if (model.phase != TRAINING_PHASE_ITEM) {
    return;
  }
  if (event == KEYER_EVENT_OVERFLOW) {
    model.pattern = MORSE_PATTERN_EMPTY;
    model.is_dropping = true;
    return;
  }
  if ((event == KEYER_EVENT_CHAR_GAP) && (model.pattern != MORSE_PATTERN_EMPTY)) {
    const char decoded = morse_decode(model.pattern);
    model.pattern = MORSE_PATTERN_EMPTY;
    model_check(decoded);
  }
}

/* STATUS("Wrong: pause too long") without the diagnostics of row_matches(). */
static bool status_is_pause_quiet(void) {
  int x;
  int y;
  for (y = 8; y < 16; ++y) {
    for (x = 0; x < 128; ++x) {
      const int first_x = 2 + 5 * 2;
      bool expected = false;
      if ((x >= first_x) && (x < first_x + 5 * 21)) {
        expected = golden_ink(PAUSE_TEXT[(x - first_x) / 5], (x - first_x) % 5, y - 8);
      }
      if (pixel(x, y) != expected) {
        return false;
      }
    }
  }
  return true;
}

static void walk_compare(void) {
  TEST_ASSERT_EQUAL(model.attempts, training_wrong_attempts());
  TEST_ASSERT_EQUAL(model.answer_length, training_answer_length());
  TEST_ASSERT_EQUAL(model.score, training_score());
  TEST_ASSERT_EQUAL(model.phase, training_phase());
  /* Row 6 stays off in every Sending state: the RETRY clear of rows 5 to 7 erases nothing. */
  TEST_ASSERT(blank_row(6));
  if ((model.phase == TRAINING_PHASE_RESULT) && (training_last_check() != TRAINING_CHECK_CORRECT)) {
    /* A keyed character shows "Wrong: <text>", 8 to 13 cells; only the word space shows the 21-cell text. */
    TEST_ASSERT_EQUAL(model.is_last_space, status_is_pause_quiet());
  }
}

/* Follows the time-driven engine transitions after a pass. */
static void walk_follow_engine(void) {
  const training_phase_t phase = training_phase();

  if (phase == model.phase) {
    return;
  }
  if ((model.phase == TRAINING_PHASE_RESULT) && (phase == TRAINING_PHASE_ITEM)) {
    TEST_ASSERT(model.attempts > 0U);
    TEST_ASSERT(model.attempts < TRAINING_ATTEMPTS);
    model.answer_length = 0U;
    ++walk_retries;
    /* D-T30 retry screen. */
    TEST_ASSERT(body_off(5, 7));
    TEST_ASSERT(status((model.attempts == 1U) ? "Try 2 of 3" : "Try 3 of 3"));
  } else if (phase == TRAINING_PHASE_ITEM) {
    TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, model.phase);
    model.answer_length = 0U;
    model.attempts = 0U;
    TEST_ASSERT(body_off(5, 7));
  }
  model.phase = phase;
}

static void walk_pass(void) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_pass();
  walk_follow_engine();
}

/* Pushes @p count events, applies them to the model at the phase of the pass that reads them. */
static void walk_events(const keyer_event_t* events, size_t count) {
  size_t index;
  for (index = 0U; index < count; ++index) {
    fake_keyer_push_event(events[index]);
    model_event(events[index]);
  }
  walk_pass();
  TEST_ASSERT_EQUAL(0U, fake_keyer_pending_count());
  walk_compare();
}

static void walk_one_event(keyer_event_t event) { walk_events(&event, 1U); }

static void walk_key_pattern(uint8_t pattern) {
  uint8_t shift = 0U;
  while ((uint8_t)(pattern >> shift) > 1U) {
    ++shift;
  }
  while (shift != 0U) {
    --shift;
    walk_one_event((((pattern >> shift) & 1U) != 0U) ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
  walk_one_event(KEYER_EVENT_CHAR_GAP);
}

static void walk_step(void) {
  const uint32_t choice = walk_random() % 16U;
  const char* item = training_item_text();
  const uint8_t position = training_answer_length();
  static const char wrong_pool[] = "KMRSUAPTLOWIEN";

  if (choice < 5U) {
    /* The expected character, when there is one. */
    if ((training_phase() == TRAINING_PHASE_ITEM) && (position < training_item_length())) {
      walk_key_pattern(morse_encode(item[position]));
    } else {
      walk_key_pattern(morse_encode('K'));
    }
  } else if (choice == 5U) {
    walk_key_pattern(morse_encode(wrong_pool[walk_random() % (sizeof(wrong_pool) - 1U)]));
  } else if (choice < 9U) {
    walk_one_event(KEYER_EVENT_WORD_GAP);
  } else if (choice == 9U) {
    walk_one_event(((walk_random() & 1U) != 0U) ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  } else if (choice == 10U) {
    walk_one_event(KEYER_EVENT_CHAR_GAP);
  } else if (choice == 11U) {
    walk_one_event(KEYER_EVENT_OVERFLOW);
  } else if (choice == 12U) {
    /* Several events in one poll, at most the per-poll limit. */
    keyer_event_t events[EVENTS_PER_POLL];
    const size_t count = 2U + (walk_random() % (EVENTS_PER_POLL - 1U));
    size_t index;
    for (index = 0U; index < count; ++index) {
      events[index] = (keyer_event_t)(KEYER_EVENT_DIT + (walk_random() % 5U));
    }
    walk_events(events, count);
  } else {
    /* Time passes: up to 1500 ms, one pass per ms. */
    const unsigned span = 1U + (walk_random() % 1500U);
    unsigned ms;
    for (ms = 0U; ms < span; ++ms) {
      walk_pass();
      if (training_phase() == TRAINING_PHASE_END) {
        break;
      }
    }
    walk_compare();
  }
}

/*
 * The session end of the walk (SL-7, D-T45 (4), B-5): the end screen, then the first enter opens the start screen
 * with the code of the new character for every Show CW Code value (UX-67) and starts no keyer. The second enter
 * starts the next session and the keyer once.
 */
static unsigned long walk_session_ends;

static void walk_next_session(void) {
  const size_t starts = fake_keyer_start_count();
  const uint16_t selected = training_selected_level();
  char code[16] = "";

  ++walk_session_ends;
  TEST_ASSERT(body_off(4, 6));
  TEST_ASSERT(qa_ok_button());
  TEST_ASSERT(!fake_keyer_is_running());
  harness_event(ENTER_EVENT);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT_EQUAL(starts, fake_keyer_start_count());
  TEST_ASSERT(qa_start_button());
  if (selected == 0U) {
    (void)snprintf(code, sizeof(code), "%s ", code_of(koch_char(0U)));
    (void)strncat(code, code_of(koch_char(1U)), sizeof(code) - strlen(code) - 1U);
  } else if (koch_kind(selected) != KOCH_KIND_WORD) {
    (void)snprintf(code, sizeof(code), "%s", code_of(koch_char((uint8_t)(koch_learned_count(selected) - 1U))));
  }
  TEST_ASSERT(qa_code(code));
  harness_event(ENTER_EVENT);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(starts + 1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(selected, training_level());
}

static void walk_session(uint16_t level, const char* const* items, size_t count, uint32_t seed, unsigned steps) {
  unsigned step;

  rng_state = seed;
  tx_open_full(level, SETTINGS_PROSIGN_BOTH, (seed & 1U) != 0U, 20U, items, count);
  model_reset_session();
  for (step = 0U; step < steps; ++step) {
    if (training_phase() == TRAINING_PHASE_END) {
      walk_next_session();
      model_reset_session();
      continue;
    }
    walk_step();
  }
}

static void test_random_walk(void) {
  static const char* const words[5] = {"KMRS", "KM", "KMRSUAPT", "LOW", "SAT"};
  static const char* const chars[2] = {"K", "M"};
  uint32_t seed;
  const unsigned long failures_before = test_failure_count;

  walk_pause_errors = 0U;
  walk_session_ends = 0U;
  walk_failed = 0U;
  walk_retries = 0U;
  for (seed = 1U; seed <= 40U; ++seed) {
    walk_session(LEVEL_WORDS_13, words, 5U, seed, 300U);
    if (test_failure_count != failures_before) {
      fprintf(stderr, "random walk: first failure with word seed %u\n", (unsigned)seed);
      return;
    }
  }
  for (seed = 1U; seed <= 10U; ++seed) {
    walk_session(LEVEL_KM, chars, 2U, seed, 200U);
    if (test_failure_count != failures_before) {
      fprintf(stderr, "random walk: first failure with char seed %u\n", (unsigned)seed);
      return;
    }
  }
  /* The walk reaches the paths under test. */
  printf("random walk: %lu pause errors, %lu failed items, %lu retries, %lu session ends\n", walk_pause_errors,
         walk_failed, walk_retries, walk_session_ends);
  TEST_ASSERT(walk_pause_errors > 20U);
  TEST_ASSERT(walk_failed > 5U);
  TEST_ASSERT(walk_retries > 20U);
}

int main(void) {
  TEST_RUN(test_word_space_never_matches);
  TEST_RUN(test_retry_screen_every_kind);
  TEST_RUN(test_row7_kept_during_result);
  TEST_RUN(test_pause_error_positions);
  TEST_RUN(test_three_pauses_failed);
  TEST_RUN(test_mixed_errors_one_item);
  TEST_RUN(test_no_effect_states);
  TEST_RUN(test_no_effect_after_overflow);
  TEST_RUN(test_no_effect_after_dit_and_dropped);
  TEST_RUN(test_word_gap_clears_itself);
  TEST_RUN(test_late_word_gap_low_eff_speed);
  TEST_RUN(test_reentry_clears_armed);
  TEST_RUN(test_random_walk);
  return TEST_FINISH();
}
