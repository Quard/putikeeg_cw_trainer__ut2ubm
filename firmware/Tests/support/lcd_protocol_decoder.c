/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lcd_protocol_decoder.h"

#include "fake_lcd_pins.h"

typedef struct {
  uint8_t levels[FAKE_LCD_PIN_COUNT];
  uint8_t shift;
  uint8_t bit_count;
} decoder_state_t;

static bool decoder_is_selected(const decoder_state_t* state) { return state->levels[FAKE_LCD_PIN_CS] == 0U; }

static size_t decoder_check(const decoder_state_t* state, fake_lcd_pin_t pin, uint8_t level) {
  const bool is_change = state->levels[pin] != level;
  const bool is_selected = decoder_is_selected(state);
  const bool is_sck_low = state->levels[FAKE_LCD_PIN_SCK] == 0U;

  switch (pin) {
    case FAKE_LCD_PIN_SDA:
      return (is_change && is_selected && !is_sck_low) ? 1U : 0U;
    case FAKE_LCD_PIN_A0:
      return (is_change && is_selected && ((state->bit_count != 0U) || is_sck_low)) ? 1U : 0U;
    case FAKE_LCD_PIN_CS: {
      size_t violations = (is_change && is_sck_low) ? 1U : 0U;
      if (is_change && (level != 0U) && (state->bit_count != 0U)) {
        ++violations;
      }
      if (is_change && (level == 0U) && (state->levels[FAKE_LCD_PIN_RST] == 0U)) {
        ++violations;
      }
      return violations;
    }
    case FAKE_LCD_PIN_RST:
      return (is_change && (level == 0U) && is_selected) ? 1U : 0U;
    default:
      return 0U;
  }
}

/* Applies one event; returns true and sets @p out when a byte completes. */
static bool decoder_apply(decoder_state_t* state, const fake_lcd_pin_event_t* event, lcd_decoded_byte_t* out) {
  const fake_lcd_pin_t pin = event->pin;
  const uint8_t level = event->level;
  bool is_byte_done = false;

  if ((unsigned)pin >= (unsigned)FAKE_LCD_PIN_COUNT) {
    return false;
  }
  if ((pin == FAKE_LCD_PIN_SCK) && (level != 0U) && (state->levels[FAKE_LCD_PIN_SCK] == 0U) &&
      decoder_is_selected(state)) {
    state->shift = (uint8_t)((state->shift << 1) | state->levels[FAKE_LCD_PIN_SDA]);
    ++state->bit_count;
    if (state->bit_count == 8U) {
      out->is_data = state->levels[FAKE_LCD_PIN_A0] != 0U;
      out->value = state->shift;
      state->bit_count = 0U;
      state->shift = 0U;
      is_byte_done = true;
    }
  }
  if ((pin == FAKE_LCD_PIN_CS) && (level != 0U)) {
    state->bit_count = 0U;
    state->shift = 0U;
  }
  state->levels[pin] = level;
  return is_byte_done;
}

size_t lcd_protocol_decode(size_t first_event, size_t end_event, lcd_decoded_byte_t* out, size_t capacity,
                           size_t* violation_count) {
  decoder_state_t state = {{1U, 1U, 1U, 1U, 1U}, 0U, 0U};
  const size_t event_count = fake_lcd_pins_event_count();
  size_t decoded = 0U;
  size_t violations = 0U;
  size_t index;
  lcd_decoded_byte_t byte;

  if (end_event > event_count) {
    end_event = event_count;
  }
  for (index = 0U; index < end_event; ++index) {
    const fake_lcd_pin_event_t* event = fake_lcd_pins_event(index);
    if (index < first_event) {
      if ((unsigned)event->pin < (unsigned)FAKE_LCD_PIN_COUNT) {
        state.levels[event->pin] = event->level;
      }
      continue;
    }
    if ((unsigned)event->pin < (unsigned)FAKE_LCD_PIN_COUNT) {
      violations += decoder_check(&state, event->pin, event->level);
    }
    if (decoder_apply(&state, event, &byte)) {
      if ((out != NULL) && (decoded < capacity)) {
        out[decoded] = byte;
      }
      ++decoded;
    }
  }
  if (violation_count != NULL) {
    *violation_count = violations;
  }
  return decoded;
}
