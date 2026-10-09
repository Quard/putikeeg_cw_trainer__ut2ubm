/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>
#include <stdint.h>

#include "board/sidetone_config.h"
#include "fake_sidetone_pwm.h"
#include "fake_tick.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"
#include "support/test_assert.h"

/* Independent copy of the duty table of references/internal/sidetone_spec.md section 6.1. */
static const uint16_t duty_permille[SIDETONE_RAMP_STEPS + 1U] = {
    0U,   1U,   5U,   11U,  19U,  30U,  42U,  57U,  73U,  91U,  111U, 132U, 154U, 177U, 201U, 225U, 250U,
    275U, 299U, 323U, 346U, 368U, 389U, 409U, 427U, 443U, 458U, 470U, 481U, 489U, 495U, 499U, 500U};

#define COUNTER_HZ 12000000UL
#define START_TICK_MS 1000U

static uint32_t expected_period(uint16_t hz) { return (COUNTER_HZ + hz / 2U) / hz; }

static uint16_t expected_arr(uint16_t hz) { return (uint16_t)(expected_period(hz) - 1U); }

static uint16_t expected_compare(uint16_t hz, uint8_t index) {
  return (uint16_t)((expected_period(hz) * duty_permille[index]) / (uint32_t)1000U);
}

static void service(size_t count) {
  size_t i;
  for (i = 0U; i < count; ++i) {
    sidetone_service();
  }
}

/* A loop bound well above any envelope path, so a broken state machine fails instead of hanging the test. */
#define MAX_SERVICE_CALLS 200U

static void setup(uint16_t hz, bool is_headphones) {
  fake_sidetone_pwm_reset();
  fake_tick_reset(START_TICK_MS);
  fake_sidetone_pwm_set_headphones(is_headphones);
  sidetone_init(hz);
  fake_sidetone_pwm_clear_log();
}

/* Start call plus 32 rise calls. */
static void rise_to_steady(void) {
  sidetone_set_on(true);
  service(SIDETONE_RAMP_STEPS + 1U);
}

/* Off, 32 fall calls, one update event and the call that disables the output. */
static void stop_to_off(void) {
  sidetone_set_on(false);
  service(SIDETONE_RAMP_STEPS);
  fake_sidetone_pwm_update_event();
  sidetone_service();
}

static size_t count_events(uint8_t kind) {
  size_t count = 0U;
  size_t i;
  for (i = 0U; i < fake_sidetone_pwm_event_count(); ++i) {
    if (fake_sidetone_pwm_event_at(i).kind == kind) {
      ++count;
    }
  }
  return count;
}

static fake_sidetone_pwm_event_t event_at(size_t index) { return fake_sidetone_pwm_event_at(index); }

static bool is_compare_kind(uint8_t kind) {
  return (kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER) || (kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES);
}

/* Returns the tone whose ARR is @p arr, or 0. */
static uint16_t tone_of_arr(uint16_t arr) {
  uint16_t hz;
  for (hz = SIDETONE_TONE_MIN_HZ; hz <= SIDETONE_TONE_MAX_HZ; ++hz) {
    if (expected_arr(hz) == arr) {
      return hz;
    }
  }
  return 0U;
}

/*
 * Walks the log as a model of the envelope: a start is compare 0 followed by UG; every other compare write is one
 * table step from the previous index, in the table of the tone that the last ARR selected.
 */
static void check_ramp_log(uint16_t initial_hz) {
  uint16_t hz = initial_hz;
  int index = 0;
  size_t i;

  for (i = 0U; i < fake_sidetone_pwm_event_count(); ++i) {
    const fake_sidetone_pwm_event_t event = event_at(i);
    if (event.kind == FAKE_SIDETONE_PWM_EVENT_SET_ARR) {
      hz = tone_of_arr(event.value);
      if (!TEST_ASSERT(hz != 0U)) {
        return;
      }
    } else if (is_compare_kind(event.kind)) {
      const bool is_start = (event.value == 0U) && (event_at(i + 1U).kind == FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE);
      if (is_start) {
        index = 0;
      } else if ((index < (int)SIDETONE_RAMP_STEPS) && (event.value == expected_compare(hz, (uint8_t)(index + 1)))) {
        ++index;
      } else if ((index > 0) && (event.value == expected_compare(hz, (uint8_t)(index - 1)))) {
        --index;
      } else {
        TEST_ASSERT_EQUAL(0xFFFFU, event.value); /* a jump in the ramp */
      }
    }
  }
}

static void end_test(void) { TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_violation_count()); }

