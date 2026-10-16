/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MODES_TRAINING_H
#define MODES_TRAINING_H

/*
 * Session engine of the Receiving and Sending modes (references/internal/training_spec.md sections 5.10 and 11).
 * It runs the phases, the checks, the score and the level advance. It draws nothing and plays nothing. Main loop
 * only.
 */

#include <stdbool.h>
#include <stdint.h>

#include "koch/koch.h"
#include "mcu/compiler.h"

#define TRAINING_MODE_RECEIVING 0U /* equal to PROGRESS_MODE_RECEIVING */
#define TRAINING_MODE_SENDING 1U   /* equal to PROGRESS_MODE_SENDING */
#define TRAINING_ATTEMPTS 3U
#define TRAINING_TEXT_SIZE (KOCH_WORD_LEN_MAX + 1U)
/* Sending answer character for a word space inside a word (D-T31). No item or decoded character is ' '. */
#define TRAINING_CHAR_WORD_SPACE ' '

/* Delays in ms, ui-ux.md sections 5.14 and 6.10 (UX-46). -D can set each one for host tests. */
#ifndef TRAINING_START_MS
#define TRAINING_START_MS 1000U
#endif
#ifndef TRAINING_CORRECT_MS
#define TRAINING_CORRECT_MS 600U
#endif
#ifndef TRAINING_WRONG_MS
#define TRAINING_WRONG_MS 1200U
#endif
#ifndef TRAINING_FAILED_MS
#define TRAINING_FAILED_MS 2500U /* Sending only: Receiving waits for an enter tap (D-T44) */
#endif
#ifndef TRAINING_RECEIVING_GAP_MS
#define TRAINING_RECEIVING_GAP_MS 500U
#endif
#ifndef TRAINING_SENDING_GAP_MS
#define TRAINING_SENDING_GAP_MS 300U
#endif

typedef uint8_t training_phase_t;
#define TRAINING_PHASE_IDLE 0U   /* no session */
#define TRAINING_PHASE_GAP 1U    /* pause before an item; Sending shows a blank item area */
#define TRAINING_PHASE_ITEM 2U   /* the item is shown or played; answers count */
#define TRAINING_PHASE_RESULT 3U /* a check result shows; answers are dropped */
#define TRAINING_PHASE_END 4U    /* session end screen: the outcome is ready; no selector acts (D-T46) */
#define TRAINING_PHASE_SELECT 5U /* start screen: the level selector, no session (D-T36) */

typedef uint8_t training_check_t;
#define TRAINING_CHECK_PARTIAL 0U /* the answer so far is accepted; more characters needed */
#define TRAINING_CHECK_CORRECT 1U
#define TRAINING_CHECK_WRONG 2U   /* an attempt is used; attempts remain */
#define TRAINING_CHECK_FAILED 3U  /* third wrong attempt; the answer shows */
#define TRAINING_CHECK_IGNORED 4U /* not in TRAINING_PHASE_ITEM; the character is dropped */

typedef uint8_t training_event_t;
#define TRAINING_EVENT_NONE 0U
#define TRAINING_EVENT_ITEM 1U  /* ITEM began with a new item */
#define TRAINING_EVENT_RETRY 2U /* ITEM began again with the same item after a wrong attempt */
#define TRAINING_EVENT_GAP 3U   /* GAP began after a resolved item */
#define TRAINING_EVENT_END 4U   /* END began; the outcome is ready */

typedef uint8_t training_outcome_t;
#define TRAINING_OUTCOME_FAIL 0U           /* the level repeats */
#define TRAINING_OUTCOME_PASS 1U           /* the level went up and is stored */
#define TRAINING_OUTCOME_PASS_NOT_SAVED 2U /* the level went up in RAM only */
#define TRAINING_OUTCOME_PASS_TOP 3U       /* passed at level index 65535; no change, no save (D-T17) */
#define TRAINING_OUTCOME_PASS_REPLAY 4U    /* passed below the earned level; no change, no save (D-T36) */

/**
 * Prepares @p mode (unknown gives RECEIVING) at @p wpm with @p max_word_len and selects the earned level. Phase
 * SELECT. No session runs. Reads the progress RAM only (D-T36).
 */
void training_prepare(uint8_t mode, MEM_XDATA uint8_t wpm, MEM_XDATA uint8_t max_word_len);
/**
 * Starts a session at training_selected_level() with the values of training_prepare(). Phase GAP, TRAINING_START_MS.
 * No phase check: the modes call it in SELECT only (D-T46).
 */
void training_start(void);
/**
 * In phase SELECT: moves the selected level one step up (@p is_up) or down, inside 0..training_earned_level().
 * No wrap. Returns true when the level changed. Other phases, END included: no change, returns false (D-T46).
 */
bool training_select_step(bool is_up);
/**
 * In phase END: sets phase SELECT and keeps the selected level, so the start screen shows the preselection of
 * section 11.4. Reads no tick, no progress. Returns true. Other phases: no change, returns false (D-T46).
 */
bool training_reopen_select(void);
/** Level index that the next training_start() uses: 0..training_earned_level(). */
uint16_t training_selected_level(void);
/** Earned level index: progress_level() of the prepared mode and speed, read at each call. */
uint16_t training_earned_level(void);
/** Ends the session without a store. Phase IDLE. */
void training_stop(void);
/** Runs the time-based steps of section 11. Returns at most one event. Reads the tick once. */
training_event_t training_poll(void);
/** Adds @p character to the answer and checks it (section 11.3). */
training_check_t training_add_char(char character);
/** Removes the last answer character in phase ITEM. Returns false when there is none. */
bool training_undo_char(void);
/**
 * In phase RESULT: true stops the RESULT delay. False clears the hold and sets the delay to 0, so the next
 * training_poll() ends the RESULT. Other phases: no effect. A phase change clears the hold (D-T14, D-T44).
 */
void training_hold_result(bool is_held);

training_phase_t training_phase(void);
uint8_t training_mode(void);
uint8_t training_wpm(void); /* the progress key of the session */
koch_kind_t training_kind(void);
uint16_t training_level(void);           /* level index of the session */
uint16_t training_shown_level(void);     /* koch_shown_level(training_level()) */
uint16_t training_new_shown_level(void); /* koch_shown_level(training_selected_level()) (D-T36) */
uint8_t training_learned_count(void);
uint8_t training_alphabet_count(void);    /* koch_alphabet_count() of the session level */
uint8_t training_item_number(void);       /* 0 before the first item, then 1..item count */
uint8_t training_shown_item_number(void); /* header number: GAP gives item number + 1, else the item number */
uint8_t training_item_count(void);
uint8_t training_score(void); /* items correct on the first attempt */
uint8_t training_pass_threshold(void);
uint8_t training_wrong_attempts(void); /* of the current item, 0..TRAINING_ATTEMPTS */
training_check_t training_last_check(void);
training_outcome_t training_outcome(void);      /* meaningful in phase END */
const char MEM_XDATA* training_item_text(void); /* NUL-terminated, 1..KOCH_WORD_LEN_MAX characters */
uint8_t training_item_length(void);
const char MEM_XDATA* training_answer_text(void); /* NUL-terminated, 0..item length characters */
uint8_t training_answer_length(void);

#endif /* MODES_TRAINING_H */
