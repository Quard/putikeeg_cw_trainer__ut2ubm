/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests for the About item and screen: every event byte on About, random event fuzz with real buttons, entry from
 * both cursor directions, the full frame with the version from version.h, and the column limits of the version rows.
 * Oracles: menu_subsystem_spec.md sections 17.2 (D-A4, D-A11, D-A12, D-A14, D-A15), 17.5 and 17.10; ui-ux.md
 * sections 5.17.4, 5.18, 6.1 and 9.5. The version tests use FIRMWARE_VERSION, so they also pass with a 17-character
 * version in firmware/VERSION.
 */

#include <stdio.h>
#include <string.h>

#include "backlight/backlight.h"
#include "board/button_pins.h"
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
#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)
#define VERSION_MAX_LENGTH 17U
#define GRID_COLUMNS 25U
#define FUZZ_STEPS 3000U

static const char version_row[] = "Version " FIRMWARE_VERSION;
static uint16_t now;
static size_t contrast_calls;
static size_t backlight_calls;
static uint8_t frame_before[FB_SIZE];
static uint32_t rng_state = 0x9E3779B9U;

static uint32_t qa_random(void) {
  rng_state ^= rng_state << 13;
  rng_state ^= rng_state >> 17;
  rng_state ^= rng_state << 5;
  return rng_state;
}

/* Power-on with an empty EEPROM; the startup screen ends with a menu/back press. */
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
  now = 3000U;
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

static void render_if_needed(void) {
  if (menu_needs_redraw()) {
    menu_render();
  }
}

static void send(button_event_t event) {
  menu_handle(event);
  render_if_needed();
}

static void clear_dirty(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
}

static void save_frame(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&frame_before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
}

static bool is_frame_unchanged(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    if (memcmp(&frame_before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH) != 0) {
      return false;
    }
  }
  return true;
}

/* One main-loop pass with real buttons, in the order of main.c. */
static void pass(void) {
  unsigned events;
  button_event_t event;
  fake_tick_set_now(now);
  for (events = 0U; events < 4U; ++events) {
    event = buttons_poll();
    if (event == BUTTON_EVENT_NONE) {
      break;
    }
    menu_handle(event);
  }
  menu_poll();
  render_if_needed();
  now = (uint16_t)(now + 1U);
}

static void run_pins(uint8_t mask, uint16_t duration) {
  uint16_t i;
  fake_button_pins_set(mask);
  for (i = 0U; i < duration; ++i) {
    pass();
  }
}

static bool has_no_side_effect(void) {
  return (fake_iap_access_count() == 0U) && (fake_sidetone_call_count() == 0U) && (fake_keyer_start_count() == 0U) &&
         (fake_player_start_count() == 0U) && (fake_stats_reset_count() == 0U) &&
         (fake_lcd_contrast_count() == contrast_calls) && (fake_backlight_count() == backlight_calls);
}

static bool is_about_frame(void) {
  frame_about(version_row);
  return frame_matches("ABOUT");
}

static bool is_main_frame(int selected) {
  frame_main(selected);
  return frame_matches("MAIN");
}

static bool is_main_on_about(void) {
  return (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_MAIN) && (menu_depth == 1U) &&
         (menu_cursor() == ABOUT_INDEX);
}

static void open_about_with_prev(void) {
  send(BUTTON_EVENT_PREV);
  send(BUTTON_EVENT_ENTER);
}

/*
 * Every event byte on About (D-A12): enter or menu/back without the long flag closes it to the main menu on About.
 * Every other byte (code NONE, prev, next, codes the buttons never give, any long event) changes nothing: no
 * redraw request, no dirty page, the same frame, no side effect.
 */
static void test_every_event_byte_on_about(void) {
  unsigned value;
  for (value = 0U; value <= 0xFFU; ++value) {
    const button_event_t event = (button_event_t)value;
    const uint8_t code = BUTTON_EVENT_CODE(event);
    const bool is_long = (event & BUTTON_EVENT_LONG_FLAG) != 0U;
    const bool is_closing = !is_long && ((code == BUTTON_EVENT_ENTER) || (code == BUTTON_EVENT_MENU));
    start();
    open_about_with_prev();
    TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
    save_frame();
    clear_dirty();
    menu_handle(event);
    if (is_closing) {
      TEST_ASSERT(is_main_on_about());
      TEST_ASSERT(menu_needs_redraw());
      menu_render();
      TEST_ASSERT(is_main_frame(4));
    } else {
      TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
      TEST_ASSERT(!menu_needs_redraw());
      menu_poll();
      TEST_ASSERT(!menu_needs_redraw());
      TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
      TEST_ASSERT(is_frame_unchanged());
      TEST_ASSERT_EQUAL(ABOUT_INDEX, menu_cursor());
    }
    TEST_ASSERT(has_no_side_effect());
  }
}