/* AC-1 */
static void test_init_is_silent_and_ordered(void) {
  size_t i;
  size_t order = 0U;
  const uint8_t expected_kinds[3] = {FAKE_SIDETONE_PWM_EVENT_CONFIGURE, FAKE_SIDETONE_PWM_EVENT_PINS_CONFIGURE,
                                     FAKE_SIDETONE_PWM_EVENT_TIMER_START};

  fake_sidetone_pwm_reset();
  fake_tick_reset(START_TICK_MS);
  sidetone_init(600U);

  TEST_ASSERT_EQUAL(1U, fake_sidetone_pwm_configure_count());
  TEST_ASSERT_EQUAL(19999U, fake_sidetone_pwm_configured_arr());
  TEST_ASSERT_EQUAL(1U, fake_sidetone_pwm_pins_configure_count());
  TEST_ASSERT_EQUAL(1U, fake_sidetone_pwm_timer_start_count());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_writes_before_configure());
  for (i = 0U; i < fake_sidetone_pwm_event_count(); ++i) {
    const uint8_t kind = event_at(i).kind;
    if (kind == FAKE_SIDETONE_PWM_EVENT_PREEMPTION_POINT) {
      continue;
    }
    TEST_ASSERT(order < 3U);
    if (order < 3U) {
      TEST_ASSERT_EQUAL(expected_kinds[order], kind);
    }
    ++order;
  }
  TEST_ASSERT_EQUAL(3U, order);
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_eno());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_compare_speaker());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_compare_headphones());
  TEST_ASSERT_EQUAL(1U, fake_sidetone_pwm_plug_read_count());
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT(!sidetone_is_busy());

  /* Without a request the service does only the request test. */
  fake_sidetone_pwm_clear_log();
  service(100U);
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_event_count());
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  end_test();
}

/* AC-2 */
static void test_full_rise_at_1000hz_writes_duty_table(void) {
  size_t i;
  size_t compare_count = 0U;
  uint16_t previous = 0U;

  setup(1000U, false);
  rise_to_steady();
  for (i = 0U; i < fake_sidetone_pwm_event_count(); ++i) {
    const fake_sidetone_pwm_event_t event = event_at(i);
    if (event.kind != FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER) {
      continue;
    }
    TEST_ASSERT(compare_count <= SIDETONE_RAMP_STEPS);
    if (compare_count <= SIDETONE_RAMP_STEPS) {
      TEST_ASSERT_EQUAL(12U * duty_permille[compare_count], event.value);
    }
    if (compare_count > 0U) {
      TEST_ASSERT(event.value > previous);
    }
    previous = event.value;
    ++compare_count;
  }
  TEST_ASSERT_EQUAL(SIDETONE_RAMP_STEPS + 1U, compare_count);
  TEST_ASSERT_EQUAL(6000U, fake_sidetone_pwm_compare_speaker());
  TEST_ASSERT_EQUAL(SIDETONE_STATE_STEADY, sidetone_state());
  end_test();
}

/* AC-3 */
static void test_rise_timing(void) {
  uint8_t index;

  setup(600U, false);
  sidetone_set_on(true);
  sidetone_service();
  TEST_ASSERT_EQUAL(3U, fake_sidetone_pwm_event_count());
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER, event_at(0U).kind);
  TEST_ASSERT_EQUAL(0U, event_at(0U).value);
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE, event_at(1U).kind);
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_SET_ENO, event_at(2U).kind);
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, event_at(2U).value);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_RISING, sidetone_state());

  for (index = 1U; index <= SIDETONE_RAMP_STEPS; ++index) {
    fake_sidetone_pwm_clear_log();
    sidetone_service();
    TEST_ASSERT_EQUAL(1U, fake_sidetone_pwm_event_count());
    TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER, event_at(0U).kind);
    TEST_ASSERT_EQUAL(expected_compare(600U, index), event_at(0U).value);
    TEST_ASSERT_EQUAL((index == SIDETONE_RAMP_STEPS) ? SIDETONE_STATE_STEADY : SIDETONE_STATE_RISING, sidetone_state());
  }
  TEST_ASSERT_EQUAL(10000U, fake_sidetone_pwm_compare_speaker());

  fake_sidetone_pwm_clear_log();
  service(50U);
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_event_count());
  TEST_ASSERT_EQUAL(SIDETONE_STATE_STEADY, sidetone_state());
  end_test();
}

