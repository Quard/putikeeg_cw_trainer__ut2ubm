/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "delay/delay.h"

#include "tick/tick.h"

void delay_ms(uint8_t ms) {
  uint16_t start_ms;

  if (ms == 0U) {
    return;
  }
  start_ms = tick_now();
  /* A tick can come right after the first read, so only ms + 1 ticks guarantee ms full milliseconds. */
  while ((uint16_t)(tick_now() - start_ms) <= ms) {
  }
}
