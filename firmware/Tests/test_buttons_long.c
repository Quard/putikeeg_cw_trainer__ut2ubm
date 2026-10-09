/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * Host tests of the long-press events and the P3.2 ownership of the buttons module
 * (references/internal/keyer_spec.md section 6.12, AC-K45 to AC-K48; ui-ux.md section 6.2 and V-75).
 * A hold is measured on the debounced state: the long event needs the button debounced-pressed for
 * BUTTONS_LONG_PRESS_MS after its press event.
 */

#include <string.h>

#include "board/button_pins.h"
#include "board/buttons_config.h"
#include "buttons/buttons.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_tick.h"
#include "support/test_assert.h"

#define LOG_CAPACITY 1024U
#define NEXT BUTTON_BIT_NEXT
#define PREV BUTTON_BIT_PREV
#define MENU BUTTON_BIT_MENU
#define ENTER BUTTON_BIT_ENTER
#define LONG(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))

typedef struct {
  uint16_t time;
  button_event_t event;
} logged_event_t;

static logged_event_t event_log[LOG_CAPACITY];
static size_t event_count;
static uint16_t now;

static void start(uint8_t mask, uint16_t start_time) {
  fake_tick_reset(start_time);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_button_pins_set(mask);
  fake_delay_reset();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  now = (uint16_t)(start_time + 100U);
  event_count = 0U;
}

static void log_event(button_event_t event) {
  if (event_count < LOG_CAPACITY) {
    event_log[event_count].time = now;
    event_log[event_count].event = event;
    ++event_count;
  }
}

/* One main-loop pass at the current time: polls until no event is left. */
static void pass(void) {
  button_event_t event;
  fake_tick_set_now(now);
  do {
    event = buttons_poll();
    if (event != BUTTON_EVENT_NONE) {
      log_event(event);
    }
  } while (event != BUTTON_EVENT_NONE);
}

/* Holds @p mask for @p duration ms with one pass per ms. */
static void hold(uint8_t mask, uint16_t duration) {
  uint16_t i;
  fake_button_pins_set(mask);
  for (i = 0U; i < duration; ++i) {
    pass();
    now = (uint16_t)(now + 1U);
  }
}

static size_t count_events(button_event_t event) {
  size_t count = 0U;
  size_t i;
  for (i = 0U; i < event_count; ++i) {
    if (event_log[i].event == event) {
      ++count;
    }
  }
  return count;
}

static size_t count_flagged(uint8_t flag) {
  size_t count = 0U;
  size_t i;
  for (i = 0U; i < event_count; ++i) {
    if ((event_log[i].event & flag) != 0U) {
      ++count;
    }
  }
  return count;
}

static uint16_t time_of(button_event_t event) {
  size_t i;
  for (i = 0U; i < event_count; ++i) {
    if (event_log[i].event == event) {
      return event_log[i].time;
    }
  }
  return 0xFFFFU;
}

/* Presses @p mask and returns the time of its press event. */
static uint16_t press(uint8_t mask) {
  hold(mask, BUTTONS_DEBOUNCE_MS);
  return now;
}

/* AC-K45: one long event, exactly BUTTONS_LONG_PRESS_MS after the press event. */
static void check_one_long_event(uint8_t mask, button_event_t code) {
  uint16_t press_time;

  start(0U, 300U);
  press_time = press(mask);
  hold(mask, 3000U);
  hold(0U, 100U);
  TEST_ASSERT_EQUAL(1U, count_events(code));
  TEST_ASSERT_EQUAL(press_time, time_of(code));
  TEST_ASSERT_EQUAL(1U, count_events(LONG(code)));
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS), time_of(LONG(code)));
}

/* The debounced state stays pressed for @p pressed_ms after the press event. */
static size_t long_events_for_hold(uint8_t mask, uint16_t pressed_ms) {
  start(0U, 0U);
  (void)press(mask);
  hold(mask, (uint16_t)(pressed_ms - BUTTONS_DEBOUNCE_MS));
  hold(0U, 2000U);
  return count_flagged(BUTTON_EVENT_LONG_FLAG);
}

static void test_long_press_once_at_threshold(void) {
  check_one_long_event(MENU, BUTTON_EVENT_MENU);
  check_one_long_event(PREV, BUTTON_EVENT_PREV);
  check_one_long_event(NEXT, BUTTON_EVENT_NEXT);
  TEST_ASSERT_EQUAL(0U, long_events_for_hold(MENU, BUTTONS_LONG_PRESS_MS - 1U));
  TEST_ASSERT_EQUAL(0U, long_events_for_hold(PREV, BUTTONS_LONG_PRESS_MS - 1U));
  TEST_ASSERT_EQUAL(0U, long_events_for_hold(NEXT, BUTTONS_LONG_PRESS_MS - 1U));
  TEST_ASSERT_EQUAL(1U, long_events_for_hold(MENU, BUTTONS_LONG_PRESS_MS + 1U));
  TEST_ASSERT_EQUAL(1U, long_events_for_hold(NEXT, BUTTONS_LONG_PRESS_MS + 1U));
}