/* AC-4 */
static void test_fall_timing(void) {
  int index;

  setup(600U, false);
  rise_to_steady();
  sidetone_set_on(false);
  for (index = (int)SIDETONE_RAMP_STEPS - 1; index >= 0; --index) {
    fake_sidetone_pwm_clear_log();
    sidetone_service();
    TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER, event_at(0U).kind);
    TEST_ASSERT_EQUAL(expected_compare(600U, (uint8_t)index), event_at(0U).value);
    if (index == 0) {
      TEST_ASSERT_EQUAL(2U, fake_sidetone_pwm_event_count());
      TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_CLEAR_UPDATE_FLAG, event_at(1U).kind);
      TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
    } else {
      TEST_ASSERT_EQUAL(1U, fake_sidetone_pwm_event_count());
      TEST_ASSERT_EQUAL(SIDETONE_STATE_FALLING, sidetone_state());
    }
  }
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, fake_sidetone_pwm_eno());
  end_test();
}

/* AC-5 */
static void test_zero_wait_ends_at_update_event(void) {
  size_t calls;

  setup(600U, false);
  sidetone_set_on(true);
  sidetone_service();
  /* Update events during the ramp load real non-zero shadow compares. */
  for (calls = 0U; (calls < MAX_SERVICE_CALLS) && (sidetone_state() != SIDETONE_STATE_STEADY); ++calls) {
    sidetone_service();
    fake_sidetone_pwm_update_event();
  }
  TEST_ASSERT_EQUAL(10000U, fake_sidetone_pwm_shadow_compare_speaker());
  sidetone_set_on(false);
  service(SIDETONE_RAMP_STEPS);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());

  service(5U);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  TEST_ASSERT(sidetone_is_busy());
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, fake_sidetone_pwm_eno());

  fake_sidetone_pwm_update_event();
  fake_sidetone_pwm_clear_log();
  sidetone_service();
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_OFF, fake_sidetone_pwm_eno());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_shadow_compare_speaker());
  TEST_ASSERT_EQUAL(1U, count_events(FAKE_SIDETONE_PWM_EVENT_SET_ENO));
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT(!sidetone_is_busy());
  end_test();
}

/* AC-6 */
static void test_zero_wait_limit_without_update_event(void) {
  uint8_t call;

  setup(400U, false);
  rise_to_steady();
  sidetone_set_on(false);
  service(SIDETONE_RAMP_STEPS);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  TEST_ASSERT_EQUAL(12U, SIDETONE_ZERO_WAIT_LIMIT);

  fake_sidetone_pwm_clear_log();
  for (call = 1U; call < SIDETONE_ZERO_WAIT_LIMIT; ++call) {
    sidetone_service();
    TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  }
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, fake_sidetone_pwm_eno());
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_OFF, fake_sidetone_pwm_eno());
  TEST_ASSERT_EQUAL(SIDETONE_ZERO_WAIT_LIMIT, count_events(FAKE_SIDETONE_PWM_EVENT_READ_UPDATE_FLAG));

  /* The next start works again. */
  sidetone_set_on(true);
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_RISING, sidetone_state());
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, fake_sidetone_pwm_eno());
  end_test();
}

/* AC-7, rising: off at each index 1..31 steps down once, on again steps up once. */
static void test_reversal_in_rising(void) {
  uint8_t index;

  for (index = 1U; index < SIDETONE_RAMP_STEPS; ++index) {
    setup(600U, false);
    sidetone_set_on(true);
    service(1U + index);
    TEST_ASSERT_EQUAL(SIDETONE_STATE_RISING, sidetone_state());
    TEST_ASSERT_EQUAL(expected_compare(600U, index), fake_sidetone_pwm_compare_speaker());

    sidetone_set_on(false);
    sidetone_service();
    TEST_ASSERT_EQUAL(expected_compare(600U, (uint8_t)(index - 1U)), fake_sidetone_pwm_compare_speaker());
    if (index == 1U) {
      TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
    } else {
      TEST_ASSERT_EQUAL(SIDETONE_STATE_FALLING, sidetone_state());
      sidetone_set_on(true);
      sidetone_service();
      TEST_ASSERT_EQUAL(expected_compare(600U, index), fake_sidetone_pwm_compare_speaker());
      TEST_ASSERT_EQUAL(SIDETONE_STATE_RISING, sidetone_state());
      /* The rise continues to STEADY in the remaining steps. */
      service(SIDETONE_RAMP_STEPS - index);
      TEST_ASSERT_EQUAL(SIDETONE_STATE_STEADY, sidetone_state());
    }
    check_ramp_log(600U);
    end_test();
  }
}

