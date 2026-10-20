/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef CW_PLAYER_SHARED_H
#define CW_PLAYER_SHARED_H

/*
 * State that the main loop shares with the player in the Timer 0 interrupt (references/internal/training_spec.md
 * sections 5.4 and 14.1). player.c defines it. The main loop writes the codes and the 16-bit tick values only while
 * player_is_enabled is 0, so each interrupt access is atomic.
 */

#include <stdint.h>

#include "cw/player.h"
#include "mcu/compiler.h"
#include "sidetone/sidetone_shared.h"

#define PLAYER_EARLY_RELEASE_TICKS sidetone_ramp_calls /* 4 x Ramp ms: the fall ends at the mark end */
#define PLAYER_CHAR_GAP_UNITS 3U

/*
 * Play form of one character: bit 0 is the next element (1 = dah). After each element the interrupt shifts the
 * value right by 1. The value 1 means "no element left".
 */
#define PLAYER_CODE_DONE 1U

extern volatile MEM_BIT player_is_enabled;         /* main sets; main and ISR clear */
extern volatile MEM_BIT player_is_start_requested; /* main sets; ISR clears */
extern volatile MEM_XDATA uint8_t player_codes[PLAYER_TEXT_MAX];
extern volatile MEM_XDATA uint8_t player_code_count;
extern volatile MEM_XDATA uint16_t player_dit_ticks;
extern volatile MEM_XDATA uint16_t player_dah_ticks;
extern volatile MEM_XDATA uint16_t player_char_gap_ticks;

#endif /* CW_PLAYER_SHARED_H */
