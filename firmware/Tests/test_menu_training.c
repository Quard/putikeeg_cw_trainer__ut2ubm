/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Menu parts of the training change (references/internal/training_spec.md D-T8, D-T11, D-T12, D-T19, sections 12.1
 * and 12.4; AC-T21, AC-T22), and Settings > Training (menu_subsystem_spec.md sections 19.5 and 20.5, AC-M19, AC-M20,
 * AC-M28, AC-M29).
 */

#include <stdlib.h>

#include "eeprom/eeprom_map.h"
#include "menu_internal.h"
#include "modes/training.h"
#include "support/training_harness.h"
#include "util/crc8.h"

#define RESET_STATISTICS_ITEM 4U /* fifth Settings item, on screen row 5 */
#define WALK_COUNT 200U
#define WALK_STEPS 300U

/* Main menu: Settings is before the last item About; Settings: Reset Statistics is the last item. */
static void confirm_reset(void) {
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_DIALOG, menu_screen());
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
}

static void start(void) {
  fake_iap_reset();
  harness_power_on();
}

/* AC-T22, V-21: a stored reset shows Statistics reset. */
static void test_reset_stored(void) {
  start();
  fake_stats_set_reset_result(true);
  confirm_reset();
  TEST_ASSERT_EQUAL(1U, fake_stats_reset_count());
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_STATS_RESET, menu_message());
  TEST_ASSERT(hdr("Reset Statistics"));
  TEST_ASSERT(centered(3, "Statistics reset"));
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  TEST_ASSERT_EQUAL(RESET_STATISTICS_ITEM, menu_cursor());
}

/* AC-T22, D-T19: a failed reset shows Not saved over Settings, and OK returns to Settings on Reset Statistics. */
static void test_reset_not_stored(void) {
  start();
  fake_stats_set_reset_result(false);
  confirm_reset();
  TEST_ASSERT_EQUAL(1U, fake_stats_reset_count());
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  TEST_ASSERT_EQUAL(MENU_MESSAGE_NOT_SAVED, menu_message());
  TEST_ASSERT(hdr("Settings"));
  TEST_ASSERT(centered(3, "Not saved"));
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  TEST_ASSERT_EQUAL(RESET_STATISTICS_ITEM, menu_cursor());
  TEST_ASSERT_EQUAL(1U, fake_stats_reset_count());
}

/* fake_stats_reset() sets the result back to true. */
static void test_fake_default_result(void) {
  start();
  fake_stats_set_reset_result(false);
  fake_stats_reset();
  confirm_reset();
  TEST_ASSERT_EQUAL(MENU_MESSAGE_STATS_RESET, menu_message());
}

/* AC-T22: the mode items open their screens. Screen ids follow menu_subsystem_spec.md D-A3. */
static void test_mode_items_open_screens(void) {
  start();
  fake_koch_item_reset();
  fake_koch_item_push("K");
  harness_open_mode(0U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  harness_event(LONG_EVENT(BUTTON_EVENT_MENU));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
  harness_event(LONG_EVENT(BUTTON_EVENT_MENU));
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_FREE_INPUT, menu_screen());
  TEST_ASSERT_EQUAL(7U, MENU_SCREEN_RECEIVING);
  TEST_ASSERT_EQUAL(8U, MENU_SCREEN_SENDING);
}

/* A message shown over a mode screen stops the mode first (training_spec.md section 12.1). */
static void test_message_over_mode_screen(void) {
  start();
  fake_koch_item_reset();
  fake_koch_item_push("K");
  harness_open_mode(0U);
  harness_run_to(HARNESS_T0 + 1000U);
  TEST_ASSERT(fake_player_is_running());
  menu_show_message(MENU_MESSAGE_NOT_SAVED);
  TEST_ASSERT(!fake_player_is_running());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
  menu_show_message(MENU_MESSAGE_NOT_SAVED);
  TEST_ASSERT(!fake_keyer_is_running());
}

