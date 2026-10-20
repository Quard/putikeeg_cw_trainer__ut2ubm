/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MENU_MENU_H
#define MENU_MENU_H

/*
 * Table-driven menu: the startup screen, lists, value edit, the About screen, the Free Input, Receiving and Sending
 * mode screens, the reset-statistics dialog and message screens (references/internal/menu_subsystem_spec.md
 * sections 6 and 17, references/internal/keyer_spec.md section 6.14, references/internal/training_spec.md
 * section 12.1, references/internal/ui-ux.md). Call from the main loop only; not reentrant.
 */

#include <stdbool.h>
#include <stdint.h>

#include "buttons/buttons.h"

#define MENU_SCREEN_LIST 0U
#define MENU_SCREEN_EDIT 1U
#define MENU_SCREEN_SPLASH 2U
#define MENU_SCREEN_DIALOG 3U
#define MENU_SCREEN_MESSAGE 4U
#define MENU_SCREEN_ABOUT 5U
#define MENU_SCREEN_FREE_INPUT 6U
#define MENU_SCREEN_RECEIVING 7U
#define MENU_SCREEN_SENDING 8U

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
#define MENU_TONE_PREVIEW_MS 300U /* Tone or Ramp preview; ui-ux.md section 6.5, decisions UX-21 and UX-73 */
#endif

#ifndef MENU_SPLASH_MS
#define MENU_SPLASH_MS 5000U /* ui-ux.md section 5.17.2, UX-59 */
#endif

/**
 * Shows the startup screen and requests a redraw. The main menu with the cursor on its first item is under it.
 * Starts the MENU_SPLASH_MS time. Call after settings_load().
 */
void menu_init(void);

/**
 * Applies @p event. A change of a visible item requests a redraw; BUTTON_EVENT_NONE does nothing. Any other event
 * on the startup screen only ends it. Free Input acts only on long events. Receiving and Sending get every event
 * except a long menu/back, which leaves them. The other screens ignore long events.
 */
void menu_handle(button_event_t event);

/**
 * Runs the time-based menu work: the end of the startup screen after MENU_SPLASH_MS, the mode screen work once its
 * frame is drawn, and the Tone or Ramp preview timeout. Main loop, each pass.
 */
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

/**
 * Shows screen MESSAGE with @p message_id over the current list and ends a Tone or Ramp preview edit. Ignores unknown
 * ids. On the startup screen, it only stores the id; the end of the startup screen shows the message.
 */
void menu_show_message(uint8_t message_id);

#endif /* MENU_MENU_H */
