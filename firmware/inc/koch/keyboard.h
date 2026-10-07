#ifndef KOCH_KEYBOARD_H
#define KOCH_KEYBOARD_H

/*
 * Receiving group keyboard: four groups narrow the character list until one character is left
 * (references/internal/training_spec.md section 8). Main loop only.
 */

#include <stdbool.h>
#include <stdint.h>

/* Runs from left to right, named after the button under each run (D-T27, D-T28). */
#define KEYBOARD_GROUP_MENU 0U /* menu/back, left */
#define KEYBOARD_GROUP_PREV 1U
#define KEYBOARD_GROUP_NEXT 2U
#define KEYBOARD_GROUP_ENTER 3U /* right */
#define KEYBOARD_GROUP_COUNT 4U
#define KEYBOARD_NO_PICK '\0'
#define KEYBOARD_DEPTH_MAX 3U /* selects per pick, enough for 4^3 = 64 characters */

/**
 * Builds the list of the first @p koch_count Koch characters in keyboard order and shows the whole list. The count
 * is clamped to 2..KOCH_CHAR_COUNT. Receiving passes training_alphabet_count(), so a word session has no prosign.
 */
void keyboard_reset(uint8_t koch_count);
/** Number of characters in the list. */
uint8_t keyboard_count(void);
/** Character @p index of the list, or KEYBOARD_NO_PICK out of range. */
char keyboard_char(uint8_t index);
/** List index of the first character of @p group in the current range. An unknown group gives the range end. */
uint8_t keyboard_group_first(uint8_t group);
/** Characters in @p group of the current range, 0 for an empty group or an unknown group. */
uint8_t keyboard_group_size(uint8_t group);
/**
 * Selects @p group. An empty or unknown group changes nothing. A group of one character returns that character and
 * shows the whole list again. A larger group becomes the current range. Returns KEYBOARD_NO_PICK unless it picks.
 */
char keyboard_select(uint8_t group);
/** Goes back to the range before the last narrowing. Returns false at the whole list. */
bool keyboard_back(void);
/** Shows the whole list. */
void keyboard_top(void);
/** Narrowings since the whole list, 0..KEYBOARD_DEPTH_MAX - 1. */
uint8_t keyboard_depth(void);

#endif /* KOCH_KEYBOARD_H */
