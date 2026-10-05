/*
 * AC-K14: the keyer drives the real sidetone envelope (references/internal/keyer_spec.md sections 6.5 and 10.3).
 * Each call runs keyer_service() and then sidetone_service(), as the Timer 0 ISR does. The compare 0 write of each
 * fall must come on the last ELEMENT call.
 */

#include <stdbool.h>
#include <stdint.h>

#include "fake_paddle_pins.h"
#include "fake_sidetone_pwm.h"
#include "sidetone/sidetone.h"
#include "test_assert.h"

#define KEYER_SIDETONE_ON() sidetone_set_on(true)
#define KEYER_SIDETONE_OFF() sidetone_set_on(false)
#include "cw/keyer_service.h"

#define MAX_ELEMENTS 16U

static size_t call_count;
static size_t element_starts[MAX_ELEMENTS];
static size_t element_ends[MAX_ELEMENTS]; /* last ELEMENT call */
static size_t element_count;
static size_t fall_zero_calls[MAX_ELEMENTS];
static size_t fall_zero_count;
/* Compare 0 writes inside an element after its rise ended: the tone dropped out. */
static size_t early_zero_count;

static bool is_compare_zero_write(void) {
  size_t index;
  for (index = 0U; index < fake_sidetone_pwm_event_count(); ++index) {
    const fake_sidetone_pwm_event_t event = fake_sidetone_pwm_event_at(index);
    if (((event.kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_SPEAKER) ||
         (event.kind == FAKE_SIDETONE_PWM_EVENT_COMPARE_HEADPHONES)) &&
        (event.value == 0U)) {
      return true;
    }
  }
  return false;
}

static void tick(void) {
  const uint8_t phase_before = keyer_phase;

  fake_sidetone_pwm_clear_log();
  keyer_service();
  sidetone_service();
  ++call_count;
  if ((keyer_phase == KEYER_PHASE_ELEMENT) && (phase_before != KEYER_PHASE_ELEMENT) && (element_count < MAX_ELEMENTS)) {
    element_starts[element_count] = call_count;
    ++element_count;
  }
  if ((phase_before == KEYER_PHASE_ELEMENT) && (keyer_phase != KEYER_PHASE_ELEMENT) && (element_count != 0U)) {
    element_ends[element_count - 1U] = call_count - 1U;
  }
  if (is_compare_zero_write()) {
    if (sidetone_state() == SIDETONE_STATE_ZERO_WAIT) {
      if (fall_zero_count < MAX_ELEMENTS) {
        fall_zero_calls[fall_zero_count] = call_count;
      }
      ++fall_zero_count;
    } else if ((keyer_phase == KEYER_PHASE_ELEMENT) && (element_count != 0U) &&
               (call_count > element_starts[element_count - 1U])) {
      ++early_zero_count;
    }
  }
}

static void run(size_t calls) {
  size_t index;
  for (index = 0U; index < calls; ++index) {
    tick();
  }
}

static void envelope_case(uint8_t wpm, bool is_headphones) {
  const uint16_t dit = (uint16_t)((4800U + (wpm / 2U)) / wpm);
  size_t index;

  fake_paddle_pins_reset();
  fake_sidetone_pwm_reset();
  fake_sidetone_pwm_set_headphones(is_headphones);
  sidetone_init(600U);
  keyer_init();
  keyer_start(KEYER_KEY_IAMBIC_B, false, wpm, wpm);
  call_count = 0U;
  element_count = 0U;
  fall_zero_count = 0U;
  early_zero_count = 0U;
  tick();

  /* A held squeeze of 5 elements: dit dah dit dah dit. */
  fake_paddle_pins_set_tip_closed(true);
  fake_paddle_pins_set_ring_closed(true);
  run(4U + (12U * dit) + 10U);
  fake_paddle_pins_set_tip_closed(false);
  fake_paddle_pins_set_ring_closed(false);
  run(8U * dit);

  TEST_ASSERT_EQUAL(6U, element_count);
  TEST_ASSERT_EQUAL(element_count, fall_zero_count);
  for (index = 0U; (index < element_count) && (index < fall_zero_count) && (index < MAX_ELEMENTS); ++index) {
    TEST_ASSERT_EQUAL(element_ends[index], fall_zero_calls[index]);
  }
  TEST_ASSERT_EQUAL(0U, early_zero_count);
  TEST_ASSERT_EQUAL(SIDETONE_STATE_OFF, sidetone_state());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_pwm_violation_count());
  keyer_stop();
}

static void test_fall_ends_on_last_element_call(void) {
  envelope_case(5U, false);
  envelope_case(5U, true);
  envelope_case(20U, false);
  envelope_case(20U, true);
  envelope_case(50U, false);
  envelope_case(50U, true);
}

int main(void) {
  TEST_RUN(test_fall_ends_on_last_element_call);
  return TEST_FINISH();
}
