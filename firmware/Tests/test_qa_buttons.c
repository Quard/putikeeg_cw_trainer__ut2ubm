/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests for the buttons module: random bounce trains with irregular poll intervals, repeat catch-up with slow
 * main-loop passes, bounce of a second button during a repeat, the recovery window with a late tick, and long
 * presses with slow passes, release bounce, the tick wrap and the P3.2 ownership. Tap mode of all four buttons
 * with the long enter (0x44) and the drop rules of buttons_set_enter_enabled() and buttons_set_tap_mode().
 * The oracles come from menu_subsystem_spec.md section 4.5, keyer_spec.md section 6.12, ui-ux.md sections 6.2
 * and 6.6, and training_spec.md D-T7, D-T22, D-T28 and section 5.9, not from buttons.c.
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
#define LONG(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))
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
 * no lost step). The backlog never grows past what a 300 ms pass produces. The hold also gives exactly one long
 * event when t >= press + 800: the first event of the first pass at or after that time (long before repeat).
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
    size_t longs = 0U;
    size_t i;
    uint32_t due;
    uint32_t first_pass_after_long = 0xFFFFFFFFU;
    bool is_long_first_of_pass = false;

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
      const size_t before = event_count;
      (void)pass_at(base + time, 4U);
      if ((first_pass_after_long == 0xFFFFFFFFU) && (time >= press_time + BUTTONS_LONG_PRESS_MS)) {
        first_pass_after_long = base + time;
        is_long_first_of_pass = (event_count > before) && (event_log[before].event == LONG(BUTTON_EVENT_NEXT));
      }
      time += rng_range(1U, 300U);
    }
    /* Final drain with the button still held. */
    {
      const size_t before = event_count;
      while (pass_at(base + time, 4U) != 0U) {
      }
      if ((first_pass_after_long == 0xFFFFFFFFU) && (time >= press_time + BUTTONS_LONG_PRESS_MS)) {
        first_pass_after_long = base + time;
        is_long_first_of_pass = (event_count > before) && (event_log[before].event == LONG(BUTTON_EVENT_NEXT));
      }
    }
    for (i = 1U; i < event_count; ++i) {
      if (event_log[i].event == LONG(BUTTON_EVENT_NEXT)) {
        ++longs;
        TEST_ASSERT_EQUAL(first_pass_after_long, event_log[i].time);
        continue;
      }
      TEST_ASSERT_EQUAL(REPEAT(BUTTON_EVENT_NEXT), event_log[i].event);
      ++repeats;
    }
    due = 0U;
    if (time >= press_time + BUTTONS_REPEAT_DELAY_MS) {
      due = 1U + (time - press_time - BUTTONS_REPEAT_DELAY_MS) / BUTTONS_REPEAT_PERIOD_MS;
    }
    TEST_ASSERT_EQUAL(due, repeats);
    TEST_ASSERT_EQUAL((time >= press_time + BUTTONS_LONG_PRESS_MS) ? 1U : 0U, longs);
    if (longs != 0U) {
      TEST_ASSERT(is_long_first_of_pass);
    }
  }
}

/*
 * A bounce of PREV shorter than the debounce time during a NEXT repeat changes nothing: no PREV press and no PREV
 * long event. NEXT gives its one long event at press + 800, before the repeat due in the same poll.
 */
static void test_prev_bounce_during_next_repeat(void) {
  uint32_t time;
  size_t i;
  size_t repeats = 0U;
  size_t longs = 0U;
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
    if (event_log[i].event == LONG(BUTTON_EVENT_NEXT)) {
      ++longs;
      TEST_ASSERT_EQUAL(event_log[0].time + BUTTONS_LONG_PRESS_MS, event_log[i].time);
      /* The repeat due at 800 ms (400 + 5 x 80) comes right after the long event. */
      TEST_ASSERT(i + 1U < event_count);
      TEST_ASSERT_EQUAL(REPEAT(BUTTON_EVENT_NEXT), event_log[i + 1U].event);
      continue;
    }
    TEST_ASSERT_EQUAL(REPEAT(BUTTON_EVENT_NEXT), event_log[i].event);
    TEST_ASSERT_EQUAL(event_log[0].time + BUTTONS_REPEAT_DELAY_MS + repeats * BUTTONS_REPEAT_PERIOD_MS,
                      event_log[i].time);
    ++repeats;
  }
  TEST_ASSERT_EQUAL(1U, longs);
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

/*
 * Long press against an independent poll-level model, with clean edges and random poll gaps of 1..max_gap ms.
 * Model: the first poll that sees a pin change starts the debounce; the press event is the first poll at least
 * BUTTONS_DEBOUNCE_MS later (P); the release is detected the same way (R). The long event comes at the first poll
 * L >= P + BUTTONS_LONG_PRESS_MS, and only when L < R (the poll that detects the release clears the pending bit
 * before the long check). Randomised hold lengths cluster around the threshold.
 */
