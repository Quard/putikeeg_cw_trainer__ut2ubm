#ifndef KOCH_KOCH_H
#define KOCH_KOCH_H

/*
 * Koch character order, keyboard order and level model (references/internal/training_spec.md sections 5.6, 6 and
 * 7). Every function takes any level index 0..65535. Main loop only.
 */

#include <stdint.h>

#include "mcu/compiler.h"

typedef uint8_t koch_kind_t;
#define KOCH_KIND_CHAR 0U
#define KOCH_KIND_WORD 1U

#define KOCH_CHAR_COUNT 43U        /* entries of the Koch table (D-T3, Q-T1) */
#define KOCH_FIRST_LEARNED 2U      /* level index 0 has K and M */
#define KOCH_WORD_ALPHABET_MAX 40U /* D-T4: BT, SK and AR stay out of words */
#define KOCH_WORD_LEN_MIN 2U
#define KOCH_WORD_LEN_MAX 8U /* equal to settings_max(SETTINGS_ID_MAX_WORD_LEN) */
#define KOCH_CHAR_ITEMS 50U
#define KOCH_WORD_ITEMS 10U
#define KOCH_FINAL_ITEMS_MAX 100U
#define KOCH_LEVEL_MAX 65535U
/* First level index of the final word phase: the first character level whose learned count is above the table. */
#define KOCH_FINAL_LEVEL \
  (((KOCH_CHAR_COUNT % 2U) != 0U) ? (3U * ((KOCH_CHAR_COUNT - 1U) / 2U)) : ((3U * ((KOCH_CHAR_COUNT - 2U) / 2U)) + 1U))

/** Returns character @p index of the Koch order, or '\0' for an index at or above KOCH_CHAR_COUNT. */
char koch_char(uint8_t index);
/** Returns character @p index of the keyboard order, or '\0' for an index at or above KOCH_CHAR_COUNT. */
char koch_keyboard_char(uint8_t index);
/** Returns the index of @p character in the Koch order, or KOCH_CHAR_COUNT when it is not in the table. */
uint8_t koch_index_of(char character);

koch_kind_t koch_kind(uint16_t level);
/** Learned characters at @p level: KOCH_FIRST_LEARNED..KOCH_CHAR_COUNT. */
uint8_t koch_learned_count(uint16_t level);
/** Items of a session at @p level: KOCH_CHAR_ITEMS, KOCH_WORD_ITEMS or 10..KOCH_FINAL_ITEMS_MAX. */
uint8_t koch_item_count(uint16_t level);
/** Word length at @p level, or 0 at a character level. @p max_word_len is clamped to 2..KOCH_WORD_LEN_MAX. */
uint8_t koch_word_length(uint16_t level, MEM_XDATA uint8_t max_word_len);
/** Characters that an item of @p level can hold: the learned count, or min(learned, 40) at a word level (D-T4). */
uint8_t koch_alphabet_count(uint16_t level);
/** Level number that the user sees: @p level + 1, saturated at KOCH_LEVEL_MAX (D-T5, D-T37). */
uint16_t koch_shown_level(uint16_t level);
/**
 * Smallest score that passes a session of @p item_count items (D-T33): ceil(0.8 * item_count) for KOCH_KIND_WORD,
 * else ceil(0.9 * item_count).
 */
uint8_t koch_pass_threshold(koch_kind_t kind, MEM_XDATA uint8_t item_count);
/**
 * Writes one random item of @p level and a NUL to @p text (KOCH_WORD_LEN_MAX + 1 bytes) and returns its length.
 * Uses rng_below() in the order of section 7. Defined in src/koch/koch_item.c (D-T18).
 */
uint8_t koch_make_item(uint16_t level, MEM_XDATA uint8_t max_word_len, char MEM_XDATA* MEM_XDATA text);

#endif /* KOCH_KOCH_H */
