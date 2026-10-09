/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Tone preview hooks H1-H6 of the menu: references/internal/ui-ux.md section 9.1, V-40 to V-53. */

#include <string.h>

#include "backlight/backlight.h"
#include "board/button_pins.h"
#include "buttons/buttons.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_koch_item.h"
#include "fake_menu_outputs.h"
#include "fake_sidetone.h"
#include "fake_tick.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "menu/menu.h"
#include "menu_internal.h"
#include "settings/settings.h"
#include "support/menu_splash.h"
#include "support/test_assert.h"
#include "util/crc8.h"

#define NEXT BUTTON_EVENT_NEXT
#define PREV BUTTON_EVENT_PREV
#define MENU BUTTON_EVENT_MENU
#define ENTER BUTTON_EVENT_ENTER
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))
#define LONG(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))

#define ON_TRUE 1U
#define ON_FALSE 0U

static const uint8_t default_record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                             0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0x54U};

/* Index of the first SLOG entry that no ADD check has seen yet. */
static size_t slog_checked;
/* Main-loop time of the event-driven scenarios (V-52, V-53). */
static uint16_t now;

/* ---------- Start-up as main.c does it, without sidetone_init() ---------- */

static void put_record(uint16_t tone_hz, uint8_t contrast, uint8_t backlight) {
  uint8_t record[SETTINGS_RECORD_SIZE];
  memcpy(record, default_record, sizeof(record));
  record[3] = (uint8_t)(tone_hz & 0xFFU);
  record[4] = (uint8_t)(tone_hz >> 8);
  record[12] = backlight;
  record[13] = contrast;
  record[14] = crc8_compute(record, 14U);
  memcpy(fake_iap_memory(), record, sizeof(record));
}

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
  menu_test_skip_splash();
  if (menu_needs_redraw()) {
    menu_render();
  }
  return is_recovery;
}

static void reset_slog(void) {
  fake_sidetone_reset();
  slog_checked = 0U;
}

static void handle(button_event_t event) {
  menu_handle(event);
  if (menu_needs_redraw()) {
    menu_render();
  }
}

/* CWTONE(hz): Settings > CW, cursor on Tone in browse mode, empty SLOG. */
static void cw_tone(uint16_t tone_hz) {
  fake_iap_reset();
  if (tone_hz != 600U) {
    put_record(tone_hz, 0x20U, 0x08U);
  }
  (void)power_on(0U);
  /* Receiving -> About -> Settings: About is the last main menu item. */
  handle(PREV);
  handle(PREV);
  handle(ENTER);
  handle(ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_CW, menu_node());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT_EQUAL(tone_hz, settings_get(SETTINGS_ID_TONE_HZ));
  reset_slog();
}

/* ---------- Steps of ui-ux.md section 9.1 ---------- */

static void at_event(uint16_t t, button_event_t event) {
  fake_tick_set_now(t);
  handle(event);
  menu_poll();
}

static void at_poll(uint16_t t) {
  fake_tick_set_now(t);
  menu_poll();
}

/* POLL t..u, inclusive, across the tick wrap when u < t. */
static void poll_range(uint16_t first, uint16_t last) {
  uint16_t t = first;
  for (;;) {
    at_poll(t);
    if (t == last) {
      break;
    }
    t = (uint16_t)(t + 1U);
  }
}

/* ADD[...]: exactly these entries came since the last check. kinds/values are pairs; count 0 is ADD[]. */
static void expect_add(const uint8_t* kinds, const uint16_t* values, size_t count, int line) {
  size_t i;
  const size_t added = fake_sidetone_call_count() - slog_checked;
  if (!test_check_equal(count, added, "SLOG entries added", __FILE__, line)) {
    slog_checked = fake_sidetone_call_count();
    return;
  }
  for (i = 0U; i < count; ++i) {
    const fake_sidetone_call_t call = fake_sidetone_call_at(slog_checked + i);
    (void)test_check_equal(kinds[i], call.kind, "SLOG kind", __FILE__, line);
    (void)test_check_equal(values[i], call.value, "SLOG value", __FILE__, line);
  }
  slog_checked = fake_sidetone_call_count();
}

