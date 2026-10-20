/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests for the sidetone module (references/internal/sidetone_spec.md, AC-7 to AC-17).
 *
 * Every service call runs in lockstep with an independent model of the state table (section 6.3) and the mailbox
 * (section 6.4). The model predicts the exact register events of each call. A second checker validates each event
 * stream without the model: ENO values, the start sequence, ramp steps of one tone only, route ownership.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "board/sidetone_config.h"
#include "fake_sidetone_pwm.h"
#include "fake_tick.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"
#include "support/test_assert.h"

#define QA_TABLE_SIZE (SIDETONE_RAMP_STEPS + 1U)
#define QA_COUNTER_HZ 12000000UL
/* Step 2, 33 table stores, the ARR store, step 5 (section 6.4). */
#define QA_POINTS_PER_WRITE 36U
#define QA_STALE_COMPARE 0xBEEFU
#define QA_STALE_ARR 0xBEEEU
#define QA_MAX_EVENTS_PER_CALL 8U
#define QA_LOG_CLEAR_LEVEL 3000U
#define QA_MAX_PRINTED_FAILURES 40U
#define QA_NO_COMPARE 0xFFFFFFFFUL

#define QA_STATE_OFF_ON 5U /* position only: OFF with the on request set, before the start call */

#define QA_ENO_OFF FAKE_SIDETONE_PWM_ENO_OFF
#define QA_ENO_SPEAKER FAKE_SIDETONE_PWM_ENO_SPEAKER
#define QA_ENO_HEADPHONES FAKE_SIDETONE_PWM_ENO_HEADPHONES

/* Independent copy of the duty table of section 6.1. */
static const uint16_t qa_duty[QA_TABLE_SIZE] = {0U,   1U,   5U,   11U,  19U,  30U,  42U,  57U,  73U,  91U,  111U,
                                                132U, 154U, 177U, 201U, 225U, 250U, 275U, 299U, 323U, 346U, 368U,
                                                389U, 409U, 427U, 443U, 458U, 470U, 481U, 489U, 495U, 499U, 500U};

/* ---------- Failure reporting ---------- */

static const char* qa_context = "";
static unsigned long qa_context_value;
static unsigned long qa_printed_failures;

static bool qa_check(bool is_passed, const char* expression, int line) {
  if (is_passed) {
    ++test_check_count;
    return true;
  }
  if (qa_printed_failures < QA_MAX_PRINTED_FAILURES) {
    ++qa_printed_failures;
    fprintf(stderr, "  context: %s %lu\n", qa_context, qa_context_value);
    (void)test_check(false, expression, __FILE__, line);
  } else {
    ++test_check_count;
    ++test_failure_count;
  }
  return false;
}

#define QA_CHECK(condition) qa_check((condition), #condition, __LINE__)

static void qa_set_context(const char* text, unsigned long value) {
  qa_context = text;
  qa_context_value = value;
}

/* ---------- Tone math, written from section 6.1 ---------- */

static uint16_t qa_clamp(uint16_t hz) {
  if (hz < 400U) {
    return 400U;
  }
  return (hz > 1000U) ? 1000U : hz;
}

static uint32_t qa_period(uint16_t hz) { return (QA_COUNTER_HZ + (uint32_t)hz / 2U) / hz; }

static uint16_t qa_arr(uint16_t hz) { return (uint16_t)(qa_period(hz) - 1U); }

static uint16_t qa_compare(uint16_t hz, uint8_t index) {
  return (uint16_t)((qa_period(hz) * (uint32_t)qa_duty[index]) / (uint32_t)1000U);
}

/* Returns the tone in 400..1000 Hz whose ARR is @p arr, or 0. */
static uint16_t qa_tone_of_arr(uint16_t arr) {
  uint16_t hz;
  for (hz = 400U; hz <= 1000U; ++hz) {
    if (qa_arr(hz) == arr) {
      return hz;
    }
  }
  return 0U;
}

/* ---------- Model of the module (sections 6.3 to 6.6) ---------- */

typedef struct {
  uint16_t table[2][QA_TABLE_SIZE];
  uint16_t arr[2];
  uint16_t requested_hz;
  uint8_t state;
  uint8_t index;
  bool is_on;
  bool is_pending;
  bool is_b_active;
  bool is_headphones_requested;
  bool is_headphones_active;
  bool is_flag;
  bool is_raw_headphones;
  bool is_candidate_headphones;
  uint16_t change_time;
} qa_model_t;

static qa_model_t model;

typedef struct {
  fake_sidetone_pwm_event_t events[QA_MAX_EVENTS_PER_CALL];
  size_t count;
} qa_expected_t;

static void qa_emit(qa_expected_t* expected, uint8_t kind, uint16_t value) {
  if (QA_CHECK(expected->count < QA_MAX_EVENTS_PER_CALL)) {
    expected->events[expected->count].kind = kind;
    expected->events[expected->count].value = value;
    ++expected->count;
  }
}

static void qa_fill(uint16_t* table, uint16_t hz) {
  uint8_t k;
  for (k = 0U; k < QA_TABLE_SIZE; ++k) {
    table[k] = qa_compare(hz, k);
  }
}

static void model_init(uint16_t hz, bool is_headphones) {
  uint8_t k;
  model.requested_hz = qa_clamp(hz);
  qa_fill(model.table[0], model.requested_hz);
  model.arr[0] = qa_arr(model.requested_hz);
  /* Table B holds no tone yet; a read of it before a write shows up as a wrong compare value. */
  for (k = 0U; k < QA_TABLE_SIZE; ++k) {
    model.table[1][k] = QA_STALE_COMPARE;
  }
  model.arr[1] = QA_STALE_ARR;
  model.state = SIDETONE_STATE_OFF;
  model.index = 0U;
  model.is_on = false;
  model.is_pending = false;
  model.is_b_active = false;
  model.is_headphones_requested = is_headphones;
  model.is_headphones_active = is_headphones;
  model.is_flag = false;
  model.is_raw_headphones = is_headphones;
  model.is_candidate_headphones = is_headphones;
  model.change_time = 0U;
}

static uint8_t model_compare_kind(void) {
  return model.is_headphones_active ? FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES
                                    : FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER;
}

static uint16_t model_active_value(uint8_t index) { return model.table[model.is_b_active ? 1U : 0U][index]; }

static void model_enter_zero_wait(qa_expected_t* expected) {
  qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_CLEAR_UPDATE_FLAG, 0U);
  model.is_flag = false;
  model.state = SIDETONE_STATE_ZERO_WAIT;
}

