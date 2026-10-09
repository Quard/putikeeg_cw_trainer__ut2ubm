/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CW_KEYER_SERVICE_H
#define CW_KEYER_SERVICE_H

/*
 * ISR side of the keyer (references/internal/keyer_spec.md decision D-K1 and section 6). On the target only
 * sidetone.c includes this header, so keyer_service() is a function of the Timer 0 interrupt file. Every includer
 * defines KEYER_SIDETONE_ON() and KEYER_SIDETONE_OFF() first and calls keyer_service().
 */

#if !defined(KEYER_SIDETONE_ON) || !defined(KEYER_SIDETONE_OFF)
#error "Define KEYER_SIDETONE_ON() and KEYER_SIDETONE_OFF() before including cw/keyer_service.h"
#endif

#include <stdbool.h>
#include <stdint.h>

#include "board/paddle_pins.h"
#include "cw/keyer.h"
#include "cw/keyer_shared.h"
#include "mcu/compiler.h"

/* ISR-private state. Host tests read it, so the names are part of the contract. */
static MEM_DATA keyer_phase_t keyer_phase;
/* ELEMENT and GAP: calls left. STRAIGHT_DOWN: mark calls so far, saturated at 0xFFFF. */
static MEM_DATA uint16_t keyer_ticks;
/* Calls since the last mark end, while a gap event is pending. */
static MEM_DATA uint16_t keyer_space_ticks;
/* Last 8 samples, bit 0 the newest, 1 = closed; the count is the number of 1 bits. */
static MEM_DATA uint8_t keyer_tip_history;
static MEM_DATA uint8_t keyer_ring_history;
static MEM_DATA uint8_t keyer_tip_count;
static MEM_DATA uint8_t keyer_ring_count;
static MEM_BIT keyer_is_tip_closed;
static MEM_BIT keyer_is_ring_closed;
/* A contact closed at the start stays ignored until it votes open. */
static MEM_BIT keyer_is_tip_masked;
static MEM_BIT keyer_is_ring_masked;
static MEM_BIT keyer_has_dit_memory;
static MEM_BIT keyer_has_dah_memory;
/* Both logical contacts were closed at one call since the last decision. */
static MEM_BIT keyer_was_squeezed;
static MEM_BIT keyer_is_element_dah;
static MEM_BIT keyer_is_char_gap_pending;
static MEM_BIT keyer_is_word_gap_pending;

#define KEYER_LEVER_DIT 0x01U
#define KEYER_LEVER_DAH 0x02U
#define KEYER_LEVERS_BOTH (KEYER_LEVER_DIT | KEYER_LEVER_DAH)

/* The logical mark. A later key output uses it, never the sidetone request. */
#define KEYER_IS_MARK_ACTIVE() ((keyer_phase == KEYER_PHASE_ELEMENT) || (keyer_phase == KEYER_PHASE_STRAIGHT_DOWN))

