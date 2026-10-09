#include "backlight/backlight.h"
#include "board/backlight_config.h"
#include "fake_backlight_pwm.h"
#include "support/test_assert.h"

static void test_init_configures_once_and_sets_level(void) {
  fake_backlight_pwm_reset();
  backlight_init(8U);
  TEST_ASSERT_EQUAL(1U, fake_backlight_pwm_configure_count());
  TEST_ASSERT_EQUAL(23U, fake_backlight_pwm_prescaler());
  TEST_ASSERT_EQUAL(999U, fake_backlight_pwm_arr());
  TEST_ASSERT_EQUAL(1U, fake_backlight_pwm_compare_count());
  TEST_ASSERT_EQUAL(800U, fake_backlight_pwm_compare());
  TEST_ASSERT_EQUAL(0U, fake_backlight_pwm_writes_before_configure());
  backlight_set_level(3U);
  TEST_ASSERT_EQUAL(1U, fake_backlight_pwm_configure_count());
}

static void test_level_to_compare(void) {
  uint8_t level;
  fake_backlight_pwm_reset();
  backlight_init(0U);
  TEST_ASSERT_EQUAL(0U, fake_backlight_pwm_compare());
  for (level = 0U; level <= BACKLIGHT_LEVEL_MAX; ++level) {
    backlight_set_level(level);
    TEST_ASSERT_EQUAL((uint16_t)level * 100U, fake_backlight_pwm_compare());
  }
  /* Level 10 gives ARR + 1: always on. */
  TEST_ASSERT_EQUAL(fake_backlight_pwm_arr() + 1U, fake_backlight_pwm_compare());
}

static void test_levels_above_max(void) {
  fake_backlight_pwm_reset();
  backlight_init(11U);
  TEST_ASSERT_EQUAL(1000U, fake_backlight_pwm_compare());
  backlight_set_level(0U);
  backlight_set_level(255U);
  TEST_ASSERT_EQUAL(1000U, fake_backlight_pwm_compare());
}

static void test_default_polarity_is_active_high(void) { TEST_ASSERT_EQUAL(0U, BACKLIGHT_IS_ACTIVE_LOW); }

int main(void) {
  TEST_RUN(test_init_configures_once_and_sets_level);
  TEST_RUN(test_level_to_compare);
  TEST_RUN(test_levels_above_max);
  TEST_RUN(test_default_polarity_is_active_high);
  return TEST_FINISH();
}