static void model_service_off(qa_expected_t* expected) {
  if (!model.is_on) {
    return;
  }
  if (model.is_pending) {
    model.is_b_active = !model.is_b_active;
    model.is_pending = false;
    qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_SET_ARR, model.arr[model.is_b_active ? 1U : 0U]);
  }
  model.is_headphones_active = model.is_headphones_requested;
  qa_emit(expected, model_compare_kind(), 0U);
  qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE, 0U);
  model.is_flag = true;
  qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_SET_ENO, model.is_headphones_active ? QA_ENO_HEADPHONES : QA_ENO_SPEAKER);
  model.index = 0U;
  model.state = SIDETONE_STATE_RISING;
}

static void model_service_zero_wait(qa_expected_t* expected) {
  qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_READ_UPDATE_FLAG, model.is_flag ? 1U : 0U);
  if (model.is_flag || ((uint8_t)(model.index + 1U) >= SIDETONE_ZERO_WAIT_LIMIT)) {
    qa_emit(expected, FAKE_SIDETONE_PWM_EVENT_SET_ENO, QA_ENO_OFF);
    model.index = 0U;
    model.state = SIDETONE_STATE_OFF;
  } else {
    ++model.index;
  }
}

static void model_service_ramp(qa_expected_t* expected) {
  const bool is_up = model.is_on && !model.is_pending;

  if (is_up) {
    if (model.index == SIDETONE_RAMP_STEPS) {
      model.state = SIDETONE_STATE_STEADY;
      return;
    }
    ++model.index;
    qa_emit(expected, model_compare_kind(), model_active_value(model.index));
    model.state = (model.index == SIDETONE_RAMP_STEPS) ? SIDETONE_STATE_STEADY : SIDETONE_STATE_RISING;
    return;
  }
  if (model.index == 0U) {
    (void)QA_CHECK(model.state == SIDETONE_STATE_RISING);
    model_enter_zero_wait(expected);
    return;
  }
  --model.index;
  qa_emit(expected, model_compare_kind(), model_active_value(model.index));
  if (model.index == 0U) {
    model_enter_zero_wait(expected);
  } else {
    model.state = SIDETONE_STATE_FALLING;
  }
}

static void model_service(qa_expected_t* expected) {
  expected->count = 0U;
  if (model.state == SIDETONE_STATE_OFF) {
    model_service_off(expected);
  } else if (model.state == SIDETONE_STATE_ZERO_WAIT) {
    model_service_zero_wait(expected);
  } else {
    model_service_ramp(expected);
  }
}

static void model_poll(uint16_t now) {
  if (model.is_raw_headphones != model.is_candidate_headphones) {
    model.is_candidate_headphones = model.is_raw_headphones;
    model.change_time = now;
  } else if ((model.is_candidate_headphones != model.is_headphones_requested) &&
             ((uint16_t)(now - model.change_time) >= SIDETONE_PLUG_DEBOUNCE_MS)) {
    model.is_headphones_requested = model.is_candidate_headphones;
  }
}

/* ---------- Event stream checker (no model) ---------- */

typedef struct {
  uint8_t eno;
  uint16_t ramp_hz;
  int ramp_index;
  uint8_t previous_kind[2];
  uint16_t previous_value[2];
  bool is_start_open;
  bool is_arr_open;
  bool was_enabled_in_call;
  uint16_t last_compare_value;
} qa_stream_t;

static qa_stream_t stream;

static void stream_init(uint16_t arr) {
  stream.eno = QA_ENO_OFF;
  stream.ramp_hz = qa_tone_of_arr(arr);
  stream.ramp_index = 0;
  stream.previous_kind[0] = 0U;
  stream.previous_kind[1] = 0U;
  stream.previous_value[0] = 0U;
  stream.previous_value[1] = 0U;
  stream.is_start_open = false;
  stream.is_arr_open = false;
  stream.last_compare_value = 0U;
  (void)QA_CHECK(stream.ramp_hz != 0U);
}

static bool stream_is_compare(uint8_t kind) {
  return (kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER) || (kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES);
}

static uint8_t stream_eno_of_compare(uint8_t kind) {
  return (kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES) ? QA_ENO_HEADPHONES : QA_ENO_SPEAKER;
}

static void stream_check_compare(fake_sidetone_pwm_event_t event) {
  if (stream.eno == QA_ENO_OFF) {
    /* Only the start writes a compare while the outputs are off, and it writes 0. */
    (void)QA_CHECK(event.value == 0U);
    (void)QA_CHECK(!stream.is_start_open);
    stream.is_start_open = true;
    stream.is_arr_open = false;
    stream.ramp_index = 0;
  } else {
    (void)QA_CHECK(stream_eno_of_compare(event.kind) == stream.eno);
    if ((stream.ramp_index < (int)SIDETONE_RAMP_STEPS) &&
        (event.value == qa_compare(stream.ramp_hz, (uint8_t)(stream.ramp_index + 1)))) {
      ++stream.ramp_index;
    } else if ((stream.ramp_index > 0) &&
               (event.value == qa_compare(stream.ramp_hz, (uint8_t)(stream.ramp_index - 1)))) {
      --stream.ramp_index;
    } else {
      (void)QA_CHECK(!"compare write is not one step of the current tone");
    }
  }
  stream.last_compare_value = event.value;
}

static void stream_check_eno(fake_sidetone_pwm_event_t event) {
  (void)QA_CHECK((event.value == QA_ENO_OFF) || (event.value == QA_ENO_SPEAKER) || (event.value == QA_ENO_HEADPHONES));
  (void)QA_CHECK((event.value & 0x01U) == 0U); /* ENO1P: P1.0 is the plug input */
  if (event.value == QA_ENO_OFF) {
    (void)QA_CHECK(stream.eno != QA_ENO_OFF);
    (void)QA_CHECK(stream.last_compare_value == 0U);
  } else {
    /* Enable only from off, directly after "compare 0 of this route, UG". */
    (void)QA_CHECK(stream.eno == QA_ENO_OFF);
    (void)QA_CHECK(stream.previous_kind[0] == FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE);
    (void)QA_CHECK(stream_is_compare(stream.previous_kind[1]));
    (void)QA_CHECK(stream_eno_of_compare(stream.previous_kind[1]) == event.value);
    (void)QA_CHECK(stream.previous_value[1] == 0U);
    (void)QA_CHECK(stream.is_start_open);
    stream.is_start_open = false;
    stream.was_enabled_in_call = true;
  }
  stream.eno = (uint8_t)event.value;
}

