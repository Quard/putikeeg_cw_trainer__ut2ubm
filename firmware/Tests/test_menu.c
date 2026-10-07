#include <string.h>

#include "backlight/backlight.h"
#include "board/button_pins.h"
#include "board/buttons_config.h"
#include "buttons/buttons.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_koch_item.h"
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
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))
#define LONG(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))
#define NO_BAND (-1)
#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)

static const uint8_t default_record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                             0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0x54U};
static uint16_t now;

/* ---------- Start-up and input, following menu_subsystem_spec.md section 4.11 ---------- */

static void put_record(uint8_t contrast, uint8_t backlight) {
  uint8_t record[SETTINGS_RECORD_SIZE];
  memcpy(record, default_record, sizeof(record));
  record[12] = backlight;
  record[13] = contrast;
  record[14] = crc8_compute(record, 14U);
  memcpy(fake_iap_memory(), record, sizeof(record));
}

/* Power-on with the EEPROM as it is and @p held_mask pressed; returns true for a recovery start. */
static bool power_on(uint8_t held_mask) {
  bool is_recovery;
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_button_pins_set(held_mask);
  fake_delay_reset();
  lcd_fb_clear();

  buttons_init();
  fake_tick_set_advance_per_read(0U);
  now = 2000U;
  (void)settings_load();
  fake_iap_clear_counters();
  is_recovery = buttons_is_recovery_start();
  if (is_recovery) {
    settings_use_display_defaults();
  }
  lcd_set_contrast((uint8_t)settings_get(SETTINGS_ID_CONTRAST));
  backlight_init((uint8_t)settings_get(SETTINGS_ID_BACKLIGHT));
  gfx_init();
  menu_init();
  if (is_recovery) {
    menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS);
  }
  if (menu_needs_redraw()) {
    menu_render();
  }
  return is_recovery;
}

static void start_empty(void) {
  fake_iap_reset();
  (void)power_on(0U);
}

/* One event as the main loop handles it. */
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

/* Main-loop passes with real buttons: @p mask held for @p duration ms. */
static void run_pins(uint8_t mask, uint16_t duration) {
  uint16_t i;
  unsigned events;
  button_event_t event;
  fake_button_pins_set(mask);
  for (i = 0U; i < duration; ++i) {
    fake_tick_set_now(now);
    for (events = 0U; events < 4U; ++events) {
      event = buttons_poll();
      if (event == BUTTON_EVENT_NONE) {
        break;
      }
      menu_handle(event);
    }
    if (menu_needs_redraw()) {
      menu_render();
    }
    now = (uint16_t)(now + 1U);
  }
}

