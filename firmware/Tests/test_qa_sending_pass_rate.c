/*
 * QA scenarios of the word pass rate (references/internal/training_spec.md D-T33, section 6, 11.4, AC-T6, AC-T18,
 * AC-T29; ui-ux.md section 5.15, V-121 and V-136). Receiving and Sending run through the real menu, buttons, engine,
 * keyboard and progress with a fake player, a fake keyer and fake items.
 */

#include "cw/morse.h"
#include "eeprom/eeprom_map.h"
#include "koch/keyboard.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define RX_WPM 32U /* default RX Speed */
#define TX_WPM 20U /* default TX Speed */
#define FINAL_LEVEL 63U /* KOCH_FINAL_LEVEL for 43 characters (section 6) */
#define LEVEL_CHARS 0U
#define LEVEL_FIRST_WORDS 2U
#define LEVEL_FINAL_11 64U  /* 11 words */
#define LEVEL_FINAL_50 103U /* 50 words: the count of a character session */
#define LEVEL_FINAL_100 153U
#define LEVEL_TOP 65535U

/* ---------- Independent oracles from the section 6 table ---------- */

static bool oracle_is_word(uint32_t level) { return (level >= FINAL_LEVEL) || ((level % 3U) == 2U); }

static unsigned oracle_item_count(uint32_t level) {
  if (level >= FINAL_LEVEL) {
    return ((level - FINAL_LEVEL) >= 90U) ? 100U : (unsigned)(10U + (level - FINAL_LEVEL));
  }
  return oracle_is_word(level) ? 10U : 50U;
}

/* Smallest t with t / n >= p / q, by exact comparison of products. */
static unsigned oracle_threshold(bool is_word, unsigned count) {
  const unsigned p = is_word ? 4U : 9U;
  const unsigned q = is_word ? 5U : 10U;
  unsigned t;
  for (t = 0U; t <= count; ++t) {
    if ((t * q) >= (p * count)) {
      return t;
    }
  }
  return count + 1U;
}

/* ---------- koch_pass_threshold() ---------- */

/* AC-T6: both rules for every count, unknown kinds on the character rule, and the edge counts by value. */
static void test_threshold_function(void) {
  static const struct {
    uint8_t count;
    uint8_t word;
    uint8_t chars;
  } edges[] = {{0U, 0U, 0U},    {1U, 1U, 1U},    {2U, 2U, 2U},    {4U, 4U, 4U},     {5U, 4U, 5U},
               {6U, 5U, 6U},    {9U, 8U, 9U},    {10U, 8U, 9U},   {11U, 9U, 10U},   {15U, 12U, 14U},
               {49U, 40U, 45U}, {50U, 40U, 45U}, {51U, 41U, 46U}, {99U, 80U, 90U},  {100U, 80U, 90U},
               {101U, 81U, 91U}, {254U, 204U, 229U}, {255U, 204U, 230U}};
  unsigned count;
  unsigned kind;
  size_t index;

  for (count = 0U; count <= 255U; ++count) {
    TEST_ASSERT_EQUAL(oracle_threshold(true, count), koch_pass_threshold(KOCH_KIND_WORD, (uint8_t)count));
    TEST_ASSERT_EQUAL(oracle_threshold(false, count), koch_pass_threshold(KOCH_KIND_CHAR, (uint8_t)count));
    for (kind = 2U; kind <= 255U; ++kind) {
      if (koch_pass_threshold((koch_kind_t)kind, (uint8_t)count) != oracle_threshold(false, count)) {
        TEST_ASSERT_EQUAL(oracle_threshold(false, count), koch_pass_threshold((koch_kind_t)kind, (uint8_t)count));
        break;
      }
    }
  }
  for (index = 0U; index < (sizeof(edges) / sizeof(edges[0])); ++index) {
    TEST_ASSERT_EQUAL(edges[index].word, koch_pass_threshold(KOCH_KIND_WORD, edges[index].count));
    TEST_ASSERT_EQUAL(edges[index].chars, koch_pass_threshold(KOCH_KIND_CHAR, edges[index].count));
  }
}

/* ---------- Engine: training_start() gives koch_kind() of the session level ---------- */

