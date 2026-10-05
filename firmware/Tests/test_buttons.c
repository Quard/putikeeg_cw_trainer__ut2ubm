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
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))

typedef struct {
  uint16_t time;
  button_event_t event;
} logged_event_t;

static logged_event_t event_log[LOG_CAPACITY];
static size_t event_count;
static uint16_t now;

/* Power-on with @p mask pressed at time @p start. The fake tick moves 1 ms per read during buttons_init(). */
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

static size_t count_events(button_event_t event) {
  size_t i;
  size_t count = 0U;
  for (i = 0U; i < event_count; ++i) {
    if (event_log[i].event == event) {
      ++count;
    }
  }
  return count;
}

static void test_bounce_shorter_than_debounce(void) {
  start(0U, 0U);
  hold(NEXT, 5U);
  hold(0U, 3U);
  hold(NEXT, 5U);
  hold(0U, 2U);
  hold(NEXT, 19U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(0U, event_count);
}

static void test_v26_bounce_then_stable_press(void) {
  uint16_t stable_start;
  start(0U, 0U);
  hold(PREV, 5U);
  hold(0U, 3U);
  hold(PREV, 5U);
  hold(0U, 2U);
  stable_start = now;
  hold(PREV, 25U);
  hold(0U, 2U);
  hold(PREV, 2U);
  hold(0U, 1U);
  hold(PREV, 1U);
  hold(0U, 40U);
  hold(0U, 500U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_PREV, event_log[0].event);
  TEST_ASSERT_EQUAL((uint16_t)(stable_start + BUTTONS_DEBOUNCE_MS), event_log[0].time);
}

static void test_press_and_release_each_button(void) {
  static const uint8_t masks[4] = {NEXT, PREV, MENU, ENTER};
  static const button_event_t events[4] = {BUTTON_EVENT_NEXT, BUTTON_EVENT_PREV, BUTTON_EVENT_MENU, BUTTON_EVENT_ENTER};
  size_t i;
  for (i = 0U; i < 4U; ++i) {
    start(0U, 1000U);
    hold(masks[i], 30U);
    hold(0U, 30U);
    hold(masks[i], 30U);
    hold(0U, 30U);
    TEST_ASSERT_EQUAL(2U, event_count);
    TEST_ASSERT_EQUAL(events[i], event_log[0].event);
    TEST_ASSERT_EQUAL(events[i], event_log[1].event);
  }
}

static void test_short_release_is_ignored(void) {
  start(0U, 0U);
  hold(ENTER, 30U);
  hold(0U, 19U);
  hold(ENTER, 30U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(1U, event_count);
}

static void check_repeat(uint8_t mask, button_event_t code) {
  uint16_t press_time;
  size_t i;
  start(0U, 200U);
  hold(mask, BUTTONS_DEBOUNCE_MS);
  press_time = now;
  hold(mask, 1000U);
  hold(0U, 100U);
  TEST_ASSERT_EQUAL(9U, event_count);
  TEST_ASSERT_EQUAL(code, event_log[0].event);
  TEST_ASSERT_EQUAL(press_time, event_log[0].time);
  for (i = 1U; i < event_count; ++i) {
    TEST_ASSERT_EQUAL(REPEAT(code), event_log[i].event);
    TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_REPEAT_DELAY_MS + (i - 1U) * BUTTONS_REPEAT_PERIOD_MS),
                      event_log[i].time);
  }
}

static void test_v08_repeat_next_and_prev(void) {
  check_repeat(NEXT, BUTTON_EVENT_NEXT);
  check_repeat(PREV, BUTTON_EVENT_PREV);
}

static void test_no_repeat_for_menu_and_enter(void) {
  start(0U, 0U);
  hold(MENU, 3000U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
}

static void test_v24_held_enter_and_next(void) {
  start(0U, 0U);
  hold(ENTER, 2000U);
  hold(ENTER | NEXT, 30U);
  hold(ENTER, 2970U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_ENTER));
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_NEXT));
  TEST_ASSERT_EQUAL(2U, event_count);
}

static void test_v31_repeat_follows_last_pressed(void) {
  size_t index_after_release;
  size_t i;
  start(0U, 0U);
  hold(NEXT, 600U);
  TEST_ASSERT(count_events(REPEAT(BUTTON_EVENT_NEXT)) > 0U);
  hold(NEXT | PREV, 600U);
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_PREV));
  TEST_ASSERT(count_events(REPEAT(BUTTON_EVENT_PREV)) > 0U);
  index_after_release = event_count;
  hold(NEXT, 500U);
  /* Repeats of prev can still come during its release debounce; next never repeats again. */
  for (i = index_after_release; i < event_count; ++i) {
    TEST_ASSERT(event_log[i].event != REPEAT(BUTTON_EVENT_NEXT));
    TEST_ASSERT(event_log[i].time < (uint16_t)(event_log[index_after_release - 1U].time + 100U));
  }
}

static void test_repeats_after_long_gap(void) {
  uint16_t press_time;
  button_event_t event;
  size_t repeats = 0U;
  start(0U, 0U);
  hold(NEXT, BUTTONS_DEBOUNCE_MS);
  press_time = now;
  hold(NEXT, 1U);
  TEST_ASSERT_EQUAL(1U, event_count);
  now = (uint16_t)(press_time + 1000U);
  fake_tick_set_now(now);
  do {
    event = buttons_poll();
    if (event == REPEAT(BUTTON_EVENT_NEXT)) {
      ++repeats;
    }
  } while (event != BUTTON_EVENT_NONE);
  /* Due at 400, 480, ..., 960 ms after the press. */
  TEST_ASSERT_EQUAL(8U, repeats);
  now = (uint16_t)(now + 40U);
  fake_tick_set_now(now);
  TEST_ASSERT_EQUAL(REPEAT(BUTTON_EVENT_NEXT), buttons_poll());
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NONE, buttons_poll());
}