/* AC-7, falling and STEADY. */
static void test_reversal_in_falling_and_steady(void) {
  uint8_t index;

  for (index = 1U; index < SIDETONE_RAMP_STEPS; ++index) {
    setup(800U, true);
    rise_to_steady();
    sidetone_set_on(false);
    service(SIDETONE_RAMP_STEPS - index);
    TEST_ASSERT_EQUAL(SIDETONE_STATE_FALLING, sidetone_state());
    TEST_ASSERT_EQUAL(expected_compare(800U, index), fake_sidetone_pwm_compare_headphones());

    sidetone_set_on(true);
    sidetone_service();
    TEST_ASSERT_EQUAL(expected_compare(800U, (uint8_t)(index + 1U)), fake_sidetone_pwm_compare_headphones());
    TEST_ASSERT_EQUAL((index + 1U == SIDETONE_RAMP_STEPS) ? SIDETONE_STATE_STEADY : SIDETONE_STATE_RISING,
                      sidetone_state());

    sidetone_set_on(false);
    sidetone_service();
    TEST_ASSERT_EQUAL(expected_compare(800U, index), fake_sidetone_pwm_compare_headphones());
    TEST_ASSERT_EQUAL(SIDETONE_STATE_FALLING, sidetone_state());
    check_ramp_log(800U);
    TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER));
    end_test();
  }

  setup(800U, false);
  rise_to_steady();
  sidetone_set_on(false);
  sidetone_service();
  TEST_ASSERT_EQUAL(expected_compare(800U, 31U), fake_sidetone_pwm_compare_speaker());
  sidetone_set_on(true);
  sidetone_service();
  TEST_ASSERT_EQUAL(expected_compare(800U, 32U), fake_sidetone_pwm_compare_speaker());
  TEST_ASSERT_EQUAL(SIDETONE_STATE_STEADY, sidetone_state());
  check_ramp_log(800U);
  end_test();
}

/* AC-7: off right after the start call. */
static void test_off_right_after_start(void) {
  setup(600U, false);
  sidetone_set_on(true);
  sidetone_service();
  sidetone_set_on(false);
  fake_sidetone_pwm_clear_log();
  sidetone_service();
  TEST_ASSERT_EQUAL(1U, fake_sidetone_pwm_event_count());
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_CLEAR_UPDATE_FLAG, event_at(0U).kind);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  fake_sidetone_pwm_update_event();
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_OFF, fake_sidetone_pwm_eno());
  TEST_ASSERT(!sidetone_is_busy());
  end_test();
}

/* AC-8: ZERO_WAIT ignores the on request; the restart comes on the call after OFF. */
static void test_restart_in_zero_wait(void) {
  setup(600U, false);
  rise_to_steady();
  sidetone_set_on(false);
  service(SIDETONE_RAMP_STEPS);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());

  sidetone_set_on(true);
  fake_sidetone_pwm_clear_log();
  service(3U);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER));
  fake_sidetone_pwm_update_event();
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_OFF, fake_sidetone_pwm_eno());
  TEST_ASSERT(sidetone_is_busy());

  fake_sidetone_pwm_clear_log();
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_RISING, sidetone_state());
  TEST_ASSERT_EQUAL(1U, count_events(FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE));
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_SET_ARR));
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, fake_sidetone_pwm_eno());
  end_test();
}

/* AC-8: a stop in RISING and in STEADY falls from the current index and ends OFF. */
static void test_stop_in_rising_and_steady(void) {
  setup(600U, false);
  sidetone_set_on(true);
  service(11U);
  stop_to_off();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT(!sidetone_is_busy());
  check_ramp_log(600U);
  rise_to_steady();
  stop_to_off();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT_EQUAL(2U, fake_sidetone_pwm_update_count());
  check_ramp_log(600U);
  end_test();
}

/* AC-9: retune while sounding. */
static void test_retune_in_steady(void) {
  size_t i;

  setup(600U, false);
  rise_to_steady();
  fake_sidetone_pwm_clear_log();
  sidetone_set_tone_hz(800U);
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_SET_ARR));
  TEST_ASSERT_EQUAL(fake_sidetone_pwm_event_count(), count_events(FAKE_SIDETONE_PWM_EVENT_PREEMPTION_POINT));
  TEST_ASSERT(sidetone_is_busy());

  /* The pending tone acts as an off request: the old tone falls. */
  service(SIDETONE_RAMP_STEPS);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_SET_ARR));
  TEST_ASSERT_EQUAL(19999U, fake_sidetone_pwm_arr());
  service(2U);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  fake_sidetone_pwm_update_event();
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT(sidetone_is_busy());

  fake_sidetone_pwm_clear_log();
  sidetone_service();
  TEST_ASSERT_EQUAL(4U, fake_sidetone_pwm_event_count());
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_SET_ARR, event_at(0U).kind);
  TEST_ASSERT_EQUAL(14999U, event_at(0U).value);
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER, event_at(1U).kind);
  TEST_ASSERT_EQUAL(0U, event_at(1U).value);
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_GENERATE_UPDATE, event_at(2U).kind);
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_EVENT_SET_ENO, event_at(3U).kind);
  TEST_ASSERT_EQUAL(14999U, fake_sidetone_pwm_shadow_arr());

  service(SIDETONE_RAMP_STEPS);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_STEADY, sidetone_state());
  TEST_ASSERT_EQUAL(7500U, fake_sidetone_pwm_compare_speaker());
  for (i = 1U; i < fake_sidetone_pwm_event_count(); ++i) {
    if (event_at(i).kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER) {
      TEST_ASSERT(event_at(i).value <= 7500U);
    }
  }
  check_ramp_log(800U);
  end_test();
}

