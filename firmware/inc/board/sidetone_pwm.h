#ifndef BOARD_SIDETONE_PWM_H
#define BOARD_SIDETONE_PWM_H

/*
 * PWMA for the sidetone: PWM1N on P1.1 (speaker), PWM2P on P1.2 (headphones), plug detect on P1.0, Timer 0 for
 * the 4 kHz envelope service. Host tests replace this header with firmware/Tests/fakes/board/sidetone_pwm.h.
 * Both routes are quiet HIGH: with CC1NP = 1 and CC2P = 1 a pin is LOW only while CNT < CCRi (sections 4.1 and 7.1 of
 * references/internal/sidetone_spec.md). ENO1P, ENO2N and ENO3P-ENO4N never become 1: P1.0 is the plug input,
 * and PWMA_PS stays 00 so that no PWMA output goes to the LCD pins of port 2 (datasheet section 3.1.5).
 * No macro calls a function, so sidetone_service() stays a leaf.
 */
#if !defined(__SDCC_mcs51)
#error "board/sidetone_pwm.h is for the target; host tests use firmware/Tests/fakes/board/sidetone_pwm.h"
#endif

#include <8051.h>
#include <stdint.h>

#include "board/sidetone_config.h"
#include "mcu/stc8h.h"

#define SIDETONE_P1_ROUTE_MASK 0x06U /* P1.1, P1.2 */
#define SIDETONE_P1_PLUG_MASK 0x01U  /* P1.0 */

/* CC1E = CC1P = CC2NE = CC2NP = 0: the unused half of each pair stays "output prohibited" (section 20.7.15). */
#define SIDETONE_CCER1_VALUE ((uint8_t)(PWMA_CCER1_CC1NE | PWMA_CCER1_CC1NP | PWMA_CCER1_CC2E | PWMA_CCER1_CC2P))

/*
 * Datasheet section 20.7: CCMR is writable only while CCER1 = 0 (20.7.11), 16-bit registers take the high byte
 * first (20.3.2), BKR lock bits must be right at the first write (20.7.32), UG loads ARR, PSCR and the compares
 * (20.7.10) and sets UIF, which the next write clears (20.7.8). The counter then runs all the time.
 */
#define SIDETONE_PWM_CONFIGURE(arr)                     \
  do {                                                  \
    P_SW2 |= P_SW2_EAXFR;                               \
    PWMA_ENO = 0U;                                      \
    PWMA_CCER1 = 0U;                                    \
    PWMA_CR1 = 0U;                                      \
    PWMA_IER = 0U;                                      \
    PWMA_CR2 = 0U;                                      \
    PWMA_IOAUX = 0U;                                    \
    PWMA_PS = PWMA_PS_P1;                               \
    PWMA_PSCRH = (uint8_t)(SIDETONE_PRESCALER >> 8);    \
    PWMA_PSCRL = (uint8_t)SIDETONE_PRESCALER;           \
    PWMA_ARRH = (uint8_t)((arr) >> 8);                  \
    PWMA_ARRL = (uint8_t)(arr);                         \
    PWMA_CCMR1 = PWMA_CCMR_PWM1_PRELOAD;                \
    PWMA_CCMR2 = PWMA_CCMR_PWM1_PRELOAD;                \
    PWMA_CCR1H = 0U;                                    \
    PWMA_CCR1L = 0U;                                    \
    PWMA_CCR2H = 0U;                                    \
    PWMA_CCR2L = 0U;                                    \
    PWMA_CCER1 = SIDETONE_CCER1_VALUE;                  \
    PWMA_BKR = PWMA_BKR_MOE;                            \
    PWMA_EGR = PWMA_EGR_UG;                             \
    PWMA_SR1 = 0U;                                      \
    PWMA_CR1 = (uint8_t)(PWMA_CR1_ARPE | PWMA_CR1_CEN); \
  } while (0)

