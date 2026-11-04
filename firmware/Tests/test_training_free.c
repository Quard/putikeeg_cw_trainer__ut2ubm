/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Host tests of free sessions at the session seam (references/internal/training_spec.md sections 23.3, 23.4 and
 * 23.8, AC-T55 (5) and AC-T56). The items come from the real callsign.c and the real RNG.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "callsign/callsign.h"
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
#define FULL KOCH_WORD_ALPHABET_MAX
#define FIRST_DIGIT_COUNT 18U /* '0' is Koch character 17 (section 23.2) */
#define STEP_MS 3000U

static uint16_t now;
static char items[TRAINING_FREE_ITEMS][TRAINING_TEXT_SIZE];

static void at(uint16_t time) {
  now = time;
  fake_tick_set_now(now);
}

static training_event_t poll_at(uint16_t time) {
  at(time);
  return training_poll();
}

/* A progress record with RX level 30 at 20 WPM and TX level 5 at 25 WPM, loaded; the counters are cleared. */
static void load_progress(void) {
  fake_iap_reset();
  progress_fixture_write(RX, 20U, 30U);
  progress_fixture_add(TX, 25U, 5U);
  (void)progress_load();
  fake_iap_clear_counters();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(0U);
  at(1000U);
}

/* Prepares @p mode at 20 WPM, then makes the next sessions free sessions of @p count characters. */
static void prepare_free(uint8_t mode, uint8_t count, uint8_t max_word_len, bool has_unlimited_attempts) {
  training_prepare(mode, 20U, max_word_len, has_unlimited_attempts);
  training_set_free(count);
}

static bool is_digit(char character) { return (character >= '0') && (character <= '9'); }

static bool is_letter(char character) { return (character >= 'A') && (character <= 'Z'); }

/*
 * D-T68 to D-T72: 3 to 6 characters, a NUL after them, only letters and digits of the first @p count Koch characters,
 * a prefix part of 2 or 3 characters that ends with a digit, then 1 to 3 letters.
 */
static bool is_callsign(const char* text, uint8_t length, uint8_t count) {
  const uint8_t limit = (count > FULL) ? FULL : count;
  uint8_t index;
  uint8_t last_digit = 0U;
  bool has_digit = false;

  if ((length < CALLSIGN_LENGTH_MIN) || (length > CALLSIGN_LENGTH_MAX) || (text[length] != '\0')) {
    return false;
  }
  for (index = 0U; index < length; ++index) {
    if (!is_digit(text[index]) && !is_letter(text[index])) {
      return false;
    }
    if (koch_index_of(text[index]) >= limit) {
      return false;
    }
    if (is_digit(text[index])) {
      last_digit = index;
      has_digit = true;
    }
  }
  if (!has_digit || (last_digit < 1U) || (last_digit > 2U)) {
    return false;
  }
  index = (uint8_t)(length - last_digit - 1U);
  return (index >= 1U) && (index <= CALLSIGN_SUFFIX_MAX);
}

/* The keyed or picked answer of the current item: all correct, or a wrong first character. */
static training_check_t answer_item(bool is_correct) {
  const char* item = training_item_text();
  training_check_t check = TRAINING_CHECK_PARTIAL;
  uint8_t index;

  if (!is_correct) {
    check = training_add_char('*');
    if (check != TRAINING_CHECK_PARTIAL) {
      return check;
    }
    for (index = 1U; (index < training_item_length()) && (check == TRAINING_CHECK_PARTIAL); ++index) {
      check = training_add_char(item[index]);
    }
    return check;
  }
  for (index = 0U; index < training_item_length(); ++index) {
    check = training_add_char(item[index]);
  }
  return check;
}

/*
 * Runs a started free session to END: items 1..@p first_correct correct at once, later items wrong once, then
 * correct. Stores each item in items[]. Returns the count of ITEM events.
 */