/* AC-T22: main.c loads the progress record right after the settings. */
static void test_main_loads_progress(void) {
  char path[512];
  char text[8192];
  const char* file = __FILE__;
  const char* slash = strrchr(file, '/');
  FILE* source;
  size_t length;
  const char* settings_call;
  const char* progress_call;

  TEST_ASSERT(slash != NULL);
  (void)snprintf(path, sizeof(path), "%.*s/../src/main.c", (int)(slash - file), file);
  source = fopen(path, "r");
  if (!TEST_ASSERT(source != NULL)) {
    return;
  }
  length = fread(text, 1U, sizeof(text) - 1U, source);
  text[length] = '\0';
  (void)fclose(source);
  settings_call = strstr(text, "(void)settings_load();");
  progress_call = strstr(text, "(void)progress_load();");
  TEST_ASSERT((settings_call != NULL) && (progress_call != NULL));
  if ((settings_call != NULL) && (progress_call != NULL)) {
    const char* between = settings_call + strlen("(void)settings_load();");
    TEST_ASSERT(progress_call > settings_call);
    while ((between < progress_call) && ((*between == ' ') || (*between == '\n'))) {
      ++between;
    }
    TEST_ASSERT(between == progress_call);
  }
}

static unsigned walk_random(void) { return (unsigned)rand(); }

/*
 * AC-T21: a random walk over the menu and the three mode screens, with real buttons, keyer events and player ends.
 * The fake keyer and the fake player never run at the same time.
 */
static void test_random_walk_keyer_and_player_exclusive(void) {
  static const button_event_t events[] = {
      BUTTON_EVENT_NEXT,
      BUTTON_EVENT_PREV,
      BUTTON_EVENT_MENU,
      BUTTON_EVENT_ENTER,
      LONG_EVENT(BUTTON_EVENT_MENU),
      LONG_EVENT(BUTTON_EVENT_PREV),
      LONG_EVENT(BUTTON_EVENT_NEXT),
      REPEAT_EVENT(BUTTON_EVENT_NEXT),
  };
  unsigned walk;
  size_t receiving_visits = 0U;
  size_t sending_visits = 0U;

  srand(1234U);
  for (walk = 0U; walk < WALK_COUNT; ++walk) {
    unsigned step;

    start();
    fake_koch_item_reset();
    fake_koch_item_push("K");
    fake_koch_item_push("M");
    for (step = 0U; step < WALK_STEPS; ++step) {
      const unsigned choice = walk_random() % 10U;
      if (choice < 6U) {
        harness_set_time((uint16_t)(harness_now + 1U + (walk_random() % 700U)));
        harness_event(events[walk_random() % (sizeof(events) / sizeof(events[0]))]);
      } else if (choice < 8U) {
        unsigned count = walk_random() % 6U;
        while (count-- != 0U) {
          fake_keyer_push_event((keyer_event_t)(1U + (walk_random() % 5U)));
        }
        harness_run_to((uint16_t)(harness_now + 1U + (walk_random() % 400U)));
      } else {
        fake_player_finish();
        harness_run_to((uint16_t)(harness_now + 1U + (walk_random() % 1500U)));
      }
      if (menu_screen() == MENU_SCREEN_RECEIVING) {
        ++receiving_visits;
        TEST_ASSERT(!fake_keyer_is_running());
      }
      if (menu_screen() == MENU_SCREEN_SENDING) {
        ++sending_visits;
        TEST_ASSERT(!fake_player_is_running());
      }
      if ((menu_screen() != MENU_SCREEN_RECEIVING) && (menu_screen() != MENU_SCREEN_SENDING)) {
        TEST_ASSERT(!fake_player_is_running());
      }
      TEST_ASSERT(!(fake_keyer_is_running() && fake_player_is_running()));
      TEST_ASSERT(menu_screen() != MENU_SCREEN_SPLASH);
    }
  }
  printf("receiving steps %u, sending steps %u, overlaps %u\n", (unsigned)receiving_visits, (unsigned)sending_visits,
         (unsigned)harness_overlap_count);
  TEST_ASSERT_EQUAL(0U, harness_overlap_count);
  TEST_ASSERT(receiving_visits >= 200U);
  TEST_ASSERT(sending_visits >= 200U);
  TEST_ASSERT(!fake_keyer_is_overflowed());
}

/* ---------- Main menu level (ui-ux.md 5.16.6, D-T41, AC-T34) ---------- */

