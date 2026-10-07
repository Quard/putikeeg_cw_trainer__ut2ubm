#include "modes/training.h"

#include "koch/koch.h"
#include "mcu/compiler.h"
#include "progress/progress.h"
#include "tick/tick.h"

#if (TRAINING_MODE_RECEIVING != PROGRESS_MODE_RECEIVING) || (TRAINING_MODE_SENDING != PROGRESS_MODE_SENDING)
#error "The training modes must equal the progress modes"
#endif

/* Session (training_spec.md section 11.1). */
static MEM_XDATA training_phase_t training_phase_value;
static MEM_XDATA uint8_t training_mode_value;
static MEM_XDATA uint8_t training_wpm_value;
static MEM_XDATA uint8_t training_max_word_len;
static MEM_XDATA uint16_t training_level_value;
/* Counters. */
static MEM_XDATA uint8_t training_item_number_value;
static MEM_XDATA uint8_t training_item_count_value;
static MEM_XDATA uint8_t training_score_value;
static MEM_XDATA uint8_t training_threshold;
static MEM_XDATA uint8_t training_wrong;
static MEM_XDATA training_check_t training_check;
static MEM_XDATA training_outcome_t training_outcome_value;
/* Time. */
static MEM_XDATA uint16_t training_now;
static MEM_XDATA uint16_t training_phase_start;
static MEM_XDATA uint16_t training_duration;
static MEM_XDATA bool training_is_held;
/* Texts. */
static MEM_XDATA char training_item[TRAINING_TEXT_SIZE];
static MEM_XDATA char training_answer[TRAINING_TEXT_SIZE];
static MEM_XDATA uint8_t training_item_len;
static MEM_XDATA uint8_t training_answer_len;

/* Each phase change starts its delay at the tick that training_now holds, and clears the hold. */
static void training_set_phase(training_phase_t phase, MEM_XDATA uint16_t duration) {
  training_phase_value = phase;
  training_duration = duration;
  training_phase_start = training_now;
  training_is_held = false;
}

static void training_clear_answer(void) {
  training_answer_len = 0U;
  training_answer[0] = '\0';
}

/* D-T5: level index + 1, saturated at 65535. */
static uint16_t training_shown_of(uint16_t level) { return (level == KOCH_LEVEL_MAX) ? level : (uint16_t)(level + 1U); }

void training_start(uint8_t mode, MEM_XDATA uint8_t wpm, MEM_XDATA uint8_t max_word_len) {
  training_mode_value = (mode == TRAINING_MODE_SENDING) ? TRAINING_MODE_SENDING : TRAINING_MODE_RECEIVING;
  training_wpm_value = wpm;
  training_max_word_len = max_word_len;
  training_level_value = progress_level(training_mode_value, wpm);
  training_item_count_value = koch_item_count(training_level_value);
  training_threshold = koch_pass_threshold(koch_kind(training_level_value), training_item_count_value);
  training_item_number_value = 0U;
  training_score_value = 0U;
  training_wrong = 0U;
  training_check = TRAINING_CHECK_PARTIAL;
  training_outcome_value = TRAINING_OUTCOME_FAIL;
  training_item_len = 0U;
  training_item[0] = '\0';
  training_clear_answer();
  training_now = tick_now();
  training_set_phase(TRAINING_PHASE_GAP, TRAINING_START_MS);
}

void training_stop(void) { training_phase_value = TRAINING_PHASE_IDLE; }

/* Section 11.4. */
static void training_finish(void) {
  if (training_score_value < training_threshold) {
    training_outcome_value = TRAINING_OUTCOME_FAIL;
  } else if (!progress_advance(training_mode_value, training_wpm_value)) {
    training_outcome_value = TRAINING_OUTCOME_PASS_TOP;
  } else if (progress_save() == PROGRESS_OK) {
    training_outcome_value = TRAINING_OUTCOME_PASS;
  } else {
    training_outcome_value = TRAINING_OUTCOME_PASS_NOT_SAVED;
  }
}