/* AC-9: a retune in RISING falls from the current index; in FALLING the fall goes on. */
static void test_retune_in_rising_and_falling(void) {
  setup(600U, false);
  sidetone_set_on(true);
  service(9U); /* index 8 */
  sidetone_set_tone_hz(1000U);
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_FALLING, sidetone_state());
  TEST_ASSERT_EQUAL(expected_compare(600U, 7U), fake_sidetone_pwm_compare_speaker());
  service(7U);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  fake_sidetone_pwm_update_event();
  service(2U);
  TEST_ASSERT_EQUAL(11999U, fake_sidetone_pwm_arr());
  TEST_ASSERT_EQUAL(SIDETONE_STATE_RISING, sidetone_state());
  service(SIDETONE_RAMP_STEPS);
  TEST_ASSERT_EQUAL(6000U, fake_sidetone_pwm_compare_speaker());
  check_ramp_log(600U);
  end_test();

  setup(600U, false);
  rise_to_steady();
  sidetone_set_on(false);
  service(10U); /* FALLING, index 22 */
  sidetone_set_tone_hz(400U);
  sidetone_set_on(true);
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_FALLING, sidetone_state());
  TEST_ASSERT_EQUAL(expected_compare(600U, 21U), fake_sidetone_pwm_compare_speaker());
  service(21U);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  fake_sidetone_pwm_update_event();
  service(2U);
  TEST_ASSERT_EQUAL(29999U, fake_sidetone_pwm_arr());
  service(SIDETONE_RAMP_STEPS);
  TEST_ASSERT_EQUAL(15000U, fake_sidetone_pwm_compare_speaker());
  check_ramp_log(600U);
  end_test();
}

/* AC-9: a retune in ZERO_WAIT and in OFF is held until the next start. */
static void test_retune_in_zero_wait_and_off(void) {
  setup(600U, false);
  rise_to_steady();
  sidetone_set_on(false);
  service(SIDETONE_RAMP_STEPS);
  sidetone_set_tone_hz(990U);
  sidetone_set_on(true);
  service(2U);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  TEST_ASSERT_EQUAL(19999U, fake_sidetone_pwm_arr());
  fake_sidetone_pwm_update_event();
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  sidetone_service();
  TEST_ASSERT_EQUAL(12120U, fake_sidetone_pwm_arr());
  end_test();

  setup(600U, false);
  sidetone_set_tone_hz(605U);
  service(10U);
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_SET_ARR));
  sidetone_set_tone_hz(610U); /* the newest tone wins */
  rise_to_steady();
  TEST_ASSERT_EQUAL(19671U, fake_sidetone_pwm_arr());
  TEST_ASSERT_EQUAL(9836U, fake_sidetone_pwm_compare_speaker());
  TEST_ASSERT_EQUAL(1U, count_events(FAKE_SIDETONE_PWM_EVENT_SET_ARR));
  check_ramp_log(600U);

  /* A second retune uses the other table again. */
  stop_to_off();
  sidetone_set_tone_hz(600U);
  rise_to_steady();
  TEST_ASSERT_EQUAL(19999U, fake_sidetone_pwm_arr());
  TEST_ASSERT_EQUAL(10000U, fake_sidetone_pwm_compare_speaker());
  check_ramp_log(600U);
  end_test();
}

/* AC-10 */
static void test_tone_math(void) {
  static const uint16_t cases[7][5] = {{400U, 29999U, 30U, 7500U, 15000U}, {600U, 19999U, 20U, 5000U, 10000U},
                                       {605U, 19834U, 19U, 4958U, 9917U},  {610U, 19671U, 19U, 4918U, 9836U},
                                       {800U, 14999U, 15U, 3750U, 7500U},  {990U, 12120U, 12U, 3030U, 6060U},
                                       {1000U, 11999U, 12U, 3000U, 6000U}};
  size_t i;

  for (i = 0U; i < 7U; ++i) {
    setup(cases[i][0], false);
    TEST_ASSERT_EQUAL(cases[i][1], fake_sidetone_pwm_configured_arr());
    sidetone_set_on(true);
    service(2U);
    TEST_ASSERT_EQUAL(cases[i][2], fake_sidetone_pwm_compare_speaker());
    service(15U);
    TEST_ASSERT_EQUAL(cases[i][3], fake_sidetone_pwm_compare_speaker());
    service(16U);
    TEST_ASSERT_EQUAL(cases[i][4], fake_sidetone_pwm_compare_speaker());
    end_test();

    /* The same values through the mailbox. */
    setup((cases[i][0] == 400U) ? 1000U : 400U, false);
    sidetone_set_tone_hz(cases[i][0]);
    sidetone_set_on(true);
    service(2U);
    TEST_ASSERT_EQUAL(cases[i][1], fake_sidetone_pwm_arr());
    TEST_ASSERT_EQUAL(cases[i][2], fake_sidetone_pwm_compare_speaker());
    service(31U);
    TEST_ASSERT_EQUAL(cases[i][4], fake_sidetone_pwm_compare_speaker());
    end_test();
  }
}

