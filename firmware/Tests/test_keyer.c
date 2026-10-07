/*
 * Host tests of the keyer: keyer.c and the ISR side in cw/keyer_service.h (references/internal/keyer_spec.md
 * section 11, AC-K1 to AC-K13 and AC-K15 to AC-K24). Each tick() is one Timer 0 call. The sidetone request macros
 * record into this file, so the tests see the exact call of each request.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "fake_iap.h"
#include "fake_paddle_pins.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"
#include "test_assert.h"

static bool test_is_tone_requested;
static void test_request_tone(bool is_on) { test_is_tone_requested = is_on; }

#define KEYER_SIDETONE_ON() test_request_tone(true)
#define KEYER_SIDETONE_OFF() test_request_tone(false)
#include "cw/keyer_service.h"

#define TRACE_SIZE 200000U
#define EVENT_LOG_SIZE 4096U

typedef struct {
  uint8_t phase;
  bool is_tone;
  bool is_logical_closed; /* a logical contact is closed after the call */
} trace_entry_t;

static trace_entry_t trace[TRACE_SIZE];
/* Calls since the last trace reset; call n (1-based) is trace[n - 1]. */
static size_t call_count;
static bool should_read_events;
static keyer_event_t event_log[EVENT_LOG_SIZE];
static size_t event_calls[EVENT_LOG_SIZE];
static size_t event_count;
/* AC-K19: calls in which the queue head moved by more than 1. */
static size_t head_step_violations;

static void reset_trace(void) {
  call_count = 0U;
  event_count = 0U;
}

static void log_event(keyer_event_t event) {
  if (event_count < EVENT_LOG_SIZE) {
    event_log[event_count] = event;
    event_calls[event_count] = call_count;
  }
  ++event_count;
}

static void tick(void) {
  const uint8_t head = keyer_queue_head;
  keyer_event_t event;

  keyer_service();
  if ((uint8_t)(keyer_queue_head - head) > 1U) {
    ++head_step_violations;
  }
  if (call_count < TRACE_SIZE) {
    trace[call_count].phase = keyer_phase;
    trace[call_count].is_tone = test_is_tone_requested;
    trace[call_count].is_logical_closed =
        (keyer_is_tip_closed && !keyer_is_tip_masked) || (keyer_is_ring_closed && !keyer_is_ring_masked);
  }
  ++call_count;
  if (should_read_events) {
    event = keyer_read_event();
    while (event != KEYER_EVENT_NONE) {
      log_event(event);
      event = keyer_read_event();
    }
  }
}

static void run(size_t calls) {
  size_t index;
  for (index = 0U; index < calls; ++index) {
    tick();
  }
}

static void set_contacts(bool is_tip_closed, bool is_ring_closed) {
  fake_paddle_pins_set_tip_closed(is_tip_closed);
  fake_paddle_pins_set_ring_closed(is_ring_closed);
}

/* Starts the keyer, runs the reseed call and resets the trace, so the next tick() is call 1. */
static void start(uint8_t key_type, bool is_swapped, uint8_t wpm, uint8_t eff_wpm) {
  should_read_events = true;
  test_is_tone_requested = true;
  keyer_start(key_type, is_swapped, wpm, eff_wpm);
  tick();
  reset_trace();
}

static void setup(void) {
  fake_paddle_pins_reset();
  keyer_init();
  set_contacts(false, false);
}

static uint16_t dit_of(uint8_t wpm) { return (uint16_t)((4800U + (wpm / 2U)) / wpm); }

static bool is_mark_phase(uint8_t phase) {
  return (phase == KEYER_PHASE_ELEMENT) || (phase == KEYER_PHASE_STRAIGHT_DOWN);
}

/* Runs of consecutive calls whose phase matches; start is a 1-based call number. */
typedef struct {
  size_t start;
  size_t length;
} run_t;

static size_t find_runs(bool (*matches)(uint8_t phase), run_t* runs, size_t max_runs) {
  size_t count = 0U;
  size_t index = 0U;
  const size_t limit = (call_count < TRACE_SIZE) ? call_count : TRACE_SIZE;

  while (index < limit) {
    if (matches(trace[index].phase)) {
      const size_t first = index;
      while ((index < limit) && matches(trace[index].phase)) {
        ++index;
      }
      if (count < max_runs) {
        runs[count].start = first + 1U;
        runs[count].length = index - first;
      }
      ++count;
    } else {
      ++index;
    }
  }
  return count;
}

static bool is_gap_phase(uint8_t phase) { return phase == KEYER_PHASE_GAP; }

static size_t count_events(keyer_event_t event) {
  size_t count = 0U;
  size_t index;
  for (index = 0U; (index < event_count) && (index < EVENT_LOG_SIZE); ++index) {
    if (event_log[index] == event) {
      ++count;
    }
  }
  return count;
}

/* Returns the element events (DIT, DAH) in order as a string of '.' and '-'. */
static void element_string(char* text, size_t size) {
  size_t length = 0U;
  size_t index;
  for (index = 0U; (index < event_count) && (index < EVENT_LOG_SIZE) && (length + 1U < size); ++index) {
    if (event_log[index] == KEYER_EVENT_DIT) {
      text[length++] = '.';
    } else if (event_log[index] == KEYER_EVENT_DAH) {
      text[length++] = '-';
    }
  }
  text[length] = '\0';
}

static bool elements_are(const char* expected) {
  char text[256];
  element_string(text, sizeof(text));
  if (strcmp(text, expected) != 0) {
    fprintf(stderr, "elements: expected \"%s\", got \"%s\"\n", expected, text);
    return false;
  }
  return true;
}