training_event_t training_poll(void) {
  if ((training_phase_value != TRAINING_PHASE_GAP) && (training_phase_value != TRAINING_PHASE_RESULT)) {
    return TRAINING_EVENT_NONE;
  }
  training_now = tick_now();
  if (training_is_held || ((uint16_t)(training_now - training_phase_start) < training_duration)) {
    return TRAINING_EVENT_NONE;
  }
  if (training_phase_value == TRAINING_PHASE_GAP) {
    ++training_item_number_value;
    training_wrong = 0U;
    training_item_len = koch_make_item(training_level_value, training_max_word_len, training_item);
    training_clear_answer();
    training_set_phase(TRAINING_PHASE_ITEM, 0U);
    return TRAINING_EVENT_ITEM;
  }
  if (training_check == TRAINING_CHECK_WRONG) {
    training_clear_answer();
    training_set_phase(TRAINING_PHASE_ITEM, 0U);
    return TRAINING_EVENT_RETRY;
  }
  if (training_item_number_value < training_item_count_value) {
    training_set_phase(TRAINING_PHASE_GAP, (training_mode_value == TRAINING_MODE_SENDING) ? TRAINING_SENDING_GAP_MS
                                                                                          : TRAINING_RECEIVING_GAP_MS);
    return TRAINING_EVENT_GAP;
  }
  training_finish();
  training_set_phase(TRAINING_PHASE_END, 0U);
  return TRAINING_EVENT_END;
}

/* Sending checks each character at once. Receiving checks the whole answer, only when it is complete. */
static bool training_is_answer_wrong(void) {
  uint8_t index = 0U;

  if (training_mode_value == TRAINING_MODE_SENDING) {
    index = (uint8_t)(training_answer_len - 1U);
  } else if (training_answer_len < training_item_len) {
    return false;
  }
  for (; index < training_answer_len; ++index) {
    if (training_answer[index] != training_item[index]) {
      return true;
    }
  }
  return false;
}

training_check_t training_add_char(char character) {
  uint16_t duration;

  if (training_phase_value != TRAINING_PHASE_ITEM) {
    return TRAINING_CHECK_IGNORED;
  }
  training_answer[training_answer_len] = character;
  ++training_answer_len;
  training_answer[training_answer_len] = '\0';
  if (training_is_answer_wrong()) {
    ++training_wrong;
    if (training_wrong >= TRAINING_ATTEMPTS) {
      training_check = TRAINING_CHECK_FAILED;
      duration = TRAINING_FAILED_MS;
    } else {
      training_check = TRAINING_CHECK_WRONG;
      duration = TRAINING_WRONG_MS;
    }
  } else if (training_answer_len < training_item_len) {
    return TRAINING_CHECK_PARTIAL;
  } else {
    if (training_wrong == 0U) {
      ++training_score_value;
    }
    training_check = TRAINING_CHECK_CORRECT;
    duration = TRAINING_CORRECT_MS;
  }
  training_now = tick_now();
  training_set_phase(TRAINING_PHASE_RESULT, duration);
  return training_check;
}

bool training_undo_char(void) {
  if ((training_phase_value != TRAINING_PHASE_ITEM) || (training_answer_len == 0U)) {
    return false;
  }
  --training_answer_len;
  training_answer[training_answer_len] = '\0';
  return true;
}

void training_hold_result(bool is_held) {
  if (training_phase_value != TRAINING_PHASE_RESULT) {
    return;
  }
  if (is_held) {
    training_is_held = true;
  } else if (training_is_held) {
    training_is_held = false;
    training_phase_start = tick_now();
  }
}

training_phase_t training_phase(void) { return training_phase_value; }

uint8_t training_mode(void) { return training_mode_value; }

uint8_t training_wpm(void) { return training_wpm_value; }

koch_kind_t training_kind(void) { return koch_kind(training_level_value); }

uint16_t training_level(void) { return training_level_value; }

uint16_t training_shown_level(void) { return training_shown_of(training_level_value); }

uint16_t training_new_shown_level(void) {
  return training_shown_of(progress_level(training_mode_value, training_wpm_value));
}

uint8_t training_learned_count(void) { return koch_learned_count(training_level_value); }

uint8_t training_alphabet_count(void) { return koch_alphabet_count(training_level_value); }

uint8_t training_item_number(void) { return training_item_number_value; }

uint8_t training_shown_item_number(void) {
  return (training_phase_value == TRAINING_PHASE_GAP) ? (uint8_t)(training_item_number_value + 1U)
                                                      : training_item_number_value;
}

uint8_t training_item_count(void) { return training_item_count_value; }

uint8_t training_score(void) { return training_score_value; }

uint8_t training_pass_threshold(void) { return training_threshold; }

uint8_t training_wrong_attempts(void) { return training_wrong; }

training_check_t training_last_check(void) { return training_check; }

training_outcome_t training_outcome(void) { return training_outcome_value; }

const char MEM_XDATA* training_item_text(void) { return training_item; }

uint8_t training_item_length(void) { return training_item_len; }

const char MEM_XDATA* training_answer_text(void) { return training_answer; }

uint8_t training_answer_length(void) { return training_answer_len; }
