/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests for the menu: random event sequences with state invariants and a check that every visible change sets
 * the redraw flag, full repaint over a garbage framebuffer, and save failures (CMD_FAIL and low supply) during
 * the random walk. The walk includes long events, random keyer events in Free Input, and the Callsigns screen of
 * Free Training. Oracles: menu_subsystem_spec.md sections 4.10, 6.3 and 6.4; keyer_spec.md section 6.14;
 * ui-ux.md 6.1, 6.6 and 5.21; training_spec.md section 23 (Q-C7).
 */
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
#include "fake_tick.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "menu/menu.h"
#include "menu_internal.h"
#include "modes/training.h"
#include "settings/settings.h"
#include "support/menu_splash.h"
#include "support/test_assert.h"

#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define MAIN_SETTINGS_ROW 4U /* training_spec.md D-T64 */
#define PROBE_PRESS_MS 40U   /* longer than BUTTONS_DEBOUNCE_MS, shorter than the repeat delay */
#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))
#define LONG(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))
#define EVENT_KINDS 9U

static const button_event_t all_events[EVENT_KINDS] = {
    BUTTON_EVENT_NEXT,       BUTTON_EVENT_PREV,         BUTTON_EVENT_MENU,
    BUTTON_EVENT_ENTER,      REPEAT(BUTTON_EVENT_NEXT), REPEAT(BUTTON_EVENT_PREV),
    LONG(BUTTON_EVENT_MENU), LONG(BUTTON_EVENT_PREV),   LONG(BUTTON_EVENT_NEXT),
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

/* The button probe runs on its own fake time; the walk itself sends events with menu_handle(). */
static uint16_t probe_now;

/* The first event of buttons_poll() while @p mask is held for @p duration ms, or BUTTON_EVENT_NONE. */
static button_event_t probe_hold(uint8_t mask, uint16_t duration) {
  button_event_t first = BUTTON_EVENT_NONE;
  uint16_t elapsed;
  fake_button_pins_set(mask);
  for (elapsed = 0U; elapsed < duration; ++elapsed) {
    button_event_t event;
    probe_now = (uint16_t)(probe_now + 1U);
    fake_tick_set_now(probe_now);
    event = buttons_poll();
    if (first == BUTTON_EVENT_NONE) {
      first = event;
    }
  }
  return first;
}

/*
 * Q-C7: tap mode is off and enter is enabled. With tap mode off, a next press gives its event while the button is
 * held (tap mode gives it only at the release). With enter enabled, a P3.2 closure gives ENTER.
 */
static bool is_tap_off_and_enter_enabled(void) {
  const button_event_t next_event = probe_hold(BUTTON_BIT_NEXT, PROBE_PRESS_MS);
  button_event_t enter_event;
  (void)probe_hold(0U, PROBE_PRESS_MS);
  enter_event = probe_hold(BUTTON_BIT_ENTER, PROBE_PRESS_MS);
  (void)probe_hold(0U, PROBE_PRESS_MS);
  if ((next_event != BUTTON_EVENT_NEXT) || (enter_event != BUTTON_EVENT_ENTER)) {
    printf("buttons after a leave: next 0x%02X, enter 0x%02X\n", next_event, enter_event);
    return false;
  }
  return true;
}

static void start_empty(void) {
  fake_keyer_reset();
  fake_player_reset();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_delay_reset();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  probe_now = 2000U;
  fake_iap_reset();
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  lcd_fb_clear();
  (void)settings_load();
  fake_iap_clear_counters();
  gfx_init();
  menu_init();
  menu_test_skip_splash();
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
  size_t free_events = 0U;
  size_t free_session_events = 0U;
  size_t free_leaves = 0U;
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
      const bool is_training_screen = (screen == MENU_SCREEN_RECEIVING) || (screen == MENU_SCREEN_SENDING);
      const bool is_free_screen = screen == MENU_SCREEN_FREE_TRAINING;
      const bool is_mode_screen = is_training_screen || is_free_screen || (screen == MENU_SCREEN_FREE_INPUT);
      /*
       * The start screen and the end screen act on short events (ui-ux.md 5.15, 5.16.4, D-T35, D-T45). So do the
       * configuration screen and the free end screen of Callsigns (ui-ux.md 5.21.2, 5.21.5).
       */
      const training_phase_t phase = training_phase();
      const bool is_selector_screen =
          (is_training_screen && ((phase == TRAINING_PHASE_SELECT) || (phase == TRAINING_PHASE_END))) ||
          (is_free_screen && ((phase == TRAINING_PHASE_IDLE) || (phase == TRAINING_PHASE_END)));
      const size_t programs_before = fake_iap_command_count(CMD_PROGRAM);
      const uint16_t selected = training_selected_level();
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
        if (is_selector_screen) {
          /*
           * A start from the start screen, and the start screen that enter opens on the end screen, draw straight
           * into the framebuffer. So does each step of the Callsigns configuration screen, and its frame must equal
           * a whole-screen draw of the new state (ui-ux.md UX-87).
           */
          snapshot(before);
        }
        menu_render();
        snapshot(after);
        if (!TEST_ASSERT_EQUAL(0, memcmp(before, after, FB_SIZE))) {
          printf("missed redraw: walk %u step %u screen %u event 0x%02X\n", walk, step, screen, event);
        }
      }

      /* UX-24 and ui-ux.md 6.1: a long event outside a mode screen, and a short event in one, change nothing. */
      if ((is_long && !is_mode_screen) || (!is_long && is_mode_screen && !is_selector_screen) ||
          (is_long && (is_training_screen || is_free_screen) && (code != BUTTON_EVENT_MENU))) {
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
      /*
       * The end screen (D-T45 (4), D-T46, A-L6): prev and next do nothing, and enter opens the start screen at the
       * same selected level with no session.
       */
      if (is_training_screen && (phase == TRAINING_PHASE_END) && !is_long) {
        if ((code == BUTTON_EVENT_PREV) || (code == BUTTON_EVENT_NEXT)) {
          TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
          TEST_ASSERT_EQUAL(selected, training_selected_level());
          TEST_ASSERT_EQUAL(0, memcmp(before, after, FB_SIZE));
        } else if (code == BUTTON_EVENT_ENTER) {
          TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
          TEST_ASSERT_EQUAL(selected, training_selected_level());
        }
      }
      /*
       * Sending starts the keyer with a session, only from its start screen (D-T40, D-T46 (6)). A TX Callsigns
       * session starts it from the configuration screen (AC-T58 (1)).
       */
      if (((screen != menu_screen()) && (menu_screen() == MENU_SCREEN_FREE_INPUT)) ||
          ((screen == MENU_SCREEN_SENDING) && (phase == TRAINING_PHASE_SELECT) && (event == BUTTON_EVENT_ENTER)) ||
          (is_free_screen && (phase == TRAINING_PHASE_IDLE) && (training_phase() == TRAINING_PHASE_GAP) &&
           (training_mode() == TRAINING_MODE_SENDING))) {
        if (menu_screen() == MENU_SCREEN_FREE_INPUT) {
          ++free_input_entries;
        }
        TEST_ASSERT_EQUAL(starts_before + 1U, fake_keyer_start_count());
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_KEY_TYPE), fake_keyer_last_key_type());
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_PADDLE_SWAP) != 0U, fake_keyer_last_is_swapped());
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_TX_WPM), fake_keyer_last_wpm());
        TEST_ASSERT_EQUAL(settings_get(SETTINGS_ID_TX_EFF_WPM), fake_keyer_last_eff_wpm());
      } else {
        TEST_ASSERT_EQUAL(starts_before, fake_keyer_start_count());
      }
      /* The keyer runs exactly while Free Input is open or a Sending session runs (D-T40), free sessions too. */
      TEST_ASSERT_EQUAL((menu_screen() == MENU_SCREEN_FREE_INPUT) ||
                            (((menu_screen() == MENU_SCREEN_SENDING) || ((menu_screen() == MENU_SCREEN_FREE_TRAINING) &&
                                                                         (training_mode() == TRAINING_MODE_SENDING))) &&
                             (training_phase() >= TRAINING_PHASE_GAP) && (training_phase() <= TRAINING_PHASE_RESULT)),
                        fake_keyer_is_running());
      /* Q-C7: Free Training never writes the EEPROM. A leave stops the session and gives the buttons back. */
      if (is_free_screen) {
        ++free_events;
        TEST_ASSERT_EQUAL(programs_before, fake_iap_command_count(CMD_PROGRAM));
        TEST_ASSERT_EQUAL(erases_before, fake_iap_erase_count(0U));
        if (training_phase() == TRAINING_PHASE_GAP) {
          ++free_session_events;
        }
        if (menu_screen() != MENU_SCREEN_FREE_TRAINING) {
          ++free_leaves;
          TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
          TEST_ASSERT_EQUAL(MENU_NODE_FREE_TRAINING, menu_node());
          TEST_ASSERT_EQUAL(0U, menu_cursor());
          TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
          TEST_ASSERT(!fake_player_is_running());
          TEST_ASSERT(!fake_keyer_is_running());
          TEST_ASSERT(is_tap_off_and_enter_enabled());
        }
      }
      if ((menu_screen() == MENU_SCREEN_FREE_INPUT) && ((rng_next() % 2U) == 0U)) {
        feed_free_input();
      }

      TEST_ASSERT((menu_depth >= 1U) && (menu_depth <= MENU_STACK_DEPTH));
      TEST_ASSERT(menu_node() < MENU_NODE_COUNT);
      TEST_ASSERT(menu_cursor() < menu_nodes[menu_node()].item_count);
      TEST_ASSERT(menu_screen() <= MENU_SCREEN_FREE_TRAINING);
      TEST_ASSERT(menu_screen() != MENU_SCREEN_SPLASH);
      TEST_ASSERT(menu_dialog_choice() <= MENU_DIALOG_YES);
      if (menu_screen() == MENU_SCREEN_EDIT) {
        TEST_ASSERT((item_under_cursor()->kind == MENU_KIND_VALUE) || (item_under_cursor()->kind == MENU_KIND_CHOICE));
      } else if (menu_screen() == MENU_SCREEN_RECEIVING) {
        TEST_ASSERT(item_under_cursor()->kind == MENU_KIND_MODE);
        TEST_ASSERT_EQUAL(MENU_MODE_RECEIVING, item_under_cursor()->target);
      } else if (menu_screen() == MENU_SCREEN_SENDING) {
        TEST_ASSERT(item_under_cursor()->kind == MENU_KIND_MODE);
        TEST_ASSERT_EQUAL(MENU_MODE_SENDING, item_under_cursor()->target);
      } else if (menu_screen() == MENU_SCREEN_FREE_INPUT) {
        TEST_ASSERT(item_under_cursor()->kind == MENU_KIND_MODE);
        TEST_ASSERT_EQUAL(MENU_MODE_FREE_INPUT, item_under_cursor()->target);
      } else if (menu_screen() == MENU_SCREEN_FREE_TRAINING) {
        TEST_ASSERT(item_under_cursor()->kind == MENU_KIND_MODE);
        TEST_ASSERT_EQUAL(MENU_MODE_CALLSIGNS, item_under_cursor()->target);
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
        TEST_ASSERT_EQUAL(MAIN_SETTINGS_ROW, menu_cursor());
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
  printf("free training events %u, in a session %u, leaves %u\n", (unsigned)free_events, (unsigned)free_session_events,
         (unsigned)free_leaves);
  TEST_ASSERT(free_events >= 100U);
  TEST_ASSERT(free_session_events >= 20U);
  TEST_ASSERT(free_leaves >= 20U);
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
