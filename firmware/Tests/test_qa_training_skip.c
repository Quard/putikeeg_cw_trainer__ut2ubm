/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of unlimited attempts and the skip in the engine (references/internal/training_spec.md D-T53 to D-T55,
 * D-T58, sections 5.10, 11.2 and 11.3, AC-T48 to AC-T50, Q8 fault case F5 and mutants M1 to M4, M6, M10, M14). The
 * oracles come from the documents: a wrong answer adds 1 up to 255 and never wraps, unlimited attempts never give
 * FAILED, a skip acts only in ITEM and in RESULT after WRONG, gives no point and uses no attempt, and an IGNORED skip
 * reads no tick and changes nothing. The score is the count of items correct on the first attempt, so a session
 * with retries or skips ends as the 3-attempt session with the same first attempts.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

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
#define WPM 20U
#define MAX_WORD 5U
#define WRONG_CHAR '#' /* never in an item */
#define LEVEL_CHARS 0U
#define LEVEL_WORDS 17U   /* KMRSUAPTLOWI., words of 4 */
#define LEVEL_CHAR_SET 1U /* K, M, R, characters */
#define SEED 0x1357U

static uint16_t now;

static void at(uint16_t time) {
  now = time;
  fake_tick_set_now(now);
}

/* Progress with @p level for @p mode at WPM, the engine prepared in SELECT. */
static void prepare(uint8_t mode, uint16_t level, bool has_unlimited_attempts) {
  fake_iap_reset();
  progress_fixture_write(mode, WPM, level);
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_load());
  fake_iap_clear_counters();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(0U);
  at(1000U);
  rng_seed(SEED);
  training_prepare(mode, WPM, MAX_WORD, has_unlimited_attempts);
}

/* Polls once per ms until @p phase, and returns the last event. */
static training_event_t poll_until(training_phase_t phase) {
  training_event_t event = TRAINING_EVENT_NONE;
  unsigned guard = 0U;
  while ((training_phase() != phase) && (guard < 60000U)) {
    at((uint16_t)(now + 1U));
    event = training_poll();
    ++guard;
  }
  TEST_ASSERT_EQUAL(phase, training_phase());
  return event;
}

static void start_to_item(uint8_t mode, uint16_t level, bool has_unlimited_attempts) {
  prepare(mode, level, has_unlimited_attempts);
  training_start();
  (void)poll_until(TRAINING_PHASE_ITEM);
}

/* One wrong attempt: Sending checks each character, Receiving the full answer. */
static training_check_t answer_wrong(void) {
  training_check_t check = TRAINING_CHECK_PARTIAL;
  if (training_mode() == TX) {
    return training_add_char(WRONG_CHAR);
  }
  while (check == TRAINING_CHECK_PARTIAL) {
    check = training_add_char(WRONG_CHAR);
  }
  return check;
}

static training_check_t answer_right(void) {
  training_check_t check = TRAINING_CHECK_PARTIAL;
  while ((check == TRAINING_CHECK_PARTIAL) && (training_answer_length() < training_item_length())) {
    check = training_add_char(training_item_text()[training_answer_length()]);
  }
  return check;
}

typedef struct {
  training_phase_t phase;
  training_check_t check;
  uint8_t score;
  uint8_t wrong;
  uint8_t answer_length;
  char answer[TRAINING_TEXT_SIZE];
  char item[TRAINING_TEXT_SIZE];
  uint8_t item_number;
  bool is_skipped;
  bool has_unlimited_attempts;
  size_t ticks;
} engine_state_t;

static engine_state_t state_now(void) {
  engine_state_t state;
  memset(&state, 0, sizeof(state));
  state.phase = training_phase();
  state.check = training_last_check();
  state.score = training_score();
  state.wrong = training_wrong_attempts();
  state.answer_length = training_answer_length();
  (void)strncpy(state.answer, training_answer_text(), sizeof(state.answer) - 1U);
  (void)strncpy(state.item, training_item_text(), sizeof(state.item) - 1U);
  state.item_number = training_item_number();
  state.is_skipped = training_is_skipped();
  state.has_unlimited_attempts = training_has_unlimited_attempts();
  state.ticks = fake_tick_read_count();
  return state;
}

