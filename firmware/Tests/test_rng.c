/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Host tests of the xorshift RNG (references/internal/training_spec.md section 5.5, AC-T3). */

#include <stdint.h>

#include "test_assert.h"
#include "util/rng.h"

#define RNG_PERIOD 65535UL

static uint16_t reference_step(uint16_t state) {
  state ^= (uint16_t)(state << 7);
  state ^= (uint16_t)(state >> 9);
  state ^= (uint16_t)(state << 8);
  return state;
}

/* Runs first: the C start-up state 0 acts as the fallback seed. */
static void test_start_up_state_acts_as_fallback(void) {
  TEST_ASSERT_EQUAL(reference_step(RNG_SEED_FALLBACK), rng_next());
}

static void test_reference_values(void) {
  rng_seed(1U);
  TEST_ASSERT_EQUAL(0x8181U, rng_next());
  TEST_ASSERT_EQUAL(0x6021U, rng_next());
  TEST_ASSERT_EQUAL(0xE999U, rng_next());
  TEST_ASSERT_EQUAL(0x2E0BU, rng_next());
  TEST_ASSERT_EQUAL(0xB59EU, rng_next());
}

static void test_period_and_never_zero(void) {
  unsigned long step;
  unsigned long zero_count = 0UL;
  unsigned long first_return = 0UL;
  uint16_t value;

  rng_seed(1U);
  for (step = 1UL; step <= RNG_PERIOD; ++step) {
    value = rng_next();
    if (value == 0U) {
      ++zero_count;
    }
    if ((value == 1U) && (first_return == 0UL)) {
      first_return = step;
    }
  }
  TEST_ASSERT_EQUAL(0UL, zero_count);
  TEST_ASSERT_EQUAL(RNG_PERIOD, first_return);
}

static void test_seed_zero_gives_fallback(void) {
  uint16_t expected;

  rng_seed(RNG_SEED_FALLBACK);
  expected = rng_next();
  rng_seed(0U);
  TEST_ASSERT_EQUAL(expected, rng_next());
}

static void test_mix(void) {
  uint16_t expected;

  /* A mix that gives 0 gives the fallback. */
  rng_seed(RNG_SEED_FALLBACK);
  expected = rng_next();
  rng_seed(0x1234U);
  rng_mix(0x1234U);
  TEST_ASSERT_EQUAL(expected, rng_next());

  /* A mix XORs and does not step. */
  rng_seed(1U);
  rng_mix(0U);
  TEST_ASSERT_EQUAL(0x8181U, rng_next());
  rng_seed(0x0001U);
  rng_mix(0x0102U);
  TEST_ASSERT_EQUAL(reference_step(0x0103U), rng_next());
}

static void test_below_range(void) {
  unsigned limit;
  unsigned long step;
  unsigned long out_of_range = 0UL;

  for (limit = 1U; limit <= 255U; ++limit) {
    rng_seed((uint16_t)limit);
    for (step = 0UL; step < RNG_PERIOD; ++step) {
      if (rng_below((uint8_t)limit) >= limit) {
        ++out_of_range;
      }
    }
  }
  TEST_ASSERT_EQUAL(0UL, out_of_range);
}

static void test_below_formula(void) {
  uint16_t state;

  rng_seed(1U);
  /* 0x8181: high byte 0x81, (0x81 * 10) >> 8 = 5. */
  TEST_ASSERT_EQUAL(5U, rng_below(10U));
  /* 0x6021: (0x60 * 255) >> 8 = 95. */
  TEST_ASSERT_EQUAL(95U, rng_below(255U));
  TEST_ASSERT_EQUAL(0U, rng_below(1U));

  rng_seed(0x5A5AU);
  state = 0x5A5AU;
  for (unsigned step = 0U; step < 1000U; ++step) {
    state = reference_step(state);
    TEST_ASSERT_EQUAL(((unsigned)(state >> 8) * 37U) >> 8, rng_below(37U));
  }
}

static void test_below_zero_does_not_step(void) {
  rng_seed(1U);
  TEST_ASSERT_EQUAL(0U, rng_below(0U));
  TEST_ASSERT_EQUAL(0x8181U, rng_next());
}

int main(void) {
  TEST_RUN(test_start_up_state_acts_as_fallback);
  TEST_RUN(test_reference_values);
  TEST_RUN(test_period_and_never_zero);
  TEST_RUN(test_seed_zero_gives_fallback);
  TEST_RUN(test_mix);
  TEST_RUN(test_below_range);
  TEST_RUN(test_below_formula);
  TEST_RUN(test_below_zero_does_not_step);
  return TEST_FINISH();
}
