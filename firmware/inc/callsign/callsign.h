/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CALLSIGN_CALLSIGN_H
#define CALLSIGN_CALLSIGN_H

/*
 * Generator of semi-real callsigns for Free Training (references/internal/training_spec.md sections 23.2 and 23.3).
 * A callsign is a prefix of the prefix list, an area digit and a suffix of 1 to 3 letters. Main loop only.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#define CALLSIGN_PREFIX_SIZE 3U /* characters of the longest prefix; a 3-character prefix holds its own area digit */
#define CALLSIGN_SUFFIX_MAX 3U
#define CALLSIGN_LENGTH_MIN 3U
#define CALLSIGN_LENGTH_MAX (CALLSIGN_PREFIX_SIZE + CALLSIGN_SUFFIX_MAX) /* D-T72 */

/**
 * True when the first @p char_count Koch characters hold at least one digit, at least one letter and every character
 * of at least one prefix of the list (D-T67). Uses no RNG.
 */
bool callsign_is_possible(uint8_t char_count);
/**
 * Writes one callsign from the first @p char_count Koch characters, then a NUL, to @p text (at least
 * CALLSIGN_LENGTH_MAX + 1 bytes). Returns its length, CALLSIGN_LENGTH_MIN..CALLSIGN_LENGTH_MAX. Uses the RNG in the
 * order of section 23.3. Call it only when callsign_is_possible(@p char_count) is true.
 */
uint8_t callsign_make(uint8_t char_count, char MEM_XDATA* MEM_XDATA text);

#endif /* CALLSIGN_CALLSIGN_H */