static void stream_check(fake_sidetone_pwm_event_t event) {
  switch (event.kind) {
    case FAKE_SIDETONE_PWM_EVENT_SET_ARR:
      (void)QA_CHECK(stream.eno == QA_ENO_OFF);
      (void)QA_CHECK(!stream.is_start_open);
      stream.ramp_hz = qa_tone_of_arr(event.value);
      (void)QA_CHECK(stream.ramp_hz != 0U);
      stream.is_arr_open = true;
      break;
    case FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER:
    case FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES:
      stream_check_compare(event);
      break;
    case FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE:
      (void)QA_CHECK(stream.eno == QA_ENO_OFF);
      (void)QA_CHECK(stream.is_start_open);
      (void)QA_CHECK(stream_is_compare(stream.previous_kind[0]) && (stream.previous_value[0] == 0U));
      break;
    case FAKE_SIDETONE_PWM_EVENT_CLEAR_UPDATE_FLAG:
      /* Compare 0 comes first, then the flag clear (section 3.3). */
      (void)QA_CHECK(stream.eno != QA_ENO_OFF);
      (void)QA_CHECK(stream.last_compare_value == 0U);
      break;
    case FAKE_SIDETONE_PWM_EVENT_SET_ENO:
      stream_check_eno(event);
      break;
    case FAKE_SIDETONE_PWM_EVENT_READ_UPDATE_FLAG:
      (void)QA_CHECK(stream.eno != QA_ENO_OFF);
      break;
    default:
      (void)QA_CHECK(!"unexpected event kind in a service call");
      break;
  }
  stream.previous_kind[1] = stream.previous_kind[0];
  stream.previous_value[1] = stream.previous_value[0];
  stream.previous_kind[0] = event.kind;
  stream.previous_value[0] = event.value;
}

/* ---------- Lockstep driver ---------- */

static size_t log_consumed;
static size_t last_call_start;
static size_t last_call_end;
static size_t service_calls;

/* Events outside a service call: only preemption points and test update events, no register access. */
static void qa_absorb_foreign(void) {
  const size_t count = fake_sidetone_pwm_event_count();
  for (; log_consumed < count; ++log_consumed) {
    const uint8_t kind = fake_sidetone_pwm_event_at(log_consumed).kind;
    (void)QA_CHECK((kind == FAKE_SIDETONE_PWM_EVENT_PREEMPTION_POINT) ||
                   (kind == FAKE_SIDETONE_PWM_EVENT_UPDATE_EVENT));
  }
}

static void qa_check_hardware_after_call(void) {
  const uint8_t eno = fake_sidetone_pwm_eno();
  (void)QA_CHECK((eno == QA_ENO_OFF) || (eno == QA_ENO_SPEAKER) || (eno == QA_ENO_HEADPHONES));
  if (eno == QA_ENO_SPEAKER) {
    (void)QA_CHECK(fake_sidetone_pwm_compare_headphones() == 0U);
    (void)QA_CHECK(fake_sidetone_pwm_shadow_compare_headphones() == 0U);
  } else if (eno == QA_ENO_HEADPHONES) {
    (void)QA_CHECK(fake_sidetone_pwm_compare_speaker() == 0U);
    (void)QA_CHECK(fake_sidetone_pwm_shadow_compare_speaker() == 0U);
  } else {
    /* At silence the last compare of the route was 0, so a later enable cannot click. */
    (void)QA_CHECK(fake_sidetone_pwm_compare_speaker() == 0U);
    (void)QA_CHECK(fake_sidetone_pwm_compare_headphones() == 0U);
  }
  if (stream.was_enabled_in_call) {
    (void)QA_CHECK(fake_sidetone_pwm_shadow_compare_speaker() == 0U);
    (void)QA_CHECK(fake_sidetone_pwm_shadow_compare_headphones() == 0U);
    (void)QA_CHECK(fake_sidetone_pwm_shadow_arr() == fake_sidetone_pwm_arr());
  }
  (void)QA_CHECK(fake_sidetone_pwm_violation_count() == 0U);
  (void)QA_CHECK(!stream.is_start_open);
  (void)QA_CHECK(!stream.is_arr_open);
}

static void qa_service(void) {
  qa_expected_t expected;
  size_t i;
  size_t count;

  qa_absorb_foreign();
  if (fake_sidetone_pwm_event_count() > QA_LOG_CLEAR_LEVEL) {
    fake_sidetone_pwm_clear_log();
    log_consumed = 0U;
  }
  last_call_start = fake_sidetone_pwm_event_count();
  model_service(&expected);
  sidetone_service();
  ++service_calls;
  last_call_end = fake_sidetone_pwm_event_count();
  count = last_call_end - last_call_start;

  (void)QA_CHECK(count == expected.count);
  stream.was_enabled_in_call = false;
  for (i = 0U; i < count; ++i) {
    const fake_sidetone_pwm_event_t actual = fake_sidetone_pwm_event_at(last_call_start + i);
    if (i < expected.count) {
      (void)QA_CHECK(actual.kind == expected.events[i].kind);
      (void)QA_CHECK(actual.value == expected.events[i].value);
    }
    stream_check(actual);
  }
  log_consumed = last_call_end;
  (void)QA_CHECK(sidetone_state() == model.state);
  (void)QA_CHECK(sidetone_is_busy() == (model.is_on || (model.state != SIDETONE_STATE_OFF)));
  (void)QA_CHECK(fake_sidetone_pwm_is_update_flag_set() == model.is_flag);
  qa_check_hardware_after_call();
}

static void qa_services(size_t count) {
  size_t i;
  for (i = 0U; i < count; ++i) {
    qa_service();
  }
}

/* Returns the compare value that the last service call wrote, or QA_NO_COMPARE. */
static uint32_t qa_last_call_compare(void) {
  size_t i;
  for (i = last_call_start; i < last_call_end; ++i) {
    const fake_sidetone_pwm_event_t event = fake_sidetone_pwm_event_at(i);
    if (stream_is_compare(event.kind)) {
      return event.value;
    }
  }
  return QA_NO_COMPARE;
}

static size_t qa_last_call_count(uint8_t kind) {
  size_t i;
  size_t count = 0U;
  for (i = last_call_start; i < last_call_end; ++i) {
    if (fake_sidetone_pwm_event_at(i).kind == kind) {
      ++count;
    }
  }
  return count;
}

static void qa_on(bool is_on) {
  const size_t before = fake_sidetone_pwm_event_count();
  sidetone_set_on(is_on);
  model.is_on = is_on;
  (void)QA_CHECK(fake_sidetone_pwm_event_count() == before);
}

static void qa_update_event(void) {
  fake_sidetone_pwm_update_event();
  model.is_flag = true;
}

static void qa_set_plug(bool is_headphones) {
  fake_sidetone_pwm_set_headphones(is_headphones);
  model.is_raw_headphones = is_headphones;
}

static void qa_poll(uint16_t now) {
  const size_t reads = fake_tick_read_count();
  const size_t plug_reads = fake_sidetone_pwm_plug_read_count();
  fake_tick_set_now(now);
  sidetone_poll();
  model_poll(now);
  (void)QA_CHECK(fake_tick_read_count() == reads + 1U);
  (void)QA_CHECK(fake_sidetone_pwm_plug_read_count() == plug_reads + 1U);
  qa_absorb_foreign();
}

