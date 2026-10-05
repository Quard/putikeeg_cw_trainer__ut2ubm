/*
 * QA tests for the buttons module: random bounce trains with irregular poll intervals, repeat catch-up with slow
 * main-loop passes, bounce of a second button during a repeat, and the recovery window with a late tick.
 * The oracles come from menu_subsystem_spec.md section 4.5 and ui-ux.md section 6.2, not from buttons.c.
 */
#include <string.h>

#include "board/button_pins.h"
#include "board/buttons_config.h"
#include "buttons/buttons.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_tick.h"
#include "support/test_assert.h"

#define LOG_CAPACITY 4096U
#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))
#define MAX_SEGMENTS 128U
/* Every bounce run is longer than the largest poll gap, so each run is seen by at least one poll. */
#define MAX_POLL_GAP 8U

typedef struct {
  uint32_t time;
  button_event_t event;
} logged_event_t;

static logged_event_t event_log[LOG_CAPACITY];
static size_t event_count;

/* Deterministic generator, so a failure is repeatable. */
static uint32_t rng_state;
static uint32_t rng_next(void) {
  rng_state = rng_state * 1664525U + 1013904223U;
  return rng_state >> 8;
}
static uint32_t rng_range(uint32_t low, uint32_t high) { return low + (rng_next() % (high - low + 1U)); }

static void start_released(uint16_t start_time) {
  fake_tick_reset(start_time);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_button_pins_set(0U);
  fake_delay_reset();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  event_count = 0U;
}

/* One main-loop pass as main.c does it: at most @p limit events. Returns the number of events. */
static size_t pass_at(uint32_t time, size_t limit) {
  size_t handled = 0U;
  button_event_t event;
  fake_tick_set_now((uint16_t)time);
  while (handled < limit) {
    event = buttons_poll();
    if (event == BUTTON_EVENT_NONE) {
      break;
    }
    if (event_count < LOG_CAPACITY) {
      event_log[event_count].time = time;
      event_log[event_count].event = event;
      ++event_count;
    }
    ++handled;
  }
  return handled;
}

/* Waveform: segment i holds level[i] (pressed or not) from begin[i] on. */
typedef struct {
  uint32_t begin[MAX_SEGMENTS];
  bool is_pressed[MAX_SEGMENTS];
  bool is_stable[MAX_SEGMENTS];
  size_t count;
  uint32_t end;
} waveform_t;

static bool level_at(const waveform_t* wave, uint32_t time) {
  size_t i = 0U;
  while ((i + 1U < wave->count) && (wave->begin[i + 1U] <= time)) {
    ++i;
  }
  return wave->is_pressed[i];
}

/*
 * Builds alternating stable low and stable high periods (60..300 ms) with a bounce train between them: runs of
 * MAX_POLL_GAP + 1 .. 19 ms each, so no run reaches the debounce time and no poll gap hides a run.
 */
static void make_waveform(waveform_t* wave, size_t stable_presses) {
  uint32_t time = 0U;
  size_t press;
  wave->count = 0U;
  wave->begin[wave->count] = time;
  wave->is_pressed[wave->count] = false;
  wave->is_stable[wave->count] = true;
  ++wave->count;
  time += rng_range(60U, 300U);
  for (press = 0U; press < stable_presses; ++press) {
    /* An even run count ends the train on the level opposite to the stable period, so they do not merge. */
    uint32_t bounces = 2U * rng_range(0U, 3U);
    bool is_pressed = true;
    while (bounces != 0U) {
      wave->begin[wave->count] = time;
      wave->is_pressed[wave->count] = is_pressed;
      wave->is_stable[wave->count] = false;
      ++wave->count;
      time += rng_range(MAX_POLL_GAP + 1U, BUTTONS_DEBOUNCE_MS - 1U);
      is_pressed = !is_pressed;
      --bounces;
    }
    wave->begin[wave->count] = time;
    wave->is_pressed[wave->count] = true;
    wave->is_stable[wave->count] = true;
    ++wave->count;
    time += rng_range(60U, 300U);
    bounces = 2U * rng_range(0U, 3U);
    is_pressed = false;
    while (bounces != 0U) {
      wave->begin[wave->count] = time;
      wave->is_pressed[wave->count] = is_pressed;
      wave->is_stable[wave->count] = false;
      ++wave->count;
      time += rng_range(MAX_POLL_GAP + 1U, BUTTONS_DEBOUNCE_MS - 1U);
      is_pressed = !is_pressed;
      --bounces;
    }
    wave->begin[wave->count] = time;
    wave->is_pressed[wave->count] = false;
    wave->is_stable[wave->count] = true;
    ++wave->count;
    time += rng_range(60U, 300U);
  }
  wave->end = time;
}

