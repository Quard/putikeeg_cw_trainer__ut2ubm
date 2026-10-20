/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CW_KEYER_SHARED_H
#define CW_KEYER_SHARED_H

/*
 * State that the main loop shares with the keyer in the Timer 0 interrupt (references/internal/keyer_spec.md
 * sections 5.5 and 7). keyer.c defines it. Each access is one bit or one byte, so it is atomic. The main loop
 * writes the parameters and the 16-bit tick values only while keyer_is_enabled is 0.
 */

#include <stdint.h>

#include "board/sidetone_config.h"
#include "mcu/compiler.h"
#include "sidetone/sidetone_shared.h"

typedef uint8_t keyer_phase_t;
#define KEYER_PHASE_IDLE 0U
#define KEYER_PHASE_STRAIGHT_DOWN 1U
#define KEYER_PHASE_ELEMENT 2U
#define KEYER_PHASE_GAP 3U

#define KEYER_SERVICE_HZ SIDETONE_SERVICE_HZ /* the keyer runs in the sidetone Timer 0 ISR */
#define KEYER_FILTER_CLOSED_MIN 5U           /* closed samples of 8 that vote closed */
#define KEYER_FILTER_OPEN_MAX 3U             /* closed samples of 8 at or below which it votes open */
#define KEYER_QUEUE_SIZE 8U                  /* power of 2, divides 256 */
#define KEYER_QUEUE_MASK (KEYER_QUEUE_SIZE - 1U)
#define KEYER_EARLY_RELEASE_TICKS sidetone_ramp_calls /* 4 x Ramp ms: the fall ends at the mark end */
#define KEYER_CHAR_GAP_DITS 2U
#define KEYER_WORD_GAP_UNITS 5U

#if (KEYER_QUEUE_SIZE == 0U) || ((KEYER_QUEUE_SIZE & (KEYER_QUEUE_SIZE - 1U)) != 0U) || (KEYER_QUEUE_SIZE > 128U)
#error "KEYER_QUEUE_SIZE must be a power of 2 and at most 128"
#endif

/* Shared state, defined in keyer.c. Writers: keyer_spec.md section 7. */
extern volatile MEM_BIT keyer_is_enabled;
extern volatile MEM_BIT keyer_is_start_requested;
extern volatile MEM_BIT keyer_is_overflow;
extern volatile MEM_BIT keyer_is_straight;
extern volatile MEM_BIT keyer_is_iambic_a;
extern volatile MEM_BIT keyer_is_swapped;
extern volatile MEM_DATA uint8_t keyer_queue_head;
extern volatile MEM_DATA uint8_t keyer_queue_tail;
extern volatile MEM_XDATA uint8_t keyer_queue[KEYER_QUEUE_SIZE];
extern volatile MEM_XDATA uint16_t keyer_dit_ticks;
extern volatile MEM_XDATA uint16_t keyer_dah_ticks;
extern volatile MEM_XDATA uint16_t keyer_two_dit_ticks; /* straight dah threshold and char gap */
extern volatile MEM_XDATA uint16_t keyer_word_gap_ticks;

#endif /* CW_KEYER_SHARED_H */
