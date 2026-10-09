/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fake_paddle_pins.h"

static bool fake_is_tip_closed;
static bool fake_is_ring_closed;
static size_t fake_configures;
static size_t fake_reads;

void fake_paddle_pins_reset(void) {
  fake_is_tip_closed = false;
  fake_is_ring_closed = false;
  fake_configures = 0U;
  fake_reads = 0U;
}

void fake_paddle_pins_set_tip_closed(bool is_closed) { fake_is_tip_closed = is_closed; }

void fake_paddle_pins_set_ring_closed(bool is_closed) { fake_is_ring_closed = is_closed; }

size_t fake_paddle_pins_configure_count(void) { return fake_configures; }

size_t fake_paddle_pins_read_count(void) { return fake_reads; }

void fake_paddle_pins_configure(void) { ++fake_configures; }

bool fake_paddle_pins_is_tip_closed(void) {
  ++fake_reads;
  return fake_is_tip_closed;
}

bool fake_paddle_pins_is_ring_closed(void) {
  ++fake_reads;
  return fake_is_ring_closed;
}
