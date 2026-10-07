/*
 * Host tests of the session engine (references/internal/training_spec.md sections 5.10 and 11, AC-T16 to AC-T18,
 * AC-T30 and AC-T31).
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "eeprom/eeprom_map.h"
#include "fake_iap.h"
#include "fake_tick.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "progress/progress.h"
#include "support/progress_fixture.h"
#include "test_assert.h"
#include "util/rng.h"

#define RX TRAINING_MODE_RECEIVING
#define TX TRAINING_MODE_SENDING
#define CMD_PROGRAM 2U
#define CMD_ERASE 3U

static uint16_t now;

static void at(uint16_t time) {
  now = time;
  fake_tick_set_now(now);
}

/* Empty EEPROM, or a record with @p level for (@p mode, @p wpm). Loads it and prepares the engine at t = 1000. */
static void prepare_at_level(uint8_t mode, uint8_t wpm, uint16_t level, uint8_t max_word_len) {
  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(mode, wpm, level);
  }
  (void)progress_load();
  fake_iap_clear_counters();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(0U);
  rng_seed(0x1234U);
  at(1000U);
  training_prepare(mode, wpm, max_word_len);
}

/* As prepare_at_level(), then starts a session at the earned level. */
static void start_at_level(uint8_t mode, uint8_t wpm, uint16_t level, uint8_t max_word_len) {
  prepare_at_level(mode, wpm, level, max_word_len);
  training_start();
}

static void restart(uint8_t mode, uint8_t wpm, uint8_t max_word_len) {
  training_prepare(mode, wpm, max_word_len);
  training_start();
}

/* D-T36 invariant: 0 <= selected <= earned. */
static void check_selection_invariant(void) { TEST_ASSERT(training_selected_level() <= training_earned_level()); }

/* Polls once at @p time and returns the event. */
static training_event_t poll_at(uint16_t time) {
  at(time);
  return training_poll();
}

static char wrong_char_for(char expected) { return (expected == 'K') ? 'M' : 'K'; }

/* AC-T16: start -> GAP -> ITEM -> RESULT -> GAP -> ITEM with the exact delays. */
static void test_flow_receiving(void) {
  char item;

  start_at_level(RX, 20U, 0U, 5U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(0U, training_item_number());
  TEST_ASSERT_EQUAL(1U, training_shown_item_number());
  TEST_ASSERT_EQUAL(50U, training_item_count());
  TEST_ASSERT_EQUAL(45U, training_pass_threshold());
  TEST_ASSERT_EQUAL(1U, training_shown_level());
  TEST_ASSERT_EQUAL(KOCH_KIND_CHAR, training_kind());
  TEST_ASSERT_EQUAL(2U, training_learned_count());
  TEST_ASSERT_EQUAL(2U, training_alphabet_count());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(1999U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(2000U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(1U, training_item_number());
  TEST_ASSERT_EQUAL(1U, training_shown_item_number());
  TEST_ASSERT_EQUAL(1U, training_item_length());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  item = training_item_text()[0];
  TEST_ASSERT((item == 'K') || (item == 'M'));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(30000U));

  at(30100U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_add_char(item));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  TEST_ASSERT_EQUAL(1U, training_score());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(30699U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, poll_at(30700U));
  TEST_ASSERT_EQUAL(2U, training_shown_item_number());
  TEST_ASSERT_EQUAL(1U, training_item_number());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(31199U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(31200U));
  TEST_ASSERT_EQUAL(2U, training_item_number());

  /* Wrong: RESULT for 1200 ms, then RETRY with the same item and an empty answer. */
  item = training_item_text()[0];
  at(32000U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_add_char(wrong_char_for(item)));
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(33199U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at(33200U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL((uint8_t)item, (uint8_t)training_item_text()[0]);
  TEST_ASSERT_EQUAL(2U, training_item_number());

  /* Second and third wrong: FAILED, then 2500 ms. */
  at(33300U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_add_char(wrong_char_for(item)));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at(34500U));
  at(34600U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_add_char(wrong_char_for(item)));
  TEST_ASSERT_EQUAL(3U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(37099U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, poll_at(37100U));
  TEST_ASSERT_EQUAL(1U, training_score());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(37599U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(37600U));
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(3U, training_item_number());
}

/* Sending uses the 300 ms gap; one event per poll also after a long time. */
static void test_flow_sending_gap(void) {
  start_at_level(TX, 20U, 0U, 5U);
  TEST_ASSERT_EQUAL(TX, training_mode());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(20000U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(20000U));
  at(20010U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_add_char(training_item_text()[0]));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, poll_at(60000U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(60299U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(60300U));
  /* Unknown mode gives RECEIVING. */
  start_at_level(7U, 20U, 0U, 5U);
  TEST_ASSERT_EQUAL(RX, training_mode());
}

/* The tick wraps at 65536 ms. */
static void test_tick_wrap(void) {
  start_at_level(RX, 20U, 0U, 5U);
  at(65000U);
  restart(RX, 20U, 5U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(463U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(464U));
}

/* AC-T17: Receiving gives PARTIAL until the last character, then one whole-word check. */
static void test_receiving_word_check(void) {
  const char* item;

  start_at_level(RX, 20U, 2U, 8U); /* word session 1: words of 2 */
  TEST_ASSERT_EQUAL(KOCH_KIND_WORD, training_kind());
  TEST_ASSERT_EQUAL(10U, training_item_count());
  TEST_ASSERT_EQUAL(8U, training_pass_threshold());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(2000U));
  item = training_item_text();
  TEST_ASSERT_EQUAL(2U, training_item_length());
  TEST_ASSERT_EQUAL(2U, strlen(item));
  /* A wrong first character still gives PARTIAL. */
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char('*'));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_add_char(item[1]));
  TEST_ASSERT(strcmp("*", training_answer_text()) != 0);
  TEST_ASSERT_EQUAL(2U, training_answer_length());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at(3200U));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(item[0]));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_add_char(item[1]));
  /* A correct item after a wrong attempt does not count. */
  TEST_ASSERT_EQUAL(0U, training_score());
}

