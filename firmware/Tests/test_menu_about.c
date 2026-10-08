/*
 * About item and screen tests (references/internal/menu_subsystem_spec.md sections 17.5 and 17.8, ui-ux.md section
 * 5.18, scenarios V-162 to V-166).
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "backlight/backlight.h"
#include "board/button_pins.h"
#include "board/buttons_config.h"
#include "buttons/buttons.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_keyer.h"
#include "fake_menu_outputs.h"
#include "fake_player.h"
#include "fake_sidetone.h"
#include "fake_tick.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "menu/menu.h"
#include "menu_internal.h"
#include "progress/progress.h"
#include "settings/settings.h"
#include "support/menu_frame.h"
#include "support/menu_splash.h"
#include "support/test_assert.h"
#include "version.h"

#define ABOUT_INDEX 4U
#define LONG(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))
#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)

static const char version_row[] = "Version " FIRMWARE_VERSION;
static uint16_t now;
static size_t contrast_calls;
static size_t backlight_calls;

/* Power-on with an empty EEPROM, then the startup screen ends; the main menu shows on Receiving. */
static void start(void) {
  fake_iap_reset();
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  fake_keyer_reset();
  fake_player_reset();
  fake_sidetone_reset();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_delay_reset();
  lcd_fb_clear();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  now = 2000U;
  fake_tick_set_now(now);
  (void)settings_load();
  (void)progress_load();
  lcd_set_contrast((uint8_t)settings_get(SETTINGS_ID_CONTRAST));
  backlight_init((uint8_t)settings_get(SETTINGS_ID_BACKLIGHT));
  gfx_init();
  menu_init();
  menu_test_skip_splash();
  fake_iap_clear_counters();
  contrast_calls = fake_lcd_contrast_count();
  backlight_calls = fake_backlight_count();
}

static void send(button_event_t event) {
  menu_handle(event);
  if (menu_needs_redraw()) {
    menu_render();
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
    menu_poll();
    if (menu_needs_redraw()) {
      menu_render();
    }
    now = (uint16_t)(now + 1U);
  }
}

static void clear_dirty(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
}

static void open_about(void) {
  send(BUTTON_EVENT_PREV);
  TEST_ASSERT_EQUAL(ABOUT_INDEX, menu_cursor());
  send(BUTTON_EVENT_ENTER);
}

static bool is_about_frame(void) {
  frame_about(version_row);
  return frame_matches("ABOUT");
}

static bool is_main_frame(int selected) {
  frame_main(selected);
  return frame_matches("MAIN");
}

static bool has_no_side_effect(void) {
  return (fake_iap_access_count() == 0U) && (fake_sidetone_call_count() == 0U) && (fake_keyer_start_count() == 0U) &&
         (fake_player_start_count() == 0U) && (fake_stats_reset_count() == 0U) &&
         (fake_lcd_contrast_count() == contrast_calls) && (fake_backlight_count() == backlight_calls);
}

/* 17.8 item 1: About is the fifth main menu item. */
static void test_about_item(void) {
  const menu_node_t MEM_CODE* main_node = &menu_nodes[MENU_NODE_MAIN];
  const menu_item_t MEM_CODE* item = &main_node->items[ABOUT_INDEX];
  TEST_ASSERT_EQUAL(5U, main_node->item_count);
  TEST_ASSERT(main_node->item_count <= MENU_VISIBLE_ROWS);
  TEST_ASSERT_EQUAL(0, strcmp(item->label, "About"));
  TEST_ASSERT_EQUAL(MENU_KIND_ABOUT, item->kind);
  TEST_ASSERT_EQUAL(0U, item->target);
  TEST_ASSERT(item->unit == NULL);
  TEST_ASSERT(item->texts == NULL);
  TEST_ASSERT_EQUAL(0, strcmp(main_node->title, "CW Trainer"));
}

/* V-162: prev wraps from Receiving to About; the About row has no marker. */
static void test_v162_about_row(void) {
  start();
  send(BUTTON_EVENT_PREV);
  TEST_ASSERT_EQUAL(ABOUT_INDEX, menu_cursor());
  TEST_ASSERT(is_main_frame(4));
  send(BUTTON_EVENT_NEXT);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT(is_main_frame(0));
}