static void test_long_press_model_random_polls(void) {
  static const uint8_t bits[3] = {BUTTON_BIT_MENU, BUTTON_BIT_PREV, BUTTON_BIT_NEXT};
  static const button_event_t codes[3] = {BUTTON_EVENT_MENU, BUTTON_EVENT_PREV, BUTTON_EVENT_NEXT};
  static uint32_t polls[8192];
  unsigned trial;
  size_t long_trials = 0U;
  size_t short_trials = 0U;

  rng_state = 31337U;
  for (trial = 0U; trial < 3000U; ++trial) {
    const size_t which = trial % 3U;
    const uint32_t max_gap = (trial % 5U == 0U) ? 1U : rng_range(1U, 40U);
    const uint32_t base = (trial % 4U == 0U) ? 64500U : 500U;
    const uint32_t raw_press = rng_range(30U, 80U);
    const uint32_t raw_release = raw_press + rng_range(BUTTONS_LONG_PRESS_MS - 60U, BUTTONS_LONG_PRESS_MS + 60U);
    const uint32_t end = raw_release + 200U;
    size_t poll_count = 0U;
    size_t i;
    uint32_t time = 0U;
    uint32_t c1 = 0xFFFFFFFFU;
    uint32_t press_event = 0xFFFFFFFFU;
    uint32_t c2 = 0xFFFFFFFFU;
    uint32_t release_event = 0xFFFFFFFFU;
    uint32_t long_event = 0xFFFFFFFFU;
    size_t longs = 0U;
    size_t presses = 0U;

    while (time < end) {
      polls[poll_count++] = time;
      time += rng_range(1U, max_gap);
    }
    for (i = 0U; i < poll_count; ++i) {
      const uint32_t t = polls[i];
      if ((c1 == 0xFFFFFFFFU) && (t >= raw_press)) {
        c1 = t;
      }
      if ((c1 != 0xFFFFFFFFU) && (press_event == 0xFFFFFFFFU) && (t >= c1 + BUTTONS_DEBOUNCE_MS) && (t < raw_release)) {
        press_event = t;
      }
      if ((press_event != 0xFFFFFFFFU) && (c2 == 0xFFFFFFFFU) && (t >= raw_release)) {
        c2 = t;
      }
      if ((c2 != 0xFFFFFFFFU) && (release_event == 0xFFFFFFFFU) && (t >= c2 + BUTTONS_DEBOUNCE_MS)) {
        release_event = t;
      }
      if ((press_event != 0xFFFFFFFFU) && (long_event == 0xFFFFFFFFU) && (t >= press_event + BUTTONS_LONG_PRESS_MS) &&
          ((release_event == 0xFFFFFFFFU) || (t < release_event))) {
        long_event = t;
      }
    }

    start_released((uint16_t)base);
    for (i = 0U; i < poll_count; ++i) {
      const uint32_t t = polls[i];
      fake_button_pins_set(((t >= raw_press) && (t < raw_release)) ? bits[which] : 0U);
      (void)pass_at(base + t, 4U);
    }
    for (i = 0U; i < event_count; ++i) {
      if (event_log[i].event == codes[which]) {
        ++presses;
        TEST_ASSERT_EQUAL(base + press_event, event_log[i].time);
      } else if (event_log[i].event == LONG(codes[which])) {
        ++longs;
        TEST_ASSERT_EQUAL(base + long_event, event_log[i].time);
      } else {
        TEST_ASSERT_EQUAL(REPEAT(codes[which]), event_log[i].event);
        TEST_ASSERT(which != 0U);
      }
    }
    TEST_ASSERT_EQUAL(1U, presses);
    TEST_ASSERT_EQUAL((long_event != 0xFFFFFFFFU) ? 1U : 0U, longs);
    if (long_event != 0xFFFFFFFFU) {
      ++long_trials;
    } else {
      ++short_trials;
    }
  }
  /* Both outcomes must occur, or the model check proves little. */
  printf("long-press model: %u long, %u short\n", (unsigned)long_trials, (unsigned)short_trials);
  TEST_ASSERT(long_trials >= 500U);
  TEST_ASSERT(short_trials >= 500U);
}

/*
 * AC-K47: release bounce trains (each run shorter than the debounce) during the hold neither cancel nor restart
 * the hold. The long event still comes exactly at press + BUTTONS_LONG_PRESS_MS, once, and no second press event
 * comes. 1 ms polls.
 */