/* AC-11 */
static void test_clamping(void) {
  static const uint16_t low_values[3] = {0U, 1U, 399U};
  static const uint16_t high_values[3] = {1001U, 5000U, 65535U};
  size_t i;

  TEST_ASSERT_EQUAL(settings_min(SETTINGS_ID_TONE_HZ), SIDETONE_TONE_MIN_HZ);
  TEST_ASSERT_EQUAL(settings_max(SETTINGS_ID_TONE_HZ), SIDETONE_TONE_MAX_HZ);
  for (i = 0U; i < 3U; ++i) {
    setup(low_values[i], false);
    TEST_ASSERT_EQUAL(29999U, fake_sidetone_pwm_configured_arr());
    rise_to_steady();
    TEST_ASSERT_EQUAL(15000U, fake_sidetone_pwm_compare_speaker());
    end_test();

    setup(high_values[i], false);
    TEST_ASSERT_EQUAL(11999U, fake_sidetone_pwm_configured_arr());
    rise_to_steady();
    TEST_ASSERT_EQUAL(6000U, fake_sidetone_pwm_compare_speaker());
    end_test();

    setup(600U, false);
    sidetone_set_tone_hz(low_values[i]);
    sidetone_set_on(true);
    sidetone_service();
    TEST_ASSERT_EQUAL(29999U, fake_sidetone_pwm_arr());
    end_test();

    setup(600U, false);
    sidetone_set_tone_hz(high_values[i]);
    sidetone_set_on(true);
    sidetone_service();
    TEST_ASSERT_EQUAL(11999U, fake_sidetone_pwm_arr());
    end_test();
  }
}

/* AC-12 */
static void test_same_tone_does_nothing(void) {
  size_t points;

  setup(600U, false);
  rise_to_steady();
  points = fake_sidetone_pwm_preemption_count();
  fake_sidetone_pwm_clear_log();
  sidetone_set_tone_hz(600U);
  TEST_ASSERT_EQUAL(points, fake_sidetone_pwm_preemption_count());
  service(10U);
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_event_count());
  TEST_ASSERT_EQUAL(SIDETONE_STATE_STEADY, sidetone_state());
  end_test();

  /* Equal after clamping. */
  setup(400U, false);
  rise_to_steady();
  points = fake_sidetone_pwm_preemption_count();
  sidetone_set_tone_hz(100U);
  TEST_ASSERT_EQUAL(points, fake_sidetone_pwm_preemption_count());
  sidetone_service();
  TEST_ASSERT_EQUAL(SIDETONE_STATE_STEADY, sidetone_state());

  /* A new request repeats; the repeat of the new value does nothing. */
  sidetone_set_tone_hz(700U);
  points = fake_sidetone_pwm_preemption_count();
  sidetone_set_tone_hz(700U);
  TEST_ASSERT_EQUAL(points, fake_sidetone_pwm_preemption_count());
  end_test();
}

static void poll_at(uint16_t now_ms) {
  fake_tick_set_now(now_ms);
  sidetone_poll();
}

/* AC-13: each route enables its own pin and writes only its own compare. */
static void test_route_selects_channel(void) {
  setup(600U, true);
  rise_to_steady();
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_HEADPHONES, fake_sidetone_pwm_eno());
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER));
  TEST_ASSERT_EQUAL(SIDETONE_RAMP_STEPS + 1U, count_events(FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES));
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_compare_speaker());
  TEST_ASSERT_EQUAL(10000U, fake_sidetone_pwm_compare_headphones());
  stop_to_off();
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_compare_headphones());
  end_test();

  setup(600U, false);
  rise_to_steady();
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, fake_sidetone_pwm_eno());
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES));
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_compare_headphones());
  end_test();
}