static bool is_same_state(const engine_state_t* a, const engine_state_t* b) {
  return (a->phase == b->phase) && (a->check == b->check) && (a->score == b->score) && (a->wrong == b->wrong) &&
         (a->answer_length == b->answer_length) && (strcmp(a->answer, b->answer) == 0) &&
         (strcmp(a->item, b->item) == 0) && (a->item_number == b->item_number) && (a->is_skipped == b->is_skipped) &&
         (a->has_unlimited_attempts == b->has_unlimited_attempts) && (a->ticks == b->ticks);
}

/* M6, M14: an IGNORED skip reads no tick and changes no accessor. */
static void expect_ignored_skip(const char* where) {
  const engine_state_t before = state_now();
  engine_state_t after;
  TEST_ASSERT_EQUAL(TRAINING_CHECK_IGNORED, training_skip());
  after = state_now();
  if (!TEST_ASSERT(is_same_state(&before, &after))) {
    fprintf(stderr, "ignored skip changed state: %s\n", where);
  }
}

/* A skip that acts: FAILED at the tick of the call, no point, no attempt, the failed position is the answer length. */
static void expect_acting_skip(uint8_t answer_length_after) {
  const engine_state_t before = state_now();
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_skip());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(training_is_skipped());
  TEST_ASSERT_EQUAL(before.score, training_score());
  TEST_ASSERT_EQUAL(before.wrong, training_wrong_attempts());
  TEST_ASSERT_EQUAL(answer_length_after, training_answer_length());
  TEST_ASSERT_EQUAL(answer_length_after, (uint8_t)strlen(training_answer_text()));
  TEST_ASSERT_EQUAL(0, strncmp(before.answer, training_answer_text(), answer_length_after));
  TEST_ASSERT(training_answer_length() < training_item_length());
  TEST_ASSERT_EQUAL(before.ticks + 1U, fake_tick_read_count());
  TEST_ASSERT_EQUAL(before.item_number, training_item_number());
}

/* The RESULT of a skip lasts TRAINING_FAILED_MS from the tick of the skip, then GAP (or END on the last item). */
static void expect_failed_ms_from(uint16_t skip_time, training_event_t end_event) {
  at((uint16_t)(skip_time + TRAINING_FAILED_MS - 1U));
  TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, training_poll());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  at((uint16_t)(skip_time + TRAINING_FAILED_MS));
  TEST_ASSERT_EQUAL(end_event, training_poll());
}

/* AC-T48, F5, M1, M2: 300 wrong attempts with unlimited attempts, then a correct answer: no point, no wrap. */
static void test_unlimited_counter_stops_at_255(void) {
  uint8_t mode;
  for (mode = RX; mode <= TX; ++mode) {
    unsigned attempt;
    start_to_item(mode, LEVEL_CHARS, true);
    TEST_ASSERT(training_has_unlimited_attempts());
    for (attempt = 1U; attempt <= 300U; ++attempt) {
      const uint16_t check_time = (uint16_t)(now + 1U);
      at(check_time);
      if (!TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong())) {
        fprintf(stderr, "mode %u attempt %u\n", (unsigned)mode, attempt);
        return;
      }
      TEST_ASSERT_EQUAL((attempt < 255U) ? attempt : 255U, training_wrong_attempts());
      TEST_ASSERT(!training_is_skipped());
      at((uint16_t)(check_time + TRAINING_WRONG_MS - 1U));
      TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, training_poll());
      at((uint16_t)(check_time + TRAINING_WRONG_MS));
      TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, training_poll());
      TEST_ASSERT_EQUAL(0U, training_answer_length());
    }
    TEST_ASSERT_EQUAL(255U, training_wrong_attempts());
    TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, answer_right());
    TEST_ASSERT_EQUAL(0U, training_score());
    TEST_ASSERT_EQUAL(255U, training_wrong_attempts());
  }
}