static void test_no_long_press_for_enter(void) {
  start(0U, 0U);
  hold(ENTER, 5000U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[0].event);
}

static void test_no_long_press_for_button_held_at_start(void) {
  start(MENU | PREV | NEXT, 0U);
  /* MENU held at start-up runs the recovery window; it still has no press edge. */
  hold(MENU | PREV | NEXT, 3000U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(0U, event_count);
  (void)press(MENU);
  hold(MENU, 1000U);
  TEST_ASSERT_EQUAL(1U, count_events(LONG(BUTTON_EVENT_MENU)));
}

/* AC-K46, V-75: the long event and a repeat are due in the same poll; the long event comes first. */
static void test_long_before_repeat(void) {
  uint16_t press_time;
  uint16_t t;

  start(0U, 0U);
  press_time = press(PREV);
  /* One poll per ms up to just before the shared due time, so no event waits. */
  fake_button_pins_set(PREV);
  for (t = 0U; t < BUTTONS_LONG_PRESS_MS; ++t) {
    pass();
    now = (uint16_t)(now + 1U);
  }
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS), now);
  TEST_ASSERT_EQUAL(0U, count_events(LONG(BUTTON_EVENT_PREV)));
  fake_tick_set_now(now);
  TEST_ASSERT_EQUAL(LONG(BUTTON_EVENT_PREV), buttons_poll());
  TEST_ASSERT_EQUAL(REPEAT(BUTTON_EVENT_PREV), buttons_poll());
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NONE, buttons_poll());
}

/* AC-K46: prev and next held together give their own long events at their own moments. */
static void test_prev_and_next_long_each(void) {
  uint16_t prev_time;
  uint16_t next_time;

  start(0U, 0U);
  prev_time = press(PREV);
  hold(PREV, 100U);
  hold(PREV | NEXT, BUTTONS_DEBOUNCE_MS);
  next_time = now;
  hold(PREV | NEXT, 2000U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(1U, count_events(LONG(BUTTON_EVENT_PREV)));
  TEST_ASSERT_EQUAL(1U, count_events(LONG(BUTTON_EVENT_NEXT)));
  TEST_ASSERT_EQUAL((uint16_t)(prev_time + BUTTONS_LONG_PRESS_MS), time_of(LONG(BUTTON_EVENT_PREV)));
  TEST_ASSERT_EQUAL((uint16_t)(next_time + BUTTONS_LONG_PRESS_MS), time_of(LONG(BUTTON_EVENT_NEXT)));

  /* Both pressed in the same poll: two long events in the order of section 6.12, step 4. */
  start(0U, 0U);
  prev_time = press(PREV | NEXT | MENU);
  hold(PREV | NEXT | MENU, 1000U);
  TEST_ASSERT_EQUAL((uint16_t)(prev_time + BUTTONS_LONG_PRESS_MS), time_of(LONG(BUTTON_EVENT_MENU)));
  TEST_ASSERT_EQUAL((uint16_t)(prev_time + BUTTONS_LONG_PRESS_MS), time_of(LONG(BUTTON_EVENT_PREV)));
  TEST_ASSERT_EQUAL((uint16_t)(prev_time + BUTTONS_LONG_PRESS_MS), time_of(LONG(BUTTON_EVENT_NEXT)));
  TEST_ASSERT(time_of(LONG(BUTTON_EVENT_MENU)) == time_of(LONG(BUTTON_EVENT_PREV)));
}

/* AC-K47 */
static void test_release_bounce_keeps_hold(void) {
  uint16_t press_time;

  start(0U, 0U);
  press_time = press(MENU);
  hold(MENU, 300U);
  hold(0U, BUTTONS_DEBOUNCE_MS - 1U);
  hold(MENU, 1000U);
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_MENU));
  TEST_ASSERT_EQUAL(1U, count_events(LONG(BUTTON_EVENT_MENU)));
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS), time_of(LONG(BUTTON_EVENT_MENU)));

  /* A real release ends the hold; the next press starts a new one. */
  start(0U, 0U);
  (void)press(MENU);
  hold(MENU, 500U);
  hold(0U, BUTTONS_DEBOUNCE_MS + 5U);
  press_time = press(MENU);
  hold(MENU, 1000U);
  TEST_ASSERT_EQUAL(2U, count_events(BUTTON_EVENT_MENU));
  TEST_ASSERT_EQUAL(1U, count_events(LONG(BUTTON_EVENT_MENU)));
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS), time_of(LONG(BUTTON_EVENT_MENU)));
}