static size_t eeprom_writes(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

/* ---------- Probes of references/internal/ui-ux.md section 9 ---------- */

static bool pixel(int x, int y) { return lcd_fb_get_pixel((int16_t)x, (int16_t)y); }

static bool probe_title(void) {
  int y;
  for (y = 0; y < 8; ++y) {
    if (!pixel(0, y) || !pixel(1, y) || !pixel(127, y)) {
      return false;
    }
  }
  return true;
}

static bool is_gap_line_off(int r) {
  int x;
  for (x = 0; x < 128; ++x) {
    if (pixel(x, 8 * r)) {
      return false;
    }
  }
  return true;
}

static bool probe_sel(int r) {
  int y;
  for (y = 8 * r + 1; y <= 8 * r + 7; ++y) {
    if (!pixel(0, y) || !pixel(1, y) || !pixel(127, y)) {
      return false;
    }
  }
  return is_gap_line_off(r);
}

static bool probe_plain(int r) {
  int y;
  for (y = 8 * r; y <= 8 * r + 7; ++y) {
    if (pixel(0, y) || pixel(1, y) || pixel(127, y)) {
      return false;
    }
  }
  return true;
}

/*
 * EDIT(r, fw). The gap line is checked over the band and the left margin only: the label stays in normal video,
 * so a label glyph with ink in cell row 0 (for example the f of "RX Eff. Speed") keeps that ink on row 8r, as
 * TEXT(r, 0, label) in normal video requires.
 */
static bool probe_edit(int r, int fw) {
  int x;
  int y;
  for (y = 8 * r + 1; y <= 8 * r + 7; ++y) {
    if (pixel(0, y) || pixel(1, y) || pixel(105 - 5 * fw, y) || !pixel(106 - 5 * fw, y) || !pixel(127, y)) {
      return false;
    }
  }
  for (x = 0; x < 128; ++x) {
    if (((x < 2) || (x >= 106 - 5 * fw)) && pixel(x, 8 * r)) {
      return false;
    }
  }
  return true;
}

static bool probe_button(int x0, int x1) {
  int y;
  for (y = 41; y <= 47; ++y) {
    if (!pixel(x0, y) || !pixel(x1, y)) {
      return false;
    }
  }
  return !pixel(x0 - 1, 44) && !pixel(x1 + 1, 44);
}

/* ---------- Full-row reference rendering with the golden font ---------- */

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

/*
 * Compares every pixel of text row @p r with the expected image: a band on x [band_x0, band_x1] (rows 8r+1..8r+7,
 * or all rows for the title row 0), the texts, and an off gap line for content rows.
 */
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
          const int index = (x - first_x) / 5;
          const bool is_ink = golden_ink(texts[i].text[index], (x - first_x) % 5, cell_row);
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

/* A list row: label in column 0 and, when @p value is not NULL, the value right-aligned to column 22. */
static bool list_row_is(int r, const char* label, const char* value, bool is_selected) {
  row_text_t texts[2];
  size_t count = 1U;
  texts[0].column = 0;
  texts[0].text = label;
  texts[0].is_inverted = is_selected;
  if (value != NULL) {
    texts[1].column = 23 - (int)strlen(value);
    texts[1].text = value;
    texts[1].is_inverted = is_selected;
    count = 2U;
  }
  return row_matches(r, is_selected ? 0 : NO_BAND, 127, texts, count);
}

static bool submenu_row_is(int r, const char* label, bool is_selected) {
  const row_text_t texts[2] = {{0, label, is_selected}, {24, ">", is_selected}};
  return row_matches(r, is_selected ? 0 : NO_BAND, 127, texts, 2U);
}

static bool edit_row_is(int r, const char* label, const char* value, int fw, bool has_less, bool has_more) {
  row_text_t texts[4];
  size_t count = 0U;
  texts[count].column = 0;
  texts[count].text = label;
  texts[count].is_inverted = false;
  ++count;
  texts[count].column = 23 - (int)strlen(value);
  texts[count].text = value;
  texts[count].is_inverted = true;
  ++count;
  if (has_less) {
    texts[count].column = 21 - fw;
    texts[count].text = "<";
    texts[count].is_inverted = true;
    ++count;
  }
  if (has_more) {
    texts[count].column = 24;
    texts[count].text = ">";
    texts[count].is_inverted = true;
    ++count;
  }
  return row_matches(r, 106 - 5 * fw, 127, texts, count);
}

static bool row_is_blank(int r) { return row_matches(r, NO_BAND, 0, NULL, 0U); }

static bool centered_row_is(int r, const char* text) {
  const row_text_t row_text = {(25 - (int)strlen(text)) / 2, text, false};
  return row_matches(r, NO_BAND, 0, &row_text, 1U);
}

static bool main_menu_is(int selected) {
  static const char* const labels[4] = {"Receiving", "Sending", "Free Input", "Settings"};
  int i;
  bool is_ok = title_is("CW Trainer");
  for (i = 0; i < 3; ++i) {
    is_ok = is_ok && list_row_is(i + 1, labels[i], NULL, i == selected);
  }
  is_ok = is_ok && submenu_row_is(4, labels[3], selected == 3);
  for (i = 5; i < 8; ++i) {
    is_ok = is_ok && row_is_blank(i);
  }
  return is_ok;
}

static bool cw_list_is(const char* const values[7], int selected) {
  static const char* const labels[7] = {"Tone",          "RX Speed", "RX Eff. Speed", "TX Speed",
                                        "TX Eff. Speed", "Key Type", "Paddle"};
  int i;
  bool is_ok = title_is("Settings > CW");
  for (i = 0; i < 7; ++i) {
    is_ok = is_ok && list_row_is(i + 1, labels[i], values[i], i == selected);
  }
  return is_ok;
}

static void open_cw(void) {
  send_times(PREV, 1U); /* Settings */
  send(ENTER);
  send(ENTER); /* CW */
}

static void open_display(void) {
  send(PREV);
  send(ENTER);
  send_times(NEXT, 4U);
  send(ENTER);
}

/* ---------- Scenarios ---------- */

static void test_v01_power_on_empty(void) {
  start_empty();
  TEST_ASSERT(probe_title());
  TEST_ASSERT(probe_sel(1));
  TEST_ASSERT(main_menu_is(0));
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT_EQUAL(32U, fake_lcd_contrast_last());
  TEST_ASSERT_EQUAL(8U, fake_backlight_last());
  TEST_ASSERT_EQUAL(1U, fake_backlight_init_count());
}

static void test_v02_bad_crc_record(void) {
  static const char* const cw_values[7] = {"600 Hz", "32 WPM", "32 WPM", "20 WPM", "20 WPM", "Iambic B", "Normal"};
  fake_iap_reset();
  put_record(5U, 3U);
  fake_iap_memory()[14] ^= 0xFFU;
  (void)power_on(0U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  open_cw();
  TEST_ASSERT(cw_list_is(cw_values, 0));
  send(MENU);
  TEST_ASSERT(title_is("Settings"));
  TEST_ASSERT(submenu_row_is(1, "CW", true));
  TEST_ASSERT(list_row_is(2, "Show CW Code", "On", false));
  TEST_ASSERT(list_row_is(3, "Prosigns", "Both", false));
  TEST_ASSERT(list_row_is(4, "Max Word Length", "5", false));
  TEST_ASSERT(submenu_row_is(5, "Display", false));
  TEST_ASSERT(list_row_is(6, "Reset Statistics", NULL, false));
  TEST_ASSERT(row_is_blank(7));
  send_times(NEXT, 4U);
  send(ENTER);
  TEST_ASSERT(title_is("Settings > Display"));
  TEST_ASSERT(list_row_is(1, "Backlight", "8", true));
  TEST_ASSERT(list_row_is(2, "Contrast", "32", false));
}

static void test_v03_wrap_on_press(void) {
  start_empty();
  send(PREV);
  TEST_ASSERT(probe_sel(4));
  TEST_ASSERT(probe_plain(1));
  TEST_ASSERT(main_menu_is(3));
  send(NEXT);
  TEST_ASSERT(probe_sel(1));
  TEST_ASSERT(main_menu_is(0));
}

static void test_v04_repeat_stops_at_end(void) {
  start_empty();
  run_pins(BUTTON_BIT_NEXT, 1000U);
  TEST_ASSERT_EQUAL(3U, menu_cursor());
  TEST_ASSERT(probe_sel(4));
  run_pins(0U, 100U);
  /* Repeat events stop at both ends; a press wraps. */
  send_times(REPEAT(NEXT), 3U);
  TEST_ASSERT_EQUAL(3U, menu_cursor());
  send_times(REPEAT(PREV), 5U);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  send(PREV);
  TEST_ASSERT_EQUAL(3U, menu_cursor());
  send(NEXT);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
}

static void test_v05_enter_and_back(void) {
  start_empty();
  send(PREV);
  send(ENTER);
  TEST_ASSERT(title_is("Settings"));
  TEST_ASSERT(probe_sel(1));
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  send(ENTER);
  TEST_ASSERT(title_is("Settings > CW"));
  TEST_ASSERT(probe_sel(1));
  send(MENU);
  TEST_ASSERT(title_is("Settings"));
  TEST_ASSERT(submenu_row_is(1, "CW", true));
  TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
}

static void test_v06_edit_tone(void) {
  start_empty();
  open_cw();
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
  TEST_ASSERT(probe_edit(1, 7));
  TEST_ASSERT(edit_row_is(1, "Tone", "600 Hz", 7, true, true));
  send(NEXT);
  TEST_ASSERT(edit_row_is(1, "Tone", "610 Hz", 7, true, true));
  send(PREV);
  TEST_ASSERT(edit_row_is(1, "Tone", "600 Hz", 7, true, true));
  send(PREV);
  TEST_ASSERT(edit_row_is(1, "Tone", "590 Hz", 7, true, true));
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(probe_sel(1));
  TEST_ASSERT(list_row_is(1, "Tone", "590 Hz", true));
  TEST_ASSERT(list_row_is(2, "RX Speed", "32 WPM", false));
}

static void test_v07_tone_limits(void) {
  start_empty();
  open_cw();
  send(ENTER);
  send_times(NEXT, 40U);
  TEST_ASSERT_EQUAL(1000U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT(edit_row_is(1, "Tone", "1000 Hz", 7, true, false));
  TEST_ASSERT(!menu_needs_redraw());
  menu_handle(NEXT);
  TEST_ASSERT(!menu_needs_redraw());
  send_times(PREV, 60U);
  TEST_ASSERT_EQUAL(400U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT(edit_row_is(1, "Tone", "400 Hz", 7, false, true));
  menu_handle(PREV);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(400U, settings_get(SETTINGS_ID_TONE_HZ));
}

static void test_v08_held_next_in_edit(void) {
  start_empty();
  open_cw();
  send(ENTER);
  run_pins(0U, 50U);
  run_pins(BUTTON_BIT_NEXT, BUTTONS_DEBOUNCE_MS + 1000U);
  run_pins(0U, 100U);
  /* 1 press + 8 repeats; the tick tolerance allows one step either way. */
  TEST_ASSERT(settings_get(SETTINGS_ID_TONE_HZ) >= 680U);
  TEST_ASSERT(settings_get(SETTINGS_ID_TONE_HZ) <= 700U);
  TEST_ASSERT_EQUAL(690U, settings_get(SETTINGS_ID_TONE_HZ));
}

static void test_v09_effective_at_base(void) {
  start_empty();
  open_cw();
  send_times(NEXT, 2U);
  send(ENTER);
  TEST_ASSERT(probe_edit(3, 6));
  send(NEXT);
  TEST_ASSERT(edit_row_is(3, "RX Eff. Speed", "32 WPM", 6, true, false));
  TEST_ASSERT_EQUAL(32U, settings_get(SETTINGS_ID_RX_EFF_WPM));
}

static void test_v10_lower_base_lowers_effective(void) {
  start_empty();
  open_cw();
  send_times(NEXT, 2U);
  send(ENTER);
  send_times(PREV, 7U);
  send(ENTER);
  TEST_ASSERT_EQUAL(25U, settings_get(SETTINGS_ID_RX_EFF_WPM));
  send(PREV);
  send(ENTER);
  send_times(PREV, 8U);
  TEST_ASSERT(edit_row_is(2, "RX Speed", "24 WPM", 6, true, true));
  TEST_ASSERT(list_row_is(3, "RX Eff. Speed", "24 WPM", false));
  send_times(NEXT, 6U);
  TEST_ASSERT(edit_row_is(2, "RX Speed", "30 WPM", 6, true, true));
  TEST_ASSERT(list_row_is(3, "RX Eff. Speed", "24 WPM", false));
}

static void test_v11_key_type_choices(void) {
  start_empty();
  open_cw();
  send_times(PREV, 2U);
  send(ENTER);
  TEST_ASSERT(probe_edit(6, 8));
  TEST_ASSERT(edit_row_is(6, "Key Type", "Iambic B", 8, true, false));
  send(PREV);
  TEST_ASSERT(edit_row_is(6, "Key Type", "Iambic A", 8, true, true));
  send(PREV);
  TEST_ASSERT(edit_row_is(6, "Key Type", "Straight", 8, false, true));
  menu_handle(PREV);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(SETTINGS_KEY_STRAIGHT, settings_get(SETTINGS_ID_KEY_TYPE));
  send_times(NEXT, 5U);
  TEST_ASSERT_EQUAL(SETTINGS_KEY_IAMBIC_B, settings_get(SETTINGS_ID_KEY_TYPE));
}

static void test_v12_show_cw_code(void) {
  start_empty();
  send(PREV);
  send(ENTER);
  send(NEXT);
  send(ENTER);
  TEST_ASSERT(probe_edit(2, 3));
  TEST_ASSERT(edit_row_is(2, "Show CW Code", "On", 3, true, false));
  send(PREV);
  TEST_ASSERT(edit_row_is(2, "Show CW Code", "Off", 3, false, true));
  menu_handle(PREV);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
}

static void test_v13_backlight_edit(void) {
  static const char* const shown[11] = {"Off", "1", "2", "3", "4", "5", "6", "7", "8", "9", "10"};
  int level;
  size_t calls;
  start_empty();
  open_display();
  send(ENTER);
  TEST_ASSERT(probe_edit(1, 3));
  calls = fake_backlight_count();
  for (level = 7; level >= 0; --level) {
    send(PREV);
    TEST_ASSERT_EQUAL((uint8_t)level, fake_backlight_last());
    TEST_ASSERT(edit_row_is(1, "Backlight", shown[level], 3, level > 0, true));
  }
  TEST_ASSERT_EQUAL(calls + 8U, fake_backlight_count());
  menu_handle(PREV);
  TEST_ASSERT_EQUAL(calls + 8U, fake_backlight_count());
  for (level = 1; level <= 10; ++level) {
    send(NEXT);
    TEST_ASSERT_EQUAL((uint8_t)level, fake_backlight_last());
    TEST_ASSERT(edit_row_is(1, "Backlight", shown[level], 3, true, level < 10));
  }
  send(NEXT);
  TEST_ASSERT_EQUAL(calls + 18U, fake_backlight_count());
  TEST_ASSERT_EQUAL(32U, fake_lcd_contrast_last());
}

static void test_v14_contrast_edit(void) {
  int value;
  size_t calls;
  start_empty();
  open_display();
  send(NEXT);
  send(ENTER);
  TEST_ASSERT(probe_edit(2, 2));
  calls = fake_lcd_contrast_count();
  for (value = 31; value >= 0; --value) {
    send(REPEAT(PREV));
    TEST_ASSERT_EQUAL((uint8_t)value, fake_lcd_contrast_last());
  }
  send(PREV);
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_CONTRAST));
  TEST_ASSERT(edit_row_is(2, "Contrast", "0", 2, false, true));
  for (value = 1; value <= 63; ++value) {
    send(NEXT);
    TEST_ASSERT_EQUAL((uint8_t)value, fake_lcd_contrast_last());
  }
  send(NEXT);
  TEST_ASSERT_EQUAL(63U, settings_get(SETTINGS_ID_CONTRAST));
  TEST_ASSERT(edit_row_is(2, "Contrast", "63", 2, true, false));
  TEST_ASSERT_EQUAL(calls + 32U + 63U, fake_lcd_contrast_count());
  TEST_ASSERT_EQUAL(1U, fake_backlight_count());
}

static void test_v15_save_on_settings_exit(void) {
  start_empty();
  open_cw();
  send(ENTER);
  send(NEXT);
  send(MENU);
  send(MENU);
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
  send(MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
  TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT(main_menu_is(3));
  TEST_ASSERT(probe_sel(4));
  (void)power_on(0U);
  TEST_ASSERT_EQUAL(610U, settings_get(SETTINGS_ID_TONE_HZ));
}

static void test_v16_no_save_without_change(void) {
  start_empty();
  send(PREV);
  send(ENTER);
  send(MENU);
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
  send(ENTER);
  send(ENTER);
  send(ENTER);
  send(NEXT);
  send(PREV);
  send(ENTER);
  send(MENU);
  send(MENU);
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
  TEST_ASSERT(main_menu_is(3));
}

static void test_v17_menu_leaves_edit_only(void) {
  start_empty();
  open_cw();
  send(ENTER);
  send_times(NEXT, 2U);
  send(MENU);
  TEST_ASSERT(title_is("Settings > CW"));
  TEST_ASSERT(probe_sel(1));
  TEST_ASSERT(list_row_is(1, "Tone", "620 Hz", true));
  TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
  /* MENU from CW to Settings does not save either. */
  send(MENU);
  TEST_ASSERT_EQUAL(0U, fake_iap_operation_count());
}

static void test_v18_menu_at_root(void) {
  static uint8_t before[FB_SIZE];
  uint8_t page;
  start_empty();
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
    lcd_fb_clear_dirty(page);
  }
  menu_handle(MENU);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    TEST_ASSERT_EQUAL(0, memcmp(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH));
  }
}

/* D-T8: Receiving and Sending open their training screens. The start pause ignores the short and long events. */
static void check_training_screen(int index, uint8_t screen, const char* header) {
  int r;
  start_empty();
  send_times(NEXT, (unsigned)index);
  send(ENTER);
  TEST_ASSERT_EQUAL(screen, menu_screen());
  TEST_ASSERT(title_is(header));
  for (r = 1; r <= 7; ++r) {
    TEST_ASSERT(probe_plain(r));
  }
  menu_handle(PREV);
  menu_handle(NEXT);
  menu_handle(ENTER);
  menu_handle(MENU);
  menu_handle(REPEAT(NEXT));
  menu_handle(LONG(PREV));
  menu_handle(LONG(NEXT));
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(screen, menu_screen());
  TEST_ASSERT(title_is(header));
  send(LONG(MENU));
  TEST_ASSERT(main_menu_is(index));
  TEST_ASSERT(probe_sel(index + 1));
}

static void test_v19_training_screens(void) {
  fake_koch_item_reset();
  fake_koch_item_push("K");
  check_training_screen(0, MENU_SCREEN_RECEIVING, "RX 32/32 L1          1/50");
  check_training_screen(1, MENU_SCREEN_SENDING, "TX 20/20 L1          1/50");
}

static void open_dialog(void) {
  send(PREV);
  send(ENTER);
  send(PREV);
  send(ENTER);
}

static bool dialog_is(bool is_yes) {
  const row_text_t no_text = {8, "No", !is_yes};
  const row_text_t yes_text = {14, "Yes", is_yes};
  row_text_t both[2];
  bool is_ok = title_is("Reset Statistics") && centered_row_is(3, "Reset statistics?");
  both[0] = no_text;
  both[1] = yes_text;
  if (is_yes) {
    is_ok = is_ok && row_matches(5, 67, 91, both, 2U) && probe_button(67, 91);
  } else {
    is_ok = is_ok && row_matches(5, 37, 56, both, 2U) && probe_button(37, 56);
  }
  return is_ok && row_is_blank(1) && row_is_blank(2) && row_is_blank(4) && row_is_blank(6) && row_is_blank(7);
}

static void test_v20_dialog_no(void) {
  start_empty();
  open_dialog();
  TEST_ASSERT_EQUAL(MENU_SCREEN_DIALOG, menu_screen());
  TEST_ASSERT_EQUAL(MENU_DIALOG_NO, menu_dialog_choice());
  TEST_ASSERT(dialog_is(false));
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(title_is("Settings"));
  TEST_ASSERT(probe_sel(6));
  TEST_ASSERT(list_row_is(6, "Reset Statistics", NULL, true));
  TEST_ASSERT_EQUAL(0U, fake_stats_reset_count());
}

static void test_v21_dialog_yes(void) {
  start_empty();
  open_dialog();
  send(NEXT);
  TEST_ASSERT(dialog_is(true));
  menu_handle(NEXT);
  TEST_ASSERT(!menu_needs_redraw());
  send(PREV);
  TEST_ASSERT(dialog_is(false));
  menu_handle(PREV);
  TEST_ASSERT(!menu_needs_redraw());
  send(NEXT);
  send(ENTER);
  TEST_ASSERT_EQUAL(1U, fake_stats_reset_count());
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_STATS_RESET, menu_message());
  TEST_ASSERT(title_is("Reset Statistics"));
  TEST_ASSERT(centered_row_is(3, "Statistics reset"));
  TEST_ASSERT(probe_button(52, 71));
  {
    const row_text_t ok = {11, "OK", true};
    TEST_ASSERT(row_matches(5, 52, 71, &ok, 1U));
  }
  menu_handle(NEXT);
  menu_handle(PREV);
  TEST_ASSERT(!menu_needs_redraw());
  send(ENTER);
  TEST_ASSERT(title_is("Settings"));
  TEST_ASSERT(probe_sel(6));
  TEST_ASSERT_EQUAL(1U, fake_stats_reset_count());
}

