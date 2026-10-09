/*
 * QA tests for the backlight polarity: the P0.3 level and the lit time of the LED for each level, over one PWM period.
 * Oracles: STC8H datasheet section 20.5.7 (PWM mode 1, up counting: OCiREF is high while CNT < CCR), section 20.7.16
 * (CC8P in PWMB_CCER2; CC1P in section 20.7.15 gives its meaning, 1 = low level is active) and components_and_wiring.md
 * section Backlight (P0.3 HIGH = on).
 */

#include <stdbool.h>
#include <stdint.h>

#include "backlight/backlight.h"
#include "board/backlight_config.h"
#include "fake_backlight_pwm.h"
#include "support/test_assert.h"

static bool is_pin_high(uint16_t count, uint16_t compare) {
  bool is_reference_high = count < compare;

  return (BACKLIGHT_IS_ACTIVE_LOW != 0) ? !is_reference_high : is_reference_high;
}

static uint16_t lit_counts_per_period(void) {
  uint16_t count;
  uint16_t lit_counts = 0U;

  for (count = 0U; count <= fake_backlight_pwm_arr(); ++count) {
    if (is_pin_high(count, fake_backlight_pwm_compare())) {
      ++lit_counts;
    }
  }
  return lit_counts;
}

static void test_lit_time_is_level_times_ten_percent(void) {
  uint8_t level;

  fake_backlight_pwm_reset();
  backlight_init(0U);
  for (level = 0U; level <= BACKLIGHT_LEVEL_MAX; ++level) {
    backlight_set_level(level);
    TEST_ASSERT_EQUAL((uint16_t)level * (BACKLIGHT_PWM_PERIOD / 10U), lit_counts_per_period());
  }
}

static void test_off_is_constant_low_and_max_is_constant_high(void) {
  fake_backlight_pwm_reset();
  backlight_init(0U);
  TEST_ASSERT_EQUAL(0U, lit_counts_per_period());
  backlight_set_level(BACKLIGHT_LEVEL_MAX);
  TEST_ASSERT_EQUAL(BACKLIGHT_PWM_PERIOD, lit_counts_per_period());
}

int main(void) {
  TEST_RUN(test_lit_time_is_level_times_ten_percent);
  TEST_RUN(test_off_is_constant_low_and_max_is_constant_high);
  return TEST_FINISH();
}