/* 17.8 items 2 and 5, V-163: enter opens About and nothing else. */
static void test_v163_enter_opens_about(void) {
  start();
  send(BUTTON_EVENT_PREV);
  menu_handle(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
  TEST_ASSERT(menu_needs_redraw());
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(ABOUT_INDEX, menu_cursor());
  menu_render();
  TEST_ASSERT(is_about_frame());
  TEST_ASSERT(has_no_side_effect());
}

/* 17.8 item 3, V-164: enter, menu/back and a tip closure return to the main menu on About. */
static void test_v164_close_about(void) {
  start();
  open_about();
  send(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(ABOUT_INDEX, menu_cursor());
  TEST_ASSERT(is_main_frame(4));

  send(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
  send(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT(is_main_frame(4));

  send(BUTTON_EVENT_ENTER);
  TEST_ASSERT(is_about_frame());
  run_pins(0U, 50U);
  run_pins(BUTTON_BIT_ENTER, 50U);
  run_pins(0U, 50U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(is_main_frame(4));
  TEST_ASSERT(has_no_side_effect());
}

/* After @p event: About stays open, no redraw request, no dirty page, the same frame. */
static void check_ignored(button_event_t event) {
  static uint8_t before[FB_SIZE];
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
  clear_dirty();
  menu_handle(event);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    TEST_ASSERT_EQUAL(0, memcmp(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH));
  }
}

/* 17.8 item 3, V-165: prev, next, repeats and long events do nothing on About. */
static void test_v165_ignored_events(void) {
  start();
  open_about();
  check_ignored(BUTTON_EVENT_PREV);
  check_ignored(BUTTON_EVENT_NEXT);
  check_ignored(REPEAT(BUTTON_EVENT_PREV));
  check_ignored(REPEAT(BUTTON_EVENT_NEXT));
  check_ignored(LONG(BUTTON_EVENT_MENU));
  check_ignored(LONG(BUTTON_EVENT_PREV));
  check_ignored(LONG(BUTTON_EVENT_NEXT));
  check_ignored(LONG(BUTTON_EVENT_ENTER));
  check_ignored(BUTTON_EVENT_NONE);
  clear_dirty();
  run_pins(BUTTON_BIT_NEXT, 1000U);
  run_pins(0U, 100U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(is_about_frame());
  run_pins(BUTTON_BIT_MENU, 1000U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  run_pins(0U, 100U);
  TEST_ASSERT_EQUAL(ABOUT_INDEX, menu_cursor());
  TEST_ASSERT(has_no_side_effect());
}

/* 17.8 item 4: a message over About shows; enter returns to the main menu on About. */
static void test_message_over_about(void) {
  start();
  open_about();
  menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS);
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT(menu_needs_redraw());
  menu_render();
  frame_display_defaults();
  TEST_ASSERT(frame_matches("Display Defaults"));
  send(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(ABOUT_INDEX, menu_cursor());
  TEST_ASSERT(is_main_frame(4));
}

/* V-166, V-169: no text reaches past column 24; the version row ends in column 24 at most. */
static void test_v169_rows_fit(void) {
  TEST_ASSERT(strlen(version_row) <= 25U);
  TEST_ASSERT_EQUAL(3U, MENU_ABOUT_LINK_ROWS);
  TEST_ASSERT_EQUAL(0, strcmp(menu_about_link_rows[0], "https://github.com/Quard/"));
  TEST_ASSERT_EQUAL(0, strcmp(menu_about_link_rows[1], "putikeeg_cw_trainer__"));
  TEST_ASSERT_EQUAL(0, strcmp(menu_about_link_rows[2], "ut2ubm"));
  TEST_ASSERT_EQUAL(0, strcmp(menu_text_version_long, version_row));
}

static bool is_version_format(const char* text) {
  int dots = 0;
  bool has_digit = false;
  for (; *text != '\0'; ++text) {
    if (isdigit((unsigned char)*text)) {
      has_digit = true;
    } else if ((*text == '.') && has_digit && (dots < 2)) {
      ++dots;
      has_digit = false;
    } else {
      return false;
    }
  }
  return (dots == 2) && has_digit;
}

/* 17.8 item 6, D-A14: the header follows the first line of firmware/VERSION, read at run time. */
static void test_version_matches_file(void) {
  char line[64] = {0};
  FILE* file = fopen(FIRMWARE_VERSION_FILE, "r");
  TEST_ASSERT(file != NULL);
  if (file != NULL) {
    TEST_ASSERT(fgets(line, (int)sizeof(line), file) != NULL);
    (void)fclose(file);
  }
  line[strcspn(line, "\r\n")] = '\0';
  TEST_ASSERT(is_version_format(FIRMWARE_VERSION));
  TEST_ASSERT(strlen(FIRMWARE_VERSION) <= 17U);
  TEST_ASSERT_EQUAL(0, strcmp(line, FIRMWARE_VERSION));
  TEST_ASSERT(!is_version_format("0.1"));
  TEST_ASSERT(!is_version_format("1..2"));
  TEST_ASSERT(!is_version_format("1.2.3a"));
}

int main(void) {
  TEST_RUN(test_about_item);
  TEST_RUN(test_v162_about_row);
  TEST_RUN(test_v163_enter_opens_about);
  TEST_RUN(test_v164_close_about);
  TEST_RUN(test_v165_ignored_events);
  TEST_RUN(test_message_over_about);
  TEST_RUN(test_v169_rows_fit);
  TEST_RUN(test_version_matches_file);
  return TEST_FINISH();
}
