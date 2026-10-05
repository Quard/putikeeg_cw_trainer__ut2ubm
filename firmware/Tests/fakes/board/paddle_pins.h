#ifndef BOARD_PADDLE_PINS_H
#define BOARD_PADDLE_PINS_H

/* Host replacement for firmware/inc/board/paddle_pins.h. Each macro calls the fake in fake_paddle_pins.c. */

#include "fake_paddle_pins.h"

#define PADDLE_RING_MASK 0x20U

#define PADDLE_PINS_CONFIGURE() fake_paddle_pins_configure()
#define PADDLE_PINS_IS_TIP_CLOSED() fake_paddle_pins_is_tip_closed()
#define PADDLE_PINS_IS_RING_CLOSED() fake_paddle_pins_is_ring_closed()

#endif /* BOARD_PADDLE_PINS_H */
