/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_BUTTON_PINS_H
#define BOARD_BUTTON_PINS_H

/*
 * Buttons, active low with internal pull-ups (references/internal/components_and_wiring.md): P0.0 next,
 * P0.1 prev, P0.2 menu/back, P3.2 enter (shared with the paddle tip). Host tests replace this header with
 * firmware/Tests/fakes/board/button_pins.h.
 */
#if !defined(__SDCC_mcs51)
#error "board/button_pins.h is for the target; host tests use firmware/Tests/fakes/board/button_pins.h"
#endif

#include <8051.h>
#include <stdint.h>

#include "mcu/stc8h.h"

#define BUTTON_BIT_NEXT 0x01U
#define BUTTON_BIT_PREV 0x02U
#define BUTTON_BIT_MENU 0x04U
#define BUTTON_BIT_ENTER 0x08U

#define BUTTON_P0_MASK 0x07U /* P0.0-P0.2 */
#define BUTTON_P3_MASK 0x04U /* P3.2 */

/*
 * Pull-ups on, then high-impedance input: PxM1 = 1, PxM0 = 0 (datasheet section 9.2). Each mode register gets
 * one ORL or ANL, so the other pins keep their mode. EAXFR stays set for the pull-up XFRs.
 */
#define BUTTON_PINS_CONFIGURE()       \
  do {                                \
    P_SW2 |= P_SW2_EAXFR;             \
    P0PU |= BUTTON_P0_MASK;           \
    P3PU |= BUTTON_P3_MASK;           \
    P0M1 |= BUTTON_P0_MASK;           \
    P0M0 &= (uint8_t)~BUTTON_P0_MASK; \
    P3M1 |= BUTTON_P3_MASK;           \
    P3M0 &= (uint8_t)~BUTTON_P3_MASK; \
  } while (0)

/* Pin bits only; a low pin is a pressed button. */
#define BUTTON_PINS_READ_PRESSED()                                                                           \
  ((uint8_t)((P0_0 ? 0U : BUTTON_BIT_NEXT) | (P0_1 ? 0U : BUTTON_BIT_PREV) | (P0_2 ? 0U : BUTTON_BIT_MENU) | \
             (P3_2 ? 0U : BUTTON_BIT_ENTER)))

#endif /* BOARD_BUTTON_PINS_H */
