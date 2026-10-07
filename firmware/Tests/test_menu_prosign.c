/*
 * Menu scenarios of the Prosigns setting: ui-ux.md section 9.3, V-80 to V-83 (references/internal/keyer_spec.md
 * section 17.4.3, AC-P9, AC-P10, AC-P14 to AC-P16).
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
#include "support/test_assert.h"
#include "util/crc8.h"

#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define NEXT BUTTON_EVENT_NEXT
#define PREV BUTTON_EVENT_PREV
#define MENU BUTTON_EVENT_MENU
#define ENTER BUTTON_EVENT_ENTER
#define NO_BAND (-1)
#define PROSIGNS_ROW 3
#define PROSIGNS_FW 11
#define RECORD_VERSION 2U
#define RECORD_FLAGS 10U

/* keyer_spec.md section 17.4.2: layout version 1, Paddle Reversed. */
static const uint8_t reversed_record_v1[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                                 0x14U, 0x02U, 0x03U, 0x05U, 0x08U, 0x20U, 0x78U};

/* ---------- Start-up and input, following menu_subsystem_spec.md section 4.11 ---------- */

/* Power-on with the EEPROM as it is. */
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

static size_t eeprom_writes(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

/* From the main menu with any cursor: Settings is the last item. */
static void open_settings(void) {
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  send_times(NEXT, 3U - menu_cursor());
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
}

/* ---------- Probes of references/internal/ui-ux.md section 9 ---------- */

static bool pixel(int x, int y) { return lcd_fb_get_pixel((int16_t)x, (int16_t)y); }

static bool probe_sel(int r) {
  int x;
  int y;
  for (y = 8 * r + 1; y <= 8 * r + 7; ++y) {
    if (!pixel(0, y) || !pixel(1, y) || !pixel(127, y)) {
      return false;
    }
  }
  for (x = 0; x < 128; ++x) {
    if (pixel(x, 8 * r)) {
      return false;
    }
  }
  return true;
}

static bool probe_edit(int r, int fw) {
  int x;
  int y;
  for (y = 8 * r + 1; y <= 8 * r + 7; ++y) {
    if (pixel(0, y) || pixel(1, y) || pixel(105 - 5 * fw, y) || !pixel(106 - 5 * fw, y) || !pixel(127, y)) {
      return false;
    }
  }
  for (x = 106 - 5 * fw; x < 128; ++x) {
    if (pixel(x, 8 * r)) {
      return false;
    }
  }
  return true;
}

typedef struct {
  int column;
  const char* text;
  bool is_inverted;
} row_text_t;

static bool golden_ink(char character, int column, int row) {
  const uint8_t code = (uint8_t)character;
  if ((code < FONT_GOLDEN_5X8_FIRST) || (code > FONT_GOLDEN_5X8_LAST)) {
    return false;
  }
  return (font_golden_5x8[code - FONT_GOLDEN_5X8_FIRST].rows[row] & (1U << (4 - column))) != 0U;
}

/* Every pixel of text row @p r: a band on x [band_x0, band_x1] (rows 8r+1..8r+7, all rows for row 0) and the texts. */
static bool row_matches(int r, int band_x0, int band_x1, const row_text_t* texts, size_t count) {
  int x;
  int y;
  size_t i;
  for (y = 8 * r; y < 8 * r + 8; ++y) {
    for (x = 0; x < 128; ++x) {
      const int cell_row = y - 8 * r;
      bool expected = (band_x0 >= 0) && (x >= band_x0) && (x <= band_x1) && ((r == 0) || (cell_row != 0));
      for (i = 0U; i < count; ++i) {
        const int first_x = 2 + 5 * texts[i].column;
        const int length = (int)strlen(texts[i].text);
        if ((x >= first_x) && (x < first_x + 5 * length)) {
          const bool is_ink = golden_ink(texts[i].text[(x - first_x) / 5], (x - first_x) % 5, cell_row);
          expected = texts[i].is_inverted ? !is_ink : is_ink;
          if ((r != 0) && (cell_row == 0)) {
            expected = expected && !texts[i].is_inverted;
          }
        }
      }
      if (pixel(x, y) != expected) {
        fprintf(stderr, "row %d: pixel (%d, %d) is %d\n", r, x, y, (int)pixel(x, y));
        return false;
      }
    }
  }
  return true;
}

static bool title_is(const char* title) {
  const row_text_t text = {0, title, true};
  return row_matches(0, 0, 127, &text, 1U);
}

/* A list row: label in column 0, an optional value right-aligned to column 22, an optional '>' in column 24. */
static bool list_row_is(int r, const char* label, const char* value, bool has_submenu, bool is_selected) {
  row_text_t texts[3];
  size_t count = 0U;
  texts[count++] = (row_text_t){0, label, is_selected};
  if (value != NULL) {
    texts[count++] = (row_text_t){23 - (int)strlen(value), value, is_selected};
  }
  if (has_submenu) {
    texts[count++] = (row_text_t){24, ">", is_selected};
  }
  return row_matches(r, is_selected ? 0 : NO_BAND, 127, texts, count);
}

static bool prosigns_edit_is(const char* value, bool has_less, bool has_more) {
  row_text_t texts[4];
  size_t count = 0U;
  texts[count++] = (row_text_t){0, "Prosigns", false};
  texts[count++] = (row_text_t){23 - (int)strlen(value), value, true};
  if (has_less) {
    texts[count++] = (row_text_t){21 - PROSIGNS_FW, "<", true};
  }
  if (has_more) {
    texts[count++] = (row_text_t){24, ">", true};
  }
  return probe_edit(PROSIGNS_ROW, PROSIGNS_FW) && row_matches(PROSIGNS_ROW, 106 - 5 * PROSIGNS_FW, 127, texts, count);
}

static bool row_is_blank(int r) { return row_matches(r, NO_BAND, 0, NULL, 0U); }

/* The Settings list with @p prosigns in row 3 and the cursor on item @p selected. */
static bool settings_list_is(const char* prosigns, int selected) {
  return title_is("Settings") && list_row_is(1, "CW", NULL, true, selected == 0) &&
         list_row_is(2, "Show CW Code", "On", false, selected == 1) &&
         list_row_is(3, "Prosigns", prosigns, false, selected == 2) &&
         list_row_is(4, "Max Word Length", "5", false, selected == 3) &&
         list_row_is(5, "Display", NULL, true, selected == 4) &&
         list_row_is(6, "Reset Statistics", NULL, false, selected == 5) && row_is_blank(7);
}

/* ---------- Scenarios ---------- */

/* AC-P16: the value buffer holds the longest value or choice text of the tree. */
static void test_value_texts_fit(void) {
  static const uint8_t nodes[3] = {MENU_NODE_SETTINGS, MENU_NODE_CW, MENU_NODE_DISPLAY};
  size_t node;
  size_t longest = 0U;

  for (node = 0U; node < 3U; ++node) {
    const menu_node_t* list = &menu_nodes[nodes[node]];
    uint8_t index;
    for (index = 0U; index < list->item_count; ++index) {
      const menu_item_t* item = &list->items[index];
      uint16_t value;
      if ((item->kind != MENU_KIND_CHOICE) || (item->texts == NULL)) {
        continue;
      }
      for (value = 0U; value <= settings_max(item->target); ++value) {
        const size_t length = strlen(item->texts[value]);
        TEST_ASSERT(length <= item->field_width);
        longest = (length > longest) ? length : longest;
      }
    }
  }
  TEST_ASSERT_EQUAL(11U, longest);
  TEST_ASSERT_EQUAL(11U, strlen(menu_nodes[MENU_NODE_SETTINGS].items[2].texts[SETTINGS_PROSIGN_PUNCTUATION]));
  TEST_ASSERT(strlen("Prosigns") <= (size_t)(20 - PROSIGNS_FW));
}

/* V-80 (AC-P14) */
static void test_v80_settings_list(void) {
  fake_iap_reset();
  power_on();
  open_settings();
  TEST_ASSERT(probe_sel(1));
  TEST_ASSERT(settings_list_is("Both", 0));
  TEST_ASSERT_EQUAL(6U, menu_nodes[MENU_NODE_SETTINGS].item_count);
  send(PREV);
  TEST_ASSERT(probe_sel(6));
  TEST_ASSERT(settings_list_is("Both", 5));
  send(NEXT);
  TEST_ASSERT(probe_sel(1));
  TEST_ASSERT(settings_list_is("Both", 0));
  send_times(NEXT, 2U);
  TEST_ASSERT(probe_sel(3));
  TEST_ASSERT(settings_list_is("Both", 2));
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
}

/* V-81 (AC-P15) */
static void test_v81_edit(void) {
  fake_iap_reset();
  power_on();
  open_settings();
  send_times(NEXT, 2U);
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
  TEST_ASSERT(prosigns_edit_is("Both", true, false));
  send(PREV);
  TEST_ASSERT(prosigns_edit_is("Prosign", true, true));
  send(PREV);
  TEST_ASSERT(prosigns_edit_is("Punctuation", false, true));
  menu_handle(PREV);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_PUNCTUATION, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  send(NEXT);
  TEST_ASSERT(prosigns_edit_is("Prosign", true, true));
  send(NEXT);
  TEST_ASSERT(prosigns_edit_is("Both", true, false));
  menu_handle(NEXT);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(probe_sel(3));
  TEST_ASSERT(settings_list_is("Both", 2));
}

/* Edits Prosigns with @p moves (PREV or NEXT presses) from the Settings list, then leaves Settings. */
static void edit_prosigns_and_leave(button_event_t move, unsigned count) {
  open_settings();
  send_times(NEXT, 2U);
  send(ENTER);
  send_times(move, count);
  send(ENTER);
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
  send(MENU);
}

/* V-82 (AC-P15): one save per exit after a change, none without a change. */
static void test_v82_save(void) {
  static const char* const shown[3] = {"Punctuation", "Prosign", "Both"};
  static const button_event_t moves[3] = {PREV, NEXT, NEXT};
  static const unsigned counts[3] = {2U, 1U, 1U};
  uint8_t style;

  fake_iap_reset();
  power_on();
  open_settings();
  send(MENU);
  TEST_ASSERT_EQUAL(0U, eeprom_writes());

  for (style = 0U; style < 3U; ++style) {
    edit_prosigns_and_leave(moves[style], counts[style]);
    TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
    TEST_ASSERT(probe_sel(4));
    TEST_ASSERT_EQUAL(1U, fake_iap_command_count(CMD_ERASE));
    TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(CMD_PROGRAM));
    TEST_ASSERT_EQUAL(SETTINGS_LAYOUT_VERSION, fake_iap_memory()[RECORD_VERSION]);
    TEST_ASSERT_EQUAL(style, (fake_iap_memory()[RECORD_FLAGS] >> 2) & 0x03U);
    TEST_ASSERT_EQUAL(fake_iap_memory()[14], crc8_compute(fake_iap_memory(), 14U));
    power_on();
    TEST_ASSERT_EQUAL(style, settings_get(SETTINGS_ID_PROSIGN_STYLE));
    open_settings();
    TEST_ASSERT(settings_list_is(shown[style], 0));
    send(MENU);
    TEST_ASSERT_EQUAL(0U, eeprom_writes());
  }

  /* Both -> Prosign -> Both: an edit step, but no difference from the stored record. */
  open_settings();
  send_times(NEXT, 2U);
  send(ENTER);
  send(PREV);
  send(NEXT);
  send(ENTER);
  send(MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
}

/* V-83 (AC-P9, AC-P10): a layout version 1 record gives Both and keeps its values. */
static void test_v83_version_1_record(void) {
  static const char* const cw_values[7] = {"600 Hz", "32 WPM", "32 WPM", "20 WPM", "20 WPM", "Iambic B", "Reversed"};
  static const char* const cw_labels[7] = {"Tone",          "RX Speed", "RX Eff. Speed", "TX Speed",
                                           "TX Eff. Speed", "Key Type", "Paddle"};
  int i;

  fake_iap_reset();
  memcpy(fake_iap_memory(), reversed_record_v1, SETTINGS_RECORD_SIZE);
  power_on();
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  open_settings();
  TEST_ASSERT(settings_list_is("Both", 0));
  send(ENTER);
  TEST_ASSERT(title_is("Settings > CW"));
  for (i = 0; i < 7; ++i) {
    TEST_ASSERT(list_row_is(i + 1, cw_labels[i], cw_values[i], false, i == 0));
  }
  send(MENU);
  send(MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
  TEST_ASSERT_EQUAL(0x01U, fake_iap_memory()[RECORD_VERSION]);

  open_settings();
  send(ENTER);
  send(ENTER);
  send(NEXT);
  send(ENTER);
  send(MENU);
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
  send(MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_command_count(CMD_ERASE));
  TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT_EQUAL(0x02U, fake_iap_memory()[RECORD_VERSION]);
  TEST_ASSERT_EQUAL(0x0BU, fake_iap_memory()[RECORD_FLAGS]);
  TEST_ASSERT_EQUAL(0x62U, fake_iap_memory()[3]);
  TEST_ASSERT_EQUAL(0x02U, fake_iap_memory()[4]);

  power_on();
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  TEST_ASSERT_EQUAL(610U, settings_get(SETTINGS_ID_TONE_HZ));
  open_settings();
  TEST_ASSERT(settings_list_is("Both", 0));
  send(ENTER);
  TEST_ASSERT(list_row_is(1, "Tone", "610 Hz", false, true));
  TEST_ASSERT(list_row_is(7, "Paddle", "Reversed", false, false));
}

int main(void) {
  TEST_RUN(test_value_texts_fit);
  TEST_RUN(test_v80_settings_list);
  TEST_RUN(test_v81_edit);
  TEST_RUN(test_v82_save);
  TEST_RUN(test_v83_version_1_record);
  return TEST_FINISH();
}
