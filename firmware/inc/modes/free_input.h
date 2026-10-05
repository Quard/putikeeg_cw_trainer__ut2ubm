#ifndef MODES_FREE_INPUT_H
#define MODES_FREE_INPUT_H

/*
 * Free Input mode: keys with the paddle and draws the decoded text in rows 1-7 (references/internal/keyer_spec.md
 * section 6.11, references/internal/ui-ux.md section 5.9). Main loop only; only menu.c calls these functions.
 */

/** Takes P3.2 from the menu, sets the tone and starts the keyer from the settings. Draws nothing. */
void free_input_enter(void);

/** Reads the keyer events and draws the decoded characters. Reads at most KEYER_QUEUE_SIZE + 1 events. */
void free_input_poll(void);

/** Clears rows 1-7 and drops the character in progress. The keyer runs on. */
void free_input_clear(void);

/** Draws a delimiter line on the next free row. The character in progress stays. */
void free_input_delimiter(void);

/** Stops the keyer and gives P3.2 back to the menu. Draws nothing. */
void free_input_leave(void);

#endif /* MODES_FREE_INPUT_H */
