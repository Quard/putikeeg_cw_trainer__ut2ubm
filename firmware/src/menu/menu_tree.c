/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stddef.h>

#include "menu_internal.h"
#include "settings/settings.h"
#include "version.h"

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
    {"About", NULL, NULL, MENU_KIND_ABOUT, 0U, 0U},
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
    {"Ramp", "ms", NULL, MENU_KIND_VALUE, SETTINGS_ID_RAMP_MS, 5U},
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

/* Indexed by MENU_NODE_*. */
const menu_node_t MEM_CODE menu_nodes[MENU_NODE_COUNT] = {
    {menu_text_product, menu_main_items, MENU_ITEM_COUNT(menu_main_items), false},
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

const char MEM_CODE menu_text_reset_question[] = "Reset statistics?";
const char MEM_CODE menu_text_no[] = "No";
const char MEM_CODE menu_text_yes[] = "Yes";
const char MEM_CODE menu_text_ok[] = "OK";

/* Startup and About texts: references/internal/ui-ux.md sections 5.17 and 5.18. */

const char MEM_CODE menu_text_brand[] = "Putikeeg";
const char MEM_CODE menu_text_product[] = "CW Trainer";
const char MEM_CODE menu_text_mod[] = "mod by UT2UBM";
const char MEM_CODE menu_text_version_short[] = "v" FIRMWARE_VERSION;
const char MEM_CODE menu_text_version_long[] = "Version " FIRMWARE_VERSION;
const uint8_t MEM_CODE menu_text_version_short_length = (uint8_t)(sizeof(menu_text_version_short) - 1U);

/*
 * "Version " + version ends in column 24 at most, "v" + version uses at most 18 cells (UX-63). The check uses the
 * array, because SDCC puts a string literal in sizeof() into code memory.
 */
_Static_assert(sizeof(menu_text_version_short) - 2U <= 17U,
               "the version must have at most 17 characters (ui-ux.md UX-63)");

/* The link https://github.com/Quard/putikeeg_cw_trainer__ut2ubm, cut into rows of at most 25 characters. */
const menu_text_t MEM_CODE menu_about_link_rows[MENU_ABOUT_LINK_ROWS] = {
    "https://github.com/Quard/",
    "putikeeg_cw_trainer__",
    "ut2ubm",
};
