/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fake_keyer.h"

static keyer_event_t fake_fifo[FAKE_KEYER_FIFO_SIZE];
static size_t fake_head;
static size_t fake_tail;
static bool fake_is_overflowed;
static size_t fake_inits;
static size_t fake_starts;
static size_t fake_stops;
static size_t fake_reads;
static bool fake_is_running;
static uint8_t fake_key_type;
static bool fake_is_swapped;
static uint8_t fake_wpm;
static uint8_t fake_eff_wpm;
static void (*fake_start_hook)(void);
static void (*fake_stop_hook)(void);

void fake_keyer_reset(void) {
  fake_head = 0U;
  fake_tail = 0U;
  fake_is_overflowed = false;
  fake_inits = 0U;
  fake_starts = 0U;
  fake_stops = 0U;
  fake_reads = 0U;
  fake_is_running = false;
  fake_key_type = 0xFFU;
  fake_is_swapped = false;
  fake_wpm = 0U;
  fake_eff_wpm = 0U;
  fake_start_hook = NULL;
  fake_stop_hook = NULL;
}

void fake_keyer_set_start_hook(void (*hook)(void)) { fake_start_hook = hook; }

void fake_keyer_set_stop_hook(void (*hook)(void)) { fake_stop_hook = hook; }

void fake_keyer_push_event(keyer_event_t event) {
  if ((fake_head - fake_tail) >= FAKE_KEYER_FIFO_SIZE) {
    fake_is_overflowed = true;
    return;
  }
  fake_fifo[fake_head % FAKE_KEYER_FIFO_SIZE] = event;
  ++fake_head;
}

size_t fake_keyer_init_count(void) { return fake_inits; }
size_t fake_keyer_start_count(void) { return fake_starts; }
size_t fake_keyer_stop_count(void) { return fake_stops; }
bool fake_keyer_is_running(void) { return fake_is_running; }
uint8_t fake_keyer_last_key_type(void) { return fake_key_type; }
bool fake_keyer_last_is_swapped(void) { return fake_is_swapped; }
uint8_t fake_keyer_last_wpm(void) { return fake_wpm; }
uint8_t fake_keyer_last_eff_wpm(void) { return fake_eff_wpm; }
size_t fake_keyer_read_count(void) { return fake_reads; }
size_t fake_keyer_pending_count(void) { return fake_head - fake_tail; }
bool fake_keyer_is_overflowed(void) { return fake_is_overflowed; }

void keyer_init(void) { ++fake_inits; }

void keyer_start(uint8_t key_type, bool is_swapped, uint8_t wpm, uint8_t eff_wpm) {
  ++fake_starts;
  fake_is_running = true;
  fake_key_type = key_type;
  fake_is_swapped = is_swapped;
  fake_wpm = wpm;
  fake_eff_wpm = eff_wpm;
  if (fake_start_hook != NULL) {
    fake_start_hook();
  }
}

void keyer_stop(void) {
  ++fake_stops;
  fake_is_running = false;
  if (fake_stop_hook != NULL) {
    fake_stop_hook();
  }
}

keyer_event_t keyer_read_event(void) {
  keyer_event_t event;

  ++fake_reads;
  if (fake_tail == fake_head) {
    return KEYER_EVENT_NONE;
  }
  event = fake_fifo[fake_tail % FAKE_KEYER_FIFO_SIZE];
  ++fake_tail;
  return event;
}