/* AC-T17: Sending ends the attempt at the first wrong character. */
static void test_sending_char_check(void) {
  const char* item;

  start_at_level(TX, 20U, 2U, 8U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(2000U));
  item = training_item_text();
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_add_char('*'));
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at(3200U));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(item[0]));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_add_char('*'));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at(4400U));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(item[0]));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_add_char('*'));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(6899U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, poll_at(6900U));
  TEST_ASSERT_EQUAL(0U, training_score());
}

/*
 * AC-T17, D-T31: TRAINING_CHAR_WORD_SPACE after k accepted characters is a wrong attempt: WRONG twice, then FAILED,
 * with the usual delays. The answer ends with it and the item does not count.
 */
static void check_sending_word_space(uint8_t k) {
  const char* item;
  uint16_t t = 2000U;
  uint8_t attempt;
  uint8_t index;

  start_at_level(TX, 20U, 17U, 8U); /* word session 6: words of 4 */
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(t));
  item = training_item_text();
  TEST_ASSERT_EQUAL(4U, training_item_length());
  for (attempt = 1U; attempt <= TRAINING_ATTEMPTS; ++attempt) {
    for (index = 0U; index < k; ++index) {
      TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(item[index]));
    }
    TEST_ASSERT_EQUAL((attempt < TRAINING_ATTEMPTS) ? TRAINING_CHECK_WRONG : TRAINING_CHECK_FAILED,
                      training_add_char(TRAINING_CHAR_WORD_SPACE));
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    TEST_ASSERT_EQUAL(attempt, training_wrong_attempts());
    TEST_ASSERT_EQUAL(k + 1U, training_answer_length());
    TEST_ASSERT_EQUAL((uint8_t)TRAINING_CHAR_WORD_SPACE, (uint8_t)training_answer_text()[k]);
    TEST_ASSERT_EQUAL(0, strncmp(item, training_answer_text(), k));
    if (attempt < TRAINING_ATTEMPTS) {
      TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at((uint16_t)(t + TRAINING_WRONG_MS - 1U)));
      t = (uint16_t)(t + TRAINING_WRONG_MS);
      TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at(t));
      TEST_ASSERT_EQUAL(0U, training_answer_length());
    }
  }
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at((uint16_t)(t + TRAINING_FAILED_MS - 1U)));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, poll_at((uint16_t)(t + TRAINING_FAILED_MS)));
  TEST_ASSERT_EQUAL(0U, training_score());
}

static void test_sending_word_space(void) {
  TEST_ASSERT_EQUAL((uint8_t)' ', (uint8_t)TRAINING_CHAR_WORD_SPACE);
  check_sending_word_space(1U);
  check_sending_word_space(3U); /* length - 1 */
}

