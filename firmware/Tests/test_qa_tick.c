/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests for the portable tick helpers (src/tick/tick.c). tick_isr() is target-only, so the test plays the
 * interrupt by incrementing the counter. The double read in tick_now() cannot be interleaved on the host; the
 * generated assembly review covers it.
 */
#include "support/test_assert.h"
#include "tick/tick.h"
#include "tick_counter.h"

#define START_STRIDE 251U
#define DELTA_STRIDE 97U

static void test_now_returns_every_counter_value(void) {
  unsigned long value;
  size_t wrong = 0U;
  for (value = 0U; value <= UINT16_MAX; ++value) {
    tick_count_ms = (uint16_t)value;
    wrong += (tick_now() == (uint16_t)value) ? 0U : 1U;
  }
  TEST_ASSERT_EQUAL(0U, wrong);
}

static void test_counter_wraps_to_zero(void) {
  tick_count_ms = 0xFFFEU;
  ++tick_count_ms;
  TEST_ASSERT_EQUAL(0xFFFFU, tick_now());
  ++tick_count_ms;
  TEST_ASSERT_EQUAL(0U, tick_now());
  ++tick_count_ms;
  TEST_ASSERT_EQUAL(1U, tick_now());
}

static size_t check_elapsed(uint16_t start, uint16_t delta) {
  tick_count_ms = (uint16_t)(start + delta);
  return (tick_elapsed_since(start) == delta) ? 0U : 1U;
}

/* elapsed = now - start modulo 65536 for starts and deltas across the whole range, including every wrap. */
static void test_elapsed_is_modular(void) {
  static const uint16_t edges[] = {0U, 1U, 0x7FFFU, 0x8000U, 0xFFF0U, 0xFFFEU, 0xFFFFU};
  unsigned long start;
  unsigned long delta;
  size_t i;
  size_t j;
  size_t wrong = 0U;

  for (start = 0U; start <= UINT16_MAX; start += START_STRIDE) {
    for (delta = 0U; delta <= UINT16_MAX; delta += DELTA_STRIDE) {
      wrong += check_elapsed((uint16_t)start, (uint16_t)delta);
    }
  }
  for (i = 0U; i < sizeof(edges) / sizeof(edges[0]); ++i) {
    for (j = 0U; j < sizeof(edges) / sizeof(edges[0]); ++j) {
      wrong += check_elapsed(edges[i], edges[j]);
    }
  }
  tick_count_ms = 0x0010U;
  TEST_ASSERT_EQUAL(0x20U, tick_elapsed_since(0xFFF0U));
  TEST_ASSERT_EQUAL(0U, wrong);
}

/* For duration d: elapsed d - 1 is false, d and d + 1 are true; duration 0 is always true. */
static void test_has_elapsed_boundaries(void) {
  static const uint16_t starts[] = {0U, 1U, 0x7FFFU, 0x8000U, 0xFFF0U, 0xFFFEU, 0xFFFFU};
  static const uint16_t durations[] = {0U, 1U, 2U, 255U, 256U, 1000U, 32767U, 32768U, 65534U, 65535U};
  size_t wrong = 0U;
  size_t i;
  size_t j;

  for (i = 0U; i < sizeof(starts) / sizeof(starts[0]); ++i) {
    for (j = 0U; j < sizeof(durations) / sizeof(durations[0]); ++j) {
      const uint16_t start = starts[i];
      const uint16_t duration = durations[j];
      tick_count_ms = start;
      wrong += (tick_has_elapsed(start, duration) == (duration == 0U)) ? 0U : 1U;
      if (duration > 0U) {
        tick_count_ms = (uint16_t)(start + duration - 1U);
        wrong += tick_has_elapsed(start, duration) ? 1U : 0U;
      }
      tick_count_ms = (uint16_t)(start + duration);
      wrong += tick_has_elapsed(start, duration) ? 0U : 1U;
      if (duration < UINT16_MAX) {
        tick_count_ms = (uint16_t)(start + duration + 1U);
        wrong += tick_has_elapsed(start, duration) ? 0U : 1U;
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, wrong);
}

/* has_elapsed must agree with elapsed_since for every elapsed value at one duration. */
static void test_has_elapsed_matches_elapsed(void) {
  unsigned long elapsed;
  size_t wrong = 0U;
  for (elapsed = 0U; elapsed <= UINT16_MAX; ++elapsed) {
    tick_count_ms = (uint16_t)(0xFF00U + elapsed);
    wrong += (tick_has_elapsed(0xFF00U, 300U) == (tick_elapsed_since(0xFF00U) >= 300U)) ? 0U : 1U;
    wrong += (tick_has_elapsed(0xFF00U, 300U) == (elapsed >= 300U)) ? 0U : 1U;
  }
  TEST_ASSERT_EQUAL(0U, wrong);
}

/* The helpers only read the counter. */
static void test_helpers_do_not_write_counter(void) {
  tick_count_ms = 0xABCDU;
  (void)tick_now();
  (void)tick_elapsed_since(0x1111U);
  (void)tick_has_elapsed(0x2222U, 5U);
  TEST_ASSERT_EQUAL(0xABCDU, tick_count_ms);
  TEST_ASSERT_EQUAL(1000U, TICK_HZ);
}

int main(void) {
  TEST_RUN(test_now_returns_every_counter_value);
  TEST_RUN(test_counter_wraps_to_zero);
  TEST_RUN(test_elapsed_is_modular);
  TEST_RUN(test_has_elapsed_boundaries);
  TEST_RUN(test_has_elapsed_matches_elapsed);
  TEST_RUN(test_helpers_do_not_write_counter);
  return TEST_FINISH();
}
