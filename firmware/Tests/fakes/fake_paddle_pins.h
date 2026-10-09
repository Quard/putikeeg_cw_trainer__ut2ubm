/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FAKE_PADDLE_PINS_H
#define FAKE_PADDLE_PINS_H

/* Host fake of the paddle contacts. A test sets each contact; the reads return it and count the reads. */

#include <stdbool.h>
#include <stddef.h>

/** Opens both contacts and clears the counters. */
void fake_paddle_pins_reset(void);
void fake_paddle_pins_set_tip_closed(bool is_closed);
void fake_paddle_pins_set_ring_closed(bool is_closed);
size_t fake_paddle_pins_configure_count(void);
/** Returns the tip and ring reads together. */
size_t fake_paddle_pins_read_count(void);

/* Called by the macros of firmware/Tests/fakes/board/paddle_pins.h. */
void fake_paddle_pins_configure(void);
bool fake_paddle_pins_is_tip_closed(void);
bool fake_paddle_pins_is_ring_closed(void);

#endif /* FAKE_PADDLE_PINS_H */
