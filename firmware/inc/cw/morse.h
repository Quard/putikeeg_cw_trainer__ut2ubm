#ifndef CW_MORSE_H
#define CW_MORSE_H

/*
 * Morse pattern builder and decoder table (references/internal/keyer_spec.md sections 5.7 and 6.10). A pattern
 * starts at MORSE_PATTERN_EMPTY; each element shifts it left by 1 and adds 1 for a dah. Main loop only.
 */

#include <stdbool.h>
#include <stdint.h>

#define MORSE_PATTERN_EMPTY 0x01U   /* no element yet */
#define MORSE_PATTERN_INVALID 0x00U /* more than MORSE_MAX_ELEMENTS, or a lost event */
#define MORSE_MAX_ELEMENTS 6U
#define MORSE_CHAR_UNKNOWN '\0' /* Free Input shows '*' */
#define MORSE_CHAR_SK '\x01'    /* prosign SK; Free Input shows "<SK>" */

/** Returns @p pattern with one more element, or MORSE_PATTERN_INVALID after the 6th element or for 64 and more. */
uint8_t morse_add_element(uint8_t pattern, bool is_dah);

/** Returns the character of @p pattern, MORSE_CHAR_SK, or MORSE_CHAR_UNKNOWN for a pattern not in the table. */
char morse_decode(uint8_t pattern);

#endif /* CW_MORSE_H */
