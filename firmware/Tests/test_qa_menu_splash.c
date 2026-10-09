/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests for the startup screen: the timeout over every 16-bit start tick, events at MENU_SPLASH_MS - 1 and
 * MENU_SPLASH_MS in both orders against menu_poll(), event fuzz with every event byte, stored and unknown message ids,
 * held buttons with real pins after the skip, and recovery starts with the release before and after the timeout.
 * Oracles: menu_subsystem_spec.md sections 17.2 (D-A5 to D-A10), 17.5 and 17.10; ui-ux.md sections 5.17, 6.1, 8.1
 * and 9.5. The expected times come from MENU_SPLASH_MS, so the file also runs with -DMENU_SPLASH_MS=<other value>.
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

#define T0 7000U
#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)
#define LONG(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))
#define FUZZ_RUNS 2000U

static const char version_text[] = "v" FIRMWARE_VERSION;
static uint16_t start_tick;
static uint16_t elapsed;
static size_t contrast_calls;
static size_t backlight_calls;
static uint32_t rng_state = 0x2545F491U;

static uint32_t qa_random(void) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

/* main.c start-up up to menu_init() at tick @p start, with @p held_mask pressed. Returns the recovery flag. */
static bool power_on_at(uint8_t held_mask, uint16_t start) {
  bool is_recovery;
  fake_iap_reset();
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  fake_keyer_reset();
  fake_player_reset();
  fake_sidetone_reset();
  fake_koch_item_reset();
  fake_tick_reset((uint16_t)(start - 200U));
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

static void set_time(uint16_t time) {
  elapsed = time;
  fake_tick_set_now((uint16_t)(start_tick + time));
}

/* The first main-loop pass at t = 0: the splash frame is drawn and flushed. */
static void first_frame(void) {
  set_time(0U);
  menu_poll();
  render_if_needed();
  clear_dirty();
}

/* One main-loop pass at t = elapsed with real buttons, in the order of main.c. */
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

static void run_to(uint16_t time) {
  while (elapsed < time) {
    ++elapsed;
    pass();
  }
}

static bool has_no_side_effect(void) {
  return (fake_iap_access_count() == 0U) && (fake_sidetone_call_count() == 0U) && (fake_keyer_start_count() == 0U) &&
         (fake_player_start_count() == 0U) && (fake_stats_reset_count() == 0U) &&
         (fake_lcd_contrast_count() == contrast_calls) && (fake_backlight_count() == backlight_calls);
}

static bool is_splash_frame(void) {
  frame_splash(version_text);
  return frame_matches("SPLASH");
}

static bool is_main_frame(int selected) {
  frame_main(selected);
  return frame_matches("MAIN");
}

static bool is_main_list_on(uint8_t cursor) {
  return (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_MAIN) && (menu_depth == 1U) &&
         (menu_cursor() == cursor);
}

/* D-A6 over the 16-bit wrap: for every start tick, MENU_SPLASH_MS - 1 keeps the splash and MENU_SPLASH_MS ends it. */
static void test_timeout_boundary_every_start_tick(void) {
  uint32_t start;
  unsigned failures = 0U;
  (void)power_on_at(0U, T0);
  for (start = 0U; start <= 0xFFFFU; ++start) {
    fake_tick_set_now((uint16_t)start);
    menu_init();
    fake_tick_set_now((uint16_t)(start + MENU_SPLASH_MS - 1U));
    menu_poll();
    if (menu_screen() != MENU_SCREEN_SPLASH) {
      ++failures;
    }
    fake_tick_set_now((uint16_t)(start + MENU_SPLASH_MS));
    menu_poll();
    if ((menu_screen() != MENU_SCREEN_LIST) || !menu_needs_redraw()) {
      ++failures;
    }
  }
  TEST_ASSERT_EQUAL(0U, failures);
}

/* The boundary sits on the tick wrap: t = MENU_SPLASH_MS - 1 is tick 65535 and t = MENU_SPLASH_MS is tick 0. */
static void test_timeout_on_wrap_with_real_passes(void) {
  static const uint16_t starts[4] = {(uint16_t)(0U - MENU_SPLASH_MS), (uint16_t)(0U - MENU_SPLASH_MS + 1U), 0xFFFFU,
                                     0U};
  unsigned index;
  for (index = 0U; index < 4U; ++index) {
    (void)power_on_at(0U, starts[index]);
    first_frame();
    TEST_ASSERT(is_splash_frame());
    run_to((uint16_t)(MENU_SPLASH_MS - 1U));
    TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
    TEST_ASSERT(!menu_needs_redraw());
    TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
    run_to(MENU_SPLASH_MS);
    TEST_ASSERT(is_main_list_on(0U));
    TEST_ASSERT(is_main_frame(0));
    clear_dirty();
    run_to((uint16_t)(MENU_SPLASH_MS + 3000U));
    TEST_ASSERT(is_main_list_on(0U));
    TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
    TEST_ASSERT(has_no_side_effect());
  }
}

/* A late first poll (a slow first flush) still ends the screen; a poll before the first render changes nothing. */
static void test_late_and_early_polls(void) {
  (void)power_on_at(0U, T0);
  set_time(0U);
  menu_poll();
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT(menu_needs_redraw());
  set_time((uint16_t)(MENU_SPLASH_MS + 30000U));
  menu_poll();
  TEST_ASSERT(is_main_list_on(0U));
  menu_render();
  TEST_ASSERT(is_main_frame(0));
  TEST_ASSERT(has_no_side_effect());
}

/* An event at t = MENU_SPLASH_MS - 1 ends the screen; the poll at MENU_SPLASH_MS then does nothing. */
static void test_event_at_last_splash_ms(void) {
  (void)power_on_at(0U, (uint16_t)(0U - MENU_SPLASH_MS));
  first_frame();
  set_time((uint16_t)(MENU_SPLASH_MS - 1U));
  menu_poll();
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  menu_handle(BUTTON_EVENT_NEXT);
  TEST_ASSERT(is_main_list_on(0U));
  menu_poll();
  render_if_needed();
  clear_dirty();
  set_time(MENU_SPLASH_MS);
  menu_poll();
  TEST_ASSERT(is_main_list_on(0U));
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(is_main_frame(0));
}

/*
 * t = MENU_SPLASH_MS in the main.c order: the event comes before the poll, so the event ends the screen and does
 * nothing else. If the poll of a pass at MENU_SPLASH_MS ended the screen first, the next event acts on the list.
 */
static void test_event_at_splash_ms_both_orders(void) {
  (void)power_on_at(0U, T0);
  first_frame();
  set_time(MENU_SPLASH_MS);
  menu_handle(BUTTON_EVENT_NEXT);
  menu_poll();
  TEST_ASSERT(is_main_list_on(0U));
  render_if_needed();
  TEST_ASSERT(is_main_frame(0));

  (void)power_on_at(0U, T0);
  first_frame();
  set_time(MENU_SPLASH_MS);
  menu_poll();
  TEST_ASSERT(is_main_list_on(0U));
  render_if_needed();
  menu_handle(BUTTON_EVENT_NEXT);
  TEST_ASSERT(is_main_list_on(1U));
  TEST_ASSERT(has_no_side_effect());
}

/* Several events in one pass (main.c takes up to 4): only the first is the skip, the others act on the list. */
static void test_event_burst_in_one_pass(void) {
  (void)power_on_at(0U, T0);
  first_frame();
  set_time(100U);
  menu_handle(BUTTON_EVENT_ENTER);
  menu_handle(BUTTON_EVENT_NEXT);
  menu_handle(REPEAT(BUTTON_EVENT_NEXT));
  menu_handle(LONG(BUTTON_EVENT_MENU));
  menu_poll();
  TEST_ASSERT(is_main_list_on(2U));
  render_if_needed();
  TEST_ASSERT(is_main_frame(2));
  TEST_ASSERT(has_no_side_effect());
}

/*
 * Event fuzz: every event byte. Code NONE (with any flag) keeps the screen and requests nothing. Any other code
 * ends the screen at once and does nothing else (D-A7): main menu on Receiving, no call to settings, sidetone,
 * contrast, backlight, stats or a mode.
 */
static void test_every_event_byte_on_splash(void) {
  unsigned value;
  for (value = 0U; value <= 0xFFU; ++value) {
    const button_event_t event = (button_event_t)value;
    (void)power_on_at(0U, T0);
    first_frame();
    set_time(50U);
    menu_handle(event);
    if (BUTTON_EVENT_CODE(event) == BUTTON_EVENT_NONE) {
      TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
      TEST_ASSERT(!menu_needs_redraw());
    } else {
      TEST_ASSERT(is_main_list_on(0U));
      TEST_ASSERT(menu_needs_redraw());
      menu_poll();
      render_if_needed();
      TEST_ASSERT(is_main_frame(0));
    }
    TEST_ASSERT(has_no_side_effect());
  }
}

/* Random codes and flags at random times inside the splash: the first event always ends it. */
static void test_random_first_event(void) {
  unsigned run;
  unsigned failures = 0U;
  static const button_event_t codes[4] = {BUTTON_EVENT_NEXT, BUTTON_EVENT_PREV, BUTTON_EVENT_MENU, BUTTON_EVENT_ENTER};
  static const uint8_t flags[4] = {0U, BUTTON_EVENT_REPEAT_FLAG, BUTTON_EVENT_LONG_FLAG,
                                   BUTTON_EVENT_REPEAT_FLAG | BUTTON_EVENT_LONG_FLAG};
  for (run = 0U; run < FUZZ_RUNS; ++run) {
    const uint16_t time = (uint16_t)(qa_random() % MENU_SPLASH_MS);
    const button_event_t event = (button_event_t)(codes[qa_random() % 4U] | flags[qa_random() % 4U]);
    const bool has_message = (qa_random() % 2U) == 0U;
    (void)power_on_at(0U, (uint16_t)qa_random());
    if (has_message) {
      menu_show_message((uint8_t)(qa_random() % MENU_MESSAGE_COUNT));
    }
    first_frame();
    set_time(time);
    menu_poll();
    if (menu_screen() != MENU_SCREEN_SPLASH) {
      ++failures;
    }
    menu_handle(event);
    menu_poll();
    render_if_needed();
    if (has_message) {
      failures += (menu_screen() == MENU_SCREEN_MESSAGE) ? 0U : 1U;
    } else {
      failures += is_main_list_on(0U) ? 0U : 1U;
    }
    failures += has_no_side_effect() ? 0U : 1U;
    set_time((uint16_t)(MENU_SPLASH_MS + 100U));
    menu_poll();
    failures += menu_needs_redraw() ? 1U : 0U;
  }
  TEST_ASSERT_EQUAL(0U, failures);
}

/* D-A9: a message on the splash is stored, draws nothing, and shows at the end; the last valid id wins. */
static void test_message_requested_during_splash(void) {
  uint8_t id;
  for (id = 0U; id < MENU_MESSAGE_COUNT; ++id) {
    (void)power_on_at(0U, T0);
    first_frame();
    set_time(700U);
    menu_show_message(id);
    TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
    TEST_ASSERT(!menu_needs_redraw());
    TEST_ASSERT_EQUAL(id, menu_message());
    menu_poll();
    render_if_needed();
    TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
    TEST_ASSERT(is_splash_frame());
    run_to(MENU_SPLASH_MS);
    TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
    TEST_ASSERT_EQUAL(id, menu_message());
    TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
    /* Closing the message gives the main menu: the stack under the splash is the main menu only. */
    menu_handle(BUTTON_EVENT_ENTER);
    TEST_ASSERT(is_main_list_on(0U));
    render_if_needed();
    TEST_ASSERT(is_main_frame(0));
    TEST_ASSERT(has_no_side_effect());
  }
}

/* Unknown ids (MENU_MESSAGE_COUNT .. 0xFF, also the sentinel 0xFF) never store, never clear a stored id. */
static void test_unknown_message_id_during_splash(void) {
  unsigned id;
  (void)power_on_at(0U, T0);
  first_frame();
  for (id = MENU_MESSAGE_COUNT; id <= 0xFFU; ++id) {
    menu_show_message((uint8_t)id);
  }
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_NONE, menu_message());
  run_to(MENU_SPLASH_MS);
  TEST_ASSERT(is_main_list_on(0U));

  (void)power_on_at(0U, T0);
  first_frame();
  menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS);
  for (id = MENU_MESSAGE_COUNT; id <= 0xFFU; ++id) {
    menu_show_message((uint8_t)id);
  }
  TEST_ASSERT_EQUAL(MENU_MESSAGE_DISPLAY_DEFAULTS, menu_message());
  menu_handle(LONG(BUTTON_EVENT_PREV));
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  render_if_needed();
  frame_display_defaults();
  TEST_ASSERT(frame_matches("Display Defaults"));
  TEST_ASSERT(has_no_side_effect());
}

