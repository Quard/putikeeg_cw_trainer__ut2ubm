#include "backlight/backlight.h"

#include "board/backlight_config.h"
#include "board/backlight_pwm.h"

/* Counts per level: level 10 gives the full period, ARR + 1, which keeps the output always on. */
#define BACKLIGHT_COMPARE_STEP ((uint16_t)(BACKLIGHT_PWM_PERIOD / BACKLIGHT_LEVEL_MAX))

void backlight_init(uint8_t level) {
  BACKLIGHT_PWM_CONFIGURE();
  backlight_set_level(level);
}

void backlight_set_level(uint8_t level) {
  uint16_t compare = 0U;

  if (level > BACKLIGHT_LEVEL_MAX) {
    level = BACKLIGHT_LEVEL_MAX;
  }
  while (level != 0U) {
    compare = (uint16_t)(compare + BACKLIGHT_COMPARE_STEP);
    --level;
  }
  BACKLIGHT_PWM_SET_COMPARE(compare);
}