static uint8_t run_free_session(uint8_t first_correct, uint8_t count) {
  uint8_t number = 0U;
  unsigned guard = 0U;
  training_event_t event;

  while ((training_phase() != TRAINING_PHASE_END) && (guard < 1000U)) {
    ++guard;
    event = poll_at((uint16_t)(now + STEP_MS));
    if (event == TRAINING_EVENT_ITEM) {
      TEST_ASSERT(number < TRAINING_FREE_ITEMS);
      if (number < TRAINING_FREE_ITEMS) {
        if (!TEST_ASSERT(is_callsign(training_item_text(), training_item_length(), count))) {
          fprintf(stderr, "item '%s' count %u\n", training_item_text(), (unsigned)count);
        }
        memcpy(items[number], training_item_text(), TRAINING_TEXT_SIZE);
      }
      ++number;
      (void)answer_item(number <= first_correct);
    } else if (event == TRAINING_EVENT_RETRY) {
      (void)answer_item(true);
    }
  }
  return number;
}

/* AC-T55 (5): the set holds a digit only from count 18 on. */
static void test_is_possible(void) {
  unsigned count;

  for (count = 0U; count < FIRST_DIGIT_COUNT; ++count) {
    if (!TEST_ASSERT(!callsign_is_possible((uint8_t)count))) {
      fprintf(stderr, "count %u\n", count);
    }
  }
  for (count = FIRST_DIGIT_COUNT; count <= FULL; ++count) {
    if (!TEST_ASSERT(callsign_is_possible((uint8_t)count))) {
      fprintf(stderr, "count %u\n", count);
    }
  }
  TEST_ASSERT(callsign_is_possible(41U));
  TEST_ASSERT(callsign_is_possible(KOCH_CHAR_COUNT));
  TEST_ASSERT(callsign_is_possible(255U));
}

/* callsign_is_possible() uses no RNG. */
static void test_is_possible_keeps_rng(void) {
  uint16_t expected;

  rng_seed(0x5A5AU);
  expected = rng_next();
  rng_seed(0x5A5AU);
  (void)callsign_is_possible(FULL);
  (void)callsign_is_possible(10U);
  TEST_ASSERT_EQUAL(expected, rng_next());
}

/* AC-T56 (1): the free state from training_set_free() to the next training_prepare(). */
static void test_free_state(void) {
  uint8_t mode;

  load_progress();
  for (mode = RX; mode <= TX; ++mode) {
    training_prepare(mode, 20U, 5U, false);
    TEST_ASSERT(!training_is_free());
    training_set_free(23U);
    TEST_ASSERT(training_is_free());
    TEST_ASSERT_EQUAL(23U, training_alphabet_count());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
    training_start();
    TEST_ASSERT(training_is_free());
    TEST_ASSERT_EQUAL(23U, training_alphabet_count());
    training_stop();
    TEST_ASSERT(training_is_free());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
    training_start();
    TEST_ASSERT(training_is_free());
    training_prepare(mode, 20U, 5U, false);
    TEST_ASSERT(!training_is_free());
    TEST_ASSERT_EQUAL(koch_alphabet_count(training_selected_level()), training_alphabet_count());
  }
}

/* AC-T56 (2): 20 items, 20 ITEM events, then END; each item is a callsign of the count. */
static void test_twenty_callsigns(void) {
  static const uint8_t counts[] = {18U, 25U, 33U, FULL, KOCH_CHAR_COUNT, 255U};
  uint8_t mode;
  uint8_t index;

  load_progress();
  for (mode = RX; mode <= TX; ++mode) {
    for (index = 0U; index < (uint8_t)sizeof(counts); ++index) {
      rng_seed((uint16_t)(0x1000U + index));
      prepare_free(mode, counts[index], 5U, false);
      training_start();
      TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
      TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, training_item_count());
      TEST_ASSERT_EQUAL(1U, training_shown_item_number());
      TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, run_free_session(TRAINING_FREE_ITEMS, counts[index]));
      TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
      TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, training_item_number());
      TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, training_score());
    }
  }
}

/* AC-T56 (2): Max Word Length does not apply (D-T73). The same seed gives the same items. */
static void test_word_length_ignored(void) {
  char first[TRAINING_FREE_ITEMS][TRAINING_TEXT_SIZE];
  uint8_t mode;

  load_progress();
  for (mode = RX; mode <= TX; ++mode) {
    rng_seed(0x2468U);
    prepare_free(mode, FULL, 2U, false);
    training_start();
    TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, run_free_session(TRAINING_FREE_ITEMS, FULL));
    memcpy(first, items, sizeof(first));
    rng_seed(0x2468U);
    prepare_free(mode, FULL, 8U, false);
    training_start();
    TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, run_free_session(TRAINING_FREE_ITEMS, FULL));
    TEST_ASSERT(memcmp(first, items, sizeof(first)) == 0);
  }
}

