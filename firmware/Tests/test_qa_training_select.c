/*
 * QA tests of the level selection engine (references/internal/training_spec.md D-T35 to D-T37, sections 5.10 and
 * 11.4, AC-T30, AC-T31, task Q5). The oracles come from the documents: a pass below the earned level never touches
 * the EEPROM or the progress RAM, the preselection is session level + 1 after a pass and the session level after a
 * fail, the steps stop at 0 and at the earned level and act only in SELECT and END.
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
#define WPM 20U
#define OTHER_WPM 25U
#define MAX_WORD 5U
#define STEP_MS 25U
#define TOP 65535U

static uint16_t now;
static uint8_t eeprom_before[FAKE_IAP_SIZE];

static void at(uint16_t time) {
  now = time;
  fake_tick_set_now(now);
}

/* Fresh EEPROM with the earned levels of both modes at WPM and OTHER_WPM, loaded into RAM. */
static void load(uint16_t rx_level, uint16_t tx_level, uint16_t rx_other, uint16_t tx_other) {
  fake_iap_reset();
  progress_fixture_write(RX, WPM, rx_level);
  progress_fixture_add(TX, WPM, tx_level);
  progress_fixture_add(RX, OTHER_WPM, rx_other);
  progress_fixture_add(TX, OTHER_WPM, tx_other);
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_load());
  fake_iap_clear_counters();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(0U);
  rng_seed(0x5A5AU);
  at(1000U);
  memcpy(eeprom_before, fake_iap_memory(), FAKE_IAP_SIZE);
}

static bool is_eeprom_untouched(void) {
  return (fake_iap_command_count(2U) == 0U) && (fake_iap_command_count(3U) == 0U) &&
         (memcmp(eeprom_before, fake_iap_memory(), FAKE_IAP_SIZE) == 0);
}

static void poll_until(training_phase_t phase) {
  unsigned guard = 0U;
  while ((training_phase() != phase) && (guard < 100000U)) {
    at((uint16_t)(now + STEP_MS));
    (void)training_poll();
    ++guard;
  }
  TEST_ASSERT_EQUAL(phase, training_phase());
}

/* Answers the item in phase ITEM: right, or one wrong attempt first. Every step also checks the step rules. */
static void answer_item(bool is_first_wrong) {
  const uint16_t selected = training_selected_level();
  uint8_t index;
  training_check_t check = TRAINING_CHECK_PARTIAL;

  if (is_first_wrong) {
    const char wrong = (training_item_text()[0] == 'K') ? 'M' : 'K';
    check = training_add_char(wrong);
    for (index = 1U; (check == TRAINING_CHECK_PARTIAL) && (index < training_item_length()); ++index) {
      check = training_add_char(training_item_text()[index]);
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, check);
    /* RESULT: no step acts. */
    TEST_ASSERT(!training_select_step(true));
    TEST_ASSERT(!training_select_step(false));
    poll_until(TRAINING_PHASE_ITEM);
  }
  for (index = 0U; index < training_item_length(); ++index) {
    check = training_add_char(training_item_text()[index]);
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, check);
  TEST_ASSERT_EQUAL(selected, training_selected_level());
}

/* Runs the started session to END with @p first_correct first-attempt answers. */
static void run_session(unsigned first_correct) {
  unsigned number;
  const unsigned count = training_item_count();

  for (number = 1U; number <= count; ++number) {
    poll_until(TRAINING_PHASE_ITEM);
    /* ITEM and GAP: no step acts. */
    TEST_ASSERT(!training_select_step(true));
    TEST_ASSERT(!training_select_step(false));
    answer_item(number > first_correct);
  }
  poll_until(TRAINING_PHASE_END);
}

static void pass_session(void) { run_session(training_item_count()); }

static void fail_session(void) { run_session((unsigned)training_pass_threshold() - 1U); }

/* Steps the selection to @p level with single steps; each step must report a change. */
static void select_level(uint16_t level) {
  while (training_selected_level() > level) {
    TEST_ASSERT(training_select_step(false));
  }
  while (training_selected_level() < level) {
    TEST_ASSERT(training_select_step(true));
  }
}

/* AC-T31 (2), D-T35 (4): a pass below the earned level never touches the EEPROM or the RAM levels, in both modes. */
static void test_replay_never_saves(void) {
  static const uint16_t replay_levels[] = {0U, 1U, 2U, 4U, 17U, 18U};
  uint8_t mode;
  size_t index;

  for (mode = RX; mode <= TX; ++mode) {
    for (index = 0U; index < sizeof(replay_levels) / sizeof(replay_levels[0]); ++index) {
      const uint16_t level = replay_levels[index];
      load(19U, 30U, 7U, 8U);
      training_prepare(mode, WPM, MAX_WORD);
      TEST_ASSERT_EQUAL((mode == RX) ? 19U : 30U, training_selected_level());
      select_level(level);
      training_start();
      TEST_ASSERT_EQUAL(level, training_level());
      pass_session();
      TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
      TEST_ASSERT(is_eeprom_untouched());
      TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
      TEST_ASSERT_EQUAL(19U, progress_level(RX, WPM));
      TEST_ASSERT_EQUAL(30U, progress_level(TX, WPM));
      TEST_ASSERT_EQUAL(7U, progress_level(RX, OTHER_WPM));
      TEST_ASSERT_EQUAL(8U, progress_level(TX, OTHER_WPM));
      /* The plan example: L5 passed with L20 earned preselects L6, and the selector still reaches L20. */
      TEST_ASSERT_EQUAL(level + 1U, training_selected_level());
      TEST_ASSERT_EQUAL(level + 2U, training_new_shown_level());
      select_level((mode == RX) ? 19U : 30U);
      TEST_ASSERT(!training_select_step(true));
    }
  }
}

