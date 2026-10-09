/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA preemption test. An interrupt can stop the software SPI between any two pin writes. The fake hook runs after
 * every pin write and plays the interrupt: it toggles a fake P2.5 latch that only the interrupt owns and adds
 * interrupt time. The hook never writes an LCD pin (the fake aborts if it does).
 *
 * Scope: lcd_init() followed by a full lcd_flush() of a pseudo-random framebuffer. For every event index k (see
 * DEFAULT_STRIDE) the sequence runs again with one interrupt after event k; the pin-event log, the delay calls, the
 * decoded bytes and the controller RAM must equal the run without interrupts. A last run has an interrupt after
 * every event.
 *
 * Host limits: the fake has no clock and keeps each pin apart, so this proves that the driver output does not
 * depend on interrupt position or duration. It cannot prove electrical timing or the absence of port-wide
 * read-modify-write; the cycle analysis and firmware/tools/check_asm.sh (run after every firmware build) cover those.
 */
#include <stdlib.h>
#include <string.h>

#include "board/lcd_config.h"
#include "fake_delay.h"
#include "fake_lcd_pins.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "lcd_controller_model.h"
#include "lcd_protocol_decoder.h"
#include "qa/qa_bus.h"
#include "support/test_assert.h"

#define NO_INTERRUPT ((size_t)-1)
#define MAX_DELAYS 8U
#define BYTE_CAPACITY 4096U
/* Runs that also decode and compare RAM; the event log comparison covers every run. */
#define DEEP_CHECK_STRIDE 2039U
/*
 * One run per event index costs about 1.3 ms, so by default every 13th k runs. 13 and the 24 events per byte have
 * no common factor, so the runs still hit every position inside a byte. QA_PREEMPTION_STRIDE=1 runs every k.
 */
#define DEFAULT_STRIDE 13U

typedef struct {
  fake_lcd_pin_event_t* events;
  size_t event_count;
  uint64_t event_hash;
  size_t delay_count;
  uint8_t delay_ms[MAX_DELAYS];
  size_t delay_event_index[MAX_DELAYS];
  qa_byte_t bytes[BYTE_CAPACITY];
  size_t byte_count;
  qa_ram_t ram;
  lcd_model_t model;
} run_record_t;

static run_record_t reference;
static run_record_t deep;

static size_t interrupt_at = NO_INTERRUPT;
static bool is_interrupt_on_every_event;
static size_t interrupt_count;
static uint8_t interrupt_p2_upper; /* P2.5-P2.7 latch bits that only the interrupt writes */
static unsigned long long interrupt_ticks;
static size_t hook_level_changes;

static uint8_t lcd_levels(void) {
  uint8_t levels = 0U;
  unsigned pin;
  for (pin = 0U; pin < (unsigned)FAKE_LCD_PIN_COUNT; ++pin) {
    levels = (uint8_t)(levels | (fake_lcd_pins_level((fake_lcd_pin_t)pin) << pin));
  }
  return levels;
}

static uint64_t event_hash;

/* FNV-1a over (pin, level) of every event, so a run compares in O(1) after it ends. */
static uint64_t hash_event(uint64_t hash, const fake_lcd_pin_event_t* event) {
  hash = (hash ^ (uint64_t)event->pin) * 0x100000001B3ULL;
  return (hash ^ (uint64_t)event->level) * 0x100000001B3ULL;
}

static void interrupt_hook(size_t event_index) {
  uint8_t before;
  event_hash = hash_event(event_hash, fake_lcd_pins_event(event_index));
  if (!is_interrupt_on_every_event && (event_index != interrupt_at)) {
    return;
  }
  before = lcd_levels();
  interrupt_p2_upper ^= 0x20U;
  /* A keyer or sidetone handler: 2 to 40 us at 24 MHz. Nothing in the driver can observe it. */
  interrupt_ticks += 48U + ((event_index % 20U) * 48U);
  ++interrupt_count;
  if (lcd_levels() != before) {
    ++hook_level_changes;
  }
}

