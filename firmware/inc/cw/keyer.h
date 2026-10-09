/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CW_KEYER_H
#define CW_KEYER_H

/*
 * Paddle and straight-key keyer (references/internal/keyer_spec.md). The Timer 0 interrupt runs the keyer; these
 * functions are for the main loop only. The keyer drives the sidetone and gives element and gap events.
 */

#include <stdbool.h>
#include <stdint.h>

#include "cw/timing.h"
#include "mcu/compiler.h"

typedef uint8_t keyer_event_t;

#define KEYER_EVENT_NONE 0U
#define KEYER_EVENT_DIT 1U      /* end of a dit mark */
#define KEYER_EVENT_DAH 2U      /* end of a dah mark */
#define KEYER_EVENT_CHAR_GAP 3U /* space reached KEYER_CHAR_GAP_DITS dits */
#define KEYER_EVENT_WORD_GAP 4U /* space reached KEYER_WORD_GAP_UNITS Farnsworth units */
#define KEYER_EVENT_OVERFLOW 5U /* events were lost after the events before it */

#define KEYER_KEY_STRAIGHT 0U /* equal to SETTINGS_KEY_STRAIGHT */
#define KEYER_KEY_IAMBIC_A 1U /* equal to SETTINGS_KEY_IAMBIC_A */
#define KEYER_KEY_IAMBIC_B 2U /* equal to SETTINGS_KEY_IAMBIC_B */

#define KEYER_WPM_MIN CW_TIMING_WPM_MIN /* equal to settings_min(SETTINGS_ID_TX_WPM) */
#define KEYER_WPM_MAX CW_TIMING_WPM_MAX /* equal to settings_max(SETTINGS_ID_TX_WPM) */

/** Sets up the ring pin and an empty queue, and leaves the keyer stopped. Call once, after sidetone_init(). */
void keyer_init(void);

/**
 * Stops the keyer, sets the new parameters and starts it. The interrupt reseeds the contacts on its next call, so
 * a contact that is closed now gives nothing until it opens.
 * @param key_type KEYER_KEY_*; a higher value gives KEYER_KEY_IAMBIC_B.
 * @param is_swapped True: the ring gives dits and the tip gives dahs.
 * @param wpm Speed, clamped to KEYER_WPM_MIN..KEYER_WPM_MAX.
 * @param eff_wpm Farnsworth speed of the word gap, clamped to KEYER_WPM_MIN..wpm.
 */
void keyer_start(uint8_t key_type, MEM_XDATA bool is_swapped, MEM_XDATA uint8_t wpm, MEM_XDATA uint8_t eff_wpm);

/** Stops the keyer and requests the sidetone off. No event comes after it. A second call has no other effect. */
void keyer_stop(void);

/** Returns the oldest event, then KEYER_EVENT_OVERFLOW once after a loss, else KEYER_EVENT_NONE. */
keyer_event_t keyer_read_event(void);

#endif /* CW_KEYER_H */