/* ---------- Mailbox writer in lockstep with its preemption points ---------- */

typedef struct {
  uint8_t services[QA_POINTS_PER_WRITE];
  bool is_update[QA_POINTS_PER_WRITE];
} qa_plan_t;

static const qa_plan_t* writer_plan;

static struct {
  bool is_active;
  uint8_t point;
  uint8_t target;
  uint16_t values[QA_TABLE_SIZE];
  uint16_t arr;
} writer;

static void qa_writer_hook(void) {
  const uint8_t point = writer.point;

  if (!QA_CHECK(writer.is_active && (point < QA_POINTS_PER_WRITE))) {
    return;
  }
  if (point == 0U) {
    model.is_pending = false;
    writer.target = model.is_b_active ? 0U : 1U;
  } else if (point <= QA_TABLE_SIZE) {
    model.table[writer.target][point - 1U] = writer.values[point - 1U];
  } else if (point == (QA_TABLE_SIZE + 1U)) {
    model.arr[writer.target] = writer.arr;
  } else {
    model.is_pending = true;
  }
  ++writer.point;
  if (writer_plan != NULL) {
    if (writer_plan->is_update[point]) {
      qa_update_event();
    }
    qa_services(writer_plan->services[point]);
  }
}

static void qa_set_tone(uint16_t hz, const qa_plan_t* plan) {
  const uint16_t clamped = qa_clamp(hz);
  const size_t points = fake_sidetone_pwm_preemption_count();

  qa_absorb_foreign();
  if (clamped == model.requested_hz) {
    const size_t events = fake_sidetone_pwm_event_count();
    sidetone_set_tone_hz(hz);
    (void)QA_CHECK(fake_sidetone_pwm_preemption_count() == points);
    (void)QA_CHECK(fake_sidetone_pwm_event_count() == events);
    return;
  }
  writer.is_active = true;
  writer.point = 0U;
  qa_fill(writer.values, clamped);
  writer.arr = qa_arr(clamped);
  writer_plan = plan;
  sidetone_set_tone_hz(hz);
  writer_plan = NULL;
  writer.is_active = false;
  (void)QA_CHECK(writer.point == QA_POINTS_PER_WRITE);
  (void)QA_CHECK(fake_sidetone_pwm_preemption_count() == points + QA_POINTS_PER_WRITE);
  model.requested_hz = clamped;
  qa_absorb_foreign();
}

static void qa_setup(uint16_t hz, bool is_headphones) {
  size_t i;
  size_t order = 0U;
  static const uint8_t init_kinds[3] = {FAKE_SIDETONE_PWM_EVENT_CONFIGURE, FAKE_SIDETONE_PWM_EVENT_PINS_CONFIGURE,
                                        FAKE_SIDETONE_PWM_EVENT_TIMER_START};

  fake_sidetone_pwm_reset();
  fake_tick_reset(1000U);
  fake_sidetone_pwm_set_headphones(is_headphones);
  sidetone_init(hz, 8U);
  model_init(hz, is_headphones);

  for (i = 0U; i < fake_sidetone_pwm_event_count(); ++i) {
    const fake_sidetone_pwm_event_t event = fake_sidetone_pwm_event_at(i);
    if (event.kind == FAKE_SIDETONE_PWM_EVENT_PREEMPTION_POINT) {
      continue;
    }
    if (QA_CHECK(order < 3U)) {
      (void)QA_CHECK(event.kind == init_kinds[order]);
    }
    ++order;
  }
  (void)QA_CHECK(order == 3U);
  (void)QA_CHECK(fake_sidetone_pwm_configured_arr() == qa_arr(qa_clamp(hz)));
  (void)QA_CHECK(fake_sidetone_pwm_writes_before_configure() == 0U);
  (void)QA_CHECK(fake_sidetone_pwm_eno() == QA_ENO_OFF);
  (void)QA_CHECK(fake_tick_read_count() == 0U);
  (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_OFF);
  (void)QA_CHECK(!sidetone_is_busy());

  fake_sidetone_pwm_clear_log();
  log_consumed = 0U;
  stream_init(fake_sidetone_pwm_configured_arr());
  fake_sidetone_pwm_set_preemption_hook(qa_writer_hook);
}

static void qa_end(void) { (void)QA_CHECK(fake_sidetone_pwm_violation_count() == 0U); }

/* Runs service calls until the model reaches @p state; an update event comes every @p update_every calls. */
static size_t qa_run_until(uint8_t state, size_t update_every) {
  size_t calls = 0U;
  while ((model.state != state) && (calls < 400U)) {
    if ((update_every != 0U) && ((calls % update_every) == (update_every - 1U))) {
      qa_update_event();
    }
    qa_service();
    ++calls;
  }
  (void)QA_CHECK(model.state == state);
  return calls;
}

/* From a fresh setup, drives the module to @p state at @p index (QA_STATE_OFF_ON: OFF with the request set). */
static void qa_drive(uint8_t state, uint8_t index) {
  switch (state) {
    case SIDETONE_STATE_OFF:
      break;
    case QA_STATE_OFF_ON:
      qa_on(true);
      break;
    case SIDETONE_STATE_RISING:
      qa_on(true);
      qa_services(1U + index);
      break;
    case SIDETONE_STATE_STEADY:
      qa_on(true);
      qa_services(1U + SIDETONE_RAMP_STEPS);
      break;
    case SIDETONE_STATE_FALLING:
      qa_on(true);
      qa_services(1U + SIDETONE_RAMP_STEPS);
      qa_on(false);
      qa_services(SIDETONE_RAMP_STEPS - index);
      break;
    default:
      qa_on(true);
      qa_services(1U + SIDETONE_RAMP_STEPS);
      qa_on(false);
      qa_services(SIDETONE_RAMP_STEPS + index);
      break;
  }
  if (state == QA_STATE_OFF_ON) {
    (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_OFF);
  } else {
    (void)QA_CHECK(sidetone_state() == state);
    (void)QA_CHECK(model.index == ((state == SIDETONE_STATE_STEADY) ? SIDETONE_RAMP_STEPS : index));
  }
}

typedef struct {
  uint8_t state;
  uint8_t index;
} qa_position_t;

/* OFF, OFF with request, RISING 0..31, STEADY, FALLING 1..31, ZERO_WAIT 0..11. */
#define QA_POSITION_COUNT (2U + SIDETONE_RAMP_STEPS + 1U + (SIDETONE_RAMP_STEPS - 1U) + SIDETONE_ZERO_WAIT_LIMIT)