static void test_event_code_removes_both_flags(void) {
  TEST_ASSERT_EQUAL(BUTTON_EVENT_PREV, BUTTON_EVENT_CODE(LONG(BUTTON_EVENT_PREV)));
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, BUTTON_EVENT_CODE(REPEAT(BUTTON_EVENT_NEXT)));
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU,
                    BUTTON_EVENT_CODE(BUTTON_EVENT_MENU | BUTTON_EVENT_LONG_FLAG | BUTTON_EVENT_REPEAT_FLAG));
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, BUTTON_EVENT_CODE(BUTTON_EVENT_ENTER));
  TEST_ASSERT_EQUAL(0U, BUTTON_EVENT_LONG_FLAG & BUTTON_EVENT_REPEAT_FLAG);
}

/* AC-K48: disabled ENTER gives no event and drops a pending one; the other buttons keep working. */
static void test_enter_disabled(void) {
  start(0U, 0U);
  /* MENU and ENTER become stable in the same poll: MENU comes out, ENTER stays pending. */
  fake_button_pins_set(MENU | ENTER);
  hold(MENU | ENTER, BUTTONS_DEBOUNCE_MS);
  fake_tick_set_now(now);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, buttons_poll());
  buttons_set_enter_enabled(false);
  hold(MENU | ENTER, 500U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(0U, count_events(BUTTON_EVENT_ENTER));

  event_count = 0U;
  hold(ENTER, 200U);
  hold(0U, 30U);
  hold(ENTER | NEXT, 100U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(0U, count_events(BUTTON_EVENT_ENTER));
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_NEXT));
  TEST_ASSERT_EQUAL(1U, event_count);
}

static void test_enter_enabled_while_low(void) {
  size_t reads;
  size_t ticks;

  start(0U, 0U);
  buttons_set_enter_enabled(false);
  hold(ENTER, 500U);
  reads = fake_button_pins_read_count();
  ticks = fake_tick_read_count();
  buttons_set_enter_enabled(true);
  TEST_ASSERT_EQUAL(reads + 1U, fake_button_pins_read_count());
  TEST_ASSERT_EQUAL(ticks + 1U, fake_tick_read_count());
  hold(ENTER, 2000U);
  TEST_ASSERT_EQUAL(0U, count_events(BUTTON_EVENT_ENTER));
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(0U, event_count);
  hold(ENTER, 30U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_ENTER));
  TEST_ASSERT_EQUAL(1U, event_count);
}

static void test_enter_enabled_while_high(void) {
  start(0U, 0U);
  buttons_set_enter_enabled(false);
  hold(ENTER, 300U);
  hold(0U, 300U);
  buttons_set_enter_enabled(true);
  hold(0U, 10U);
  TEST_ASSERT_EQUAL(0U, event_count);
  hold(ENTER, 30U);
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_ENTER));
}

/* The other buttons keep their press, repeat and long timing through both calls. */
static void test_enter_ownership_keeps_other_buttons(void) {
  uint16_t press_time;

  start(0U, 0U);
  press_time = press(NEXT);
  hold(NEXT, 200U);
  buttons_set_enter_enabled(false);
  hold(NEXT, 300U);
  buttons_set_enter_enabled(true);
  hold(NEXT, 600U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_NEXT));
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS), time_of(LONG(BUTTON_EVENT_NEXT)));
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_REPEAT_DELAY_MS), time_of(REPEAT(BUTTON_EVENT_NEXT)));
  TEST_ASSERT_EQUAL(1U, count_events(LONG(BUTTON_EVENT_NEXT)));
  TEST_ASSERT_EQUAL(0U, count_events(BUTTON_EVENT_ENTER));
}

int main(void) {
  TEST_RUN(test_long_press_once_at_threshold);
  TEST_RUN(test_no_long_press_for_enter);
  TEST_RUN(test_no_long_press_for_button_held_at_start);
  TEST_RUN(test_long_before_repeat);
  TEST_RUN(test_prev_and_next_long_each);
  TEST_RUN(test_release_bounce_keeps_hold);
  TEST_RUN(test_event_code_removes_both_flags);
  TEST_RUN(test_enter_disabled);
  TEST_RUN(test_enter_enabled_while_low);
  TEST_RUN(test_enter_enabled_while_high);
  TEST_RUN(test_enter_ownership_keeps_other_buttons);
  return TEST_FINISH();
}
