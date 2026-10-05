/*
 * QA tests for the menu: random event sequences with state invariants and a check that every visible change sets
 * the redraw flag, full repaint over a garbage framebuffer, and save failures (CMD_FAIL and low supply) during
 * the random walk. The walk includes long events and random keyer events in Free Input.
 * Oracles: menu_subsystem_spec.md sections 4.10, 6.3 and 6.4; keyer_spec.md section 6.14; ui-ux.md 6.1 and 6.6.
 */
#include <string.h>

#include "backlight/backlight.h"
#include "buttons/buttons.h"
#include "fake_iap.h"
#include "fake_keyer.h"
#include "fake_menu_outputs.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "menu/menu.h"
#include "menu_internal.h"
#include "settings/settings.h"
#include "support/test_assert.h"

#define CMD_ERASE 3U
#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))
#define LONG(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))
#define EVENT_KINDS 9U

static const button_event_t all_events[EVENT_KINDS] = {
    BUTTON_EVENT_NEXT,        BUTTON_EVENT_PREV,         BUTTON_EVENT_MENU,         BUTTON_EVENT_ENTER,
    REPEAT(BUTTON_EVENT_NEXT), REPEAT(BUTTON_EVENT_PREV), LONG(BUTTON_EVENT_MENU),   LONG(BUTTON_EVENT_PREV),
    LONG(BUTTON_EVENT_NEXT),
};

static uint32_t rng_state;
static uint32_t rng_next(void) {
  rng_state = rng_state * 1664525U + 1013904223U;
  return rng_state >> 8;
}

static void snapshot(uint8_t* copy) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&copy[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
}

static void fill_garbage(void) {
  uint8_t page;
  size_t i;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    for (i = 0U; i < LCD_WIDTH; ++i) {
      lcd_fb_page(page)[i] = (uint8_t)rng_next();
    }
  }
}

static void start_empty(void) {
  fake_keyer_reset();
  fake_iap_reset();
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  lcd_fb_clear();
  (void)settings_load();
  fake_iap_clear_counters();
  gfx_init();
  menu_init();
  menu_render();
}

static const menu_item_t MEM_CODE* item_under_cursor(void) { return &menu_nodes[menu_node()].items[menu_cursor()]; }

/* Rendering twice, or over random pixels, gives the same image: the render paints every pixel. */
static void check_full_repaint(void) {
  static uint8_t clean[FB_SIZE];
  static uint8_t again[FB_SIZE];
  lcd_fb_clear();
  menu_render();
  snapshot(clean);
  menu_render();
  snapshot(again);
  TEST_ASSERT_EQUAL(0, memcmp(clean, again, FB_SIZE));
  fill_garbage();
  menu_render();
  snapshot(again);
  TEST_ASSERT_EQUAL(0, memcmp(clean, again, FB_SIZE));
}

/*
 * Random walk over the real events. After each event:
 * - the stack, cursor, screen and choice stay in range and match the item kind (spec 6.3);
 * - when the redraw flag stays clear, a forced render does not change the image (no missed redraw);
 * - stats_reset() runs only for ENTER on Yes; the EEPROM is erased only for MENU from the Settings list;
 *   contrast and backlight outputs change only in the edit of their own item;
 * - a failed save (CMD_FAIL or low supply, injected at random) shows Not saved over the main menu on Settings.
 */
/* Random keyer events in Free Input, then menu_poll(): page 0 never changes and no redraw is requested. */
static void feed_free_input(void) {
  static uint8_t title[LCD_WIDTH];
  unsigned count = rng_next() % 12U;
  unsigned page;
  unsigned x;
  memcpy(title, lcd_fb_page(0U), LCD_WIDTH);
  while (count-- != 0U) {
    fake_keyer_push_event((keyer_event_t)(1U + (rng_next() % 5U)));
  }
  menu_poll();
  menu_poll();
  TEST_ASSERT_EQUAL(0U, fake_keyer_pending_count());
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(0, memcmp(title, lcd_fb_page(0U), LCD_WIDTH));
  /* Margin columns x 0, 1 and 127 of the text area stay off. */
  for (page = 1U; page < LCD_PAGE_COUNT; ++page) {
    for (x = 0U; x < LCD_WIDTH; x = (x == 1U) ? 127U : x + 1U) {
      TEST_ASSERT_EQUAL(0U, lcd_fb_page((uint8_t)page)[x]);
    }
  }
}

