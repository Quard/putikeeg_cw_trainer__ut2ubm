/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fake_backlight_pwm.h"

static size_t fake_configures;
static uint16_t fake_prescaler;
static uint16_t fake_arr;
static size_t fake_compares;
static uint16_t fake_compare;
static size_t fake_early_writes;

void fake_backlight_pwm_reset(void) {
  fake_configures = 0U;
  fake_prescaler = 0U;
  fake_arr = 0U;
  fake_compares = 0U;
  fake_compare = 0U;
  fake_early_writes = 0U;
}

size_t fake_backlight_pwm_configure_count(void) { return fake_configures; }
uint16_t fake_backlight_pwm_prescaler(void) { return fake_prescaler; }
uint16_t fake_backlight_pwm_arr(void) { return fake_arr; }
size_t fake_backlight_pwm_compare_count(void) { return fake_compares; }
uint16_t fake_backlight_pwm_compare(void) { return fake_compare; }
size_t fake_backlight_pwm_writes_before_configure(void) { return fake_early_writes; }

void fake_backlight_pwm_configure(uint16_t prescaler, uint16_t arr) {
  ++fake_configures;
  fake_prescaler = prescaler;
  fake_arr = arr;
}

void fake_backlight_pwm_set_compare(uint16_t value) {
  if (fake_configures == 0U) {
    ++fake_early_writes;
  }
  ++fake_compares;
  fake_compare = value;
}