/* The example of keyer_spec.md section 6.4: 20 WPM, tip closed on calls 1-99. */
static void test_spec_example(void) {
  size_t call;
  size_t tone_on = 0U;
  size_t tone_off = 0U;

  setup();
  start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  set_contacts(true, false);
  run(99U);
  set_contacts(false, false);
  run(1500U);
  for (call = 1U; call <= 4U; ++call) {
    TEST_ASSERT_EQUAL(KEYER_PHASE_IDLE, trace[call - 1U].phase);
  }
  for (call = 5U; call <= 244U; ++call) {
    TEST_ASSERT_EQUAL(KEYER_PHASE_ELEMENT, trace[call - 1U].phase);
  }
  for (call = 245U; call <= 484U; ++call) {
    TEST_ASSERT_EQUAL(KEYER_PHASE_GAP, trace[call - 1U].phase);
  }
  TEST_ASSERT_EQUAL(KEYER_PHASE_IDLE, trace[485U - 1U].phase);
  for (call = 2U; call <= call_count; ++call) {
    if (trace[call - 1U].is_tone && !trace[call - 2U].is_tone) {
      tone_on = (tone_on == 0U) ? call : tone_on;
    } else if (!trace[call - 1U].is_tone && trace[call - 2U].is_tone) {
      tone_off = (tone_off == 0U) ? call : tone_off;
    }
  }
  TEST_ASSERT_EQUAL(5U, tone_on);
  TEST_ASSERT_EQUAL(213U, tone_off);
  TEST_ASSERT_EQUAL(3U, event_count);
  TEST_ASSERT_EQUAL(KEYER_EVENT_DIT, event_log[0]);
  TEST_ASSERT_EQUAL(245U, event_calls[0]);
  TEST_ASSERT_EQUAL(KEYER_EVENT_CHAR_GAP, event_log[1]);
  TEST_ASSERT_EQUAL(724U, event_calls[1]);
  TEST_ASSERT_EQUAL(KEYER_EVENT_WORD_GAP, event_log[2]);
  TEST_ASSERT_EQUAL(1444U, event_calls[2]);
}

/* AC-K1 */
static void test_filter_votes_on_fifth_sample(void) {
  size_t call;

  setup();
  start(KEYER_KEY_STRAIGHT, false, 20U, 20U);
  set_contacts(true, false);
  for (call = 1U; call <= 4U; ++call) {
    tick();
    TEST_ASSERT(!keyer_is_tip_closed);
  }
  tick();
  TEST_ASSERT(keyer_is_tip_closed);
  TEST_ASSERT(!keyer_is_ring_closed);
  run(20U);
  set_contacts(false, true);
  for (call = 1U; call <= 4U; ++call) {
    tick();
    TEST_ASSERT(keyer_is_tip_closed);
    TEST_ASSERT(!keyer_is_ring_closed);
  }
  tick();
  TEST_ASSERT(!keyer_is_tip_closed);
  TEST_ASSERT(keyer_is_ring_closed);
  run(20U);
  set_contacts(false, false);
  run(4U);
  TEST_ASSERT(keyer_is_ring_closed);
  tick();
  TEST_ASSERT(!keyer_is_ring_closed);
}

/* AC-K2: from settled @p is_settled_closed, @p count opposite samples, then back. The vote never changes. */
static bool glitch_keeps_state(bool is_settled_closed, size_t count, bool is_ring) {
  size_t index;
  bool is_kept = true;

  setup();
  start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  set_contacts(is_ring ? false : is_settled_closed, is_ring ? is_settled_closed : false);
  run(20U);
  for (index = 0U; index < count; ++index) {
    set_contacts(is_ring ? false : !is_settled_closed, is_ring ? !is_settled_closed : false);
    tick();
    is_kept = is_kept && ((is_ring ? keyer_is_ring_closed : keyer_is_tip_closed) == is_settled_closed);
  }
  set_contacts(is_ring ? false : is_settled_closed, is_ring ? is_settled_closed : false);
  for (index = 0U; index < 20U; ++index) {
    tick();
    is_kept = is_kept && ((is_ring ? keyer_is_ring_closed : keyer_is_tip_closed) == is_settled_closed);
  }
  return is_kept;
}

static bool alternating_keeps_state(bool is_settled_closed) {
  size_t index;
  bool is_kept = true;

  setup();
  start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  set_contacts(is_settled_closed, false);
  run(20U);
  for (index = 0U; index < 200U; ++index) {
    set_contacts((index % 2U) == 0U ? !is_settled_closed : is_settled_closed, false);
    tick();
    is_kept = is_kept && (keyer_is_tip_closed == is_settled_closed);
  }
  return is_kept;
}

static void test_filter_ignores_glitches(void) {
  size_t count;
  for (count = 1U; count <= 3U; ++count) {
    TEST_ASSERT(glitch_keeps_state(false, count, false));
    TEST_ASSERT(glitch_keeps_state(true, count, false));
    TEST_ASSERT(glitch_keeps_state(false, count, true));
    TEST_ASSERT(glitch_keeps_state(true, count, true));
  }
  TEST_ASSERT(alternating_keeps_state(false));
  TEST_ASSERT(alternating_keeps_state(true));
}

/* AC-K3: a contact closed at the start is masked until it votes open, for every key type and both contacts. */
static void entry_mask_case(uint8_t key_type, bool is_ring) {
  size_t index;
  bool is_quiet = true;
  run_t runs[4] = {{0U, 0U}};

  setup();
  set_contacts(!is_ring, is_ring);
  start(key_type, false, 20U, 20U);
  TEST_ASSERT(!test_is_tone_requested);
  run(8000U);
  for (index = 0U; index < call_count; ++index) {
    is_quiet = is_quiet && (trace[index].phase == KEYER_PHASE_IDLE) && !trace[index].is_tone;
  }
  TEST_ASSERT(is_quiet);
  TEST_ASSERT_EQUAL(0U, event_count);
  TEST_ASSERT(is_ring ? keyer_is_ring_masked : keyer_is_tip_masked);

  set_contacts(false, false);
  run(20U);
  TEST_ASSERT(!keyer_is_tip_masked);
  TEST_ASSERT(!keyer_is_ring_masked);
  TEST_ASSERT_EQUAL(0U, event_count);
  TEST_ASSERT_EQUAL(KEYER_PHASE_IDLE, keyer_phase);

  reset_trace();
  set_contacts(!is_ring, is_ring);
  run(30U);
  set_contacts(false, false);
  run(4000U);
  TEST_ASSERT_EQUAL(1U, find_runs(is_mark_phase, runs, 4U));
  TEST_ASSERT_EQUAL(5U, runs[0].start);
  TEST_ASSERT_EQUAL(1U, count_events(KEYER_EVENT_DIT) + count_events(KEYER_EVENT_DAH));
}

static void test_entry_mask(void) {
  entry_mask_case(KEYER_KEY_STRAIGHT, false);
  entry_mask_case(KEYER_KEY_STRAIGHT, true);
  entry_mask_case(KEYER_KEY_IAMBIC_A, false);
  entry_mask_case(KEYER_KEY_IAMBIC_A, true);
  entry_mask_case(KEYER_KEY_IAMBIC_B, false);
  entry_mask_case(KEYER_KEY_IAMBIC_B, true);
}

