/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "sidetone/sidetone.h"

#include "board/sidetone_config.h"
#include "board/sidetone_pwm.h"
#include "mcu/compiler.h"
#include "sidetone/sidetone_shared.h"
#include "tick/tick.h"

#define SIDETONE_TABLE_SIZE (SIDETONE_RAMP_STEPS + 1U)
/* The envelope position counts in 1/64 of a table step, so a byte increment gives every ramp of 4 to 12 ms. */
#define SIDETONE_POSITION_SHIFT 6U
#define SIDETONE_POSITION_FULL ((uint16_t)(SIDETONE_RAMP_STEPS << SIDETONE_POSITION_SHIFT)) /* 2048 */
#define SIDETONE_RAMP_COUNT (SIDETONE_RAMP_MAX_MS - SIDETONE_RAMP_MIN_MS + 1U)

/* Raised-cosine duty in permille of the period, 0 to 50 %. Source: sidetone_pwm_envelope_research.md. */
static const MEM_CODE uint16_t sidetone_duty_permille[] = {
    0U,   1U,   5U,   11U,  19U,  30U,  42U,  57U,  73U,  91U,  111U, 132U, 154U, 177U, 201U, 225U, 250U,
    275U, 299U, 323U, 346U, 368U, 389U, 409U, 427U, 443U, 458U, 470U, 481U, 489U, 495U, 499U, 500U};
_Static_assert(sizeof(sidetone_duty_permille) / sizeof(sidetone_duty_permille[0]) == SIDETONE_TABLE_SIZE,
               "the duty table needs SIDETONE_RAMP_STEPS + 1 values");

/*
 * Position increment per service call for the ramps 4..12 ms: ceil(2048 / (4 * N)) = ceil(512 / N). With the
 * rounding up, a full rise and a full fall each take exactly 4 * N calls (sidetone_spec.md section 6.8).
 */
static const MEM_CODE uint8_t sidetone_ramp_increments[] = {128U, 103U, 86U, 74U, 64U, 57U, 52U, 47U, 43U};
_Static_assert(sizeof(sidetone_ramp_increments) == SIDETONE_RAMP_COUNT, "one increment for each ramp in ms");

/* The Timer 0 interrupt writes it; the main loop reads it with one MOV, so the read is atomic. */
static volatile MEM_DATA uint8_t sidetone_state_value;
/* 0..SIDETONE_POSITION_FULL; in ZERO_WAIT it counts the wait calls. Timer 0 interrupt only. */
static MEM_DATA uint16_t sidetone_position;
/* The main loop writes it, the Timer 0 interrupt reads it once per call. One byte, so each access is atomic. */
static volatile MEM_DATA uint8_t sidetone_ramp_increment;
volatile MEM_DATA uint8_t sidetone_ramp_calls;
/* Shared with the Timer 0 interrupt. Each bit access is one instruction, so it is atomic. */
static volatile MEM_BIT sidetone_is_on_requested;
static volatile MEM_BIT sidetone_is_tone_pending;
static volatile MEM_BIT sidetone_is_table_b_active;
/* Also the debounced plug value: after init only sidetone_poll() writes it. */
static volatile MEM_BIT sidetone_is_headphones_requested;
static MEM_BIT sidetone_is_headphones_active;

/*
 * Two separate tables, so the interrupt never multiplies to index a 2-D array. Main writes only the inactive one.
 * Volatile, so C keeps the table and ARR stores before the volatile sidetone_is_tone_pending = true.
 */
static volatile MEM_XDATA uint16_t sidetone_table_a[SIDETONE_TABLE_SIZE];
static volatile MEM_XDATA uint16_t sidetone_table_b[SIDETONE_TABLE_SIZE];
static volatile MEM_XDATA uint16_t sidetone_arr_a;
static volatile MEM_XDATA uint16_t sidetone_arr_b;
static MEM_XDATA uint16_t sidetone_requested_hz;

static MEM_XDATA bool sidetone_is_plug_candidate_headphones;
static MEM_XDATA uint16_t sidetone_plug_change_time;

