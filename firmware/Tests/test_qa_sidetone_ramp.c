/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the sidetone ramp setting (references/internal/sidetone_spec.md section 24.6 checks 1 to 3, AC-3,
 * AC-4, AC-7, AC-15, AC-25 to AC-29). Two oracles:
 * - direct checks of the compare log against the call counts and the table of section 6.8;
 * - an independent model of the position table of section 6.3 that predicts every register event of every call, in
 *   lockstep, also when the tone writer or the ramp writer is preempted at each of its preemption points.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "board/sidetone_config.h"
#include "fake_sidetone_pwm.h"
#include "fake_tick.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"
#include "sidetone/sidetone_shared.h"
#include "support/test_assert.h"

#define QA_STEPS 32U
#define QA_TABLE_SIZE (QA_STEPS + 1U)
#define QA_FULL 2048U
#define QA_COUNTER_HZ 12000000UL
#define QA_TONE_POINTS 36U /* step 2, 33 table stores, the ARR store, step 5 (section 6.4) */
#define QA_RAMP_POINTS 2U  /* after the increment store, after the call count store (section 6.8) */
#define QA_TORN 0U         /* tone of a table that the writer has not finished */
#define QA_SENTINEL 0xBEEFU
#define QA_MAX_EVENTS 8U
#define QA_LOG_CLEAR_LEVEL 3000U
#define QA_MAX_PRINTED 40U
#define QA_ZERO_WAIT_LIMIT 12U

static const uint16_t qa_duty[QA_TABLE_SIZE] = {0U,   1U,   5U,   11U,  19U,  30U,  42U,  57U,  73U,  91U,  111U,
                                                132U, 154U, 177U, 201U, 225U, 250U, 275U, 299U, 323U, 346U, 368U,
                                                389U, 409U, 427U, 443U, 458U, 470U, 481U, 489U, 495U, 499U, 500U};

/* ---------- Failure reporting ---------- */

static const char* qa_context = "";
static unsigned long qa_context_a;
static unsigned long qa_context_b;
static unsigned long qa_printed;

static bool qa_check(bool is_passed, const char* expression, int line) {
  if (is_passed) {
    ++test_check_count;
    return true;
  }
  if (qa_printed < QA_MAX_PRINTED) {
    ++qa_printed;
    fprintf(stderr, "  context: %s %lu %lu\n", qa_context, qa_context_a, qa_context_b);
    (void)test_check(false, expression, __FILE__, line);
  } else {
    ++test_check_count;
    ++test_failure_count;
  }
  return false;
}

#define QA_CHECK(condition) qa_check((condition), #condition, __LINE__)

static void qa_set_context(const char* text, unsigned long a, unsigned long b) {
  qa_context = text;
  qa_context_a = a;
  qa_context_b = b;
}

/* ---------- Spec math (sections 6.1 and 6.8) ---------- */

static uint8_t qa_clamp_ms(unsigned ms) {
  if (ms < 4U) {
    return 4U;
  }
  return (uint8_t)((ms > 12U) ? 12U : ms);
}

static uint16_t qa_clamp_hz(uint16_t hz) {
  if (hz < 400U) {
    return 400U;
  }
  return (hz > 1000U) ? 1000U : hz;
}

static unsigned qa_increment(uint8_t ms) { return (512U + ms - 1U) / ms; }

static uint32_t qa_period(uint16_t hz) { return (QA_COUNTER_HZ + (uint32_t)hz / 2U) / hz; }

static uint16_t qa_arr(uint16_t hz) { return (hz == QA_TORN) ? QA_SENTINEL : (uint16_t)(qa_period(hz) - 1U); }

static uint16_t qa_compare(uint16_t hz, unsigned index) {
  if (hz == QA_TORN) {
    return QA_SENTINEL;
  }
  return (uint16_t)((qa_period(hz) * (uint32_t)qa_duty[index]) / 1000UL);
}

/* Table index of a 1000 Hz compare value (12 x permille), or -1. */
static int qa_index_of_1000(uint16_t compare) {
  unsigned index;
  for (index = 0U; index < QA_TABLE_SIZE; ++index) {
    if ((uint16_t)(12U * qa_duty[index]) == compare) {
      return (int)index;
    }
  }
  return -1;
}

/* ---------- Model of section 6.3 with the position of D-RM1 ---------- */

typedef struct {
  uint16_t hz[2]; /* tone of table A and B, or QA_TORN */
  uint16_t requested_hz;
  uint8_t state;
  unsigned position;
  unsigned increment;
  unsigned calls;
  bool is_on;
  bool is_pending;
  bool is_b_active;
  bool is_headphones;
  bool is_flag;
} qa_model_t;