static qa_position_t qa_position(size_t number) {
  qa_position_t position;
  if (number == 0U) {
    position.state = SIDETONE_STATE_OFF;
    position.index = 0U;
  } else if (number == 1U) {
    position.state = QA_STATE_OFF_ON;
    position.index = 0U;
  } else if (number < (2U + SIDETONE_RAMP_STEPS)) {
    position.state = SIDETONE_STATE_RISING;
    position.index = (uint8_t)(number - 2U);
  } else if (number == (2U + SIDETONE_RAMP_STEPS)) {
    position.state = SIDETONE_STATE_STEADY;
    position.index = SIDETONE_RAMP_STEPS;
  } else if (number < (2U + SIDETONE_RAMP_STEPS + SIDETONE_RAMP_STEPS)) {
    position.state = SIDETONE_STATE_FALLING;
    position.index = (uint8_t)(number - (2U + SIDETONE_RAMP_STEPS));
  } else {
    position.state = SIDETONE_STATE_ZERO_WAIT;
    position.index = (uint8_t)(number - (2U + SIDETONE_RAMP_STEPS + SIDETONE_RAMP_STEPS));
  }
  return position;
}

/* ---------- Tests ---------- */

/* AC-7: a reversal at each index moves one table step in the new direction on the next call. */
static void test_reversal_at_every_index(void) {
  static const uint16_t tones[2] = {400U, 1000U};
  size_t t;
  uint8_t route;
  uint8_t i;

  for (t = 0U; t < 2U; ++t) {
    for (route = 0U; route < 2U; ++route) {
      const bool is_headphones = (route != 0U);
      const uint16_t hz = tones[t];

      /* RISING at index i (i = 0 is the start call), and STEADY at i = 32: off request. */
      for (i = 0U; i <= SIDETONE_RAMP_STEPS; ++i) {
        qa_set_context("rising reversal down, index", i);
        qa_setup(hz, is_headphones);
        qa_drive((i == SIDETONE_RAMP_STEPS) ? SIDETONE_STATE_STEADY : SIDETONE_STATE_RISING, i);
        qa_on(false);
        qa_service();
        if (i == 0U) {
          (void)QA_CHECK(qa_last_call_compare() == QA_NO_COMPARE);
          (void)QA_CHECK(qa_last_call_count(FAKE_SIDETONE_PWM_EVENT_CLEAR_UPDATE_FLAG) == 1U);
          (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_ZERO_WAIT);
        } else {
          (void)QA_CHECK(qa_last_call_compare() == qa_compare(hz, (uint8_t)(i - 1U)));
          (void)QA_CHECK(sidetone_state() == ((i == 1U) ? SIDETONE_STATE_ZERO_WAIT : SIDETONE_STATE_FALLING));
        }
        if (i >= 2U) {
          /* Reverse again: one step back up. */
          qa_on(true);
          qa_service();
          (void)QA_CHECK(qa_last_call_compare() == qa_compare(hz, i));
          qa_on(false);
        }
        (void)qa_run_until(SIDETONE_STATE_ZERO_WAIT, 0U);
        qa_update_event();
        qa_service();
        (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_OFF);
        (void)QA_CHECK(fake_sidetone_pwm_eno() == QA_ENO_OFF);
        qa_end();
      }

      /* FALLING at index i: on request. */
      for (i = 1U; i < SIDETONE_RAMP_STEPS; ++i) {
        qa_set_context("falling reversal up, index", i);
        qa_setup(hz, is_headphones);
        qa_drive(SIDETONE_STATE_FALLING, i);
        qa_on(true);
        qa_service();
        (void)QA_CHECK(qa_last_call_compare() == qa_compare(hz, (uint8_t)(i + 1U)));
        (void)QA_CHECK(sidetone_state() ==
                       ((i + 1U == SIDETONE_RAMP_STEPS) ? SIDETONE_STATE_STEADY : SIDETONE_STATE_RISING));
        (void)QA_CHECK(qa_run_until(SIDETONE_STATE_STEADY, 0U) == (size_t)(SIDETONE_RAMP_STEPS - i - 1U));
        qa_end();
      }
    }
  }

  /* Chatter: the request toggles on every call, from each phase of the rise. */
  for (i = 0U; i < 40U; ++i) {
    size_t call;
    qa_set_context("chatter, phase", i);
    qa_setup(600U, (i % 2U) != 0U);
    qa_on(true);
    qa_services(i);
    for (call = 0U; call < 200U; ++call) {
      qa_on((call % 2U) == 0U);
      if ((call % 7U) == 6U) {
        qa_update_event();
      }
      qa_service();
    }
    qa_end();
  }
}

/* AC-8, AC-9: a retune in each position of the envelope behaves as section 6.3. */
static void test_retune_in_every_state(void) {
  size_t number;
  uint8_t variant;

  for (number = 0U; number < QA_POSITION_COUNT; ++number) {
    for (variant = 0U; variant < 2U; ++variant) {
      const qa_position_t position = qa_position(number);
      bool was_off_before = false;
      size_t calls;
      size_t arr_writes = 0U;

      qa_set_context("retune 600 -> 800 at position", (unsigned long)number);
      qa_setup(600U, variant != 0U);
      qa_drive(position.state, position.index);
      qa_set_tone(800U, NULL);
      qa_on(true);

      /* No ARR write and no UG until a start from OFF; the first ARR is the 800 Hz one. */
      for (calls = 0U; (calls < 300U) && !((model.state == SIDETONE_STATE_STEADY) && !model.is_pending &&
                                           (fake_sidetone_pwm_arr() == qa_arr(800U)));
           ++calls) {
        was_off_before = (model.state == SIDETONE_STATE_OFF);
        if ((calls % 5U) == 4U) {
          qa_update_event();
        }
        qa_service();
        if (qa_last_call_count(FAKE_SIDETONE_PWM_EVENT_SET_ARR) != 0U) {
          ++arr_writes;
          (void)QA_CHECK(was_off_before);
          (void)QA_CHECK(fake_sidetone_pwm_arr() == qa_arr(800U));
        }
        if (qa_last_call_count(FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE) != 0U) {
          (void)QA_CHECK(was_off_before);
        }
      }
      (void)QA_CHECK(arr_writes == 1U);
      (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_STEADY);
      (void)QA_CHECK((variant != 0U ? fake_sidetone_pwm_compare_headphones() : fake_sidetone_pwm_compare_speaker()) ==
                     7500U);

      /* Back to the old tone: the other table again. */
      qa_set_tone(600U, NULL);
      (void)qa_run_until(SIDETONE_STATE_ZERO_WAIT, 0U);
      qa_update_event();
      (void)qa_run_until(SIDETONE_STATE_STEADY, 0U);
      (void)QA_CHECK(fake_sidetone_pwm_arr() == qa_arr(600U));
      qa_end();
    }

    /* The same tone again changes nothing. */
    {
      const qa_position_t position = qa_position(number);
      qa_set_context("same tone at position", (unsigned long)number);
      qa_setup(600U, false);
      qa_drive(position.state, position.index);
      qa_set_tone(600U, NULL);
      qa_set_tone(600U, NULL);
      (void)QA_CHECK(!model.is_pending);
      qa_services(50U);
      qa_end();
    }
  }

  /* A retune that is replaced before the start: the newest tone wins, the first one never sounds. */
  qa_set_context("replace before use", 0U);
  qa_setup(600U, false);
  qa_set_tone(800U, NULL);
  qa_set_tone(1000U, NULL);
  qa_set_tone(600U, NULL);
  qa_set_tone(990U, NULL);
  qa_on(true);
  qa_service();
  (void)QA_CHECK(fake_sidetone_pwm_arr() == qa_arr(990U));
  (void)qa_run_until(SIDETONE_STATE_STEADY, 0U);
  (void)QA_CHECK(fake_sidetone_pwm_compare_speaker() == qa_compare(990U, SIDETONE_RAMP_STEPS));
  qa_end();
}

