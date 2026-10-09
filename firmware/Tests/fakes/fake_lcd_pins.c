/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fake_lcd_pins.h"

#include <stdio.h>
#include <stdlib.h>

static fake_lcd_pin_event_t* fake_events;
static size_t fake_event_count;
static size_t fake_event_capacity;
static uint8_t fake_levels[FAKE_LCD_PIN_COUNT];
static bool fake_are_outputs;
static fake_lcd_pins_hook_t fake_hook;
static bool fake_is_in_hook;

static void fake_log(fake_lcd_pin_t pin, uint8_t level) {
  if (fake_is_in_hook) {
    fprintf(stderr, "fake_lcd_pins: pin %d written from the hook (interrupt handler)\n", (int)pin);
    abort();
  }
  if (fake_event_count == fake_event_capacity) {
    size_t new_capacity = (fake_event_capacity == 0U) ? 65536U : (fake_event_capacity * 2U);
    fake_lcd_pin_event_t* grown = realloc(fake_events, new_capacity * sizeof(*grown));
    if (grown == NULL) {
      fprintf(stderr, "fake_lcd_pins: out of memory\n");
      abort();
    }
    fake_events = grown;
    fake_event_capacity = new_capacity;
  }
  fake_events[fake_event_count].pin = pin;
  fake_events[fake_event_count].level = level;
  ++fake_event_count;
  if (fake_hook != NULL) {
    fake_is_in_hook = true;
    fake_hook(fake_event_count - 1U);
    fake_is_in_hook = false;
  }
}

void fake_lcd_pins_reset(void) {
  size_t pin;
  for (pin = 0U; pin < (size_t)FAKE_LCD_PIN_COUNT; ++pin) {
    fake_levels[pin] = 1U;
  }
  fake_are_outputs = false;
  fake_event_count = 0U;
  fake_hook = NULL;
  fake_is_in_hook = false;
}

void fake_lcd_pins_write(fake_lcd_pin_t pin, uint8_t level) {
  if ((unsigned)pin >= (unsigned)FAKE_LCD_PIN_COUNT) {
    fprintf(stderr, "fake_lcd_pins: invalid pin %d\n", (int)pin);
    abort();
  }
  fake_levels[pin] = (level != 0U) ? 1U : 0U;
  fake_log(pin, fake_levels[pin]);
}

void fake_lcd_pins_configure_outputs(void) {
  fake_are_outputs = true;
  fake_log(FAKE_LCD_EVENT_CONFIGURE_OUTPUTS, 1U);
}

bool fake_lcd_pins_are_outputs(void) { return fake_are_outputs; }

uint8_t fake_lcd_pins_level(fake_lcd_pin_t pin) {
  return ((unsigned)pin < (unsigned)FAKE_LCD_PIN_COUNT) ? fake_levels[pin] : 0U;
}

size_t fake_lcd_pins_event_count(void) { return fake_event_count; }

const fake_lcd_pin_event_t* fake_lcd_pins_event(size_t index) {
  return (index < fake_event_count) ? &fake_events[index] : NULL;
}

void fake_lcd_pins_set_hook(fake_lcd_pins_hook_t hook) { fake_hook = hook; }
