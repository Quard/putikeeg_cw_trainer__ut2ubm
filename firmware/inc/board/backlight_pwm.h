/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_BACKLIGHT_PWM_H
#define BOARD_BACKLIGHT_PWM_H

/*
 * PWMB channel 8 on P0.3 for the backlight. Host tests replace this header with
 * firmware/Tests/fakes/board/backlight_pwm.h. No PWMB output may go to port 2: P2.3 is the PWM8 reset pin and the
 * LCD SCLK, and P2.0-P2.2 are the reset pins of PWM5-PWM7. So PWMB_PS is written before any ENO bit, and
 * ENO5P-ENO7P stay 0. Source: references/context/STC8H_datasheet.md sections 3.1.5, 9.2 and 20.7.
 */
#if !defined(__SDCC_mcs51)
#error "board/backlight_pwm.h is for the target; host tests use firmware/Tests/fakes/board/backlight_pwm.h"
#endif

#include <stdint.h>

#include "board/backlight_config.h"
#include "mcu/stc8h.h"

#if BACKLIGHT_IS_ACTIVE_LOW
#define BACKLIGHT_CCER2_VALUE (PWMB_CCER2_CC8E | PWMB_CCER2_CC8P)
#else
#define BACKLIGHT_CCER2_VALUE PWMB_CCER2_CC8E
#endif

#define BACKLIGHT_P0_MASK 0x08U /* P0.3 */

/* The datasheet asks for the high byte of a 16-bit register first (section 20.3.2). */
/* D-BL1: P0PU drives the Q6 base in open drain. Push-pull would source about 27 mA through R2, over the 20 mA limit. */
#define BACKLIGHT_PWM_CONFIGURE()                       \
  do {                                                  \
    P_SW2 |= P_SW2_EAXFR;                               \
    PWMB_ENO = 0U;                                      \
    PWMB_CCER2 = 0U;                                    \
    PWMB_CR1 = 0U;                                      \
    PWMB_PS = PWMB_PS_C8PS_P03;                         \
    PWMB_PSCRH = (uint8_t)(BACKLIGHT_PRESCALER >> 8);   \
    PWMB_PSCRL = (uint8_t)BACKLIGHT_PRESCALER;          \
    PWMB_ARRH = (uint8_t)(BACKLIGHT_PWM_ARR >> 8);      \
    PWMB_ARRL = (uint8_t)BACKLIGHT_PWM_ARR;             \
    PWMB_CCMR4 = PWMB_CCMR4_PWM1_PRELOAD;               \
    PWMB_CCR8H = 0U;                                    \
    PWMB_CCR8L = 0U;                                    \
    PWMB_CCER2 = BACKLIGHT_CCER2_VALUE;                 \
    PWMB_ENO = PWMB_ENO_ENO8P;                          \
    PWMB_BKR = PWMB_BKR_MOE;                            \
    PWMB_EGR = PWMB_EGR_UG;                             \
    PWMB_CR1 = (uint8_t)(PWMB_CR1_ARPE | PWMB_CR1_CEN); \
    P0M1 |= BACKLIGHT_P0_MASK;                          \
    P0M0 |= BACKLIGHT_P0_MASK;                          \
    P0PU |= BACKLIGHT_P0_MASK;                          \
  } while (0)

#define BACKLIGHT_PWM_SET_COMPARE(value)  \
  do {                                    \
    PWMB_CCR8H = (uint8_t)((value) >> 8); \
    PWMB_CCR8L = (uint8_t)(value);        \
  } while (0)

#endif /* BOARD_BACKLIGHT_PWM_H */