/* The Free Input and Settings rows have no level (V-01). */
static bool other_rows_unchanged(void) {
  const row_text_t settings_row[2] = {{0, "Settings", false}, {24, ">", false}};
  return menu_row_is(3, "Free Input", NULL, false) && row_matches(4, HARNESS_NO_BAND, 0, settings_row, 2U);
}

/* AC-T34: stored levels 0, 11, 65534 and 65535 show L1, L12, L65535 and L65535, right-aligned to column 22. */
static void test_menu_level_texts(void) {
  static const uint16_t levels[4] = {0U, 11U, 65534U, 65535U};
  static const char* const texts[4] = {"L1", "L12", "L65535", "L65535"};
  size_t index;

  for (index = 0U; index < 4U; ++index) {
    fake_iap_reset();
    progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, levels[index]);
    progress_fixture_add(PROGRESS_MODE_SENDING, 20U, levels[3U - index]);
    harness_power_on();
    TEST_ASSERT(menu_row_is(1, "Receiving", texts[index], true));
    TEST_ASSERT(menu_row_is(2, "Sending", texts[3U - index], false));
    TEST_ASSERT(other_rows_unchanged());
    TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  }
}

/* V-149: each mode shows the level of its own speed; the cursor move and a speed change redraw it. */
static void test_v149_level_per_speed(void) {
  unsigned index;

  fake_iap_reset();
  progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, 11U);
  progress_fixture_add(PROGRESS_MODE_RECEIVING, 20U, 4U);
  progress_fixture_add(PROGRESS_MODE_SENDING, 20U, 0U);
  progress_fixture_add(PROGRESS_MODE_SENDING, 25U, 65534U);
  harness_power_on();
  TEST_ASSERT(menu_row_is(1, "Receiving", "L12", true));
  TEST_ASSERT(menu_row_is(2, "Sending", "L1", false));
  TEST_ASSERT(other_rows_unchanged());
  harness_clear_dirty();
  harness_event(BUTTON_EVENT_NEXT);
  TEST_ASSERT(menu_row_is(1, "Receiving", "L12", false));
  TEST_ASSERT(menu_row_is(2, "Sending", "L1", true));
  /* Settings > CW: RX Speed 32 -> 20, TX Speed 20 -> 25. */
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  for (index = 0U; index < 12U; ++index) {
    harness_event(BUTTON_EVENT_PREV);
  }
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  for (index = 0U; index < 5U; ++index) {
    harness_event(BUTTON_EVENT_NEXT);
  }
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(20U, settings_get(SETTINGS_ID_RX_WPM));
  TEST_ASSERT_EQUAL(25U, settings_get(SETTINGS_ID_TX_WPM));
  fake_iap_clear_counters();
  harness_event(BUTTON_EVENT_MENU);
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(3U, menu_cursor());
  TEST_ASSERT(menu_row_is(1, "Receiving", "L5", false));
  TEST_ASSERT(menu_row_is(2, "Sending", "L65535", false));
  TEST_ASSERT(menu_row_is(3, "Free Input", NULL, false));
  TEST_ASSERT_EQUAL(0U, fake_iap_erase_count(EEPROM_MAP_PROGRESS_PAGE));
  TEST_ASSERT_EQUAL(0U, fake_iap_program_count(EEPROM_MAP_PROGRESS_ADDRESS));
}

