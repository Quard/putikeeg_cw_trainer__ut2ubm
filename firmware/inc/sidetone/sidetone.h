/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef SIDETONE_SIDETONE_H
#define SIDETONE_SIDETONE_H

/*
 * CW sidetone on PWMA with a click-free envelope. The Timer 0 interrupt runs sidetone_service() at 4 kHz; all other
 * functions are for the main loop only. The route follows the plug (P1.0 LOW: headphones on P1.2, else the speaker
 * on P1.1) and changes only at silence. Source: references/internal/sidetone_spec.md.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#if defined(__SDCC_mcs51)
#include "mcu/stc8h.h"
#endif

typedef uint8_t sidetone_state_t;

#define SIDETONE_STATE_OFF 0U /* output disabled, confirmed at zero */
#define SIDETONE_STATE_RISING 1U
#define SIDETONE_STATE_STEADY 2U /* position at full scale, 50 % duty */
#define SIDETONE_STATE_FALLING 3U
#define SIDETONE_STATE_ZERO_WAIT 4U /* compare 0 written, waits for the PWMA update flag */

/**
 * Sets up the tone tables, PWMA, the audio pins and the plug route, then starts Timer 0. Stays silent and does not
 * change EA. Call it once, at start-up.
 * @param tone_hz Start tone, clamped to SIDETONE_TONE_MIN_HZ..SIDETONE_TONE_MAX_HZ.
 * @param ramp_ms Start rise and fall time, as sidetone_set_ramp_ms() takes it.
 */
void sidetone_init(uint16_t tone_hz, MEM_XDATA uint8_t ramp_ms);

/**
 * Requests a new tone. It sounds after the next silence. A repeated request with the same clamped value does
 * nothing.
 * @param tone_hz Tone in Hz, clamped to SIDETONE_TONE_MIN_HZ..SIDETONE_TONE_MAX_HZ.
 */
void sidetone_set_tone_hz(uint16_t tone_hz);

/**
 * Sets the rise and fall time. A ramp that runs goes on at the new speed; nothing restarts. Call it only while the
 * keyer and the CW player are disabled.
 * @param ramp_ms Ramp in ms, clamped to SIDETONE_RAMP_MIN_MS..SIDETONE_RAMP_MAX_MS.
 */
void sidetone_set_ramp_ms(uint8_t ramp_ms);

/** Requests the tone on or off. The envelope starts or ends at the next Timer 0 service. */
void sidetone_set_on(bool is_on);

/** Returns true while the tone is requested or the output is not yet off. */
bool sidetone_is_busy(void);

/** Returns the envelope state, one of SIDETONE_STATE_*. */
sidetone_state_t sidetone_state(void);

/** Debounces the plug input. Call it on each main-loop pass. */
void sidetone_poll(void);

/** Runs one envelope step. Timer 0 interrupt only; host tests call it directly. */
void sidetone_service(void);

#if defined(__SDCC_mcs51)
/* SDCC creates the interrupt vector only when main.c sees this declaration. */
void sidetone_timer0_isr(void) __interrupt(TIMER0_INTERRUPT);
#endif

#endif /* SIDETONE_SIDETONE_H */