/* AC-T48 (3), M2: with 3 attempts the third wrong answer gives FAILED; the counter never passes 3. */
static void test_three_attempts_unchanged(void) {
  uint8_t mode;
  for (mode = RX; mode <= TX; ++mode) {
    start_to_item(mode, LEVEL_CHARS, false);
    TEST_ASSERT(!training_has_unlimited_attempts());
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong());
    (void)poll_until(TRAINING_PHASE_ITEM);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong());
    (void)poll_until(TRAINING_PHASE_ITEM);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, answer_wrong());
    TEST_ASSERT_EQUAL(TRAINING_ATTEMPTS, training_wrong_attempts());
    TEST_ASSERT(!training_is_skipped());
    TEST_ASSERT_EQUAL(TRAINING_CHECK_IGNORED, training_add_char(WRONG_CHAR));
    TEST_ASSERT_EQUAL(TRAINING_ATTEMPTS, training_wrong_attempts());
  }
}

/* AC-T48 (4), M10: the value of training_prepare() stays through start, end and reopen, and only prepare sets it. */
static void test_unlimited_value_lives_for_the_visit(void) {
  unsigned value;
  for (value = 0U; value <= 1U; ++value) {
    const bool is_unlimited = (value == 1U);
    prepare(TX, LEVEL_CHARS, is_unlimited);
    TEST_ASSERT_EQUAL(is_unlimited, training_has_unlimited_attempts());
    training_start();
    TEST_ASSERT_EQUAL(is_unlimited, training_has_unlimited_attempts());
    training_stop();
    TEST_ASSERT_EQUAL(is_unlimited, training_has_unlimited_attempts());
    prepare(RX, LEVEL_CHARS, !is_unlimited);
    TEST_ASSERT_EQUAL(!is_unlimited, training_has_unlimited_attempts());
  }
  /* Through a whole session to END and the reopened SELECT. */
  prepare(RX, LEVEL_CHARS, true);
  training_start();
  while (training_phase() != TRAINING_PHASE_END) {
    if (training_phase() == TRAINING_PHASE_ITEM) {
      TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_skip());
    }
    at((uint16_t)(now + 1U));
    (void)training_poll();
  }
  TEST_ASSERT(training_has_unlimited_attempts());
  TEST_ASSERT(training_reopen_select());
  TEST_ASSERT(training_has_unlimited_attempts());
  training_start();
  TEST_ASSERT(training_has_unlimited_attempts());
}

/* AC-T50 (3), M6, M14: the phases where a skip does nothing. */
static void test_skip_ignored_outside_item_and_wrong(void) {
  uint8_t mode;
  for (mode = RX; mode <= TX; ++mode) {
    unsigned setting;
    for (setting = 0U; setting <= 1U; ++setting) {
      prepare(mode, LEVEL_CHARS, setting == 1U);
      training_stop();
      expect_ignored_skip("IDLE");
      prepare(mode, LEVEL_CHARS, setting == 1U);
      expect_ignored_skip("SELECT");
      training_start();
      expect_ignored_skip("GAP at start");
      (void)poll_until(TRAINING_PHASE_ITEM);
      TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, answer_right());
      expect_ignored_skip("RESULT after CORRECT");
      (void)poll_until(TRAINING_PHASE_GAP);
      expect_ignored_skip("GAP between items");
      (void)poll_until(TRAINING_PHASE_ITEM);
      at((uint16_t)(now + 1U));
      expect_acting_skip(0U);
      expect_ignored_skip("RESULT after a skip");
      training_hold_result(true);
      expect_ignored_skip("held RESULT after a skip");
      training_hold_result(false);
      (void)poll_until(TRAINING_PHASE_GAP);
      (void)poll_until(TRAINING_PHASE_ITEM);
      if (setting == 0U) {
        TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong());
        (void)poll_until(TRAINING_PHASE_ITEM);
        TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong());
        (void)poll_until(TRAINING_PHASE_ITEM);
        TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, answer_wrong());
        expect_ignored_skip("RESULT after the third wrong attempt");
        TEST_ASSERT(!training_is_skipped());
      }
      training_stop();
      expect_ignored_skip("IDLE after stop");
    }
  }
}