/* V-150, AC-T34: a pass at the earned level shows the new level; a reset shows L1 again. */
static void test_v150_level_after_pass_and_reset(void) {
  unsigned number;

  fake_iap_reset();
  harness_power_on();
  fake_koch_item_reset();
  fake_koch_item_push("K");
  harness_open_mode(0U);
  for (number = 0U; number < 50U; ++number) {
    unsigned guard = 0U;
    while ((training_phase() != TRAINING_PHASE_ITEM) && (guard < 5000U)) {
      harness_run_to((uint16_t)(harness_now + 1U));
      ++guard;
    }
    fake_player_finish();
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_event(BUTTON_EVENT_MENU); /* run 0 holds K */
  }
  harness_run_to((uint16_t)(harness_now + 700U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(menu_row_is(1, "Receiving", "L2", true));
  TEST_ASSERT(menu_row_is(2, "Sending", "L1", false));
  confirm_reset();
  /* The menu tests link a fake stats_reset(); the real one is progress_reset() (src/stats/stats.c). */
  TEST_ASSERT_EQUAL(PROGRESS_OK, progress_reset());
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT(menu_row_is(1, "Receiving", "L1", false));
  TEST_ASSERT(menu_row_is(2, "Sending", "L1", false));
}

/* ---------- Settings > Training (ui-ux.md sections 5.19 and 9.7, menu_subsystem_spec.md section 19.5) ---------- */

#define TRAINING_ROW_RX 1
#define TRAINING_ROW_TX 2
#define TRAINING_ROW_WORD 3
#define TRAINING_ROW_CODE 4
#define TRAINING_ROW_MARKER 5

/* A list row: @p label, and @p value right-aligned to column 22 or the submenu mark in column 24 for NULL. */
static bool list_row(int r, const char* label, const char* value, bool is_selected) {
  row_text_t texts[2];
  texts[0].column = 0;
  texts[0].text = label;
  texts[0].is_inverted = is_selected;
  texts[1].column = (value != NULL) ? (23 - (int)strlen(value)) : 24;
  texts[1].text = (value != NULL) ? value : ">";
  texts[1].is_inverted = is_selected;
  return row_matches(r, is_selected ? 0 : HARNESS_NO_BAND, 127, texts, 2U);
}

static bool action_row(int r, const char* label, bool is_selected) {
  const row_text_t text = {0, label, is_selected};
  return row_matches(r, is_selected ? 0 : HARNESS_NO_BAND, 127, &text, 1U);
}

/* EDIT(r, fw) of ui-ux.md section 9: the band from '<' (column 21 - fw) to x 127, the label in normal video. */
static bool edit_row(int r, const char* label, const char* value, int field_width, bool has_less, bool has_more) {
  row_text_t texts[4];
  size_t count = 2U;
  const int band_x0 = 2 + 5 * (21 - field_width) - 1;

  texts[0].column = 0;
  texts[0].text = label;
  texts[0].is_inverted = false;
  texts[1].column = 23 - (int)strlen(value);
  texts[1].text = value;
  texts[1].is_inverted = true;
  if (has_less) {
    texts[count].column = 21 - field_width;
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
  return row_matches(r, band_x0, 127, texts, count);
}

/* The Settings list of V-192 with the cursor on item @p selected. */
static bool settings_list_is(unsigned selected) {
  return hdr("Settings") && list_row(1, "CW", NULL, selected == 0U) && list_row(2, "Training", NULL, selected == 1U) &&
         list_row(3, "Prosigns", "Both", selected == 2U) && list_row(4, "Display", NULL, selected == 3U) &&
         action_row(5, "Reset Statistics", selected == 4U) && blank(6) && blank(7);
}

/* The Training list with the values @p rx, @p tx, @p word, @p code and @p marker and the cursor on row 1. */
static bool training_list_is(const char* rx, const char* tx, const char* word, const char* code, const char* marker) {
  return hdr("Settings > Training") && list_row(TRAINING_ROW_RX, "RX Attempts", rx, true) &&
         list_row(TRAINING_ROW_TX, "TX Attempts", tx, false) &&
         list_row(TRAINING_ROW_WORD, "Max Word Length", word, false) &&
         list_row(TRAINING_ROW_CODE, "Show CW Code", code, false) &&
         list_row(TRAINING_ROW_MARKER, "TX Char Marker", marker, false) && blank(6) && blank(7);
}

/* TRAINING of ui-ux.md section 9.7 without the power-on: open Settings, then Training. */
static void open_training(void) {
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
}

static size_t settings_writes(void) { return fake_iap_command_count(2U) + fake_iap_command_count(3U); }

/* AC-M19, AC-M28: the tree of menu_subsystem_spec.md sections 19.5 and 20.5. */
static void test_training_node_tree(void) {
  static const char* const labels[5] = {"RX Attempts", "TX Attempts", "Max Word Length", "Show CW Code",
                                        "TX Char Marker"};
  static const uint8_t ids[5] = {SETTINGS_ID_RX_UNLIMITED, SETTINGS_ID_TX_UNLIMITED, SETTINGS_ID_MAX_WORD_LEN,
                                 SETTINGS_ID_SHOW_CW_CODE, SETTINGS_ID_TX_MARKER_HIDDEN};
  static const uint8_t widths[5] = {9U, 9U, 1U, 3U, 3U};
  const menu_node_t* settings = &menu_nodes[MENU_NODE_SETTINGS];
  const menu_node_t* training = &menu_nodes[MENU_NODE_TRAINING];
  size_t index;

  TEST_ASSERT_EQUAL(4U, MENU_NODE_TRAINING);
  TEST_ASSERT_EQUAL(5U, settings->item_count);
  TEST_ASSERT_EQUAL(MENU_KIND_SUBMENU, settings->items[1].kind);
  TEST_ASSERT_EQUAL(MENU_NODE_TRAINING, settings->items[1].target);
  TEST_ASSERT(settings->is_save_on_exit);
  TEST_ASSERT_EQUAL(0, strcmp("Settings > Training", training->title));
  TEST_ASSERT_EQUAL(5U, training->item_count);
  TEST_ASSERT(training->item_count < MENU_VISIBLE_ROWS);
  TEST_ASSERT(!training->is_save_on_exit);
  for (index = 0U; index < 5U; ++index) {
    const menu_item_t* item = &training->items[index];
    TEST_ASSERT_EQUAL(0, strcmp(labels[index], item->label));
    TEST_ASSERT_EQUAL(ids[index], item->target);
    TEST_ASSERT_EQUAL(widths[index], item->field_width);
    TEST_ASSERT(item->unit == NULL);
  }
  for (index = 0U; index < 2U; ++index) {
    const menu_item_t* item = &training->items[index];
    TEST_ASSERT_EQUAL(MENU_KIND_CHOICE, item->kind);
    TEST_ASSERT_EQUAL(0, strcmp("3", item->texts[0]));
    TEST_ASSERT_EQUAL(0, strcmp("Unlimited", item->texts[1]));
  }
  TEST_ASSERT_EQUAL(MENU_KIND_VALUE, training->items[2].kind);
  TEST_ASSERT_EQUAL(MENU_KIND_CHOICE, training->items[3].kind);
  /* The marker id stores "hidden", so the texts are in the inverse order of Show CW Code. */
  TEST_ASSERT_EQUAL(MENU_KIND_CHOICE, training->items[4].kind);
  TEST_ASSERT_EQUAL(0, strcmp("On", training->items[4].texts[0]));
  TEST_ASSERT_EQUAL(0, strcmp("Off", training->items[4].texts[1]));
}

/* V-192, AC-M20: the lists; the back from Training with no edit writes nothing and keeps the cursor on Training. */
static void test_v192_training_list(void) {
  start();
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(settings_list_is(0U));
  harness_event(BUTTON_EVENT_NEXT);
  TEST_ASSERT(settings_list_is(1U));
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_TRAINING, menu_node());
  TEST_ASSERT(training_list_is("3", "3", "5", "On", "On"));
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  TEST_ASSERT(settings_list_is(1U));
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/* V-193, AC-M20: both attempt settings to Unlimited, one save at the exit from Settings, the values after a re-init. */
static void test_v193_attempts_edit(void) {
  start();
  open_training();
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
  TEST_ASSERT(edit_row(TRAINING_ROW_RX, "RX Attempts", "3", 9, false, true));
  harness_clear_dirty();
  harness_event(BUTTON_EVENT_PREV);
  TEST_ASSERT(dirty_is(0U));
  harness_event(BUTTON_EVENT_NEXT);
  TEST_ASSERT(edit_row(TRAINING_ROW_RX, "RX Attempts", "Unlimited", 9, true, false));
  harness_clear_dirty();
  harness_event(BUTTON_EVENT_NEXT);
  TEST_ASSERT(dirty_is(0U));
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(list_row(TRAINING_ROW_RX, "RX Attempts", "Unlimited", true));
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(list_row(TRAINING_ROW_TX, "TX Attempts", "Unlimited", true));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_RX_UNLIMITED));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_TX_UNLIMITED));
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(0U, settings_writes());
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
  TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(2U));
  TEST_ASSERT_EQUAL(0x03U, fake_iap_memory()[14]);
  harness_power_on();
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_RX_UNLIMITED));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_TX_UNLIMITED));
  open_training();
  TEST_ASSERT(training_list_is("Unlimited", "Unlimited", "5", "On", "On"));
}

