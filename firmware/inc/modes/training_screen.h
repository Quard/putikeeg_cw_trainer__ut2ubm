#ifndef MODES_TRAINING_SCREEN_H
#define MODES_TRAINING_SCREEN_H

/*
 * Drawing shared by Receiving and Sending (references/internal/training_spec.md section 5.11). Layouts: ui-ux.md
 * sections 5.11 and 5.15 (U-7, U-10). Main loop only.
 */

/** Draws row 0 (ui-ux.md 5.11.1): mode tag, speeds, shown level, training_shown_item_number() and the count. */
void training_screen_draw_header(void);
/** Draws the session end screen in rows 1-7 from the engine state. */
void training_screen_draw_end(void);

#endif /* MODES_TRAINING_SCREEN_H */