static void test_long_press_release_bounce(void) {
  static const uint8_t bits[3] = {BUTTON_BIT_MENU, BUTTON_BIT_PREV, BUTTON_BIT_NEXT};
  static const button_event_t codes[3] = {BUTTON_EVENT_MENU, BUTTON_EVENT_PREV, BUTTON_EVENT_NEXT};
  unsigned trial;

  rng_state = 2024U;
  for (trial = 0U; trial < 600U; ++trial) {
    const size_t which = trial % 3U;
    static bool level[3000];
    uint32_t time;
    uint32_t t;
    size_t i;
    size_t longs = 0U;
    size_t presses = 0U;
    uint32_t press_time = 0U;

    /* Pressed from 10 to 2000; bounce trains of released runs (1..19 ms) with pressed runs in between. */
    for (t = 0U; t < 3000U; ++t) {
      level[t] = (t >= 10U) && (t < 2000U);
    }
    t = 40U;
    while (t < 1950U) {
      const uint32_t run = rng_range(1U, BUTTONS_DEBOUNCE_MS - 1U);
      uint32_t k;
      for (k = 0U; (k < run) && (t + k < 1950U); ++k) {
        level[t + k] = false;
      }
      t += run + rng_range(1U, 120U);
    }
    start_released(1000U);
    for (time = 0U; time < 3000U; ++time) {
      fake_button_pins_set(level[time] ? bits[which] : 0U);
      (void)pass_at(1000U + time, 4U);
    }
    for (i = 0U; i < event_count; ++i) {
      if (event_log[i].event == codes[which]) {
        ++presses;
        press_time = event_log[i].time;
      } else if (event_log[i].event == LONG(codes[which])) {
        ++longs;
        TEST_ASSERT_EQUAL(press_time + BUTTONS_LONG_PRESS_MS, event_log[i].time);
      }
    }
    TEST_ASSERT_EQUAL(1U, presses);
    TEST_ASSERT_EQUAL(1030U, press_time);
    TEST_ASSERT_EQUAL(1U, longs);
  }
}

/* Simultaneous holds: long events of buttons due in the same poll come out MENU, PREV, NEXT, each once. */
static void test_long_press_order_same_poll(void) {
  uint32_t time;
  size_t longs = 0U;
  size_t i;
  start_released(65000U);
  for (time = 0U; time < 1500U; ++time) {
    fake_button_pins_set(
        ((time >= 10U) && (time < 1400U)) ? (uint8_t)(BUTTON_BIT_MENU | BUTTON_BIT_PREV | BUTTON_BIT_NEXT) : 0U);
    (void)pass_at(65000U + time, 8U);
  }
  /* Presses at 30 (MENU, PREV, NEXT), then at 830 the three long events, then the PREV repeat due at 830. */
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_PREV, event_log[1].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, event_log[2].event);
  for (i = 0U; i < event_count; ++i) {
    if ((event_log[i].event & BUTTON_EVENT_LONG_FLAG) != 0U) {
      TEST_ASSERT_EQUAL(65000U + 30U + BUTTONS_LONG_PRESS_MS, event_log[i].time);
      ++longs;
    }
  }
  TEST_ASSERT_EQUAL(3U, longs);
  for (i = 0U; i < event_count; ++i) {
    if (event_log[i].event == LONG(BUTTON_EVENT_MENU)) {
      TEST_ASSERT_EQUAL(LONG(BUTTON_EVENT_PREV), event_log[i + 1U].event);
      TEST_ASSERT_EQUAL(LONG(BUTTON_EVENT_NEXT), event_log[i + 2U].event);
      /* D-K3: PREV and NEXT presses that debounce in the same poll give the repeat to PREV. */
      TEST_ASSERT_EQUAL(REPEAT(BUTTON_EVENT_PREV), event_log[i + 3U].event);
      TEST_ASSERT_EQUAL(event_log[i].time, event_log[i + 3U].time);
    }
  }
}

/*
 * The P3.2 ownership never touches the long state of the other buttons: MENU held across disable/enable cycles
 * of ENTER (with P3.2 low and high) still gives its long event at press + 800 exactly. While disabled, P3.2 never
 * gives ENTER, and after enabling with P3.2 low no ENTER comes until a release and a new press.
 */
static void test_enter_ownership_keeps_long_state(void) {
  uint32_t time;
  size_t i;
  size_t enters = 0U;
  size_t longs = 0U;
  start_released(0U);
  for (time = 0U; time < 2500U; ++time) {
    uint8_t mask = 0U;
    if ((time >= 10U) && (time < 1500U)) {
      mask |= BUTTON_BIT_MENU;
    }
    /* P3.2 low 100..700 (keyer contact), and 1000..1300. */
    if (((time >= 100U) && (time < 700U)) || ((time >= 1000U) && (time < 1300U))) {
      mask |= BUTTON_BIT_ENTER;
    }
    if (time == 50U) {
      buttons_set_enter_enabled(false);
    }
    if (time == 400U) {
      buttons_set_enter_enabled(true); /* P3.2 low here: no ENTER for this closure */
    }
    if (time == 401U) {
      buttons_set_enter_enabled(false);
    }
    if (time == 650U) {
      buttons_set_enter_enabled(true); /* still low */
    }
    fake_button_pins_set(mask);
    (void)pass_at(time, 4U);
  }
  for (i = 0U; i < event_count; ++i) {
    if (event_log[i].event == BUTTON_EVENT_ENTER) {
      ++enters;
      /* Only the new closure at 1000 gives ENTER, 20 ms later. */
      TEST_ASSERT_EQUAL(1000U + BUTTONS_DEBOUNCE_MS, event_log[i].time);
    } else if (event_log[i].event == LONG(BUTTON_EVENT_MENU)) {
      ++longs;
      TEST_ASSERT_EQUAL(30U + BUTTONS_LONG_PRESS_MS, event_log[i].time);
    } else {
      TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[i].event);
    }
  }
  TEST_ASSERT_EQUAL(1U, enters);
  TEST_ASSERT_EQUAL(1U, longs);
}

