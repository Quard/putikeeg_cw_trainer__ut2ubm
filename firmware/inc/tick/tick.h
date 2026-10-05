#ifndef TICK_TICK_H
#define TICK_TICK_H

/*
 * 1 ms system tick on Timer 3. The counter is 16 bits and wraps from 65535 to 0, so intervals up to 65535 ms
 * are measured correctly with tick_elapsed_since().
 * tick_now() and the helpers are for the main loop only: a higher-priority interrupt that preempts the tick
 * interrupt in the middle of the increment would read a torn value.
 */

#include <stdbool.h>
#include <stdint.h>

#if defined(__SDCC_mcs51)
#include "mcu/stc8h.h"
#endif

#define TICK_HZ 1000U

/** Sets the counter to 0 and starts Timer 3 with its interrupt enabled. Does not change EA. */
void tick_init(void);

/** Returns the milliseconds since tick_init(). */
uint16_t tick_now(void);

/** Returns the milliseconds since @p start_ms, correct across one counter wrap. */
uint16_t tick_elapsed_since(uint16_t start_ms);

/** Returns true when at least @p duration_ms have passed since @p start_ms. A duration of 0 is true at once. */
bool tick_has_elapsed(uint16_t start_ms, uint16_t duration_ms);

#if defined(__SDCC_mcs51)
/* SDCC creates the interrupt vector only when main.c sees this declaration. */
void tick_isr(void) __interrupt(TIMER3_INTERRUPT);
#endif

#endif /* TICK_TICK_H */
