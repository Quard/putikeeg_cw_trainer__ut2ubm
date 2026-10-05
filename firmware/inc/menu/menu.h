#ifndef MENU_MENU_H
#define MENU_MENU_H

/*
 * Table-driven menu: lists, value edit, placeholder mode screens, the reset-statistics dialog and message screens
 * (references/internal/menu_subsystem_spec.md section 6, references/internal/ui-ux.md). Call from the main loop
 * only; not reentrant.
 */

#include <stdbool.h>
#include <stdint.h>

#include "buttons/buttons.h"

#define MENU_SCREEN_LIST 0U
#define MENU_SCREEN_EDIT 1U
#define MENU_SCREEN_PLACEHOLDER 2U
#define MENU_SCREEN_DIALOG 3U
#define MENU_SCREEN_MESSAGE 4U

#define MENU_NODE_MAIN 0U
#define MENU_NODE_SETTINGS 1U
#define MENU_NODE_CW 2U
#define MENU_NODE_DISPLAY 3U

#define MENU_DIALOG_NO 0U
#define MENU_DIALOG_YES 1U

#define MENU_MESSAGE_STATS_RESET 0U
#define MENU_MESSAGE_NOT_SAVED 1U
#define MENU_MESSAGE_DISPLAY_DEFAULTS 2U /* ui-ux.md section 8.1 */

#define MENU_VISIBLE_ROWS 7U
#define MENU_STACK_DEPTH 3U

#ifndef MENU_TONE_PREVIEW_MS
#define MENU_TONE_PREVIEW_MS 300U /* ui-ux.md section 6.5, decision UX-21 */
#endif

/** Shows the main menu with the cursor on its first item and requests a redraw. Call after settings_load(). */
void menu_init(void);

/** Applies @p event. A change of a visible item requests a redraw; BUTTON_EVENT_NONE does nothing. */
void menu_handle(button_event_t event);

/** Runs the time-based menu work: the Tone preview timeout. Main loop, each pass. Reads the tick only in a preview. */
void menu_poll(void);

/** Returns true while a redraw is pending. */
bool menu_needs_redraw(void);

/** Draws the full screen for the current state into lcd_fb and clears the redraw request. Does not flush. */
void menu_render(void);

uint8_t menu_screen(void);        /* MENU_SCREEN_* */
uint8_t menu_node(void);          /* list node on top of the stack */
uint8_t menu_cursor(void);        /* item index in that node */
uint8_t menu_dialog_choice(void); /* MENU_DIALOG_*; meaningful on MENU_SCREEN_DIALOG */
uint8_t menu_message(void);       /* MENU_MESSAGE_*; meaningful on MENU_SCREEN_MESSAGE */

/** Shows screen MESSAGE with @p message_id over the current list and ends a Tone edit. Ignores unknown ids. */
void menu_show_message(uint8_t message_id);

#endif /* MENU_MENU_H */