static void expect_none_at(int line) { expect_add(NULL, NULL, 0U, line); }

static void expect_start_at(uint16_t tone_hz, int line) {
  static const uint8_t kinds[2] = {FAKE_SIDETONE_CALL_TONE, FAKE_SIDETONE_CALL_ON};
  const uint16_t values[2] = {tone_hz, ON_TRUE};
  expect_add(kinds, values, 2U, line);
}

static void expect_stop_at(int line) {
  static const uint8_t kinds[1] = {FAKE_SIDETONE_CALL_ON};
  static const uint16_t values[1] = {ON_FALSE};
  expect_add(kinds, values, 1U, line);
}

#define EXPECT_NONE() expect_none_at(__LINE__)
#define EXPECT_START(hz) expect_start_at((hz), __LINE__)
#define EXPECT_STOP() expect_stop_at(__LINE__)

/*
 * Global rule of V-40 to V-53: each on(true) comes directly after a tone(). The menu never calls init, poll or
 * service of the sidetone.
 */
static void check_slog_rules(void) {
  size_t i;
  for (i = 0U; i < fake_sidetone_call_count(); ++i) {
    const fake_sidetone_call_t call = fake_sidetone_call_at(i);
    TEST_ASSERT(call.kind != FAKE_SIDETONE_CALL_INIT);
    if ((call.kind == FAKE_SIDETONE_CALL_ON) && (call.value == ON_TRUE)) {
      TEST_ASSERT((i > 0U) && (fake_sidetone_call_at(i - 1U).kind == FAKE_SIDETONE_CALL_TONE));
    }
  }
  TEST_ASSERT_EQUAL(0U, fake_sidetone_poll_count());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_service_count());
  TEST_ASSERT(!fake_sidetone_is_log_overflowed());
}

static bool pixel(int x, int y) { return lcd_fb_get_pixel((int16_t)x, (int16_t)y); }

/* SEL(r) of ui-ux.md section 9. */
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

static bool is_tone_edit(void) { return (menu_screen() == MENU_SCREEN_EDIT) && (menu_cursor() == 0U); }

/* ---------- Scenarios ---------- */

static void test_v40_v41_start_and_timeout(void) {
  size_t reads;

  cw_tone(600U);
  at_event(10000U, ENTER);
  EXPECT_START(600U);
  TEST_ASSERT(is_tone_edit());

  poll_range(10001U, 10299U);
  EXPECT_NONE();
  at_poll(10300U);
  EXPECT_STOP();
  reads = fake_tick_read_count();
  poll_range(10301U, 11000U);
  EXPECT_NONE();
  TEST_ASSERT_EQUAL(reads, fake_tick_read_count());
  TEST_ASSERT(is_tone_edit());
  TEST_ASSERT_EQUAL(600U, settings_get(SETTINGS_ID_TONE_HZ));
  check_slog_rules();
}

static void test_v42_change_restarts(void) {
  cw_tone(600U);
  at_event(10000U, ENTER);
  EXPECT_START(600U);
  at_event(10200U, NEXT);
  EXPECT_START(610U);
  poll_range(10201U, 10499U);
  EXPECT_NONE();
  at_poll(10500U);
  EXPECT_STOP();
  at_event(11000U, PREV);
  EXPECT_START(600U);
  at_poll(11299U);
  EXPECT_NONE();
  at_poll(11300U);
  EXPECT_STOP();
  check_slog_rules();
}

static void check_limit_press(uint16_t tone_hz, button_event_t toward_limit) {
  cw_tone(tone_hz);
  at_event(10000U, ENTER);
  EXPECT_START(tone_hz);
  at_event(10100U, toward_limit);
  EXPECT_NONE();
  poll_range(10101U, 10299U);
  EXPECT_NONE();
  at_poll(10300U);
  EXPECT_STOP();
  at_event(10500U, toward_limit);
  poll_range(10501U, 11000U);
  EXPECT_NONE();
  TEST_ASSERT_EQUAL(tone_hz, settings_get(SETTINGS_ID_TONE_HZ));
  check_slog_rules();
}