static void test_v22_v23_dialog_menu_is_no(void) {
  start_empty();
  open_dialog();
  send(NEXT);
  send(MENU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(probe_sel(6));
  TEST_ASSERT_EQUAL(0U, fake_stats_reset_count());
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_DIALOG_NO, menu_dialog_choice());
  TEST_ASSERT(dialog_is(false));
}

static void test_v27_clamped_record(void) {
  uint8_t record[SETTINGS_RECORD_SIZE];
  memcpy(record, default_record, sizeof(record));
  record[3] = 0xDCU; /* 1500 Hz */
  record[4] = 0x05U;
  record[5] = 30U;
  record[6] = 40U;
  record[14] = crc8_compute(record, 14U);
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, sizeof(record));
  (void)power_on(0U);
  open_cw();
  TEST_ASSERT(list_row_is(1, "Tone", "1000 Hz", true));
  TEST_ASSERT(list_row_is(2, "RX Speed", "30 WPM", false));
  TEST_ASSERT(list_row_is(3, "RX Eff. Speed", "30 WPM", false));
}

static void test_v28_power_off_inside_settings(void) {
  start_empty();
  open_cw();
  send(ENTER);
  send(NEXT);
  TEST_ASSERT_EQUAL(610U, settings_get(SETTINGS_ID_TONE_HZ));
  (void)power_on(0U);
  TEST_ASSERT_EQUAL(600U, settings_get(SETTINGS_ID_TONE_HZ));
}

