/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FAKE_LCD_PINS_H
#define FAKE_LCD_PINS_H

/* Host fake of the LCD pins. It logs every pin write so tests can decode the bus traffic. */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  FAKE_LCD_PIN_CS,
  FAKE_LCD_PIN_RST,
  FAKE_LCD_PIN_A0,
  FAKE_LCD_PIN_SCK,
  FAKE_LCD_PIN_SDA,
  FAKE_LCD_PIN_COUNT
} fake_lcd_pin_t;

/* Event pin value that marks the call of fake_lcd_pins_configure_outputs(); its level is 1. */
#define FAKE_LCD_EVENT_CONFIGURE_OUTPUTS FAKE_LCD_PIN_COUNT

typedef struct {
  fake_lcd_pin_t pin;
  uint8_t level;
} fake_lcd_pin_event_t;

typedef void (*fake_lcd_pins_hook_t)(size_t event_index);

/** Sets all latches to 1, disables the outputs, clears the log and removes the hook. */
void fake_lcd_pins_reset(void);

/** Sets the latch of @p pin (any non-zero @p level is 1), logs one event and then runs the hook. */
void fake_lcd_pins_write(fake_lcd_pin_t pin, uint8_t level);

/** Enables the outputs and logs one FAKE_LCD_EVENT_CONFIGURE_OUTPUTS event. */
void fake_lcd_pins_configure_outputs(void);

bool fake_lcd_pins_are_outputs(void);

/** Returns the current latch level (0 or 1) of @p pin. */
uint8_t fake_lcd_pins_level(fake_lcd_pin_t pin);

size_t fake_lcd_pins_event_count(void);

/** Returns event @p index, or NULL when @p index is out of range. */
const fake_lcd_pin_event_t* fake_lcd_pins_event(size_t index);

/**
 * Sets a function that runs after every logged event with the index of that event. NULL removes it.
 * A pin write from inside the hook aborts the test program, because interrupt handlers must not drive LCD pins.
 */
void fake_lcd_pins_set_hook(fake_lcd_pins_hook_t hook);

#endif /* FAKE_LCD_PINS_H */
