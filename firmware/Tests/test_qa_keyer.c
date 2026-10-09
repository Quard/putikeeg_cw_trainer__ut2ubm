/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the keyer (references/internal/keyer_spec.md sections 6.1 to 6.9, AC-K2, AC-K3, AC-K11, AC-K16,
 * AC-K17, AC-K19, AC-K21, AC-K23). An independent reference model of section 6, written from the spec text, runs
 * call by call next to the real keyer_service(). Random contacts with bounce, all key types, swap, 5..50 WPM,
 * random starts and stops in every phase and a reader that stalls are compared state by state. Exhaustive sweeps
 * cover the squeeze release on every call around each boundary and every WPM pair of the Farnsworth formula.
 */

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "fake_paddle_pins.h"
#include "test_assert.h"

static bool qa_is_tone_requested;
#define KEYER_SIDETONE_ON() (qa_is_tone_requested = true)
#define KEYER_SIDETONE_OFF() (qa_is_tone_requested = false)
#include "cw/keyer_service.h"

/* ---- Reference model (keyer_spec.md section 6), independent of keyer_service.h ---- */

typedef struct {
  bool is_enabled;
  bool is_start_requested;
  bool is_straight;
  bool is_iambic_a;
  bool is_swapped;
  uint16_t dit;
  uint16_t dah;
  uint16_t two_dit;
  uint16_t word_gap;
  /* ISR state */
  uint8_t phase;
  uint32_t ticks;
  uint32_t space;
  uint8_t history[2];
  uint8_t count[2];
  bool is_closed[2];
  bool is_masked[2];
  bool has_dit_memory;
  bool has_dah_memory;
  bool was_squeezed;
  bool is_element_dah;
  bool is_char_pending;
  bool is_word_pending;
  bool is_tone;
  /* queue */
  uint8_t queue[256];
  unsigned head;
  unsigned tail;
  bool is_overflow;
  /* last call */
  uint8_t pushed;
} model_t;

static model_t model;

/* round(4800 / wpm): the nearest integer, checked as |dit * wpm - 4800| <= wpm / 2. */
static uint16_t model_dit(unsigned wpm) {
  unsigned dit = 4800U / wpm;
  if ((4800U - dit * wpm) * 2U >= wpm) {
    ++dit;
  }
  return (uint16_t)dit;
}

/* Section 6.7 in 64-bit; the result must be the nearest integer to the exact quotient. */
static uint16_t model_fu(unsigned c, unsigned s) {
  const int64_t numerator = 240000LL * c - 148800LL * s;
  const int64_t denominator = 19LL * c * s;
  int64_t fu = numerator / denominator;
  int64_t error;
  if ((numerator - fu * denominator) * 2 >= denominator) {
    ++fu;
  }
  error = fu * denominator - numerator;
  TEST_ASSERT(((error < 0) ? -error : error) * 2 <= denominator);
  if (fu < model_dit(c)) {
    fu = model_dit(c);
  }
  return (uint16_t)fu;
}

static void model_stop(void) {
  model.is_enabled = false;
  model.is_start_requested = false;
}

static void model_start(unsigned key_type, bool is_swapped, unsigned wpm, unsigned eff) {
  model_stop();
  if (wpm < 5U) {
    wpm = 5U;
  }
  if (wpm > 50U) {
    wpm = 50U;
  }
  if (eff < 5U) {
    eff = 5U;
  }
  if (eff > wpm) {
    eff = wpm;
  }
  if (key_type > 2U) {
    key_type = 2U;
  }
  model.dit = model_dit(wpm);
  model.dah = (uint16_t)(3U * model.dit);
  model.two_dit = (uint16_t)(2U * model.dit);
  model.word_gap = (uint16_t)(5U * model_fu(wpm, eff));
  model.is_straight = (key_type == 0U);
  model.is_iambic_a = (key_type == 1U);
  model.is_swapped = is_swapped;
  model.tail = model.head;
  model.is_overflow = false;
  model.is_start_requested = true;
  model.is_enabled = true;
}

static uint8_t model_read(void) {
  if (model.tail != model.head) {
    const uint8_t event = model.queue[model.tail % 256U];
    model.tail = (model.tail + 1U) % 256U;
    return event;
  }
  if (model.is_overflow) {
    model.is_overflow = false;
    return KEYER_EVENT_OVERFLOW;
  }
  return KEYER_EVENT_NONE;
}