/* A second menu_init() starts a new splash with no stored message, also after a message and the About screen. */
static void test_reinit_clears_stored_message(void) {
  (void)power_on_at(0U, T0);
  menu_show_message(MENU_MESSAGE_NOT_SAVED);
  menu_handle(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  menu_handle(BUTTON_EVENT_ENTER);
  menu_handle(BUTTON_EVENT_PREV);
  menu_handle(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
  set_time(400U);
  menu_init();
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_NONE, menu_message());
  TEST_ASSERT(is_main_list_on(0U) == false);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  set_time((uint16_t)(400U + MENU_SPLASH_MS - 1U));
  menu_poll();
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  set_time((uint16_t)(400U + MENU_SPLASH_MS));
  menu_poll();
  TEST_ASSERT(is_main_list_on(0U));
}

/* A held enter skips with its press; enter has no repeat and no long event outside tap mode, so nothing opens. */
static void test_held_enter_after_skip(void) {
  (void)power_on_at(0U, T0);
  first_frame();
  run_to(300U);
  fake_button_pins_set(BUTTON_BIT_ENTER);
  run_to(330U);
  TEST_ASSERT(is_main_list_on(0U));
  render_if_needed();
  clear_dirty();
  run_to((uint16_t)(MENU_SPLASH_MS + 500U));
  TEST_ASSERT(is_main_list_on(0U));
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  fake_button_pins_set(0U);
  run_to((uint16_t)(MENU_SPLASH_MS + 600U));
  TEST_ASSERT(is_main_list_on(0U));
  TEST_ASSERT(is_main_frame(0));
  TEST_ASSERT(has_no_side_effect());
}

/* A held menu/back skips with its press; the long event at 800 ms does nothing in the main menu. */
static void test_held_menu_after_skip(void) {
  (void)power_on_at(0U, T0);
  first_frame();
  run_to(300U);
  fake_button_pins_set(BUTTON_BIT_MENU);
  run_to(330U);
  TEST_ASSERT(is_main_list_on(0U));
  render_if_needed();
  clear_dirty();
  run_to((uint16_t)(MENU_SPLASH_MS + 500U));
  fake_button_pins_set(0U);
  run_to((uint16_t)(MENU_SPLASH_MS + 600U));
  TEST_ASSERT(is_main_list_on(0U));
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  TEST_ASSERT(has_no_side_effect());
}

/* A held next that crosses MENU_SPLASH_MS: its press skips, its repeats move the cursor, the timeout adds nothing. */
static void test_held_next_across_timeout(void) {
  const uint16_t press_time = (uint16_t)(MENU_SPLASH_MS - 100U);
  (void)power_on_at(0U, T0);
  first_frame();
  run_to((uint16_t)(press_time - BUTTONS_DEBOUNCE_MS - 1U));
  fake_button_pins_set(BUTTON_BIT_NEXT);
  run_to(press_time);
  TEST_ASSERT(is_main_list_on(0U));
  run_to((uint16_t)(press_time + BUTTONS_REPEAT_DELAY_MS - 1U));
  TEST_ASSERT(is_main_list_on(0U));
  run_to((uint16_t)(press_time + BUTTONS_REPEAT_DELAY_MS));
  TEST_ASSERT(is_main_list_on(1U));
  fake_button_pins_set(0U);
  run_to((uint16_t)(press_time + 1500U));
  TEST_ASSERT(is_main_list_on(1U));
  TEST_ASSERT(is_main_frame(1));
}

/* Recovery start with real pins: menu/back held past the timeout. The message shows at MENU_SPLASH_MS and stays. */
static void test_recovery_release_after_timeout(void) {
  TEST_ASSERT(power_on_at(BUTTON_BIT_MENU, T0));
  first_frame();
  TEST_ASSERT(is_splash_frame());
  run_to((uint16_t)(MENU_SPLASH_MS - 1U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  run_to(MENU_SPLASH_MS);
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_DISPLAY_DEFAULTS, menu_message());
  frame_display_defaults();
  TEST_ASSERT(frame_matches("Display Defaults"));
  clear_dirty();
  /* The held button gives no press, repeat or long event (ui-ux.md 6.2); the release gives none either. */
  run_to((uint16_t)(MENU_SPLASH_MS + 1500U));
  fake_button_pins_set(0U);
  run_to((uint16_t)(MENU_SPLASH_MS + 1600U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  fake_button_pins_set(BUTTON_BIT_MENU);
  run_to((uint16_t)(MENU_SPLASH_MS + 1650U));
  fake_button_pins_set(0U);
  run_to((uint16_t)(MENU_SPLASH_MS + 1700U));
  TEST_ASSERT(is_main_list_on(0U));
  TEST_ASSERT(is_main_frame(0));
  TEST_ASSERT_EQUAL(32U, fake_lcd_contrast_last());
  TEST_ASSERT_EQUAL(8U, fake_backlight_last());
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/* Recovery start with real pins: release before the timeout, then a held prev skips into the message only. */
static void test_recovery_release_before_timeout_then_hold(void) {
  TEST_ASSERT(power_on_at(BUTTON_BIT_MENU, T0));
  first_frame();
  run_to(200U);
  fake_button_pins_set(0U);
  run_to(600U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  fake_button_pins_set(BUTTON_BIT_PREV);
  run_to(1800U);
  /* The press skipped; repeats and the long prev do nothing on the message (ui-ux.md 6.1). */
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  fake_button_pins_set(0U);
  run_to((uint16_t)(MENU_SPLASH_MS + 2000U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  frame_display_defaults();
  TEST_ASSERT(frame_matches("Display Defaults"));
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/*
 * D-A10: the splash render leaves the 5 x 8 font selected; a render of the same screen gives the same frame. The
 * render clears the frame first, so pages with ink get dirty again (gfx marks a page on each pixel change).
 */
static void test_splash_render_restores_font(void) {
  static uint8_t before[FB_SIZE];
  uint8_t page;
  (void)power_on_at(0U, T0);
  first_frame();
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
  menu_render();
  menu_render();
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    TEST_ASSERT_EQUAL(0, memcmp(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH));
  }
  TEST_ASSERT_EQUAL(GFX_FONT_5X8_WIDTH, gfx_text_width("A"));
}

/* V-169: the splash ink stays inside its areas, with any version length up to 17 characters. */
static void test_splash_ink_areas(void) {
  int x;
  int y;
  unsigned outside = 0U;
  const int version_x = 122 - 5 * (int)strlen(version_text);
  (void)power_on_at(0U, T0);
  first_frame();
  TEST_ASSERT(is_splash_frame());
  TEST_ASSERT(version_x >= FRAME_CELL_X(6));
  for (y = 0; y < FRAME_HEIGHT; ++y) {
    for (x = 0; x < FRAME_WIDTH; ++x) {
      const bool is_brand = (y >= 8) && (y < 16) && (x >= 82) && (x <= 121);
      const bool is_product = (y >= 16) && (y < 32) && (x >= 7) && (x <= 86);
      const bool is_mod = (y >= 32) && (y < 40) && (x >= 7) && (x <= 71);
      const bool is_version = (y >= 56) && (x >= version_x) && (x <= 121);
      if (lcd_fb_get_pixel((int16_t)x, (int16_t)y) && !(is_brand || is_product || is_mod || is_version)) {
        ++outside;
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, outside);
  for (y = 0; y < FRAME_HEIGHT; ++y) {
    TEST_ASSERT(!lcd_fb_get_pixel(0, (int16_t)y));
    TEST_ASSERT(!lcd_fb_get_pixel(1, (int16_t)y));
    TEST_ASSERT(!lcd_fb_get_pixel(127, (int16_t)y));
  }
  /* The last version glyph has ink: the text is not cut on the right. */
  {
    bool has_ink = false;
    for (y = 56; y < 64; ++y) {
      for (x = 117; x <= 121; ++x) {
        has_ink = has_ink || lcd_fb_get_pixel((int16_t)x, (int16_t)y);
      }
    }
    TEST_ASSERT(has_ink);
  }
}

int main(void) {
  TEST_RUN(test_timeout_boundary_every_start_tick);
  TEST_RUN(test_timeout_on_wrap_with_real_passes);
  TEST_RUN(test_late_and_early_polls);
  TEST_RUN(test_event_at_last_splash_ms);
  TEST_RUN(test_event_at_splash_ms_both_orders);
  TEST_RUN(test_event_burst_in_one_pass);
  TEST_RUN(test_every_event_byte_on_splash);
  TEST_RUN(test_random_first_event);
  TEST_RUN(test_message_requested_during_splash);
  TEST_RUN(test_unknown_message_id_during_splash);
  TEST_RUN(test_reinit_clears_stored_message);
  TEST_RUN(test_held_enter_after_skip);
  TEST_RUN(test_held_menu_after_skip);
  TEST_RUN(test_held_next_across_timeout);
  TEST_RUN(test_recovery_release_after_timeout);
  TEST_RUN(test_recovery_release_before_timeout_then_hold);
  TEST_RUN(test_splash_render_restores_font);
  TEST_RUN(test_splash_ink_areas);
  return TEST_FINISH();
}