/* The interrupt calls it, so its locals must not share the overlay with main-loop functions. */
#if defined(__SDCC_mcs51)
#pragma save
#pragma nooverlay
#endif
/** Runs one keyer step: filters, phases, sidetone request, gap detection and at most one event. */
static void keyer_service(void) {
  /* Raw samples, 1 = closed. Bytes, so SDCC keeps them in registers. */
  uint8_t tip_sample;
  uint8_t ring_sample;
  /* Logical contacts as KEYER_LEVER_* bits. */
  uint8_t levers;
  uint8_t wanted;
  /* The element to start in this call, KEYER_LEVER_DIT or KEYER_LEVER_DAH, or 0. */
  uint8_t start;
  keyer_event_t event;

  if (!keyer_is_enabled) {
    return;
  }
  tip_sample = PADDLE_PINS_IS_TIP_CLOSED() ? 1U : 0U;
  ring_sample = PADDLE_PINS_IS_RING_CLOSED() ? 1U : 0U;

  if (keyer_is_start_requested) {
    keyer_tip_history = 0x00U;
    keyer_tip_count = 0U;
    keyer_is_tip_closed = false;
    if (tip_sample != 0U) {
      keyer_tip_history = 0xFFU;
      keyer_tip_count = 8U;
      keyer_is_tip_closed = true;
    }
    keyer_is_tip_masked = keyer_is_tip_closed;
    keyer_ring_history = 0x00U;
    keyer_ring_count = 0U;
    keyer_is_ring_closed = false;
    if (ring_sample != 0U) {
      keyer_ring_history = 0xFFU;
      keyer_ring_count = 8U;
      keyer_is_ring_closed = true;
    }
    keyer_is_ring_masked = keyer_is_ring_closed;
    keyer_phase = KEYER_PHASE_IDLE;
    keyer_ticks = 0U;
    keyer_space_ticks = 0U;
    keyer_has_dit_memory = false;
    keyer_has_dah_memory = false;
    keyer_was_squeezed = false;
    keyer_is_element_dah = false;
    keyer_is_char_gap_pending = false;
    keyer_is_word_gap_pending = false;
    KEYER_SIDETONE_OFF();
    keyer_is_start_requested = false;
    return;
  }

  /* Majority of the last 8 samples; a count of 4 keeps the vote, so a settled state needs 5 opposite samples. */
  if ((keyer_tip_history & 0x80U) != 0U) {
    --keyer_tip_count;
  }
  keyer_tip_history = (uint8_t)(keyer_tip_history << 1);
  if (tip_sample != 0U) {
    keyer_tip_history |= 0x01U;
    ++keyer_tip_count;
  }
  if (keyer_tip_count >= KEYER_FILTER_CLOSED_MIN) {
    keyer_is_tip_closed = true;
  } else if (keyer_tip_count <= KEYER_FILTER_OPEN_MAX) {
    keyer_is_tip_closed = false;
  }
  if (!keyer_is_tip_closed) {
    keyer_is_tip_masked = false;
  }

  if ((keyer_ring_history & 0x80U) != 0U) {
    --keyer_ring_count;
  }
  keyer_ring_history = (uint8_t)(keyer_ring_history << 1);
  if (ring_sample != 0U) {
    keyer_ring_history |= 0x01U;
    ++keyer_ring_count;
  }
  if (keyer_ring_count >= KEYER_FILTER_CLOSED_MIN) {
    keyer_is_ring_closed = true;
  } else if (keyer_ring_count <= KEYER_FILTER_OPEN_MAX) {
    keyer_is_ring_closed = false;
  }
  if (!keyer_is_ring_closed) {
    keyer_is_ring_masked = false;
  }

  levers = 0U;
  if (keyer_is_tip_closed && !keyer_is_tip_masked) {
    levers = keyer_is_swapped ? KEYER_LEVER_DAH : KEYER_LEVER_DIT;
  }
  if (keyer_is_ring_closed && !keyer_is_ring_masked) {
    levers |= keyer_is_swapped ? KEYER_LEVER_DIT : KEYER_LEVER_DAH;
  }

  event = KEYER_EVENT_NONE;
  start = 0U;
  if (keyer_phase == KEYER_PHASE_IDLE) {
    if (levers == 0U) {
      /* Nothing to do. */
    } else if (keyer_is_straight) {
      keyer_phase = KEYER_PHASE_STRAIGHT_DOWN;
      keyer_ticks = 1U;
      KEYER_SIDETONE_ON();
      keyer_is_char_gap_pending = false;
      keyer_is_word_gap_pending = false;
    } else {
      /* A squeeze from idle starts with a dit. */
      start = ((levers & KEYER_LEVER_DIT) != 0U) ? KEYER_LEVER_DIT : KEYER_LEVER_DAH;
    }
  } else if (keyer_phase == KEYER_PHASE_STRAIGHT_DOWN) {
    if (levers != 0U) {
      if (keyer_ticks != 0xFFFFU) {
        ++keyer_ticks;
      }
    } else {
      KEYER_SIDETONE_OFF();
      event = (keyer_ticks >= keyer_two_dit_ticks) ? KEYER_EVENT_DAH : KEYER_EVENT_DIT;
      keyer_phase = KEYER_PHASE_IDLE;
    }
  } else {
    /* ELEMENT latches only the other lever; GAP latches both. */
    if (((levers & KEYER_LEVER_DIT) != 0U) && ((keyer_phase == KEYER_PHASE_GAP) || keyer_is_element_dah)) {
      keyer_has_dit_memory = true;
    }
    if (((levers & KEYER_LEVER_DAH) != 0U) && ((keyer_phase == KEYER_PHASE_GAP) || !keyer_is_element_dah)) {
      keyer_has_dah_memory = true;
    }
    if (levers == KEYER_LEVERS_BOTH) {
      keyer_was_squeezed = true;
    }
    --keyer_ticks;
    if (keyer_phase == KEYER_PHASE_ELEMENT) {
      if (keyer_ticks == 0U) {
        event = keyer_is_element_dah ? KEYER_EVENT_DAH : KEYER_EVENT_DIT;
        keyer_ticks = keyer_dit_ticks;
        keyer_phase = KEYER_PHASE_GAP;
      } else if (keyer_ticks <= KEYER_EARLY_RELEASE_TICKS) {
        /* The 32-call fall then writes compare 0 on the last ELEMENT call. */
        KEYER_SIDETONE_OFF();
      }
    } else if (keyer_ticks == 0U) {
      /* Gap end. A lever is wanted when it is closed now or its memory is set. */
      wanted = levers;
      if (keyer_has_dit_memory) {
        wanted |= KEYER_LEVER_DIT;
      }
      if (keyer_has_dah_memory) {
        wanted |= KEYER_LEVER_DAH;
      }
      keyer_phase = KEYER_PHASE_IDLE;
      if (keyer_is_iambic_a && keyer_was_squeezed && (levers == 0U)) {
        /* Iambic A: a released squeeze adds no trailing element. */
      } else if ((wanted & (keyer_is_element_dah ? KEYER_LEVER_DIT : KEYER_LEVER_DAH)) != 0U) {
        start = keyer_is_element_dah ? KEYER_LEVER_DIT : KEYER_LEVER_DAH;
      } else if (wanted != 0U) {
        start = keyer_is_element_dah ? KEYER_LEVER_DAH : KEYER_LEVER_DIT;
      }
      keyer_has_dit_memory = false;
      keyer_has_dah_memory = false;
      keyer_was_squeezed = false;
    }
  }
  if (start != 0U) {
    keyer_is_element_dah = (start == KEYER_LEVER_DAH);
    /* The start call is the first element call, so it latches like one (decision D-K2). */
    if ((levers & (keyer_is_element_dah ? KEYER_LEVER_DIT : KEYER_LEVER_DAH)) != 0U) {
      if (keyer_is_element_dah) {
        keyer_has_dit_memory = true;
      } else {
        keyer_has_dah_memory = true;
      }
    }
    if (levers == KEYER_LEVERS_BOTH) {
      keyer_was_squeezed = true;
    }
    keyer_phase = KEYER_PHASE_ELEMENT;
    keyer_ticks = keyer_is_element_dah ? keyer_dah_ticks : keyer_dit_ticks;
    KEYER_SIDETONE_ON();
    keyer_is_char_gap_pending = false;
    keyer_is_word_gap_pending = false;
  }
  if (event != KEYER_EVENT_NONE) {
    /* Mark end: the space starts here. */
    keyer_space_ticks = 0U;
    keyer_is_char_gap_pending = true;
    keyer_is_word_gap_pending = true;
  }

  if (keyer_is_word_gap_pending) {
    ++keyer_space_ticks;
    if (keyer_is_char_gap_pending && (keyer_space_ticks == keyer_two_dit_ticks)) {
      event = KEYER_EVENT_CHAR_GAP;
      keyer_is_char_gap_pending = false;
    }
    if (keyer_space_ticks == keyer_word_gap_ticks) {
      event = KEYER_EVENT_WORD_GAP;
      keyer_is_word_gap_pending = false;
    }
  }

  /* One push site: keyer_spec.md section 6.8 proves that one call never has two events. */
  if ((event != KEYER_EVENT_NONE) && !keyer_is_overflow) {
    if ((uint8_t)(keyer_queue_head - keyer_queue_tail) >= KEYER_QUEUE_SIZE) {
      keyer_is_overflow = true;
    } else {
      keyer_queue[keyer_queue_head & KEYER_QUEUE_MASK] = event;
      ++keyer_queue_head;
    }
  }
}
#if defined(__SDCC_mcs51)
#pragma restore
#endif

#endif /* CW_KEYER_SERVICE_H */