static void start_engine(uint8_t mode, uint8_t wpm, uint16_t level) {
  fake_iap_reset();
  progress_fixture_write(mode, wpm, level);
  harness_power_on();
  training_start(mode, wpm, 8U);
}

/* AC-T18: the session threshold at every level up to all 100-word levels, the top levels, both modes. */
static void test_engine_threshold_every_level(void) {
  static const uint16_t top_levels[3] = {65533U, 65534U, 65535U};
  uint32_t level;
  size_t index;
  uint8_t mode;

  TEST_ASSERT_EQUAL(FINAL_LEVEL, KOCH_FINAL_LEVEL);
  for (mode = 0U; mode < 2U; ++mode) {
    const uint8_t wpm = (mode == PROGRESS_MODE_SENDING) ? TX_WPM : RX_WPM;
    for (level = 0U; level <= 170U; ++level) {
      start_engine(mode, wpm, (uint16_t)level);
      TEST_ASSERT_EQUAL(level, training_level());
      TEST_ASSERT_EQUAL(oracle_is_word(level) ? KOCH_KIND_WORD : KOCH_KIND_CHAR, training_kind());
      TEST_ASSERT_EQUAL(oracle_item_count(level), training_item_count());
      TEST_ASSERT_EQUAL(oracle_threshold(oracle_is_word(level), oracle_item_count(level)), training_pass_threshold());
      training_stop();
    }
    for (index = 0U; index < 3U; ++index) {
      start_engine(mode, wpm, top_levels[index]);
      TEST_ASSERT_EQUAL(100U, training_item_count());
      TEST_ASSERT_EQUAL(80U, training_pass_threshold());
      training_stop();
    }
  }
}

/* ---------- Sessions through the menu ---------- */

static uint8_t eeprom_before[FAKE_IAP_SIZE];
static unsigned long sessions_run;
static unsigned long items_run;

static void open_mode(uint8_t mode, uint16_t level, const char* item) {
  const uint8_t wpm = (mode == PROGRESS_MODE_SENDING) ? TX_WPM : RX_WPM;

  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(mode, wpm, level);
  }
  harness_power_on();
  fake_koch_item_reset();
  fake_koch_item_push(item);
  memcpy(eeprom_before, fake_iap_memory(), FAKE_IAP_SIZE);
  harness_open_mode((mode == PROGRESS_MODE_SENDING) ? 1U : 0U);
}

