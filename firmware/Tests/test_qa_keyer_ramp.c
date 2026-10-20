/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the keyer early release at the Ramp setting (references/internal/sidetone_spec.md section 24.6
 * check 4, keyer_spec.md section 18, sidetone_spec.md D-RM5). Each call runs keyer_service() and then the real
 * sidetone_service(), as the Timer 0 ISR does. Oracles: the mark schedule of the same paddle stimulus with the
 * keyer alone (the ramp must not move any element), and the call numbers of keyer_spec.md 18.2 for the off
 * request, the ZERO_WAIT entry and the next start from OFF.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "fake_paddle_pins.h"
#include "fake_sidetone_pwm.h"
#include "sidetone/sidetone.h"
#include "sidetone/sidetone_shared.h"
#include "test_assert.h"

static void qa_record_on(void);
static void qa_record_off(void);
#define KEYER_SIDETONE_ON() qa_record_on()
#define KEYER_SIDETONE_OFF() qa_record_off()
#include "cw/keyer_service.h"

#define QA_MAX_CALLS 60000U
#define QA_MAX_MARKS 64U
#define QA_STEPS 32U

static const uint16_t qa_duty[QA_STEPS + 1U] = {0U,   1U,   5U,   11U,  19U,  30U,  42U,  57U,  73U,  91U,  111U,
                                                132U, 154U, 177U, 201U, 225U, 250U, 275U, 299U, 323U, 346U, 368U,
                                                389U, 409U, 427U, 443U, 458U, 470U, 481U, 489U, 495U, 499U, 500U};

typedef struct {
  size_t first;
  size_t last;
  size_t first_off; /* first off request after the mark start */
  size_t zero_entry;
  size_t start_ug;
  int max_index;
  bool is_shape_ok;
} qa_mark_t;

typedef struct {
  uint8_t key_type;
  uint8_t wpm;
  bool is_tip; /* dit lever (no swap) */
  bool is_ring;
  size_t hold_calls;
} qa_stimulus_t;

static bool qa_is_sidetone_run;
static bool qa_is_reference_run;
static size_t qa_call;
static qa_mark_t qa_marks[QA_MAX_MARKS];
static size_t qa_mark_count;
static bool qa_is_in_mark;
static uint8_t qa_reference[QA_MAX_CALLS];
static size_t qa_reference_length;

static void qa_record_on(void) {
  if (qa_is_sidetone_run) {
    sidetone_set_on(true);
  }
}

static void qa_record_off(void) {
  if ((qa_mark_count != 0U) && (qa_marks[qa_mark_count - 1U].first_off == 0U) &&
      (qa_call >= qa_marks[qa_mark_count - 1U].first)) {
    qa_marks[qa_mark_count - 1U].first_off = qa_call;
  }
  if (qa_is_sidetone_run) {
    sidetone_set_on(false);
  }
}

static int qa_index_of_600(uint16_t compare) {
  unsigned index;
  for (index = 0U; index <= QA_STEPS; ++index) {
    if ((uint16_t)((20000UL * qa_duty[index]) / 1000UL) == compare) {
      return (int)index;
    }
  }
  return -2;
}

static void qa_tick(const qa_stimulus_t* stimulus, int* last_index, bool* is_falling) {
  bool is_mark;
  uint8_t state_before;
  size_t i;
  bool is_closed;

  ++qa_call;
  /* Closed from call 20 on: a contact closed at the start call stays masked until it opens. */
  is_closed = (qa_call >= 20U) && (qa_call < (20U + stimulus->hold_calls));
  fake_paddle_pins_set_tip_closed(stimulus->is_tip && is_closed);
  fake_paddle_pins_set_ring_closed(stimulus->is_ring && is_closed);
  fake_sidetone_pwm_clear_log();
  if ((qa_call % 10U) == 0U) {
    fake_sidetone_pwm_update_event(); /* about one carrier period at 400 Hz */
  }
  state_before = sidetone_state();
  keyer_service();
  is_mark = KEYER_IS_MARK_ACTIVE();
  if (is_mark && !qa_is_in_mark && TEST_ASSERT(qa_mark_count < QA_MAX_MARKS)) {
    memset(&qa_marks[qa_mark_count], 0, sizeof(qa_marks[0]));
    qa_marks[qa_mark_count].first = qa_call;
    qa_marks[qa_mark_count].max_index = -1;
    qa_marks[qa_mark_count].is_shape_ok = true;
    /* An off request in the start call belongs to no mark. */
    ++qa_mark_count;
    *last_index = 0;
    *is_falling = false;
  }
  qa_is_in_mark = is_mark;
  if (is_mark) {
    qa_marks[qa_mark_count - 1U].last = qa_call;
  }
  if (!TEST_ASSERT(qa_call < QA_MAX_CALLS)) {
    return;
  }
  if (qa_is_reference_run) {
    qa_reference[qa_call] = is_mark ? 1U : 0U;
    return;
  }
  if (qa_call <= qa_reference_length) {
    if (!TEST_ASSERT_EQUAL(qa_reference[qa_call], is_mark ? 1U : 0U)) {
      fprintf(stderr, "  mark schedule differs at call %zu\n", qa_call);
    }
  }
  sidetone_service();
  for (i = 0U; i < fake_sidetone_pwm_event_count(); ++i) {
    const fake_sidetone_pwm_event_t event = fake_sidetone_pwm_event_at(i);
    qa_mark_t* mark = (qa_mark_count != 0U) ? &qa_marks[qa_mark_count - 1U] : NULL;
    if ((event.kind == FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE) && (mark != NULL)) {
      mark->start_ug = qa_call;
    }
    if ((event.kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER) && (mark != NULL) && (state_before != 0U)) {
      const int index = qa_index_of_600(event.value);
      TEST_ASSERT(index >= 0);
      if (index > mark->max_index) {
        mark->max_index = index;
      }
      if (index < *last_index) {
        *is_falling = true;
      } else if ((index > *last_index) && *is_falling) {
        mark->is_shape_ok = false;
      }
      *last_index = index;
    }
  }
  if ((sidetone_state() == SIDETONE_STATE_ZERO_WAIT) && (state_before != SIDETONE_STATE_ZERO_WAIT) &&
      (qa_mark_count != 0U)) {
    qa_marks[qa_mark_count - 1U].zero_entry = qa_call;
  }
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_violation_count());
}