/* V-194: the two moved items edit the same ids and keep their field widths. */
static void test_v194_moved_items(void) {
  unsigned step;

  start();
  open_training();
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(edit_row(TRAINING_ROW_WORD, "Max Word Length", "5", 1, true, true));
  for (step = 0U; step < 2U; ++step) {
    harness_event(BUTTON_EVENT_NEXT);
  }
  TEST_ASSERT(edit_row(TRAINING_ROW_WORD, "Max Word Length", "7", 1, true, true));
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(edit_row(TRAINING_ROW_CODE, "Show CW Code", "On", 3, true, false));
  harness_event(BUTTON_EVENT_PREV);
  TEST_ASSERT(edit_row(TRAINING_ROW_CODE, "Show CW Code", "Off", 3, false, true));
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_MENU);
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
  harness_power_on();
  TEST_ASSERT_EQUAL(7U, settings_get(SETTINGS_ID_MAX_WORD_LEN));
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_SHOW_CW_CODE));
  open_training();
  TEST_ASSERT(training_list_is("3", "3", "7", "Off", "On"));
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT(settings_list_is(1U));
}

/* V-195, F-27: a layout version 3 record keeps its values; the first save writes version 4. */
static void test_v195_version_3_upgrade(void) {
  uint8_t record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x03U, 0xBCU, 0x02U, 0x20U, 0x20U, 0x14U,
                                          0x14U, 0x02U, 0x64U, 0x07U, 0x08U, 0x20U, 0x00U, 0xFFU};
  unsigned step;

  record[14] = crc8_compute(record, 14U);
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, sizeof(record));
  harness_power_on();
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(0U, settings_writes());
  TEST_ASSERT_EQUAL(700U, settings_get(SETTINGS_ID_TONE_HZ));
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(list_row(3, "Prosigns", "Prosign", false));
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(training_list_is("3", "3", "7", "Off", "On"));
  harness_event(BUTTON_EVENT_MENU);
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_NEXT);
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_MENU);
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
  TEST_ASSERT_EQUAL(SETTINGS_LAYOUT_VERSION, fake_iap_memory()[2]);
  harness_power_on();
  TEST_ASSERT_EQUAL(710U, settings_get(SETTINGS_ID_TONE_HZ));
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_NAME, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  for (step = 0U; step < 2U; ++step) {
    TEST_ASSERT_EQUAL(0U, settings_get((settings_id_t)(SETTINGS_ID_RX_UNLIMITED + step)));
  }
  open_training();
  TEST_ASSERT(training_list_is("3", "3", "7", "Off", "On"));
}