/* AC-K4 */
static void test_mono_plug_keys_straight_on_tip(void) {
  run_t runs[4] = {{0U, 0U}};

  setup();
  set_contacts(false, true);
  start(KEYER_KEY_STRAIGHT, false, 20U, 20U);
  run(100U);
  set_contacts(true, true);
  run(300U);
  set_contacts(false, true);
  run(100U);
  set_contacts(true, true);
  run(700U);
  set_contacts(false, true);
  run(3000U);
  TEST_ASSERT(keyer_is_ring_masked);
  TEST_ASSERT_EQUAL(2U, find_runs(is_mark_phase, runs, 4U));
  TEST_ASSERT_EQUAL(300U, runs[0].length);
  TEST_ASSERT_EQUAL(700U, runs[1].length);
  TEST_ASSERT(elements_are(".-"));
}

/* AC-K5: the same contact sequence with Normal and Reversed. */
static void swap_case(uint8_t key_type, bool is_swapped, char* elements, size_t size) {
  setup();
  start(key_type, is_swapped, 20U, 20U);
  set_contacts(true, false);
  run(20U);
  set_contacts(false, false);
  run(1000U);
  set_contacts(false, true);
  run(20U);
  set_contacts(false, false);
  run(3000U);
  element_string(elements, size);
}

static void test_swap(void) {
  char normal[16];
  char reversed[16];

  swap_case(KEYER_KEY_IAMBIC_B, false, normal, sizeof(normal));
  swap_case(KEYER_KEY_IAMBIC_B, true, reversed, sizeof(reversed));
  TEST_ASSERT(strcmp(normal, ".-") == 0);
  TEST_ASSERT(strcmp(reversed, "-.") == 0);
  swap_case(KEYER_KEY_IAMBIC_A, false, normal, sizeof(normal));
  swap_case(KEYER_KEY_IAMBIC_A, true, reversed, sizeof(reversed));
  TEST_ASSERT(strcmp(normal, ".-") == 0);
  TEST_ASSERT(strcmp(reversed, "-.") == 0);
  swap_case(KEYER_KEY_STRAIGHT, false, normal, sizeof(normal));
  swap_case(KEYER_KEY_STRAIGHT, true, reversed, sizeof(reversed));
  TEST_ASSERT(strcmp(normal, reversed) == 0);
  TEST_ASSERT(strcmp(normal, "..") == 0);
}

/* AC-K6, AC-K7: a straight mark of @p calls with the given contacts. Returns the element string. */
static void straight_mark(bool is_tip, bool is_ring, size_t calls, char* elements, size_t size) {
  size_t index;
  size_t logical_calls = 0U;
  size_t down_calls = 0U;
  bool is_tone_exact = true;

  setup();
  start(KEYER_KEY_STRAIGHT, false, 20U, 20U);
  set_contacts(is_tip, is_ring);
  run(calls);
  set_contacts(false, false);
  run(2000U);
  for (index = 0U; index < call_count; ++index) {
    logical_calls += trace[index].is_logical_closed ? 1U : 0U;
    down_calls += (trace[index].phase == KEYER_PHASE_STRAIGHT_DOWN) ? 1U : 0U;
    is_tone_exact = is_tone_exact && (trace[index].is_tone == (trace[index].phase == KEYER_PHASE_STRAIGHT_DOWN));
  }
  TEST_ASSERT_EQUAL(calls, down_calls);
  TEST_ASSERT_EQUAL(logical_calls, down_calls);
  TEST_ASSERT(is_tone_exact);
  element_string(elements, size);
}

static void test_straight(void) {
  char elements[8];

  straight_mark(true, false, 300U, elements, sizeof(elements));
  TEST_ASSERT(strcmp(elements, ".") == 0);
  straight_mark(false, true, 300U, elements, sizeof(elements));
  TEST_ASSERT(strcmp(elements, ".") == 0);
  straight_mark(true, true, 300U, elements, sizeof(elements));
  TEST_ASSERT(strcmp(elements, ".") == 0);
  /* 20 WPM: 2 dit = 480 calls. */
  straight_mark(true, false, 479U, elements, sizeof(elements));
  TEST_ASSERT(strcmp(elements, ".") == 0);
  straight_mark(true, false, 480U, elements, sizeof(elements));
  TEST_ASSERT(strcmp(elements, "-") == 0);
}

static void test_straight_saturation(void) {
  setup();
  start(KEYER_KEY_STRAIGHT, false, 20U, 20U);
  set_contacts(true, false);
  run(70000U);
  TEST_ASSERT_EQUAL(0xFFFFU, keyer_ticks);
  TEST_ASSERT(test_is_tone_requested);
  set_contacts(false, false);
  run(5U);
  TEST_ASSERT(!test_is_tone_requested);
  TEST_ASSERT(elements_are("-"));
}

/* AC-K8, AC-K13: every ELEMENT run is a dit or a dah, every GAP run is one dit, and the early release is exact. */
static void check_element_timing(uint16_t dit, const char* expected) {
  run_t marks[64] = {{0U, 0U}};
  run_t gaps[64] = {{0U, 0U}};
  size_t mark_count;
  size_t gap_count;
  size_t index;
  size_t call;
  bool is_release_exact = true;

  mark_count = find_runs(is_mark_phase, marks, 64U);
  gap_count = find_runs(is_gap_phase, gaps, 64U);
  TEST_ASSERT_EQUAL(strlen(expected), mark_count);
  TEST_ASSERT_EQUAL(mark_count, gap_count);
  for (index = 0U; (index < mark_count) && (index < 64U) && (index < strlen(expected)); ++index) {
    TEST_ASSERT_EQUAL((expected[index] == '-') ? (3U * dit) : dit, marks[index].length);
    TEST_ASSERT_EQUAL(dit, gaps[index].length);
    TEST_ASSERT_EQUAL(marks[index].start + marks[index].length, gaps[index].start);
    for (call = marks[index].start; call < marks[index].start + marks[index].length; ++call) {
      const bool is_on_expected = call < (marks[index].start + marks[index].length - KEYER_EARLY_RELEASE_TICKS);
      is_release_exact = is_release_exact && (trace[call - 1U].is_tone == is_on_expected);
    }
    is_release_exact = is_release_exact && !trace[gaps[index].start - 1U].is_tone;
  }
  TEST_ASSERT(is_release_exact);
  TEST_ASSERT(elements_are(expected));
}

