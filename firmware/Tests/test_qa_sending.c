/*
 * QA scenarios of the Sending mode (references/internal/training_spec.md section 12.3, D-T15, D-T16; ui-ux.md
 * sections 5.13 and 5.14). The real menu, buttons, engine and progress run with a fake keyer and fake items.
 */

#include "cw/morse.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define LEVEL_KM 0U
#define LEVEL_WORDS_13 17U /* KMRSUAPTLOWI., words of 4 */

static void tx_open(uint16_t level, const char* const* items, size_t count) {
  size_t index;

  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_SENDING, 20U, level);
  }
  harness_power_on();
  (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, SETTINGS_PROSIGN_BOTH);
  (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, 1U);
  fake_koch_item_reset();
  for (index = 0U; index < count; ++index) {
    fake_koch_item_push(items[index]);
  }
  harness_open_mode(1U);
}

static void tx_open_one(uint16_t level, const char* item) {
  const char* const items[1] = {item};
  tx_open(level, items, 1U);
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

/* KEYP(p): the elements of @p pattern, then one CHAR_GAP. */
static void keyp(const char* pattern) {
  size_t index;
  for (index = 0U; pattern[index] != '\0'; ++index) {
    push((pattern[index] == '-') ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
  push(KEYER_EVENT_CHAR_GAP);
}

static void run_to_item(void) {
  unsigned guard = 0U;
  while ((training_phase() != TRAINING_PHASE_ITEM) && (guard < 20000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
}

/* D-T16: in phase ITEM, OVERFLOW drops the rest of the character up to its CHAR_GAP. No check, no attempt. */
static void test_overflow_drops_rest_of_character(void) {
  tx_open_one(LEVEL_KM, "K");
  run_to_item();
  push(KEYER_EVENT_DAH);
  push(KEYER_EVENT_OVERFLOW);
  /* The keyer gives the elements after the lost events; they belong to the dropped character. */
  push(KEYER_EVENT_DIT);
  push(KEYER_EVENT_DAH);
  push(KEYER_EVENT_CHAR_GAP);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT(status(NULL));
  keyp("-.-");
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  TEST_ASSERT_EQUAL(1U, training_score());
  TEST_ASSERT(status("Correct"));
}

/*
 * Section 12.3 table: outside phase ITEM with the drop flag clear, only DIT and DAH set the flag. OVERFLOW, CHAR_GAP
 * and WORD_GAP in a delay are dropped alone, so the first character of the next item counts.
 */
static void test_non_element_events_in_delays_set_no_drop(void) {
  static const char* const items[2] = {"K", "K"};

  tx_open(LEVEL_KM, items, 2U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  push(KEYER_EVENT_OVERFLOW);
  push(KEYER_EVENT_CHAR_GAP);
  push(KEYER_EVENT_WORD_GAP);
  run_to_item();
  keyp("-.-");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  TEST_ASSERT_EQUAL(1U, training_score());

  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  push(KEYER_EVENT_OVERFLOW);
  run_to_item();
  TEST_ASSERT_EQUAL(2U, training_item_number());
  keyp("-.-");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  TEST_ASSERT_EQUAL(2U, training_score());
}

/*
 * Section 12.3 FAILED: row 5 shows the code of the expected character at the failed position of a word, for the
 * third wrong attempt. Item KMRS, keyed K then N: the expected character is M ("--").
 */
static void test_word_failed_shows_code_of_failed_position(void) {
  unsigned attempt;

  tx_open_one(LEVEL_WORDS_13, "KMRS");
  for (attempt = 0U; attempt < 3U; ++attempt) {
    run_to_item();
    keyp("-.-"); /* K */
    TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
    keyp("-."); /* N for M */
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(status("Wrong: N"));
  /* Two dashes: w = 6 + 3 + 6 = 15, x0 = (128 - 15) / 2 = 56. */
  TEST_ASSERT(marks_are(5, 56, "--"));
}

/* Section 12.3 entry: buttons_set_enter_enabled(false). P3.2 gives no menu enter while a session runs. */
static void test_no_enter_event_during_session(void) {
  unsigned ms;
  unsigned enter_events = 0U;

  tx_open_one(LEVEL_KM, "K");
  run_to_item();
  fake_button_pins_set(BUTTON_BIT_ENTER);
  for (ms = 0U; ms < 200U; ++ms) {
    button_event_t event;
    harness_set_time((uint16_t)(harness_now + 1U));
    fake_tick_set_now(harness_now);
    event = buttons_poll();
    if (BUTTON_EVENT_CODE(event) == BUTTON_EVENT_ENTER) {
      ++enter_events;
    }
  }
  fake_button_pins_set(0U);
  TEST_ASSERT_EQUAL(0U, enter_events);
}

int main(void) {
  TEST_RUN(test_overflow_drops_rest_of_character);
  TEST_RUN(test_non_element_events_in_delays_set_no_drop);
  TEST_RUN(test_word_failed_shows_code_of_failed_position);
  TEST_RUN(test_no_enter_event_during_session);
  return TEST_FINISH();
}
