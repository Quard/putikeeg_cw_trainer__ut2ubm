/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef TICK_TICK_COUNTER_H
#define TICK_TICK_COUNTER_H

#include <stdint.h>

/* Milliseconds since tick_init(). Only the Timer 3 interrupt writes it after tick_init(). */
extern volatile uint16_t tick_count_ms;

#endif /* TICK_TICK_COUNTER_H */
