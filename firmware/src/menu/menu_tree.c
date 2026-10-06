#include <stddef.h>

#include "menu_internal.h"
#include "settings/settings.h"

/* Labels, values and field widths: references/internal/ui-ux.md section 5.3. */

#define MENU_ITEM_COUNT(items) ((uint8_t)(sizeof(items) / sizeof((items)[0])))

static const menu_text_t MEM_CODE menu_show_code_texts[2] = {"Off", "On"};
static const menu_text_t MEM_CODE menu_key_texts[3] = {"Straight", "Iambic A", "Iambic B"};
static const menu_text_t MEM_CODE menu_backlight_texts[1] = {"Off"};
static const menu_text_t MEM_CODE menu_paddle_texts[2] = {"Normal", "Reversed"};
/* Indexed by SETTINGS_PROSIGN_*. */
static const menu_text_t MEM_CODE menu_prosign_texts[3] = {"Punctuation", "Prosign", "Both"};

static const menu_item_t MEM_CODE menu_main_items[] = {
    {"Receiving", NULL, NULL, MENU_KIND_MODE, MENU_MODE_RECEIVING, 0U},
    {"Sending", NULL, NULL, MENU_KIND_MODE, MENU_MODE_SENDING, 0U},
    {"Free Input", NULL, NULL, MENU_KIND_MODE, MENU_MODE_FREE_INPUT, 0U},
    {"Settings", NULL, NULL, MENU_KIND_SUBMENU, MENU_NODE_SETTINGS, 0U},
};

static const menu_item_t MEM_CODE menu_settings_items[] = {
    {"CW", NULL, NULL, MENU_KIND_SUBMENU, MENU_NODE_CW, 0U},
    {"Show CW Code", NULL, menu_show_code_texts, MENU_KIND_CHOICE, SETTINGS_ID_SHOW_CW_CODE, 3U},
    {"Prosigns", NULL, menu_prosign_texts, MENU_KIND_CHOICE, SETTINGS_ID_PROSIGN_STYLE, 11U},
    {"Max Word Length", NULL, NULL, MENU_KIND_VALUE, SETTINGS_ID_MAX_WORD_LEN, 1U},
    {"Display", NULL, NULL, MENU_KIND_SUBMENU, MENU_NODE_DISPLAY, 0U},
    {"Reset Statistics", NULL, NULL, MENU_KIND_ACTION, MENU_ACTION_RESET_STATS, 0U},
};

static const menu_item_t MEM_CODE menu_cw_items[] = {
    {"Tone", "Hz", NULL, MENU_KIND_VALUE, SETTINGS_ID_TONE_HZ, 7U},
    {"RX Speed", "WPM", NULL, MENU_KIND_VALUE, SETTINGS_ID_RX_WPM, 6U},
    {"RX Eff. Speed", "WPM", NULL, MENU_KIND_VALUE, SETTINGS_ID_RX_EFF_WPM, 6U},
    {"TX Speed", "WPM", NULL, MENU_KIND_VALUE, SETTINGS_ID_TX_WPM, 6U},
    {"TX Eff. Speed", "WPM", NULL, MENU_KIND_VALUE, SETTINGS_ID_TX_EFF_WPM, 6U},
    {"Key Type", NULL, menu_key_texts, MENU_KIND_CHOICE, SETTINGS_ID_KEY_TYPE, 8U},
    {"Paddle", NULL, menu_paddle_texts, MENU_KIND_CHOICE, SETTINGS_ID_PADDLE_SWAP, 8U},
};

static const menu_item_t MEM_CODE menu_display_items[] = {
    {"Backlight", NULL, menu_backlight_texts, MENU_KIND_VALUE, SETTINGS_ID_BACKLIGHT, 3U},
    {"Contrast", NULL, NULL, MENU_KIND_VALUE, SETTINGS_ID_CONTRAST, 2U},
};

/* The menu has no scrolling (spec section 6.2), so no list may have more items than visible rows. */
_Static_assert(MENU_ITEM_COUNT(menu_main_items) <= MENU_VISIBLE_ROWS, "main menu needs scrolling");
_Static_assert(MENU_ITEM_COUNT(menu_settings_items) <= MENU_VISIBLE_ROWS, "Settings needs scrolling");
_Static_assert(MENU_ITEM_COUNT(menu_cw_items) <= MENU_VISIBLE_ROWS, "CW needs scrolling");
_Static_assert(MENU_ITEM_COUNT(menu_display_items) <= MENU_VISIBLE_ROWS, "Display needs scrolling");

/* Indexed by MENU_NODE_*. */
const menu_node_t MEM_CODE menu_nodes[MENU_NODE_COUNT] = {
    {"CW Trainer", menu_main_items, MENU_ITEM_COUNT(menu_main_items), false},
    {"Settings", menu_settings_items, MENU_ITEM_COUNT(menu_settings_items), true},
    {"Settings > CW", menu_cw_items, MENU_ITEM_COUNT(menu_cw_items), false},
    {"Settings > Display", menu_display_items, MENU_ITEM_COUNT(menu_display_items), false},
};

/* Indexed by MENU_MESSAGE_*. */
const menu_message_t MEM_CODE menu_messages[MENU_MESSAGE_COUNT] = {
    {"Reset Statistics", "Statistics reset", NULL},
    {"Settings", "Not saved", NULL},
    {"Display Defaults", "Default contrast and", "backlight are in use."},
};

const char MEM_CODE menu_text_not_ready[] = "Not ready";
const char MEM_CODE menu_text_reset_question[] = "Reset statistics?";
const char MEM_CODE menu_text_no[] = "No";
const char MEM_CODE menu_text_yes[] = "Yes";
const char MEM_CODE menu_text_ok[] = "OK";
