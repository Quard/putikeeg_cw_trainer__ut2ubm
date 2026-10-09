/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Host tests of cw_timing_compute() (references/internal/training_spec.md section 5.3, AC-T2). */

#include <stdint.h>

#include "cw/timing.h"
#include "test_assert.h"

static uint16_t expected_dit(uint8_t wpm) { return (uint16_t)((4800U + (wpm / 2U)) / wpm); }

/* keyer_spec.md section 6.7: round((240000 c - 148800 s) / (19 c s)), half up, at least the dit. */
static uint16_t expected_unit(uint8_t wpm, uint8_t eff_wpm) {
  const uint64_t numerator = (240000ULL * wpm) - (148800ULL * eff_wpm);
  const uint64_t denominator = 19ULL * wpm * eff_wpm;
  const uint64_t unit = ((2ULL * numerator) + denominator) / (2ULL * denominator);
  const uint16_t dit = expected_dit(wpm);

  return (unit < dit) ? dit : (uint16_t)unit;
}

static void test_every_valid_pair(void) {
  uint8_t wpm;
  uint8_t eff_wpm;

  for (wpm = CW_TIMING_WPM_MIN; wpm <= CW_TIMING_WPM_MAX; ++wpm) {
    for (eff_wpm = CW_TIMING_WPM_MIN; eff_wpm <= wpm; ++eff_wpm) {
      cw_timing_compute(wpm, eff_wpm);
      TEST_ASSERT_EQUAL(expected_dit(wpm), cw_timing_dit_ticks);
      TEST_ASSERT_EQUAL(expected_unit(wpm, eff_wpm), cw_timing_unit_ticks);
      TEST_ASSERT(cw_timing_unit_ticks >= cw_timing_dit_ticks);
      TEST_ASSERT(cw_timing_unit_ticks <= 2370U);
    }
    cw_timing_compute(wpm, wpm);
    TEST_ASSERT_EQUAL(cw_timing_dit_ticks, cw_timing_unit_ticks);
  }
}

typedef struct {
  uint8_t wpm;
  uint8_t eff_wpm;
  uint16_t dit;
  uint16_t unit;
} timing_row_t;

/* keyer_spec.md section 6.7 (word gap / 5) and training_spec.md section 9.2 (character gap / 3). */
static void test_reference_rows(void) {
  static const timing_row_t rows[] = {
      {5U, 5U, 960U, 960U},   {20U, 20U, 240U, 240U}, {20U, 10U, 240U, 872U}, {20U, 5U, 240U, 2135U},
      {25U, 15U, 192U, 529U}, {50U, 50U, 96U, 96U},   {50U, 49U, 96U, 101U},  {50U, 5U, 96U, 2370U},
      {13U, 13U, 369U, 369U}, {7U, 7U, 686U, 686U},
  };
  size_t index;

  for (index = 0U; index < sizeof(rows) / sizeof(rows[0]); ++index) {
    cw_timing_compute(rows[index].wpm, rows[index].eff_wpm);
    TEST_ASSERT_EQUAL(rows[index].dit, cw_timing_dit_ticks);
    TEST_ASSERT_EQUAL(rows[index].unit, cw_timing_unit_ticks);
  }
}

/* The clamps of AC-K20. */
static void test_clamps(void) {
  cw_timing_compute(0U, 0U);
  TEST_ASSERT_EQUAL(960U, cw_timing_dit_ticks);
  TEST_ASSERT_EQUAL(960U, cw_timing_unit_ticks);
  cw_timing_compute(4U, 4U);
  TEST_ASSERT_EQUAL(960U, cw_timing_dit_ticks);
  TEST_ASSERT_EQUAL(960U, cw_timing_unit_ticks);
  cw_timing_compute(51U, 51U);
  TEST_ASSERT_EQUAL(96U, cw_timing_dit_ticks);
  TEST_ASSERT_EQUAL(96U, cw_timing_unit_ticks);
  cw_timing_compute(255U, 255U);
  TEST_ASSERT_EQUAL(96U, cw_timing_dit_ticks);
  TEST_ASSERT_EQUAL(96U, cw_timing_unit_ticks);
  /* eff_wpm above wpm gives wpm. */
  cw_timing_compute(20U, 30U);
  TEST_ASSERT_EQUAL(240U, cw_timing_unit_ticks);
  cw_timing_compute(255U, 255U);
  TEST_ASSERT_EQUAL(96U, cw_timing_unit_ticks);
  /* eff_wpm below the minimum gives 5. */
  cw_timing_compute(20U, 0U);
  TEST_ASSERT_EQUAL(2135U, cw_timing_unit_ticks);
  cw_timing_compute(20U, 4U);
  TEST_ASSERT_EQUAL(2135U, cw_timing_unit_ticks);
  /* wpm is clamped first, so eff_wpm 60 with wpm 255 gives 50 / 50. */
  cw_timing_compute(255U, 60U);
  TEST_ASSERT_EQUAL(96U, cw_timing_unit_ticks);
  /* wpm 0 gives 5, so eff_wpm 50 also gives 5. */
  cw_timing_compute(0U, 50U);
  TEST_ASSERT_EQUAL(960U, cw_timing_unit_ticks);
}

int main(void) {
  TEST_RUN(test_every_valid_pair);
  TEST_RUN(test_reference_rows);
  TEST_RUN(test_clamps);
  return TEST_FINISH();
}
