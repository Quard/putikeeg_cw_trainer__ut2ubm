/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "menu/menu.h"

#include "backlight/backlight.h"
#include "lcd/lcd.h"
#include "menu_internal.h"
#include "modes/free_input.h"
#include "modes/receiving.h"
#include "modes/sending.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"
#include "stats/stats.h"
#include "tick/tick.h"

/* menu_handle() takes every screen from MENU_SCREEN_FREE_INPUT on as a mode screen. */
#if (MENU_SCREEN_RECEIVING <= MENU_SCREEN_FREE_INPUT) || (MENU_SCREEN_SENDING <= MENU_SCREEN_FREE_INPUT) || \
    (MENU_SCREEN_MESSAGE >= MENU_SCREEN_FREE_INPUT) || (MENU_SCREEN_DIALOG >= MENU_SCREEN_FREE_INPUT) ||    \
    (MENU_SCREEN_SPLASH >= MENU_SCREEN_FREE_INPUT) || (MENU_SCREEN_ABOUT >= MENU_SCREEN_FREE_INPUT)
#error "The mode screens must have the highest screen numbers"
#endif

MEM_XDATA uint8_t menu_stack_nodes[MENU_STACK_DEPTH];
MEM_XDATA uint8_t menu_stack_cursors[MENU_STACK_DEPTH];
MEM_XDATA uint8_t menu_stack_tops[MENU_STACK_DEPTH];
MEM_XDATA uint8_t menu_depth;
MEM_XDATA uint8_t menu_current_screen;
MEM_XDATA uint8_t menu_current_choice;
MEM_XDATA uint8_t menu_current_message;
MEM_XDATA bool menu_is_redraw_pending;

/* Working values of menu_handle(); SDCC would put them in internal RAM. */
static MEM_XDATA uint8_t handle_code;
static MEM_XDATA bool handle_is_repeat;
static MEM_XDATA bool handle_is_long;
static MEM_XDATA uint16_t edit_value;

/* Tone and Ramp preview, references/internal/ui-ux.md section 6.5. */
static MEM_XDATA bool menu_is_preview_running;
static MEM_XDATA uint16_t menu_preview_start;

/* Startup screen, references/internal/ui-ux.md section 5.17. */
static MEM_XDATA uint16_t menu_splash_start;

#define MENU_TOP ((uint8_t)(menu_depth - 1U))

static const menu_node_t MEM_CODE* menu_top_node(void) { return &menu_nodes[menu_stack_nodes[MENU_TOP]]; }

const menu_item_t MEM_CODE* menu_current_item(void) { return &menu_top_node()->items[menu_stack_cursors[MENU_TOP]]; }

static void menu_show_screen(uint8_t screen) {
  menu_current_screen = screen;
  menu_is_redraw_pending = true;
}

static bool menu_is_preview_item(settings_id_t id) {
  return (id == SETTINGS_ID_TONE_HZ) || (id == SETTINGS_ID_RAMP_MS);
}

/* EDIT opens only on value and choice items, so the settings id alone identifies a Tone or Ramp edit. */
static bool menu_is_preview_edit(void) {
  if (menu_current_screen != MENU_SCREEN_EDIT) {
    return false;
  }
  return menu_is_preview_item(menu_current_item()->target);
}

/* H1, H2: the ramp and the tone first, so the rise already uses them. */
static void menu_preview_play(settings_id_t id) {
  if (id == SETTINGS_ID_RAMP_MS) {
    sidetone_set_ramp_ms((uint8_t)settings_get(SETTINGS_ID_RAMP_MS));
  }
  sidetone_set_tone_hz(settings_get(SETTINGS_ID_TONE_HZ));
  sidetone_set_on(true);
  menu_preview_start = tick_now();
  menu_is_preview_running = true;
}

/* H5: every exit from a Tone or Ramp edit stops the tone, also after the timeout. */
static void menu_leave_preview_edit(void) {
  if (menu_is_preview_edit()) {
    sidetone_set_on(false);
    menu_is_preview_running = false;
  }
}

/*
 * Moves the list cursor. A press wraps at the ends; a repeat stops there. The window then moves only as far as the
 * cursor needs (ui-ux.md section 5.5).
 */
static void menu_move_cursor(bool is_down) {
  const uint8_t last = (uint8_t)(menu_top_node()->item_count - 1U);
  uint8_t cursor = menu_stack_cursors[MENU_TOP];
  uint8_t top;

  if (is_down) {
    if (cursor < last) {
      ++cursor;
    } else if (!handle_is_repeat) {
      cursor = 0U;
    }
  } else {
    if (cursor > 0U) {
      --cursor;
    } else if (!handle_is_repeat) {
      cursor = last;
    }
  }
  if (cursor != menu_stack_cursors[MENU_TOP]) {
    menu_stack_cursors[MENU_TOP] = cursor;
    top = menu_stack_tops[MENU_TOP];
    if (cursor < top) {
      top = cursor;
    } else if (cursor >= (uint8_t)(top + MENU_VISIBLE_ROWS)) {
      top = (uint8_t)(cursor - (MENU_VISIBLE_ROWS - 1U));
    }
    menu_stack_tops[MENU_TOP] = top;
    menu_is_redraw_pending = true;
  }
}