/* The Training list with the cursor on item @p selected and the default values. */
static bool training_cursor_is(unsigned selected) {
  return hdr("Settings > Training") && list_row(TRAINING_ROW_RX, "RX Attempts", "3", selected == 0U) &&
         list_row(TRAINING_ROW_TX, "TX Attempts", "3", selected == 1U) &&
         list_row(TRAINING_ROW_WORD, "Max Word Length", "5", selected == 2U) &&
         list_row(TRAINING_ROW_CODE, "Show CW Code", "On", selected == 3U) &&
         list_row(TRAINING_ROW_MARKER, "TX Char Marker", "On", selected == 4U) && blank(6) && blank(7);
}

static bool page_0_unchanged(void) { return (harness_changed_pages() & PAGE(0U)) == 0U; }

/* V-207, AC-M28: the fifth item, the cursor wraps both ways, and the list does not scroll. */
static void test_v207_marker_item_cursor(void) {
  unsigned step;

  start();
  open_training();
  TEST_ASSERT(training_cursor_is(0U));
  for (step = 1U; step <= 4U; ++step) {
    harness_mark_frame();
    harness_event(BUTTON_EVENT_NEXT);
    TEST_ASSERT(page_0_unchanged());
    TEST_ASSERT(training_cursor_is(step));
  }
  TEST_ASSERT_EQUAL(4U, menu_cursor());
  harness_mark_frame();
  harness_event(BUTTON_EVENT_NEXT);
  TEST_ASSERT(page_0_unchanged());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT(training_cursor_is(0U));
  harness_mark_frame();
  harness_event(BUTTON_EVENT_PREV);
  TEST_ASSERT(page_0_unchanged());
  TEST_ASSERT_EQUAL(4U, menu_cursor());
  TEST_ASSERT(training_cursor_is(4U));
  TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
}

