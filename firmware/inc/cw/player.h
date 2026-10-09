/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CW_PLAYER_H
#define CW_PLAYER_H

/*
 * CW player for the Receiving mode (references/internal/training_spec.md sections 5.4 and 9). The Timer 0
 * interrupt plays the text; these functions are for the main loop only. The keyer must be stopped (D-T12).
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#define PLAYER_TEXT_MAX 8U /* equal to KOCH_WORD_LEN_MAX */

/**
 * Stops a running playback, then plays @p length characters of @p text at @p wpm with the Farnsworth spacing of
 * @p eff_wpm (both clamped as in cw_timing_compute()). Returns false and plays nothing when @p length is 0 or above
 * PLAYER_TEXT_MAX, or when a character has no Morse code. The keyer must be stopped (D-T12).
 */
bool player_start(const char MEM_XDATA* text, MEM_XDATA uint8_t length, MEM_XDATA uint8_t wpm,
                  MEM_XDATA uint8_t eff_wpm);

/** Returns true from player_start() until the last mark ends or player_stop() runs. */
bool player_is_busy(void);

/** Stops the playback and requests the sidetone off. A second call has no other effect. */
void player_stop(void);

#endif /* CW_PLAYER_H */