/*
 * MENU and ENTER: every stable low period gives exactly one press event, inside that period and no later than
 * debounce + 2 poll gaps after its start. Bounce runs give nothing. Polls come at random gaps of 1..MAX_POLL_GAP ms,
 * also across the tick wrap.
 */
static void test_random_bounce_irregular_polls(void) {
  static const uint8_t bits[2] = {BUTTON_BIT_MENU, BUTTON_BIT_ENTER};
  static const button_event_t codes[2] = {BUTTON_EVENT_MENU, BUTTON_EVENT_ENTER};
  const uint32_t max_gap = MAX_POLL_GAP;
  waveform_t wave;
  unsigned trial;

  rng_state = 12345U;
  for (trial = 0U; trial < 400U; ++trial) {
    const size_t which = trial & 1U;
    const uint32_t base = (trial % 3U == 0U) ? 64000U : 100U;
    size_t presses = rng_range(1U, 8U);
    size_t segment;
    size_t next_event = 0U;
    uint32_t time = 0U;

    make_waveform(&wave, presses);
    start_released((uint16_t)base);
    while (time < wave.end) {
      fake_button_pins_set(level_at(&wave, time) ? bits[which] : 0U);
      (void)pass_at(base + time, 4U);
      time += rng_range(1U, max_gap);
    }
    TEST_ASSERT_EQUAL(presses, event_count);
    for (segment = 0U; segment < wave.count; ++segment) {
      if (wave.is_stable[segment] && wave.is_pressed[segment] && (next_event < event_count)) {
        const uint32_t begin = base + wave.begin[segment];
        TEST_ASSERT_EQUAL(codes[which], event_log[next_event].event);
        TEST_ASSERT(event_log[next_event].time >= begin + BUTTONS_DEBOUNCE_MS);
        TEST_ASSERT(event_log[next_event].time <= begin + BUTTONS_DEBOUNCE_MS + 2U * max_gap);
        ++next_event;
      }
    }
  }
}

/*
 * NEXT held for a long time with slow main-loop passes (1..300 ms apart, at most 4 events each, as main.c does).
 * After a final drain at time t, the repeat count is the number of due times press + 400 + 80 k <= t (UX-16:
 * no lost step). The backlog never grows past what a 300 ms pass produces.
 */
static void test_repeat_catch_up_with_slow_passes(void) {
  unsigned trial;
  rng_state = 777U;
  for (trial = 0U; trial < 200U; ++trial) {
    const uint32_t base = (trial & 1U) ? 65000U : 10U;
    const uint32_t hold_time = rng_range(500U, 6000U);
    uint32_t time = 0U;
    uint32_t press_time = 0U;
    size_t repeats = 0U;
    size_t i;
    uint32_t due;

    start_released((uint16_t)base);
    fake_button_pins_set(BUTTON_BIT_NEXT);
    /* 1 ms passes until the press event, so its time is exact. */
    while (event_count == 0U) {
      (void)pass_at(base + time, 4U);
      ++time;
    }
    press_time = event_log[0].time - base;
    TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, event_log[0].event);
    while (time < hold_time) {
      (void)pass_at(base + time, 4U);
      time += rng_range(1U, 300U);
    }
    /* Final drain with the button still held. */
    while (pass_at(base + time, 4U) != 0U) {
    }
    for (i = 1U; i < event_count; ++i) {
      TEST_ASSERT_EQUAL(REPEAT(BUTTON_EVENT_NEXT), event_log[i].event);
      ++repeats;
    }
    due = 0U;
    if (time >= press_time + BUTTONS_REPEAT_DELAY_MS) {
      due = 1U + (time - press_time - BUTTONS_REPEAT_DELAY_MS) / BUTTONS_REPEAT_PERIOD_MS;
    }
    TEST_ASSERT_EQUAL(due, repeats);
  }
}