/* A pending ENTER that the menu has not read yet is dropped by the disable (AC-K48). */
static void test_disable_drops_pending_enter(void) {
  uint32_t time;
  start_released(0U);
  for (time = 0U; time < 200U; ++time) {
    fake_button_pins_set((time >= 10U) && (time < 100U) ? (uint8_t)(BUTTON_BIT_ENTER | BUTTON_BIT_MENU) : 0U);
    if (time == 30U) {
      /* Debounce both, but read only one event: MENU comes first, ENTER stays pending. */
      (void)pass_at(time, 1U);
      buttons_set_enter_enabled(false);
      continue;
    }
    (void)pass_at(time, 4U);
  }
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
}

/*
 * D-K3 / AC-K46: PREV and NEXT presses that debounce in the same poll give every repeat to PREV, at the normal
 * times; NEXT never repeats until it is pressed again. Also across the tick wrap and with slow passes.
 */
static void test_same_poll_repeat_goes_to_prev(void) {
  static const uint32_t bases[3] = {0U, 65200U, 1000U};
  unsigned k;
  for (k = 0U; k < 3U; ++k) {
    uint32_t time;
    size_t i;
    size_t repeats = 0U;
    const uint32_t base = bases[k];
    const uint32_t gap = (k == 2U) ? 7U : 1U;
    uint32_t last_poll = 0U;
    start_released((uint16_t)base);
    for (time = 0U; time < 1500U; time += gap) {
      fake_button_pins_set((time >= 10U) ? (uint8_t)(BUTTON_BIT_PREV | BUTTON_BIT_NEXT) : 0U);
      (void)pass_at(base + time, 8U);
      last_poll = base + time;
    }
    TEST_ASSERT_EQUAL(BUTTON_EVENT_PREV, event_log[0].event);
    TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, event_log[1].event);
    TEST_ASSERT_EQUAL(event_log[0].time, event_log[1].time);
    for (i = 2U; i < event_count; ++i) {
      TEST_ASSERT(event_log[i].event != REPEAT(BUTTON_EVENT_NEXT));
      if (event_log[i].event == REPEAT(BUTTON_EVENT_PREV)) {
        ++repeats;
      }
    }
    TEST_ASSERT_EQUAL(1U + (last_poll - event_log[0].time - BUTTONS_REPEAT_DELAY_MS) / BUTTONS_REPEAT_PERIOD_MS,
                      repeats);
  }
}

/*
 * D-K4 / AC-K45 exact boundary, 1 ms polls: the pin opens BUTTONS_LONG_PRESS_MS - BUTTONS_DEBOUNCE_MS (780) ms after
 * the press event P: no long event, because the debounced release lands on the poll at P + 800 and clears the hold
 * before the long check. The pin opens 1 ms later: one long event at exactly P + 800.
 */
static void test_long_press_exact_boundary(void) {
  static const uint8_t bits[3] = {BUTTON_BIT_MENU, BUTTON_BIT_PREV, BUTTON_BIT_NEXT};
  static const button_event_t codes[3] = {BUTTON_EVENT_MENU, BUTTON_EVENT_PREV, BUTTON_EVENT_NEXT};
  size_t which;
  uint32_t extra;
  for (which = 0U; which < 3U; ++which) {
    for (extra = 0U; extra <= 1U; ++extra) {
      const uint32_t open_after = BUTTONS_LONG_PRESS_MS - BUTTONS_DEBOUNCE_MS + extra;
      uint32_t press_event = 0xFFFFFFFFU;
      uint32_t time;
      size_t i;
      size_t longs = 0U;
      start_released(65400U);
      for (time = 65410U; time < 65410U + 2000U; ++time) {
        const bool is_low = (press_event == 0xFFFFFFFFU) || (time < press_event + open_after);
        fake_button_pins_set(is_low ? bits[which] : 0U);
        (void)pass_at(time, 4U);
        if ((press_event == 0xFFFFFFFFU) && (event_count != 0U)) {
          press_event = event_log[0].time;
          TEST_ASSERT_EQUAL(codes[which], event_log[0].event);
        }
      }
      TEST_ASSERT_EQUAL(65410U + BUTTONS_DEBOUNCE_MS, press_event);
      for (i = 0U; i < event_count; ++i) {
        if (event_log[i].event == LONG(codes[which])) {
          ++longs;
          TEST_ASSERT_EQUAL(press_event + BUTTONS_LONG_PRESS_MS, event_log[i].time);
        }
      }
      TEST_ASSERT_EQUAL(extra, longs);
    }
  }
}