/* AC-13: a plug change during a tone waits for OFF. */
static void test_route_change_waits_for_off(void) {
  setup(600U, false);
  sidetone_set_on(true);
  service(5U);
  fake_sidetone_pwm_set_headphones(true);
  poll_at(2000U);
  poll_at(2000U + SIDETONE_PLUG_DEBOUNCE_MS);
  service(SIDETONE_RAMP_STEPS);
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, fake_sidetone_pwm_eno());
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES));
  stop_to_off();
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES));

  fake_sidetone_pwm_clear_log();
  rise_to_steady();
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_HEADPHONES, fake_sidetone_pwm_eno());
  TEST_ASSERT_EQUAL(0U, count_events(FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER));
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_compare_speaker());
  end_test();
}

/* Returns the route a start from OFF uses, as its ENO value. */
static uint8_t route_of_next_start(void) {
  uint8_t eno;
  sidetone_set_on(true);
  sidetone_service();
  eno = fake_sidetone_pwm_eno();
  sidetone_set_on(false);
  sidetone_service();
  fake_sidetone_pwm_update_event();
  sidetone_service();
  return eno;
}

/* AC-13: the debounce takes exactly SIDETONE_PLUG_DEBOUNCE_MS, also across the tick wrap. */
static void test_plug_debounce(void) {
  static const uint16_t starts[2] = {2000U, 65530U};
  size_t i;
  size_t reads;

  for (i = 0U; i < 2U; ++i) {
    const uint16_t start = starts[i];

    setup(600U, false);
    fake_sidetone_pwm_set_headphones(true);
    poll_at(start);
    poll_at((uint16_t)(start + SIDETONE_PLUG_DEBOUNCE_MS - 1U));
    TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, route_of_next_start());
    poll_at((uint16_t)(start + SIDETONE_PLUG_DEBOUNCE_MS));
    TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_HEADPHONES, route_of_next_start());

    /* A removal shorter than the debounce changes nothing. */
    fake_sidetone_pwm_set_headphones(false);
    poll_at((uint16_t)(start + 100U));
    poll_at((uint16_t)(start + 100U + SIDETONE_PLUG_DEBOUNCE_MS - 1U));
    fake_sidetone_pwm_set_headphones(true);
    poll_at((uint16_t)(start + 100U + SIDETONE_PLUG_DEBOUNCE_MS));
    poll_at((uint16_t)(start + 300U));
    TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_HEADPHONES, route_of_next_start());

    /* The bounce restarts the timer. */
    fake_sidetone_pwm_set_headphones(false);
    poll_at((uint16_t)(start + 400U));
    fake_sidetone_pwm_set_headphones(true);
    poll_at((uint16_t)(start + 420U));
    fake_sidetone_pwm_set_headphones(false);
    poll_at((uint16_t)(start + 430U));
    poll_at((uint16_t)(start + 430U + SIDETONE_PLUG_DEBOUNCE_MS - 1U));
    TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_HEADPHONES, route_of_next_start());
    poll_at((uint16_t)(start + 430U + SIDETONE_PLUG_DEBOUNCE_MS));
    TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_SPEAKER, route_of_next_start());
    end_test();
  }

  setup(600U, true);
  TEST_ASSERT_EQUAL(FAKE_SIDETONE_PWM_ENO_HEADPHONES, route_of_next_start());
  reads = fake_tick_read_count();
  sidetone_poll();
  TEST_ASSERT_EQUAL(reads + 1U, fake_tick_read_count());
  sidetone_poll();
  TEST_ASSERT_EQUAL(reads + 2U, fake_tick_read_count());
  end_test();
}

/* AC-16 */
static void test_busy(void) {
  size_t calls;

  setup(600U, false);
  TEST_ASSERT(!sidetone_is_busy());
  sidetone_set_on(true);
  TEST_ASSERT(sidetone_is_busy());
  sidetone_set_on(false);
  TEST_ASSERT(!sidetone_is_busy());

  sidetone_set_on(true);
  for (calls = 0U; (calls < MAX_SERVICE_CALLS) && (sidetone_state() != SIDETONE_STATE_STEADY); ++calls) {
    TEST_ASSERT(sidetone_is_busy());
    sidetone_service();
  }
  TEST_ASSERT_EQUAL(SIDETONE_RAMP_STEPS + 1U, calls);
  sidetone_set_on(false);
  for (calls = 0U; (calls < MAX_SERVICE_CALLS) && (sidetone_state() != SIDETONE_STATE_ZERO_WAIT); ++calls) {
    TEST_ASSERT(sidetone_is_busy());
    sidetone_service();
  }
  TEST_ASSERT_EQUAL(SIDETONE_RAMP_STEPS, calls);
  TEST_ASSERT(sidetone_is_busy());
  fake_sidetone_pwm_update_event();
  sidetone_service();
  TEST_ASSERT(!sidetone_is_busy());
  end_test();
}