static void test_v29_gap_line_under_title(void) {
  int x;
  start_empty();
  TEST_ASSERT(probe_title());
  for (x = 0; x < 128; ++x) {
    TEST_ASSERT(!pixel(x, 8));
  }
}

static bool is_ascii_text(const char MEM_CODE* text, size_t max_length) {
  size_t length = 0U;
  while (text[length] != '\0') {
    if ((text[length] < 0x20) || (text[length] > 0x7E)) {
      return false;
    }
    ++length;
  }
  return length <= max_length;
}

static void test_v32_strings_fit(void) {
  uint8_t node;
  uint8_t index;
  for (node = 0U; node < MENU_NODE_COUNT; ++node) {
    const menu_node_t MEM_CODE* list = &menu_nodes[node];
    TEST_ASSERT(is_ascii_text(list->title, 25U));
    TEST_ASSERT(list->item_count <= MENU_VISIBLE_ROWS);
    for (index = 0U; index < list->item_count; ++index) {
      const menu_item_t MEM_CODE* item = &list->items[index];
      if ((item->kind == MENU_KIND_VALUE) || (item->kind == MENU_KIND_CHOICE)) {
        TEST_ASSERT(is_ascii_text(item->label, (size_t)(20U - item->field_width)));
      } else {
        TEST_ASSERT(is_ascii_text(item->label, 23U));
      }
      if (item->kind == MENU_KIND_CHOICE) {
        uint16_t value;
        for (value = settings_min(item->target); value <= settings_max(item->target); ++value) {
          TEST_ASSERT(is_ascii_text(item->texts[value], item->field_width));
        }
      }
    }
  }
  for (index = 0U; index < MENU_MESSAGE_COUNT; ++index) {
    TEST_ASSERT(is_ascii_text(menu_messages[index].title, 25U));
    TEST_ASSERT(is_ascii_text(menu_messages[index].first_line, 25U));
    if (menu_messages[index].second_line != NULL) {
      TEST_ASSERT(is_ascii_text(menu_messages[index].second_line, 25U));
    }
  }
}

