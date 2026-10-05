#include "tick/tick.h"

#include "tick_counter.h"

volatile uint16_t tick_count_ms;

/*
 * The 8051 reads the counter one byte at a time; two equal reads mean no tick came in between.
 * A macro, not a call, keeps the helpers leaf functions, so SDCC overlays their parameters.
 */
#define TICK_READ(snapshot)     \
  do {                          \
    (snapshot) = tick_count_ms; \
  } while ((snapshot) != tick_count_ms)

uint16_t tick_now(void) {
  uint16_t snapshot;
  TICK_READ(snapshot);
  return snapshot;
}

uint16_t tick_elapsed_since(uint16_t start_ms) {
  uint16_t snapshot;
  TICK_READ(snapshot);
  return (uint16_t)(snapshot - start_ms);
}

bool tick_has_elapsed(uint16_t start_ms, uint16_t duration_ms) {
  uint16_t snapshot;
  TICK_READ(snapshot);
  /* An if, not a comparison result, so that SDCC needs no bit variable. */
  if ((uint16_t)(snapshot - start_ms) < duration_ms) {
    return false;
  }
  return true;
}