/* AC-T17: characters outside ITEM give IGNORED and change nothing; undo works only in ITEM. */
static void test_ignored_and_undo(void) {
  start_at_level(RX, 20U, 2U, 8U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_IGNORED, training_add_char('K'));
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT(!training_undo_char());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(2000U));
  TEST_ASSERT(!training_undo_char());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char('*'));
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  TEST_ASSERT(training_undo_char());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(0U, (uint8_t)training_answer_text()[0]);
  TEST_ASSERT(!training_undo_char());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(training_item_text()[0]));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_add_char(training_item_text()[1]));
  /* RESULT */
  TEST_ASSERT_EQUAL(TRAINING_CHECK_IGNORED, training_add_char('K'));
  TEST_ASSERT(!training_undo_char());
  TEST_ASSERT_EQUAL(2U, training_answer_length());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  TEST_ASSERT_EQUAL(1U, training_score());
  training_stop();
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_IGNORED, training_add_char('K'));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(60000U));
}

/* AC-T17, D-T14: a held RESULT never ends; a release at tick P ends it at P + TRAINING_FAILED_MS. */
static void test_hold_result(void) {
  char item;

  start_at_level(RX, 20U, 0U, 5U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(2000U));
  item = training_item_text()[0];
  /* No effect outside RESULT. */
  training_hold_result(true);
  at(2100U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_add_char(wrong_char_for(item)));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at(3300U));
  at(3400U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_add_char(wrong_char_for(item)));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at(4600U));
  at(4700U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_add_char(wrong_char_for(item)));
  training_hold_result(true);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(20000U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(60000U));
  training_hold_result(true);
  at(61000U);
  training_hold_result(false);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(63499U));
  /* A second false does not restart the delay. */
  at(63000U);
  training_hold_result(false);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, poll_at(63500U));
  /* A false without a hold does not move the delay. */
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(64000U));
  item = training_item_text()[0];
  at(64100U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_add_char(item));
  at(64500U);
  training_hold_result(false);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, poll_at(64700U));
  /* A phase change clears the hold: a start drops it. */
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(65200U));
  at(65300U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_add_char(training_item_text()[0]));
  training_hold_result(true);
  at(100U);
  restart(RX, 20U, 5U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(1100U));
  at(1200U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_add_char(training_item_text()[0]));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, poll_at(1800U));
}

/*
 * Runs a whole session from its start: @p first_correct items correct on the first attempt, the rest correct on the
 * second attempt. Returns the event of the last poll.
 */
static void add_char_checked(char character) {
  (void)training_add_char(character);
  check_selection_invariant();
}

static training_event_t run_session(uint8_t first_correct) {
  uint8_t number = 0U;
  uint16_t time = now;
  training_event_t event = TRAINING_EVENT_NONE;
  unsigned guard = 0U;

  while ((training_phase() != TRAINING_PHASE_END) && (guard < 10000U)) {
    ++guard;
    time = (uint16_t)(time + 3000U);
    event = poll_at(time);
    check_selection_invariant();
    if ((event == TRAINING_EVENT_ITEM) || (event == TRAINING_EVENT_RETRY)) {
      const char* item = training_item_text();
      uint8_t index;

      if (event == TRAINING_EVENT_ITEM) {
        ++number;
      }
      if ((event == TRAINING_EVENT_ITEM) && (number > first_correct)) {
        add_char_checked('*');
        if (training_phase() != TRAINING_PHASE_RESULT) {
          for (index = 1U; index < training_item_length(); ++index) {
            add_char_checked(item[index]);
          }
        }
        continue;
      }
      for (index = 0U; index < training_item_length(); ++index) {
        add_char_checked(item[index]);
      }
    }
  }
  return event;
}

/* AC-T18: threshold - 1 fails and keeps the level; no save. */
static void test_finish_fail(void) {
  start_at_level(RX, 20U, 0U, 5U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_END, run_session(44U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(44U, training_score());
  TEST_ASSERT_EQUAL(50U, training_item_number());
  TEST_ASSERT_EQUAL(50U, training_shown_item_number());
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
  TEST_ASSERT_EQUAL(0U, progress_level(RX, 20U));
  TEST_ASSERT_EQUAL(1U, training_new_shown_level());
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_ERASE));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at((uint16_t)(now + 10000U)));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_IGNORED, training_add_char('K'));
}

