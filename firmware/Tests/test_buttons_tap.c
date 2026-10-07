/*
 * Host tests of the button tap mode (references/internal/training_spec.md D-T7, D-T28 and section 5.9, AC-T15).
 * Press and release times are those of the debounced state.
 */

#include <stdbool.h>
#include <stdint.h>

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

typedef struct {
  uint16_t time;
  button_event_t event;
} logged_event_t;

static logged_event_t event_log[LOG_CAPACITY];
static size_t event_count;
static uint16_t now;

static void start(void) {
  fake_tick_reset(300U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_delay_reset();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  now = 400U;
  event_count = 0U;
}

/* One main-loop pass at the current time: polls until no event is left. */
static void pass(void) {
  button_event_t event;

  fake_tick_set_now(now);
  do {
    event = buttons_poll();
    if ((event != BUTTON_EVENT_NONE) && (event_count < LOG_CAPACITY)) {
      event_log[event_count].time = now;
      event_log[event_count].event = event;
      ++event_count;
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

/* Sets @p mask and returns the time of the pass that debounces it. */
static uint16_t change_to(uint8_t mask) {
  hold(mask, BUTTONS_DEBOUNCE_MS);
  return now;
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

/* A tap gives its press event at the debounced release, and nothing at the press. */
static void check_tap(uint8_t mask, button_event_t code) {
  uint16_t release_time;

  start();
  buttons_set_tap_mode(true);
  (void)change_to(mask);
  hold(mask, 101U);
  TEST_ASSERT_EQUAL(0U, event_count);
  release_time = change_to(0U);
  hold(0U, 3000U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(code, event_log[0].event);
  TEST_ASSERT_EQUAL(release_time, event_log[0].time);
}

static void test_tap_gives_event_at_release(void) {
  check_tap(MENU, BUTTON_EVENT_MENU);
  check_tap(PREV, BUTTON_EVENT_PREV);
  check_tap(NEXT, BUTTON_EVENT_NEXT);
  check_tap(ENTER, BUTTON_EVENT_ENTER);
}

/* A hold gives only the long event at BUTTONS_LONG_PRESS_MS, nothing at the release and no repeat. */
static void check_hold(uint8_t mask, button_event_t code) {
  uint16_t press_time;

  start();
  buttons_set_tap_mode(true);
  press_time = change_to(mask);
  hold(mask, 3000U);
  hold(0U, 3000U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(LONG(code), event_log[0].event);
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS), event_log[0].time);
  TEST_ASSERT_EQUAL(0U, count_flagged(BUTTON_EVENT_REPEAT_FLAG));
}

static void test_hold_gives_only_long_event(void) {
  check_hold(MENU, BUTTON_EVENT_MENU);
  check_hold(PREV, BUTTON_EVENT_PREV);
  check_hold(NEXT, BUTTON_EVENT_NEXT);
  check_hold(ENTER, BUTTON_EVENT_ENTER);
  /* D-T28: the long enter event is the ENTER code with the long flag. */
  TEST_ASSERT_EQUAL(0x44U, LONG(BUTTON_EVENT_ENTER));
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, BUTTON_EVENT_CODE(LONG(BUTTON_EVENT_ENTER)));
}

/* The debounced hold of LONG - 1 ms is a tap; a hold of LONG ms is a long press. */
static void check_long_press_boundary(uint8_t mask, button_event_t code) {
  uint16_t press_time;
  uint16_t release_time;

  start();
  buttons_set_tap_mode(true);
  press_time = change_to(mask);
  hold(mask, (uint16_t)(BUTTONS_LONG_PRESS_MS - BUTTONS_DEBOUNCE_MS - 1U));
  release_time = change_to(0U);
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS - 1U), release_time);
  hold(0U, 2000U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(code, event_log[0].event);
  TEST_ASSERT_EQUAL(release_time, event_log[0].time);

  start();
  buttons_set_tap_mode(true);
  press_time = change_to(mask);
  hold(mask, (uint16_t)(BUTTONS_LONG_PRESS_MS - BUTTONS_DEBOUNCE_MS));
  release_time = change_to(0U);
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS), release_time);
  hold(0U, 2000U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(LONG(code), event_log[0].event);
  TEST_ASSERT_EQUAL(release_time, event_log[0].time);
}

static void test_long_press_boundary(void) {
  check_long_press_boundary(PREV, BUTTON_EVENT_PREV);
  check_long_press_boundary(ENTER, BUTTON_EVENT_ENTER);
}

/* A slow main loop that sees the release after the long-press time still gives only the long event (D-T22). */
static void check_slow_loop_release(uint8_t mask, button_event_t code) {
  uint16_t press_time;

  start();
  buttons_set_tap_mode(true);
  press_time = change_to(mask);
  fake_button_pins_set(mask);
  pass();
  /* No pass from the press until the release is debounced, 900 ms later. */
  fake_button_pins_set(0U);
  now = (uint16_t)(press_time + 900U);
  pass();
  now = (uint16_t)(now + BUTTONS_DEBOUNCE_MS);
  pass();
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(LONG(code), event_log[0].event);
  TEST_ASSERT_EQUAL(0U, count_events(code));
}

static void test_slow_loop_release_after_long_time(void) {
  check_slow_loop_release(NEXT, BUTTON_EVENT_NEXT);
  check_slow_loop_release(ENTER, BUTTON_EVENT_ENTER);
}

/* Tap mode off: ENTER gives its event at the press and never a long event (AC-K45). */
static void test_enter_off_has_no_long_event(void) {
  uint16_t press_time;

  start();
  press_time = change_to(ENTER);
  pass();
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[0].event);
  TEST_ASSERT_EQUAL(press_time, event_log[0].time);
  hold(ENTER, 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(1U, event_count);

  /* The same after tap mode was on and off again. */
  start();
  buttons_set_tap_mode(true);
  buttons_set_tap_mode(false);
  press_time = change_to(ENTER);
  hold(ENTER, 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[0].event);
  TEST_ASSERT_EQUAL(press_time, event_log[0].time);
}

/* Taps of all buttons in one pass come out in the order MENU, ENTER, PREV, NEXT. */
static void test_tap_order(void) {
  start();
  buttons_set_tap_mode(true);
  (void)change_to((uint8_t)(MENU | PREV | NEXT | ENTER));
  hold((uint8_t)(MENU | PREV | NEXT | ENTER), 50U);
  (void)change_to(0U);
  hold(0U, 100U);
  TEST_ASSERT_EQUAL(4U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[1].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_PREV, event_log[2].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, event_log[3].event);
  TEST_ASSERT_EQUAL(event_log[0].time, event_log[3].time);
}

/* Long events due in one poll come out in the order MENU, ENTER, PREV, NEXT. */
static void test_long_order(void) {
  uint16_t press_time;

  start();
  buttons_set_tap_mode(true);
  press_time = change_to((uint8_t)(MENU | PREV | NEXT | ENTER));
  hold((uint8_t)(MENU | PREV | NEXT | ENTER), 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(4U, event_count);
  TEST_ASSERT_EQUAL(LONG(BUTTON_EVENT_MENU), event_log[0].event);
  TEST_ASSERT_EQUAL(LONG(BUTTON_EVENT_ENTER), event_log[1].event);
  TEST_ASSERT_EQUAL(LONG(BUTTON_EVENT_PREV), event_log[2].event);
  TEST_ASSERT_EQUAL(LONG(BUTTON_EVENT_NEXT), event_log[3].event);
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS), event_log[0].time);
  TEST_ASSERT_EQUAL(event_log[0].time, event_log[3].time);
}

/* A hold that started before the switch gives no event, in both directions. */
static void test_switch_drops_held_buttons(void) {
  button_event_t event;

  /* Normal mode: PREV gives its press event; tap mode on while it is held: no long event, nothing at release. */
  start();
  (void)change_to(PREV);
  pass();
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_PREV));
  buttons_set_tap_mode(true);
  hold(PREV, 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(1U, event_count);

  /* A pending press that the loop has not read yet is dropped. */
  start();
  hold((uint8_t)(MENU | PREV), BUTTONS_DEBOUNCE_MS);
  fake_tick_set_now(now);
  event = buttons_poll();
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event);
  buttons_set_tap_mode(true);
  hold((uint8_t)(MENU | PREV), 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(0U, event_count);

  /* Tap mode off while NEXT is held: no repeat, no long event, nothing at release. */
  start();
  buttons_set_tap_mode(true);
  (void)change_to(NEXT);
  hold(NEXT, 100U);
  buttons_set_tap_mode(false);
  hold(NEXT, 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(0U, event_count);

  /* A pending ENTER press is dropped too. */
  start();
  hold((uint8_t)(ENTER | MENU), BUTTONS_DEBOUNCE_MS);
  fake_tick_set_now(now);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, buttons_poll());
  buttons_set_tap_mode(true);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NONE, buttons_poll());
  hold((uint8_t)(ENTER | MENU), 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(0U, event_count);

  /* The menu ENTER press that opens Receiving: no event at its release, no long event for a hold. */
  start();
  (void)change_to(ENTER);
  pass();
  TEST_ASSERT_EQUAL(1U, event_count);
  buttons_set_tap_mode(true);
  hold(ENTER, 100U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(1U, event_count);
  start();
  (void)change_to(ENTER);
  pass();
  buttons_set_tap_mode(true);
  hold(ENTER, 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(1U, event_count);
  /* The next tap gives its event at the release. */
  (void)change_to(ENTER);
  hold(ENTER, 50U);
  hold(0U, 100U);
  TEST_ASSERT_EQUAL(2U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[1].event);

  /* Tap mode off while ENTER is held: no long event and nothing at release. */
  start();
  buttons_set_tap_mode(true);
  (void)change_to(ENTER);
  hold(ENTER, 100U);
  buttons_set_tap_mode(false);
  hold(ENTER, 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(0U, event_count);
}

/* Both values of buttons_set_enter_enabled() drop a long-pending ENTER (D-T28). */
static void test_enter_enabled_drops_long_pending(void) {
  /* false: the keyer takes P3.2 during a hold, so neither a tap nor a long event comes. */
  start();
  buttons_set_tap_mode(true);
  (void)change_to(ENTER);
  hold(ENTER, 100U);
  buttons_set_enter_enabled(false);
  hold(ENTER, 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(0U, event_count);
  buttons_set_enter_enabled(true);
  hold(0U, 100U);
  TEST_ASSERT_EQUAL(0U, event_count);

  /* false and a release before the long-press time: no tap event. */
  start();
  buttons_set_tap_mode(true);
  (void)change_to(ENTER);
  buttons_set_enter_enabled(false);
  hold(ENTER, 50U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(0U, event_count);

  /* true while the contact is closed: no event until it opens and closes again. */
  start();
  buttons_set_tap_mode(true);
  (void)change_to(ENTER);
  hold(ENTER, 100U);
  buttons_set_enter_enabled(true);
  hold(ENTER, 3000U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(0U, event_count);
  start();
  buttons_set_tap_mode(true);
  (void)change_to(ENTER);
  buttons_set_enter_enabled(true);
  hold(ENTER, 50U);
  hold(0U, 1000U);
  TEST_ASSERT_EQUAL(0U, event_count);
  (void)change_to(ENTER);
  hold(ENTER, 50U);
  hold(0U, 100U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[0].event);
}

/* Off again: the normal press, repeat and long events come back. buttons_init() sets tap mode off. */
static void test_off_restores_normal_events(void) {
  uint16_t press_time;

  start();
  buttons_set_tap_mode(true);
  buttons_set_tap_mode(false);
  press_time = change_to(NEXT);
  hold(NEXT, 1000U);
  hold(0U, 100U);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, event_log[0].event);
  TEST_ASSERT_EQUAL(press_time, event_log[0].time);
  TEST_ASSERT(count_flagged(BUTTON_EVENT_REPEAT_FLAG) > 0U);
  TEST_ASSERT_EQUAL(1U, count_events(LONG(BUTTON_EVENT_NEXT)));

  buttons_set_tap_mode(true);
  start();
  press_time = change_to(MENU);
  pass();
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
  TEST_ASSERT_EQUAL(press_time, event_log[0].time);
}

int main(void) {
  TEST_RUN(test_tap_gives_event_at_release);
  TEST_RUN(test_hold_gives_only_long_event);
  TEST_RUN(test_long_press_boundary);
  TEST_RUN(test_slow_loop_release_after_long_time);
  TEST_RUN(test_enter_off_has_no_long_event);
  TEST_RUN(test_tap_order);
  TEST_RUN(test_long_order);
  TEST_RUN(test_switch_drops_held_buttons);
  TEST_RUN(test_enter_enabled_drops_long_pending);
  TEST_RUN(test_off_restores_normal_events);
  return TEST_FINISH();
}
