#ifndef BUTTONS_BUTTONS_H
#define BUTTONS_BUTTONS_H

/*
 * Debounced press events of the four buttons, auto-repeat on prev and next and long presses of menu/back, prev and
 * next (references/internal/menu_subsystem_spec.md section 4.5, references/internal/keyer_spec.md section 6.12).
 * Call from the main loop only; not reentrant.
 */

#include <stdbool.h>
#include <stdint.h>

typedef uint8_t button_event_t;

#define BUTTON_EVENT_NONE 0x00U
#define BUTTON_EVENT_NEXT 0x01U
#define BUTTON_EVENT_PREV 0x02U
#define BUTTON_EVENT_MENU 0x03U /* menu/back */
#define BUTTON_EVENT_ENTER 0x04U
#define BUTTON_EVENT_REPEAT_FLAG 0x80U
#define BUTTON_EVENT_LONG_FLAG 0x40U
#define BUTTON_EVENT_CODE(event) ((uint8_t)((event) & 0x3FU))

/**
 * Configures the pins and pull-ups, checks for the display recovery start and takes the start state. Needs the
 * tick and EA = 1. Adds at most BUTTONS_RECOVERY_HOLD_MS + 1 ms, and only while MENU is held. A button held now
 * gives no event until it is released and pressed again.
 */
void buttons_init(void);

/** Returns true when MENU was held through the recovery window of buttons_init(). */
bool buttons_is_recovery_start(void);

/**
 * Returns at most one event: a pending press in the order MENU, ENTER, PREV, NEXT, else a due long press of MENU,
 * PREV or NEXT with BUTTON_EVENT_LONG_FLAG, else a due repeat of prev or next with BUTTON_EVENT_REPEAT_FLAG, else
 * BUTTON_EVENT_NONE. Calls tick_now() exactly once.
 */
button_event_t buttons_poll(void);

/** Gives P3.2 to the menu (true) or takes it away (false). Main loop only. */
void buttons_set_enter_enabled(bool is_enabled);

#endif /* BUTTONS_BUTTONS_H */