/* Many callsigns at every count from 18 to 40: the shape, the set, and all three suffix lengths. */
static void test_callsign_shapes(void) {
  unsigned length_seen[CALLSIGN_LENGTH_MAX + 1U] = {0U};
  bool is_suffix_seen[CALLSIGN_SUFFIX_MAX + 1U] = {false};
  uint8_t count;
  unsigned round;
  uint8_t length;
  uint8_t last;

  load_progress();
  rng_seed(0x7777U);
  for (count = FIRST_DIGIT_COUNT; count <= FULL; ++count) {
    prepare_free(RX, count, 5U, false);
    for (round = 0U; round < 20U; ++round) {
      training_start();
      (void)run_free_session(TRAINING_FREE_ITEMS, count);
      for (length = 0U; length < TRAINING_FREE_ITEMS; ++length) {
        const uint8_t size = (uint8_t)strlen(items[length]);

        ++length_seen[size];
        for (last = size; (last > 0U) && is_letter(items[length][last - 1U]); --last) {
        }
        if ((uint8_t)(size - last) <= CALLSIGN_SUFFIX_MAX) {
          is_suffix_seen[size - last] = true;
        }
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, length_seen[0] + length_seen[1] + length_seen[2]);
  TEST_ASSERT(length_seen[CALLSIGN_LENGTH_MIN] != 0U);
  TEST_ASSERT(length_seen[CALLSIGN_LENGTH_MAX] != 0U);
  TEST_ASSERT(is_suffix_seen[1] && is_suffix_seen[2] && is_suffix_seen[3]);
}

/* With count 18 the only digit is '0', and K is in the set: the first item of some session is K0 + a suffix. */
static void test_learned_set_digit(void) {
  uint8_t index;
  unsigned round;
  bool has_k0 = false;

  load_progress();
  rng_seed(0x0101U);
  prepare_free(TX, FIRST_DIGIT_COUNT, 5U, false);
  for (round = 0U; round < 30U; ++round) {
    training_start();
    (void)run_free_session(TRAINING_FREE_ITEMS, FIRST_DIGIT_COUNT);
    for (index = 0U; index < TRAINING_FREE_ITEMS; ++index) {
      const char* digit = strpbrk(items[index], "0123456789");

      TEST_ASSERT((digit != NULL) && (*digit == '0'));
      if ((items[index][0] == 'K') && (items[index][1] == '0')) {
        has_k0 = true;
      }
    }
  }
  TEST_ASSERT(has_k0);
}

/* Starts a free session of @p mode and polls to the first item. */
static void start_to_first_item(uint8_t mode, bool has_unlimited_attempts) {
  load_progress();
  rng_seed(0x3141U);
  prepare_free(mode, FULL, 5U, has_unlimited_attempts);
  training_start();
  TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at((uint16_t)(now + TRAINING_START_MS)));
}

/* AC-T56 (3): three attempts, unlimited attempts and the skip act as in a Koch word session. */
static void test_checks_attempts_and_skip(void) {
  uint8_t mode;
  uint16_t answer;

  for (mode = RX; mode <= TX; ++mode) {
    start_to_first_item(mode, false);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_item(false));
    TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at((uint16_t)(now + TRAINING_WRONG_MS)));
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_item(false));
    TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at((uint16_t)(now + TRAINING_WRONG_MS)));
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, answer_item(false));
    TEST_ASSERT(!training_is_skipped());
    TEST_ASSERT_EQUAL(0U, training_score());

    start_to_first_item(mode, false);
    TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, answer_item(true));
    TEST_ASSERT_EQUAL(1U, training_score());
    TEST_ASSERT_EQUAL(TRAINING_EVENT_GAP, poll_at((uint16_t)(now + TRAINING_CORRECT_MS)));
    TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at((uint16_t)(now + STEP_MS)));
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_skip());
    TEST_ASSERT(training_is_skipped());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
    TEST_ASSERT_EQUAL(1U, training_score());

    start_to_first_item(mode, true);
    for (answer = 1U; answer <= 300U; ++answer) {
      TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, answer_item(false));
      TEST_ASSERT_EQUAL(TRAINING_EVENT_RETRY, poll_at((uint16_t)(now + TRAINING_WRONG_MS)));
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, answer_item(true));
    TEST_ASSERT_EQUAL(0U, training_score());
    TEST_ASSERT_EQUAL(1U, training_item_number());
    TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, training_item_count());
  }
}

