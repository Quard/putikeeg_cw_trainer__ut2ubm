#ifndef DELAY_DELAY_H
#define DELAY_DELAY_H

#include <stdint.h>

/**
 * Waits for at least @p ms milliseconds on the system tick: at most ms + 1 ms plus interrupt time.
 * A value of 0 returns at once.
 *
 * Call from the main loop only, after tick_init() and with interrupts enabled (EA = 1); otherwise the tick does
 * not advance and the call never returns.
 */
void delay_ms(uint8_t ms);

#endif /* DELAY_DELAY_H */
