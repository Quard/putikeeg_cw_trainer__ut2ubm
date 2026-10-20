/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the CW player early release at the Ramp setting (references/internal/sidetone_spec.md section 24.6
 * check 4, training_spec.md D-T51 and AC-T12, sidetone_spec.md D-RM5). Each call runs player_service() and then the
 * real sidetone_service(), as the Timer 0 ISR does. Oracles: the mark schedule of a run with no sidetone (the ramp
 * must not move any element), and the call numbers of D-RM5 for the off request, the ZERO_WAIT entry and the next
 * start from OFF.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "fake_sidetone_pwm.h"
#include "sidetone/sidetone.h"
#include "sidetone/sidetone_shared.h"
#include "test_assert.h"

static void qa_record_on(void);
static void qa_record_off(void);
#define PLAYER_SIDETONE_ON() qa_record_on()
#define PLAYER_SIDETONE_OFF() qa_record_off()
#include "cw/player.h"
#include "cw/player_service.h"

#define QA_MAX_CALLS 120000U
#define QA_MAX_MARKS 64U
#define QA_STEPS 32U

static const uint16_t qa_duty[QA_STEPS + 1U] = {0U,   1U,   5U,   11U,  19U,  30U,  42U,  57U,  73U,  91U,  111U,
                                                132U, 154U, 177U, 201U, 225U, 250U, 275U, 299U, 323U, 346U, 368U,
                                                389U, 409U, 427U, 443U, 458U, 470U, 481U, 489U, 495U, 499U, 500U};

static bool qa_is_sidetone_run;
static size_t qa_call;
static bool qa_was_off_requested_this_mark;

typedef struct {
  size_t first;      /* first mark call */
  size_t last;       /* last mark call */
  size_t first_off;  /* first off request */
  size_t zero_entry; /* call that entered ZERO_WAIT */
  size_t start_ug;   /* call with the start UG */
  int max_index;
  bool is_shape_ok; /* indexes go up, then only down */
} qa_mark_t;

static qa_mark_t qa_marks[QA_MAX_MARKS];
static size_t qa_mark_count;
static uint8_t qa_reference[QA_MAX_CALLS]; /* 1: mark call of the run without sidetone */
static size_t qa_reference_length;
static bool qa_is_reference_run;

static void qa_record_on(void) {
  qa_was_off_requested_this_mark = false;
  if (qa_is_sidetone_run) {
    sidetone_set_on(true);
  }
}

static void qa_record_off(void) {
  if ((qa_mark_count != 0U) && !qa_was_off_requested_this_mark && player_is_mark) {
    qa_marks[qa_mark_count - 1U].first_off = qa_call;
    qa_was_off_requested_this_mark = true;
  }
  if (qa_is_sidetone_run) {
    sidetone_set_on(false);
  }
}

static int qa_index_of_1000(uint16_t compare) {
  unsigned index;
  for (index = 0U; index <= QA_STEPS; ++index) {
    if ((uint16_t)(12U * qa_duty[index]) == compare) {
      return (int)index;
    }
  }
  return -2;
}

static bool qa_start(const char* text, uint8_t wpm, uint8_t eff_wpm) {
  char buffer[PLAYER_TEXT_MAX + 2U];
  const size_t length = strlen(text);
  memset(buffer, 0, sizeof(buffer));
  memcpy(buffer, text, length + 1U);
  return player_start(buffer, (uint8_t)length, wpm, eff_wpm);
}

