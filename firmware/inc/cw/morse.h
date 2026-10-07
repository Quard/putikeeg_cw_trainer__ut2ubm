#ifndef CW_MORSE_H
#define CW_MORSE_H

/*
 * Morse pattern builder and decoder table (references/internal/keyer_spec.md sections 5.7 and 6.10). A pattern
 * starts at MORSE_PATTERN_EMPTY; each element shifts it left by 1 and adds 1 for a dah. Main loop only.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#define MORSE_PATTERN_EMPTY 0x01U   /* no element yet */
#define MORSE_PATTERN_INVALID 0x00U /* more than MORSE_MAX_ELEMENTS, or a lost event */
#define MORSE_MAX_ELEMENTS 6U
#define MORSE_CHAR_UNKNOWN '\0' /* Free Input shows '*' */
#define MORSE_CHAR_SK '\x01'    /* prosign SK; Free Input shows "<SK>" */
#define MORSE_CHAR_AR '\x02'    /* .-.-.  mark '+' */
#define MORSE_CHAR_BT '\x03'    /* -...-  mark '=' */
#define MORSE_CHAR_KN '\x04'    /* -.--.  mark '(' */
#define MORSE_CHAR_AS '\x05'    /* .-...  mark '&' */

#define MORSE_STYLE_PUNCTUATION 0U /* equal to SETTINGS_PROSIGN_PUNCTUATION */
#define MORSE_STYLE_NAME 1U        /* equal to SETTINGS_PROSIGN_NAME */
#define MORSE_STYLE_BOTH 2U        /* equal to SETTINGS_PROSIGN_BOTH */

#define MORSE_TEXT_UNKNOWN '*'
#define MORSE_TEXT_MAX_LENGTH 6U /* "<AR/+>" */
#define MORSE_TEXT_SIZE 7U       /* MORSE_TEXT_MAX_LENGTH and NUL */

/** Returns @p pattern with one more element, or MORSE_PATTERN_INVALID after the 6th element or for 64 and more. */
uint8_t morse_add_element(uint8_t pattern, bool is_dah);

/**
 * Returns the character of @p pattern, a prosign code MORSE_CHAR_SK to MORSE_CHAR_AS, or MORSE_CHAR_UNKNOWN for a
 * pattern not in the table.
 */
char morse_decode(uint8_t pattern);

/**
 * Returns the pattern of @p character (a table character or a prosign code MORSE_CHAR_SK to MORSE_CHAR_AS), or
 * MORSE_PATTERN_INVALID when the table has no such character. MORSE_CHAR_UNKNOWN gives MORSE_PATTERN_INVALID.
 */
uint8_t morse_encode(char character);

/**
 * Writes the display text of @p decoded, a morse_decode() result, in @p style to @p text and returns its length,
 * 1 to MORSE_TEXT_MAX_LENGTH. @p text holds MORSE_TEXT_SIZE bytes.
 */
uint8_t morse_format(char decoded, uint8_t style, char MEM_XDATA* text);

#endif /* CW_MORSE_H */