/* A bounce of PREV shorter than the debounce time during a NEXT repeat changes nothing. */
static void test_prev_bounce_during_next_repeat(void) {
  uint32_t time;
  size_t i;
  size_t repeats = 0U;
  start_released(0U);
  for (time = 0U; time < 1500U; ++time) {
    uint8_t mask = BUTTON_BIT_NEXT;
    if (((time >= 700U) && (time < 715U)) || ((time >= 720U) && (time < 735U))) {
      mask |= BUTTON_BIT_PREV;
    }
    fake_button_pins_set(mask);
    (void)pass_at(time, 4U);
  }
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, event_log[0].event);
  for (i = 1U; i < event_count; ++i) {
    TEST_ASSERT_EQUAL(REPEAT(BUTTON_EVENT_NEXT), event_log[i].event);
    TEST_ASSERT_EQUAL(event_log[0].time + BUTTONS_REPEAT_DELAY_MS + (i - 1U) * BUTTONS_REPEAT_PERIOD_MS,
                      event_log[i].time);
    ++repeats;
  }
  TEST_ASSERT_EQUAL(1U + (1499U - event_log[0].time - BUTTONS_REPEAT_DELAY_MS) / BUTTONS_REPEAT_PERIOD_MS, repeats);
}

/* A press that starts just before the tick wraps comes out once, 20 ms later. */
static void test_press_debounce_across_tick_wrap(void) {
  uint32_t time;
  start_released(65500U);
  for (time = 65520U; time < 65536U + 100U; ++time) {
    fake_button_pins_set((time >= 65530U) ? BUTTON_BIT_MENU : 0U);
    (void)pass_at(time, 4U);
  }
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
  TEST_ASSERT_EQUAL(65530U + BUTTONS_DEBOUNCE_MS, event_log[0].time);
}

/* Recovery window with a tick that moves @p step ms per read and MENU low for @p low_samples samples. */
static bool run_window(uint16_t step, size_t low_samples, uint16_t start_time) {
  static uint8_t sequence[200];
  memset(sequence, BUTTON_BIT_MENU, sizeof(sequence));
  if (low_samples < sizeof(sequence)) {
    memset(&sequence[low_samples], 0, sizeof(sequence) - low_samples);
  }
  fake_tick_reset(start_time);
  fake_tick_set_advance_per_read(step);
  fake_button_pins_reset();
  fake_button_pins_set_sequence(sequence, sizeof(sequence));
  fake_delay_reset();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  return buttons_is_recovery_start();
}

/*
 * The window counts time, not samples: with a late tick (2 or 3 ms per read) the decision still comes at the
 * first sample at 50 ms or later. Sample k is at k * step ms after the first sample.
 */
static void test_recovery_window_with_late_tick(void) {
  uint16_t step;
  for (step = 1U; step <= 3U; ++step) {
    const size_t last_sample = (BUTTONS_RECOVERY_HOLD_MS + step - 1U) / step; /* first sample at >= 50 ms */
    TEST_ASSERT(run_window(step, 200U, 0U));
    TEST_ASSERT_EQUAL(last_sample + 1U, fake_button_pins_read_count());
    TEST_ASSERT(run_window(step, last_sample + 1U, 65530U));
    /* MENU high at the decision sample: normal start. */
    TEST_ASSERT(!run_window(step, last_sample, 0U));
    TEST_ASSERT_EQUAL(last_sample + 1U, fake_button_pins_read_count());
  }
}

/* After a recovery start, the held MENU gives no event; release and a new press give one. */
static void test_recovery_menu_needs_new_press(void) {
  uint32_t time;
  TEST_ASSERT(run_window(1U, 200U, 100U));
  event_count = 0U;
  for (time = 200U; time < 3000U; ++time) {
    fake_button_pins_set(BUTTON_BIT_MENU);
    (void)pass_at(time, 4U);
  }
  TEST_ASSERT_EQUAL(0U, event_count);
  for (; time < 3100U; ++time) {
    fake_button_pins_set(0U);
    (void)pass_at(time, 4U);
  }
  TEST_ASSERT_EQUAL(0U, event_count);
  for (; time < 3200U; ++time) {
    fake_button_pins_set(BUTTON_BIT_MENU);
    (void)pass_at(time, 4U);
  }
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
}

int main(void) {
  TEST_RUN(test_random_bounce_irregular_polls);
  TEST_RUN(test_repeat_catch_up_with_slow_passes);
  TEST_RUN(test_prev_bounce_during_next_repeat);
  TEST_RUN(test_press_debounce_across_tick_wrap);
  TEST_RUN(test_recovery_window_with_late_tick);
  TEST_RUN(test_recovery_menu_needs_new_press);
  return TEST_FINISH();
}
