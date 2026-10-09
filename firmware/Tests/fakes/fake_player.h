/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef FAKE_PLAYER_H
#define FAKE_PLAYER_H

/*
 * Host fake of cw/player.h for the mode and menu tests (references/internal/training_spec.md section 15.2). It
 * records the calls in order and plays nothing. The playback ends only when the test calls fake_player_finish().
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "cw/player.h"

#define FAKE_PLAYER_LOG_SIZE 512U

#define FAKE_PLAYER_CALL_START 1U
#define FAKE_PLAYER_CALL_STOP 2U

/** Clears the log, the counters and the last start arguments; busy reads false and starts are accepted. */
void fake_player_reset(void);
size_t fake_player_start_count(void);
size_t fake_player_stop_count(void);
/** Returns true after a successful start, before a stop or fake_player_finish(). */
bool fake_player_is_running(void);
/** The playback ends: player_is_busy() reads false. */
void fake_player_finish(void);
/** Sets the result of the next player_start() calls. */
void fake_player_set_start_result(bool is_accepted);
/** Copy of the last started text, NUL-terminated ("" without a start). */
const char* fake_player_last_text(void);
uint8_t fake_player_last_wpm(void);
uint8_t fake_player_last_eff_wpm(void);
/** Calls in order: FAKE_PLAYER_CALL_START or FAKE_PLAYER_CALL_STOP. */
size_t fake_player_call_count(void);
uint8_t fake_player_call_at(size_t index);
/** Sets a hook that player_start() calls after it records its arguments, or NULL for none. */
void fake_player_set_start_hook(void (*hook)(void));

#endif /* FAKE_PLAYER_H */