/* An END state: every item right on the first attempt. */
static void run_session_to_end(void) {
  while (training_phase() != TRAINING_PHASE_END) {
    (void)poll_until(TRAINING_PHASE_ITEM);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, answer_right());
    at((uint16_t)(now + TRAINING_CORRECT_MS));
    (void)training_poll();
  }
}

static void test_skip_ignored_in_end(void) {
  prepare(RX, LEVEL_CHARS, true);
  training_start();
  run_session_to_end();
  expect_ignored_skip("END");
  TEST_ASSERT_EQUAL(training_item_count(), training_score());
}

/* AC-T50 (1), (5): a skip in ITEM with an empty and with a partial answer. */
static void test_skip_in_item(void) {
  uint8_t mode;
  for (mode = RX; mode <= TX; ++mode) {
    unsigned setting;
    for (setting = 0U; setting <= 1U; ++setting) {
      uint16_t skip_time;
      start_to_item(mode, LEVEL_WORDS, setting == 1U);
      TEST_ASSERT(training_item_length() >= 2U);
      skip_time = (uint16_t)(now + 7U);
      at(skip_time);
      expect_acting_skip(0U);
      expect_failed_ms_from(skip_time, TRAINING_EVENT_GAP);
      TEST_ASSERT(training_is_skipped());
      (void)poll_until(TRAINING_PHASE_ITEM);
      TEST_ASSERT(!training_is_skipped()); /* M4: a new item clears the flag */
      TEST_ASSERT_EQUAL(2U, training_item_number());

      TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(training_item_text()[0]));
      skip_time = (uint16_t)(now + 3U);
      at(skip_time);
      expect_acting_skip(1U);
      /* D-T44 hold: held past TRAINING_FAILED_MS; released, it ends at the next poll. */
      training_hold_result(true);
      at((uint16_t)(skip_time + TRAINING_FAILED_MS + 500U));
      TEST_ASSERT_EQUAL(TRAINING_EVENT_NONE, training_poll());
      training_hold_result(false);
      at((uint16_t)(now + 1U));
      TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, training_poll());
    }
  }
}

/* AC-T50 (2), M3, M5: a skip in RESULT after WRONG removes the rejected character and restarts the delay. */
static void test_skip_after_wrong(void) {
  uint8_t mode;
  for (mode = RX; mode <= TX; ++mode) {
    unsigned setting;
    for (setting = 0U; setting <= 1U; ++setting) {
      uint16_t skip_time;
      uint8_t expected_length;
      start_to_item(mode, LEVEL_WORDS, setting == 1U);
      if (mode == TX) {
        /* One accepted character, then a wrong one: the answer holds two characters. */
        TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(training_item_text()[0]));
      }
      at((uint16_t)(now + 1U));
      TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong());
      TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
      expected_length = (uint8_t)(training_answer_length() - 1U);
      skip_time = (uint16_t)(now + TRAINING_WRONG_MS - 1U);
      at(skip_time);
      expect_acting_skip(expected_length);
      TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
      if (mode == TX) {
        TEST_ASSERT_EQUAL(training_item_text()[0], training_answer_text()[0]);
      }
      /* No RETRY at the end of the old WRONG delay; FAILED_MS counts from the skip. */
      expect_failed_ms_from(skip_time, TRAINING_EVENT_GAP);
      TEST_ASSERT_EQUAL(0U, training_score());
    }
  }
}