static void test_random_walk_invariants(void) {
  static uint8_t before[FB_SIZE];
  static uint8_t after[FB_SIZE];
  unsigned walk;
  size_t faults = 0U;
  size_t saves = 0U;
  size_t stats_resets = 0U;
  size_t live_outputs = 0U;
  size_t free_input_entries = 0U;
  size_t free_input_exits = 0U;
  size_t free_input_longs = 0U;
  size_t ignored_longs = 0U;
  rng_state = 4242U;
  for (walk = 0U; walk < 300U; ++walk) {
    unsigned step;
    start_empty();
    for (step = 0U; step < 400U; ++step) {
      const button_event_t event = all_events[rng_next() % EVENT_KINDS];
      const uint8_t code = BUTTON_EVENT_CODE(event);
      const bool is_long = (event & BUTTON_EVENT_LONG_FLAG) != 0U;
      const uint8_t screen = menu_screen();
      const uint8_t node = menu_node();
      const uint8_t cursor = menu_cursor();
      const uint8_t depth = menu_depth;
      const uint8_t choice = menu_dialog_choice();
      const size_t starts_before = fake_keyer_start_count();
      const size_t stops_before = fake_keyer_stop_count();
      const bool is_mode_screen = (screen == MENU_SCREEN_PLACEHOLDER) || (screen == MENU_SCREEN_FREE_INPUT);
      const menu_item_t MEM_CODE* item = item_under_cursor();
      const size_t stats_before = fake_stats_reset_count();
      const size_t erases_before = fake_iap_erase_count(0U);
      const size_t contrast_before = fake_lcd_contrast_count();
      const size_t backlight_before = fake_backlight_count();
      const bool is_settings_exit =
          (screen == MENU_SCREEN_LIST) && (node == MENU_NODE_SETTINGS) && (code == BUTTON_EVENT_MENU) && !is_long;
      bool is_fault = false;

      if (is_settings_exit && settings_is_edited() && settings_has_unsaved_changes() && ((rng_next() % 3U) == 0U)) {
        is_fault = true;
        ++faults;
        fake_iap_clear_counters();
        if ((rng_next() & 1U) != 0U) {
          fake_iap_set_fail_operation(1U + (rng_next() % 31U));
        } else {
          /* Operation 31, the last read-back, has no program after it, so a flag from there on stops nothing. */
          fake_iap_set_supply_low_from(1U + (rng_next() % 30U));
        }
      }
      snapshot(before);
      menu_handle(event);
      if (menu_needs_redraw()) {
        menu_render();
        TEST_ASSERT(!menu_needs_redraw());
      } else if (screen == MENU_SCREEN_FREE_INPUT) {
        /* Free Input draws text, clear and delimiter straight into the framebuffer: only the title is fixed. */
        snapshot(after);
        TEST_ASSERT_EQUAL(0, memcmp(before, after, LCD_WIDTH));
      } else {
        menu_render();
        snapshot(after);
        if (!TEST_ASSERT_EQUAL(0, memcmp(before, after, FB_SIZE))) {
          printf("missed redraw: walk %u step %u screen %u event 0x%02X\n", walk, step, screen, event);
        }
      }

      /* UX-24 and ui-ux.md 6.1: a long event outside a mode screen, and a short event in one, change nothing. */
      if ((is_long && !is_mode_screen) || (!is_long && is_mode_screen) ||
          (is_long && (screen == MENU_SCREEN_PLACEHOLDER) && (code != BUTTON_EVENT_MENU))) {
        ++ignored_longs;
        TEST_ASSERT_EQUAL(screen, menu_screen());
        TEST_ASSERT_EQUAL(node, menu_node());
        TEST_ASSERT_EQUAL(cursor, menu_cursor());
        TEST_ASSERT_EQUAL(depth, menu_depth);
        TEST_ASSERT_EQUAL(choice, menu_dialog_choice());
        TEST_ASSERT_EQUAL(0, memcmp(before, after, FB_SIZE));
      }
      if (is_long && (screen == MENU_SCREEN_FREE_INPUT)) {
        ++free_input_longs;
        if (code == BUTTON_EVENT_MENU) {
          ++free_input_exits;
          TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
          TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
          TEST_ASSERT_EQUAL(MENU_MODE_FREE_INPUT, item_under_cursor()->target);
          TEST_ASSERT_EQUAL(stops_before + 1U, fake_keyer_stop_count());
        } else {
          TEST_ASSERT_EQUAL(MENU_SCREEN_FREE_INPUT, menu_screen());
          TEST_ASSERT_EQUAL(stops_before, fake_keyer_stop_count());
        }
      }
      if ((screen != MENU_SCREEN_FREE_INPUT) && (menu_screen() == MENU_SCREEN_FREE_INPUT)) {
        ++free_input_entries;
        TEST_ASSERT_EQUAL(starts_before + 1U, fake_keyer_start_count());
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_KEY_TYPE), fake_keyer_last_key_type());
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_PADDLE_SWAP) != 0U, fake_keyer_last_is_swapped());
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_TX_WPM), fake_keyer_last_wpm());
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_TX_EFF_WPM), fake_keyer_last_eff_wpm());
      } else {
        TEST_ASSERT_EQUAL(starts_before, fake_keyer_start_count());
      }
      /* The keyer runs exactly while Free Input is open. */
      TEST_ASSERT_EQUAL(menu_screen() == MENU_SCREEN_FREE_INPUT, fake_keyer_is_running());
      if ((menu_screen() == MENU_SCREEN_FREE_INPUT) && ((rng_next() % 2U) == 0U)) {
        feed_free_input();
      }

      TEST_ASSERT((menu_depth >= 1U) && (menu_depth <= MENU_STACK_DEPTH));
      TEST_ASSERT(menu_node() < MENU_NODE_COUNT);
      TEST_ASSERT(menu_cursor() < menu_nodes[menu_node()].item_count);
      TEST_ASSERT(menu_screen() <= MENU_SCREEN_FREE_INPUT);
      TEST_ASSERT(menu_dialog_choice() <= MENU_DIALOG_YES);
      if (menu_screen() == MENU_SCREEN_EDIT) {
        TEST_ASSERT((item_under_cursor()->kind == MENU_KIND_VALUE) || (item_under_cursor()->kind == MENU_KIND_CHOICE));
      } else if (menu_screen() == MENU_SCREEN_PLACEHOLDER) {
        TEST_ASSERT(item_under_cursor()->kind == MENU_KIND_MODE);
        TEST_ASSERT(item_under_cursor()->target != MENU_MODE_FREE_INPUT);
      } else if (menu_screen() == MENU_SCREEN_FREE_INPUT) {
        TEST_ASSERT(item_under_cursor()->kind == MENU_KIND_MODE);
        TEST_ASSERT_EQUAL(MENU_MODE_FREE_INPUT, item_under_cursor()->target);
      } else if (menu_screen() == MENU_SCREEN_DIALOG) {
        TEST_ASSERT(item_under_cursor()->kind == MENU_KIND_ACTION);
      }

      if (fake_iap_erase_count(0U) != erases_before) {
        ++saves;
      }
      if ((fake_lcd_contrast_count() != contrast_before) || (fake_backlight_count() != backlight_before)) {
        ++live_outputs;
      }
      if (fake_stats_reset_count() != stats_before) {
        ++stats_resets;
        TEST_ASSERT_EQUAL(stats_before + 1U, fake_stats_reset_count());
        TEST_ASSERT((screen == MENU_SCREEN_DIALOG) && (code == BUTTON_EVENT_ENTER) && (choice == MENU_DIALOG_YES));
        TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
        TEST_ASSERT_EQUAL(MENU_MESSAGE_STATS_RESET, menu_message());
      }
      if (!is_fault && (fake_iap_erase_count(0U) != erases_before)) {
        TEST_ASSERT(is_settings_exit);
      }
      if (fake_lcd_contrast_count() != contrast_before) {
        TEST_ASSERT((screen == MENU_SCREEN_EDIT) && (item->target == SETTINGS_ID_CONTRAST) &&
                    (item->kind == MENU_KIND_VALUE) && (node == MENU_NODE_DISPLAY));
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_CONTRAST), fake_lcd_contrast_last());
      }
      if (fake_backlight_count() != backlight_before) {
        TEST_ASSERT((screen == MENU_SCREEN_EDIT) && (item->target == SETTINGS_ID_BACKLIGHT) &&
                    (node == MENU_NODE_DISPLAY));
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_BACKLIGHT), fake_backlight_last());
      }
      if (is_settings_exit) {
        TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
        TEST_ASSERT_EQUAL(3U, menu_cursor());
        if (is_fault) {
          TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
          TEST_ASSERT_EQUAL(MENU_MESSAGE_NOT_SAVED, menu_message());
          TEST_ASSERT(settings_is_edited());
          fake_iap_clear_counters();
          fake_iap_set_supply_low(false);
        } else {
          TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
          TEST_ASSERT(!settings_is_edited());
          TEST_ASSERT(!settings_has_unsaved_changes());
        }
      }
      if ((step % 97U) == 0U) {
        check_full_repaint();
      }
    }
  }
  /* The walk must reach the interesting paths, or the checks above prove little. */
  printf("faults %u, erases %u, stats resets %u, live outputs %u\n", (unsigned)faults, (unsigned)saves,
         (unsigned)stats_resets, (unsigned)live_outputs);
  printf("free input entries %u, exits %u, long events %u, ignored events %u\n", (unsigned)free_input_entries,
         (unsigned)free_input_exits, (unsigned)free_input_longs, (unsigned)ignored_longs);
  TEST_ASSERT(faults >= 20U);
  TEST_ASSERT(saves >= 20U);
  TEST_ASSERT(stats_resets >= 20U);
  TEST_ASSERT(live_outputs >= 20U);
  TEST_ASSERT(free_input_entries >= 20U);
  TEST_ASSERT(free_input_exits >= 20U);
  TEST_ASSERT(fake_keyer_is_overflowed() == false);
}

/* The settings that were live at the last successful save come back after a power-on. */
static void test_random_walk_persists_last_save(void) {
  unsigned walk;
  rng_state = 99U;
  for (walk = 0U; walk < 100U; ++walk) {
    uint16_t saved[SETTINGS_ID_COUNT];
    settings_id_t id;
    unsigned step;
    start_empty();
    for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
      saved[id] = settings_get(id);
    }
    for (step = 0U; step < 600U; ++step) {
      const bool is_settings_exit = (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_SETTINGS);
      const button_event_t event = all_events[rng_next() % EVENT_KINDS];
      menu_handle(event);
      if (menu_needs_redraw()) {
        menu_render();
      }
      if (is_settings_exit && (event == BUTTON_EVENT_MENU)) {
        for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
          saved[id] = settings_get(id);
        }
      }
    }
    /* Power-on: the menu state is lost, the record stays. */
    (void)settings_load();
    for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
      TEST_ASSERT_EQUAL(saved[id], settings_get(id));
    }
  }
}

int main(void) {
  TEST_RUN(test_random_walk_invariants);
  TEST_RUN(test_random_walk_persists_last_save);
  return TEST_FINISH();
}