/* AC-13: a plug change in each position keeps the route until OFF; the next start uses the new route. */
static void test_plug_change_while_sounding(void) {
  size_t number;
  uint8_t direction;

  for (number = 2U; number < QA_POSITION_COUNT; ++number) {
    for (direction = 0U; direction < 2U; ++direction) {
      const qa_position_t position = qa_position(number);
      const bool is_old_headphones = (direction != 0U);
      const uint8_t old_eno = is_old_headphones ? QA_ENO_HEADPHONES : QA_ENO_SPEAKER;
      const uint8_t new_eno = is_old_headphones ? QA_ENO_SPEAKER : QA_ENO_HEADPHONES;
      const uint8_t new_kind =
          is_old_headphones ? FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER : FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES;
      size_t calls;

      qa_set_context("plug change at position", (unsigned long)number);
      qa_setup(600U, is_old_headphones);
      qa_drive(position.state, position.index);
      qa_set_plug(!is_old_headphones);
      qa_poll(5000U);
      qa_service();
      qa_poll(5000U + SIDETONE_PLUG_DEBOUNCE_MS);
      (void)QA_CHECK(model.is_headphones_requested == !is_old_headphones);

      /* The old route stays to the end of this tone, also when the request comes back. */
      qa_on(true);
      for (calls = 0U; calls < 60U; ++calls) {
        if (model.state == SIDETONE_STATE_OFF) {
          break;
        }
        if (calls == 3U) {
          qa_on(false);
        }
        qa_service();
        (void)QA_CHECK(qa_last_call_count(new_kind) == 0U);
        if (model.state != SIDETONE_STATE_OFF) {
          (void)QA_CHECK(fake_sidetone_pwm_eno() == old_eno);
        }
        if ((calls % 4U) == 3U) {
          qa_update_event();
        }
      }
      (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_OFF);
      qa_on(true);
      qa_service();
      (void)QA_CHECK(fake_sidetone_pwm_eno() == new_eno);
      (void)QA_CHECK(qa_last_call_count(new_kind) == 1U);
      (void)qa_run_until(SIDETONE_STATE_STEADY, 0U);
      qa_end();
    }
  }

  /* A change shorter than the debounce, in every position, changes nothing. */
  for (number = 2U; number < QA_POSITION_COUNT; ++number) {
    const qa_position_t position = qa_position(number);
    qa_set_context("short plug glitch at position", (unsigned long)number);
    qa_setup(600U, false);
    qa_drive(position.state, position.index);
    qa_set_plug(true);
    qa_poll(7000U);
    qa_poll(7000U + SIDETONE_PLUG_DEBOUNCE_MS - 1U);
    qa_set_plug(false);
    qa_poll(7000U + SIDETONE_PLUG_DEBOUNCE_MS);
    qa_poll(9000U);
    (void)QA_CHECK(!model.is_headphones_requested);
    qa_on(false);
    (void)qa_run_until(SIDETONE_STATE_OFF, 3U);
    qa_on(true);
    qa_service();
    (void)QA_CHECK(fake_sidetone_pwm_eno() == QA_ENO_SPEAKER);
    qa_end();
  }
}

/* AC-5, AC-6: ZERO_WAIT ends at the flag or after exactly SIDETONE_ZERO_WAIT_LIMIT calls without it. */
static void test_zero_wait_limit_without_update_flag(void) {
  uint8_t entry;
  uint8_t route;
  uint8_t flag_call;

  for (entry = 0U; entry < 3U; ++entry) {
    for (route = 0U; route < 2U; ++route) {
      size_t calls;
      qa_set_context("zero wait limit, entry", entry);
      qa_setup(400U, route != 0U);
      qa_on(true);
      if (entry == 0U) {
        qa_service(); /* off right after the start: UG set the flag, the clear removes it */
      } else if (entry == 1U) {
        qa_services(1U + 5U);
        qa_update_event(); /* a carrier update during the ramp leaves a stale flag */
      } else {
        qa_services(1U + SIDETONE_RAMP_STEPS);
        qa_update_event();
      }
      qa_on(false);
      (void)qa_run_until(SIDETONE_STATE_ZERO_WAIT, 0U);
      (void)QA_CHECK(!fake_sidetone_pwm_is_update_flag_set());
      qa_on(true); /* ignored in ZERO_WAIT */
      for (calls = 1U; calls <= SIDETONE_ZERO_WAIT_LIMIT; ++calls) {
        qa_service();
        (void)QA_CHECK(qa_last_call_count(FAKE_SIDETONE_PWM_EVENT_READ_UPDATE_FLAG) == 1U);
        if (calls < SIDETONE_ZERO_WAIT_LIMIT) {
          (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_ZERO_WAIT);
          (void)QA_CHECK(qa_last_call_count(FAKE_SIDETONE_PWM_EVENT_SET_ENO) == 0U);
          (void)QA_CHECK(sidetone_is_busy());
        }
      }
      (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_OFF);
      (void)QA_CHECK(fake_sidetone_pwm_eno() == QA_ENO_OFF);
      (void)QA_CHECK(qa_last_call_count(FAKE_SIDETONE_PWM_EVENT_SET_ENO) == 1U);
      /* The next call starts again. */
      qa_service();
      (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_RISING);
      qa_end();
    }
  }

  /* The flag at wait call k ends the wait at call k. */
  for (flag_call = 1U; flag_call <= SIDETONE_ZERO_WAIT_LIMIT; ++flag_call) {
    size_t calls;
    qa_set_context("zero wait flag at call", flag_call);
    qa_setup(1000U, false);
    qa_drive(SIDETONE_STATE_ZERO_WAIT, 0U);
    for (calls = 1U; calls < flag_call; ++calls) {
      qa_service();
    }
    qa_update_event();
    qa_service();
    (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_OFF);
    (void)QA_CHECK(!sidetone_is_busy());
    qa_end();
  }

  /* With no update event at all, many tones in a row still start and stop. */
  {
    size_t round;
    qa_set_context("no update events at all", 0U);
    qa_setup(600U, true);
    for (round = 0U; round < 20U; ++round) {
      qa_on(true);
      (void)qa_run_until(SIDETONE_STATE_STEADY, 0U);
      qa_on(false);
      (void)QA_CHECK(qa_run_until(SIDETONE_STATE_OFF, 0U) == (size_t)(SIDETONE_RAMP_STEPS + SIDETONE_ZERO_WAIT_LIMIT));
    }
    qa_end();
  }
}