/* AC-17: UG and ARR only in a start from OFF. */
static void test_update_only_at_start(void) {
  static const uint16_t tones[4] = {800U, 400U, 1000U, 650U};
  size_t starts = 0U;
  size_t round;
  size_t call;

  setup(600U, false);
  for (round = 0U; round < 4U; ++round) {
    sidetone_set_on(true);
    for (call = 0U; call < 60U; ++call) {
      const sidetone_state_t before = sidetone_state();
      const size_t updates = fake_sidetone_pwm_update_count();
      const size_t arrs = count_events(FAKE_SIDETONE_PWM_EVENT_SET_ARR);
      if (call == 20U) {
        sidetone_set_tone_hz(tones[round]);
      }
      sidetone_service();
      if (before == SIDETONE_STATE_OFF) {
        ++starts;
        TEST_ASSERT_EQUAL(updates + 1U, fake_sidetone_pwm_update_count());
      } else {
        TEST_ASSERT_EQUAL(updates, fake_sidetone_pwm_update_count());
        TEST_ASSERT_EQUAL(arrs, count_events(FAKE_SIDETONE_PWM_EVENT_SET_ARR));
      }
      if ((call % 10U) == 9U) {
        fake_sidetone_pwm_update_event();
      }
    }
    stop_to_off();
  }
  TEST_ASSERT_EQUAL(starts, fake_sidetone_pwm_update_count());
  TEST_ASSERT_EQUAL(8U, starts);
  check_ramp_log(600U);
  end_test();
}

/* AC-15 smoke test: the interrupt runs at every preemption point of the writer. */
static void service_hook(void) { sidetone_service(); }

static void test_mailbox_preempted_at_every_point(void) {
  size_t calls;

  setup(600U, false);
  sidetone_set_on(true);
  fake_sidetone_pwm_set_preemption_hook(service_hook);
  sidetone_set_tone_hz(800U);
  sidetone_set_tone_hz(1000U);
  fake_sidetone_pwm_set_preemption_hook(NULL);
  TEST_ASSERT_EQUAL(2U * 36U, fake_sidetone_pwm_preemption_count() - 33U);

  for (calls = 0U; (calls < MAX_SERVICE_CALLS) && (sidetone_state() != SIDETONE_STATE_ZERO_WAIT); ++calls) {
    sidetone_service();
  }
  TEST_ASSERT_EQUAL(SIDETONE_STATE_ZERO_WAIT, sidetone_state());
  fake_sidetone_pwm_update_event();
  service(2U);
  TEST_ASSERT_EQUAL(11999U, fake_sidetone_pwm_arr());
  service(SIDETONE_RAMP_STEPS);
  TEST_ASSERT_EQUAL(6000U, fake_sidetone_pwm_compare_speaker());
  /* The tables keep data from earlier tests, so a stale table must not pass as a valid tone. */
  for (calls = 0U; calls < fake_sidetone_pwm_event_count(); ++calls) {
    if (event_at(calls).kind == FAKE_SIDETONE_PWM_EVENT_SET_ARR) {
      TEST_ASSERT_EQUAL(11999U, event_at(calls).value);
    }
  }
  TEST_ASSERT_EQUAL(1U, count_events(FAKE_SIDETONE_PWM_EVENT_SET_ARR));
  check_ramp_log(600U);
  end_test();
}

int main(void) {
  TEST_RUN(test_init_is_silent_and_ordered);
  TEST_RUN(test_full_rise_at_1000hz_writes_duty_table);
  TEST_RUN(test_rise_timing);
  TEST_RUN(test_fall_timing);
  TEST_RUN(test_zero_wait_ends_at_update_event);
  TEST_RUN(test_zero_wait_limit_without_update_event);
  TEST_RUN(test_reversal_in_rising);
  TEST_RUN(test_reversal_in_falling_and_steady);
  TEST_RUN(test_off_right_after_start);
  TEST_RUN(test_restart_in_zero_wait);
  TEST_RUN(test_stop_in_rising_and_steady);
  TEST_RUN(test_retune_in_steady);
  TEST_RUN(test_retune_in_rising_and_falling);
  TEST_RUN(test_retune_in_zero_wait_and_off);
  TEST_RUN(test_tone_math);
  TEST_RUN(test_clamping);
  TEST_RUN(test_same_tone_does_nothing);
  TEST_RUN(test_route_selects_channel);
  TEST_RUN(test_route_change_waits_for_off);
  TEST_RUN(test_plug_debounce);
  TEST_RUN(test_busy);
  TEST_RUN(test_update_only_at_start);
  TEST_RUN(test_mailbox_preempted_at_every_point);
  return TEST_FINISH();
}