static qa_model_t model;

typedef struct {
  fake_sidetone_pwm_event_t events[QA_MAX_EVENTS];
  size_t count;
} qa_expected_t;

static void qa_emit(qa_expected_t* expected, uint8_t kind, uint16_t value) {
  if (QA_CHECK(expected->count < QA_MAX_EVENTS)) {
    expected->events[expected->count].kind = kind;
    expected->events[expected->count].value = value;
    ++expected->count;
  }
}

static uint8_t model_compare_kind(void) {
  return model.is_headphones ? FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES : FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER;
}

static void model_write(qa_expected_t* expected) {
  qa_emit(expected, model_compare_kind(), qa_compare(model.hz[model.is_b_active ? 1U : 0U], model.position / 64U));
}

static void model_service(qa_expected_t* expected) {
  const unsigned increment = model.increment;
  bool is_up;

  expected->count = 0U;
  if (model.state == SIDETONE_STATE_OFF) {
    if (!model.is_on) {
      return;
    }
    if (model.is_pending) {
      model.is_b_active = !model.is_b_active;
      model.is_pending = false;
      qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_SET_ARR, qa_arr(model.hz[model.is_b_active ? 1U : 0U]));
    }
    qa_emit(expected, model_compare_kind(), 0U);
    qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE, 0U);
    model.is_flag = true;
    qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_SET_ENO,
            model.is_headphones ? FAKE_SIDETONE_PWM_ENO_HEADPHONES : FAKE_SIDETONE_PWM_ENO_SPEAKER);
    model.position = 0U;
    model.state = SIDETONE_STATE_RISING;
    return;
  }
  if (model.state == SIDETONE_STATE_ZERO_WAIT) {
    qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_READ_UPDATE_FLAG, model.is_flag ? 1U : 0U);
    if (model.is_flag || ((model.position + 1U) >= QA_ZERO_WAIT_LIMIT)) {
      qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_SET_ENO, FAKE_SIDETONE_PWM_ENO_OFF);
      model.position = 0U;
      model.state = SIDETONE_STATE_OFF;
    } else {
      ++model.position;
    }
    return;
  }
  is_up = model.is_on && !model.is_pending;
  if (is_up) {
    if (model.position == QA_FULL) {
      model.state = SIDETONE_STATE_STEADY;
      return;
    }
    model.position += increment;
    if (model.position >= QA_FULL) {
      model.position = QA_FULL;
    }
    model_write(expected);
    model.state = (model.position == QA_FULL) ? SIDETONE_STATE_STEADY : SIDETONE_STATE_RISING;
    return;
  }
  if (model.position == 0U) {
    (void)QA_CHECK(model.state == SIDETONE_STATE_RISING);
    qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_CLEAR_UPDATE_FLAG, 0U);
    model.is_flag = false;
    model.state = SIDETONE_STATE_ZERO_WAIT;
    return;
  }
  if (model.position > increment) {
    model.position -= increment;
    model_write(expected);
    model.state = SIDETONE_STATE_FALLING;
    return;
  }
  model.position = 0U;
  model_write(expected);
  qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_CLEAR_UPDATE_FLAG, 0U);
  model.is_flag = false;
  model.state = SIDETONE_STATE_ZERO_WAIT;
}

/* ---------- Lockstep driver ---------- */

static size_t log_consumed;
static unsigned long service_count;
/* Index of the last compare write of the last call, or -1 when the call wrote none. */
static int last_write_index;

static bool qa_is_compare(uint8_t kind) {
  return (kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER) || (kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES);
}

/* Events outside a service call: only preemption points and test update events. */
static void qa_absorb_foreign(void) {
  const size_t count = fake_sidetone_pwm_event_count();
  for (; log_consumed < count; ++log_consumed) {
    const uint8_t kind = fake_sidetone_pwm_event_at(log_consumed).kind;
    (void)QA_CHECK((kind == FAKE_SIDETONE_PWM_EVENT_PREEMPTION_POINT) ||
                   (kind == FAKE_SIDETONE_PWM_EVENT_UPDATE_EVENT));
  }
}

