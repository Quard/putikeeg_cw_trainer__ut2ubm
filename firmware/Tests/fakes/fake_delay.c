#include "fake_delay.h"

#include <stdio.h>
#include <stdlib.h>

#include "delay/delay.h"
#include "fake_lcd_pins.h"

#define FAKE_DELAY_CAPACITY 256U

static uint8_t fake_delay_ms_values[FAKE_DELAY_CAPACITY];
static size_t fake_delay_event_indexes[FAKE_DELAY_CAPACITY];
static size_t fake_delay_calls;

void delay_ms(uint8_t ms) {
  if (fake_delay_calls == FAKE_DELAY_CAPACITY) {
    fprintf(stderr, "fake_delay: more than %u calls without fake_delay_reset()\n", FAKE_DELAY_CAPACITY);
    abort();
  }
  fake_delay_ms_values[fake_delay_calls] = ms;
  fake_delay_event_indexes[fake_delay_calls] = fake_lcd_pins_event_count();
  ++fake_delay_calls;
}

void fake_delay_reset(void) { fake_delay_calls = 0U; }

size_t fake_delay_count(void) { return fake_delay_calls; }

uint8_t fake_delay_ms_at(size_t i) { return (i < fake_delay_calls) ? fake_delay_ms_values[i] : 0U; }

size_t fake_delay_event_index_at(size_t i) { return (i < fake_delay_calls) ? fake_delay_event_indexes[i] : 0U; }
