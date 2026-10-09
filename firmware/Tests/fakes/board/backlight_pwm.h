/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_BACKLIGHT_PWM_H
#define BOARD_BACKLIGHT_PWM_H

/* Host replacement for firmware/inc/board/backlight_pwm.h. Records the calls in fake_backlight_pwm.c. */

#include "board/backlight_config.h"
#include "fake_backlight_pwm.h"

#define BACKLIGHT_PWM_CONFIGURE() fake_backlight_pwm_configure(BACKLIGHT_PRESCALER, BACKLIGHT_PWM_ARR)
#define BACKLIGHT_PWM_SET_COMPARE(value) fake_backlight_pwm_set_compare(value)

#endif /* BOARD_BACKLIGHT_PWM_H */
