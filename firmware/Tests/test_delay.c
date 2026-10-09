/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "delay/delay.h"
#include "fake_tick.h"
#include "support/test_assert.h"

/*
 * With a step of 1 ms per read, delay_ms(ms) reads the start value and then elapsed values 1 .. ms + 1:
 * ms + 2 reads in total, and it stops at the first read where elapsed >= ms + 1.
 */
static void check_delay(uint16_t start_ms, uint8_t ms) {
  fake_tick_reset(start_ms);
  fake_tick_set_advance_per_read(1U);
  delay_ms(ms);
  TEST_ASSERT_EQUAL((size_t)ms + 2U, fake_tick_read_count());
}

static void test_zero_does_not_read_tick(void) {
  fake_tick_reset(0U);
  fake_tick_set_advance_per_read(1U);
  delay_ms(0U);
  TEST_ASSERT_EQUAL(0U, fake_tick_read_count());
}

static void test_waits_ms_plus_one_ticks(void) {
  check_delay(0U, 1U);
  check_delay(0U, 2U);
  check_delay(1000U, 255U);
}

static void test_waits_across_wrap(void) {
  check_delay(0xFFFEU, 1U);
  check_delay(0xFFFEU, 5U);
  check_delay(0xFFFEU, 255U);
  check_delay(0xFFFFU, 2U);
}

static void test_stops_on_first_read_past_target(void) {
  /* With 3 ms per read: elapsed 3, 6 -> stops at 6 >= 5 for ms = 4, after 3 reads in total. */
  fake_tick_reset(0xFFF0U);
  fake_tick_set_advance_per_read(3U);
  delay_ms(4U);
  TEST_ASSERT_EQUAL(3U, fake_tick_read_count());
  /* Exactly ms + 1 on the second read. */
  fake_tick_reset(0U);
  fake_tick_set_advance_per_read(6U);
  delay_ms(5U);
  TEST_ASSERT_EQUAL(2U, fake_tick_read_count());
}

int main(void) {
  TEST_RUN(test_zero_does_not_read_tick);
  TEST_RUN(test_waits_ms_plus_one_ticks);
  TEST_RUN(test_waits_across_wrap);
  TEST_RUN(test_stops_on_first_read_past_target);
  return TEST_FINISH();
}
