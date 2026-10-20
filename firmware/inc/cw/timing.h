/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CW_TIMING_H
#define CW_TIMING_H

/*
 * Dit and Farnsworth unit in Timer 0 calls, for the keyer and the CW player (references/internal/training_spec.md
 * section 5.3, formulas of keyer_spec.md section 6.7). Main loop only.
 */

#include <stdint.h>

#include "board/sidetone_config.h"
#include "mcu/compiler.h"

#define CW_TIMING_WPM_MIN 5U
#define CW_TIMING_WPM_MAX 50U
#define CW_TIMING_TICKS_HZ SIDETONE_SERVICE_HZ /* the keyer and the player run in the Timer 0 ISR */
#define CW_TIMING_DIT_NUMERATOR 4800UL         /* CW_TIMING_TICKS_HZ * 1.2 s */
#define CW_TIMING_FARNSWORTH_C_FACTOR 240000UL /* CW_TIMING_TICKS_HZ * 60 */
#define CW_TIMING_FARNSWORTH_S_FACTOR 148800UL /* CW_TIMING_TICKS_HZ * 37.2 */
#define CW_TIMING_FARNSWORTH_DIVISOR 19UL

#if CW_TIMING_TICKS_HZ != 4000U
#error "The CW timing factors assume 4000 Timer 0 calls per second"
#endif
/* The early release comes after the start call, the fall ends by the mark end, and ZERO_WAIT ends in the gap. */
#if ((CW_TIMING_DIT_NUMERATOR + (CW_TIMING_WPM_MAX / 2U)) / CW_TIMING_WPM_MAX) < (2U * SIDETONE_RAMP_MAX_CALLS)
#error "The dit at CW_TIMING_WPM_MAX must be at least two of the longest sidetone ramps"
#endif
#if ((CW_TIMING_DIT_NUMERATOR + (CW_TIMING_WPM_MAX / 2U)) / CW_TIMING_WPM_MAX) <= SIDETONE_ZERO_WAIT_CALLS
#error "The dit at CW_TIMING_WPM_MAX must be longer than the sidetone ZERO_WAIT limit"
#endif

/* Results of the last cw_timing_compute() call, in Timer 0 calls. Main loop only. */
extern MEM_XDATA uint16_t cw_timing_dit_ticks;  /* round(4800 / wpm), 960..96 */
extern MEM_XDATA uint16_t cw_timing_unit_ticks; /* Farnsworth unit, at least cw_timing_dit_ticks, at most 2370 */

/**
 * Computes the dit and the Farnsworth unit. @p wpm is clamped to CW_TIMING_WPM_MIN..CW_TIMING_WPM_MAX, then
 * @p eff_wpm to CW_TIMING_WPM_MIN..wpm.
 */
void cw_timing_compute(uint8_t wpm, MEM_XDATA uint8_t eff_wpm);

#endif /* CW_TIMING_H */