/* "Start element E" of section 6.5, with the start latch of decision D-K2 on the levels of this call. */
static void model_begin_element(bool is_dah, bool is_dit_now, bool is_dah_now) {
  model.phase = KEYER_PHASE_ELEMENT;
  model.is_element_dah = is_dah;
  model.ticks = is_dah ? model.dah : model.dit;
  model.is_tone = true;
  model.is_char_pending = false;
  model.is_word_pending = false;
  if (is_dah && is_dit_now) {
    model.has_dit_memory = true;
  }
  if (!is_dah && is_dah_now) {
    model.has_dah_memory = true;
  }
  if (is_dit_now && is_dah_now) {
    model.was_squeezed = true;
  }
}

static void model_mark_end(uint8_t* event, uint8_t value) {
  *event = value;
  model.space = 0U;
  model.is_char_pending = true;
  model.is_word_pending = true;
}

static void model_service(bool raw_tip, bool raw_ring) {
  const bool raw[2] = {raw_tip, raw_ring};
  bool logical[2];
  bool is_dit;
  bool is_dah;
  uint8_t event = KEYER_EVENT_NONE;
  unsigned k;

  model.pushed = KEYER_EVENT_NONE;
  if (!model.is_enabled) {
    return;
  }
  if (model.is_start_requested) {
    for (k = 0U; k < 2U; ++k) {
      model.history[k] = raw[k] ? 0xFFU : 0x00U;
      model.count[k] = raw[k] ? 8U : 0U;
      model.is_closed[k] = raw[k];
      model.is_masked[k] = raw[k];
    }
    model.phase = KEYER_PHASE_IDLE;
    model.ticks = 0U;
    model.space = 0U;
    model.has_dit_memory = false;
    model.has_dah_memory = false;
    model.was_squeezed = false;
    model.is_element_dah = false;
    model.is_char_pending = false;
    model.is_word_pending = false;
    model.is_tone = false;
    model.is_start_requested = false;
    return;
  }
  for (k = 0U; k < 2U; ++k) {
    if ((model.history[k] & 0x80U) != 0U) {
      --model.count[k];
    }
    model.history[k] = (uint8_t)(model.history[k] << 1);
    if (raw[k]) {
      model.history[k] |= 1U;
      ++model.count[k];
    }
    if (model.count[k] >= 5U) {
      model.is_closed[k] = true;
    } else if (model.count[k] <= 3U) {
      model.is_closed[k] = false;
    }
    if (model.is_masked[k] && !model.is_closed[k]) {
      model.is_masked[k] = false;
    }
    logical[k] = model.is_closed[k] && !model.is_masked[k];
  }
  is_dit = model.is_swapped ? logical[1] : logical[0];
  is_dah = model.is_swapped ? logical[0] : logical[1];

  switch (model.phase) {
    case KEYER_PHASE_IDLE:
      if (model.is_straight) {
        if (is_dit || is_dah) {
          model.ticks = 1U;
          model.is_tone = true;
          model.is_char_pending = false;
          model.is_word_pending = false;
          model.phase = KEYER_PHASE_STRAIGHT_DOWN;
        }
      } else if (is_dit) {
        model_begin_element(false, is_dit, is_dah);
      } else if (is_dah) {
        model_begin_element(true, is_dit, is_dah);
      }
      break;
    case KEYER_PHASE_STRAIGHT_DOWN:
      if (is_dit || is_dah) {
        if (model.ticks < 0xFFFFU) {
          ++model.ticks;
        }
      } else {
        model.is_tone = false;
        model_mark_end(&event, (model.ticks >= model.two_dit) ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
        model.phase = KEYER_PHASE_IDLE;
      }
      break;
    case KEYER_PHASE_ELEMENT:
      if (model.is_element_dah && is_dit) {
        model.has_dit_memory = true;
      }
      if (!model.is_element_dah && is_dah) {
        model.has_dah_memory = true;
      }
      if (is_dit && is_dah) {
        model.was_squeezed = true;
      }
      --model.ticks;
      if (model.ticks == 0U) {
        model_mark_end(&event, model.is_element_dah ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
        model.ticks = model.dit;
        model.phase = KEYER_PHASE_GAP;
      } else if (model.ticks <= 32U) {
        model.is_tone = false;
      }
      break;
    default: /* GAP */
      if (is_dit) {
        model.has_dit_memory = true;
      }
      if (is_dah) {
        model.has_dah_memory = true;
      }
      if (is_dit && is_dah) {
        model.was_squeezed = true;
      }
      --model.ticks;
      if (model.ticks == 0U) {
        const bool last = model.is_element_dah;
        const bool wants_dit = is_dit || model.has_dit_memory;
        const bool wants_dah = is_dah || model.has_dah_memory;
        const bool wants_opposite = last ? wants_dit : wants_dah;
        const bool wants_same = last ? wants_dah : wants_dit;
        /* 0 = IDLE, 1 = start a dit, 2 = start a dah. */
        unsigned next = 0U;
        if (model.is_iambic_a && model.was_squeezed && !is_dit && !is_dah) {
          next = 0U;
        } else if (wants_opposite) {
          next = last ? 1U : 2U;
        } else if (wants_same) {
          next = last ? 2U : 1U;
        }
        /* The clear comes first; the start latch of the new element can set the memories again (D-K2). */
        model.has_dit_memory = false;
        model.has_dah_memory = false;
        model.was_squeezed = false;
        model.phase = KEYER_PHASE_IDLE;
        if (next != 0U) {
          model_begin_element(next == 2U, is_dit, is_dah);
        }
      }
      break;
  }

  if (model.is_word_pending) {
    ++model.space;
    if (model.is_char_pending && (model.space == model.two_dit)) {
      event = KEYER_EVENT_CHAR_GAP;
      model.is_char_pending = false;
    }
    if (model.space == model.word_gap) {
      event = KEYER_EVENT_WORD_GAP;
      model.is_word_pending = false;
    }
  }

  if (event != KEYER_EVENT_NONE) {
    if (model.is_overflow) {
      /* dropped */
    } else if (((model.head - model.tail) & 0xFFU) >= KEYER_QUEUE_SIZE) {
      model.is_overflow = true;
    } else {
      model.queue[model.head % 256U] = event;
      model.head = (model.head + 1U) % 256U;
      model.pushed = event;
    }
  }
}

/* ---- Harness ---- */

static bool raw_tip;
static bool raw_ring;
static unsigned long mismatches;
static unsigned long calls_total;

static void setup(void) {
  memset(&model, 0, sizeof(model));
  fake_paddle_pins_reset();
  keyer_init();
  model.head = keyer_queue_head;
  model.tail = keyer_queue_tail;
  raw_tip = false;
  raw_ring = false;
  qa_is_tone_requested = false;
}

static void both_start(unsigned key_type, bool is_swapped, unsigned wpm, unsigned eff) {
  keyer_start((uint8_t)key_type, is_swapped, (uint8_t)wpm, (uint8_t)eff);
  model_start(key_type, is_swapped, wpm, eff);
  TEST_ASSERT_EQUAL(model.dit, keyer_dit_ticks);
  TEST_ASSERT_EQUAL(model.dah, keyer_dah_ticks);
  TEST_ASSERT_EQUAL(model.two_dit, keyer_two_dit_ticks);
  TEST_ASSERT_EQUAL(model.word_gap, keyer_word_gap_ticks);
}

static void both_stop(void) {
  keyer_stop();
  model_stop();
}

/* One call of both; returns false at the first state difference (reported once per call). */
static bool step(void) {
  const uint8_t head_before = keyer_queue_head;
  const size_t reads_before = fake_paddle_pins_read_count();
  const bool was_enabled = model.is_enabled;
  bool is_same = true;

  fake_paddle_pins_set_tip_closed(raw_tip);
  fake_paddle_pins_set_ring_closed(raw_ring);
  keyer_service();
  model_service(raw_tip, raw_ring);
  ++calls_total;

  /* AC-K19: at most one event per call. */
  is_same = is_same && ((uint8_t)(keyer_queue_head - head_before) <= 1U);
  /* AC-K23: a disabled call reads no pin. */
  if (!was_enabled) {
    is_same = is_same && (fake_paddle_pins_read_count() == reads_before);
  }
  is_same = is_same && (keyer_queue_head == model.head) && (keyer_is_overflow == model.is_overflow);
  if ((model.pushed != KEYER_EVENT_NONE) && is_same) {
    is_same = keyer_queue[(uint8_t)(keyer_queue_head - 1U) & KEYER_QUEUE_MASK] == model.pushed;
  }
  if (was_enabled) {
    is_same = is_same && (keyer_phase == model.phase) && (keyer_ticks == model.ticks) &&
              (keyer_space_ticks == model.space) && (qa_is_tone_requested == model.is_tone) &&
              (keyer_tip_history == model.history[0]) && (keyer_ring_history == model.history[1]) &&
              (keyer_tip_count == model.count[0]) && (keyer_ring_count == model.count[1]) &&
              (keyer_is_tip_closed == model.is_closed[0]) && (keyer_is_ring_closed == model.is_closed[1]) &&
              (keyer_is_tip_masked == model.is_masked[0]) && (keyer_is_ring_masked == model.is_masked[1]) &&
              (keyer_has_dit_memory == model.has_dit_memory) && (keyer_has_dah_memory == model.has_dah_memory) &&
              (keyer_was_squeezed == model.was_squeezed) && (keyer_is_element_dah == model.is_element_dah) &&
              (keyer_is_char_gap_pending == model.is_char_pending) &&
              (keyer_is_word_gap_pending == model.is_word_pending) &&
              (keyer_is_start_requested == model.is_start_requested);
  }
  if (!is_same) {
    ++mismatches;
    if (mismatches <= 5U) {
      fprintf(stderr,
              "mismatch at call %lu: phase %u/%u ticks %u/%lu space %u/%lu tone %d/%d head %u/%u ovf %d/%d "
              "mem %d%d/%d%d sq %d/%d\n",
              calls_total, keyer_phase, model.phase, keyer_ticks, (unsigned long)model.ticks, keyer_space_ticks,
              (unsigned long)model.space, qa_is_tone_requested, model.is_tone, keyer_queue_head, model.head,
              keyer_is_overflow, model.is_overflow, keyer_has_dit_memory, keyer_has_dah_memory, model.has_dit_memory,
              model.has_dah_memory, keyer_was_squeezed, model.was_squeezed);
    }
  }
  return is_same;
}

static uint32_t rng_state;
static uint32_t rng_next(void) {
  rng_state = rng_state * 1664525U + 1013904223U;
  return rng_state >> 8;
}
static uint32_t rng_range(uint32_t low, uint32_t high) { return low + (rng_next() % (high - low + 1U)); }

/* ---- Tests ---- */

/*
 * AC-K19 and a long run against the model: random contact segments (with bounce bursts), all key types, swap,
 * 5..50 WPM, random Farnsworth speeds, random restarts and stops in every phase, and a reader that reads at random
 * times and sometimes stalls long enough to overflow. Every call compares the full ISR state, and every read
 * compares the returned event.
 */
static void test_random_against_model(void) {
  unsigned run;
  unsigned long reads = 0U;
  unsigned long overflows = 0U;
  unsigned long starts_in_phase[4] = {0U, 0U, 0U, 0U};
  unsigned long stops_in_phase[4] = {0U, 0U, 0U, 0U};
  unsigned long events_by_kind[6] = {0U, 0U, 0U, 0U, 0U, 0U};

  rng_state = 0xC0FFEEU;
  mismatches = 0U;
  for (run = 0U; run < 120U; ++run) {
    unsigned long call;
    const unsigned long length = 200000U;
    unsigned long segment_end = 0U;
    unsigned long stall_until = 0U;
    unsigned wpm = rng_range(5U, 50U);
    setup();
    /* The queue bytes start anywhere, so the wrap of the free-running indexes is covered. */
    keyer_queue_head = (uint8_t)rng_next();
    keyer_queue_tail = keyer_queue_head;
    model.head = keyer_queue_head;
    model.tail = keyer_queue_tail;
    raw_tip = (rng_next() & 1U) != 0U;
    raw_ring = (rng_next() & 3U) == 0U;
    both_start(rng_range(0U, 4U), (rng_next() & 1U) != 0U, wpm, rng_range(0U, 55U));
    for (call = 0U; call < length; ++call) {
      if (call >= segment_end) {
        const uint32_t kind = rng_next() % 16U;
        const uint16_t unit = keyer_dit_ticks;
        if (kind < 10U) {
          raw_tip = (rng_next() & 1U) != 0U;
          raw_ring = (rng_next() & 1U) != 0U;
          segment_end = call + rng_range(1U, 4U * unit);
        } else if (kind < 13U) {
          /* Bounce burst: 1..3-call runs. */
          raw_tip = !raw_tip;
          segment_end = call + rng_range(1U, 3U);
        } else if (kind < 15U) {
          raw_tip = false;
          raw_ring = false;
          segment_end = call + rng_range(unit, 15U * unit);
        } else {
          const uint8_t phase = keyer_phase;
          if ((rng_next() & 3U) == 0U) {
            ++stops_in_phase[phase & 3U];
            both_stop();
            /* Stopped calls: nothing changes and no pin is read. */
            segment_end = call + rng_range(1U, 50U);
          } else {
            ++starts_in_phase[phase & 3U];
            wpm = rng_range(0U, 60U);
            both_start(rng_range(0U, 255U), (rng_next() & 1U) != 0U, wpm, rng_range(0U, 60U));
            segment_end = call + 1U;
          }
        }
      }
      if (!model.is_enabled && ((rng_next() % 64U) == 0U)) {
        both_start(rng_range(0U, 2U), (rng_next() & 1U) != 0U, rng_range(5U, 50U), rng_range(5U, 50U));
      }
      if (!step()) {
        break;
      }
      if ((call >= stall_until) && ((rng_next() % 4U) == 0U)) {
        unsigned n = rng_range(1U, 3U);
        while (n-- != 0U) {
          const uint8_t real = keyer_read_event();
          const uint8_t expected = model_read();
          ++reads;
          if (!TEST_ASSERT_EQUAL(expected, real)) {
            break;
          }
          if (real < 6U) {
            ++events_by_kind[real];
          }
          if (real == KEYER_EVENT_OVERFLOW) {
            ++overflows;
          }
          if (real == KEYER_EVENT_NONE) {
            break;
          }
        }
        if ((rng_next() % 2000U) == 0U) {
          stall_until = call + rng_range(4000U, 60000U);
        }
      }
    }
  }
  printf("model run: %lu calls, %lu reads, %lu overflows; events dit %lu dah %lu char %lu word %lu\n", calls_total,
         reads, overflows, events_by_kind[1], events_by_kind[2], events_by_kind[3], events_by_kind[4]);
  printf("starts in phase idle/straight/element/gap: %lu/%lu/%lu/%lu, stops %lu/%lu/%lu/%lu\n", starts_in_phase[0],
         starts_in_phase[1], starts_in_phase[2], starts_in_phase[3], stops_in_phase[0], stops_in_phase[1],
         stops_in_phase[2], stops_in_phase[3]);
  TEST_ASSERT_EQUAL(0U, mismatches);
  TEST_ASSERT(overflows >= 20U);
  TEST_ASSERT(events_by_kind[3] >= 1000U);
  TEST_ASSERT(events_by_kind[4] >= 100U);
  TEST_ASSERT(starts_in_phase[KEYER_PHASE_ELEMENT] >= 20U);
  TEST_ASSERT(starts_in_phase[KEYER_PHASE_GAP] >= 20U);
  TEST_ASSERT(starts_in_phase[KEYER_PHASE_STRAIGHT_DOWN] >= 5U);
  TEST_ASSERT(stops_in_phase[KEYER_PHASE_ELEMENT] >= 5U);
}

/* Runs the keyer and the model with the current contacts for @p calls calls. Returns false at a mismatch. */
static bool run_both(unsigned long calls) {
  while (calls-- != 0U) {
    if (!step()) {
      return false;
    }
  }
  return true;
}

/* Collects the elements generated from now on (reads the queue), until @p calls calls pass. */
static void collect(unsigned long calls, char* out, size_t size) {
  size_t n = strlen(out);
  while (calls-- != 0U) {
    uint8_t event;
    (void)step();
    while ((event = keyer_read_event()) != KEYER_EVENT_NONE) {
      (void)model_read();
      if (((event == KEYER_EVENT_DIT) || (event == KEYER_EVENT_DAH)) && (n + 1U < size)) {
        out[n++] = (event == KEYER_EVENT_DAH) ? '-' : '.';
        out[n] = '\0';
      }
    }
  }
}

/*
 * AC-K11 swept on every call (decision D-K2): a squeeze from idle at 50 WPM whose raw release lands on each call
 * from 1 to 10 dits after the logical close. For every release call the keyer matches the model, and every release
 * that gives an element gives B = the A sequence plus exactly one element of the other kind. B = A only when the
 * squeeze never votes closed (raw release on calls 1..4: no element at all).
 */
static void test_squeeze_release_every_call(void) {
  static char a[64];
  static char b[64];
  const unsigned wpm = 50U;
  const unsigned long d = model_dit(wpm);
  unsigned long release;
  unsigned long b_equals_a = 0U;
  unsigned long b_extends_a = 0U;
  unsigned long other = 0U;
  unsigned long first_b_equals_a = 0U;
  mismatches = 0U;

  for (release = 1U; release <= 10U * d; ++release) {
    /* The filter votes closed on the 5th closed sample, so releases 1..4 give no element. */
    const bool is_before_first_element = (release < KEYER_FILTER_CLOSED_MIN);
    unsigned type;
    for (type = 1U; type <= 2U; ++type) {
      char* out = (type == 1U) ? a : b;
      setup();
      both_start(type, false, wpm, wpm);
      (void)run_both(1U); /* reseed */
      raw_tip = true;
      raw_ring = true;
      out[0] = '\0';
      collect(release, out, 64U);
      raw_tip = false;
      raw_ring = false;
      collect(20U * d, out, 64U);
      TEST_ASSERT_EQUAL(KEYER_PHASE_IDLE, keyer_phase);
    }
    {
      const size_t la = strlen(a);
      const size_t lb = strlen(b);
      if ((la != 0U) && (lb == la + 1U) && (strncmp(a, b, la) == 0) && (b[la] != a[la - 1U])) {
        ++b_extends_a;
        TEST_ASSERT(!is_before_first_element);
      } else if (strcmp(a, b) == 0) {
        if (b_equals_a == 0U) {
          first_b_equals_a = release;
        }
        ++b_equals_a;
        if (!TEST_ASSERT(is_before_first_element && (la == 0U))) {
          fprintf(stderr, "release %lu: A = B = \"%s\"\n", release, a);
        }
      } else {
        ++other;
        fprintf(stderr, "release %lu: A %s B %s\n", release, a, b);
      }
    }
  }
  printf("squeeze sweep at %u WPM (d = %lu): B = A + 1 in %lu cases, B = A in %lu cases (first at raw release %lu)\n",
         wpm, d, b_extends_a, b_equals_a, first_b_equals_a);
  TEST_ASSERT_EQUAL(0U, mismatches);
  TEST_ASSERT_EQUAL(0U, other);
  TEST_ASSERT_EQUAL(KEYER_FILTER_CLOSED_MIN - 1U, b_equals_a);
  TEST_ASSERT_EQUAL(10U * d - (KEYER_FILTER_CLOSED_MIN - 1U), b_extends_a);
}

/*
 * AC-K11 boundary list from the spec, for every key type pair and both swap values: logical open early in an
 * element, on the second call of an element (the D-K2 case), on the last ELEMENT call, inside the gap, and on the
 * gap-end call. The raw release comes 4 calls before the logical open (5th open sample).
 */
static void test_squeeze_release_named_boundaries(void) {
  const unsigned wpms[3] = {5U, 20U, 50U};
  unsigned w;
  mismatches = 0U;
  for (w = 0U; w < 3U; ++w) {
    const unsigned long d = model_dit(wpms[w]);
    /*
     * Logical close at call 5 of the run (after reseed). Dit 5..5+d-1, gap 5+d..5+2d (gap-end call 5+2d starts
     * the dah), dah 5+2d..5+5d-1, gap, gap-end call 5+6d starts the next dit.
     */
    const unsigned long logical_open[10] = {5U + 2U * d + 10U, 5U + 5U * d - 1U, 5U + 5U * d + 10U, 5U + 6U * d - 1U,
                                            5U + d - 1U,       5U + 2U * d - 1U, 5U + 2U * d + 1U,  5U + 6U * d + 1U,
                                            5U + 2U * d,       5U + 6U * d};
    unsigned i;
    unsigned swap;
    for (swap = 0U; swap <= 1U; ++swap) {
      for (i = 0U; i < 10U; ++i) {
        char a[32] = "";
        char b[32] = "";
        unsigned type;
        for (type = 1U; type <= 2U; ++type) {
          char* out = (type == 1U) ? a : b;
          setup();
          both_start(type, swap != 0U, wpms[w], wpms[w]);
          (void)run_both(1U);
          raw_tip = true;
          raw_ring = true;
          collect(logical_open[i] - 5U, out, 32U);
          raw_tip = false;
          raw_ring = false;
          collect(5U, out, 32U);
          TEST_ASSERT(!keyer_is_tip_closed && !keyer_is_ring_closed);
          collect(20U * d, out, 32U);
        }
        TEST_ASSERT_EQUAL(strlen(a) + 1U, strlen(b));
        TEST_ASSERT_EQUAL(0, strncmp(a, b, strlen(a)));
        TEST_ASSERT(b[strlen(a)] != a[strlen(a) - 1U]);
        /* Swapped: the tip gives dahs, so a squeeze from idle still starts with a dit (the ring). */
        TEST_ASSERT_EQUAL('.', a[0]);
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, mismatches);
}

/*
 * AC-K25 start latch (D-K2), checked on the real state after the start call of a gap-end element:
 * - both levers closed: the memory of the other lever and keyer_was_squeezed are set;
 * - only the lever of the new element closed: no memory and no squeeze.
 * Then the held-squeeze case of section 6.5: after a dit both levers are closed at the gap-end call, so a dah starts;
 * the dit lever opens logically on the next call and the dah lever stays closed. A dit follows the dah, in A and B.
 */
static void test_start_latch(void) {
  const unsigned wpm = 20U;
  const unsigned long d = model_dit(wpm);
  unsigned type;
  mismatches = 0U;
  for (type = 1U; type <= 2U; ++type) {
    char out[16] = "";
    /* Both closed through the gap-end call 5 + 2d, which starts the dah. */
    setup();
    both_start(type, false, wpm, wpm);
    (void)run_both(1U);
    raw_tip = true;
    raw_ring = true;
    (void)run_both(5U + 2U * d - 1U);
    TEST_ASSERT_EQUAL(KEYER_PHASE_GAP, keyer_phase);
    (void)run_both(1U);
    TEST_ASSERT_EQUAL(KEYER_PHASE_ELEMENT, keyer_phase);
    TEST_ASSERT(keyer_is_element_dah);
    TEST_ASSERT(keyer_has_dit_memory);
    TEST_ASSERT(keyer_was_squeezed);
    TEST_ASSERT(!keyer_has_dah_memory);

    /* Only the dah lever closed at the gap-end call of a dah repeat: the start call sets nothing. */
    setup();
    both_start(type, false, wpm, wpm);
    (void)run_both(1U);
    raw_ring = true;
    (void)run_both(5U + 4U * d - 1U);
    TEST_ASSERT_EQUAL(KEYER_PHASE_GAP, keyer_phase);
    (void)run_both(1U);
    TEST_ASSERT_EQUAL(KEYER_PHASE_ELEMENT, keyer_phase);
    TEST_ASSERT(keyer_is_element_dah);
    TEST_ASSERT(!keyer_has_dit_memory);
    TEST_ASSERT(!keyer_has_dah_memory);
    TEST_ASSERT(!keyer_was_squeezed);

    /* Held squeeze through the start call, dit lever logically open from the next call, dah lever held. */
    setup();
    both_start(type, false, wpm, wpm);
    (void)run_both(1U);
    raw_tip = true;
    raw_ring = true;
    collect(5U + 2U * d + 1U - 5U, out, sizeof(out));
    raw_tip = false;
    collect(5U, out, sizeof(out));
    TEST_ASSERT(!keyer_is_tip_closed);
    /* The dah lever stays closed to the end of the following dit, then opens. */
    collect(3U * d + 2U * d, out, sizeof(out));
    raw_ring = false;
    collect(20U * d, out, sizeof(out));
    TEST_ASSERT_EQUAL(0, strncmp(out, ".-.", 3U));
  }
  TEST_ASSERT_EQUAL(0U, mismatches);
}

/*
 * A single lever released on every call around the mark end: the dit lever held from call 1, its logical open on
 * each call from 10 calls before to 10 calls after the mark-end call (5 + d, entry phase ELEMENT). The GAP latch
 * sets the memory of a lever that is closed on any call with entry phase GAP (section 6.5), so the dit repeats
 * exactly when the lever is still closed on call 5 + d + 1. Iambic A and B.
 */
static void test_release_around_gap_end(void) {
  const unsigned wpm = 20U;
  const long d = model_dit(wpm);
  long offset;
  mismatches = 0U;
  for (offset = -10; offset <= 10; ++offset) {
    unsigned type;
    for (type = 1U; type <= 2U; ++type) {
      char out[16] = "";
      const long mark_end_call = 5 + d;
      const long logical_open = mark_end_call + offset;
      setup();
      both_start(type, false, wpm, wpm);
      (void)run_both(1U);
      raw_tip = true;
      collect((unsigned long)(logical_open - 5), out, sizeof(out));
      raw_tip = false;
      collect(5U, out, sizeof(out));
      collect((unsigned long)(10 * d), out, sizeof(out));
      if (!TEST_ASSERT_EQUAL((offset <= 1) ? 1U : 2U, strlen(out))) {
        fprintf(stderr, "type %u offset %ld: %s\n", type, offset, out);
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, mismatches);
}

/*
 * AC-K17 under a stalled reader: a held dit lever at 50 WPM gives one DIT per 192 calls. The reader stalls for
 * longer than 8 events, then drains: 8 events in order, then OVERFLOW once, then NONE. Events during the
 * overflow are dropped. After the drain, new events flow. Repeated with the queue indexes near the byte wrap.
 */
static void test_overflow_stalled_reader(void) {
  unsigned start_index;
  mismatches = 0U;
  for (start_index = 0U; start_index < 256U; start_index += 37U) {
    unsigned i;
    setup();
    keyer_queue_head = (uint8_t)start_index;
    keyer_queue_tail = (uint8_t)start_index;
    model.head = start_index;
    model.tail = start_index;
    both_start(KEYER_KEY_IAMBIC_B, false, 50U, 50U);
    (void)run_both(1U);
    raw_tip = true;
    (void)run_both(192U * 12U);
    TEST_ASSERT(keyer_is_overflow);
    TEST_ASSERT_EQUAL(8U, (uint8_t)(keyer_queue_head - keyer_queue_tail));
    for (i = 0U; i < 8U; ++i) {
      TEST_ASSERT_EQUAL(KEYER_EVENT_DIT, keyer_read_event());
      (void)model_read();
    }
    TEST_ASSERT_EQUAL(KEYER_EVENT_OVERFLOW, keyer_read_event());
    (void)model_read();
    TEST_ASSERT_EQUAL(KEYER_EVENT_NONE, keyer_read_event());
    TEST_ASSERT(!keyer_is_overflow);
    (void)run_both(192U * 3U);
    TEST_ASSERT_EQUAL(3U, (uint8_t)(keyer_queue_head - keyer_queue_tail));
    TEST_ASSERT_EQUAL(KEYER_EVENT_DIT, keyer_read_event());
    (void)model_read();
  }
  TEST_ASSERT_EQUAL(0U, mismatches);
}

/*
 * The read order across a full queue edge: with 7 events queued the reader reads one, the ISR pushes two more
 * (one fits, the next overflows). The reader then sees 8 events, OVERFLOW and NONE. Then an ISR push between the
 * last read and the overflow read is impossible to lose order: it is dropped while the flag is set.
 */
static void test_overflow_flag_while_draining(void) {
  unsigned i;
  mismatches = 0U;
  setup();
  both_start(KEYER_KEY_IAMBIC_B, false, 50U, 50U);
  (void)run_both(1U);
  raw_tip = true;
  /* 50 WPM: DIT events on calls 101 + 192 k. */
  (void)run_both(101U + 192U * 6U + 50U);
  TEST_ASSERT_EQUAL(7U, (uint8_t)(keyer_queue_head - keyer_queue_tail));
  TEST_ASSERT_EQUAL(KEYER_EVENT_DIT, keyer_read_event());
  (void)model_read();
  (void)run_both(192U * 3U);
  TEST_ASSERT(keyer_is_overflow);
  for (i = 0U; i < 8U; ++i) {
    TEST_ASSERT_EQUAL(KEYER_EVENT_DIT, keyer_read_event());
    (void)model_read();
    /* The ISR keeps running between reads; every event is dropped while the flag is set. */
    (void)run_both(192U);
  }
  TEST_ASSERT_EQUAL(KEYER_EVENT_OVERFLOW, keyer_read_event());
  (void)model_read();
  TEST_ASSERT_EQUAL(KEYER_EVENT_NONE, keyer_read_event());
  TEST_ASSERT_EQUAL(0U, mismatches);
}

/* AC-K16 extended: the word gap of every WPM pair 5..50, eff 5..wpm, and the clamp of every out-of-range pair. */
static void test_farnsworth_every_pair(void) {
  unsigned c;
  unsigned s;
  setup();
  for (c = 0U; c <= 60U; ++c) {
    for (s = 0U; s <= 60U; ++s) {
      both_start(KEYER_KEY_IAMBIC_B, false, c, s);
      TEST_ASSERT(keyer_word_gap_ticks >= 5U * keyer_dit_ticks);
      TEST_ASSERT(keyer_word_gap_ticks <= 11850U);
    }
  }
  both_stop();
}

/*
 * AC-K3 with bounce: a contact closed at the start that bounces (runs of 1..3 open samples, never 5 open of 8)
 * stays masked; no mark, no tone, no event for 8000 calls. Both contacts at once, all key types.
 */
static void test_entry_mask_with_bounce(void) {
  unsigned type;
  mismatches = 0U;
  rng_state = 99U;
  for (type = 0U; type <= 2U; ++type) {
    unsigned long call;
    setup();
    raw_tip = true;
    raw_ring = true;
    both_start(type, (type & 1U) != 0U, 20U, 20U);
    (void)run_both(1U);
    for (call = 0U; call < 8000U; ++call) {
      /* One open sample in every 4: the count never falls below 6. */
      raw_tip = (call % 4U) != 0U;
      raw_ring = (call % 4U) != 2U;
      (void)step();
      if (KEYER_IS_MARK_ACTIVE() || qa_is_tone_requested) {
        break;
      }
    }
    TEST_ASSERT_EQUAL(keyer_queue_tail, keyer_queue_head);
    TEST_ASSERT(keyer_is_tip_masked && keyer_is_ring_masked);
  }
  TEST_ASSERT_EQUAL(0U, mismatches);
}

int main(void) {
  TEST_RUN(test_random_against_model);
  TEST_RUN(test_squeeze_release_every_call);
  TEST_RUN(test_squeeze_release_named_boundaries);
  TEST_RUN(test_start_latch);
  TEST_RUN(test_release_around_gap_end);
  TEST_RUN(test_overflow_stalled_reader);
  TEST_RUN(test_overflow_flag_while_draining);
  TEST_RUN(test_farnsworth_every_pair);
  TEST_RUN(test_entry_mask_with_bounce);
  return TEST_FINISH();
}