static void draw_pattern(void) {
  uint32_t seed = 0x5EEDU;
  uint8_t page;
  size_t column;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    for (column = 0U; column < LCD_WIDTH; ++column) {
      seed = (seed * 1664525U) + 1013904223U;
      lcd_fb_page(page)[column] = (uint8_t)(seed >> 24);
    }
  }
}

static void run_sequence(void) {
  fake_lcd_pins_reset();
  fake_delay_reset();
  interrupt_count = 0U;
  event_hash = 0xCBF29CE484222325ULL;
  fake_lcd_pins_set_hook(interrupt_hook);
  draw_pattern();
  lcd_init();
  lcd_flush();
  fake_lcd_pins_set_hook(NULL);
}

static void record_delays(run_record_t* record) {
  size_t i;
  record->delay_count = fake_delay_count();
  for (i = 0U; (i < record->delay_count) && (i < MAX_DELAYS); ++i) {
    record->delay_ms[i] = fake_delay_ms_at(i);
    record->delay_event_index[i] = fake_delay_event_index_at(i);
  }
}

static void record_bus(run_record_t* record) {
  static lcd_decoded_byte_t developer_bytes[BYTE_CAPACITY];
  qa_bus_t bus;
  size_t violations = 99U;
  size_t developer_count;

  qa_bus_start(&bus, QA_ALL_HIGH, record->bytes, BYTE_CAPACITY);
  qa_bus_feed_events(&bus, 0U, fake_lcd_pins_event_count());
  TEST_ASSERT_EQUAL(0U, qa_rules_total(&bus.rules));
  TEST_ASSERT(bus.byte_count <= BYTE_CAPACITY);
  record->byte_count = (bus.byte_count <= BYTE_CAPACITY) ? bus.byte_count : BYTE_CAPACITY;
  qa_ram_reset(&record->ram, 0xA5U);
  qa_ram_apply(&record->ram, record->bytes, record->byte_count);
  TEST_ASSERT_EQUAL(0U, record->ram.out_of_ram_writes);

  developer_count = lcd_protocol_decode(0U, fake_lcd_pins_event_count(), developer_bytes, BYTE_CAPACITY, &violations);
  TEST_ASSERT_EQUAL(0U, violations);
  TEST_ASSERT_EQUAL(record->byte_count, developer_count);
  lcd_model_reset();
  lcd_model_apply(developer_bytes, (developer_count <= BYTE_CAPACITY) ? developer_count : BYTE_CAPACITY);
  memcpy(&record->model, lcd_model_state(), sizeof(record->model));
}

static bool are_delays_equal(const run_record_t* record) {
  size_t i;
  if (fake_delay_count() != record->delay_count) {
    return false;
  }
  for (i = 0U; (i < record->delay_count) && (i < MAX_DELAYS); ++i) {
    if ((fake_delay_ms_at(i) != record->delay_ms[i]) ||
        (fake_delay_event_index_at(i) != record->delay_event_index[i])) {
      return false;
    }
  }
  return true;
}

static bool are_events_equal(const run_record_t* record) {
  size_t i;
  if (fake_lcd_pins_event_count() != record->event_count) {
    return false;
  }
  for (i = 0U; i < record->event_count; ++i) {
    const fake_lcd_pin_event_t* event = fake_lcd_pins_event(i);
    if ((event->pin != record->events[i].pin) || (event->level != record->events[i].level)) {
      return false;
    }
  }
  return true;
}

static void compare_deep(void) {
  record_bus(&deep);
  TEST_ASSERT_EQUAL(reference.byte_count, deep.byte_count);
  TEST_ASSERT(memcmp(reference.bytes, deep.bytes, reference.byte_count * sizeof(reference.bytes[0])) == 0);
  TEST_ASSERT(memcmp(reference.ram.ram, deep.ram.ram, sizeof(reference.ram.ram)) == 0);
  TEST_ASSERT(memcmp(reference.model.ram, deep.model.ram, sizeof(reference.model.ram)) == 0);
}