/* ---------- Tap mode (training_spec.md D-T7, D-T22, D-T28, section 5.9; AC-T15) ---------- */

static const uint8_t all_bits[4] = {BUTTON_BIT_MENU, BUTTON_BIT_PREV, BUTTON_BIT_NEXT, BUTTON_BIT_ENTER};
static const button_event_t all_codes[4] = {BUTTON_EVENT_MENU, BUTTON_EVENT_PREV, BUTTON_EVENT_NEXT,
                                            BUTTON_EVENT_ENTER};

static void start_tap_mode(uint16_t start_time) {
  start_released(start_time);
  buttons_set_tap_mode(true);
}

/* D-T28: the long enter is BUTTON_EVENT_ENTER | BUTTON_EVENT_LONG_FLAG, and BUTTON_EVENT_CODE() gives ENTER. */
static void test_long_enter_code(void) {
  TEST_ASSERT_EQUAL(0x44U, LONG(BUTTON_EVENT_ENTER));
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, BUTTON_EVENT_CODE(LONG(BUTTON_EVENT_ENTER)));
}

/*
 * Random poll model in tap mode for all four buttons, also across the tick wrap. The debounced press P and the
 * debounced release Q come from the poll times alone. Q - P < 800: one press event at Q, nothing else. Else: one
 * long event at the first poll at or after P + 800 (also the poll of Q, D-T22), and nothing at Q. No repeat.
 */
static void test_tap_model_random_polls(void) {
  static uint32_t polls[8192];
  unsigned trial;
  size_t long_trials = 0U;
  size_t tap_trials = 0U;

  rng_state = 4242U;
  for (trial = 0U; trial < 4000U; ++trial) {
    const size_t which = trial % 4U;
    const uint32_t max_gap = (trial % 5U == 0U) ? 1U : rng_range(1U, 40U);
    const uint32_t base = (trial % 3U == 0U) ? 64700U : 700U;
    const uint32_t raw_press = rng_range(30U, 80U);
    const uint32_t hold =
        (trial % 7U == 0U) ? rng_range(60U, 400U) : rng_range(BUTTONS_LONG_PRESS_MS - 60U, BUTTONS_LONG_PRESS_MS + 60U);
    const uint32_t raw_release = raw_press + hold;
    const uint32_t end = raw_release + 200U;
    size_t poll_count = 0U;
    size_t i;
    uint32_t time = 0U;
    uint32_t c1 = 0xFFFFFFFFU;
    uint32_t press = 0xFFFFFFFFU;
    uint32_t c2 = 0xFFFFFFFFU;
    uint32_t release = 0xFFFFFFFFU;
    uint32_t long_event = 0xFFFFFFFFU;
    bool is_long;

    while (time < end) {
      polls[poll_count++] = time;
      time += rng_range(1U, max_gap);
    }
    for (i = 0U; i < poll_count; ++i) {
      const uint32_t t = polls[i];
      if ((c1 == 0xFFFFFFFFU) && (t >= raw_press)) {
        c1 = t;
      }
      if ((c1 != 0xFFFFFFFFU) && (press == 0xFFFFFFFFU) && (t >= c1 + BUTTONS_DEBOUNCE_MS) && (t < raw_release)) {
        press = t;
      }
      if ((press != 0xFFFFFFFFU) && (c2 == 0xFFFFFFFFU) && (t >= raw_release)) {
        c2 = t;
      }
      if ((c2 != 0xFFFFFFFFU) && (release == 0xFFFFFFFFU) && (t >= c2 + BUTTONS_DEBOUNCE_MS)) {
        release = t;
      }
    }
    if (!TEST_ASSERT((press != 0xFFFFFFFFU) && (release != 0xFFFFFFFFU))) {
      return;
    }
    is_long = (release - press) >= BUTTONS_LONG_PRESS_MS;
    if (is_long) {
      for (i = 0U; i < poll_count; ++i) {
        if (polls[i] >= press + BUTTONS_LONG_PRESS_MS) {
          long_event = polls[i];
          break;
        }
      }
    }

    start_tap_mode((uint16_t)base);
    for (i = 0U; i < poll_count; ++i) {
      const uint32_t t = polls[i];
      fake_button_pins_set(((t >= raw_press) && (t < raw_release)) ? all_bits[which] : 0U);
      (void)pass_at(base + t, 4U);
    }
    if (!TEST_ASSERT_EQUAL(1U, event_count)) {
      fprintf(stderr, "trial %u: button %u, %u events\n", trial, (unsigned)which, (unsigned)event_count);
      continue;
    }
    if (is_long) {
      ++long_trials;
      TEST_ASSERT_EQUAL(LONG(all_codes[which]), event_log[0].event);
      TEST_ASSERT_EQUAL(base + long_event, event_log[0].time);
    } else {
      ++tap_trials;
      TEST_ASSERT_EQUAL(all_codes[which], event_log[0].event);
      TEST_ASSERT_EQUAL(base + release, event_log[0].time);
    }
  }
  printf("tap model: %u long, %u tap\n", (unsigned)long_trials, (unsigned)tap_trials);
  TEST_ASSERT(long_trials >= 500U);
  TEST_ASSERT(tap_trials >= 500U);
}