/*
 * Mutation guard for the finish test (Q5: '<' in place of '!='). The earned level drops below the session level
 * during the session (fault injection: a progress reset that the UI cannot do then). The pass must not save.
 */
static void test_pass_above_earned_never_saves(void) {
  load(5U, 0U, 0U, 0U);
  training_prepare(RX, WPM, MAX_WORD);
  training_start();
  TEST_ASSERT_EQUAL(5U, training_level());
  poll_until(TRAINING_PHASE_ITEM);
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_reset());
  fake_iap_clear_counters();
  memcpy(eeprom_before, fake_iap_memory(), FAKE_IAP_SIZE);
  pass_session();
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
  TEST_ASSERT(is_eeprom_untouched());
  TEST_ASSERT_EQUAL(0U, progress_level(RX, WPM));
}

/* AC-T31 (1), (9): a pass at the earned level saves once and changes only the slot of its mode and speed. */
static void test_pass_at_earned_saves_own_slot(void) {
  uint8_t mode;

  for (mode = RX; mode <= TX; ++mode) {
    load(19U, 30U, 7U, 8U);
    training_prepare(mode, WPM, MAX_WORD);
    training_start();
    pass_session();
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
    TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));
    TEST_ASSERT_EQUAL(1U, fake_iap_program_count(EEPROM_MAP_PROGRESS_ADDRESS));
    TEST_ASSERT_EQUAL((mode == RX) ? 20U : 19U, progress_level(RX, WPM));
    TEST_ASSERT_EQUAL((mode == TX) ? 31U : 30U, progress_level(TX, WPM));
    TEST_ASSERT_EQUAL(7U, progress_level(RX, OTHER_WPM));
    TEST_ASSERT_EQUAL(8U, progress_level(TX, OTHER_WPM));
    TEST_ASSERT_EQUAL(training_earned_level(), training_selected_level());
    TEST_ASSERT(!training_select_step(true));
    /* The stored record holds the new level: a reload gives the same RAM values. */
    TEST_ASSERT_EQUAL(PROGRESS_OK, progress_load());
    TEST_ASSERT_EQUAL((mode == RX) ? 20U : 19U, progress_level(RX, WPM));
    TEST_ASSERT_EQUAL((mode == TX) ? 31U : 30U, progress_level(TX, WPM));
  }
}

/* AC-T31 (3), D-T35 (5): a fail keeps the selection at the session level, at and below the earned level. */
static void test_fail_preselects_session_level(void) {
  static const uint16_t levels[] = {0U, 4U, 19U};
  size_t index;

  for (index = 0U; index < sizeof(levels) / sizeof(levels[0]); ++index) {
    load(19U, 0U, 0U, 0U);
    training_prepare(RX, WPM, MAX_WORD);
    select_level(levels[index]);
    training_start();
    fail_session();
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
    TEST_ASSERT_EQUAL(levels[index], training_selected_level());
    TEST_ASSERT(is_eeprom_untouched());
    TEST_ASSERT_EQUAL(19U, progress_level(RX, WPM));
  }
}

/* Fault injection (F-16): every failed IAP operation of the save gives PASS_NOT_SAVED with the new level in RAM. */
static void test_save_failure_at_each_operation(void) {
  size_t operation;

  for (operation = 1U; operation <= 8U; ++operation) {
    load(3U, 0U, 0U, 0U);
    training_prepare(RX, WPM, MAX_WORD);
    training_start();
    poll_until(TRAINING_PHASE_ITEM);
    fake_iap_set_fail_operation(operation);
    pass_session();
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_NOT_SAVED, training_outcome());
    TEST_ASSERT_EQUAL(4U, progress_level(RX, WPM));
    TEST_ASSERT_EQUAL(4U, training_earned_level());
    TEST_ASSERT_EQUAL(4U, training_selected_level());
    /* A replay of the old level after the failure still never touches the EEPROM. */
    fake_iap_set_fail_operation(0U);
    fake_iap_clear_counters();
    TEST_ASSERT(training_select_step(false));
    training_start();
    pass_session();
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
    TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
    TEST_ASSERT_EQUAL(4U, training_selected_level());
    /* The next pass at the RAM level saves again. */
    training_start();
    pass_session();
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
    TEST_ASSERT_EQUAL(5U, progress_level(RX, WPM));
    TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));
  }
}

