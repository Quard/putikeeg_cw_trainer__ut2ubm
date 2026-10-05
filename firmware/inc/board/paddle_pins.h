#ifndef BOARD_PADDLE_PINS_H
#define BOARD_PADDLE_PINS_H

/*
 * Paddle contacts, active low (references/internal/components_and_wiring.md): tip on P3.2, shared with the enter
 * button, and ring on P5.5. buttons_init() sets up P3.2. Host tests replace this header with
 * firmware/Tests/fakes/board/paddle_pins.h. No macro calls a function, so keyer_service() stays a leaf.
 */
#if !defined(__SDCC_mcs51)
#error "board/paddle_pins.h is for the target; host tests use firmware/Tests/fakes/board/paddle_pins.h"
#endif

#include <8051.h>
#include <stdint.h>

#include "mcu/stc8h.h"

#define PADDLE_RING_MASK 0x20U /* P5.5 */

/*
 * Pull-up first, then high-impedance input: P5M1 = 1, P5M0 = 0 (datasheet sections 9.1.2 and 9.1.3). The reset
 * mode is already input (section 2.9.3, note 1); the mode writes make it explicit. Each mode register gets one ORL
 * or ANL, so the other pins keep their mode. EAXFR stays set for the pull-up XFR.
 */
#define PADDLE_PINS_CONFIGURE()         \
  do {                                  \
    P_SW2 |= P_SW2_EAXFR;               \
    P5PU |= PADDLE_RING_MASK;           \
    P5M1 |= PADDLE_RING_MASK;           \
    P5M0 &= (uint8_t)~PADDLE_RING_MASK; \
  } while (0)

/* Pin bits only; a low pin is a closed contact. */
#define PADDLE_PINS_IS_TIP_CLOSED() (!P3_2)
#define PADDLE_PINS_IS_RING_CLOSED() (!P5_5)

#endif /* BOARD_PADDLE_PINS_H */
