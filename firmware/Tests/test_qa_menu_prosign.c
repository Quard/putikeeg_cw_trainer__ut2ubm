/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the value field after the Prosigns change (references/internal/keyer_spec.md sections 17.3.4 and
 * 17.4.3, ui-ux.md sections 5.3 and 5.10; AC-P14 to AC-P16). Every value and choice item of the tree shows every
 * value in full, in browse mode and in edit mode, so no value text is cut by the render buffer. The Prosigns row
 * shows the value of each version 2 record, also bits 2-3 = 3, and of a version 1 record.
 */

#include <string.h>

#include "backlight/backlight.h"
#include "buttons/buttons.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_menu_outputs.h"
#include "fake_tick.h"
#include "fixtures/font_golden.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "menu/menu.h"
#include "menu_internal.h"
#include "settings/settings.h"
#include "support/menu_splash.h"
#include "support/test_assert.h"
#include "util/crc8.h"

#define NEXT BUTTON_EVENT_NEXT
#define ENTER BUTTON_EVENT_ENTER
#define MENU BUTTON_EVENT_MENU
#define NO_BAND (-1)
#define VALUE_END_COLUMN 23
#define MARKER_COLUMN_BASE 21
#define TEXT_SIZE 32U

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

/* From the main menu after a power-on: the cursor goes to list @p node, item @p index. */
static void go_to_item(uint8_t node, uint8_t index) {
  send_times(NEXT, 3U - menu_cursor());
  send(ENTER);
  if (node == MENU_NODE_CW) {
    send(ENTER);
  } else if (node == MENU_NODE_DISPLAY) {
    send_times(NEXT, 4U);
    send(ENTER);
  }
  send_times(NEXT, index);
  TEST_ASSERT_EQUAL(node, menu_node());
  TEST_ASSERT_EQUAL(index, menu_cursor());
}

static bool golden_ink(char character, int column, int row) {
  const unsigned code = (unsigned char)character;
  if ((code < FONT_GOLDEN_5X8_FIRST) || (code > FONT_GOLDEN_5X8_LAST)) {
    return false;
  }
  return (font_golden_5x8[code - FONT_GOLDEN_5X8_FIRST].rows[row] & (1U << (4 - column))) != 0U;
}

typedef struct {
  int column;
  const char* text;
  bool is_inverted;
} row_text_t;

/* Every pixel of item row @p r: a band on x [band_x0, 127] (pixel rows 8r+1..8r+7) and the texts. */
static bool row_matches(int r, int band_x0, const row_text_t* texts, size_t count) {
  int x;
  int y;
  for (y = 8 * r; y < 8 * r + 8; ++y) {
    for (x = 0; x < 128; ++x) {
      const int cell_row = y - 8 * r;
      bool expected = (band_x0 >= 0) && (x >= band_x0) && (cell_row != 0);
      size_t i;
      for (i = 0U; i < count; ++i) {
        const int first_x = 2 + 5 * texts[i].column;
        const int length = (int)strlen(texts[i].text);
        if ((x >= first_x) && (x < first_x + 5 * length)) {
          const bool is_ink = golden_ink(texts[i].text[(x - first_x) / 5], (x - first_x) % 5, cell_row);
          expected = texts[i].is_inverted ? !is_ink : is_ink;
          if (cell_row == 0) {
            expected = expected && !texts[i].is_inverted;
          }
        }
      }
      if (lcd_fb_get_pixel((int16_t)x, (int16_t)y) != expected) {
        fprintf(stderr, "row %d: pixel (%d, %d) is %d\n", r, x, y, (int)!expected);
        return false;
      }
    }
  }
  return true;
}

/* ui-ux.md section 5.3: the value text of @p item at @p value, from the tree strings and the number. */
static void expected_value_text(const menu_item_t* item, uint16_t value, char* text) {
  if ((item->kind == MENU_KIND_CHOICE) || ((item->texts != NULL) && (value == 0U))) {
    (void)snprintf(text, TEXT_SIZE, "%s", item->texts[value]);
  } else if (item->unit != NULL) {
    (void)snprintf(text, TEXT_SIZE, "%u %s", (unsigned)value, item->unit);
  } else {
    (void)snprintf(text, TEXT_SIZE, "%u", (unsigned)value);
  }
}

static bool browse_row_is(int r, const menu_item_t* item, const char* value) {
  const row_text_t texts[2] = {{0, item->label, true}, {VALUE_END_COLUMN - (int)strlen(value), value, true}};
  return row_matches(r, 0, texts, 2U);
}

static bool edit_row_is(int r, const menu_item_t* item, const char* value, bool has_less, bool has_more) {
  const int fw = item->field_width;
  row_text_t texts[4];
  size_t count = 0U;
  texts[count++] = (row_text_t){0, item->label, false};
  texts[count++] = (row_text_t){VALUE_END_COLUMN - (int)strlen(value), value, true};
  if (has_less) {
    texts[count++] = (row_text_t){MARKER_COLUMN_BASE - fw, "<", true};
  }
  if (has_more) {
    texts[count++] = (row_text_t){24, ">", true};
  }
  return row_matches(r, 106 - 5 * fw, texts, count);
}