static qa_plan_t qa_plan_single(uint8_t point, uint8_t services, bool is_update) {
  qa_plan_t plan;
  uint8_t p;
  for (p = 0U; p < QA_POINTS_PER_WRITE; ++p) {
    plan.services[p] = 0U;
    plan.is_update[p] = false;
  }
  plan.services[point] = services;
  plan.is_update[point] = is_update;
  return plan;
}

/* Drives a sounding or silent module to the end: new tone in STEADY with the request on. */
static void qa_finish_with_tone(uint16_t hz) {
  size_t calls;
  qa_on(true);
  for (calls = 0U; (calls < 400U) && !((model.state == SIDETONE_STATE_STEADY) && !model.is_pending); ++calls) {
    if ((calls % 6U) == 5U) {
      qa_update_event();
    }
    qa_service();
  }
  (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_STEADY);
  (void)QA_CHECK(fake_sidetone_pwm_arr() == qa_arr(hz));
  (void)QA_CHECK((fake_sidetone_pwm_compare_speaker() | fake_sidetone_pwm_compare_headphones()) ==
                 qa_compare(hz, SIDETONE_RAMP_STEPS));
}

/* AC-15: the interrupt at every preemption point of the writer, in each position. */
static void test_mailbox_preempted_at_every_point(void) {
  static const uint8_t service_counts[3] = {1U, 2U, 45U};
  size_t number;
  uint8_t point;
  size_t c;
  uint8_t round;

  for (number = 0U; number < QA_POSITION_COUNT; ++number) {
    for (point = 0U; point < QA_POINTS_PER_WRITE; ++point) {
      for (c = 0U; c < 3U; ++c) {
        const qa_position_t position = qa_position(number);
        const qa_plan_t plan = qa_plan_single(point, service_counts[c], (c == 1U));
        qa_set_context("mailbox position * 1000 + point", (unsigned long)(number * 1000U + point));
        qa_setup(600U, (number % 2U) != 0U);
        qa_drive(position.state, position.index);
        if (position.state == SIDETONE_STATE_FALLING || position.state == SIDETONE_STATE_ZERO_WAIT) {
          qa_on((c == 2U)); /* also a request that comes back during the write */
        }
        qa_set_tone(800U, &plan);
        qa_finish_with_tone(800U);
        qa_end();
      }
    }
  }

  /* The interrupt at every point at once, twice in a row with two tones, from a sounding tone. */
  for (round = 0U; round < 2U; ++round) {
    qa_plan_t plan = qa_plan_single(0U, 1U, false);
    for (point = 0U; point < QA_POINTS_PER_WRITE; ++point) {
      plan.services[point] = (uint8_t)(1U + (point % 3U));
      plan.is_update[point] = ((point % 5U) == round);
    }
    qa_set_context("mailbox every point, round", round);
    qa_setup(600U, round != 0U);
    qa_drive(SIDETONE_STATE_STEADY, SIDETONE_RAMP_STEPS);
    qa_set_tone(800U, &plan);
    qa_set_tone(1000U, &plan);
    qa_finish_with_tone(1000U);
    qa_set_tone(400U, &plan);
    qa_set_tone(600U, &plan);
    qa_finish_with_tone(600U);
    qa_end();
  }

  /* Two writes before the interrupt uses the first: the first tone never reaches the hardware. */
  {
    size_t i;
    const qa_plan_t idle_plan = qa_plan_single(17U, 3U, false);
    qa_set_context("two writes before use", 0U);
    qa_setup(600U, false);
    qa_set_tone(800U, &idle_plan);
    qa_set_tone(1000U, &idle_plan);
    qa_on(true);
    qa_service();
    (void)QA_CHECK(fake_sidetone_pwm_arr() == qa_arr(1000U));
    qa_finish_with_tone(1000U);
    for (i = 0U; i < fake_sidetone_pwm_event_count(); ++i) {
      const fake_sidetone_pwm_event_t event = fake_sidetone_pwm_event_at(i);
      (void)QA_CHECK(!((event.kind == FAKE_SIDETONE_PWM_EVENT_SET_ARR) && (event.value == qa_arr(800U))));
    }
    qa_end();
  }
}

/* AC-10, AC-11: tone math, clamping and representability over the whole uint16_t range. */
static void qa_check_table_through_rise(uint16_t hz, bool is_init_path) {
  uint8_t k;
  uint16_t previous = 0U;
  const uint16_t clamped = qa_clamp(hz);

  if (is_init_path) {
    qa_setup(hz, false);
  } else {
    qa_setup((clamped == 1000U) ? 400U : 1000U, false);
    qa_set_tone(hz, NULL);
  }
  qa_on(true);
  qa_service();
  (void)QA_CHECK(fake_sidetone_pwm_arr() == qa_arr(clamped));
  for (k = 1U; k <= SIDETONE_RAMP_STEPS; ++k) {
    uint32_t value;
    qa_service();
    value = qa_last_call_compare();
    (void)QA_CHECK(value == qa_compare(clamped, k));
    (void)QA_CHECK(value > previous);                                       /* strictly increasing */
    (void)QA_CHECK(value <= ((uint32_t)fake_sidetone_pwm_arr() + 1U) / 2U); /* at most 50 % */
    previous = (uint16_t)value;
  }
  qa_end();
}