/* AC-T56 (4): every score gives the free outcome; no IAP access and no level change. */
static void test_free_outcome_keeps_progress(void) {
  static const uint8_t scores[] = {0U, 10U, TRAINING_FREE_ITEMS};
  uint8_t record[PROGRESS_RECORD_SIZE];
  uint16_t levels[2][PROGRESS_SPEED_COUNT];
  uint8_t mode;
  uint8_t index;
  uint8_t wpm;
  uint16_t selected;

  load_progress();
  memcpy(record, &fake_iap_memory()[EEPROM_MAP_PROGRESS_ADDRESS], sizeof(record));
  for (mode = RX; mode <= TX; ++mode) {
    for (wpm = PROGRESS_WPM_MIN; wpm <= PROGRESS_WPM_MAX; ++wpm) {
      levels[mode][wpm - PROGRESS_WPM_MIN] = progress_level(mode, wpm);
    }
  }
  for (mode = RX; mode <= TX; ++mode) {
    for (index = 0U; index < (uint8_t)sizeof(scores); ++index) {
      rng_seed((uint16_t)(0x4000U + index));
      prepare_free(mode, FULL, 5U, false);
      selected = training_selected_level();
      training_start();
      TEST_ASSERT_EQUAL(TRAINING_FREE_ITEMS, run_free_session(scores[index], FULL));
      TEST_ASSERT_EQUAL(scores[index], training_score());
      TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FREE, training_outcome());
      TEST_ASSERT_EQUAL(selected, training_selected_level());
    }
    prepare_free(mode, FULL, 5U, false);
    training_start();
    while (training_phase() != TRAINING_PHASE_END) {
      if (poll_at((uint16_t)(now + STEP_MS)) == TRAINING_EVENT_ITEM) {
        TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_skip());
      }
    }
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FREE, training_outcome());
  }
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_ERASE));
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT(memcmp(record, &fake_iap_memory()[EEPROM_MAP_PROGRESS_ADDRESS], sizeof(record)) == 0);
  for (mode = RX; mode <= TX; ++mode) {
    for (wpm = PROGRESS_WPM_MIN; wpm <= PROGRESS_WPM_MAX; ++wpm) {
      TEST_ASSERT_EQUAL(levels[mode][wpm - PROGRESS_WPM_MIN], progress_level(mode, wpm));
    }
  }
}

/* AC-T56 (5): a Koch session after a free session gets Koch items and the Koch item count. */
static void test_koch_after_free(void) {
  uint8_t mode;

  for (mode = RX; mode <= TX; ++mode) {
    load_progress();
    rng_seed(0x5151U);
    prepare_free(mode, FULL, 5U, false);
    training_start();
    (void)run_free_session(TRAINING_FREE_ITEMS, FULL);
    training_prepare(mode, 20U, 5U, false);
    training_start();
    TEST_ASSERT_EQUAL(koch_item_count(training_level()), training_item_count());
    TEST_ASSERT_EQUAL(koch_alphabet_count(training_level()), training_alphabet_count());
    TEST_ASSERT_EQUAL(TRAINING_EVENT_ITEM, poll_at((uint16_t)(now + TRAINING_START_MS)));
    /* RX level 30 is a character level: one character; TX at 20 WPM is level 0: K or M. */
    if (mode == RX) {
      TEST_ASSERT_EQUAL(KOCH_KIND_CHAR, koch_kind(30U));
      TEST_ASSERT_EQUAL(1U, training_item_length());
    } else {
      TEST_ASSERT((training_item_text()[0] == 'K') || (training_item_text()[0] == 'M'));
    }
  }
}

int main(void) {
  TEST_RUN(test_is_possible);
  TEST_RUN(test_is_possible_keeps_rng);
  TEST_RUN(test_free_state);
  TEST_RUN(test_twenty_callsigns);
  TEST_RUN(test_word_length_ignored);
  TEST_RUN(test_callsign_shapes);
  TEST_RUN(test_learned_set_digit);
  TEST_RUN(test_checks_attempts_and_skip);
  TEST_RUN(test_free_outcome_keeps_progress);
  TEST_RUN(test_koch_after_free);
  return TEST_FINISH();
}