static void timing_case(uint8_t wpm) {
  const uint16_t dit = dit_of(wpm);

  /* Single dit and single dah. */
  setup();
  start(KEYER_KEY_IAMBIC_B, false, wpm, wpm);
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  run(3U * dit);
  check_element_timing(dit, ".");
  setup();
  start(KEYER_KEY_IAMBIC_B, false, wpm, wpm);
  set_contacts(false, true);
  run(10U);
  set_contacts(false, false);
  run(5U * dit);
  check_element_timing(dit, "-");

  /* Held repeat: logical close at call 5, release so that the 6th dit element is the last. */
  setup();
  start(KEYER_KEY_IAMBIC_A, false, wpm, wpm);
  set_contacts(true, false);
  run(4U + (10U * dit) + 10U);
  set_contacts(false, false);
  run(4U * dit);
  check_element_timing(dit, "......");
  setup();
  start(KEYER_KEY_IAMBIC_A, false, wpm, wpm);
  set_contacts(false, true);
  run(4U + (20U * dit) + 10U);
  set_contacts(false, false);
  run(6U * dit);
  check_element_timing(dit, "------");

  /* Held squeeze, released early in the 6th element (a dah); Iambic A then stops. */
  setup();
  start(KEYER_KEY_IAMBIC_A, false, wpm, wpm);
  set_contacts(true, true);
  run(4U + (14U * dit) + 10U);
  set_contacts(false, false);
  run(6U * dit);
  check_element_timing(dit, ".-.-.-");
}

static void test_exact_timing(void) {
  timing_case(5U);
  timing_case(20U);
  timing_case(50U);
  timing_case(13U);
}

/* AC-K9, AC-K16 */
static void test_dit_formula(void) {
  uint8_t wpm;

  setup();
  for (wpm = KEYER_WPM_MIN; wpm <= KEYER_WPM_MAX; ++wpm) {
    keyer_start(KEYER_KEY_IAMBIC_B, false, wpm, wpm);
    TEST_ASSERT_EQUAL(dit_of(wpm), keyer_dit_ticks);
    TEST_ASSERT_EQUAL(3U * dit_of(wpm), keyer_dah_ticks);
    TEST_ASSERT_EQUAL(2U * dit_of(wpm), keyer_two_dit_ticks);
    TEST_ASSERT_EQUAL(5U * dit_of(wpm), keyer_word_gap_ticks);
  }
  keyer_stop();
}

typedef struct {
  uint8_t wpm;
  uint8_t eff_wpm;
  uint16_t dit;
  uint16_t dah;
  uint16_t two_dit;
  uint16_t word_gap;
} timing_row_t;

static void test_farnsworth_table(void) {
  /* keyer_spec.md section 6.7. */
  static const timing_row_t rows[] = {
      {5U, 5U, 960U, 2880U, 1920U, 4800U}, {7U, 7U, 686U, 2058U, 1372U, 3430U}, {13U, 13U, 369U, 1107U, 738U, 1845U},
      {20U, 20U, 240U, 720U, 480U, 1200U}, {20U, 10U, 240U, 720U, 480U, 4360U}, {20U, 5U, 240U, 720U, 480U, 10675U},
      {25U, 15U, 192U, 576U, 384U, 2645U}, {32U, 32U, 150U, 450U, 300U, 750U},  {50U, 50U, 96U, 288U, 192U, 480U},
      {50U, 49U, 96U, 288U, 192U, 505U},   {50U, 5U, 96U, 288U, 192U, 11850U},
  };
  size_t index;

  setup();
  for (index = 0U; index < sizeof(rows) / sizeof(rows[0]); ++index) {
    keyer_start(KEYER_KEY_IAMBIC_B, false, rows[index].wpm, rows[index].eff_wpm);
    TEST_ASSERT_EQUAL(rows[index].dit, keyer_dit_ticks);
    TEST_ASSERT_EQUAL(rows[index].dah, keyer_dah_ticks);
    TEST_ASSERT_EQUAL(rows[index].two_dit, keyer_two_dit_ticks);
    TEST_ASSERT_EQUAL(rows[index].word_gap, keyer_word_gap_ticks);
  }
  keyer_stop();
}

/* AC-K10 */
static void iambic_decisions(uint8_t key_type) {
  const uint16_t dit = dit_of(20U);

  /* One dit, one dah. */
  setup();
  start(key_type, false, 20U, 20U);
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  run(4000U);
  TEST_ASSERT(elements_are("."));
  setup();
  start(key_type, false, 20U, 20U);
  set_contacts(false, true);
  run(10U);
  set_contacts(false, false);
  run(4000U);
  TEST_ASSERT(elements_are("-"));

  /* Held repeat of dits. */
  setup();
  start(key_type, false, 20U, 20U);
  set_contacts(true, false);
  run(4U + (6U * dit) + 10U);
  set_contacts(false, false);
  run(4000U);
  TEST_ASSERT(elements_are("...."));

  /* Held squeeze from idle: alternates and starts with a dit. */
  setup();
  start(key_type, false, 20U, 20U);
  set_contacts(true, true);
  run(4U + (16U * dit) + 10U);
  TEST_ASSERT(elements_are(".-.-."));

  /* Opposite tap during ELEMENT: the dit lever opens first, then the dah lever taps inside the dit. */
  setup();
  start(key_type, false, 20U, 20U);
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  run(40U);
  set_contacts(false, true);
  run(10U);
  set_contacts(false, false);
  run(4000U);
  TEST_ASSERT(elements_are(".-"));

  /* Opposite tap during GAP. */
  setup();
  start(key_type, false, 20U, 20U);
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  run(300U);
  TEST_ASSERT_EQUAL(KEYER_PHASE_GAP, keyer_phase);
  set_contacts(false, true);
  run(10U);
  set_contacts(false, false);
  run(4000U);
  TEST_ASSERT(elements_are(".-"));

  /* Same-lever tap during GAP repeats the element. */
  setup();
  start(key_type, false, 20U, 20U);
  set_contacts(false, true);
  run(10U);
  set_contacts(false, false);
  run((3U * dit) + 50U);
  TEST_ASSERT_EQUAL(KEYER_PHASE_GAP, keyer_phase);
  set_contacts(false, true);
  run(10U);
  set_contacts(false, false);
  run(4000U);
  TEST_ASSERT(elements_are("--"));
}

static void test_iambic_decisions(void) {
  iambic_decisions(KEYER_KEY_IAMBIC_A);
  iambic_decisions(KEYER_KEY_IAMBIC_B);
}

/* AC-K11: a squeeze from idle whose logical open comes on call @p open_call. */
static void squeeze_release_at(uint8_t key_type, uint8_t wpm, size_t open_call, char* elements, size_t size) {
  setup();
  start(key_type, false, wpm, wpm);
  set_contacts(true, true);
  /* The 5th open sample votes open: calls open_call - 4 .. open_call are open. */
  run(open_call - 5U);
  set_contacts(false, false);
  run(5U);
  TEST_ASSERT(!keyer_is_tip_closed && !keyer_is_ring_closed);
  TEST_ASSERT_EQUAL(open_call, call_count);
  run(8U * dit_of(wpm));
  element_string(elements, size);
}

