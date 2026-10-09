/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MODES_RECEIVING_H
#define MODES_RECEIVING_H

/*
 * Receiving mode: the player plays an item, the user picks the answer with the group keyboard
 * (references/internal/training_spec.md section 12.2, ui-ux.md sections 5.12, 5.14 and 6.8). Main loop only; only
 * menu.c calls these functions.
 */

#include <stdbool.h>

#include "buttons/buttons.h"

/** Seeds the RNG, reads the settings, sets the tone and prepares the engine (phase SELECT). Tap mode stays off. Draws
 * nothing. */
void receiving_enter(void);
/** Draws the whole Receiving screen for the current state. menu_render() calls it after the clear. */
void receiving_render(void);
/** Runs the engine and the playback, and redraws the rows that change. */
void receiving_poll(void);
/** Applies a button event (section 12). Returns true when the user asks to leave. */
bool receiving_handle(button_event_t event);
/** Stops the playback and the session and turns tap mode off. Draws nothing. */
void receiving_leave(void);

#endif /* MODES_RECEIVING_H */