/* AC-T50 (2): Sending, a skip after a word-space error removes TRAINING_CHAR_WORD_SPACE. */
static void test_skip_after_word_space(void) {
  start_to_item(TX, LEVEL_WORDS, false);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(training_item_text()[0]));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_add_char(training_item_text()[1]));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_add_char(TRAINING_CHAR_WORD_SPACE));
  TEST_ASSERT_EQUAL(3U, training_answer_length());
  expect_acting_skip(2U);
  TEST_ASSERT(strchr(training_answer_text(), TRAINING_CHAR_WORD_SPACE) == NULL);
}

/* M4: training_start() clears the flag of a skip on the last item; a FAILED of add_char never sets it. */
static void test_skip_flag_cleared_by_start(void) {
  prepare(TX, LEVEL_CHARS, false);
  training_start();
  while (training_phase() != TRAINING_PHASE_END) {
    (void)poll_until(TRAINING_PHASE_ITEM);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_skip());
    if (training_item_number() < training_item_count()) {
      (void)poll_until(TRAINING_PHASE_GAP);
    } else {
      TEST_ASSERT_EQUAL(TRAINING_EVENT_END, poll_until(TRAINING_PHASE_END));
    }
  }
  TEST_ASSERT_EQUAL(0U, training_score()); /* M3: no skip gives a point */
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
  TEST_ASSERT(training_is_skipped());
  TEST_ASSERT(training_reopen_select());
  training_start();
  TEST_ASSERT(!training_is_skipped());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_PARTIAL, training_last_check());
  (void)poll_until(TRAINING_PHASE_ITEM);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong());
  (void)poll_until(TRAINING_PHASE_ITEM);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_wrong());
  (void)poll_until(TRAINING_PHASE_ITEM);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, answer_wrong());
  TEST_ASSERT(!training_is_skipped());
}

/* ---------- D-T58, AC-T49: the score and the outcome do not depend on the setting ---------- */

#define HOW_SKIP 0U
#define HOW_RETRY 1U    /* wrong attempts, then a correct one (one wrong with 3 attempts) */
#define HOW_FAIL_OUT 2U /* wrong attempts to the end: FAILED with 3 attempts, then a skip with unlimited */

typedef struct {
  uint8_t score;
  training_outcome_t outcome;
  uint16_t earned;
  uint16_t selected;
  size_t erases;
  size_t programs;
  uint8_t items;
} session_result_t;

/* Ends the current item after a first wrong attempt as @p how says, with @p extra_wrong more wrong attempts. */
static void end_item_without_point(unsigned how, unsigned extra_wrong) {
  unsigned k;
  if (how == HOW_SKIP) {
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_skip());
    return;
  }
  for (k = 0U; k <= extra_wrong; ++k) {
    const training_check_t check = answer_wrong();
    if (check == TRAINING_CHECK_FAILED) {
      return;
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, check);
    (void)poll_until(TRAINING_PHASE_ITEM);
  }
  if (how == HOW_RETRY) {
    TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, answer_right());
  } else {
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_skip());
  }
}

/*
 * A session where the first @p first_ok_count items are correct on the first attempt. Each other item ends as
 * @p how says: 7 wrong attempts with unlimited attempts, 1 (retry) or 3 (fail out) with 3 attempts.
 */
