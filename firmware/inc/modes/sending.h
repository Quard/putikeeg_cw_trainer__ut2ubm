/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MODES_SENDING_H
#define MODES_SENDING_H

/*
 * Sending mode: the device shows an item, the user keys it, the keyer events are checked
 * (references/internal/training_spec.md section 12.3, ui-ux.md sections 5.13, 5.14 and 6.9). Main loop only; only
 * menu.c calls these functions.
 */

#include <stdbool.h>

#include "buttons/buttons.h"

/**
 * Seeds the RNG, reads the settings, sets the tone and prepares the engine (phase SELECT). No keyer: P3.2 stays with
 * the menu.
 */
void sending_enter(void);
/** Draws the whole Sending screen for the current state. menu_render() calls it after the clear. */
void sending_render(void);
/** Reads at most KEYER_QUEUE_SIZE + 1 keyer events, runs the engine and redraws the rows that change. */
void sending_poll(void);
/** Applies a button event (section 12). Returns true when the user asks to leave. */
bool sending_handle(button_event_t event);
/**
 * In GAP, ITEM and RESULT: stops the keyer and gives P3.2 back. In SELECT and END no keyer runs (D-T40). Then stops
 * the session. Draws nothing.
 */
void sending_leave(void);

#endif /* MODES_SENDING_H */