/* Main-loop work memory for the table fill: in xdata, so the 32-bit math costs no internal RAM. */
static MEM_XDATA uint32_t fill_period;
static MEM_XDATA uint8_t fill_step;

/*
 * Clamps the variable @p ramp_ms in place and writes its increment. A macro, not a function: sidetone_set_ramp_ms()
 * calls no function (sidetone_spec.md AC-30).
 */
#define SIDETONE_WRITE_INCREMENT(ramp_ms)                                                            \
  do {                                                                                               \
    if ((ramp_ms) < SIDETONE_RAMP_MIN_MS) {                                                          \
      (ramp_ms) = SIDETONE_RAMP_MIN_MS;                                                              \
    } else if ((ramp_ms) > SIDETONE_RAMP_MAX_MS) {                                                   \
      (ramp_ms) = SIDETONE_RAMP_MAX_MS;                                                              \
    }                                                                                                \
    sidetone_ramp_increment = sidetone_ramp_increments[(uint8_t)((ramp_ms) - SIDETONE_RAMP_MIN_MS)]; \
  } while (0)

static uint16_t sidetone_clamp(uint16_t tone_hz) {
  if (tone_hz < SIDETONE_TONE_MIN_HZ) {
    return SIDETONE_TONE_MIN_HZ;
  }
  if (tone_hz > SIDETONE_TONE_MAX_HZ) {
    return SIDETONE_TONE_MAX_HZ;
  }
  return tone_hz;
}

/*
 * Fills @p table for @p tone_hz and returns its ARR. period = round(SIDETONE_COUNTER_HZ / tone_hz) counts, ARR =
 * period - 1 (PWM mode 1: period = ARR + 1, datasheet section 20.5.7). compare[k] = period * duty[k] / 1000,
 * truncated; period * 500 < 2^32. The period stays 32-bit, so 65536 counts also work.
 */
static uint16_t sidetone_fill_table(volatile uint16_t MEM_XDATA* MEM_XDATA table, MEM_XDATA uint16_t tone_hz) {
  fill_period = (SIDETONE_COUNTER_HZ + ((uint32_t)tone_hz / 2UL)) / (uint32_t)tone_hz;
  for (fill_step = 0U; fill_step < SIDETONE_TABLE_SIZE; ++fill_step) {
    table[fill_step] = (uint16_t)((fill_period * sidetone_duty_permille[fill_step]) / (uint32_t)1000U);
    SIDETONE_PREEMPTION_POINT();
  }
  return (uint16_t)(fill_period - 1UL);
}

void sidetone_init(uint16_t tone_hz, MEM_XDATA uint8_t ramp_ms) {
  /* Cleared first: Timer 0 is not running yet, and a host test hook in the fill then sees an idle module. */
  sidetone_is_on_requested = false;
  sidetone_is_tone_pending = false;
  sidetone_is_table_b_active = false;
  sidetone_state_value = SIDETONE_STATE_OFF;
  sidetone_position = 0U;
  /* As sidetone_set_ramp_ms(), but with no preemption point: Timer 0 is not running yet. */
  SIDETONE_WRITE_INCREMENT(ramp_ms);
  sidetone_ramp_calls = (uint8_t)(ramp_ms << 2);
  sidetone_requested_hz = sidetone_clamp(tone_hz);
  sidetone_arr_a = sidetone_fill_table(sidetone_table_a, sidetone_requested_hz);

  SIDETONE_PWM_CONFIGURE(sidetone_arr_a);
  SIDETONE_PINS_CONFIGURE();

  /* The pull-up has just turned on, so this sample can be wrong; sidetone_poll() corrects it within the debounce. */
  sidetone_is_headphones_requested = SIDETONE_PINS_IS_HEADPHONES();
  sidetone_is_headphones_active = sidetone_is_headphones_requested;
  sidetone_is_plug_candidate_headphones = sidetone_is_headphones_requested;
  sidetone_plug_change_time = 0U;

  SIDETONE_TIMER_START();
}

