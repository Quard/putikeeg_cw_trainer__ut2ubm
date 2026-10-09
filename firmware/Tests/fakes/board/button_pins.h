/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_BUTTON_PINS_H
#define BOARD_BUTTON_PINS_H

/* Host replacement for firmware/inc/board/button_pins.h. The bit values must match the target header. */

#include "fake_button_pins.h"

#define BUTTON_BIT_NEXT 0x01U
#define BUTTON_BIT_PREV 0x02U
#define BUTTON_BIT_MENU 0x04U
#define BUTTON_BIT_ENTER 0x08U

#define BUTTON_PINS_CONFIGURE() fake_button_pins_configure()
#define BUTTON_PINS_READ_PRESSED() fake_button_pins_read()

#endif /* BOARD_BUTTON_PINS_H */
