#ifndef BOARD_SIDETONE_PWM_H
#define BOARD_SIDETONE_PWM_H

/* Host replacement for firmware/inc/board/sidetone_pwm.h. Each macro calls the fake in fake_sidetone_pwm.c. */

#include "board/sidetone_config.h"
#include "fake_sidetone_pwm.h"

#define SIDETONE_PWM_CONFIGURE(arr) fake_sidetone_pwm_configure(arr)
#define SIDETONE_PINS_CONFIGURE() fake_sidetone_pwm_pins_configure()
#define SIDETONE_PINS_IS_HEADPHONES() fake_sidetone_pwm_is_headphones()
#define SIDETONE_TIMER_START() fake_sidetone_pwm_timer_start()
#define SIDETONE_PWM_SET_ARR(value) fake_sidetone_pwm_set_arr(value)
#define SIDETONE_PWM_SET_COMPARE_SPEAKER(value) fake_sidetone_pwm_set_compare_speaker(value)
#define SIDETONE_PWM_SET_COMPARE_HEADPHONES(value) fake_sidetone_pwm_set_compare_headphones(value)
#define SIDETONE_PWM_GENERATE_UPDATE() fake_sidetone_pwm_generate_update()
#define SIDETONE_PWM_CLEAR_UPDATE_FLAG() fake_sidetone_pwm_clear_update_flag()
#define SIDETONE_PWM_IS_UPDATE_FLAG_SET() fake_sidetone_pwm_read_update_flag()
#define SIDETONE_PWM_ENABLE_SPEAKER() fake_sidetone_pwm_set_eno(FAKE_SIDETONE_PWM_ENO_SPEAKER)
#define SIDETONE_PWM_ENABLE_HEADPHONES() fake_sidetone_pwm_set_eno(FAKE_SIDETONE_PWM_ENO_HEADPHONES)
#define SIDETONE_PWM_DISABLE_OUTPUTS() fake_sidetone_pwm_set_eno(FAKE_SIDETONE_PWM_ENO_OFF)
#define SIDETONE_PREEMPTION_POINT() fake_sidetone_pwm_preemption_point()

#endif /* BOARD_SIDETONE_PWM_H */