void sidetone_set_tone_hz(uint16_t tone_hz) {
  const uint16_t clamped = sidetone_clamp(tone_hz);

  if (clamped == sidetone_requested_hz) {
    return;
  }
  /* While the flag is clear the interrupt never flips the selector, so the inactive table stays inactive. */
  sidetone_is_tone_pending = false;
  SIDETONE_PREEMPTION_POINT();
  sidetone_requested_hz = clamped;
  if (sidetone_is_table_b_active) {
    sidetone_arr_a = sidetone_fill_table(sidetone_table_a, clamped);
  } else {
    sidetone_arr_b = sidetone_fill_table(sidetone_table_b, clamped);
  }
  SIDETONE_PREEMPTION_POINT();
  sidetone_is_tone_pending = true;
  SIDETONE_PREEMPTION_POINT();
}

/*
 * Two byte stores, each atomic. An interrupt between them sees the new speed with the old call count; the fall
 * still ends in silence, because it needs no call count (sidetone_spec.md section 6.8).
 */
void sidetone_set_ramp_ms(uint8_t ramp_ms) {
  SIDETONE_WRITE_INCREMENT(ramp_ms);
  SIDETONE_PREEMPTION_POINT();
  /* 4 service calls per ms (checked in board/sidetone_config.h). */
  sidetone_ramp_calls = (uint8_t)(ramp_ms << 2);
  SIDETONE_PREEMPTION_POINT();
}

void sidetone_set_on(bool is_on) { sidetone_is_on_requested = is_on; }

bool sidetone_is_busy(void) {
  if (sidetone_state_value != SIDETONE_STATE_OFF) {
    return true;
  }
  return sidetone_is_on_requested;
}

sidetone_state_t sidetone_state(void) { return sidetone_state_value; }

void sidetone_poll(void) {
  const uint16_t now = tick_now();
  const bool is_raw_headphones = SIDETONE_PINS_IS_HEADPHONES();

  if (is_raw_headphones != sidetone_is_plug_candidate_headphones) {
    sidetone_is_plug_candidate_headphones = is_raw_headphones;
    sidetone_plug_change_time = now;
  } else if ((sidetone_is_plug_candidate_headphones != sidetone_is_headphones_requested) &&
             ((uint16_t)(now - sidetone_plug_change_time) >= SIDETONE_PLUG_DEBOUNCE_MS)) {
    sidetone_is_headphones_requested = sidetone_is_plug_candidate_headphones;
  }
}

