#include "fake_tick.h"

#include "tick/tick.h"

static uint16_t fake_tick_value;
static uint16_t fake_tick_step;
static size_t fake_tick_reads;

void fake_tick_reset(uint16_t start_ms) {
  fake_tick_value = start_ms;
  fake_tick_step = 0U;
  fake_tick_reads = 0U;
}

void fake_tick_set_advance_per_read(uint16_t step_ms) { fake_tick_step = step_ms; }

size_t fake_tick_read_count(void) { return fake_tick_reads; }

uint16_t tick_now(void) {
  const uint16_t value = fake_tick_value;
  fake_tick_value = (uint16_t)(fake_tick_value + fake_tick_step);
  ++fake_tick_reads;
  return value;
}