/*
 * Tap mode off: ENTER gives its event at the debounced press, never a long event and never a repeat, also for a
 * hold of 3 s and across the tick wrap (AC-K45 unchanged by D-T28).
 */
static void test_tap_off_enter_has_no_long(void) {
  static const uint32_t bases[2] = {500U, 65300U};
  size_t k;
  for (k = 0U; k < 2U; ++k) {
    uint32_t time;
    start_released((uint16_t)bases[k]);
    for (time = 0U; time < 4000U; ++time) {
      fake_button_pins_set(((time >= 10U) && (time < 3010U)) ? BUTTON_BIT_ENTER : 0U);
      (void)pass_at(bases[k] + time, 4U);
    }
    TEST_ASSERT_EQUAL(1U, event_count);
    TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[0].event);
    TEST_ASSERT_EQUAL((uint16_t)(bases[k] + 10U + BUTTONS_DEBOUNCE_MS), (uint16_t)event_log[0].time);
  }
}

/* Tap mode: release bounce (runs shorter than the debounce) during a hold gives no tap; one long at P + 800. */
static void test_tap_release_bounce(void) {
  unsigned trial;

  rng_state = 777U;
  for (trial = 0U; trial < 400U; ++trial) {
    const size_t which = trial % 4U;
    const bool is_short_hold = (trial % 2U) == 0U;
    const uint32_t raw_end = is_short_hold ? 500U : 2000U;
    static bool level[3000];
    uint32_t time;
    uint32_t t;

    for (t = 0U; t < 3000U; ++t) {
      level[t] = (t >= 10U) && (t < raw_end);
    }
    t = 40U;
    while (t < raw_end - 50U) {
      const uint32_t run = rng_range(1U, BUTTONS_DEBOUNCE_MS - 1U);
      uint32_t k;
      for (k = 0U; (k < run) && (t + k < raw_end - 50U); ++k) {
        level[t + k] = false;
      }
      t += run + rng_range(1U, 120U);
    }
    /* A release bounce train at the end of the hold. */
    level[raw_end + 3U] = true;
    level[raw_end + 4U] = true;
    level[raw_end + 11U] = true;
    start_tap_mode(1000U);
    for (time = 0U; time < 3000U; ++time) {
      fake_button_pins_set(level[time] ? all_bits[which] : 0U);
      (void)pass_at(1000U + time, 4U);
    }
    if (!TEST_ASSERT_EQUAL(1U, event_count)) {
      continue;
    }
    if (is_short_hold) {
      /* The last pin change is at raw_end + 12; the debounced release comes 20 ms later. */
      TEST_ASSERT_EQUAL(all_codes[which], event_log[0].event);
      TEST_ASSERT_EQUAL(1000U + raw_end + 12U + BUTTONS_DEBOUNCE_MS, event_log[0].time);
    } else {
      TEST_ASSERT_EQUAL(LONG(all_codes[which]), event_log[0].event);
      TEST_ASSERT_EQUAL(1000U + 30U + BUTTONS_LONG_PRESS_MS, event_log[0].time);
    }
  }
}

/* Tap mode: long events due in one poll come out MENU, ENTER, PREV, NEXT; taps released together likewise. */
static void test_tap_orders_with_enter(void) {
  static const button_event_t press_order[4] = {BUTTON_EVENT_MENU, BUTTON_EVENT_ENTER, BUTTON_EVENT_PREV,
                                                BUTTON_EVENT_NEXT};
  const uint8_t all = (uint8_t)(BUTTON_BIT_MENU | BUTTON_BIT_PREV | BUTTON_BIT_NEXT | BUTTON_BIT_ENTER);
  uint32_t time;
  size_t i;

  start_tap_mode(65000U);
  for (time = 0U; time < 1500U; ++time) {
    fake_button_pins_set(((time >= 10U) && (time < 1400U)) ? all : 0U);
    (void)pass_at(65000U + time, 8U);
  }
  TEST_ASSERT_EQUAL(4U, event_count);
  for (i = 0U; i < 4U; ++i) {
    TEST_ASSERT_EQUAL(LONG(press_order[i]), event_log[i].event);
    TEST_ASSERT_EQUAL(65000U + 30U + BUTTONS_LONG_PRESS_MS, event_log[i].time);
  }

  start_tap_mode(65000U);
  for (time = 0U; time < 500U; ++time) {
    fake_button_pins_set(((time >= 10U) && (time < 300U)) ? all : 0U);
    (void)pass_at(65000U + time, 8U);
  }
  TEST_ASSERT_EQUAL(4U, event_count);
  for (i = 0U; i < 4U; ++i) {
    TEST_ASSERT_EQUAL(press_order[i], event_log[i].event);
    TEST_ASSERT_EQUAL(65000U + 300U + BUTTONS_DEBOUNCE_MS, event_log[i].time);
  }
}

