/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "support/test_assert.h"
#include "tick/tick.h"
#include "tick_counter.h"

/* Plays the Timer 3 interrupt: tick_isr() is target-only, so the host test increments the counter itself. */
static void simulate_tick(void) { ++tick_count_ms; }

static void test_now_returns_counter(void) {
  tick_count_ms = 0x1234U;
  TEST_ASSERT_EQUAL(0x1234U, tick_now());
  simulate_tick();
  TEST_ASSERT_EQUAL(0x1235U, tick_now());
}

static void test_counter_wraps(void) {
  tick_count_ms = 0xFFFFU;
  TEST_ASSERT_EQUAL(0xFFFFU, tick_now());
  simulate_tick();
  TEST_ASSERT_EQUAL(0U, tick_now());
}

static void test_elapsed_across_wrap(void) {
  tick_count_ms = 0x0010U;
  TEST_ASSERT_EQUAL(0x20U, tick_elapsed_since(0xFFF0U));
  TEST_ASSERT_EQUAL(0U, tick_elapsed_since(0x0010U));
  TEST_ASSERT_EQUAL(0xFFFFU, tick_elapsed_since(0x0011U));
}

static void test_has_elapsed_boundaries(void) {
  tick_count_ms = 100U;
  TEST_ASSERT(tick_has_elapsed(100U, 0U));
  TEST_ASSERT(tick_has_elapsed(90U, 10U));
  TEST_ASSERT(!tick_has_elapsed(91U, 10U));
  TEST_ASSERT(tick_has_elapsed(89U, 10U));

  tick_count_ms = 0xFFFFU;
  TEST_ASSERT(tick_has_elapsed(0U, 65535U));
  TEST_ASSERT(!tick_has_elapsed(1U, 65535U));
  tick_count_ms = 0x0005U;
  TEST_ASSERT(tick_has_elapsed(0xFFFBU, 10U));
  TEST_ASSERT(!tick_has_elapsed(0xFFFCU, 10U));
}

int main(void) {
  TEST_RUN(test_now_returns_counter);
  TEST_RUN(test_counter_wraps);
  TEST_RUN(test_elapsed_across_wrap);
  TEST_RUN(test_has_elapsed_boundaries);
  return TEST_FINISH();
}
