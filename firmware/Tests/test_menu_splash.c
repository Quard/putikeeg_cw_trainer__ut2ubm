/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Startup screen tests (references/internal/menu_subsystem_spec.md sections 17.5 and 17.8, ui-ux.md section 5.17,
 * scenarios V-152 to V-161, V-166, V-167 and V-169). Time t is in ms after menu_init().
 */

#include <string.h>

#include "backlight/backlight.h"
#include "board/button_pins.h"
#include "board/buttons_config.h"
#include "buttons/buttons.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_keyer.h"
#include "fake_koch_item.h"
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
#include "support/test_assert.h"
#include "version.h"

#define T0 5000U
#define LONG(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))

static const char version_text[] = "v" FIRMWARE_VERSION;
static uint16_t start_tick;
static uint16_t elapsed;
static size_t contrast_calls;
static size_t backlight_calls;

/* Power-on as main.c runs it, with @p held_mask pressed, up to menu_init() at tick @p start. Draws nothing. */
static bool power_on_at(uint8_t held_mask, uint16_t start) {
  bool is_recovery;
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  fake_keyer_reset();
  fake_player_reset();
  fake_sidetone_reset();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_button_pins_set(held_mask);
  fake_delay_reset();
  lcd_fb_clear();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  (void)settings_load();
  (void)progress_load();
  is_recovery = buttons_is_recovery_start();
  if (is_recovery) {
    settings_use_display_defaults();
  }
  lcd_set_contrast((uint8_t)settings_get(SETTINGS_ID_CONTRAST));
  backlight_init((uint8_t)settings_get(SETTINGS_ID_BACKLIGHT));
  gfx_init();
  fake_iap_clear_counters();
  contrast_calls = fake_lcd_contrast_count();
  backlight_calls = fake_backlight_count();
  start_tick = start;
  elapsed = 0U;
  fake_tick_set_now(start);
  menu_init();
  if (is_recovery) {
    menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS);
  }
  return is_recovery;
}

static bool power_on(uint8_t held_mask) {
  fake_iap_reset();
  return power_on_at(held_mask, T0);
}

static void clear_dirty(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
}

static void render_if_needed(void) {
  if (menu_needs_redraw()) {
    menu_render();
  }
}

/* The first main-loop pass at t = 0 and the first flush. */
static void first_frame(void) {
  fake_tick_set_now(start_tick);
  menu_poll();
  render_if_needed();
  clear_dirty();
}

/* One main-loop pass at t = elapsed, as main.c runs it, with the pins as they are. */
static void pass(void) {
  unsigned events;
  button_event_t event;
  fake_tick_set_now((uint16_t)(start_tick + elapsed));
  for (events = 0U; events < 4U; ++events) {
    event = buttons_poll();
    if (event == BUTTON_EVENT_NONE) {
      break;
    }
    menu_handle(event);
  }
  menu_poll();
  render_if_needed();
}

/* Passes, one per ms, up to and including t = @p time. */
static void run_to(uint16_t time) {
  while (elapsed < time) {
    ++elapsed;
    pass();
  }
}

static void set_pins(uint8_t mask) { fake_button_pins_set(mask); }

/* One event at t = @p time, then the poll and the render of the same pass. */
static void event_at(uint16_t time, button_event_t event) {
  run_to((uint16_t)(time - 1U));
  elapsed = time;
  fake_tick_set_now((uint16_t)(start_tick + elapsed));
  menu_handle(event);
  menu_poll();
  render_if_needed();
}

static bool is_splash_frame(void) {
  frame_splash(version_text);
  return frame_matches("SPLASH");
}

static bool is_main_frame(int selected) {
  frame_main(selected);
  return frame_matches("MAIN");
}

