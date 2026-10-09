/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_BACKLIGHT_CONFIG_H
#define BOARD_BACKLIGHT_CONFIG_H

/*
 * Backlight PWM on P0.3. P0.3 drives the Q6 base through R2, so a high pin turns the backlight on. R10 pulls the pin
 * down. build.sh passes no -D options, so change a value here.
 */

#ifndef BACKLIGHT_PWM_HZ
#define BACKLIGHT_PWM_HZ 1000U /* not checked on the device */
#endif
#ifndef BACKLIGHT_IS_ACTIVE_LOW
#define BACKLIGHT_IS_ACTIVE_LOW 0 /* 1 sets CC8P; polarity from the corrected schematic */
#endif
#if (BACKLIGHT_IS_ACTIVE_LOW != 0) && (BACKLIGHT_IS_ACTIVE_LOW != 1)
#error "BACKLIGHT_IS_ACTIVE_LOW must be 0 or 1"
#endif

/*
 * The counter runs at 1 MHz: prescaler = MCU_CLOCK_HZ / 1 MHz - 1 (23 at 24 MHz); the counter clock is
 * fck / (PSCR + 1) (datasheet section 20.7.19). Period = 1 MHz / BACKLIGHT_PWM_HZ counts (1000 at 1 kHz),
 * ARR = period - 1. In PWM mode 1 the output is active while the counter is below CCR8, so CCR8 = 0 is always off
 * and CCR8 = ARR + 1 is always on (section 20.5.7).
 */
#define BACKLIGHT_COUNTER_HZ 1000000UL
#define BACKLIGHT_PRESCALER ((uint16_t)((MCU_CLOCK_HZ / BACKLIGHT_COUNTER_HZ) - 1UL))
#define BACKLIGHT_PWM_PERIOD ((uint16_t)(BACKLIGHT_COUNTER_HZ / BACKLIGHT_PWM_HZ))
#define BACKLIGHT_PWM_ARR ((uint16_t)(BACKLIGHT_PWM_PERIOD - 1U))

#if (MCU_CLOCK_HZ % 1000000UL) != 0UL
#error "MCU_CLOCK_HZ must be a whole number of MHz for the 1 MHz backlight counter"
#endif
#if ((BACKLIGHT_COUNTER_HZ % BACKLIGHT_PWM_HZ) != 0UL) || ((BACKLIGHT_COUNTER_HZ / BACKLIGHT_PWM_HZ) % 10UL != 0UL)
#error "BACKLIGHT_PWM_HZ must give a period that is a multiple of 10 counts"
#endif
#if ((BACKLIGHT_COUNTER_HZ / BACKLIGHT_PWM_HZ) < 10UL) || ((BACKLIGHT_COUNTER_HZ / BACKLIGHT_PWM_HZ) > 65535UL)
#error "BACKLIGHT_PWM_HZ gives a period outside 10..65535 counts"
#endif

#endif /* BOARD_BACKLIGHT_CONFIG_H */
