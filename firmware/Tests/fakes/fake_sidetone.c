/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "fake_sidetone.h"

#include "sidetone/sidetone.h"

static fake_sidetone_call_t fake_log[FAKE_SIDETONE_LOG_SIZE];
static size_t fake_log_count;
static bool fake_is_log_overflowed;
static uint16_t fake_last_tone;
static bool fake_is_last_on;
static bool fake_is_busy;
static size_t fake_polls;
static size_t fake_services;

static void fake_record(uint8_t kind, uint16_t value) {
  if (fake_log_count >= FAKE_SIDETONE_LOG_SIZE) {
    fake_is_log_overflowed = true;
    return;
  }
  fake_log[fake_log_count].kind = kind;
  fake_log[fake_log_count].value = value;
  ++fake_log_count;
}

void fake_sidetone_reset(void) {
  fake_log_count = 0U;
  fake_is_log_overflowed = false;
  fake_last_tone = 0U;
  fake_is_last_on = false;
  fake_is_busy = false;
  fake_polls = 0U;
  fake_services = 0U;
}

void fake_sidetone_set_busy(bool is_busy) { fake_is_busy = is_busy; }
size_t fake_sidetone_call_count(void) { return fake_log_count; }

fake_sidetone_call_t fake_sidetone_call_at(size_t index) {
  const fake_sidetone_call_t none = {0U, 0U};
  return (index < fake_log_count) ? fake_log[index] : none;
}

uint16_t fake_sidetone_last_tone(void) { return fake_last_tone; }
bool fake_sidetone_last_on(void) { return fake_is_last_on; }
size_t fake_sidetone_poll_count(void) { return fake_polls; }
size_t fake_sidetone_service_count(void) { return fake_services; }
bool fake_sidetone_is_log_overflowed(void) { return fake_is_log_overflowed; }

void sidetone_init(uint16_t tone_hz) { fake_record(FAKE_SIDETONE_CALL_INIT, tone_hz); }

void sidetone_set_tone_hz(uint16_t tone_hz) {
  fake_last_tone = tone_hz;
  fake_record(FAKE_SIDETONE_CALL_TONE, tone_hz);
}

void sidetone_set_on(bool is_on) {
  fake_is_last_on = is_on;
  fake_record(FAKE_SIDETONE_CALL_ON, is_on ? 1U : 0U);
}

bool sidetone_is_busy(void) { return fake_is_busy; }

sidetone_state_t sidetone_state(void) { return fake_is_busy ? SIDETONE_STATE_STEADY : SIDETONE_STATE_OFF; }

void sidetone_poll(void) { ++fake_polls; }

void sidetone_service(void) { ++fake_services; }