/* The interrupt calls it, so its locals must not share the overlay with main-loop functions. */
#if defined(__SDCC_mcs51)
#pragma save
#pragma nooverlay
#endif
void sidetone_service(void) {
  const uint8_t state = sidetone_state_value;
  const uint8_t increment = sidetone_ramp_increment;
  uint16_t position = sidetone_position;
  uint16_t compare;
  uint8_t index;
  volatile const uint16_t MEM_XDATA* table;
  bool is_up;

  if (state == SIDETONE_STATE_OFF) {
    if (!sidetone_is_on_requested) {
      return;
    }
    if (sidetone_is_tone_pending) {
      sidetone_is_table_b_active = !sidetone_is_table_b_active;
      sidetone_is_tone_pending = false;
      /* compare holds the ARR here: SIDETONE_PWM_SET_ARR() reads its argument twice. */
      if (sidetone_is_table_b_active) {
        compare = sidetone_arr_b;
      } else {
        compare = sidetone_arr_a;
      }
      SIDETONE_PWM_SET_ARR(compare);
    }
    sidetone_is_headphones_active = sidetone_is_headphones_requested;
    /* UG loads ARR and the zero compare at once, so the output starts quiet. */
    if (sidetone_is_headphones_active) {
      SIDETONE_PWM_SET_COMPARE_HEADPHONES(0U);
      SIDETONE_PWM_GENERATE_UPDATE();
      SIDETONE_PWM_ENABLE_HEADPHONES();
    } else {
      SIDETONE_PWM_SET_COMPARE_SPEAKER(0U);
      SIDETONE_PWM_GENERATE_UPDATE();
      SIDETONE_PWM_ENABLE_SPEAKER();
    }
    sidetone_position = 0U;
    sidetone_state_value = SIDETONE_STATE_RISING;
    return;
  }

  if (state == SIDETONE_STATE_ZERO_WAIT) {
    /* The wait count stays below SIDETONE_ZERO_WAIT_LIMIT, so its low byte is enough. */
    index = (uint8_t)position;
    ++index;
    /* The wait also ends without the flag, so a stopped PWMA cannot keep the tone off for ever. */
    if (SIDETONE_PWM_IS_UPDATE_FLAG_SET() || (index >= SIDETONE_ZERO_WAIT_LIMIT)) {
      SIDETONE_PWM_DISABLE_OUTPUTS();
      index = 0U;
      sidetone_state_value = SIDETONE_STATE_OFF;
    }
    sidetone_position = index;
    return;
  }

  /* A pending tone acts as an off request: the new table applies only at the next start from OFF. */
  is_up = sidetone_is_on_requested && !sidetone_is_tone_pending;
  if (is_up) {
    if (position >= SIDETONE_POSITION_FULL) {
      sidetone_state_value = SIDETONE_STATE_STEADY;
      return;
    }
    position += increment;
    if (position >= SIDETONE_POSITION_FULL) {
      position = SIDETONE_POSITION_FULL;
      sidetone_state_value = SIDETONE_STATE_STEADY;
    } else {
      sidetone_state_value = SIDETONE_STATE_RISING;
    }
  } else {
    if (position == 0U) {
      /* Off right after the start call: UG already loaded compare 0. */
      SIDETONE_PWM_CLEAR_UPDATE_FLAG();
      sidetone_state_value = SIDETONE_STATE_ZERO_WAIT;
      return;
    }
    if (position > increment) {
      position -= increment;
      sidetone_state_value = SIDETONE_STATE_FALLING;
    } else {
      position = 0U;
      sidetone_state_value = SIDETONE_STATE_ZERO_WAIT;
    }
  }
  sidetone_position = position;

  /* position >> 6 from its two bytes; position is at most 2048, so the index fits a byte. */
  index = (uint8_t)((uint8_t)((uint8_t)(position >> 8) << 2) | (uint8_t)((uint8_t)position >> SIDETONE_POSITION_SHIFT));
  table = sidetone_is_table_b_active ? sidetone_table_b : sidetone_table_a;
  compare = table[index];
  if (sidetone_is_headphones_active) {
    SIDETONE_PWM_SET_COMPARE_HEADPHONES(compare);
  } else {
    SIDETONE_PWM_SET_COMPARE_SPEAKER(compare);
  }
  /*
   * Compare 0 first, then the flag clear: an update between the two then only loads the zero compare. The other
   * order lets an update set the flag while the shadow still holds a non-zero compare.
   */
  if (position == 0U) {
    SIDETONE_PWM_CLEAR_UPDATE_FLAG();
  }
}
#if defined(__SDCC_mcs51)
#pragma restore
#endif

#if defined(__SDCC_mcs51)
/* The ISR-side sidetone entry for the keyer and the player: one SETB or CLR (sidetone_spec.md section 3.10). */
#define KEYER_SIDETONE_ON() (sidetone_is_on_requested = true)
#define KEYER_SIDETONE_OFF() (sidetone_is_on_requested = false)
#include "cw/keyer_service.h"
#define PLAYER_SIDETONE_ON() (sidetone_is_on_requested = true)
#define PLAYER_SIDETONE_OFF() (sidetone_is_on_requested = false)
#include "cw/player_service.h"

/*
 * keyer_service(), player_service() and sidetone_service() are functions of this file, so SDCC ends the ISR with a
 * plain reti (decisions D-R2-1, D-K1 and training_spec.md D-T1). At most one of the keyer and the player is
 * enabled. Both run before the sidetone, so a request reaches the envelope in the same call. Hardware clears TF0
 * at the vector (datasheet section 13.2.1).
 */
void sidetone_timer0_isr(void) __interrupt(TIMER0_INTERRUPT) {
  keyer_service();
  player_service();
  sidetone_service();
}
#endif
