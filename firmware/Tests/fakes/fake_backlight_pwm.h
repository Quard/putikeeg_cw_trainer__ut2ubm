#ifndef FAKE_BACKLIGHT_PWM_H
#define FAKE_BACKLIGHT_PWM_H

/* Host fake of the backlight PWM registers: records configure and compare writes. */

#include <stddef.h>
#include <stdint.h>

void fake_backlight_pwm_reset(void);
size_t fake_backlight_pwm_configure_count(void);
uint16_t fake_backlight_pwm_prescaler(void);
uint16_t fake_backlight_pwm_arr(void);
size_t fake_backlight_pwm_compare_count(void);
/** Returns the last compare value. */
uint16_t fake_backlight_pwm_compare(void);
/** Returns the number of compare writes before the first configure (must be 0). */
size_t fake_backlight_pwm_writes_before_configure(void);

void fake_backlight_pwm_configure(uint16_t prescaler, uint16_t arr);
void fake_backlight_pwm_set_compare(uint16_t value);

#endif /* FAKE_BACKLIGHT_PWM_H */