/* AC-T18: threshold passes, advances and saves; the last item ends with no GAP. */
static void test_finish_pass(void) {
  start_at_level(RX, 20U, 0U, 5U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_END, run_session(45U));
  TEST_ASSERT_EQUAL(45U, training_score());
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  TEST_ASSERT_EQUAL(1U, progress_level(RX, 20U));
  TEST_ASSERT_EQUAL(1U, training_shown_level());
  TEST_ASSERT_EQUAL(2U, training_new_shown_level());
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));
  fake_iap_clear_counters();
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_load());
  TEST_ASSERT_EQUAL(1U, progress_level(RX, 20U));
  TEST_ASSERT_EQUAL(0U, progress_level(TX, 20U));
  /* The next session starts at the new level. */
  restart(RX, 20U, 5U);
  TEST_ASSERT_EQUAL(1U, training_level());
  TEST_ASSERT_EQUAL(2U, training_shown_level());
}

/* Runs a session at @p level with @p first_correct and checks the outcome and the stored level (D-T33). */
static void check_finish(uint8_t mode, uint16_t level, uint8_t first_correct, bool is_passed) {
  start_at_level(mode, 20U, level, 8U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_END, run_session(first_correct));
  TEST_ASSERT_EQUAL(first_correct, training_score());
  TEST_ASSERT_EQUAL(is_passed ? TRAINING_OUTCOME_PASS : TRAINING_OUTCOME_FAIL, training_outcome());
  TEST_ASSERT_EQUAL(is_passed ? (uint16_t)(level + 1U) : level, progress_level(mode, 20U));
  TEST_ASSERT_EQUAL(is_passed ? 1U : 0U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));
}

/* AC-T18 (D-T33): thresholds 45 (characters), 8 (first word level) and 80 (final level of 100 words). */
static void test_finish_thresholds(void) {
  start_at_level(RX, 20U, 0U, 8U);
  TEST_ASSERT_EQUAL(KOCH_KIND_CHAR, training_kind());
  TEST_ASSERT_EQUAL(45U, training_pass_threshold());
  start_at_level(TX, 20U, 2U, 8U);
  TEST_ASSERT_EQUAL(KOCH_KIND_WORD, training_kind());
  TEST_ASSERT_EQUAL(8U, training_pass_threshold());
  start_at_level(RX, 20U, KOCH_FINAL_LEVEL, 8U);
  TEST_ASSERT_EQUAL(10U, training_item_count());
  TEST_ASSERT_EQUAL(8U, training_pass_threshold());
  start_at_level(TX, 20U, 153U, 8U);
  TEST_ASSERT_EQUAL(100U, training_item_count());
  TEST_ASSERT_EQUAL(80U, training_pass_threshold());
  /* A final level of 50 words has the count of a character session, but the word rule. */
  start_at_level(RX, 20U, (uint16_t)(KOCH_FINAL_LEVEL + 40U), 8U);
  TEST_ASSERT_EQUAL(50U, training_item_count());
  TEST_ASSERT_EQUAL(40U, training_pass_threshold());

  check_finish(RX, 2U, 7U, false);
  check_finish(RX, 2U, 8U, true);
  check_finish(TX, 2U, 7U, false);
  check_finish(TX, 2U, 8U, true);
  check_finish(RX, 153U, 79U, false);
  check_finish(RX, 153U, 80U, true);
}

/* AC-T18: a failed save gives PASS_NOT_SAVED and keeps the new level in RAM. */
static void test_finish_save_failed(void) {
  start_at_level(TX, 25U, 0U, 5U);
  fake_iap_set_fail_operation(1U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_END, run_session(50U));
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_NOT_SAVED, training_outcome());
  TEST_ASSERT_EQUAL(1U, progress_level(TX, 25U));
  TEST_ASSERT_EQUAL(2U, training_new_shown_level());
}

/* AC-T18, D-T5, D-T17: shown levels and the pass at the top. */
static void test_top_levels(void) {
  start_at_level(RX, 50U, 65534U, 8U);
  TEST_ASSERT_EQUAL(65534U, training_level());
  TEST_ASSERT_EQUAL(65535U, training_shown_level());
  TEST_ASSERT_EQUAL(100U, training_item_count());
  TEST_ASSERT_EQUAL(80U, training_pass_threshold());
  TEST_ASSERT_EQUAL(KOCH_KIND_WORD, training_kind());
  TEST_ASSERT_EQUAL(40U, training_alphabet_count());
  TEST_ASSERT_EQUAL(43U, training_learned_count());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_END, run_session(80U));
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  TEST_ASSERT_EQUAL(65535U, progress_level(RX, 50U));
  TEST_ASSERT_EQUAL(65535U, training_new_shown_level());

  start_at_level(RX, 50U, 65535U, 8U);
  TEST_ASSERT_EQUAL(65535U, training_shown_level());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_END, run_session(100U));
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_TOP, training_outcome());
  TEST_ASSERT_EQUAL(65535U, progress_level(RX, 50U));
  TEST_ASSERT_EQUAL(65535U, training_new_shown_level());
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_ERASE));

  /* A fail at the top keeps the level. */
  start_at_level(RX, 50U, 65535U, 8U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_END, run_session(79U));
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
}

