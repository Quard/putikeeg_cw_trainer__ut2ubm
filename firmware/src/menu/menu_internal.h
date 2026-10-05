#ifndef MENU_MENU_INTERNAL_H
#define MENU_MENU_INTERNAL_H

/* Tree types and state shared by menu.c, menu_tree.c and menu_render.c. Private to menu. */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"
#include "menu/menu.h"

#define MENU_KIND_SUBMENU 0U
#define MENU_KIND_MODE 1U
#define MENU_KIND_VALUE 2U
#define MENU_KIND_CHOICE 3U
#define MENU_KIND_ACTION 4U

#define MENU_MODE_RECEIVING 0U
#define MENU_MODE_SENDING 1U
#define MENU_MODE_FREE_INPUT 2U

#define MENU_ACTION_RESET_STATS 0U

#define MENU_NODE_COUNT 4U
#define MENU_MESSAGE_COUNT 3U

typedef const char MEM_CODE* menu_text_t;

typedef struct {
  menu_text_t label;
  /* Value item: unit shown after one space, or NULL. */
  menu_text_t unit;
  /* Choice item: text of each value. Value item: text of value 0 (Backlight "Off"), or NULL. */
  const menu_text_t MEM_CODE* texts;
  uint8_t kind;
  /* Node id (submenu), mode id (mode), settings id (value, choice) or action id (action). */
  uint8_t target;
  /* Width in characters of the longest value text (ui-ux.md section 5.3). */
  uint8_t field_width;
} menu_item_t;

typedef struct {
  menu_text_t title;
  const menu_item_t MEM_CODE* items;
  uint8_t item_count;
  bool is_save_on_exit;
} menu_node_t;

typedef struct {
  menu_text_t title;
  menu_text_t first_line;
  /* NULL for a one-line message; a one-line message uses row 3, two lines use rows 2 and 3. */
  menu_text_t second_line;
} menu_message_t;

extern const menu_node_t MEM_CODE menu_nodes[MENU_NODE_COUNT];
extern const menu_message_t MEM_CODE menu_messages[MENU_MESSAGE_COUNT];
extern const char MEM_CODE menu_text_not_ready[];
extern const char MEM_CODE menu_text_reset_question[];
extern const char MEM_CODE menu_text_no[];
extern const char MEM_CODE menu_text_yes[];
extern const char MEM_CODE menu_text_ok[];

/* State. menu_depth is 1..MENU_STACK_DEPTH; level menu_depth - 1 is the top. */
extern MEM_XDATA uint8_t menu_stack_nodes[MENU_STACK_DEPTH];
extern MEM_XDATA uint8_t menu_stack_cursors[MENU_STACK_DEPTH];
extern MEM_XDATA uint8_t menu_depth;
extern MEM_XDATA uint8_t menu_current_screen;
extern MEM_XDATA uint8_t menu_current_choice;
extern MEM_XDATA uint8_t menu_current_message;
extern MEM_XDATA bool menu_is_redraw_pending;

/** Returns the item under the cursor of the top list. */
const menu_item_t MEM_CODE* menu_current_item(void);

#endif /* MENU_MENU_INTERNAL_H */
