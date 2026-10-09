/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fake_button_pins.h"

#include <stdbool.h>

#include "fake_delay.h"

static uint8_t fake_mask;
static const uint8_t* fake_sequence;
static size_t fake_sequence_count;
static size_t fake_reads;
static size_t fake_configures;
static size_t fake_delays_at_first_read;

void fake_button_pins_reset(void) {
  fake_mask = 0U;
  fake_sequence = NULL;
  fake_sequence_count = 0U;
  fake_reads = 0U;
  fake_configures = 0U;
  fake_delays_at_first_read = 0U;
}

void fake_button_pins_set(uint8_t pressed_mask) {
  fake_mask = pressed_mask;
  fake_sequence = NULL;
  fake_sequence_count = 0U;
}

void fake_button_pins_set_sequence(const uint8_t* masks, size_t count) {
  fake_sequence = masks;
  fake_sequence_count = count;
}

size_t fake_button_pins_read_count(void) { return fake_reads; }

size_t fake_button_pins_configure_count(void) { return fake_configures; }

size_t fake_button_pins_delays_before_first_read(void) { return fake_delays_at_first_read; }

uint8_t fake_button_pins_read(void) {
  if (fake_reads == 0U) {
    fake_delays_at_first_read = fake_delay_count();
  }
  ++fake_reads;
  if (fake_sequence_count != 0U) {
    fake_mask = *fake_sequence;
    if (fake_sequence_count > 1U) {
      ++fake_sequence;
      --fake_sequence_count;
    }
  }
  return fake_mask;
}

void fake_button_pins_configure(void) { ++fake_configures; }