/* Word items have the length of the level and Max Word Length. */
static void test_word_length_from_setting(void) {
  start_at_level(TX, 20U, 62U, 6U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(2000U));
  TEST_ASSERT_EQUAL(6U, training_item_length());
  TEST_ASSERT_EQUAL(6U, strlen(training_item_text()));
  start_at_level(TX, 20U, 62U, 200U);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(2000U));
  TEST_ASSERT_EQUAL(8U, training_item_length());
  TEST_ASSERT_EQUAL(20U, training_wpm());
}

/* ---------- Level selection (D-T35, D-T36) ---------- */

/* AC-T30 (1): prepare selects the earned level, reads no EEPROM, and SELECT ignores the session calls. */
static void check_prepare(uint8_t mode, uint16_t level) {
  static const uint16_t ticks[4] = {0U, 1000U, 40000U, 65535U};
  size_t index;

  prepare_at_level(mode, 20U, level, 8U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT_EQUAL(level, training_earned_level());
  TEST_ASSERT_EQUAL(level, training_selected_level());
  TEST_ASSERT_EQUAL(koch_shown_level(level), training_new_shown_level());
  TEST_ASSERT_EQUAL(20U, training_wpm());
  TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
  for (index = 0U; index < (sizeof(ticks) / sizeof(ticks[0])); ++index) {
    TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(ticks[index]));
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_IGNORED, training_add_char('K'));
  TEST_ASSERT(!training_undo_char());
  training_hold_result(false);
  training_hold_result(true);
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(30000U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT_EQUAL(level, training_selected_level());
  TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
}

static void test_prepare(void) {
  static const uint16_t levels[5] = {0U, 1U, 17U, 62U, 65535U};
  size_t index;

  for (index = 0U; index < (sizeof(levels) / sizeof(levels[0])); ++index) {
    check_prepare(RX, levels[index]);
    TEST_ASSERT_EQUAL(RX, training_mode());
    check_prepare(TX, levels[index]);
    TEST_ASSERT_EQUAL(TX, training_mode());
  }
  /* An unknown mode acts as RECEIVING: it reads the Receiving level, not the Sending one. */
  prepare_at_level(TX, 20U, 9U, 8U);
  training_prepare(2U, 20U, 8U);
  TEST_ASSERT_EQUAL(RX, training_mode());
  TEST_ASSERT_EQUAL(0U, training_earned_level());
  TEST_ASSERT_EQUAL(0U, training_selected_level());
  prepare_at_level(RX, 20U, 9U, 8U);
  training_prepare(255U, 20U, 8U);
  TEST_ASSERT_EQUAL(RX, training_mode());
  TEST_ASSERT_EQUAL(9U, training_earned_level());
  TEST_ASSERT_EQUAL(9U, training_selected_level());
  TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
}

/* Steps in @p is_up direction until a step returns false; checks each value. Returns the number of true steps. */
static uint32_t step_to_limit(bool is_up) {
  uint32_t count = 0U;
  uint16_t before = training_selected_level();

  while (training_select_step(is_up)) {
    ++count;
    TEST_ASSERT_EQUAL(is_up ? (uint16_t)(before + 1U) : (uint16_t)(before - 1U), training_selected_level());
    TEST_ASSERT_EQUAL(koch_shown_level(training_selected_level()), training_new_shown_level());
    check_selection_invariant();
    before = training_selected_level();
    if (count > 70000U) {
      break;
    }
  }
  TEST_ASSERT_EQUAL(before, training_selected_level());
  return count;
}

/* AC-T30 (2), (4): the limits 0 and earned, no wrap. */
static void test_select_step_limits(void) {
  prepare_at_level(RX, 20U, 19U, 8U);
  TEST_ASSERT_EQUAL(19U, step_to_limit(false));
  TEST_ASSERT_EQUAL(0U, training_selected_level());
  TEST_ASSERT_EQUAL(1U, training_new_shown_level());
  TEST_ASSERT(!training_select_step(false));
  TEST_ASSERT_EQUAL(0U, training_selected_level());
  TEST_ASSERT_EQUAL(19U, step_to_limit(true));
  TEST_ASSERT_EQUAL(19U, training_selected_level());
  TEST_ASSERT_EQUAL(20U, training_new_shown_level());
  TEST_ASSERT(!training_select_step(true));
  TEST_ASSERT_EQUAL(19U, training_selected_level());

  prepare_at_level(TX, 20U, 0U, 8U);
  TEST_ASSERT(!training_select_step(true));
  TEST_ASSERT(!training_select_step(false));
  TEST_ASSERT_EQUAL(0U, training_selected_level());

  /* D-T5: indexes 65534 and 65535 both show 65535. */
  prepare_at_level(RX, 50U, 65535U, 8U);
  TEST_ASSERT(!training_select_step(true));
  TEST_ASSERT_EQUAL(65535U, training_selected_level());
  TEST_ASSERT_EQUAL(65535U, training_new_shown_level());
  TEST_ASSERT(training_select_step(false));
  TEST_ASSERT_EQUAL(65534U, training_selected_level());
  TEST_ASSERT_EQUAL(65535U, training_new_shown_level());
  TEST_ASSERT(training_select_step(false));
  TEST_ASSERT_EQUAL(65533U, training_selected_level());
  TEST_ASSERT_EQUAL(65534U, training_new_shown_level());
  TEST_ASSERT(training_select_step(true));
  TEST_ASSERT(training_select_step(true));
  TEST_ASSERT(!training_select_step(true));
  TEST_ASSERT_EQUAL(65535U, training_selected_level());
  TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
}

static void check_step_ignored(void) {
  const uint16_t selected = training_selected_level();

  TEST_ASSERT(!training_select_step(true));
  TEST_ASSERT(!training_select_step(false));
  TEST_ASSERT_EQUAL(selected, training_selected_level());
}

/* AC-T30 (2): no step acts in GAP, ITEM, RESULT and IDLE; a step acts in END. */
static void test_select_step_phases(void) {
  prepare_at_level(TX, 20U, 19U, 8U);
  TEST_ASSERT(training_select_step(false));
  TEST_ASSERT(training_select_step(false));
  training_start();
  TEST_ASSERT_EQUAL(17U, training_level());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  check_step_ignored();
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(2000U));
  check_step_ignored();
  at(2100U);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_add_char('*'));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  check_step_ignored();
  training_stop();
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
  check_step_ignored();
  TEST_ASSERT_EQUAL(17U, training_selected_level());

  restart(TX, 20U, 8U);
  TEST_ASSERT_EQUAL(19U, training_level());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_END, run_session(0U));
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
  TEST_ASSERT_EQUAL(19U, training_selected_level());
  TEST_ASSERT(!training_select_step(true));
  TEST_ASSERT(training_select_step(false));
  TEST_ASSERT_EQUAL(18U, training_selected_level());
  TEST_ASSERT(training_select_step(true));
  TEST_ASSERT_EQUAL(19U, training_selected_level());
}