/* Opening About from the other direction: next from Settings; a held next stops on About and enter opens it. */
static void test_open_about_from_both_directions(void) {
  start();
  send(BUTTON_EVENT_NEXT);
  send(BUTTON_EVENT_NEXT);
  send(BUTTON_EVENT_NEXT);
  send(BUTTON_EVENT_NEXT);
  TEST_ASSERT_EQUAL(ABOUT_INDEX, menu_cursor());
  send(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
  TEST_ASSERT(is_about_frame());

  start();
  run_pins(BUTTON_BIT_NEXT, 2000U);
  run_pins(0U, 50U);
  TEST_ASSERT(is_main_on_about());
  TEST_ASSERT(is_main_frame(4));
  run_pins(BUTTON_BIT_ENTER, 40U);
  run_pins(0U, 40U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
  TEST_ASSERT(is_about_frame());
  TEST_ASSERT(has_no_side_effect());
}

/*
 * Random real-button fuzz on About: random masks and hold times. While About is open the frame never changes and no
 * page gets dirty, unless the mask holds enter or menu/back (D-A12).
 */
static void test_random_buttons_on_about(void) {
  unsigned step;
  unsigned bad_frames = 0U;
  unsigned bad_closes = 0U;
  unsigned closes = 0U;
  start();
  open_about_with_prev();
  for (step = 0U; step < FUZZ_STEPS; ++step) {
    const uint8_t mask = (uint8_t)(qa_random() & 0x0FU);
    const uint16_t hold = (uint16_t)(1U + (qa_random() % 1200U));
    clear_dirty();
    save_frame();
    run_pins(mask, hold);
    run_pins(0U, 30U);
    if ((mask & (BUTTON_BIT_ENTER | BUTTON_BIT_MENU)) == 0U) {
      /* prev and next presses, repeats and long events: About stays, nothing is drawn. */
      if (menu_screen() != MENU_SCREEN_ABOUT) {
        ++bad_closes;
      } else if (!is_frame_unchanged() || (lcd_fb_dirty_mask() != 0U)) {
        ++bad_frames;
      }
    } else {
      /* The enter or menu/back press closed About; later events of the hold may act on the list. */
      closes += (menu_screen() != MENU_SCREEN_ABOUT) ? 1U : 0U;
      start();
      open_about_with_prev();
    }
  }
  printf("about fuzz: %u steps, %u closes\n", FUZZ_STEPS, closes);
  TEST_ASSERT(closes > 0U);
  TEST_ASSERT_EQUAL(0U, bad_frames);
  TEST_ASSERT_EQUAL(0U, bad_closes);
}

/* The About frame after the splash with a stored message: the message comes first, then About opens as usual. */
static void test_about_after_recovery_message(void) {
  start();
  menu_init();
  menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS);
  send(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  send(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  open_about_with_prev();
  TEST_ASSERT(is_about_frame());
  send(BUTTON_EVENT_MENU);
  TEST_ASSERT(is_main_on_about());
}

/* A render of About over a full-on framebuffer paints every pixel (17.5: the full clear stays). */
static void test_about_repaints_garbage(void) {
  start();
  open_about_with_prev();
  gfx_fill_rect(0, 0, (int16_t)LCD_WIDTH, (int16_t)LCD_HEIGHT, LCD_PIXEL_ON);
  menu_render();
  TEST_ASSERT(is_about_frame());
  gfx_fill_rect(0, 0, (int16_t)LCD_WIDTH, (int16_t)LCD_HEIGHT, LCD_PIXEL_ON);
  menu_init();
  menu_render();
  frame_splash("v" FIRMWARE_VERSION);
  TEST_ASSERT(frame_matches("SPLASH"));
}

/* Ink of 5 x 8 text row @p r that is past column @p last_column, or at x 0, 1 or 127. */
static unsigned ink_outside(int r, int last_column) {
  unsigned count = 0U;
  int x;
  int y;
  for (y = 8 * r; y < 8 * r + 8; ++y) {
    for (x = 0; x < FRAME_WIDTH; ++x) {
      const bool is_outside = (x < 2) || (x == 127) || (x >= FRAME_CELL_X(last_column + 1));
      if (is_outside && lcd_fb_get_pixel((int16_t)x, (int16_t)y)) {
        ++count;
      }
    }
  }
  return count;
}

/* V-167, V-169, UX-63: the version rows are not cut and stay inside the grid, for any version up to 17 characters. */
static void test_version_rows_not_cut(void) {
  const size_t version_length = strlen(FIRMWARE_VERSION);
  start();
  TEST_ASSERT(version_length >= 5U);
  TEST_ASSERT(version_length <= VERSION_MAX_LENGTH);
  TEST_ASSERT(strlen(version_row) <= GRID_COLUMNS);
  TEST_ASSERT_EQUAL(0, strcmp(menu_text_version_long, version_row));
  TEST_ASSERT_EQUAL(strlen(menu_text_version_short), (size_t)menu_text_version_short_length);
  open_about_with_prev();
  TEST_ASSERT(is_about_frame());
  TEST_ASSERT_EQUAL(0U, ink_outside(3, (int)strlen(version_row) - 1));
  TEST_ASSERT_EQUAL(0U, ink_outside(5, 24));
  TEST_ASSERT_EQUAL(0U, ink_outside(6, 20));
  TEST_ASSERT_EQUAL(0U, ink_outside(7, 5));
  /* The last character of the version row has ink in its own cell. */
  {
    const int last_x = FRAME_CELL_X((int)strlen(version_row) - 1);
    bool has_ink = false;
    int x;
    int y;
    for (y = 24; y < 32; ++y) {
      for (x = last_x; x < last_x + 5; ++x) {
        has_ink = has_ink || lcd_fb_get_pixel((int16_t)x, (int16_t)y);
      }
    }
    TEST_ASSERT(has_ink);
  }
  printf("version %s: About row 3 ends in column %u\n", FIRMWARE_VERSION, (unsigned)(strlen(version_row) - 1U));
}

int main(void) {
  TEST_RUN(test_every_event_byte_on_about);
  TEST_RUN(test_open_about_from_both_directions);
  TEST_RUN(test_random_buttons_on_about);
  TEST_RUN(test_about_after_recovery_message);
  TEST_RUN(test_about_repaints_garbage);
  TEST_RUN(test_version_rows_not_cut);
  return TEST_FINISH();
}