static void menu_enter_item(void) {
  const menu_item_t MEM_CODE* item = menu_current_item();

  if (item->kind == MENU_KIND_SUBMENU) {
    /* The tree has MENU_STACK_DEPTH levels, so a submenu never opens from the deepest level. */
    if (menu_depth < MENU_STACK_DEPTH) {
      menu_stack_nodes[menu_depth] = item->target;
      menu_stack_cursors[menu_depth] = 0U;
      menu_stack_tops[menu_depth] = 0U;
      ++menu_depth;
      menu_is_redraw_pending = true;
    }
  } else if (item->kind == MENU_KIND_MODE) {
    if (item->target == MENU_MODE_FREE_INPUT) {
      menu_show_screen(MENU_SCREEN_FREE_INPUT);
      free_input_enter();
    } else if (item->target == MENU_MODE_RECEIVING) {
      menu_show_screen(MENU_SCREEN_RECEIVING);
      receiving_enter();
    } else {
      menu_show_screen(MENU_SCREEN_SENDING);
      sending_enter();
    }
  } else if (item->kind == MENU_KIND_ACTION) {
    menu_current_choice = MENU_DIALOG_NO;
    menu_show_screen(MENU_SCREEN_DIALOG);
  } else if (item->kind == MENU_KIND_ABOUT) {
    menu_show_screen(MENU_SCREEN_ABOUT);
  } else {
    menu_show_screen(MENU_SCREEN_EDIT);
    if (menu_is_preview_item(item->target)) {
      menu_preview_play(item->target);
    }
  }
}

static void menu_go_back(void) {
  bool is_save_failed = false;

  if (menu_depth <= 1U) {
    return;
  }
  if (menu_top_node()->is_save_on_exit) {
    is_save_failed = (settings_save_if_changed() == SETTINGS_SAVE_FAILED);
  }
  --menu_depth;
  menu_is_redraw_pending = true;
  if (is_save_failed) {
    menu_show_message(MENU_MESSAGE_NOT_SAVED);
  }
}

static void menu_handle_list(void) {
  if (handle_code == BUTTON_EVENT_NEXT) {
    menu_move_cursor(true);
  } else if (handle_code == BUTTON_EVENT_PREV) {
    menu_move_cursor(false);
  } else if (handle_code == BUTTON_EVENT_ENTER) {
    menu_enter_item();
  } else if (handle_code == BUTTON_EVENT_MENU) {
    menu_go_back();
  }
}

static void menu_handle_edit(void) {
  const settings_id_t id = menu_current_item()->target;
  const uint16_t step = settings_step(id);

  if ((handle_code == BUTTON_EVENT_ENTER) || (handle_code == BUTTON_EVENT_MENU)) {
    menu_leave_preview_edit();
    menu_show_screen(MENU_SCREEN_LIST);
    return;
  }
  edit_value = settings_get(id);
  if (handle_code == BUTTON_EVENT_NEXT) {
    edit_value = (uint16_t)(edit_value + step);
  } else if (handle_code == BUTTON_EVENT_PREV) {
    edit_value = (edit_value < step) ? 0U : (uint16_t)(edit_value - step);
  } else {
    return;
  }
  if (!settings_set(id, edit_value)) {
    return;
  }
  /* Contrast and backlight apply at once; the other values only change in memory. */
  if (id == SETTINGS_ID_CONTRAST) {
    lcd_set_contrast((uint8_t)settings_get(id));
  } else if (id == SETTINGS_ID_BACKLIGHT) {
    backlight_set_level((uint8_t)settings_get(id));
  } else if (menu_is_preview_item(id)) {
    menu_preview_play(id);
  }
  menu_is_redraw_pending = true;
}

static void menu_handle_dialog(void) {
  if (handle_code == BUTTON_EVENT_NEXT) {
    if (menu_current_choice != MENU_DIALOG_YES) {
      menu_current_choice = MENU_DIALOG_YES;
      menu_is_redraw_pending = true;
    }
  } else if (handle_code == BUTTON_EVENT_PREV) {
    if (menu_current_choice != MENU_DIALOG_NO) {
      menu_current_choice = MENU_DIALOG_NO;
      menu_is_redraw_pending = true;
    }
  } else if ((handle_code == BUTTON_EVENT_ENTER) && (menu_current_choice == MENU_DIALOG_YES)) {
    menu_show_message(stats_reset() ? MENU_MESSAGE_STATS_RESET : MENU_MESSAGE_NOT_SAVED);
  } else {
    menu_show_screen(MENU_SCREEN_LIST);
  }
}

/* Stops the mode of a mode screen. Draws nothing. */
static void menu_leave_mode(void) {
  if (menu_current_screen == MENU_SCREEN_FREE_INPUT) {
    free_input_leave();
  } else if (menu_current_screen == MENU_SCREEN_RECEIVING) {
    receiving_leave();
  } else if (menu_current_screen == MENU_SCREEN_SENDING) {
    sending_leave();
  }
}