static void squeeze_release(uint8_t key_type, size_t open_call, char* elements, size_t size) {
  squeeze_release_at(key_type, 20U, open_call, elements, size);
}

static void a_versus_b(size_t open_call, const char* expected_a) {
  char elements_a[32];
  char elements_b[32];
  char expected_b[32];
  const size_t length = strlen(expected_a);

  squeeze_release(KEYER_KEY_IAMBIC_A, open_call, elements_a, sizeof(elements_a));
  squeeze_release(KEYER_KEY_IAMBIC_B, open_call, elements_b, sizeof(elements_b));
  memcpy(expected_b, expected_a, length);
  expected_b[length] = (expected_a[length - 1U] == '-') ? '.' : '-';
  expected_b[length + 1U] = '\0';
  if (!TEST_ASSERT(strcmp(elements_a, expected_a) == 0) || !TEST_ASSERT(strcmp(elements_b, expected_b) == 0)) {
    fprintf(stderr, "open call %zu: A \"%s\" (expected \"%s\"), B \"%s\" (expected \"%s\")\n", open_call, elements_a,
            expected_a, elements_b, expected_b);
  }
}

static void test_iambic_a_versus_b(void) {
  const size_t d = dit_of(20U);
  /* Logical close at call 5: dit 5..5+d-1, gap, dah 5+2d..5+5d-1, gap 5+5d..5+6d-1. */
  a_versus_b(5U + (2U * d) + 10U, ".-");
  a_versus_b(5U + (5U * d) - 1U, ".-");
  a_versus_b(5U + (5U * d) + 10U, ".-");
  a_versus_b(5U + (6U * d) - 1U, ".-");
  /* The same moments in the first dit and its gap. */
  a_versus_b(5U + 10U, ".");
  a_versus_b(5U + d - 1U, ".");
  a_versus_b(5U + d + 10U, ".");
  a_versus_b(5U + (2U * d) - 1U, ".");
  /* The gap-end call is the start call of the next element; then its second call (D-K2). */
  a_versus_b(5U + (2U * d), ".");
  a_versus_b(5U + (2U * d) + 1U, ".-");
  a_versus_b(5U + (6U * d), ".-");
  a_versus_b(5U + (6U * d) + 1U, ".-.");
}

/* AC-K11: every logical open call over 10 dits at 50 WPM; B is A plus one element of the other kind. */
static void test_iambic_a_versus_b_sweep(void) {
  const size_t d = dit_of(50U);
  char elements_a[64];
  char elements_b[64];
  size_t open_call;
  size_t length;
  size_t exceptions = 0U;

  /* The first logical close is call 5; the earliest logical open after it is call 10. */
  for (open_call = 10U; open_call <= 5U + (10U * d); ++open_call) {
    squeeze_release_at(KEYER_KEY_IAMBIC_A, 50U, open_call, elements_a, sizeof(elements_a));
    squeeze_release_at(KEYER_KEY_IAMBIC_B, 50U, open_call, elements_b, sizeof(elements_b));
    length = strlen(elements_a);
    if ((length == 0U) || (strlen(elements_b) != length + 1U) || (strncmp(elements_a, elements_b, length) != 0) ||
        (elements_b[length] == elements_a[length - 1U])) {
      if (exceptions < 5U) {
        fprintf(stderr, "open call %zu: A \"%s\", B \"%s\"\n", open_call, elements_a, elements_b);
      }
      ++exceptions;
    }
  }
  TEST_ASSERT_EQUAL(0U, exceptions);
}

/* AC-K25: the start call of an element latches the other lever and the squeeze (D-K2). */
static void test_start_latch(void) {
  const size_t d = dit_of(20U);
  char elements[32];

  /* (a) Both levers closed on the start call from IDLE (call 5) and at the gap end (call 5 + 2d, a dah). */
  setup();
  start(KEYER_KEY_IAMBIC_A, false, 20U, 20U);
  set_contacts(true, true);
  run(5U);
  TEST_ASSERT_EQUAL(KEYER_PHASE_ELEMENT, keyer_phase);
  TEST_ASSERT(!keyer_is_element_dah);
  TEST_ASSERT(keyer_has_dah_memory && !keyer_has_dit_memory && keyer_was_squeezed);
  run((2U * d) - 1U);
  TEST_ASSERT_EQUAL(KEYER_PHASE_GAP, keyer_phase);
  tick();
  TEST_ASSERT_EQUAL(5U + (2U * d), call_count);
  TEST_ASSERT_EQUAL(KEYER_PHASE_ELEMENT, keyer_phase);
  TEST_ASSERT(keyer_is_element_dah);
  TEST_ASSERT(keyer_has_dit_memory && !keyer_has_dah_memory && keyer_was_squeezed);

  /* (b) One lever closed on the start call sets no memory, from IDLE and at the gap end. */
  setup();
  start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  set_contacts(true, false);
  run(5U);
  TEST_ASSERT_EQUAL(KEYER_PHASE_ELEMENT, keyer_phase);
  TEST_ASSERT(!keyer_has_dit_memory && !keyer_has_dah_memory && !keyer_was_squeezed);
  run(2U * d);
  TEST_ASSERT_EQUAL(KEYER_PHASE_ELEMENT, keyer_phase);
  TEST_ASSERT(!keyer_is_element_dah);
  TEST_ASSERT(!keyer_has_dit_memory && !keyer_has_dah_memory && !keyer_was_squeezed);
  setup();
  start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  set_contacts(false, true);
  run(5U + (4U * d));
  TEST_ASSERT_EQUAL(KEYER_PHASE_ELEMENT, keyer_phase);
  TEST_ASSERT(keyer_is_element_dah);
  TEST_ASSERT(!keyer_has_dit_memory && !keyer_has_dah_memory && !keyer_was_squeezed);

  /*
   * (c) The squeeze is held through the start call of the dah (call 5 + 2d); the dit lever opens on the next call
   * and the dah lever stays closed. A dit follows the dah, in A and in B.
   */
  setup();
  start(KEYER_KEY_IAMBIC_A, false, 20U, 20U);
  set_contacts(true, true);
  run((2U * d) + 1U);
  set_contacts(false, true);
  run(5U);
  TEST_ASSERT(!keyer_is_tip_closed && keyer_is_ring_closed);
  TEST_ASSERT_EQUAL(5U + (2U * d) + 1U, call_count);
  run(7U * d);
  element_string(elements, sizeof(elements));
  TEST_ASSERT(strncmp(elements, ".-.", 3U) == 0);
  setup();
  start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  set_contacts(true, true);
  run((2U * d) + 1U);
  set_contacts(false, true);
  run(5U + (7U * d));
  element_string(elements, sizeof(elements));
  TEST_ASSERT(strncmp(elements, ".-.", 3U) == 0);
}