static void qa_service(void) {
  qa_expected_t expected;
  size_t start;
  size_t count;
  size_t i;

  qa_absorb_foreign();
  if (fake_sidetone_pwm_event_count() > QA_LOG_CLEAR_LEVEL) {
    fake_sidetone_pwm_clear_log();
    log_consumed = 0U;
  }
  start = fake_sidetone_pwm_event_count();
  model_service(&expected);
  sidetone_service();
  ++service_count;
  count = fake_sidetone_pwm_event_count() - start;
  last_write_index = -1;
  (void)QA_CHECK(count == expected.count);
  for (i = 0U; i < count; ++i) {
    const fake_sidetone_pwm_event_t actual = fake_sidetone_pwm_event_at(start + i);
    if (i < expected.count) {
      if (!QA_CHECK((actual.kind == expected.events[i].kind) && (actual.value == expected.events[i].value))) {
        fprintf(stderr, "  call %lu event %zu: kind %u value %u, expected kind %u value %u\n", service_count, i,
                (unsigned)actual.kind, (unsigned)actual.value, (unsigned)expected.events[i].kind,
                (unsigned)expected.events[i].value);
      }
    }
    if (qa_is_compare(actual.kind) && (model.hz[model.is_b_active ? 1U : 0U] == 1000U)) {
      last_write_index = qa_index_of_1000(actual.value);
    }
  }
  log_consumed = start + count;
  (void)QA_CHECK(sidetone_state() == model.state);
  (void)QA_CHECK(sidetone_is_busy() == (model.is_on || (model.state != SIDETONE_STATE_OFF)));
  (void)QA_CHECK(fake_sidetone_pwm_is_update_flag_set() == model.is_flag);
  (void)QA_CHECK(fake_sidetone_pwm_violation_count() == 0U);
}

static void qa_services(size_t count) {
  size_t i;
  for (i = 0U; i < count; ++i) {
    qa_service();
  }
}

static void qa_on(bool is_on) {
  sidetone_set_on(is_on);
  model.is_on = is_on;
}

static void qa_update_event(void) {
  fake_sidetone_pwm_update_event();
  model.is_flag = true;
}

/* ---------- Writers in lockstep with their preemption points ---------- */

typedef enum { WRITER_NONE, WRITER_TONE, WRITER_RAMP } qa_writer_kind_t;

static struct {
  qa_writer_kind_t kind;
  unsigned point;
  unsigned target;
  uint16_t new_hz;
  unsigned new_increment;
  unsigned new_calls;
  unsigned old_calls;
  /* Service calls to run at each point, and whether an update event comes first. */
  const uint8_t* services;
  const bool* updates;
} writer;

static void qa_writer_hook(void) {
  const unsigned point = writer.point;

  if (writer.kind == WRITER_TONE) {
    if (!QA_CHECK(point < QA_TONE_POINTS)) {
      return;
    }
    if (point == 0U) {
      model.is_pending = false;
      writer.target = model.is_b_active ? 0U : 1U;
      model.hz[writer.target] = QA_TORN;
    } else if (point == (QA_TONE_POINTS - 1U)) {
      model.hz[writer.target] = writer.new_hz;
      model.is_pending = true;
    }
  } else if (writer.kind == WRITER_RAMP) {
    if (!QA_CHECK(point < QA_RAMP_POINTS)) {
      return;
    }
    /* Section 6.8: the increment first, then the call count. */
    if (point == 0U) {
      model.increment = writer.new_increment;
      (void)QA_CHECK(sidetone_ramp_calls == writer.old_calls);
    } else {
      model.calls = writer.new_calls;
      (void)QA_CHECK(sidetone_ramp_calls == writer.new_calls);
    }
  } else {
    (void)QA_CHECK(false);
    return;
  }
  ++writer.point;
  if (writer.services != NULL) {
    if ((writer.updates != NULL) && writer.updates[point]) {
      qa_update_event();
    }
    qa_services(writer.services[point]);
  }
}

static void qa_set_tone(uint16_t hz, const uint8_t* services, const bool* updates) {
  const uint16_t clamped = qa_clamp_hz(hz);
  const size_t points = fake_sidetone_pwm_preemption_count();

  qa_absorb_foreign();
  writer.kind = WRITER_TONE;
  writer.point = 0U;
  writer.new_hz = clamped;
  writer.services = services;
  writer.updates = updates;
  sidetone_set_tone_hz(hz);
  writer.kind = WRITER_NONE;
  if (clamped == model.requested_hz) {
    (void)QA_CHECK(fake_sidetone_pwm_preemption_count() == points);
  } else {
    (void)QA_CHECK(writer.point == QA_TONE_POINTS);
    (void)QA_CHECK(fake_sidetone_pwm_preemption_count() == (points + QA_TONE_POINTS));
    model.requested_hz = clamped;
  }
  qa_absorb_foreign();
}