/* Pages where the splash frame and MAIN(0) differ: the switch must mark each of them dirty. */
static uint8_t splash_to_main_pages(void) {
  static bool splash[FRAME_HEIGHT][FRAME_WIDTH];
  uint8_t mask = 0U;
  int x;
  int y;
  frame_splash(version_text);
  memcpy(splash, frame_expected, sizeof(splash));
  frame_main(0);
  for (y = 0; y < FRAME_HEIGHT; ++y) {
    for (x = 0; x < FRAME_WIDTH; ++x) {
      if (splash[y][x] != frame_expected[y][x]) {
        mask = (uint8_t)(mask | (1U << (y / 8)));
      }
    }
  }
  return mask;
}

static bool has_no_side_effect(void) {
  return (fake_iap_access_count() == 0U) && (fake_sidetone_call_count() == 0U) && (fake_keyer_start_count() == 0U) &&
         (fake_player_start_count() == 0U) && (fake_stats_reset_count() == 0U) &&
         (fake_lcd_contrast_count() == contrast_calls) && (fake_backlight_count() == backlight_calls);
}

/* 17.8 item 1, V-152, V-153. */
static void test_v152_v153_timeout(void) {
  (void)power_on(0U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT(menu_needs_redraw());
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  first_frame();
  TEST_ASSERT(is_splash_frame());
  run_to(MENU_SPLASH_MS - 1U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(is_splash_frame());
  run_to(MENU_SPLASH_MS);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  /* The render repaints all 8 pages; gfx marks a page dirty only when its pixels change. */
  TEST_ASSERT_EQUAL(splash_to_main_pages(), lcd_fb_dirty_mask());
  TEST_ASSERT(is_main_frame(0));
  clear_dirty();
  run_to((uint16_t)(MENU_SPLASH_MS + 3000U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(is_main_frame(0));
  TEST_ASSERT(has_no_side_effect());
}

/* 17.8 item 2: the elapsed time wraps with the 16-bit tick. */
static void test_timeout_over_tick_wrap(void) {
  fake_iap_reset();
  (void)power_on_at(0U, (uint16_t)(65535U - 500U));
  first_frame();
  run_to(MENU_SPLASH_MS - 1U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT(!menu_needs_redraw());
  run_to(MENU_SPLASH_MS);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(is_main_frame(0));
}

/* menu_init() reads the tick once; the splash check reads no tick on other screens. */
static void test_tick_reads(void) {
  size_t reads;
  fake_iap_reset();
  fake_tick_reset(T0);
  menu_init();
  TEST_ASSERT_EQUAL(1U, fake_tick_read_count());
  menu_handle(BUTTON_EVENT_MENU);
  reads = fake_tick_read_count();
  menu_poll();
  TEST_ASSERT_EQUAL(reads, fake_tick_read_count());
}

/* 17.8 item 3, V-154, V-155: each event only ends the screen. */
static void check_skip_event(button_event_t event) {
  (void)power_on(0U);
  first_frame();
  event_at(500U, event);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT(is_main_frame(0));
  clear_dirty();
  run_to(MENU_SPLASH_MS + 10U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(has_no_side_effect());
}

static void test_v154_v155_any_event_skips(void) {
  static const button_event_t codes[4] = {BUTTON_EVENT_NEXT, BUTTON_EVENT_PREV, BUTTON_EVENT_MENU, BUTTON_EVENT_ENTER};
  unsigned index;
  for (index = 0U; index < 4U; ++index) {
    check_skip_event(codes[index]);
    check_skip_event(REPEAT(codes[index]));
    check_skip_event(LONG(codes[index]));
  }
}

/* V-156: a tip closure of 50 ms gives one enter press; it skips the screen and opens nothing. */
static void test_v156_paddle_tip(void) {
  (void)power_on(0U);
  first_frame();
  run_to(500U);
  set_pins(BUTTON_BIT_ENTER);
  run_to(550U);
  set_pins(0U);
  run_to(700U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT(is_main_frame(0));
  TEST_ASSERT(has_no_side_effect());
}

/* Holds @p mask so that its press event comes at t = 500; returns the t of the pass that ended the screen. */
static uint16_t hold_after_skip(uint8_t mask, uint16_t* repeat_check_time) {
  uint16_t skip_time = 0U;
  (void)power_on(0U);
  first_frame();
  run_to((uint16_t)(500U - BUTTONS_DEBOUNCE_MS - 1U));
  set_pins(mask);
  while ((elapsed < 1500U) && (menu_screen() == MENU_SCREEN_SPLASH)) {
    ++elapsed;
    pass();
  }
  skip_time = elapsed;
  *repeat_check_time = (uint16_t)(skip_time + BUTTONS_REPEAT_DELAY_MS);
  return skip_time;
}

/* 17.8 item 4, V-157: the repeats of a held next act on the main menu after the skip; the long event does nothing. */
static void test_v157_held_next(void) {
  uint16_t repeat_time;
  const uint16_t skip_time = hold_after_skip(BUTTON_BIT_NEXT, &repeat_time);
  TEST_ASSERT_EQUAL(500U, skip_time);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT(is_main_frame(0));
  run_to((uint16_t)(repeat_time - 1U));
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  run_to(repeat_time);
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  run_to((uint16_t)(skip_time + BUTTONS_LONG_PRESS_MS - 1U));
  TEST_ASSERT_EQUAL(4U, menu_cursor());
  TEST_ASSERT(is_main_frame(4));
  clear_dirty();
  run_to(1500U);
  set_pins(0U);
  run_to(1600U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(4U, menu_cursor());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(has_no_side_effect());
}

/* 17.8 item 4, V-158: a held prev skips; its repeats stay on Receiving (no wrap on a repeat). */
static void test_v158_held_prev(void) {
  uint16_t repeat_time;
  (void)hold_after_skip(BUTTON_BIT_PREV, &repeat_time);
  TEST_ASSERT(is_main_frame(0));
  clear_dirty();
  run_to(1500U);
  set_pins(0U);
  run_to(1600U);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(is_main_frame(0));
}

/* V-159: enter held from power-on gives no event and does not skip; a new press opens Receiving. */
static void test_v159_enter_held_at_power_on(void) {
  TEST_ASSERT(!power_on(BUTTON_BIT_ENTER));
  first_frame();
  run_to(MENU_SPLASH_MS - 1U);
  TEST_ASSERT(is_splash_frame());
  run_to(MENU_SPLASH_MS);
  TEST_ASSERT(is_main_frame(0));
  run_to((uint16_t)(MENU_SPLASH_MS + 1000U));
  set_pins(0U);
  run_to((uint16_t)(MENU_SPLASH_MS + 1100U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(is_main_frame(0));
  fake_koch_item_reset();
  fake_koch_item_push("K");
  set_pins(BUTTON_BIT_ENTER);
  run_to((uint16_t)(MENU_SPLASH_MS + 1150U));
  set_pins(0U);
  run_to((uint16_t)(MENU_SPLASH_MS + 1200U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
}

/* 17.8 item 5, V-160: recovery start; the first press shows the message, the second closes it. */
static void check_recovery_press(uint8_t first_mask) {
  TEST_ASSERT(power_on(BUTTON_BIT_MENU));
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_DISPLAY_DEFAULTS, menu_message());
  first_frame();
  TEST_ASSERT(is_splash_frame());
  run_to(300U);
  set_pins(0U);
  run_to(980U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT(is_splash_frame());
  set_pins(first_mask);
  run_to(1050U);
  set_pins(0U);
  run_to(1200U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_DISPLAY_DEFAULTS, menu_message());
  frame_display_defaults();
  TEST_ASSERT(frame_matches("Display Defaults"));
  run_to((uint16_t)(MENU_SPLASH_MS + 1000U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  set_pins(BUTTON_BIT_ENTER);
  run_to((uint16_t)(MENU_SPLASH_MS + 1050U));
  set_pins(0U);
  run_to((uint16_t)(MENU_SPLASH_MS + 1100U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(is_main_frame(0));
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

static void test_v160_recovery_then_press(void) {
  check_recovery_press(BUTTON_BIT_ENTER);
  check_recovery_press(BUTTON_BIT_NEXT);
  check_recovery_press(BUTTON_BIT_MENU);
}

/* 17.8 item 5, V-161: recovery start with no event; the message shows at the timeout and stays. */
static void test_v161_recovery_timeout(void) {
  TEST_ASSERT(power_on(BUTTON_BIT_MENU));
  first_frame();
  run_to(300U);
  set_pins(0U);
  run_to(MENU_SPLASH_MS - 1U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT(is_splash_frame());
  run_to(MENU_SPLASH_MS);
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  frame_display_defaults();
  TEST_ASSERT(frame_matches("Display Defaults"));
  run_to((uint16_t)(MENU_SPLASH_MS + 8000U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT_EQUAL(32U, fake_lcd_contrast_last());
  TEST_ASSERT_EQUAL(8U, fake_backlight_last());
}

/* 17.8 item 6: on the splash, an unknown id changes nothing and a second id replaces the first. */
static void test_message_during_splash(void) {
  (void)power_on(0U);
  first_frame();
  menu_show_message(MENU_MESSAGE_COUNT);
  menu_show_message(0xFEU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_NONE, menu_message());
  event_at(100U, BUTTON_EVENT_NEXT);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());

  (void)power_on(0U);
  first_frame();
  menu_show_message(MENU_MESSAGE_NOT_SAVED);
  menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT(!menu_needs_redraw());
  event_at(100U, BUTTON_EVENT_PREV);
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_DISPLAY_DEFAULTS, menu_message());
  frame_display_defaults();
  TEST_ASSERT(frame_matches("Display Defaults"));
}

/* 17.8 item 7, V-166, V-167, V-169: the frame, the version position and the font after the splash. */
static void test_v166_v169_render(void) {
  const int version_length = (int)strlen(version_text);
  (void)power_on(0U);
  first_frame();
  TEST_ASSERT(is_splash_frame());
  TEST_ASSERT_EQUAL(1U + strlen(FIRMWARE_VERSION), (size_t)version_length);
  TEST_ASSERT(version_length <= 18);
  TEST_ASSERT(122 - 5 * version_length >= FRAME_CELL_X(6));
  /* A second render paints the same frame, so the 8 x 16 font was switched back. */
  menu_render();
  TEST_ASSERT(is_splash_frame());
  event_at(100U, BUTTON_EVENT_MENU);
  TEST_ASSERT(is_main_frame(0));
}

static bool is_ascii(const char MEM_CODE* text) {
  for (; *text != '\0'; ++text) {
    if ((*text < 0x20) || (*text > 0x7E)) {
      return false;
    }
  }
  return true;
}

/* V-169: only ASCII 0x20-0x7E. */
static void test_v169_ascii_texts(void) {
  TEST_ASSERT(is_ascii(menu_text_brand));
  TEST_ASSERT(is_ascii(menu_text_product));
  TEST_ASSERT(is_ascii(menu_text_mod));
  TEST_ASSERT(is_ascii(menu_text_version_short));
  TEST_ASSERT_EQUAL(0, strcmp(menu_text_version_short, version_text));
  TEST_ASSERT_EQUAL(strlen(version_text), menu_text_version_short_length);
}

int main(void) {
  TEST_RUN(test_v152_v153_timeout);
  TEST_RUN(test_timeout_over_tick_wrap);
  TEST_RUN(test_tick_reads);
  TEST_RUN(test_v154_v155_any_event_skips);
  TEST_RUN(test_v156_paddle_tip);
  TEST_RUN(test_v157_held_next);
  TEST_RUN(test_v158_held_prev);
  TEST_RUN(test_v159_enter_held_at_power_on);
  TEST_RUN(test_v160_recovery_then_press);
  TEST_RUN(test_v161_recovery_timeout);
  TEST_RUN(test_message_during_splash);
  TEST_RUN(test_v166_v169_render);
  TEST_RUN(test_v169_ascii_texts);
  return TEST_FINISH();
}