/* AC-K12: the dah lever closes while the dit lever is held; both open before the gap end. */
static void test_overlap_tap(void) {
  setup();
  start(KEYER_KEY_IAMBIC_A, false, 20U, 20U);
  set_contacts(true, false);
  run(20U);
  set_contacts(true, true);
  run(20U);
  set_contacts(false, false);
  run(4000U);
  TEST_ASSERT(elements_are("."));

  setup();
  start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  set_contacts(true, false);
  run(20U);
  set_contacts(true, true);
  run(20U);
  set_contacts(false, false);
  run(4000U);
  TEST_ASSERT(elements_are(".-"));
}

/* AC-K13 at 50 WPM through a held squeeze, in addition to the timing cases. */
static void test_early_release(void) {
  const uint16_t dit = dit_of(50U);

  setup();
  start(KEYER_KEY_IAMBIC_B, false, 50U, 50U);
  set_contacts(true, true);
  run(4U + (19U * dit) + 10U);
  set_contacts(false, false);
  run(10U * dit);
  /* The release falls in the gap after the 7th element, so Iambic B adds a dah. */
  check_element_timing(dit, ".-.-.-.-");
}

/* AC-K15 */
static size_t event_call_of(keyer_event_t event, size_t occurrence) {
  size_t index;
  for (index = 0U; (index < event_count) && (index < EVENT_LOG_SIZE); ++index) {
    if (event_log[index] == event) {
      if (occurrence == 0U) {
        return event_calls[index];
      }
      --occurrence;
    }
  }
  return 0U;
}

static void gap_case(uint8_t key_type, uint8_t wpm, uint8_t eff_wpm, uint16_t word_gap) {
  const uint16_t dit = dit_of(wpm);
  size_t mark_end;

  setup();
  start(key_type, false, wpm, eff_wpm);
  /* No gap event before the first mark. */
  run(20000U);
  TEST_ASSERT_EQUAL(0U, event_count);
  reset_trace();
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  run((size_t)word_gap + (4U * dit));
  mark_end = event_call_of(KEYER_EVENT_DIT, 0U);
  TEST_ASSERT(mark_end != 0U);
  TEST_ASSERT_EQUAL(1U, count_events(KEYER_EVENT_CHAR_GAP));
  TEST_ASSERT_EQUAL(1U, count_events(KEYER_EVENT_WORD_GAP));
  TEST_ASSERT_EQUAL(mark_end + (2U * dit) - 1U, event_call_of(KEYER_EVENT_CHAR_GAP, 0U));
  TEST_ASSERT_EQUAL(mark_end + word_gap - 1U, event_call_of(KEYER_EVENT_WORD_GAP, 0U));
  TEST_ASSERT_EQUAL(3U, event_count);

  /* A new mark before 2 dit cancels the char gap; one before the word gap cancels the word gap. */
  reset_trace();
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  run((key_type == KEYER_KEY_STRAIGHT) ? dit : (2U * dit));
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  run((size_t)word_gap - (2U * dit));
  TEST_ASSERT_EQUAL(0U, count_events(KEYER_EVENT_WORD_GAP));
  TEST_ASSERT_EQUAL(1U, count_events(KEYER_EVENT_CHAR_GAP));
  TEST_ASSERT_EQUAL(event_call_of(KEYER_EVENT_DIT, 1U) + (2U * dit) - 1U, event_call_of(KEYER_EVENT_CHAR_GAP, 0U));
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  run((size_t)word_gap + (4U * dit));
  TEST_ASSERT_EQUAL(2U, count_events(KEYER_EVENT_CHAR_GAP));
  TEST_ASSERT_EQUAL(1U, count_events(KEYER_EVENT_WORD_GAP));
  TEST_ASSERT(elements_are("..."));
}

static void test_gap_events(void) {
  gap_case(KEYER_KEY_STRAIGHT, 20U, 20U, 1200U);
  gap_case(KEYER_KEY_IAMBIC_A, 20U, 20U, 1200U);
  gap_case(KEYER_KEY_IAMBIC_B, 20U, 10U, 4360U);
  gap_case(KEYER_KEY_STRAIGHT, 50U, 5U, 11850U);
  gap_case(KEYER_KEY_IAMBIC_B, 5U, 5U, 4800U);
}

/*
 * ui-ux.md V-133 part B (UX-52): a space of keyer_word_gap_ticks - 1 calls between two marks gives no WORD_GAP. A space
 * of keyer_word_gap_ticks calls gives one, on the call where the space reaches it. The space starts on the call of the
 * DIT event (AC-K15).
 */
static void word_space_case(uint8_t key_type, uint8_t wpm, uint8_t eff_wpm, uint16_t word_gap, bool is_long) {
  const size_t space = is_long ? word_gap : (size_t)word_gap - 1U;
  run_t marks[4];
  size_t latency;
  size_t mark_end;
  size_t guard = 0U;

  setup();
  start(key_type, false, wpm, eff_wpm);
  TEST_ASSERT_EQUAL(word_gap, keyer_word_gap_ticks);
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  while ((count_events(KEYER_EVENT_DIT) == 0U) && (guard < 1000U)) {
    tick();
    ++guard;
  }
  mark_end = event_call_of(KEYER_EVENT_DIT, 0U);
  /* The contact closed before call 1; the input filter delays the mark start by the same count each time. */
  if (!TEST_ASSERT(find_runs(is_mark_phase, marks, 4U) == 1U)) {
    return;
  }
  latency = marks[0].start - 1U;
  run(mark_end + space - latency - 1U - call_count);
  set_contacts(true, false);
  run(10U);
  set_contacts(false, false);
  run((size_t)dit_of(wpm) + 50U);
  TEST_ASSERT_EQUAL(2U, find_runs(is_mark_phase, marks, 4U));
  TEST_ASSERT_EQUAL(mark_end + space, marks[1].start);
  TEST_ASSERT_EQUAL(1U, count_events(KEYER_EVENT_CHAR_GAP));
  TEST_ASSERT_EQUAL(mark_end + (2U * dit_of(wpm)) - 1U, event_call_of(KEYER_EVENT_CHAR_GAP, 0U));
  if (is_long) {
    TEST_ASSERT_EQUAL(1U, count_events(KEYER_EVENT_WORD_GAP));
    TEST_ASSERT_EQUAL(mark_end + word_gap - 1U, event_call_of(KEYER_EVENT_WORD_GAP, 0U));
  } else {
    TEST_ASSERT_EQUAL(0U, count_events(KEYER_EVENT_WORD_GAP));
  }
  TEST_ASSERT_EQUAL(2U, count_events(KEYER_EVENT_DIT));
}