static void test_tone_math_and_clamping(void) {
  uint32_t hz;
  static const uint16_t edges[10] = {0U, 1U, 399U, 400U, 600U, 1000U, 1001U, 5000U, 32768U, 65535U};
  size_t i;

  /* Every tone of the range: period error below half a count, exact 50 % at the top. */
  for (hz = 400U; hz <= 1000U; ++hz) {
    const uint32_t period = qa_period((uint16_t)hz);
    const uint32_t counts = period * hz;
    const uint32_t error =
        (counts > QA_COUNTER_HZ) ? (uint32_t)(counts - QA_COUNTER_HZ) : (uint32_t)(QA_COUNTER_HZ - counts);
    const uint32_t error_x2 = (uint32_t)(2U * error);
    qa_set_context("table at Hz", hz);
    (void)QA_CHECK(error_x2 <= hz);
    (void)QA_CHECK(qa_compare((uint16_t)hz, SIDETONE_RAMP_STEPS) == (uint16_t)(period / 2U));
    (void)QA_CHECK(qa_compare((uint16_t)hz, 1U) >= 12U);
    qa_check_table_through_rise((uint16_t)hz, (hz % 2U) == 0U);
  }

  qa_set_context("reference values", 0U);
  (void)QA_CHECK(qa_arr(400U) == 29999U);
  (void)QA_CHECK(qa_compare(400U, 32U) == 15000U);
  (void)QA_CHECK(qa_arr(600U) == 19999U);
  (void)QA_CHECK(qa_compare(600U, 16U) == 5000U);
  (void)QA_CHECK(qa_arr(1000U) == 11999U);
  (void)QA_CHECK(qa_compare(1000U, 1U) == 12U);

  for (i = 0U; i < 10U; ++i) {
    qa_set_context("edge Hz", edges[i]);
    qa_check_table_through_rise(edges[i], true);
    qa_check_table_through_rise(edges[i], false);
  }
  for (hz = 0U; hz <= 65535U; hz += 97U) {
    qa_set_context("sweep Hz", hz);
    qa_check_table_through_rise((uint16_t)hz, (hz % 2U) != 0U);
  }

  qa_set_context("settings range", 0U);
  (void)QA_CHECK(settings_min(SETTINGS_ID_TONE_HZ) == SIDETONE_TONE_MIN_HZ);
  (void)QA_CHECK(settings_max(SETTINGS_ID_TONE_HZ) == SIDETONE_TONE_MAX_HZ);
  (void)QA_CHECK(SIDETONE_ZERO_WAIT_LIMIT == 12U);
  (void)QA_CHECK(SIDETONE_TIMER0_RELOAD == 0xFE0CU);
}

/* Observes the debounced route with one start and stop. */
static uint8_t qa_route_by_start(void) {
  uint8_t eno;
  qa_on(true);
  qa_service();
  eno = fake_sidetone_pwm_eno();
  qa_on(false);
  qa_service();
  qa_update_event();
  qa_service();
  (void)QA_CHECK(sidetone_state() == SIDETONE_STATE_OFF);
  return eno;
}

/* AC-13: the debounce takes exactly SIDETONE_PLUG_DEBOUNCE_MS for a change at every tick value. */
static void test_plug_debounce_at_every_tick(void) {
  uint32_t start;
  bool is_headphones = false;
  uint8_t k;

  qa_setup(600U, false);
  for (start = 0U; start <= 65535U; ++start) {
    const uint16_t s = (uint16_t)start;
    qa_set_context("debounce start tick", start);
    is_headphones = !is_headphones;
    qa_set_plug(is_headphones);
    qa_poll(s);
    qa_poll((uint16_t)(s + SIDETONE_PLUG_DEBOUNCE_MS - 1U));
    (void)QA_CHECK(qa_route_by_start() == (is_headphones ? QA_ENO_SPEAKER : QA_ENO_HEADPHONES));
    qa_poll((uint16_t)(s + SIDETONE_PLUG_DEBOUNCE_MS));
    (void)QA_CHECK(qa_route_by_start() == (is_headphones ? QA_ENO_HEADPHONES : QA_ENO_SPEAKER));
  }
  qa_end();

  /* A poll on every ms across the wrap: the route changes at exactly +SIDETONE_PLUG_DEBOUNCE_MS. */
  for (k = 0U; k < 26U; ++k) {
    const uint16_t first = (uint16_t)(65480U + 3U * k);
    uint16_t offset;
    qa_set_context("debounce poll every ms from", first);
    qa_setup(600U, true);
    qa_set_plug(false);
    for (offset = 0U; offset <= SIDETONE_PLUG_DEBOUNCE_MS + 5U; ++offset) {
      qa_poll((uint16_t)(first + offset));
      (void)QA_CHECK(qa_route_by_start() ==
                     ((offset >= SIDETONE_PLUG_DEBOUNCE_MS) ? QA_ENO_SPEAKER : QA_ENO_HEADPHONES));
    }
    qa_end();
  }

  /* The init sample is taken as the route at once; the first poll after it does not reset it. */
  qa_set_context("init sample", 0U);
  qa_setup(600U, true);
  qa_poll(0U);
  (void)QA_CHECK(qa_route_by_start() == QA_ENO_HEADPHONES);
  qa_end();
}

/* Random sequences of all inputs, in lockstep with the model. */
static uint32_t qa_random_state = 0x12345678UL;

static uint32_t qa_random(uint32_t limit) {
  qa_random_state = qa_random_state * 1664525UL + 1013904223UL;
  return (qa_random_state >> 8) % limit;
}

static void test_random_lockstep(void) {
  uint32_t step;
  uint16_t now = 64000U;
  bool is_headphones = false;
  qa_plan_t plan;

  qa_setup(600U, false);
  for (step = 0U; step < 400000UL; ++step) {
    const uint32_t action = qa_random(1000U);
    qa_set_context("random step", step);
    if (action < 600U) {
      qa_service();
    } else if (action < 700U) {
      qa_update_event();
    } else if (action < 760U) {
      qa_on(qa_random(2U) != 0U);
    } else if (action < 800U) {
      uint8_t p;
      for (p = 0U; p < QA_POINTS_PER_WRITE; ++p) {
        plan.services[p] = (qa_random(8U) == 0U) ? (uint8_t)qa_random(4U) : 0U;
        plan.is_update[p] = (qa_random(16U) == 0U);
      }
      qa_set_tone((uint16_t)(300U + qa_random(800U)), (qa_random(2U) != 0U) ? &plan : NULL);
    } else if (action < 810U) {
      qa_set_tone(model.requested_hz, NULL);
    } else if (action < 830U) {
      is_headphones = !is_headphones;
      qa_set_plug(is_headphones);
    } else if (action < 980U) {
      now = (uint16_t)(now + qa_random(30U));
      qa_poll(now);
    } else {
      qa_services(1U + qa_random(40U));
    }
  }
  qa_end();
}

int main(void) {
  TEST_RUN(test_reversal_at_every_index);
  TEST_RUN(test_retune_in_every_state);
  TEST_RUN(test_plug_change_while_sounding);
  TEST_RUN(test_zero_wait_limit_without_update_flag);
  TEST_RUN(test_mailbox_preempted_at_every_point);
  TEST_RUN(test_tone_math_and_clamping);
  TEST_RUN(test_plug_debounce_at_every_tick);
  TEST_RUN(test_random_lockstep);
  printf("service calls: %zu\n", service_calls);
  return TEST_FINISH();
}
