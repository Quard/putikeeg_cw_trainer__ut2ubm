/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FAKE_DELAY_H
#define FAKE_DELAY_H

/* Host fake of delay_ms(): records each call instead of waiting. */

#include <stddef.h>
#include <stdint.h>

/** Clears the recorded calls. */
void fake_delay_reset(void);

size_t fake_delay_count(void);

/** Returns the argument of call @p i, or 0 when @p i is out of range. */
uint8_t fake_delay_ms_at(size_t i);

/** Returns the pin-event count at call @p i: the index the next pin event gets. 0 when @p i is out of range. */
size_t fake_delay_event_index_at(size_t i);

#endif /* FAKE_DELAY_H */