static session_result_t run_scripted(uint8_t mode, uint16_t earned, uint16_t level, bool has_unlimited_attempts,
                                     uint8_t first_ok_count, unsigned how) {
  const unsigned extra_wrong = has_unlimited_attempts ? 6U : ((how == HOW_RETRY) ? 0U : 2U);
  session_result_t result;
  uint8_t item = 0U;
  prepare(mode, earned, has_unlimited_attempts);
  while (training_selected_level() > level) {
    TEST_ASSERT(training_select_step(false));
  }
  training_start();
  while (training_phase() != TRAINING_PHASE_END) {
    if (training_phase() == TRAINING_PHASE_ITEM) {
      ++item;
      if (item <= first_ok_count) {
        TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, answer_right());
      } else {
        end_item_without_point(how, extra_wrong);
      }
      TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    }
    at((uint16_t)(now + 1U));
    (void)training_poll();
  }
  result.score = training_score();
  result.outcome = training_outcome();
  result.earned = training_earned_level();
  result.selected = training_selected_level();
  result.erases = fake_iap_command_count(3U);
  result.programs = fake_iap_command_count(2U);
  result.items = item;
  return result;
}

static void compare_settings(uint8_t mode, uint16_t earned, uint16_t level, uint8_t first_ok_offset,
                             training_outcome_t expected_outcome) {
  unsigned how;
  for (how = HOW_SKIP; how <= HOW_FAIL_OUT; ++how) {
    uint8_t threshold;
    uint8_t first_ok;
    session_result_t three;
    session_result_t unlimited;
    prepare(mode, earned, false);
    while (training_selected_level() > level) {
      TEST_ASSERT(training_select_step(false));
    }
    training_start();
    threshold = training_pass_threshold();
    first_ok = (uint8_t)(threshold - 1U + first_ok_offset);
    three = run_scripted(mode, earned, level, false, first_ok, how);
    unlimited = run_scripted(mode, earned, level, true, first_ok, how);
    if (!TEST_ASSERT_EQUAL(0, memcmp(&three, &unlimited, sizeof(three)))) {
      fprintf(stderr, "mode %u level %u how %u: score %u/%u outcome %u/%u\n", (unsigned)mode, (unsigned)level, how,
              (unsigned)three.score, (unsigned)unlimited.score, (unsigned)three.outcome, (unsigned)unlimited.outcome);
    }
    TEST_ASSERT_EQUAL(first_ok, three.score);
    TEST_ASSERT_EQUAL(expected_outcome, three.outcome);
  }
}

static void test_score_and_outcome_equal_to_three_attempts(void) {
  uint8_t mode;
  TEST_ASSERT_EQUAL(KOCH_KIND_CHAR, koch_kind(LEVEL_CHAR_SET));
  TEST_ASSERT_EQUAL(KOCH_KIND_WORD, koch_kind(LEVEL_WORDS));
  for (mode = RX; mode <= TX; ++mode) {
    compare_settings(mode, LEVEL_CHAR_SET, LEVEL_CHAR_SET, 0U, TRAINING_OUTCOME_FAIL);
    compare_settings(mode, LEVEL_CHAR_SET, LEVEL_CHAR_SET, 1U, TRAINING_OUTCOME_PASS);
    compare_settings(mode, LEVEL_WORDS, LEVEL_WORDS, 0U, TRAINING_OUTCOME_FAIL);
    compare_settings(mode, LEVEL_WORDS, LEVEL_WORDS, 1U, TRAINING_OUTCOME_PASS);
    compare_settings(mode, LEVEL_WORDS, LEVEL_CHAR_SET, 1U, TRAINING_OUTCOME_PASS_REPLAY);
  }
}

int main(void) {
  TEST_RUN(test_unlimited_counter_stops_at_255);
  TEST_RUN(test_three_attempts_unchanged);
  TEST_RUN(test_unlimited_value_lives_for_the_visit);
  TEST_RUN(test_skip_ignored_outside_item_and_wrong);
  TEST_RUN(test_skip_ignored_in_end);
  TEST_RUN(test_skip_in_item);
  TEST_RUN(test_skip_after_wrong);
  TEST_RUN(test_skip_after_word_space);
  TEST_RUN(test_skip_flag_cleared_by_start);
  TEST_RUN(test_score_and_outcome_equal_to_three_attempts);
  return TEST_FINISH();
}