static void qa_run(const qa_stimulus_t* stimulus, uint8_t ramp_ms, size_t calls, bool is_reference) {
  int last_index = 0;
  bool is_falling = false;
  size_t i;

  fake_paddle_pins_reset();
  fake_sidetone_pwm_reset();
  sidetone_init(600U, ramp_ms);
  keyer_init();
  keyer_start(stimulus->key_type, false, stimulus->wpm, stimulus->wpm);
  qa_is_reference_run = is_reference;
  qa_is_sidetone_run = !is_reference;
  qa_call = 0U;
  qa_mark_count = 0U;
  qa_is_in_mark = false;
  for (i = 0U; i < calls; ++i) {
    qa_tick(stimulus, &last_index, &is_falling);
  }
  keyer_stop();
}

static void qa_case(const qa_stimulus_t* stimulus, uint8_t ramp_ms) {
  const unsigned ramp = 4U * ramp_ms;
  const size_t dit = (4800U + (stimulus->wpm / 2U)) / stimulus->wpm;
  const size_t calls = 20U + stimulus->hold_calls + 10U * dit;
  size_t k;

  memset(qa_reference, 0, sizeof(qa_reference));
  qa_run(stimulus, ramp_ms, calls, true);
  qa_reference_length = qa_call;
  qa_run(stimulus, ramp_ms, calls, false);
  TEST_ASSERT_EQUAL(ramp, sidetone_ramp_calls);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT(qa_mark_count > 0U);
  for (k = 0U; k < qa_mark_count; ++k) {
    const qa_mark_t* mark = &qa_marks[k];
    const size_t length = mark->last - mark->first + 1U;
    bool is_ok = true;
    is_ok &= TEST_ASSERT_EQUAL(mark->first, mark->start_ug);
    is_ok &= TEST_ASSERT(mark->is_shape_ok);
    if (stimulus->key_type == KEYER_KEY_STRAIGHT) {
      /* No early release: the request goes off after the mark, and the fall takes R calls from there. */
      if (length > ramp) {
        is_ok &= TEST_ASSERT(mark->first_off > mark->last);
        is_ok &= TEST_ASSERT_EQUAL(mark->first_off + ramp - 1U, mark->zero_entry);
        is_ok &= TEST_ASSERT_EQUAL(32U, (unsigned)mark->max_index);
      }
    } else {
      const bool is_two_ramps = (length == (2U * ramp));
      is_ok &= TEST_ASSERT((length == dit) || (length == 3U * dit));
      is_ok &= TEST_ASSERT(length >= (2U * ramp));
      is_ok &= TEST_ASSERT_EQUAL(mark->last - ramp + 1U, mark->first_off);
      is_ok &= TEST_ASSERT_EQUAL(mark->last - (is_two_ramps ? 1U : 0U), mark->zero_entry);
      is_ok &= TEST_ASSERT_EQUAL(is_two_ramps ? 31U : 32U, (unsigned)mark->max_index);
    }
    if (!is_ok) {
      fprintf(stderr, "  key %u wpm %u tip %u ring %u ramp %u mark %zu length %zu\n", stimulus->key_type, stimulus->wpm,
              stimulus->is_tip, stimulus->is_ring, ramp_ms, k, length);
    }
  }
}

/* Check 4 for the keyer: every ramp 4..12 ms at 5, 20 and 50 WPM; squeeze, dits, dahs, iambic A and the key. */
static void test_early_release_at_every_ramp(void) {
  static const uint8_t speeds[3] = {5U, 20U, 50U};
  uint8_t ramp;
  for (ramp = 4U; ramp <= 12U; ++ramp) {
    size_t s;
    for (s = 0U; s < 3U; ++s) {
      const uint8_t wpm = speeds[s];
      const size_t dit = (4800U + (wpm / 2U)) / wpm;
      const qa_stimulus_t stimuli[5] = {
          {KEYER_KEY_IAMBIC_B, wpm, true, true, 12U * dit}, {KEYER_KEY_IAMBIC_A, wpm, true, true, 9U * dit},
          {KEYER_KEY_IAMBIC_B, wpm, true, false, 8U * dit}, {KEYER_KEY_IAMBIC_B, wpm, false, true, 10U * dit},
          {KEYER_KEY_STRAIGHT, wpm, true, false, 3U * dit},
      };
      size_t k;
      for (k = 0U; k < 5U; ++k) {
        qa_case(&stimuli[k], ramp);
      }
    }
  }
}

int main(void) {
  TEST_RUN(test_early_release_at_every_ramp);
  return TEST_FINISH();
}
