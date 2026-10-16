/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MODES_TRAINING_SCREEN_H
#define MODES_TRAINING_SCREEN_H

/*
 * Drawing shared by Receiving and Sending (references/internal/training_spec.md section 5.11). Layouts: ui-ux.md
 * sections 5.11, 5.15 and 5.16 (U-7, U-10). Main loop only.
 */

#include <stdint.h>

#include "buttons/buttons.h"

typedef uint8_t training_select_action_t;
#define TRAINING_SELECT_NONE 0U  /* the event did nothing, or it only changed the selection */
#define TRAINING_SELECT_START 1U /* the user starts a session at the selected level */
#define TRAINING_SELECT_LEAVE 2U /* the user leaves the mode */

/**
 * Draws row 0 (ui-ux.md 5.11.1): mode tag, speeds, shown level, item number and count. In phase SELECT the level
 * and the item count follow the selection, and the item number is 1 (SL-6).
 */
void training_screen_draw_header(void);
/**
 * Draws rows 1-7 of the start screen (ui-ux.md 5.16.1, task D6): the selector, the label, the big line, the code
 * group in Sending, and the Start band. Does not draw row 0.
 */
void training_screen_draw_start(void);
/** Draws rows 1-7 of the session end screen from the engine state (SL-7). Does not draw the start screen (D-T47). */
void training_screen_draw_end(void);
/** In phase SELECT: redraws rows 0-5 for the selected level: the header and the start screen above the band (SL-4). */
void training_screen_draw_select(void);
/**
 * Applies @p event in phase SELECT or END (D-T38, D-T47). A long event: NONE, no redraw.
 * SELECT: prev or next, press or repeat: training_select_step() (next is up), then training_screen_draw_select()
 * when the level changed. enter press: START. menu/back press: LEAVE. Every other event: NONE, no redraw.
 * END: enter press: training_reopen_select(), training_screen_draw_header() and training_screen_draw_start(), then
 * NONE. menu/back press: LEAVE. Every other event, prev and next included: NONE, no redraw.
 */
training_select_action_t training_screen_select_input(button_event_t event);

#endif /* MODES_TRAINING_SCREEN_H */