/*
 * A long menu/back leaves every mode screen. Free Input acts only on long presses (ui-ux.md sections 6.1 and 6.6).
 * Receiving and Sending get every other event (training_spec.md section 12.1).
 */
static void menu_handle_mode_screen(button_event_t event) {
  bool is_leaving = false;

  if (handle_is_long && (handle_code == BUTTON_EVENT_MENU)) {
    is_leaving = true;
  } else if (menu_current_screen == MENU_SCREEN_RECEIVING) {
    is_leaving = receiving_handle(event);
  } else if (menu_current_screen == MENU_SCREEN_SENDING) {
    is_leaving = sending_handle(event);
  } else if (handle_is_long) {
    if (handle_code == BUTTON_EVENT_PREV) {
      free_input_clear();
    } else if (handle_code == BUTTON_EVENT_NEXT) {
      free_input_delimiter();
    }
  }
  if (is_leaving) {
    menu_leave_mode();
    menu_show_screen(MENU_SCREEN_LIST);
  }
}

/* A message stored during the startup screen shows at its end (spec D-A9, ui-ux.md UX-60). */
static void menu_end_splash(void) {
  menu_show_screen((menu_current_message != MENU_MESSAGE_NONE) ? MENU_SCREEN_MESSAGE : MENU_SCREEN_LIST);
}

/*
 * Any event only ends the startup screen (UX-59). Lists, edit mode, the dialog, messages and About ignore long
 * events: the press of the same hold already acted (UX-24).
 */
static void menu_handle_other_screen(void) {
  if (menu_current_screen == MENU_SCREEN_SPLASH) {
    menu_end_splash();
    return;
  }
  if (handle_is_long) {
    return;
  }
  if (menu_current_screen == MENU_SCREEN_LIST) {
    menu_handle_list();
  } else if (menu_current_screen == MENU_SCREEN_EDIT) {
    menu_handle_edit();
  } else if (menu_current_screen == MENU_SCREEN_DIALOG) {
    menu_handle_dialog();
  } else if ((handle_code == BUTTON_EVENT_ENTER) || (handle_code == BUTTON_EVENT_MENU)) {
    menu_show_screen(MENU_SCREEN_LIST);
  }
}

void menu_init(void) {
  menu_stack_nodes[0] = MENU_NODE_MAIN;
  menu_stack_cursors[0] = 0U;
  menu_stack_tops[0] = 0U;
  menu_depth = 1U;
  menu_current_choice = MENU_DIALOG_NO;
  menu_current_message = MENU_MESSAGE_NONE;
  menu_is_preview_running = false;
  menu_splash_start = tick_now();
  menu_show_screen(MENU_SCREEN_SPLASH);
}

void menu_handle(button_event_t event) {
  handle_code = BUTTON_EVENT_CODE(event);
  handle_is_repeat = ((event & BUTTON_EVENT_REPEAT_FLAG) != 0U);
  handle_is_long = ((event & BUTTON_EVENT_LONG_FLAG) != 0U);
  if (handle_code == BUTTON_EVENT_NONE) {
    return;
  }
  if (menu_current_screen >= MENU_SCREEN_FREE_INPUT) {
    menu_handle_mode_screen(event);
  } else {
    menu_handle_other_screen();
  }
}

void menu_poll(void) {
  if ((menu_current_screen == MENU_SCREEN_SPLASH) && ((uint16_t)(tick_now() - menu_splash_start) >= MENU_SPLASH_MS)) {
    menu_end_splash();
  }
  /* After the frame: the mode never draws before its frame is drawn. */
  if (!menu_is_redraw_pending) {
    if (menu_current_screen == MENU_SCREEN_FREE_INPUT) {
      free_input_poll();
    } else if (menu_current_screen == MENU_SCREEN_RECEIVING) {
      receiving_poll();
    } else if (menu_current_screen == MENU_SCREEN_SENDING) {
      sending_poll();
    }
  }
  if (!menu_is_preview_running) {
    return;
  }
  if ((uint16_t)(tick_now() - menu_preview_start) >= MENU_TONE_PREVIEW_MS) {
    sidetone_set_on(false);
    menu_is_preview_running = false;
  }
}

bool menu_needs_redraw(void) { return menu_is_redraw_pending; }

uint8_t menu_screen(void) { return menu_current_screen; }

uint8_t menu_node(void) { return menu_stack_nodes[MENU_TOP]; }

uint8_t menu_cursor(void) { return menu_stack_cursors[MENU_TOP]; }

uint8_t menu_dialog_choice(void) { return menu_current_choice; }

uint8_t menu_message(void) { return menu_current_message; }

void menu_show_message(uint8_t message_id) {
  if (message_id >= MENU_MESSAGE_COUNT) {
    return;
  }
  menu_current_message = message_id;
  if (menu_current_screen == MENU_SCREEN_SPLASH) {
    return;
  }
  menu_leave_preview_edit();
  menu_leave_mode();
  menu_show_screen(MENU_SCREEN_MESSAGE);
}