/* Opens the edit of TX Char Marker from the Training list with the cursor on row 1. */
static void edit_marker(void) {
  unsigned step;

  for (step = 0U; step < 4U; ++step) {
    harness_event(BUTTON_EVENT_NEXT);
  }
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
}

/* V-208, AC-M29: the edit order is On, then Off; each exit from Settings saves once, and the value survives a re-init.
 */
static void test_v208_marker_edit_and_save(void) {
  start();
  open_training();
  edit_marker();
  TEST_ASSERT(edit_row(TRAINING_ROW_MARKER, "TX Char Marker", "On", 3, false, true));
  harness_clear_dirty();
  harness_event(BUTTON_EVENT_PREV);
  TEST_ASSERT(dirty_is(0U));
  harness_event(BUTTON_EVENT_NEXT);
  TEST_ASSERT(edit_row(TRAINING_ROW_MARKER, "TX Char Marker", "Off", 3, true, false));
  harness_clear_dirty();
  harness_event(BUTTON_EVENT_NEXT);
  TEST_ASSERT(dirty_is(0U));
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(list_row(TRAINING_ROW_MARKER, "TX Char Marker", "Off", true));
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_TX_MARKER_HIDDEN));
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(0U, settings_writes());
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
  TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(2U));
  TEST_ASSERT_EQUAL(0x04U, fake_iap_memory()[14]);
  TEST_ASSERT_EQUAL(0xC8U, fake_iap_memory()[15]);

  harness_power_on();
  TEST_ASSERT_EQUAL(1U, settings_get(SETTINGS_ID_TX_MARKER_HIDDEN));
  open_training();
  TEST_ASSERT(training_list_is("3", "3", "5", "On", "Off"));
  edit_marker();
  TEST_ASSERT(edit_row(TRAINING_ROW_MARKER, "TX Char Marker", "Off", 3, true, false));
  harness_event(BUTTON_EVENT_PREV);
  TEST_ASSERT(edit_row(TRAINING_ROW_MARKER, "TX Char Marker", "On", 3, false, true));
  harness_event(BUTTON_EVENT_ENTER);
  harness_event(BUTTON_EVENT_MENU);
  harness_event(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(1U, fake_iap_erase_count(0U));
  TEST_ASSERT_EQUAL(0x00U, fake_iap_memory()[14]);

  harness_power_on();
  TEST_ASSERT_EQUAL(0U, settings_get(SETTINGS_ID_TX_MARKER_HIDDEN));
  open_training();
  TEST_ASSERT(training_list_is("3", "3", "5", "On", "On"));
}

int main(void) {
  TEST_RUN(test_reset_stored);
  TEST_RUN(test_reset_not_stored);
  TEST_RUN(test_fake_default_result);
  TEST_RUN(test_mode_items_open_screens);
  TEST_RUN(test_message_over_mode_screen);
  TEST_RUN(test_main_loads_progress);
  TEST_RUN(test_random_walk_keyer_and_player_exclusive);
  TEST_RUN(test_menu_level_texts);
  TEST_RUN(test_v149_level_per_speed);
  TEST_RUN(test_v150_level_after_pass_and_reset);
  TEST_RUN(test_training_node_tree);
  TEST_RUN(test_v192_training_list);
  TEST_RUN(test_v193_attempts_edit);
  TEST_RUN(test_v194_moved_items);
  TEST_RUN(test_v195_version_3_upgrade);
  TEST_RUN(test_v207_marker_item_cursor);
  TEST_RUN(test_v208_marker_edit_and_save);
  return TEST_FINISH();
}
