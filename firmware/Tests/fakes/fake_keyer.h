#ifndef FAKE_KEYER_H
#define FAKE_KEYER_H

/*
 * Host fake of cw/keyer.h for the menu and Free Input tests. It records the calls and returns the events of a FIFO
 * that the test fills. A FIFO overflow is a test failure: fake_keyer_is_overflowed() then returns true.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cw/keyer.h"

#define FAKE_KEYER_FIFO_SIZE 512U

/** Clears the FIFO, the counters and the last start arguments. */
void fake_keyer_reset(void);
void fake_keyer_push_event(keyer_event_t event);
size_t fake_keyer_init_count(void);
size_t fake_keyer_start_count(void);
size_t fake_keyer_stop_count(void);
/** Returns true after a keyer_start() and before the next keyer_stop(). */
bool fake_keyer_is_running(void);
uint8_t fake_keyer_last_key_type(void);
bool fake_keyer_last_is_swapped(void);
uint8_t fake_keyer_last_wpm(void);
uint8_t fake_keyer_last_eff_wpm(void);
/** Returns the keyer_read_event() calls since the reset. */
size_t fake_keyer_read_count(void);
/** Returns the events still in the FIFO. */
size_t fake_keyer_pending_count(void);
bool fake_keyer_is_overflowed(void);
/** Sets a hook that keyer_start() calls after it records its arguments, or NULL for none. */
void fake_keyer_set_start_hook(void (*hook)(void));
/** Sets a hook that keyer_stop() calls after it counts the call, or NULL for none. */
void fake_keyer_set_stop_hook(void (*hook)(void));

#endif /* FAKE_KEYER_H */
