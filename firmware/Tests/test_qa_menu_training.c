/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the Settings > Training list (references/internal/menu_subsystem_spec.md sections 19.1, 19.5 and 20.5,
 * AC-M19, AC-M20, AC-M28, AC-M29; training_spec.md D-T60, D-T65, note N6, Q8 mutant M13, Q9 mutant M16; ui-ux.md
 * section 5.19, UX-74, UX-75, UX-79 to UX-81). The real menu, buttons and settings run with the fake EEPROM. The
 * oracle is the item table of 19.1, 20.5 and 5.19: each item
 * edits only its own id through the buttons, an edit in the Training list saves once at the exit from Settings, and
 * a visit with no edit step writes nothing.
 */

#include <string.h>

#include "backlight/backlight.h"
#include "buttons/buttons.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_menu_outputs.h"
#include "fake_tick.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "menu/menu.h"
#include "menu_internal.h"
#include "settings/settings.h"
#include "support/menu_splash.h"
#include "support/test_assert.h"

#define NEXT BUTTON_EVENT_NEXT
#define PREV BUTTON_EVENT_PREV
#define ENTER BUTTON_EVENT_ENTER
#define MENU BUTTON_EVENT_MENU
#define MAIN_SETTINGS_ROW 3U
#define SETTINGS_TRAINING_ROW 1U
#define TRAINING_ITEM_COUNT 5U
#define MARKER_ITEM 4U

typedef struct {
  const char* label;
  uint8_t kind;
  uint8_t target;
  uint8_t field_width;
} expected_item_t;

/* ui-ux.md section 5.19, note N6 and UX-79. */
static const expected_item_t training_items[TRAINING_ITEM_COUNT] = {
    {"RX Attempts", MENU_KIND_CHOICE, SETTINGS_ID_RX_UNLIMITED, 9U},
    {"TX Attempts", MENU_KIND_CHOICE, SETTINGS_ID_TX_UNLIMITED, 9U},
    {"Max Word Length", MENU_KIND_VALUE, SETTINGS_ID_MAX_WORD_LEN, 1U},
    {"Show CW Code", MENU_KIND_CHOICE, SETTINGS_ID_SHOW_CW_CODE, 3U},
    {"TX Char Marker", MENU_KIND_CHOICE, SETTINGS_ID_TX_MARKER_HIDDEN, 3U},
};

static void power_on(void) {
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_delay_reset();
  lcd_fb_clear();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  (void)settings_load();
  fake_iap_clear_counters();
  lcd_set_contrast((uint8_t)settings_get(SETTINGS_ID_CONTRAST));
  backlight_init((uint8_t)settings_get(SETTINGS_ID_BACKLIGHT));
  gfx_init();
  menu_init();
  menu_test_skip_splash();
  if (menu_needs_redraw()) {
    menu_render();
  }
}

static void send(button_event_t event) {
  menu_handle(event);
  if (menu_needs_redraw()) {
    menu_render();
  }
}

static void send_times(button_event_t event, unsigned count) {
  unsigned i;
  for (i = 0U; i < count; ++i) {
    send(event);
  }
}

static size_t write_operations(void) { return fake_iap_command_count(2U) + fake_iap_command_count(3U); }

static void open_training_list(void) {
  send_times(NEXT, MAIN_SETTINGS_ROW - menu_cursor());
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  send_times(NEXT, SETTINGS_TRAINING_ROW);
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_TRAINING, menu_node());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
}

