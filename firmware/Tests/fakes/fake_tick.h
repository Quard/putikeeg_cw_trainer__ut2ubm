/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FAKE_TICK_H
#define FAKE_TICK_H

/* Host fake of tick_now(): each read returns the current value and then advances it by a fixed step. */

#include <stddef.h>
#include <stdint.h>

/** Sets the value the next read returns, sets the step to 0 and clears the read count. */
void fake_tick_reset(uint16_t start_ms);

/** Sets how far the value moves after each read. The value wraps from 65535 to 0. */
void fake_tick_set_advance_per_read(uint16_t step_ms);

/** Sets the value the next read returns. Keeps the step and the read count. */
void fake_tick_set_now(uint16_t now_ms);

/** Returns the number of tick_now() calls since fake_tick_reset(). */
size_t fake_tick_read_count(void);

#endif /* FAKE_TICK_H */