/* Every value at its widest still fits its field: edit each value item to its min and max. */
static void test_edit_every_item_to_limits(void) {
  uint8_t node;
  uint8_t index;
  static const uint8_t nodes[3] = {MENU_NODE_SETTINGS, MENU_NODE_CW, MENU_NODE_DISPLAY};
  for (node = 0U; node < 3U; ++node) {
    const menu_node_t MEM_CODE* list = &menu_nodes[nodes[node]];
    for (index = 0U; index < list->item_count; ++index) {
      const menu_item_t MEM_CODE* item = &list->items[index];
      if ((item->kind != MENU_KIND_VALUE) && (item->kind != MENU_KIND_CHOICE)) {
        continue;
      }
      start_empty();
      send(PREV);
      send(ENTER);
      if (nodes[node] == MENU_NODE_CW) {
        send(ENTER);
      } else if (nodes[node] == MENU_NODE_DISPLAY) {
        send_times(NEXT, 4U);
        send(ENTER);
      }
      send_times(NEXT, index);
      send(ENTER);
      TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
      TEST_ASSERT(probe_edit(index + 1, item->field_width));
      send_times(REPEAT(NEXT), 700U);
      TEST_ASSERT_EQUAL(settings_max(item->target), settings_get(item->target));
      TEST_ASSERT(probe_edit(index + 1, item->field_width));
      TEST_ASSERT(!pixel(105 - 5 * item->field_width, 8 * (index + 1) + 4));
      send_times(REPEAT(PREV), 700U);
      TEST_ASSERT_EQUAL(settings_min(item->target), settings_get(item->target));
      TEST_ASSERT(probe_edit(index + 1, item->field_width));
    }
  }
}