static void qa_set_ramp(unsigned ms, const uint8_t* services, const bool* updates) {
  const uint8_t clamped = qa_clamp_ms(ms);
  const size_t points = fake_sidetone_pwm_preemption_count();

  qa_absorb_foreign();
  writer.kind = WRITER_RAMP;
  writer.point = 0U;
  writer.new_increment = qa_increment(clamped);
  writer.new_calls = 4U * clamped;
  writer.old_calls = model.calls;
  writer.services = services;
  writer.updates = updates;
  sidetone_set_ramp_ms((uint8_t)ms);
  writer.kind = WRITER_NONE;
  (void)QA_CHECK(writer.point == QA_RAMP_POINTS);
  (void)QA_CHECK(fake_sidetone_pwm_preemption_count() == (points + QA_RAMP_POINTS));
  (void)QA_CHECK(sidetone_ramp_calls == model.calls);
  qa_absorb_foreign();
}

/* Preemption points of sidetone_init() at the first-design ramp of 8 ms. */
static size_t init_points_of_8_ms(void) {
  static size_t points;
  static bool is_known;
  if (!is_known) {
    fake_sidetone_pwm_reset();
    sidetone_init(600U, 8U);
    points = fake_sidetone_pwm_preemption_count();
    is_known = true;
  }
  return points;
}

static void qa_setup(uint16_t hz, unsigned ms, bool is_headphones) {
  (void)init_points_of_8_ms();
  const uint8_t clamped = qa_clamp_ms(ms);

  fake_sidetone_pwm_reset();
  fake_tick_reset(1000U);
  fake_sidetone_pwm_set_headphones(is_headphones);
  sidetone_init(hz, (uint8_t)ms);
  /* D-RM11: the ramp adds no preemption point to the init (the table fill has its own). */
  (void)QA_CHECK(fake_sidetone_pwm_preemption_count() == init_points_of_8_ms());
  (void)QA_CHECK(sidetone_ramp_calls == (4U * clamped));
  (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_OFF);
  (void)QA_CHECK(!sidetone_is_busy());
  model.requested_hz = qa_clamp_hz(hz);
  model.hz[0] = model.requested_hz;
  model.hz[1] = QA_TORN; /* never written yet */
  model.state = SIDETONE_STATE_OFF;
  model.position = 0U;
  model.increment = qa_increment(clamped);
  model.calls = 4U * clamped;
  model.is_on = false;
  model.is_pending = false;
  model.is_b_active = false;
  model.is_headphones = is_headphones;
  model.is_flag = false;
  fake_sidetone_pwm_clear_log();
  log_consumed = 0U;
  fake_sidetone_pwm_set_preemption_hook(qa_writer_hook);
  writer.kind = WRITER_NONE;
}

static void qa_end(void) {
  fake_sidetone_pwm_set_preemption_hook(NULL);
  (void)QA_CHECK(fake_sidetone_pwm_violation_count() == 0U);
  (void)QA_CHECK(!fake_sidetone_pwm_is_log_overflowed());
}

/* Runs until the model reaches @p state; returns the number of calls, or 1000 when it never does. */
static size_t qa_run_until(uint8_t state) {
  size_t calls = 0U;
  while ((model.state != state) && (calls < 1000U)) {
    qa_service();
    ++calls;
  }
  (void)QA_CHECK(calls < 1000U);
  return calls;
}

/* Ends any tone: request off, update events in ZERO_WAIT, until OFF. */
static void qa_finish_to_off(void) {
  size_t calls = 0U;
  qa_on(false);
  while ((model.state != SIDETONE_STATE_OFF) && (calls < 1000U)) {
    if (model.state == SIDETONE_STATE_ZERO_WAIT) {
      qa_update_event();
    }
    qa_service();
    ++calls;
  }
  (void)QA_CHECK(calls < 1000U);
}

/* ---------- Tests ---------- */

/*
 * Check 1, AC-3, AC-4, AC-26: for every requested ramp 0..20 (clamped), from OFF and from STEADY, the rise and the
 * fall each take exactly 4 x N calls; the indexes are monotonic and step by at most ceil(I / 64); the rise ends at
 * index 32 and the fall at index 0, and the flag clear comes after the compare 0 write. Not a model check: the
 * oracle is the compare log at 1000 Hz.
 */