static void test_v43_limit_press_changes_nothing(void) {
  check_limit_press(1000U, NEXT);
  check_limit_press(400U, PREV);
}

static void test_v44_held_repeat(void) {
  uint16_t k;

  cw_tone(600U);
  at_event(10000U, ENTER);
  EXPECT_START(600U);
  at_poll(10300U);
  EXPECT_STOP();
  at_event(11000U, NEXT);
  EXPECT_START(610U);
  poll_range(11001U, 11399U);
  EXPECT_STOP(); /* at 11300: the repeat delay is longer than the preview */
  for (k = 0U; k < 8U; ++k) {
    const uint16_t t = (uint16_t)(11400U + 80U * k);
    at_event(t, REPEAT(NEXT));
    EXPECT_START((uint16_t)(620U + 10U * k));
    if (k < 7U) {
      poll_range((uint16_t)(t + 1U), (uint16_t)(t + 79U));
      EXPECT_NONE();
    }
  }
  poll_range(11961U, 12259U);
  EXPECT_NONE();
  at_poll(12260U);
  EXPECT_STOP();
  TEST_ASSERT_EQUAL(690U, settings_get(SETTINGS_ID_TONE_HZ));
  check_slog_rules();
}

static void test_v45_repeat_into_limit(void) {
  uint16_t t;

  cw_tone(980U);
  at_event(10000U, ENTER);
  EXPECT_START(980U);
  at_event(10100U, NEXT);
  EXPECT_START(990U);
  at_event(10500U, REPEAT(NEXT));
  EXPECT_START(1000U);
  for (t = 10501U; t <= 10900U; ++t) {
    if (((t - 10500U) % 80U) == 0U) {
      at_event(t, REPEAT(NEXT));
    } else {
      at_poll(t);
    }
    if (t == 10799U) {
      EXPECT_NONE();
    } else if (t == 10800U) {
      EXPECT_STOP();
    }
  }
  EXPECT_NONE();
  TEST_ASSERT_EQUAL(1000U, settings_get(SETTINGS_ID_TONE_HZ));
  check_slog_rules();
}

static void test_v46_enter_leaves_edit(void) {
  cw_tone(600U);
  at_event(10000U, ENTER);
  EXPECT_START(600U);
  at_event(10100U, ENTER);
  EXPECT_STOP();
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT(probe_sel(1));
  poll_range(10101U, 10600U);
  EXPECT_NONE();
  check_slog_rules();
}

static void test_v47_menu_leaves_edit_also_after_timeout(void) {
  cw_tone(600U);
  at_event(10000U, ENTER);
  EXPECT_START(600U);
  at_event(10100U, MENU);
  EXPECT_STOP();
  TEST_ASSERT_EQUAL(MENU_NODE_CW, menu_node());
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  poll_range(10101U, 10600U);
  EXPECT_NONE();
  at_event(11000U, ENTER);
  EXPECT_START(600U);
  at_poll(11300U);
  EXPECT_STOP();
  at_event(11500U, MENU);
  EXPECT_STOP();
  TEST_ASSERT_EQUAL(MENU_NODE_CW, menu_node());
  check_slog_rules();
}