/* AC-T30 (2): earned 0, and the D-T5 saturation at 65534 and 65535 (A-L5, V-148). */
static void test_limits_zero_and_top(void) {
  load(0U, TOP, 0U, 0U);
  training_prepare(RX, WPM, MAX_WORD);
  TEST_ASSERT(!training_select_step(true));
  TEST_ASSERT(!training_select_step(false));
  TEST_ASSERT_EQUAL(0U, training_selected_level());
  TEST_ASSERT_EQUAL(1U, training_new_shown_level());

  training_prepare(TX, WPM, MAX_WORD);
  TEST_ASSERT_EQUAL(TOP, training_selected_level());
  TEST_ASSERT(!training_select_step(true));
  TEST_ASSERT_EQUAL(TOP, training_selected_level());
  TEST_ASSERT(training_select_step(false));
  TEST_ASSERT_EQUAL(65534U, training_selected_level());
  TEST_ASSERT_EQUAL(TOP, training_new_shown_level());
  training_start();
  pass_session();
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
  TEST_ASSERT_EQUAL(TOP, training_selected_level());
  TEST_ASSERT(is_eeprom_untouched());
  TEST_ASSERT(!training_select_step(true));

  training_start();
  TEST_ASSERT_EQUAL(TOP, training_level());
  pass_session();
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_TOP, training_outcome());
  TEST_ASSERT_EQUAL(TOP, training_selected_level());
  TEST_ASSERT_EQUAL(TOP, training_new_shown_level());
  TEST_ASSERT(is_eeprom_untouched());
  TEST_ASSERT_EQUAL(TOP, progress_level(TX, WPM));

  training_start();
  fail_session();
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
  TEST_ASSERT_EQUAL(TOP, training_selected_level());
  TEST_ASSERT(is_eeprom_untouched());
}

/* A pass at the earned index 65534 advances to 65535 and saves; the selection then stops at 65535. */
static void test_pass_at_65534(void) {
  load(65534U, 0U, 0U, 0U);
  training_prepare(RX, WPM, MAX_WORD);
  training_start();
  pass_session();
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  TEST_ASSERT_EQUAL(TOP, progress_level(RX, WPM));
  TEST_ASSERT_EQUAL(TOP, training_selected_level());
  TEST_ASSERT(!training_select_step(true));
}

/* A new prepare (speed change, other mode, re-entry) always selects the earned level of its own key. */
static void test_prepare_resets_selection(void) {
  load(19U, 30U, 7U, 8U);
  training_prepare(RX, WPM, MAX_WORD);
  select_level(3U);
  training_prepare(RX, OTHER_WPM, MAX_WORD);
  TEST_ASSERT_EQUAL(7U, training_selected_level());
  TEST_ASSERT_EQUAL(7U, training_earned_level());
  TEST_ASSERT(!training_select_step(true));
  training_prepare(TX, OTHER_WPM, MAX_WORD);
  TEST_ASSERT_EQUAL(8U, training_selected_level());
  training_prepare(TX, WPM, MAX_WORD);
  TEST_ASSERT_EQUAL(30U, training_selected_level());
  training_prepare(RX, WPM, MAX_WORD);
  TEST_ASSERT_EQUAL(19U, training_selected_level());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT(is_eeprom_untouched());
}

/* Steps in END change the next session level; after a stop (IDLE) no step acts. */
static void test_end_steps_and_idle(void) {
  load(19U, 0U, 0U, 0U);
  training_prepare(RX, WPM, MAX_WORD);
  select_level(8U);
  training_start();
  fail_session();
  TEST_ASSERT_EQUAL(8U, training_selected_level());
  TEST_ASSERT(training_select_step(true));
  TEST_ASSERT(training_select_step(true));
  TEST_ASSERT_EQUAL(10U, training_selected_level());
  /* The session accessors keep the session that ended. */
  TEST_ASSERT_EQUAL(8U, training_level());
  training_start();
  TEST_ASSERT_EQUAL(10U, training_level());
  TEST_ASSERT_EQUAL(koch_item_count(10U), training_item_count());
  training_stop();
  TEST_ASSERT(!training_select_step(true));
  TEST_ASSERT(!training_select_step(false));
  TEST_ASSERT_EQUAL(10U, training_selected_level());
}

int main(void) {
  TEST_RUN(test_replay_never_saves);
  TEST_RUN(test_pass_above_earned_never_saves);
  TEST_RUN(test_pass_at_earned_saves_own_slot);
  TEST_RUN(test_fail_preselects_session_level);
  TEST_RUN(test_save_failure_at_each_operation);
  TEST_RUN(test_limits_zero_and_top);
  TEST_RUN(test_pass_at_65534);
  TEST_RUN(test_prepare_resets_selection);
  TEST_RUN(test_end_steps_and_idle);
  return TEST_FINISH();
}