static void test_full_ramps_take_4n_calls(void) {
  unsigned ms;
  for (ms = 0U; ms <= 20U; ++ms) {
    const uint8_t n = qa_clamp_ms(ms);
    const unsigned max_step = (qa_increment(n) + 63U) / 64U;
    unsigned use_init;
    for (use_init = 0U; use_init < 2U; ++use_init) {
      unsigned call;
      int previous = 0;
      unsigned cycle;
      qa_set_context("ramp ms, init", ms, use_init);
      if (use_init != 0U) {
        qa_setup(1000U, ms, false);
      } else {
        qa_setup(1000U, (ms == 9U) ? 4U : 9U, false);
        qa_set_ramp(ms, NULL, NULL);
      }
      for (cycle = 0U; cycle < 2U; ++cycle) {
        qa_on(true);
        qa_service(); /* start call */
        (void)QA_CHECK(model.state == SIDETONE_STATE_RISING);
        previous = 0;
        for (call = 1U; call <= 4U * n; ++call) {
          (void)QA_CHECK(sidetone_state() != SIDETONE_STATE_STEADY);
          qa_service();
          if (QA_CHECK(last_write_index >= 0)) {
            (void)QA_CHECK(last_write_index >= previous);
            (void)QA_CHECK((unsigned)(last_write_index - previous) <= max_step);
            previous = last_write_index;
          }
        }
        (void)QA_CHECK(previous == (int)QA_STEPS);
        (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_STEADY);
        (void)QA_CHECK(fake_sidetone_pwm_compare_speaker() == 6000U);
        qa_services(5U);
        (void)QA_CHECK(last_write_index == -1); /* STEADY writes nothing */
        qa_on(false);
        for (call = 1U; call <= 4U * n; ++call) {
          (void)QA_CHECK(sidetone_state() != SIDETONE_STATE_ZERO_WAIT);
          qa_service();
          if (QA_CHECK(last_write_index >= 0)) {
            (void)QA_CHECK(last_write_index <= previous);
            (void)QA_CHECK((unsigned)(previous - last_write_index) <= max_step);
            previous = last_write_index;
          }
        }
        (void)QA_CHECK(previous == 0);
        (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_ZERO_WAIT);
        (void)QA_CHECK(fake_sidetone_pwm_compare_speaker() == 0U);
        (void)QA_CHECK(!fake_sidetone_pwm_is_update_flag_set());
        qa_update_event();
        qa_service();
        (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_OFF);
        (void)QA_CHECK(fake_sidetone_pwm_eno() == FAKE_SIDETONE_PWM_ENO_OFF);
      }
      qa_end();
    }
  }
}

/* AC-26: sidetone_ramp_calls is 4 x the clamped value for every byte, through both entry points. */
static void test_call_count_for_every_byte(void) {
  unsigned ms;
  for (ms = 0U; ms <= 255U; ++ms) {
    qa_set_context("ramp ms", ms, 0U);
    qa_setup(600U, ms, false);
    (void)QA_CHECK(sidetone_ramp_calls == (4U * qa_clamp_ms(ms)));
    qa_set_ramp((ms + 77U) & 0xFFU, NULL, NULL);
    qa_set_ramp(ms, NULL, NULL);
    (void)QA_CHECK(sidetone_ramp_calls == (4U * qa_clamp_ms(ms)));
    qa_end();
  }
  (void)QA_CHECK(SIDETONE_RAMP_MIN_MS == settings_min(SETTINGS_ID_RAMP_MS));
  (void)QA_CHECK(SIDETONE_RAMP_MAX_MS == settings_max(SETTINGS_ID_RAMP_MS));
  (void)QA_CHECK(SIDETONE_RAMP_MAX_CALLS == 48U);
}

/* AC-25: at 8 ms the rise writes index 1 to 32 and the fall 31 to 0, one step per call (the first design). */
static void test_8_ms_is_the_first_design(void) {
  unsigned call;
  qa_set_context("8 ms", 0U, 0U);
  qa_setup(1000U, 8U, true);
  qa_on(true);
  qa_service();
  for (call = 1U; call <= QA_STEPS; ++call) {
    qa_service();
    (void)QA_CHECK(last_write_index == (int)call);
    (void)QA_CHECK(sidetone_state() == ((call == QA_STEPS) ? SIDETONE_STATE_STEADY : SIDETONE_STATE_RISING));
  }
  qa_on(false);
  for (call = 1U; call <= QA_STEPS; ++call) {
    qa_service();
    (void)QA_CHECK(last_write_index == (int)(QA_STEPS - call));
    (void)QA_CHECK(sidetone_state() == ((call == QA_STEPS) ? SIDETONE_STATE_ZERO_WAIT : SIDETONE_STATE_FALLING));
  }
  qa_end();
}