/* AC-M19, AC-M28: the tree of 19.1, 19.5 and 20.5. */
static void test_tree(void) {
  static const char* const settings_labels[5] = {"CW", "Training", "Prosigns", "Display", "Reset Statistics"};
  const menu_node_t* settings = &menu_nodes[MENU_NODE_SETTINGS];
  const menu_node_t* training = &menu_nodes[MENU_NODE_TRAINING];
  size_t index;

  TEST_ASSERT_EQUAL(5U, MENU_NODE_COUNT);
  TEST_ASSERT_EQUAL(5U, settings->item_count);
  for (index = 0U; index < 5U; ++index) {
    TEST_ASSERT_EQUAL(0, strcmp(settings_labels[index], settings->items[index].label));
  }
  TEST_ASSERT_EQUAL(MENU_KIND_SUBMENU, settings->items[SETTINGS_TRAINING_ROW].kind);
  TEST_ASSERT_EQUAL(MENU_NODE_TRAINING, settings->items[SETTINGS_TRAINING_ROW].target);
  TEST_ASSERT_EQUAL(SETTINGS_ID_PROSIGN_STYLE, settings->items[2].target);
  TEST_ASSERT_EQUAL(MENU_NODE_DISPLAY, settings->items[3].target);
  TEST_ASSERT(settings->is_save_on_exit);

  TEST_ASSERT_EQUAL(0, strcmp("Settings > Training", training->title));
  TEST_ASSERT(!training->is_save_on_exit);
  TEST_ASSERT_EQUAL(TRAINING_ITEM_COUNT, training->item_count);
  for (index = 0U; index < TRAINING_ITEM_COUNT; ++index) {
    const menu_item_t* item = &training->items[index];
    TEST_ASSERT_EQUAL(0, strcmp(training_items[index].label, item->label));
    TEST_ASSERT_EQUAL(training_items[index].kind, item->kind);
    TEST_ASSERT_EQUAL(training_items[index].target, item->target);
    TEST_ASSERT_EQUAL(training_items[index].field_width, item->field_width);
  }
  for (index = 0U; index < 2U; ++index) {
    const menu_item_t* item = &training->items[index];
    TEST_ASSERT_EQUAL(0, strcmp("3", item->texts[0]));
    TEST_ASSERT_EQUAL(0, strcmp("Unlimited", item->texts[1]));
  }
  /* M16: the text index is the stored "hidden" value, so 0 shows On. The list does not scroll (UX-81). */
  TEST_ASSERT_EQUAL(0, strcmp("On", training->items[MARKER_ITEM].texts[0]));
  TEST_ASSERT_EQUAL(0, strcmp("Off", training->items[MARKER_ITEM].texts[1]));
  TEST_ASSERT(training->item_count < MENU_VISIBLE_ROWS);
  /* No other node edits the moved ids, the attempt ids or the marker id. */
  for (index = 0U; index < MENU_NODE_COUNT; ++index) {
    uint8_t k;
    if (index == MENU_NODE_TRAINING) {
      continue;
    }
    for (k = 0U; k < menu_nodes[index].item_count; ++k) {
      const menu_item_t* item = &menu_nodes[index].items[k];
      if ((item->kind == MENU_KIND_VALUE) || (item->kind == MENU_KIND_CHOICE)) {
        TEST_ASSERT(item->target != SETTINGS_ID_RX_UNLIMITED);
        TEST_ASSERT(item->target != SETTINGS_ID_TX_UNLIMITED);
        TEST_ASSERT(item->target != SETTINGS_ID_MAX_WORD_LEN);
        TEST_ASSERT(item->target != SETTINGS_ID_SHOW_CW_CODE);
        TEST_ASSERT(item->target != SETTINGS_ID_TX_MARKER_HIDDEN);
      }
    }
  }
}

/* M13: a step of each Training item through the buttons changes only its own id. */
static void test_each_item_edits_its_own_id(void) {
  size_t index;
  for (index = 0U; index < TRAINING_ITEM_COUNT; ++index) {
    uint16_t before[SETTINGS_ID_COUNT];
    settings_id_t id;
    const settings_id_t target = training_items[index].target;
    const bool is_up = settings_get(target) < settings_max(target);
    fake_iap_reset();
    power_on();
    for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
      before[id] = settings_get(id);
    }
    open_training_list();
    send_times(NEXT, (unsigned)index);
    send(ENTER);
    TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
    send(is_up ? NEXT : PREV);
    send(ENTER);
    for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
      if (id == target) {
        TEST_ASSERT_EQUAL(is_up ? before[id] + 1U : before[id] - 1U, settings_get(id));
      } else if (!TEST_ASSERT_EQUAL(before[id], settings_get(id))) {
        fprintf(stderr, "item %zu changed id %u\n", index, (unsigned)id);
      }
    }
  }
}