static void test_word_space_threshold(void) {
  static const uint8_t key_types[2] = {KEYER_KEY_STRAIGHT, KEYER_KEY_IAMBIC_B};
  size_t index;

  for (index = 0U; index < 2U; ++index) {
    word_space_case(key_types[index], 20U, 20U, 1200U, false);
    word_space_case(key_types[index], 20U, 20U, 1200U, true);
    word_space_case(key_types[index], 20U, 10U, 4360U, false);
    word_space_case(key_types[index], 20U, 10U, 4360U, true);
  }
}

/* AC-K17: straight taps of 20 calls at 5 WPM give DIT events with no gap event between them. */
static void straight_taps(size_t count) {
  size_t index;
  for (index = 0U; index < count; ++index) {
    set_contacts(true, false);
    run(20U);
    set_contacts(false, false);
    run(20U);
  }
}

static void test_queue_overflow(void) {
  size_t index;

  setup();
  start(KEYER_KEY_STRAIGHT, false, 5U, 5U);
  should_read_events = false;
  straight_taps(8U);
  TEST_ASSERT_EQUAL(8U, (uint8_t)(keyer_queue_head - keyer_queue_tail));
  TEST_ASSERT(!keyer_is_overflow);
  straight_taps(1U);
  TEST_ASSERT(keyer_is_overflow);
  TEST_ASSERT_EQUAL(8U, (uint8_t)(keyer_queue_head - keyer_queue_tail));
  /* More taps and the char and word gaps are dropped too. */
  straight_taps(3U);
  run(20000U);
  TEST_ASSERT_EQUAL(8U, (uint8_t)(keyer_queue_head - keyer_queue_tail));
  for (index = 0U; index < 8U; ++index) {
    TEST_ASSERT_EQUAL(KEYER_EVENT_DIT, keyer_read_event());
  }
  TEST_ASSERT_EQUAL(KEYER_EVENT_OVERFLOW, keyer_read_event());
  TEST_ASSERT(!keyer_is_overflow);
  TEST_ASSERT_EQUAL(KEYER_EVENT_NONE, keyer_read_event());
  straight_taps(1U);
  TEST_ASSERT_EQUAL(KEYER_EVENT_DIT, keyer_read_event());
  TEST_ASSERT_EQUAL(KEYER_EVENT_NONE, keyer_read_event());
  keyer_stop();
}

/* AC-K18: a held squeeze at 50 WPM read in batches; the order holds across the byte wrap. */
static void test_queue_wrap(void) {
  size_t batch;
  size_t count = 0U;
  bool is_order_kept = true;
  keyer_event_t expected = KEYER_EVENT_DIT;
  keyer_event_t event;

  setup();
  start(KEYER_KEY_IAMBIC_B, false, 50U, 50U);
  should_read_events = false;
  set_contacts(true, true);
  for (batch = 0U; batch < 120U; ++batch) {
    run(1000U);
    TEST_ASSERT(!keyer_is_overflow);
    for (event = keyer_read_event(); event != KEYER_EVENT_NONE; event = keyer_read_event()) {
      is_order_kept = is_order_kept && (event == expected);
      expected = (expected == KEYER_EVENT_DIT) ? KEYER_EVENT_DAH : KEYER_EVENT_DIT;
      ++count;
    }
  }
  TEST_ASSERT(is_order_kept);
  TEST_ASSERT(count >= 300U);
  keyer_stop();
}