static void test_reference_run(void) {
  size_t i;
  uint8_t page;
  size_t column;
  size_t mismatches = 0U;

  interrupt_at = NO_INTERRUPT;
  is_interrupt_on_every_event = false;
  run_sequence();
  TEST_ASSERT_EQUAL(0U, interrupt_count);
  reference.event_count = fake_lcd_pins_event_count();
  reference.events = malloc(reference.event_count * sizeof(*reference.events));
  if (!TEST_ASSERT(reference.events != NULL)) {
    exit(EXIT_FAILURE);
  }
  for (i = 0U; i < reference.event_count; ++i) {
    reference.events[i] = *fake_lcd_pins_event(i);
  }
  reference.event_hash = event_hash;
  record_delays(&reference);
  TEST_ASSERT_EQUAL(3U, reference.delay_count);
  record_bus(&reference);

  /* The uninterrupted run is itself correct: visible RAM equals the framebuffer. */
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    for (column = 0U; column < LCD_WIDTH; ++column) {
      mismatches += (reference.ram.ram[page][column] != lcd_fb_page(page)[column]) ? 1U : 0U;
    }
  }
  TEST_ASSERT_EQUAL(0U, mismatches);
  printf("  reference: %zu pin events, %zu bytes\n", reference.event_count, reference.byte_count);
}

static size_t interrupt_stride(void) {
  const char* text = getenv("QA_PREEMPTION_STRIDE");
  const long value = (text != NULL) ? strtol(text, NULL, 10) : 0L;
  return (value > 0L) ? (size_t)value : DEFAULT_STRIDE;
}

static void test_one_interrupt_at_every_event(void) {
  const size_t stride = interrupt_stride();
  size_t runs = 0U;
  size_t k;
  size_t event_mismatches = 0U;
  size_t delay_mismatches = 0U;
  size_t missed_interrupts = 0U;
  size_t deep_runs = 0U;

  is_interrupt_on_every_event = false;
  hook_level_changes = 0U;
  interrupt_p2_upper = 0U;
  for (k = 0U; k < reference.event_count; k += stride) {
    ++runs;
    interrupt_at = k;
    run_sequence();
    missed_interrupts += (interrupt_count == 1U) ? 0U : 1U;
    event_mismatches +=
        ((fake_lcd_pins_event_count() == reference.event_count) && (event_hash == reference.event_hash)) ? 0U : 1U;
    delay_mismatches += are_delays_equal(&reference) ? 0U : 1U;
    if (((k % DEEP_CHECK_STRIDE) < stride) || (k == (reference.event_count - 1U)) ||
        (k == reference.delay_event_index[0]) || (k == reference.delay_event_index[2])) {
      TEST_ASSERT(are_events_equal(&reference));
      compare_deep();
      ++deep_runs;
    }
  }
  interrupt_at = NO_INTERRUPT;
  TEST_ASSERT_EQUAL(0U, missed_interrupts);
  TEST_ASSERT_EQUAL(0U, event_mismatches);
  TEST_ASSERT_EQUAL(0U, delay_mismatches);
  TEST_ASSERT_EQUAL(0U, hook_level_changes);
  /* Each run toggled P2.5 once; nothing else wrote it. */
  TEST_ASSERT_EQUAL((runs % 2U) * 0x20U, interrupt_p2_upper);
  printf("  %zu single-interrupt runs (every %zu of %zu event indexes), %zu with decode and RAM compare\n", runs,
         stride, reference.event_count, deep_runs);
}

static void test_interrupt_after_every_event(void) {
  is_interrupt_on_every_event = true;
  hook_level_changes = 0U;
  interrupt_p2_upper = 0U;
  interrupt_ticks = 0U;
  run_sequence();
  is_interrupt_on_every_event = false;
  TEST_ASSERT_EQUAL(reference.event_count, interrupt_count);
  TEST_ASSERT(are_events_equal(&reference));
  TEST_ASSERT(are_delays_equal(&reference));
  TEST_ASSERT_EQUAL(0U, hook_level_changes);
  TEST_ASSERT(interrupt_ticks > 0U);
  compare_deep();
}

int main(void) {
  TEST_RUN(test_reference_run);
  TEST_RUN(test_one_interrupt_at_every_event);
  TEST_RUN(test_interrupt_after_every_event);
  free(reference.events);
  return TEST_FINISH();
}