/*
 * Check 2, AC-7: a reversal after every rise call, after every fall call and in STEADY, for every ramp 4..12.
 * The model checks each event; the test also checks that the first write after the reversal moves at most
 * ceil(I / 64) indexes and that the envelope ends at STEADY or OFF.
 */
static void test_reversal_at_every_position(void) {
  uint8_t n;
  for (n = 4U; n <= 12U; ++n) {
    const unsigned max_step = (qa_increment(n) + 63U) / 64U;
    unsigned at;
    for (at = 0U; at <= 4U * n; ++at) {
      int before;
      /* Off after rise call `at`, then on again after one fall call. */
      qa_set_context("reversal in rise: ms, call", n, at);
      qa_setup(1000U, n, (at & 1U) != 0U);
      qa_on(true);
      qa_services(1U + at);
      before = (int)(model.position / 64U);
      qa_on(false);
      qa_service();
      if (at == 0U) {
        (void)QA_CHECK(model.state == SIDETONE_STATE_ZERO_WAIT); /* off right after the start call */
      } else {
        (void)QA_CHECK((last_write_index >= 0) && (last_write_index <= before) &&
                       ((unsigned)(before - last_write_index) <= max_step));
      }
      qa_on(true);
      if (model.state != SIDETONE_STATE_ZERO_WAIT) {
        before = (int)(model.position / 64U);
        qa_service();
        (void)QA_CHECK((last_write_index >= before) && ((unsigned)(last_write_index - before) <= max_step));
        (void)qa_run_until(SIDETONE_STATE_STEADY);
      } else {
        qa_update_event();
        (void)qa_run_until(SIDETONE_STATE_STEADY);
      }
      qa_on(false);
      (void)qa_run_until(SIDETONE_STATE_ZERO_WAIT);
      qa_update_event();
      (void)qa_run_until(SIDETONE_STATE_OFF);
      qa_end();

      /* On after fall call `at` from STEADY, then off again. */
      qa_set_context("reversal in fall: ms, call", n, at);
      qa_setup(1000U, n, (at & 1U) == 0U);
      qa_on(true);
      (void)qa_run_until(SIDETONE_STATE_STEADY);
      qa_on(false);
      qa_services(at);
      if (model.state == SIDETONE_STATE_ZERO_WAIT) {
        qa_on(true); /* ZERO_WAIT ignores the request */
        qa_service();
        (void)QA_CHECK(model.state != SIDETONE_STATE_RISING);
      } else {
        qa_on(true);
        before = (int)(model.position / 64U);
        qa_service();
        if (at == 0U) {
          (void)QA_CHECK(last_write_index == -1); /* still at full: STEADY */
        } else {
          (void)QA_CHECK((last_write_index >= before) && ((unsigned)(last_write_index - before) <= max_step));
        }
      }
      qa_update_event();
      (void)qa_run_until(SIDETONE_STATE_STEADY);
      qa_on(false);
      (void)qa_run_until(SIDETONE_STATE_ZERO_WAIT);
      qa_services(QA_ZERO_WAIT_LIMIT); /* the limit path ends it */
      (void)QA_CHECK(model.state == SIDETONE_STATE_OFF);
      qa_end();
    }
  }
}

/*
 * Check 2, AC-27: a ramp change at each call of a rise and of a fall, for 4 -> 12, 12 -> 4 and 6 -> 7 ms. The model
 * checks every event (no jump: the next write is one new increment away). A change in OFF, STEADY and ZERO_WAIT
 * writes no register and changes no state.
 */
