/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MODES_FREE_TRAINING_H
#define MODES_FREE_TRAINING_H

/*
 * Callsigns program of Free Training: the configuration screen with Mode and Chars, then a free session of the
 * Receiving or Sending code (references/internal/training_spec.md section 23.5, ui-ux.md section 5.21). Main loop
 * only; only menu.c calls these functions.
 */

#include <stdbool.h>

#include "buttons/buttons.h"

/** Opens the configuration screen of Callsigns with Mode and Chars from RAM (D-T65). training_stop(). Draws nothing. */
void free_training_enter(void);
/** Draws the whole screen for the current state. menu_render() calls it after the clear. */
void free_training_render(void);
/** In a session (engine phase not IDLE): receiving_poll() or sending_poll() of the session mode. Else nothing. */
void free_training_poll(void);
/**
 * Applies @p event (never a long menu/back, section 23.6). Returns true when the user leaves to the Free Training
 * list. Configuration screen and end screen: the controls of ui-ux.md 5.21. GAP, ITEM and RESULT: receiving_handle()
 * or sending_handle().
 */
bool free_training_handle(button_event_t event);
/** Stops a running session with receiving_leave() or sending_leave(). Keeps Mode and Chars. Draws nothing. */
void free_training_leave(void);

#endif /* MODES_FREE_TRAINING_H */