/*
 * D-T28: both values of buttons_set_enter_enabled() drop a long-pending ENTER in tap mode. A held contact then gives
 * no long event and no tap at its release. The next closure works normally. MENU, held over the same calls, keeps
 * its long event.
 */
static void test_tap_enter_enabled_drops_long_pending(void) {
  size_t variant;

  for (variant = 0U; variant < 3U; ++variant) {
    uint32_t time;
    size_t i;
    size_t enter_events = 0U;
    size_t menu_longs = 0U;

    start_tap_mode(2000U);
    for (time = 0U; time < 3000U; ++time) {
      uint8_t mask = 0U;
      /* ENTER closed 10..1500 and 1700..1800. MENU closed 5..1300 (long at 25 + 800). */
      if (((time >= 10U) && (time < 1500U)) || ((time >= 1700U) && (time < 1800U))) {
        mask |= BUTTON_BIT_ENTER;
      }
      if ((time >= 5U) && (time < 1300U)) {
        mask |= BUTTON_BIT_MENU;
      }
      if (time == 400U) {
        buttons_set_enter_enabled(variant == 0U); /* variant 0: true while closed */
      }
      if ((variant == 1U) && (time == 1000U)) {
        buttons_set_enter_enabled(true); /* still closed */
      }
      /* variant 2: false at 400 and true only after the release. */
      if ((variant == 2U) && (time == 1600U)) {
        buttons_set_enter_enabled(true);
      }
      fake_button_pins_set(mask);
      (void)pass_at(2000U + time, 4U);
    }
    for (i = 0U; i < event_count; ++i) {
      if (BUTTON_EVENT_CODE(event_log[i].event) == BUTTON_EVENT_ENTER) {
        ++enter_events;
        /* Only the closure at 1700 gives an event: a tap at its debounced release. */
        TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[i].event);
        TEST_ASSERT_EQUAL(2000U + 1800U + BUTTONS_DEBOUNCE_MS, event_log[i].time);
      } else if (event_log[i].event == LONG(BUTTON_EVENT_MENU)) {
        ++menu_longs;
        TEST_ASSERT_EQUAL(2000U + 25U + BUTTONS_LONG_PRESS_MS, event_log[i].time);
      } else {
        TEST_ASSERT(false);
      }
    }
    if (!TEST_ASSERT_EQUAL(1U, enter_events)) {
      fprintf(stderr, "variant %u\n", (unsigned)variant);
    }
    TEST_ASSERT_EQUAL(1U, menu_longs);
  }
}

/* ENTER alone: disabled at 400 and enabled at 1000 while still closed. Only the new closure at 1700 gives a tap. */
static void test_tap_enable_while_closed_then_new_closure(void) {
  uint32_t time;
  start_tap_mode(100U);
  for (time = 0U; time < 3000U; ++time) {
    const bool is_closed = ((time >= 10U) && (time < 1500U)) || ((time >= 1700U) && (time < 1800U));
    if (time == 400U) {
      buttons_set_enter_enabled(false);
    }
    if (time == 1000U) {
      buttons_set_enter_enabled(true);
    }
    fake_button_pins_set(is_closed ? BUTTON_BIT_ENTER : 0U);
    (void)pass_at(100U + time, 4U);
  }
  TEST_ASSERT_EQUAL(1U, event_count);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[0].event);
  TEST_ASSERT_EQUAL(100U + 1800U + BUTTONS_DEBOUNCE_MS, event_log[0].time);
}

/*
 * Section 5.9: buttons_set_tap_mode() drops the pending presses. A pending ENTER that the caller has not read yet
 * (MENU came first in the same poll) is dropped by the switch in both directions.
 */
static void test_tap_switch_drops_pending_enter(void) {
  size_t direction;

  for (direction = 0U; direction < 2U; ++direction) {
    const bool is_tap_before = (direction == 0U);
    uint32_t time;
    /* Tap on: the events come at the release (100 + 20). Tap off: at the press (10 + 20). */
    const uint32_t event_time = is_tap_before ? 120U : 30U;

    start_released(0U);
    buttons_set_tap_mode(is_tap_before);
    for (time = 0U; time < 1500U; ++time) {
      fake_button_pins_set(((time >= 10U) && (time < 100U)) ? (uint8_t)(BUTTON_BIT_ENTER | BUTTON_BIT_MENU) : 0U);
      if (time == event_time) {
        (void)pass_at(time, 1U);
        buttons_set_tap_mode(!is_tap_before);
        continue;
      }
      (void)pass_at(time, 4U);
    }
    TEST_ASSERT_EQUAL(1U, event_count);
    TEST_ASSERT_EQUAL(BUTTON_EVENT_MENU, event_log[0].event);
    TEST_ASSERT_EQUAL(event_time, event_log[0].time);
  }
}