static void run_to_phase_end_or_item(void) {
  unsigned guard = 0U;
  while ((training_phase() != TRAINING_PHASE_ITEM) && (training_phase() != TRAINING_PHASE_END) && (guard < 20000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
}

/* Receiving: taps the runs that lead to @p character; checks that the pick reached the engine. */
static void rx_pick(char character) {
  static const button_event_t taps[KEYBOARD_GROUP_COUNT] = {L_TAP, M_TAP, R_TAP, ENTER_EVENT};
  const uint8_t answer_before = training_answer_length();
  const uint8_t attempts_before = training_wrong_attempts();
  bool is_found = false;
  uint8_t target = 0U;
  uint8_t index;
  unsigned guard;

  for (index = 0U; index < keyboard_count(); ++index) {
    if (keyboard_char(index) == character) {
      target = index;
      is_found = true;
    }
  }
  if (!TEST_ASSERT(is_found)) {
    return;
  }
  for (guard = 0U; guard <= KEYBOARD_DEPTH_MAX; ++guard) {
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
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_event(taps[group]);
    if (keyboard_depth() == 0U) {
      break;
    }
  }
  /* The pick reached the engine: one more answer character, or a check that ended the attempt. */
  TEST_ASSERT((training_answer_length() == (uint8_t)(answer_before + 1U)) ||
              (training_phase() != TRAINING_PHASE_ITEM) || (training_wrong_attempts() != attempts_before));
}

static void rx_answer(const char* answer) {
  size_t index;
  fake_player_finish();
  for (index = 0U; answer[index] != '\0'; ++index) {
    rx_pick(answer[index]);
  }
}

/* Sending: the fake keyer gives the elements and the CHAR_GAP of each character. */
static void tx_key(const char* text) {
  size_t index;
  for (index = 0U; text[index] != '\0'; ++index) {
    const uint8_t pattern = morse_encode(text[index]);
    uint8_t shift = 0U;
    while ((uint8_t)(pattern >> shift) > 1U) {
      ++shift;
    }
    while (shift != 0U) {
      --shift;
      fake_keyer_push_event((((pattern >> shift) & 1U) != 0U) ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
    }
    fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
    while (fake_keyer_pending_count() != 0U) {
      harness_set_time((uint16_t)(harness_now + 1U));
      harness_pass();
    }
  }
}

/*
 * One session of the item @p item (K or KM): @p first_correct items right on the first attempt, then items that fail
 * 3 times, and @p late_correct items right on the second attempt at the end. Runs to the end screen.
 */
static void run_session(uint8_t mode, uint16_t level, const char* item, unsigned first_correct, unsigned late_correct) {
  const char* wrong = (strlen(item) == 1U) ? "M" : "MM";
  unsigned count;
  unsigned number;
  unsigned attempt;

  open_mode(mode, level, item);
  count = training_item_count();
  for (number = 1U; number <= count; ++number) {
    const bool is_first = number <= first_correct;
    const bool is_late = !is_first && (number > (count - late_correct));
    const unsigned wrong_count = is_first ? 0U : (is_late ? 1U : TRAINING_ATTEMPTS);
    for (attempt = 0U; attempt < wrong_count; ++attempt) {
      run_to_phase_end_or_item();
      if (mode == PROGRESS_MODE_SENDING) {
        tx_key("N");
      } else {
        rx_answer(wrong);
      }
    }
    if (wrong_count == TRAINING_ATTEMPTS) {
      if (mode == PROGRESS_MODE_RECEIVING) {
        fake_player_finish(); /* the answer playback holds the result (D-T14) */
      }
      continue;
    }
    run_to_phase_end_or_item();
    if (mode == PROGRESS_MODE_SENDING) {
      tx_key(item);
    } else {
      rx_answer(item);
    }
  }
  if (mode == PROGRESS_MODE_RECEIVING) {
    fake_player_finish();
  }
  harness_run_to((uint16_t)(harness_now + TRAINING_FAILED_MS + 10U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(count, training_item_number());
  TEST_ASSERT_EQUAL(first_correct, training_score());
  ++sessions_run;
  items_run += count;
}

static bool is_eeprom_unchanged(void) {
  uint8_t page;
  for (page = 0U; page < FAKE_IAP_PAGE_COUNT; ++page) {
    if (fake_iap_erase_count(page) != 0U) {
      return false;
    }
  }
  return memcmp(eeprom_before, fake_iap_memory(), FAKE_IAP_SIZE) == 0;
}

/* The end screen of ui-ux.md 5.15 (rows 1 to 7), every text centered and inside 25 cells. */
static void expect_end_screen(const char* score, const char* result, const char* level_text) {
  TEST_ASSERT(strlen(result) <= 25U);
  TEST_ASSERT(strlen(level_text) <= 25U);
  TEST_ASSERT(blank_row(1));
  TEST_ASSERT(centered(2, score));
  TEST_ASSERT(centered(3, result));
  TEST_ASSERT(centered(4, level_text));
  TEST_ASSERT(blank_row(5));
  TEST_ASSERT(blank_row(6));
  TEST_ASSERT(next_button());
  TEST_ASSERT(margins_off());
}

/* The level that the EEPROM holds after a power cycle. */
static uint16_t stored_level(uint8_t mode) {
  harness_power_on();
  return progress_level(mode, (mode == PROGRESS_MODE_SENDING) ? TX_WPM : RX_WPM);
}

/* Fail at threshold - 1 and pass at threshold, at @p level, in @p mode. */
static void check_boundary(uint8_t mode, uint16_t level, const char* item, unsigned late_correct) {
  const unsigned count = oracle_item_count(level);
  const unsigned threshold = oracle_threshold(oracle_is_word(level), count);
  char score[32];
  char result[32];
  char level_text[32];

  run_session(mode, level, item, threshold - 1U, late_correct);
  TEST_ASSERT_EQUAL(threshold, training_pass_threshold());
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
  (void)snprintf(score, sizeof(score), "Score %u/%u", threshold - 1U, count);
  (void)snprintf(result, sizeof(result), "Not passed, need %u", threshold);
  (void)snprintf(level_text, sizeof(level_text), "Repeat level L%u", (unsigned)level + 1U);
  expect_end_screen(score, result, level_text);
  TEST_ASSERT(is_eeprom_unchanged());
  TEST_ASSERT_EQUAL(level, stored_level(mode));

  run_session(mode, level, item, threshold, late_correct);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  (void)snprintf(score, sizeof(score), "Score %u/%u", threshold, count);
  (void)snprintf(level_text, sizeof(level_text), "New level L%u", (unsigned)level + 2U);
  expect_end_screen(score, "Passed", level_text);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));
  TEST_ASSERT_EQUAL(1U, fake_iap_program_count(EEPROM_MAP_PROGRESS_ADDRESS));
  TEST_ASSERT_EQUAL(level + 1U, stored_level(mode));
}

/* AC-T29 (2), V-136: 10 words, a final level of 11 words, of 50 words and of 100 words, and 50 characters. */
static void check_mode_boundaries(uint8_t mode) {
  check_boundary(mode, LEVEL_FIRST_WORDS, "KM", 0U);
  /* Items right on the second attempt do not count in the score. */
  check_boundary(mode, LEVEL_FIRST_WORDS, "KM", 2U);
  check_boundary(mode, LEVEL_FINAL_11, "KM", 0U);
  check_boundary(mode, LEVEL_FINAL_50, "KM", 0U);
  check_boundary(mode, LEVEL_CHARS, "K", 1U);
}

static void test_receiving_boundaries(void) { check_mode_boundaries(PROGRESS_MODE_RECEIVING); }

static void test_sending_boundaries(void) {
  check_mode_boundaries(PROGRESS_MODE_SENDING);
  check_boundary(PROGRESS_MODE_SENDING, LEVEL_FINAL_100, "KM", 0U);
}

/* The top level: 79 of 100 repeats L65535, 80 passes with no store (D-T17). The longest texts of row 3 and 4. */
static void test_top_level_texts(void) {
  run_session(PROGRESS_MODE_SENDING, LEVEL_TOP, "KM", 79U, 0U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
  expect_end_screen("Score 79/100", "Not passed, need 80", "Repeat level L65535");
  TEST_ASSERT(text_at(3, 3, "Not passed, need 80"));
  TEST_ASSERT(text_at(4, 3, "Repeat level L65535"));
  TEST_ASSERT(is_eeprom_unchanged());

  run_session(PROGRESS_MODE_SENDING, LEVEL_TOP, "KM", 100U, 0U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_TOP, training_outcome());
  expect_end_screen("Score 100/100", "Passed", "Top level L65535");
  TEST_ASSERT(is_eeprom_unchanged());
}

/* Section 5.15: every reachable fail text fits 25 cells; the threshold has at most 2 digits. */
static void test_fail_text_fits_every_count(void) {
  unsigned count;
  unsigned longest = 0U;
  char text[40];

  for (count = 10U; count <= 100U; ++count) {
    (void)snprintf(text, sizeof(text), "Not passed, need %u", (unsigned)koch_pass_threshold(KOCH_KIND_WORD, (uint8_t)count));
    longest = (strlen(text) > longest) ? (unsigned)strlen(text) : longest;
  }
  (void)snprintf(text, sizeof(text), "Not passed, need %u", (unsigned)koch_pass_threshold(KOCH_KIND_CHAR, 50U));
  longest = (strlen(text) > longest) ? (unsigned)strlen(text) : longest;
  TEST_ASSERT_EQUAL(19U, longest);
  TEST_ASSERT(koch_pass_threshold(KOCH_KIND_WORD, 100U) < 100U);
}

int main(void) {
  TEST_RUN(test_threshold_function);
  TEST_RUN(test_engine_threshold_every_level);
  TEST_RUN(test_fail_text_fits_every_count);
  TEST_RUN(test_receiving_boundaries);
  TEST_RUN(test_sending_boundaries);
  TEST_RUN(test_top_level_texts);
  printf("sessions: %lu, items: %lu\n", sessions_run, items_run);
  TEST_ASSERT_EQUAL(24U, sessions_run);
  return TEST_FINISH();
}