/*
 * Latch HIGH first, then M0 before M1, so P1.1 and P1.2 pass through open-drain HIGH and never glitch LOW
 * (datasheet section 9.1.2). P1.0 gets its pull-up and the high-impedance input mode. Each mode register gets one
 * ORL or ANL, so the other pins keep their mode.
 */
#define SIDETONE_PINS_CONFIGURE()             \
  do {                                        \
    P1_1 = 1;                                 \
    P1_2 = 1;                                 \
    P1M0 |= SIDETONE_P1_ROUTE_MASK;           \
    P1M1 &= (uint8_t)~SIDETONE_P1_ROUTE_MASK; \
    P1PU |= SIDETONE_P1_PLUG_MASK;            \
    P1M1 |= SIDETONE_P1_PLUG_MASK;            \
    P1M0 &= (uint8_t)~SIDETONE_P1_PLUG_MASK;  \
  } while (0)

/* A LOW P1.0 means headphones inserted (references/internal/components_and_wiring.md). Reads the pin bit only. */
#define SIDETONE_PINS_IS_HEADPHONES() (!P1_0)

/*
 * Timer 0 mode 0 (16-bit auto-reload), 12T. TMOD mode 3 is a non-maskable interrupt and must never be used
 * (datasheet section 13.2.2). TL0/TH0 writes while TR0 = 0 also set the reload (13.2.3). No IP/IPH write: Timer 0
 * keeps level 0, the level of the Timer 3 tick, so the two ISRs never nest.
 */
#define SIDETONE_TIMER_START()                    \
  do {                                            \
    TR0 = 0;                                      \
    ET0 = 0;                                      \
    TMOD &= (uint8_t)~TMOD_T0_MASK;               \
    AUXR &= (uint8_t)~AUXR_T0X12;                 \
    TL0 = (uint8_t)SIDETONE_TIMER0_RELOAD;        \
    TH0 = (uint8_t)(SIDETONE_TIMER0_RELOAD >> 8); \
    TF0 = 0;                                      \
    ET0 = 1;                                      \
    TR0 = 1;                                      \
  } while (0)

#define SIDETONE_PWM_SET_ARR(value)      \
  do {                                   \
    PWMA_ARRH = (uint8_t)((value) >> 8); \
    PWMA_ARRL = (uint8_t)(value);        \
  } while (0)

#define SIDETONE_PWM_SET_COMPARE_SPEAKER(value) \
  do {                                          \
    PWMA_CCR1H = (uint8_t)((value) >> 8);       \
    PWMA_CCR1L = (uint8_t)(value);              \
  } while (0)

#define SIDETONE_PWM_SET_COMPARE_HEADPHONES(value) \
  do {                                             \
    PWMA_CCR2H = (uint8_t)((value) >> 8);          \
    PWMA_CCR2L = (uint8_t)(value);                 \
  } while (0)

#define SIDETONE_PWM_GENERATE_UPDATE() \
  do {                                 \
    PWMA_EGR = PWMA_EGR_UG;            \
  } while (0)

/* PWMA uses no other flag and no interrupt, so a full clear is safe. */
#define SIDETONE_PWM_CLEAR_UPDATE_FLAG() \
  do {                                   \
    PWMA_SR1 = 0U;                       \
  } while (0)

#define SIDETONE_PWM_IS_UPDATE_FLAG_SET() ((PWMA_SR1 & PWMA_SR1_UIF) != 0U)

#define SIDETONE_PWM_ENABLE_SPEAKER() \
  do {                                \
    PWMA_ENO = PWMA_ENO_ENO1N;        \
  } while (0)

#define SIDETONE_PWM_ENABLE_HEADPHONES() \
  do {                                   \
    PWMA_ENO = PWMA_ENO_ENO2P;           \
  } while (0)

#define SIDETONE_PWM_DISABLE_OUTPUTS() \
  do {                                 \
    PWMA_ENO = 0U;                     \
  } while (0)

/* Host tests run the Timer 0 service here; on the target the interrupt can come at any instruction. */
#define SIDETONE_PREEMPTION_POINT() \
  do {                              \
  } while (0)

#endif /* BOARD_SIDETONE_PWM_H */