/* One Timer 0 call; returns false when the run is over. */
static bool qa_tick(size_t update_every, int* last_index, bool* is_falling) {
  bool was_mark;
  uint8_t state_before;
  size_t i;

  ++qa_call;
  fake_sidetone_pwm_clear_log();
  if ((update_every != 0U) && ((qa_call % update_every) == 0U)) {
    fake_sidetone_pwm_update_event();
  }
  state_before = sidetone_state();
  player_service();
  was_mark = player_is_enabled && player_is_mark;
  if (was_mark && ((qa_mark_count == 0U) || (qa_marks[qa_mark_count - 1U].last + 1U != qa_call))) {
    if (TEST_ASSERT(qa_mark_count < QA_MAX_MARKS)) {
      qa_mark_t* mark = &qa_marks[qa_mark_count];
      memset(mark, 0, sizeof(*mark));
      mark->first = qa_call;
      mark->max_index = -1;
      mark->is_shape_ok = true;
      ++qa_mark_count;
      *last_index = 0;
      *is_falling = false;
    }
  }
  if (was_mark) {
    qa_marks[qa_mark_count - 1U].last = qa_call;
  }
  if (qa_is_reference_run) {
    if (TEST_ASSERT(qa_call < QA_MAX_CALLS)) {
      qa_reference[qa_call] = was_mark ? 1U : 0U;
    }
    return player_is_busy();
  }
  if (TEST_ASSERT(qa_call < QA_MAX_CALLS) && (qa_call <= qa_reference_length)) {
    if (!TEST_ASSERT_EQUAL(qa_reference[qa_call], was_mark ? 1U : 0U)) {
      fprintf(stderr, "  mark schedule differs at call %zu\n", qa_call);
    }
  }
  if (was_mark) {
    /* The busy flag holds the full mark, also after the early release. */
    TEST_ASSERT(player_is_busy());
  }
  sidetone_service();
  for (i = 0U; i < fake_sidetone_pwm_event_count(); ++i) {
    const fake_sidetone_pwm_event_t event = fake_sidetone_pwm_event_at(i);
    qa_mark_t* mark = (qa_mark_count != 0U) ? &qa_marks[qa_mark_count - 1U] : NULL;
    if ((event.kind == FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE) && (mark != NULL)) {
      mark->start_ug = qa_call;
    }
    if ((event.kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER) && (mark != NULL) && (state_before != 0U)) {
      const int index = qa_index_of_1000(event.value);
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
  return player_is_busy() || (sidetone_state() != SIDETONE_STATE_OFF);
}

static void qa_case(const char* text, uint8_t wpm, uint8_t eff_wpm, uint8_t ramp_ms, size_t update_every) {
  const unsigned ramp = 4U * ramp_ms;
  int last_index = 0;
  bool is_falling = false;
  size_t k;

  /* Reference: the player alone. */
  fake_sidetone_pwm_reset();
  sidetone_init(1000U, ramp_ms);
  qa_is_sidetone_run = false;
  qa_is_reference_run = true;
  qa_call = 0U;
  qa_mark_count = 0U;
  memset(qa_reference, 0, sizeof(qa_reference));
  TEST_ASSERT(qa_start(text, wpm, eff_wpm));
  while (qa_tick(0U, &last_index, &is_falling) && (qa_call < QA_MAX_CALLS)) {
  }
  qa_reference_length = qa_call;
  player_stop();

  /* The run with the real sidetone at the ramp under test. */
  fake_sidetone_pwm_reset();
  sidetone_init(1000U, ramp_ms);
  TEST_ASSERT_EQUAL(ramp, sidetone_ramp_calls);
  qa_is_sidetone_run = true;
  qa_is_reference_run = false;
  qa_call = 0U;
  qa_mark_count = 0U;
  TEST_ASSERT(qa_start(text, wpm, eff_wpm));
  while (qa_tick(update_every, &last_index, &is_falling) && (qa_call < QA_MAX_CALLS)) {
  }
  TEST_ASSERT(qa_call >= qa_reference_length);
  TEST_ASSERT(!player_is_busy());
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT(qa_mark_count > 0U);

  for (k = 0U; k < qa_mark_count; ++k) {
    const qa_mark_t* mark = &qa_marks[k];
    const size_t length = mark->last - mark->first + 1U;
    const bool is_two_ramps = (length == (2U * ramp));
    bool is_ok = true;
    is_ok &= TEST_ASSERT(length >= (2U * ramp));
    /* The tone starts from OFF on the first mark call: the previous ZERO_WAIT has ended. */
    is_ok &= TEST_ASSERT_EQUAL(mark->first, mark->start_ug);
    /* The request goes off on the call where R mark calls remain. */
    is_ok &= TEST_ASSERT_EQUAL(mark->last - ramp + 1U, mark->first_off);
    /* D-RM5: ZERO_WAIT on the last mark call, or one call earlier only for a mark of exactly two ramps. */
    is_ok &= TEST_ASSERT_EQUAL(mark->last - (is_two_ramps ? 1U : 0U), mark->zero_entry);
    is_ok &= TEST_ASSERT_EQUAL(is_two_ramps ? 31U : 32U, (unsigned)mark->max_index);
    is_ok &= TEST_ASSERT(mark->is_shape_ok);
    if (!is_ok) {
      fprintf(stderr, "  text %s wpm %u/%u ramp %u update %zu mark %zu length %zu\n", text, wpm, eff_wpm, ramp_ms,
              update_every, k, length);
    }
  }
}

/* Check 4 for the player: every ramp 4..12 ms, 5, 20 and 50 WPM, Farnsworth 50/5 and 20/10. */
static void test_early_release_at_every_ramp(void) {
  static const char* const texts[] = {"PARIS", "50", "E", "\x01"};
  static const uint8_t speeds[][2] = {{5U, 5U}, {20U, 20U}, {50U, 50U}, {50U, 5U}, {20U, 10U}};
  static const size_t updates[] = {0U, 4U, 10U};
  uint8_t ramp;
  for (ramp = 4U; ramp <= 12U; ++ramp) {
    size_t speed;
    for (speed = 0U; speed < sizeof(speeds) / sizeof(speeds[0]); ++speed) {
      size_t text;
      for (text = 0U; text < sizeof(texts) / sizeof(texts[0]); ++text) {
        size_t update;
        for (update = 0U; update < sizeof(updates) / sizeof(updates[0]); ++update) {
          if ((speeds[speed][0] == 5U) && (update != 1U)) {
            continue; /* the 5 WPM cases are long; one update rate is enough there */
          }
          qa_case(texts[text], speeds[speed][0], speeds[speed][1], ramp, updates[update]);
        }
      }
    }
  }
}

int main(void) {
  TEST_RUN(test_early_release_at_every_ramp);
  return TEST_FINISH();
}