/* AC-M20: edit, back to Settings with the cursor on Training, exit: one save; a visit with no edit writes nothing. */
static void test_save_rule(void) {
  fake_iap_reset();
  power_on();
  open_training_list();
  send(MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  TEST_ASSERT_EQUAL(SETTINGS_TRAINING_ROW, menu_cursor());
  TEST_ASSERT_EQUAL(0U, write_operations());
  send(MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(0U, write_operations());

  /* An edit and an edit back: no difference, no write. */
  open_training_list();
  send(ENTER);
  send(NEXT);
  send(PREV);
  send(ENTER);
  send(MENU);
  send(MENU);
  TEST_ASSERT_EQUAL(0U, write_operations());

  /* TX Attempts to Unlimited: no write in the Training list or at its exit, one save at the Settings exit. */
  open_training_list();
  send(NEXT);
  send(ENTER);
  send(NEXT);
  send(NEXT); /* no wrap at the last value */
  send(ENTER);
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_TX_UNLIMITED));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_RX_UNLIMITED));
  send(MENU);
  TEST_ASSERT_EQUAL(SETTINGS_TRAINING_ROW, menu_cursor());
  TEST_ASSERT_EQUAL(0U, write_operations());
  send(MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_command_count(3U));
  TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(2U));
  TEST_ASSERT_EQUAL(4U, fake_iap_memory()[2]);
  TEST_ASSERT_EQUAL(0x02U, fake_iap_memory()[14]);
  power_on();
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_TX_UNLIMITED));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_RX_UNLIMITED));
  TEST_ASSERT_EQUAL(0U, write_operations());

  /* AC-M29: TX Char Marker to Off: no write in the Training list, one save at the Settings exit, Off after a load. */
  fake_iap_reset();
  power_on();
  open_training_list();
  send_times(NEXT, MARKER_ITEM);
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
  send(PREV); /* no wrap below On */
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_TX_MARKER_HIDDEN));
  send(NEXT);
  send(NEXT); /* no wrap above Off */
  send(ENTER);
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_TX_MARKER_HIDDEN));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_TX_UNLIMITED));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_RX_UNLIMITED));
  send(MENU);
  TEST_ASSERT_EQUAL(SETTINGS_TRAINING_ROW, menu_cursor());
  TEST_ASSERT_EQUAL(0U, write_operations());
  send(MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_command_count(3U));
  TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(2U));
  TEST_ASSERT_EQUAL(4U, fake_iap_memory()[2]);
  TEST_ASSERT_EQUAL(0x04U, fake_iap_memory()[14]);
  TEST_ASSERT_EQUAL(0xC8U, fake_iap_memory()[15]);
  TEST_ASSERT_EQUAL(0x69U, fake_iap_memory()[10]);
  power_on();
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_TX_MARKER_HIDDEN));
  TEST_ASSERT_EQUAL(0U, write_operations());
}

/* F-27, AC-M15, AC-M24: after a version 3 record, a visit of both lists with no edit writes nothing. */
static void test_version_3_record_visit(void) {
  static const uint8_t v3_record[16] = {0x43, 0x57, 0x03, 0x58, 0x02, 0x20, 0x20, 0x14,
                                        0x14, 0x02, 0x68, 0x08, 0x08, 0x20, 0x8C, 0xFF};
  fake_iap_reset();
  memcpy(fake_iap_memory(), v3_record, sizeof(v3_record));
  power_on();
  TEST_ASSERT_EQUAL(8U, settings_get(SETTINGS_ID_MAX_WORD_LEN));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_TX_MARKER_HIDDEN));
  open_training_list();
  send_times(NEXT, TRAINING_ITEM_COUNT - 1U);
  send(MENU);
  send(MENU);
  TEST_ASSERT_EQUAL(0U, write_operations());
  TEST_ASSERT_EQUAL(0, memcmp(v3_record, fake_iap_memory(), sizeof(v3_record)));
}

int main(void) {
  TEST_RUN(test_tree);
  TEST_RUN(test_each_item_edits_its_own_id);
  TEST_RUN(test_save_rule);
  TEST_RUN(test_version_3_record_visit);
  return TEST_FINISH();
}