/* AC-T30 (3): a start from SELECT and from END runs at the selected level. */
static void test_start_at_selected_level(void) {
  prepare_at_level(TX, 20U, 19U, 8U);
  while (training_selected_level() > 2U) {
    TEST_ASSERT(training_select_step(false));
  }
  training_start();
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(2U, training_level());
  TEST_ASSERT_EQUAL(3U, training_shown_level());
  TEST_ASSERT_EQUAL(KOCH_KIND_WORD, training_kind());
  TEST_ASSERT_EQUAL(10U, training_item_count());
  TEST_ASSERT_EQUAL(8U, training_pass_threshold());
  TEST_ASSERT_EQUAL(0U, training_item_number());
  TEST_ASSERT_EQUAL(0U, training_score());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at(1999U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at(2000U));
  TEST_ASSERT_EQUAL(2U, training_item_length());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(training_item_text()[0]));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_add_char(training_item_text()[1]));

  TEST_ASSERT_EQUAL(TRAINING_EVENT_END, run_session(0U));
  TEST_ASSERT_EQUAL(1U, training_score());
  TEST_ASSERT_EQUAL(2U, training_selected_level());
  TEST_ASSERT(training_select_step(false));
  TEST_ASSERT(training_select_step(false));
  TEST_ASSERT_EQUAL(0U, training_selected_level());
  at((uint16_t)(now + 50U));
  training_start();
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(0U, training_level());
  TEST_ASSERT_EQUAL(KOCH_KIND_CHAR, training_kind());
  TEST_ASSERT_EQUAL(50U, training_item_count());
  TEST_ASSERT_EQUAL(45U, training_pass_threshold());
  TEST_ASSERT_EQUAL(0U, training_item_number());
  TEST_ASSERT_EQUAL(1U, training_shown_item_number());
  TEST_ASSERT_EQUAL(0U, training_score());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, poll_at((uint16_t)(now + TRAINING_START_MS - 1U)));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at((uint16_t)(now + 1U)));
  TEST_ASSERT_EQUAL(19U, training_earned_level());
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_ERASE));
}

