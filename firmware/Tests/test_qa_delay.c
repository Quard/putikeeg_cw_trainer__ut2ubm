/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests for delay_ms() on the tick (src/delay/delay.c with fakes/fake_tick.c). Each tick_now() read returns the
 * current value and then moves it by a fixed step, so the step is the tick time that passes per loop pass.
 *
 * Oracle: the call reads the start value, then stops at the first read whose elapsed value is at least ms + 1.
 * So with R reads, (R - 1) * step >= ms + 1 and (R - 2) * step <= ms. ms + 1 ticks guarantee ms full milliseconds
 * because a tick can come right after the start read.
 */
#include "delay/delay.h"
#include "fake_tick.h"
#include "support/test_assert.h"

static const uint16_t starts[] = {0U, 1U, 0x1234U, 0x7FFFU, 0xFF00U, 0xFFF0U, 0xFFFEU, 0xFFFFU};

static size_t check_delay(uint16_t start, uint16_t step, uint8_t ms) {
  const unsigned long target = (unsigned long)ms + 1U;
  unsigned long last_elapsed;
  size_t reads;

  fake_tick_reset(start);
  fake_tick_set_advance_per_read(step);
  delay_ms(ms);
  reads = fake_tick_read_count();
  if (reads < 2U) {
    return 1U;
  }
  last_elapsed = (unsigned long)(reads - 1U) * step;
  if (last_elapsed < target) {
    return 1U;
  }
  /* The read before the last one was still short of the target. */
  return ((reads == 2U) || (((unsigned long)(reads - 2U) * step) < target)) ? 0U : 1U;
}

static void test_zero_never_reads_tick(void) {
  size_t i;
  for (i = 0U; i < sizeof(starts) / sizeof(starts[0]); ++i) {
    fake_tick_reset(starts[i]);
    fake_tick_set_advance_per_read(1U);
    delay_ms(0U);
    TEST_ASSERT_EQUAL(0U, fake_tick_read_count());
  }
}

/* Every ms 1..255 at 1, 2 and 7 ms per read, from starts that cross the counter wrap. */
static void test_every_ms_rounds_up_to_ms_plus_one(void) {
  static const uint16_t steps[] = {1U, 2U, 7U};
  size_t wrong = 0U;
  size_t i;
  size_t j;
  unsigned ms;

  for (i = 0U; i < sizeof(starts) / sizeof(starts[0]); ++i) {
    for (j = 0U; j < sizeof(steps) / sizeof(steps[0]); ++j) {
      for (ms = 1U; ms <= UINT8_MAX; ++ms) {
        wrong += check_delay(starts[i], steps[j], (uint8_t)ms);
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, wrong);
}

/* Exact read counts at 1 ms per read: start read plus elapsed 1 .. ms + 1. */
static void test_exact_reads_at_one_ms_per_read(void) {
  static const uint8_t values[] = {1U, 2U, 7U, 128U, 255U};
  size_t i;
  size_t j;
  for (i = 0U; i < sizeof(starts) / sizeof(starts[0]); ++i) {
    for (j = 0U; j < sizeof(values); ++j) {
      fake_tick_reset(starts[i]);
      fake_tick_set_advance_per_read(1U);
      delay_ms(values[j]);
      TEST_ASSERT_EQUAL((size_t)values[j] + 2U, fake_tick_read_count());
    }
  }
}

/* A slow loop (one read takes longer than the delay) stops after the second read. */
static void test_long_pass_stops_after_second_read(void) {
  TEST_ASSERT_EQUAL(0U, check_delay(0xFFFFU, 300U, 255U));
  fake_tick_reset(0xFFFFU);
  fake_tick_set_advance_per_read(300U);
  delay_ms(255U);
  TEST_ASSERT_EQUAL(2U, fake_tick_read_count());
}

int main(void) {
  TEST_RUN(test_zero_never_reads_tick);
  TEST_RUN(test_every_ms_rounds_up_to_ms_plus_one);
  TEST_RUN(test_exact_reads_at_one_ms_per_read);
  TEST_RUN(test_long_pass_stops_after_second_read);
  return TEST_FINISH();
}
