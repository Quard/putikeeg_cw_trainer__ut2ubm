#ifndef MODES_SENDING_H
#define MODES_SENDING_H

/*
 * Sending mode: the device shows an item, the user keys it, the keyer events are checked
 * (references/internal/training_spec.md section 12.3, ui-ux.md sections 5.13, 5.14 and 6.9). Main loop only; only
 * menu.c calls these functions.
 */

#include <stdbool.h>

#include "buttons/buttons.h"

/** Seeds the RNG, takes P3.2 from the menu, sets the tone, starts the keyer and a session at the TX speed. */
void sending_enter(void);
/** Draws the whole Sending screen for the current state. menu_render() calls it after the clear. */
void sending_render(void);
/** Reads at most KEYER_QUEUE_SIZE + 1 keyer events, runs the engine and redraws the rows that change. */
void sending_poll(void);
/** Applies a button event (section 12). Returns true when the user asks to leave. */
bool sending_handle(button_event_t event);
/** Stops the keyer and the session and gives P3.2 back to the menu. Draws nothing. */
void sending_leave(void);

#endif /* MODES_SENDING_H */