static void test_ramp_change_at_every_call(void) {
  static const uint8_t pairs[3][2] = {{4U, 12U}, {12U, 4U}, {6U, 7U}};
  size_t pair;
  for (pair = 0U; pair < 3U; ++pair) {
    const uint8_t from = pairs[pair][0];
    const uint8_t to = pairs[pair][1];
    unsigned at;
    for (at = 0U; at <= 4U * from; ++at) {
      size_t calls;
      qa_set_context("change in rise: pair, call", pair, at);
      qa_setup(1000U, from, false);
      qa_on(true);
      qa_services(1U + at);
      qa_set_ramp(to, NULL, NULL);
      calls = qa_run_until(SIDETONE_STATE_STEADY);
      (void)QA_CHECK(calls <= (4U * to));
      qa_on(false);
      calls = qa_run_until(SIDETONE_STATE_ZERO_WAIT);
      (void)QA_CHECK(calls == (4U * to)); /* a full fall at the new speed */
      qa_update_event();
      (void)qa_run_until(SIDETONE_STATE_OFF);
      qa_end();

      qa_set_context("change in fall: pair, call", pair, at);
      qa_setup(1000U, from, true);
      qa_on(true);
      (void)qa_run_until(SIDETONE_STATE_STEADY);
      qa_on(false);
      qa_services(at);
      qa_set_ramp(to, NULL, NULL);
      (void)qa_run_until(SIDETONE_STATE_ZERO_WAIT);
      qa_on(true);
      qa_update_event();
      (void)qa_run_until(SIDETONE_STATE_OFF);
      calls = qa_run_until(SIDETONE_STATE_STEADY);
      (void)QA_CHECK(calls == (1U + 4U * to)); /* start call, then a full rise at the new speed */
      qa_end();
    }
  }
  {
    static const uint8_t states[3] = {SIDETONE_STATE_OFF, SIDETONE_STATE_STEADY, SIDETONE_STATE_ZERO_WAIT};
    size_t which;
    for (which = 0U; which < 3U; ++which) {
      uint8_t to;
      for (to = 0U; to <= 14U; ++to) {
        size_t register_events;
        size_t i;
        qa_set_context("change in state: state, ms", states[which], to);
        qa_setup(800U, 6U, false);
        if (states[which] != SIDETONE_STATE_OFF) {
          qa_on(true);
          (void)qa_run_until(SIDETONE_STATE_STEADY);
        }
        if (states[which] == SIDETONE_STATE_ZERO_WAIT) {
          qa_on(false);
          (void)qa_run_until(SIDETONE_STATE_ZERO_WAIT);
        }
        fake_sidetone_pwm_clear_log();
        log_consumed = 0U;
        qa_set_ramp(to, NULL, NULL);
        register_events = 0U;
        for (i = 0U; i < fake_sidetone_pwm_event_count(); ++i) {
          if (fake_sidetone_pwm_event_at(i).kind != FAKE_SIDETONE_PWM_EVENT_PREEMPTION_POINT) {
            ++register_events;
          }
        }
        (void)QA_CHECK(register_events == 0U);
        (void)QA_CHECK(sidetone_state() == states[which]);
        qa_services(3U);
        (void)QA_CHECK(sidetone_state() ==
                       ((states[which] == SIDETONE_STATE_ZERO_WAIT) ? SIDETONE_STATE_ZERO_WAIT : states[which]));
        qa_end();
      }
    }
  }
}

/* Drives a fresh module at @p ms to position @p number: 0 OFF, 1 OFF with request, then RISING, STEADY, FALLING. */
static bool qa_drive(unsigned ms, unsigned number) {
  const unsigned ramp = 4U * qa_clamp_ms(ms);
  qa_setup(1000U, ms, (number & 1U) != 0U);
  if (number == 0U) {
    return true;
  }
  qa_on(true);
  if (number == 1U) {
    return true;
  }
  if (number <= (2U + ramp)) {
    qa_services(number - 1U); /* start call and number - 2 rise calls */
    return true;
  }
  (void)qa_run_until(SIDETONE_STATE_STEADY);
  if (number == (3U + ramp)) {
    return true;
  }
  qa_on(false);
  if (number < (4U + 2U * ramp)) {
    qa_services(number - (3U + ramp));
    return true;
  }
  if (number < (4U + 2U * ramp + QA_ZERO_WAIT_LIMIT)) {
    (void)qa_run_until(SIDETONE_STATE_ZERO_WAIT);
    qa_services(number - (4U + 2U * ramp));
    return true;
  }
  return false;
}

/*
 * Check 3, AC-15 at 4 and 12 ms: in every position, the tone writer is preempted at each of its 36 points (one
 * service call there, with and without an update event), and also at every point. The model checks that each rise
 * uses one tone only and that the next start after the write uses the new tone.
 */