/* AC-K19: a long run with mixed contacts in every key type; tick() counts calls with more than one event. */
static void test_one_event_per_call(void) {
  static const uint8_t key_types[3] = {KEYER_KEY_STRAIGHT, KEYER_KEY_IAMBIC_A, KEYER_KEY_IAMBIC_B};
  static const uint8_t speeds[3] = {5U, 23U, 50U};
  uint32_t seed = 12345U;
  size_t type_index;
  size_t speed_index;
  size_t step;

  for (type_index = 0U; type_index < 3U; ++type_index) {
    for (speed_index = 0U; speed_index < 3U; ++speed_index) {
      setup();
      start(key_types[type_index], (speed_index % 2U) != 0U, speeds[speed_index], 5U);
      for (step = 0U; step < 400U; ++step) {
        seed = (seed * 1103515245U) + 12345U;
        set_contacts(((seed >> 16) & 1U) != 0U, ((seed >> 17) & 1U) != 0U);
        run(1U + ((seed >> 18) % 600U));
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, head_step_violations);
}

/* AC-K20 */
static void test_start_clamps(void) {
  setup();
  keyer_start(KEYER_KEY_STRAIGHT, false, 0U, 0U);
  TEST_ASSERT_EQUAL(960U, keyer_dit_ticks);
  TEST_ASSERT_EQUAL(4800U, keyer_word_gap_ticks);
  keyer_start(KEYER_KEY_STRAIGHT, false, 4U, 4U);
  TEST_ASSERT_EQUAL(960U, keyer_dit_ticks);
  keyer_start(KEYER_KEY_STRAIGHT, false, 51U, 51U);
  TEST_ASSERT_EQUAL(96U, keyer_dit_ticks);
  TEST_ASSERT_EQUAL(480U, keyer_word_gap_ticks);
  keyer_start(KEYER_KEY_STRAIGHT, false, 255U, 255U);
  TEST_ASSERT_EQUAL(96U, keyer_dit_ticks);
  TEST_ASSERT_EQUAL(480U, keyer_word_gap_ticks);
  /* eff_wpm above wpm gives wpm: the word gap is 5 dit. */
  keyer_start(KEYER_KEY_STRAIGHT, false, 20U, 30U);
  TEST_ASSERT_EQUAL(1200U, keyer_word_gap_ticks);
  /* eff_wpm 0 gives 5. */
  keyer_start(KEYER_KEY_STRAIGHT, false, 20U, 0U);
  TEST_ASSERT_EQUAL(10675U, keyer_word_gap_ticks);
  keyer_start(KEYER_KEY_STRAIGHT, false, 20U, 20U);
  TEST_ASSERT(keyer_is_straight && !keyer_is_iambic_a);
  keyer_start(KEYER_KEY_IAMBIC_A, false, 20U, 20U);
  TEST_ASSERT(!keyer_is_straight && keyer_is_iambic_a);
  keyer_start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  TEST_ASSERT(!keyer_is_straight && !keyer_is_iambic_a);
  keyer_start(3U, true, 20U, 20U);
  TEST_ASSERT(!keyer_is_straight && !keyer_is_iambic_a);
  TEST_ASSERT(keyer_is_swapped);
  keyer_start(255U, false, 20U, 20U);
  TEST_ASSERT(!keyer_is_straight && !keyer_is_iambic_a);
  TEST_ASSERT(!keyer_is_swapped);
  TEST_ASSERT(keyer_is_enabled && keyer_is_start_requested);
  keyer_stop();
}

/* AC-K21 */
static void test_start_during_element(void) {
  setup();
  start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  should_read_events = false;
  set_contacts(false, true);
  run(100U);
  /* A dit tap inside the dah sets the dit memory. */
  set_contacts(true, true);
  run(10U);
  set_contacts(false, true);
  run(10U);
  TEST_ASSERT_EQUAL(KEYER_PHASE_ELEMENT, keyer_phase);
  TEST_ASSERT(keyer_has_dit_memory);
  TEST_ASSERT(test_is_tone_requested);

  keyer_start(KEYER_KEY_IAMBIC_B, false, 20U, 20U);
  TEST_ASSERT(keyer_is_start_requested);
  tick();
  TEST_ASSERT(!keyer_is_start_requested);
  TEST_ASSERT_EQUAL(KEYER_PHASE_IDLE, keyer_phase);
  TEST_ASSERT(!test_is_tone_requested);
  TEST_ASSERT(!keyer_has_dit_memory && !keyer_has_dah_memory && !keyer_was_squeezed);
  TEST_ASSERT(keyer_is_ring_masked);
  TEST_ASSERT_EQUAL(KEYER_EVENT_NONE, keyer_read_event());

  /* The held dah lever is masked; the old memory never starts a dit. */
  reset_trace();
  should_read_events = true;
  run(3000U);
  set_contacts(false, false);
  run(3000U);
  TEST_ASSERT_EQUAL(0U, event_count);
  TEST_ASSERT_EQUAL(KEYER_PHASE_IDLE, keyer_phase);
  keyer_stop();
}

/* AC-K22 */
static void test_wpm_range_matches_settings(void) {
  fake_iap_reset();
  (void)settings_load();
  TEST_ASSERT_EQUAL(settings_min(SETTINGS_ID_TX_WPM), KEYER_WPM_MIN);
  TEST_ASSERT_EQUAL(settings_max(SETTINGS_ID_TX_WPM), KEYER_WPM_MAX);
  TEST_ASSERT_EQUAL(SETTINGS_KEY_STRAIGHT, KEYER_KEY_STRAIGHT);
  TEST_ASSERT_EQUAL(SETTINGS_KEY_IAMBIC_A, KEYER_KEY_IAMBIC_A);
  TEST_ASSERT_EQUAL(SETTINGS_KEY_IAMBIC_B, KEYER_KEY_IAMBIC_B);
}

/* AC-K23 */
static void test_stop(void) {
  uint8_t head;
  size_t reads;
  uint16_t ticks;

  setup();
  start(KEYER_KEY_STRAIGHT, false, 20U, 20U);
  set_contacts(true, false);
  run(50U);
  TEST_ASSERT_EQUAL(KEYER_PHASE_STRAIGHT_DOWN, keyer_phase);
  ticks = keyer_ticks;
  sidetone_set_on(true);
  keyer_stop();
  TEST_ASSERT(!keyer_is_enabled);
  TEST_ASSERT(!keyer_is_start_requested);
  /* The real sidetone gets the off request; its state is OFF because no service ran. */
  TEST_ASSERT(!sidetone_is_busy());
  head = keyer_queue_head;
  reads = fake_paddle_pins_read_count();
  reset_trace();
  set_contacts(false, false);
  run(5000U);
  TEST_ASSERT_EQUAL(head, keyer_queue_head);
  TEST_ASSERT_EQUAL(reads, fake_paddle_pins_read_count());
  TEST_ASSERT_EQUAL(KEYER_PHASE_STRAIGHT_DOWN, keyer_phase);
  TEST_ASSERT_EQUAL(ticks, keyer_ticks);
  TEST_ASSERT(test_is_tone_requested);
  TEST_ASSERT_EQUAL(0U, event_count);
  keyer_stop();
  TEST_ASSERT(!keyer_is_enabled);
}

/* AC-K24 */
static void test_init(void) {
  size_t reads;

  fake_paddle_pins_reset();
  keyer_start(KEYER_KEY_STRAIGHT, false, 20U, 20U);
  keyer_init();
  TEST_ASSERT_EQUAL(1U, fake_paddle_pins_configure_count());
  TEST_ASSERT(!keyer_is_enabled);
  TEST_ASSERT(!keyer_is_start_requested);
  TEST_ASSERT(!keyer_is_overflow);
  TEST_ASSERT_EQUAL(keyer_queue_head, keyer_queue_tail);
  reads = fake_paddle_pins_read_count();
  run(100U);
  TEST_ASSERT_EQUAL(reads, fake_paddle_pins_read_count());
  TEST_ASSERT_EQUAL(KEYER_EVENT_NONE, keyer_read_event());
}

int main(void) {
  TEST_RUN(test_init);
  TEST_RUN(test_spec_example);
  TEST_RUN(test_filter_votes_on_fifth_sample);
  TEST_RUN(test_filter_ignores_glitches);
  TEST_RUN(test_entry_mask);
  TEST_RUN(test_mono_plug_keys_straight_on_tip);
  TEST_RUN(test_swap);
  TEST_RUN(test_straight);
  TEST_RUN(test_straight_saturation);
  TEST_RUN(test_exact_timing);
  TEST_RUN(test_dit_formula);
  TEST_RUN(test_farnsworth_table);
  TEST_RUN(test_iambic_decisions);
  TEST_RUN(test_iambic_a_versus_b);
  TEST_RUN(test_iambic_a_versus_b_sweep);
  TEST_RUN(test_start_latch);
  TEST_RUN(test_overlap_tap);
  TEST_RUN(test_early_release);
  TEST_RUN(test_gap_events);
  TEST_RUN(test_word_space_threshold);
  TEST_RUN(test_queue_overflow);
  TEST_RUN(test_queue_wrap);
  TEST_RUN(test_one_event_per_call);
  TEST_RUN(test_start_clamps);
  TEST_RUN(test_start_during_element);
  TEST_RUN(test_wpm_range_matches_settings);
  TEST_RUN(test_stop);
  TEST_RUN(test_init);
  TEST_ASSERT_EQUAL(0U, head_step_violations);
  return TEST_FINISH();
}