static void test_full_navigation(void) {
  start_empty();
  send_times(NEXT, 4U);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  send(PREV);
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  send_times(NEXT, 6U);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  send_times(REPEAT(NEXT), 9U);
  TEST_ASSERT_EQUAL(5U, menu_cursor());
  send(PREV);
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_DISPLAY, menu_node());
  TEST_ASSERT(title_is("Settings > Display"));
  send(PREV);
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  send(MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  TEST_ASSERT_EQUAL(4U, menu_cursor());
  send_times(REPEAT(PREV), 9U);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  send(ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_CW, menu_node());
  send(PREV);
  TEST_ASSERT_EQUAL(6U, menu_cursor());
  send(MENU);
  send(MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(3U, menu_cursor());
  menu_handle(MENU);
  TEST_ASSERT(!menu_needs_redraw());
  menu_handle(BUTTON_EVENT_NONE);
  TEST_ASSERT(!menu_needs_redraw());
}

static void test_failed_save_shows_not_saved(void) {
  start_empty();
  open_cw();
  send(ENTER);
  send(NEXT);
  send(MENU);
  send(MENU);
  fake_iap_set_fail_operation(fake_iap_operation_count() + 1U);
  send(MENU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_NOT_SAVED, menu_message());
  TEST_ASSERT(title_is("Settings"));
  TEST_ASSERT(centered_row_is(3, "Not saved"));
  TEST_ASSERT(probe_button(52, 71));
  send(ENTER);
  TEST_ASSERT(main_menu_is(3));
  TEST_ASSERT_EQUAL(610U, settings_get(SETTINGS_ID_TONE_HZ));
  /* The next exit from Settings tries again, also without a new edit. */
  fake_iap_clear_counters();
  send(ENTER);
  send(MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
  TEST_ASSERT(main_menu_is(3));
  (void)power_on(0U);
  TEST_ASSERT_EQUAL(610U, settings_get(SETTINGS_ID_TONE_HZ));
}

static void test_show_message(void) {
  start_empty();
  menu_show_message(3U);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  menu_show_message(0xFFU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  send(NEXT);
  menu_show_message(MENU_MESSAGE_STATS_RESET);
  menu_render();
  TEST_ASSERT(title_is("Reset Statistics"));
  send(MENU);
  TEST_ASSERT(main_menu_is(1));
  menu_show_message(MENU_MESSAGE_NOT_SAVED);
  menu_render();
  TEST_ASSERT(title_is("Settings"));
  send(ENTER);
  TEST_ASSERT(main_menu_is(1));
  menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS);
  menu_render();
  TEST_ASSERT(title_is("Display Defaults"));
  TEST_ASSERT(centered_row_is(2, "Default contrast and"));
  TEST_ASSERT(centered_row_is(3, "backlight are in use."));
  send(ENTER);
  TEST_ASSERT(main_menu_is(1));
}

/* ---------- Display recovery start (ui-ux.md V-33 .. V-38) ---------- */

static void prepare_bad_display_record(void) {
  fake_iap_reset();
  put_record(0U, 0U);
}

static void check_display_defaults_screen(void) {
  const row_text_t ok = {11, "OK", true};
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT(title_is("Display Defaults"));
  TEST_ASSERT(centered_row_is(2, "Default contrast and"));
  TEST_ASSERT(centered_row_is(3, "backlight are in use."));
  TEST_ASSERT(row_matches(5, 52, 71, &ok, 1U));
  TEST_ASSERT(probe_button(52, 71));
}

static void test_v33_v34_recovery_start(void) {
  size_t i;
  prepare_bad_display_record();
  TEST_ASSERT(power_on(BUTTON_BIT_MENU));
  TEST_ASSERT_EQUAL(1U, fake_lcd_contrast_count());
  TEST_ASSERT_EQUAL(32U, fake_lcd_contrast_at(0U));
  TEST_ASSERT_EQUAL(1U, fake_backlight_count());
  TEST_ASSERT_EQUAL(8U, fake_backlight_at(0U));
  for (i = 0U; i < fake_lcd_contrast_count(); ++i) {
    TEST_ASSERT(fake_lcd_contrast_at(i) != 0U);
  }
  check_display_defaults_screen();
  TEST_ASSERT_EQUAL(0U, eeprom_writes());

  /* V-34: the held button gives no event; a new press closes the message. */
  run_pins(BUTTON_BIT_MENU, 2000U);
  check_display_defaults_screen();
  run_pins(0U, 100U);
  run_pins(BUTTON_BIT_MENU, 30U);
  run_pins(0U, 30U);
  TEST_ASSERT(main_menu_is(0));
  open_display();
  TEST_ASSERT(list_row_is(1, "Backlight", "8", true));
  TEST_ASSERT(list_row_is(2, "Contrast", "32", false));
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
}

static void test_v35_short_hold_is_normal_start(void) {
  uint8_t sequence[40];
  prepare_bad_display_record();
  memset(sequence, BUTTON_BIT_MENU, 31U);
  memset(&sequence[31], 0, sizeof(sequence) - 31U);
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_tick_reset(0U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_button_pins_set_sequence(sequence, sizeof(sequence));
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  TEST_ASSERT(!buttons_is_recovery_start());
  /* The rest of the start-up as power_on() does it, without the recovery branch. */
  (void)settings_load();
  lcd_set_contrast((uint8_t)settings_get(SETTINGS_ID_CONTRAST));
  backlight_init((uint8_t)settings_get(SETTINGS_ID_BACKLIGHT));
  gfx_init();
  menu_init();
  menu_render();
  TEST_ASSERT_EQUAL(0U, fake_lcd_contrast_last());
  TEST_ASSERT_EQUAL(0U, fake_backlight_last());
  TEST_ASSERT(main_menu_is(0));
}

static void close_recovery_message(void) {
  run_pins(0U, 50U);
  run_pins(BUTTON_BIT_MENU, 30U);
  run_pins(0U, 30U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
}

static void test_v36_recovery_visit_without_edit(void) {
  prepare_bad_display_record();
  TEST_ASSERT(power_on(BUTTON_BIT_MENU));
  close_recovery_message();
  open_display();
  send(MENU);
  send(MENU);
  TEST_ASSERT(main_menu_is(3));
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
  TEST_ASSERT(!power_on(0U));
  TEST_ASSERT_EQUAL(0U, fake_lcd_contrast_last());
  TEST_ASSERT_EQUAL(0U, fake_backlight_last());
}

static void test_v37_recovery_then_tone_edit(void) {
  prepare_bad_display_record();
  TEST_ASSERT(power_on(BUTTON_BIT_MENU));
  close_recovery_message();
  open_cw();
  send(ENTER);
  send(NEXT);
  send(MENU);
  send(MENU);
  send(MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
  TEST_ASSERT(!power_on(0U));
  TEST_ASSERT_EQUAL(610U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT_EQUAL(32U, fake_lcd_contrast_last());
  TEST_ASSERT_EQUAL(8U, fake_backlight_last());
}

static void test_v38_recovery_contrast_back_and_forth(void) {
  prepare_bad_display_record();
  TEST_ASSERT(power_on(BUTTON_BIT_MENU));
  close_recovery_message();
  open_display();
  send(NEXT);
  send(ENTER);
  send(NEXT);
  send(PREV);
  send(ENTER);
  send(MENU);
  send(MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
  TEST_ASSERT(!power_on(0U));
  TEST_ASSERT_EQUAL(32U, settings_get(SETTINGS_ID_CONTRAST));
  TEST_ASSERT_EQUAL(8U, settings_get(SETTINGS_ID_BACKLIGHT));
  TEST_ASSERT_EQUAL(32U, fake_lcd_contrast_last());
}

static void test_v39_enter_held_is_normal_start(void) {
  prepare_bad_display_record();
  TEST_ASSERT(!power_on(BUTTON_BIT_ENTER));
  TEST_ASSERT_EQUAL(0U, fake_lcd_contrast_last());
  TEST_ASSERT_EQUAL(0U, fake_backlight_last());
  TEST_ASSERT(main_menu_is(0));
}

/* V-74: long holds in lists, the edit and the dialog act only through their press and repeat events. */
static void test_v74_long_holds_outside_mode_screens(void) {
  static uint8_t before[FB_SIZE];
  uint8_t page;

  start_empty();
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
    lcd_fb_clear_dirty(page);
  }
  run_pins(BUTTON_BIT_MENU, 2000U);
  run_pins(0U, 100U);
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    TEST_ASSERT_EQUAL(0, memcmp(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH));
  }

  start_empty();
  open_cw();
  run_pins(BUTTON_BIT_MENU, 2000U);
  run_pins(0U, 100U);
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  TEST_ASSERT(title_is("Settings"));
  TEST_ASSERT(probe_sel(1));
  TEST_ASSERT(submenu_row_is(1, "CW", true));

  start_empty();
  run_pins(BUTTON_BIT_NEXT, 1000U);
  run_pins(0U, 100U);
  TEST_ASSERT_EQUAL(3U, menu_cursor());
  TEST_ASSERT(probe_sel(4));

  start_empty();
  open_cw();
  send(ENTER);
  run_pins(0U, 50U);
  run_pins(BUTTON_BIT_NEXT, BUTTONS_DEBOUNCE_MS + 1000U);
  run_pins(0U, 100U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
  TEST_ASSERT_EQUAL(690U, settings_get(SETTINGS_ID_TONE_HZ));

  start_empty();
  open_dialog();
  run_pins(BUTTON_BIT_NEXT, BUTTONS_DEBOUNCE_MS + 1000U);
  run_pins(0U, 100U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_DIALOG, menu_screen());
  TEST_ASSERT_EQUAL(MENU_DIALOG_YES, menu_dialog_choice());
  TEST_ASSERT(dialog_is(true));
  TEST_ASSERT_EQUAL(0U, fake_stats_reset_count());
}

/* V-76 */
static void test_v76_paddle_row(void) {
  start_empty();
  open_cw();
  TEST_ASSERT(list_row_is(7, "Paddle", "Normal", false));
}

/* V-77 */
static void test_v77_paddle_edit_and_save(void) {
  start_empty();
  open_cw();
  send(PREV);
  send(ENTER);
  TEST_ASSERT(probe_edit(7, 8));
  TEST_ASSERT(edit_row_is(7, "Paddle", "Normal", 8, false, true));
  send(NEXT);
  TEST_ASSERT(edit_row_is(7, "Paddle", "Reversed", 8, true, false));
  menu_handle(NEXT);
  TEST_ASSERT(!menu_needs_redraw());
  send(ENTER);
  TEST_ASSERT(list_row_is(7, "Paddle", "Reversed", true));
  send(MENU);
  send(MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(1U, fake_iap_command_count(CMD_ERASE));
  (void)power_on(0U);
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  open_cw();
  TEST_ASSERT(list_row_is(7, "Paddle", "Reversed", false));
}

/* V-78: a record of the earlier firmware (layout version 1) has flags bit 1 = 0 and no prosign style. */
static void test_v78_earlier_record_reads_normal(void) {
  fake_iap_reset();
  put_record(32U, 8U);
  (void)power_on(0U);
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_PADDLE_SWAP));
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_BOTH, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  open_cw();
  TEST_ASSERT(list_row_is(7, "Paddle", "Normal", false));
  TEST_ASSERT(list_row_is(6, "Key Type", "Iambic B", false));
  send(MENU);
  TEST_ASSERT(list_row_is(3, "Prosigns", "Both", false));
}

int main(void) {
  TEST_RUN(test_v01_power_on_empty);
  TEST_RUN(test_v02_bad_crc_record);
  TEST_RUN(test_v03_wrap_on_press);
  TEST_RUN(test_v04_repeat_stops_at_end);
  TEST_RUN(test_v05_enter_and_back);
  TEST_RUN(test_v06_edit_tone);
  TEST_RUN(test_v07_tone_limits);
  TEST_RUN(test_v08_held_next_in_edit);
  TEST_RUN(test_v09_effective_at_base);
  TEST_RUN(test_v10_lower_base_lowers_effective);
  TEST_RUN(test_v11_key_type_choices);
  TEST_RUN(test_v12_show_cw_code);
  TEST_RUN(test_v13_backlight_edit);
  TEST_RUN(test_v14_contrast_edit);
  TEST_RUN(test_v15_save_on_settings_exit);
  TEST_RUN(test_v16_no_save_without_change);
  TEST_RUN(test_v17_menu_leaves_edit_only);
  TEST_RUN(test_v18_menu_at_root);
  TEST_RUN(test_v19_training_screens);
  TEST_RUN(test_v20_dialog_no);
  TEST_RUN(test_v21_dialog_yes);
  TEST_RUN(test_v22_v23_dialog_menu_is_no);
  TEST_RUN(test_v27_clamped_record);
  TEST_RUN(test_v28_power_off_inside_settings);
  TEST_RUN(test_v29_gap_line_under_title);
  TEST_RUN(test_v32_strings_fit);
  TEST_RUN(test_edit_every_item_to_limits);
  TEST_RUN(test_full_navigation);
  TEST_RUN(test_failed_save_shows_not_saved);
  TEST_RUN(test_show_message);
  TEST_RUN(test_v33_v34_recovery_start);
  TEST_RUN(test_v35_short_hold_is_normal_start);
  TEST_RUN(test_v36_recovery_visit_without_edit);
  TEST_RUN(test_v37_recovery_then_tone_edit);
  TEST_RUN(test_v38_recovery_contrast_back_and_forth);
  TEST_RUN(test_v39_enter_held_is_normal_start);
  TEST_RUN(test_v74_long_holds_outside_mode_screens);
  TEST_RUN(test_v76_paddle_row);
  TEST_RUN(test_v77_paddle_edit_and_save);
  TEST_RUN(test_v78_earlier_record_reads_normal);
  return TEST_FINISH();
}