/* Runs one session at the selected level that passes (@p is_passed) or fails by one. */
static void run_selected_session(bool is_passed) {
  training_start();
  check_selection_invariant();
  TEST_ASSERT_EQUAL(TRAINING_EVENT_END,
                    run_session(is_passed ? training_item_count() : (uint8_t)(training_pass_threshold() - 1U)));
  check_selection_invariant();
}

/* Prepares the earned level @p earned at (RX, 20) and steps down to @p level. */
static void prepare_selected(uint16_t earned, uint16_t level) {
  prepare_at_level(RX, 20U, earned, 8U);
  while (training_selected_level() > level) {
    TEST_ASSERT(training_select_step(false));
  }
  TEST_ASSERT_EQUAL(level, training_selected_level());
}

/* AC-T31 (1): a pass at the earned level advances, saves once and preselects the new earned level. */
static void test_finish_pass_at_earned(void) {
  prepare_selected(19U, 19U);
  run_selected_session(true);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));
  TEST_ASSERT_EQUAL(1U, fake_iap_command_count(CMD_ERASE));
  TEST_ASSERT_EQUAL(20U, training_earned_level());
  TEST_ASSERT_EQUAL(20U, training_selected_level());
  TEST_ASSERT_EQUAL(21U, training_new_shown_level());
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_load());
  TEST_ASSERT_EQUAL(20U, progress_level(RX, 20U));
}

/* AC-T31 (2), (6): a pass below the earned level is a replay; the EEPROM gets no erase and no write. */
static void check_replay(uint16_t earned, uint16_t level) {
  static uint8_t record[FAKE_IAP_SIZE];

  prepare_selected(earned, level);
  memcpy(record, fake_iap_memory(), sizeof(record));
  run_selected_session(true);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_ERASE));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT_EQUAL(0, memcmp(record, fake_iap_memory(), sizeof(record)));
  TEST_ASSERT_EQUAL(earned, training_earned_level());
  TEST_ASSERT_EQUAL(earned, progress_level(RX, 20U));
  TEST_ASSERT_EQUAL((uint16_t)(level + 1U), training_selected_level());
  TEST_ASSERT_EQUAL(koch_shown_level((uint16_t)(level + 1U)), training_new_shown_level());
  TEST_ASSERT_EQUAL(earned - level - 1U, step_to_limit(true));
  TEST_ASSERT_EQUAL(earned, training_selected_level());
}

static void test_finish_replay(void) {
  check_replay(19U, 4U);
  check_replay(19U, 18U);
  check_replay(19U, 0U);
  check_replay(65535U, 65534U);
  TEST_ASSERT_EQUAL(65535U, training_new_shown_level());
}

/* AC-T31 (3): a fail keeps the store and preselects the session level. */
static void test_finish_fail_selection(void) {
  static const uint16_t levels[2] = {4U, 19U};
  size_t index;

  for (index = 0U; index < 2U; ++index) {
    prepare_selected(19U, levels[index]);
    run_selected_session(false);
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
    TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_ERASE));
    TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_PROGRAM));
    TEST_ASSERT_EQUAL(19U, training_earned_level());
    TEST_ASSERT_EQUAL(levels[index], training_selected_level());
    TEST_ASSERT_EQUAL((uint16_t)(levels[index] + 1U), training_new_shown_level());
  }
}

/* AC-T31 (4), (5): the top pass and a failed save. */
static void test_finish_top_and_save_failed(void) {
  prepare_selected(65535U, 65535U);
  run_selected_session(true);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_TOP, training_outcome());
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_ERASE));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT_EQUAL(65535U, training_selected_level());
  TEST_ASSERT_EQUAL(65535U, training_earned_level());

  prepare_selected(7U, 7U);
  fake_iap_set_fail_operation(1U);
  run_selected_session(true);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_NOT_SAVED, training_outcome());
  TEST_ASSERT_EQUAL(8U, training_earned_level());
  TEST_ASSERT_EQUAL(8U, training_selected_level());
  TEST_ASSERT_EQUAL(9U, training_new_shown_level());
}

