/*
 * Menu parts of the training change (references/internal/training_spec.md D-T8, D-T11, D-T12, D-T19, sections 12.1
 * and 12.4; AC-T21, AC-T22).
 */

#include <stdlib.h>

#include "eeprom/eeprom_map.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define RESET_STATISTICS_ITEM 5U /* sixth Settings item, on screen row 6 */
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
  return TEST_FINISH();
}