static void test_v48_message_over_edit(void) {
  cw_tone(600U);
  at_event(10000U, ENTER);
  EXPECT_START(600U);
  fake_tick_set_now(10100U);
  menu_show_message(MENU_MESSAGE_NOT_SAVED);
  menu_poll();
  EXPECT_STOP();
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  poll_range(10101U, 10600U);
  EXPECT_NONE();
  at_event(10700U, ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  poll_range(10701U, 11200U);
  EXPECT_NONE();

  /* An unknown message id changes no screen, so the edit and its preview go on. */
  at_event(12000U, ENTER);
  EXPECT_START(600U);
  menu_show_message(0xFFU);
  EXPECT_NONE();
  TEST_ASSERT(is_tone_edit());
  check_slog_rules();
}

static void test_v49_new_edit_plays_current_value(void) {
  cw_tone(600U);
  at_event(10000U, ENTER);
  EXPECT_START(600U);
  at_event(10100U, NEXT);
  EXPECT_START(610U);
  at_event(10200U, ENTER);
  EXPECT_STOP();
  at_event(11000U, ENTER);
  EXPECT_START(610U);
  check_slog_rules();
}

static void test_v50_timeout_across_tick_wrap(void) {
  cw_tone(600U);
  at_event(65400U, ENTER);
  EXPECT_START(600U);
  poll_range(65401U, 65535U);
  poll_range(0U, 163U);
  EXPECT_NONE();
  at_poll(164U);
  EXPECT_STOP();
  check_slog_rules();
}

static void test_v51_two_events_in_one_pass(void) {
  static const uint8_t kinds[4] = {FAKE_SIDETONE_CALL_TONE, FAKE_SIDETONE_CALL_ON, FAKE_SIDETONE_CALL_TONE,
                                   FAKE_SIDETONE_CALL_ON};
  static const uint16_t values[4] = {610U, ON_TRUE, 620U, ON_TRUE};

  cw_tone(600U);
  at_event(10000U, ENTER);
  EXPECT_START(600U);
  fake_tick_set_now(10100U);
  handle(NEXT);
  handle(REPEAT(NEXT));
  menu_poll();
  expect_add(kinds, values, 4U, __LINE__);
  poll_range(10101U, 10399U);
  EXPECT_NONE();
  at_poll(10400U);
  EXPECT_STOP();
  check_slog_rules();
}

/* ---------- V-52, V-53: no other screen calls the sidetone ---------- */

static void step(button_event_t event) {
  now = (uint16_t)(now + 1U);
  at_event(now, event);
}

static void steps(button_event_t event, unsigned count) {
  unsigned i;
  for (i = 0U; i < count; ++i) {
    step(event);
  }
}

/* POLL for 500 ms; a stopped preview reads no tick. */
static void poll_500(void) {
  const size_t reads = fake_tick_read_count();
  poll_range((uint16_t)(now + 1U), (uint16_t)(now + 500U));
  now = (uint16_t)(now + 500U);
  TEST_ASSERT_EQUAL(reads, fake_tick_read_count());
}

static void edit_to_both_limits(uint8_t row) {
  steps(NEXT, row);
  step(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
  step(NEXT);
  steps(REPEAT(NEXT), 70U);
  step(PREV);
  steps(REPEAT(PREV), 70U);
  step(NEXT);
  step(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  poll_500();
  steps(PREV, row);
}

static void test_v52_no_sound_outside_tone_edit(void) {
  uint8_t row;

  fake_iap_reset();
  (void)power_on(0U);
  reset_slog();

  /* 1. Main menu: presses and repeats. */
  steps(NEXT, 5U);
  steps(REPEAT(NEXT), 5U);
  steps(REPEAT(PREV), 5U);
  steps(PREV, 2U);
  poll_500();
  /*
   * 2. Receiving and Sending (D-T8), all four buttons in the start pause; only a long menu/back closes them. The
   * mode start sets the tone and makes no other sidetone call.
   */
  fake_koch_item_reset();
  fake_koch_item_push("K");
  for (row = 0U; row < 2U; ++row) {
    steps(REPEAT(PREV), 5U);
    steps(NEXT, row);
    step(ENTER);
    TEST_ASSERT_EQUAL((row == 0U) ? MENU_SCREEN_RECEIVING : MENU_SCREEN_SENDING, menu_screen());
    step(PREV);
    step(NEXT);
    step(ENTER);
    step(MENU);
    TEST_ASSERT_EQUAL((row == 0U) ? MENU_SCREEN_RECEIVING : MENU_SCREEN_SENDING, menu_screen());
    step(LONG(MENU));
    TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    TEST_ASSERT_EQUAL(1U, fake_sidetone_call_count());
    TEST_ASSERT_EQUAL(FAKE_SIDETONE_CALL_TONE, fake_sidetone_call_at(0U).kind);
    reset_slog();
    poll_500();
  }
  /* 3. About (the last main menu item), then Settings > CW: onto Tone and past it in browse mode. */
  steps(REPEAT(NEXT), 5U);
  step(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_ABOUT, menu_screen());
  step(PREV);
  step(NEXT);
  step(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  step(PREV);
  step(ENTER);
  step(ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_CW, menu_node());
  steps(NEXT, 7U);
  steps(PREV, 7U);
  steps(REPEAT(NEXT), 6U);
  steps(REPEAT(PREV), 6U);
  poll_500();
  /* 4. Every value and choice except Tone, to both limits. */
  for (row = 1U; row < 6U; ++row) {
    edit_to_both_limits(row);
  }
  step(MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  edit_to_both_limits(1U); /* Show CW Code */
  edit_to_both_limits(2U); /* Prosigns */
  edit_to_both_limits(3U); /* Max Word Length */
  steps(NEXT, 4U);
  step(ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_DISPLAY, menu_node());
  edit_to_both_limits(0U); /* Backlight */
  edit_to_both_limits(1U); /* Contrast */
  step(MENU);
  steps(PREV, 4U);
  /* 5. The dialog: No, then Yes, then close the message. */
  steps(NEXT, 5U);
  step(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_DIALOG, menu_screen());
  step(ENTER);
  poll_500();
  step(ENTER);
  step(NEXT);
  step(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  step(ENTER);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  poll_500();
  /* 6. Change a value and leave Settings with a save. */
  steps(PREV, 4U);
  step(ENTER);
  step(PREV);
  step(ENTER);
  step(MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT(fake_iap_command_count(3U) > 0U);
  poll_500();

  TEST_ASSERT_EQUAL(0U, fake_sidetone_call_count());
  check_slog_rules();
}

/* Main-loop passes with real buttons: @p mask held for @p duration ms, menu_poll() on each pass. */
static void run_pins(uint8_t mask, uint16_t duration) {
  uint16_t i;
  button_event_t event;
  fake_button_pins_set(mask);
  for (i = 0U; i < duration; ++i) {
    fake_tick_set_now(now);
    event = buttons_poll();
    if (event != BUTTON_EVENT_NONE) {
      handle(event);
    }
    menu_poll();
    now = (uint16_t)(now + 1U);
  }
}

static void test_v53_recovery_start_is_silent(void) {
  fake_iap_reset();
  put_record(600U, 0U, 0U);
  TEST_ASSERT(power_on(BUTTON_BIT_MENU));
  TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
  reset_slog();
  run_pins(0U, 50U);
  run_pins(BUTTON_BIT_MENU, 30U);
  run_pins(0U, 30U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  run_pins(0U, 1000U);
  TEST_ASSERT_EQUAL(0U, fake_sidetone_call_count());
  check_slog_rules();
}

int main(void) {
  TEST_RUN(test_v40_v41_start_and_timeout);
  TEST_RUN(test_v42_change_restarts);
  TEST_RUN(test_v43_limit_press_changes_nothing);
  TEST_RUN(test_v44_held_repeat);
  TEST_RUN(test_v45_repeat_into_limit);
  TEST_RUN(test_v46_enter_leaves_edit);
  TEST_RUN(test_v47_menu_leaves_edit_also_after_timeout);
  TEST_RUN(test_v48_message_over_edit);
  TEST_RUN(test_v49_new_edit_plays_current_value);
  TEST_RUN(test_v50_timeout_across_tick_wrap);
  TEST_RUN(test_v51_two_events_in_one_pass);
  TEST_RUN(test_v52_no_sound_outside_tone_edit);
  TEST_RUN(test_v53_recovery_start_is_silent);
  return TEST_FINISH();
}