static uint32_t walk_state = 0x2468ACE1UL;

/* Local LCG, so the walk does not change the RNG sequence of the engine items. */
static uint16_t walk_below(uint16_t limit) {
  walk_state = (walk_state * 1103515245UL) + 12345UL;
  return (uint16_t)((uint16_t)(walk_state >> 16U) % limit);
}

/* AC-T31 (7): a random walk keeps 0 <= selected <= earned, and only a pass at the earned level changes the store. */
static void test_selection_random_walk(void) {
  static const uint8_t speeds[3] = {5U, 20U, 50U};
  static uint8_t record[FAKE_IAP_SIZE];
  uint16_t session;
  uint16_t steps;
  uint16_t earned;
  uint16_t level;
  uint8_t mode;
  uint8_t wpm;
  bool is_passed;
  bool is_at_earned;
  uint16_t pass_count = 0U;
  uint16_t replay_count = 0U;

  prepare_at_level(RX, 20U, 0U, 8U);
  for (session = 0U; session < 1200U; ++session) {
    mode = (walk_below(2U) == 0U) ? RX : TX;
    wpm = speeds[walk_below(3U)];
    training_prepare(mode, wpm, (uint8_t)(2U + walk_below(7U)));
    check_selection_invariant();
    earned = training_earned_level();
    TEST_ASSERT_EQUAL(earned, training_selected_level());
    for (steps = walk_below(6U); steps > 0U; --steps) {
      (void)training_select_step(walk_below(3U) == 0U);
      check_selection_invariant();
    }
    level = training_selected_level();
    is_at_earned = (level == earned);
    is_passed = (walk_below(2U) == 0U);
    memcpy(record, fake_iap_memory(), sizeof(record));
    fake_iap_clear_counters();
    run_selected_session(is_passed);
    if (is_passed && is_at_earned) {
      ++pass_count;
      TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
      TEST_ASSERT(memcmp(record, fake_iap_memory(), sizeof(record)) != 0);
      TEST_ASSERT_EQUAL((uint16_t)(earned + 1U), progress_level(mode, wpm));
    } else {
      TEST_ASSERT_EQUAL(0, memcmp(record, fake_iap_memory(), sizeof(record)));
      TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_ERASE) + fake_iap_command_count(CMD_PROGRAM));
      TEST_ASSERT_EQUAL(earned, progress_level(mode, wpm));
      if (is_passed) {
        ++replay_count;
        TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
      }
    }
    TEST_ASSERT_EQUAL(is_passed ? (uint16_t)(level + 1U) : level, training_selected_level());
    for (steps = walk_below(4U); steps > 0U; --steps) {
      (void)training_select_step(walk_below(2U) == 0U);
      check_selection_invariant();
    }
  }
  /* The walk must reach both pass kinds, or it proves little. */
  TEST_ASSERT(pass_count > 50U);
  TEST_ASSERT(replay_count > 50U);
}

int main(void) {
  TEST_RUN(test_flow_receiving);
  TEST_RUN(test_flow_sending_gap);
  TEST_RUN(test_tick_wrap);
  TEST_RUN(test_receiving_word_check);
  TEST_RUN(test_sending_char_check);
  TEST_RUN(test_sending_word_space);
  TEST_RUN(test_ignored_and_undo);
  TEST_RUN(test_hold_result);
  TEST_RUN(test_finish_fail);
  TEST_RUN(test_finish_pass);
  TEST_RUN(test_finish_save_failed);
  TEST_RUN(test_finish_thresholds);
  TEST_RUN(test_top_levels);
  TEST_RUN(test_word_length_from_setting);
  TEST_RUN(test_prepare);
  TEST_RUN(test_select_step_limits);
  TEST_RUN(test_select_step_phases);
  TEST_RUN(test_start_at_selected_level);
  TEST_RUN(test_finish_pass_at_earned);
  TEST_RUN(test_finish_replay);
  TEST_RUN(test_finish_fail_selection);
  TEST_RUN(test_finish_top_and_save_failed);
  TEST_RUN(test_selection_random_walk);
  return TEST_FINISH();
}