static void test_tick_wrap(void) {
  start(0U, 65000U);
  now = 65500U;
  hold(NEXT, 30U);
  TEST_ASSERT_EQUAL(1U, event_count);
  hold(NEXT, 470U);
  TEST_ASSERT_EQUAL(2U, event_count);
  TEST_ASSERT_EQUAL(REPEAT(BUTTON_EVENT_NEXT), event_log[1].event);
  TEST_ASSERT_EQUAL((uint16_t)(65500U + BUTTONS_DEBOUNCE_MS + BUTTONS_REPEAT_DELAY_MS), event_log[1].time);
  hold(0U, 40U);
  hold(MENU, 25U);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[event_count - 1U].event);
}

static void test_v30_order_in_one_poll(void) {
  start(0U, 0U);
  hold(MENU | NEXT, 25U);
  TEST_ASSERT_EQUAL(2U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, event_log[1].event);
  TEST_ASSERT_EQUAL(event_log[0].time, event_log[1].time);

  start(0U, 0U);
  hold(MENU | NEXT | PREV | ENTER, 25U);
  TEST_ASSERT_EQUAL(4U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[1].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_PREV, event_log[2].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, event_log[3].event);
}

static void test_v25_held_at_init(void) {
  start(ENTER | MENU, 0U);
  hold(ENTER | MENU, 2000U);
  TEST_ASSERT_EQUAL(0U, event_count);
  hold(0U, 50U);
  TEST_ASSERT_EQUAL(0U, event_count);
  hold(MENU, 30U);
  hold(0U, 30U);
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
}

static void test_recovery_window(void) {
  uint8_t sequence[80];

  /* V-33: MENU low in every sample up to 50 ms. */
  start(MENU, 10U);
  TEST_ASSERT(buttons_is_recovery_start());
  TEST_ASSERT_EQUAL(BUTTONS_RECOVERY_HOLD_MS + 1U, fake_button_pins_read_count());

  /* V-35: MENU low for 30 ms after the first sample, then high. */
  memset(sequence, MENU, sizeof(sequence));
  memset(&sequence[31], 0, sizeof(sequence) - 31U);
  fake_tick_reset(0U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_button_pins_set_sequence(sequence, sizeof(sequence));
  fake_delay_reset();
  buttons_init();
  TEST_ASSERT(!buttons_is_recovery_start());
  TEST_ASSERT_EQUAL(32U, fake_button_pins_read_count());

  /* MENU released only at the last sample of the window. */
  memset(sequence, MENU, sizeof(sequence));
  sequence[50] = 0U;
  fake_tick_reset(0U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_button_pins_set_sequence(sequence, 51U);
  fake_delay_reset();
  buttons_init();
  TEST_ASSERT(!buttons_is_recovery_start());

  /* V-39: ENTER held, MENU high. */
  start(ENTER, 0U);
  TEST_ASSERT(!buttons_is_recovery_start());
  TEST_ASSERT_EQUAL(1U, fake_button_pins_read_count());

  start(0U, 0U);
  TEST_ASSERT(!buttons_is_recovery_start());
  TEST_ASSERT_EQUAL(1U, fake_button_pins_read_count());
  TEST_ASSERT_EQUAL(1U, fake_button_pins_configure_count());

  /* A recovery start over the tick wrap. */
  start(MENU, 65520U);
  TEST_ASSERT(buttons_is_recovery_start());
}

static void test_settle_delay_before_first_sample(void) {
  start(0U, 0U);
  TEST_ASSERT_EQUAL(1U, fake_delay_count());
  TEST_ASSERT_EQUAL(BUTTONS_SETTLE_MS, fake_delay_ms_at(0U));
  TEST_ASSERT_EQUAL(1U, fake_button_pins_delays_before_first_read());
}

static void test_one_tick_read_per_poll(void) {
  size_t reads;
  start(0U, 0U);
  fake_button_pins_set(NEXT | MENU);
  for (now = 0U; now < 800U; ++now) {
    fake_tick_set_now(now);
    reads = fake_tick_read_count();
    (void)buttons_poll();
    TEST_ASSERT_EQUAL(reads + 1U, fake_tick_read_count());
  }
}

int main(void) {
  TEST_RUN(test_bounce_shorter_than_debounce);
  TEST_RUN(test_v26_bounce_then_stable_press);
  TEST_RUN(test_press_and_release_each_button);
  TEST_RUN(test_short_release_is_ignored);
  TEST_RUN(test_v08_repeat_next_and_prev);
  TEST_RUN(test_no_repeat_for_menu_and_enter);
  TEST_RUN(test_v24_held_enter_and_next);
  TEST_RUN(test_v31_repeat_follows_last_pressed);
  TEST_RUN(test_repeats_after_long_gap);
  TEST_RUN(test_tick_wrap);
  TEST_RUN(test_v30_order_in_one_poll);
  TEST_RUN(test_v25_held_at_init);
  TEST_RUN(test_recovery_window);
  TEST_RUN(test_settle_delay_before_first_sample);
  TEST_RUN(test_one_tick_read_per_poll);
  return TEST_FINISH();
}
