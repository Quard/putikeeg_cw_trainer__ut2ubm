/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "backlight/backlight.h"
#include "fake_menu_outputs.h"

static uint8_t fake_levels[FAKE_OUTPUT_HISTORY];
static size_t fake_count;
static size_t fake_inits;

void fake_backlight_reset(void) {
  fake_count = 0U;
  fake_inits = 0U;
}

size_t fake_backlight_init_count(void) { return fake_inits; }

size_t fake_backlight_count(void) { return fake_count; }

uint8_t fake_backlight_at(size_t index) { return (index < fake_count) ? fake_levels[index] : 0xFFU; }

uint8_t fake_backlight_last(void) { return (fake_count != 0U) ? fake_levels[fake_count - 1U] : 0xFFU; }

void backlight_set_level(uint8_t level) {
  if (fake_count < FAKE_OUTPUT_HISTORY) {
    fake_levels[fake_count] = level;
    ++fake_count;
  }
}

void backlight_init(uint8_t level) {
  ++fake_inits;
  backlight_set_level(level);
}