/* Power-on with an empty EEPROM, base speeds at 50 so that each effective speed reaches 50. */
static void power_on_wide(void) {
  fake_iap_reset();
  power_on();
  (void)settings_set(SETTINGS_ID_RX_WPM, 50U);
  (void)settings_set(SETTINGS_ID_TX_WPM, 50U);
}

/* AC-P15, AC-P16: each value of each value and choice item shows in full in browse mode and in edit mode. */
static void test_every_value_shows_in_full(void) {
  static const uint8_t nodes[3] = {MENU_NODE_SETTINGS, MENU_NODE_CW, MENU_NODE_DISPLAY};
  size_t node_index;
  unsigned long checked = 0U;
  size_t longest = 0U;

  for (node_index = 0U; node_index < 3U; ++node_index) {
    const menu_node_t* node = &menu_nodes[nodes[node_index]];
    uint8_t index;
    for (index = 0U; index < node->item_count; ++index) {
      const menu_item_t* item = &node->items[index];
      const int row = index + 1;
      uint16_t min;
      uint16_t max;
      uint16_t step;
      uint32_t value;
      if ((item->kind != MENU_KIND_VALUE) && (item->kind != MENU_KIND_CHOICE)) {
        continue;
      }
      power_on_wide();
      min = settings_min(item->target);
      max = settings_max(item->target);
      step = settings_step(item->target);
      for (value = min; value <= max; value += step) {
        char text[TEXT_SIZE];
        expected_value_text(item, (uint16_t)value, text);
        longest = (strlen(text) > longest) ? strlen(text) : longest;
        TEST_ASSERT(strlen(text) <= item->field_width);
        power_on_wide();
        (void)settings_set(item->target, (uint16_t)value);
        go_to_item(nodes[node_index], index);
        if (!TEST_ASSERT(browse_row_is(row, item, text))) {
          fprintf(stderr, "browse: %s = \"%s\"\n", item->label, text);
        }
        ++checked;
      }
      /* Edit mode: from min to max with next. */
      power_on_wide();
      (void)settings_set(item->target, min);
      go_to_item(nodes[node_index], index);
      send(ENTER);
      TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
      for (value = min; value <= max; value += step) {
        char text[TEXT_SIZE];
        expected_value_text(item, (uint16_t)value, text);
        TEST_ASSERT_EQUAL(value, settings_get(item->target));
        if (!TEST_ASSERT(edit_row_is(row, item, text, value > min, value < max))) {
          fprintf(stderr, "edit: %s = \"%s\"\n", item->label, text);
        }
        send(NEXT);
        ++checked;
      }
      send(MENU);
    }
  }
  printf("value fields: %lu checked, longest text %zu\n", checked, longest);
  TEST_ASSERT_EQUAL(11U, longest);
}

/* AC-P14: the Prosigns row of the list for each record value; bits 2-3 = 3 and a version 1 record give Both. */
static void test_prosigns_row_per_record(void) {
  static const uint8_t records[5][SETTINGS_RECORD_SIZE] = {
      {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0xB3U},
      {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x05U, 0x05U, 0x08U, 0x20U, 0xEBU},
      {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x03U},
      {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x0DU, 0x05U, 0x08U, 0x20U, 0x5BU},
      {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x0DU, 0x05U, 0x08U, 0x20U, 0xBCU},
  };
  static const char* const shown[5] = {"Punctuation", "Prosign", "Both", "Both", "Both"};
  const menu_item_t* item = &menu_nodes[MENU_NODE_SETTINGS].items[2];
  size_t which;

  TEST_ASSERT_EQUAL(0, strcmp("Prosigns", item->label));
  TEST_ASSERT_EQUAL(SETTINGS_ID_PROSIGN_STYLE, item->target);
  TEST_ASSERT_EQUAL(MENU_KIND_CHOICE, item->kind);
  TEST_ASSERT_EQUAL(11U, item->field_width);
  for (which = 0U; which < 5U; ++which) {
    TEST_ASSERT_EQUAL(records[which][14], crc8_compute(records[which], 14U));
    fake_iap_reset();
    memcpy(fake_iap_memory(), records[which], SETTINGS_RECORD_SIZE);
    power_on();
    go_to_item(MENU_NODE_SETTINGS, 2U);
    TEST_ASSERT(browse_row_is(3, item, shown[which]));
    send(ENTER);
    TEST_ASSERT(edit_row_is(3, item, shown[which], which != 0U, (which == 0U) || (which == 1U)));
    send(ENTER);
    send(MENU);
    TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  }
}

int main(void) {
  TEST_RUN(test_every_value_shows_in_full);
  TEST_RUN(test_prosigns_row_per_record);
  return TEST_FINISH();
}