/*
 * The enter that opens Receiving (ui-ux.md 6.8, V-125): tap off, ENTER gives its event at the press, tap mode goes on
 * while it is still closed. No long ENTER and no tap at the release, for holds below and above 800 ms. A release
 * bounce at the end of the hold changes nothing. Then a new closure gives its tap.
 */
static void test_tap_on_during_enter_hold(void) {
  static const uint32_t holds[3] = {300U, 900U, 2500U};
  size_t k;

  for (k = 0U; k < 3U; ++k) {
    const uint32_t release = 10U + holds[k];
    uint32_t time;
    start_released(65200U);
    for (time = 0U; time < 4000U; ++time) {
      bool is_closed = (time >= 10U) && (time < release);
      if ((time == release + 5U) || (time == release + 6U)) {
        is_closed = true; /* bounce shorter than the debounce */
      }
      if ((time >= 3500U) && (time < 3600U)) {
        is_closed = true;
      }
      fake_button_pins_set(is_closed ? BUTTON_BIT_ENTER : 0U);
      (void)pass_at(65200U + time, 4U);
      if (time == 30U) {
        buttons_set_tap_mode(true); /* receiving_enter() after the menu took the ENTER press */
      }
    }
    TEST_ASSERT_EQUAL(2U, event_count);
    TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[0].event);
    TEST_ASSERT_EQUAL(65200U + 30U, event_log[0].time);
    TEST_ASSERT_EQUAL(BUTTON_EVENT_ENTER, event_log[1].event);
    TEST_ASSERT_EQUAL(65200U + 3600U + BUTTONS_DEBOUNCE_MS, event_log[1].time);
  }
}

/* Tap mode never repeats: PREV and NEXT held 3 s give one long event each and no repeat; a tap after works. */
static void test_tap_no_repeat(void) {
  uint32_t time;
  start_tap_mode(10U);
  for (time = 0U; time < 4000U; ++time) {
    uint8_t mask = ((time >= 10U) && (time < 3010U)) ? (uint8_t)(BUTTON_BIT_PREV | BUTTON_BIT_NEXT) : 0U;
    if ((time >= 3500U) && (time < 3550U)) {
      mask = BUTTON_BIT_NEXT;
    }
    fake_button_pins_set(mask);
    (void)pass_at(10U + time, 4U);
  }
  TEST_ASSERT_EQUAL(3U, event_count);
  TEST_ASSERT_EQUAL(LONG(BUTTON_EVENT_PREV), event_log[0].event);
  TEST_ASSERT_EQUAL(LONG(BUTTON_EVENT_NEXT), event_log[1].event);
  TEST_ASSERT_EQUAL(BUTTON_EVENT_NEXT, event_log[2].event);
  TEST_ASSERT_EQUAL(10U + 3550U + BUTTONS_DEBOUNCE_MS, event_log[2].time);
}

int main(void) {
  TEST_RUN(test_random_bounce_irregular_polls);
  TEST_RUN(test_repeat_catch_up_with_slow_passes);
  TEST_RUN(test_prev_bounce_during_next_repeat);
  TEST_RUN(test_press_debounce_across_tick_wrap);
  TEST_RUN(test_recovery_window_with_late_tick);
  TEST_RUN(test_recovery_menu_needs_new_press);
  TEST_RUN(test_long_press_model_random_polls);
  TEST_RUN(test_long_press_release_bounce);
  TEST_RUN(test_long_press_exact_boundary);
  TEST_RUN(test_long_press_order_same_poll);
  TEST_RUN(test_same_poll_repeat_goes_to_prev);
  TEST_RUN(test_enter_ownership_keeps_long_state);
  TEST_RUN(test_disable_drops_pending_enter);
  TEST_RUN(test_long_enter_code);
  TEST_RUN(test_tap_model_random_polls);
  TEST_RUN(test_tap_off_enter_has_no_long);
  TEST_RUN(test_tap_release_bounce);
  TEST_RUN(test_tap_orders_with_enter);
  TEST_RUN(test_tap_enter_enabled_drops_long_pending);
  TEST_RUN(test_tap_enable_while_closed_then_new_closure);
  TEST_RUN(test_tap_switch_drops_pending_enter);
  TEST_RUN(test_tap_on_during_enter_hold);
  TEST_RUN(test_tap_no_repeat);
  return TEST_FINISH();
}
