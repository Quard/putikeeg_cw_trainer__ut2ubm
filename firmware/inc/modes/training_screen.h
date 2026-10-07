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
/** Draws the start screen in rows 1-7 (ui-ux.md 5.16.1): the selector, the description and the Start band. */
void training_screen_draw_start(void);
/** Draws the session end screen in rows 1-7 from the engine state, with the level selector (D-T38, SL-7). */
void training_screen_draw_end(void);
/**
 * Redraws the rows that show the selection: rows 4 and 5, and row 0 in phase SELECT (SL-4, D-T42 (4)).
 */
void training_screen_draw_select(void);
/**
 * Applies @p event in phase SELECT or END. prev or next, press or repeat: training_select_step() (next is up), then
 * training_screen_draw_select() when the level changed. enter press: START. menu/back press: LEAVE. A long event
 * and every other event: NONE, no redraw.
 */
training_select_action_t training_screen_select_input(button_event_t event);

#endif /* MODES_TRAINING_SCREEN_H */