static void test_tone_mailbox_preemption(void) {
  static const uint8_t ramps[2] = {4U, 12U};
  size_t r;
  for (r = 0U; r < 2U; ++r) {
    unsigned number;
    for (number = 0U; qa_drive(ramps[r], number); ++number) {
      unsigned point;
      for (point = 0U; point <= QA_TONE_POINTS; ++point) {
        uint8_t services[QA_TONE_POINTS] = {0U};
        bool updates[QA_TONE_POINTS] = {false};
        unsigned variant;
        for (variant = 0U; variant < 2U; ++variant) {
          size_t k;
          qa_set_context("tone tear: ramp, position*64+point", ramps[r], number * 64U + point);
          (void)qa_drive(ramps[r], number);
          for (k = 0U; k < QA_TONE_POINTS; ++k) {
            services[k] = (uint8_t)((point == QA_TONE_POINTS) || (k == point) ? 1U : 0U);
            updates[k] = (variant != 0U) && (services[k] != 0U);
          }
          qa_set_tone(600U, services, updates);
          /* Second writer call before the module used the first. */
          if (variant != 0U) {
            qa_set_tone(450U, services, NULL);
          }
          qa_finish_to_off();
          qa_on(true);
          qa_service();
          (void)QA_CHECK(fake_sidetone_pwm_arr() == qa_arr((variant != 0U) ? 450U : 600U));
          (void)qa_run_until(SIDETONE_STATE_STEADY);
          (void)QA_CHECK(fake_sidetone_pwm_compare_speaker() + fake_sidetone_pwm_compare_headphones() ==
                         qa_compare((variant != 0U) ? 450U : 600U, QA_STEPS));
          qa_end();
        }
      }
    }
  }
}

/*
 * Check 3, AC-29: in every position, the ramp writer is preempted at its first point, its second point, or both,
 * with 1 or 5 service calls there, for 4 -> 12 and 12 -> 4 ms. The model checks every event; the envelope then ends
 * at STEADY or ZERO_WAIT with no ENO violation.
 */
static void test_ramp_writer_preemption(void) {
  static const uint8_t pairs[2][2] = {{4U, 12U}, {12U, 4U}};
  static const uint8_t plans[6][QA_RAMP_POINTS] = {{1U, 0U}, {0U, 1U}, {1U, 1U}, {5U, 0U}, {0U, 5U}, {5U, 5U}};
  size_t pair;
  for (pair = 0U; pair < 2U; ++pair) {
    unsigned number;
    for (number = 0U; qa_drive(pairs[pair][0], number); ++number) {
      size_t plan;
      for (plan = 0U; plan < 6U; ++plan) {
        qa_set_context("ramp tear: pair*1000+position, plan", pair * 1000U + number, plan);
        (void)qa_drive(pairs[pair][0], number);
        qa_set_ramp(pairs[pair][1], plans[plan], NULL);
        if (model.is_on) {
          (void)qa_run_until(SIDETONE_STATE_STEADY);
        } else if ((model.state == SIDETONE_STATE_RISING) || (model.state == SIDETONE_STATE_FALLING)) {
          (void)qa_run_until(SIDETONE_STATE_ZERO_WAIT);
        }
        qa_finish_to_off();
        qa_on(true);
        (void)QA_CHECK(qa_run_until(SIDETONE_STATE_STEADY) == (1U + 4U * pairs[pair][1]));
        qa_end();
      }
    }
  }
}

/* Random stimulus over every input: on, off, ramp 0..15, tone, update events, in lockstep with the model. */
static void test_random_lockstep(void) {
  uint32_t rng = 0x5EEDU;
  unsigned run;
  for (run = 0U; run < 400U; ++run) {
    unsigned step;
    rng = rng * 1664525U + 1013904223U;
    qa_set_context("random run", run, 0U);
    qa_setup((uint16_t)(400U + ((rng >> 8) % 61U) * 10U), (rng >> 16) % 16U, (rng & 0x100U) != 0U);
    for (step = 0U; step < 2000U; ++step) {
      unsigned pick;
      rng = rng * 1664525U + 1013904223U;
      pick = (rng >> 8) % 1000U;
      qa_set_context("random run, step", run, step);
      if (pick < 30U) {
        qa_on(!model.is_on);
      } else if (pick < 38U) {
        qa_set_ramp((rng >> 20) % 16U, NULL, NULL);
      } else if (pick < 41U) {
        qa_set_tone((uint16_t)(390U + ((rng >> 18) % 63U) * 10U), NULL, NULL);
      } else if (pick < 120U) {
        qa_update_event();
      } else {
        qa_service();
      }
    }
    qa_end();
  }
}

int main(void) {
  TEST_RUN(test_full_ramps_take_4n_calls);
  TEST_RUN(test_call_count_for_every_byte);
  TEST_RUN(test_8_ms_is_the_first_design);
  TEST_RUN(test_reversal_at_every_position);
  TEST_RUN(test_ramp_change_at_every_call);
  TEST_RUN(test_tone_mailbox_preemption);
  TEST_RUN(test_ramp_writer_preemption);
  TEST_RUN(test_random_lockstep);
  return TEST_FINISH();
}
