/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FAKE_BUTTON_PINS_H
#define FAKE_BUTTON_PINS_H

/*
 * Host fake of the button pins. Each read returns the next mask of a sequence that the test sets (bit set =
 * pressed); after the sequence the last mask stays. Without a sequence, reads return the mask of
 * fake_button_pins_set().
 */

#include <stddef.h>
#include <stdint.h>

/** Clears the sequence, the counters and the pressed mask. */
void fake_button_pins_reset(void);

/** Sets the mask for the next reads and removes the sequence. */
void fake_button_pins_set(uint8_t pressed_mask);

/** Sets a sequence of masks for the next reads (at most 256). The array must stay valid. */
void fake_button_pins_set_sequence(const uint8_t* masks, size_t count);

size_t fake_button_pins_read_count(void);
size_t fake_button_pins_configure_count(void);

/** Returns the delay_ms() call count of the fake delay at the first read since the reset. */
size_t fake_button_pins_delays_before_first_read(void);

uint8_t fake_button_pins_read(void);
void fake_button_pins_configure(void);

#endif /* FAKE_BUTTON_PINS_H */
