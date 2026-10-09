/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "cw/morse.h"

#include "mcu/compiler.h"

#define MORSE_TABLE_SIZE 128U
#define MORSE_PRINTABLE_FIRST 0x20U
#define MORSE_PRINTABLE_LAST 0x7EU
#define MORSE_NO_MARK '\0'
#define MORSE_PROSIGN_COUNT 5U /* MORSE_CHAR_SK to MORSE_CHAR_AS */
#define MORSE_PROSIGN_FIELDS 3U

/*
 * Index: start at 1, shift left by 1 per element, add 1 for a dah (keyer_spec.md section 6.10). Slots 40, 42, 49 and
 * 54 hold MORSE_CHAR_AS, MORSE_CHAR_AR, MORSE_CHAR_BT and MORSE_CHAR_KN, as escapes that keep the column layout.
 * morse_format() gives their marks. All other slots are MORSE_CHAR_UNKNOWN.
 */
static const MEM_CODE char morse_table[MORSE_TABLE_SIZE] = {
    [2] = 'E',   [3] = 'T',     [4] = 'I',     [5] = 'A',     [6] = 'N',   [7] = 'M',   [8] = 'S',
    [9] = 'U',   [10] = 'R',    [11] = 'W',    [12] = 'D',    [13] = 'K',  [14] = 'G',  [15] = 'O',
    [16] = 'H',  [17] = 'V',    [18] = 'F',    [20] = 'L',    [22] = 'P',  [23] = 'J',  [24] = 'B',
    [25] = 'X',  [26] = 'C',    [27] = 'Y',    [28] = 'Z',    [29] = 'Q',  [32] = '5',  [33] = '4',
    [35] = '3',  [39] = '2',    [40] = '\x05', [42] = '\x02', [47] = '1',  [48] = '6',  [49] = '\x03',
    [50] = '/',  [54] = '\x04', [56] = '7',    [60] = '8',    [62] = '9',  [63] = '0',  [69] = MORSE_CHAR_SK,
    [76] = '?',  [77] = '_',    [82] = '"',    [85] = '.',    [90] = '@',  [94] = '\'', [97] = '-',
    [106] = ';', [107] = '!',   [109] = ')',   [115] = ',',   [120] = ':',
};

/* By prosign code minus 1: the two letters of the name, then the mark (keyer_spec.md section 17.4.1). */
static const MEM_CODE char morse_prosigns[MORSE_PROSIGN_COUNT * MORSE_PROSIGN_FIELDS] = {
    'S', 'K', MORSE_NO_MARK, 'A', 'R', '+', 'B', 'T', '=', 'K', 'N', '(', 'A', 'S', '&',
};

uint8_t morse_add_element(uint8_t pattern, bool is_dah) {
  /* 64 and more already has 6 elements or is not a pattern; MORSE_PATTERN_INVALID stays invalid. */
  if ((pattern == MORSE_PATTERN_INVALID) || (pattern >= (1U << MORSE_MAX_ELEMENTS))) {
    return MORSE_PATTERN_INVALID;
  }
  return (uint8_t)((uint8_t)(pattern << 1) | (is_dah ? 1U : 0U));
}

char morse_decode(uint8_t pattern) {
  if (pattern >= MORSE_TABLE_SIZE) {
    return MORSE_CHAR_UNKNOWN;
  }
  return morse_table[pattern];
}

uint8_t morse_encode(char character) {
  uint8_t pattern;

  /* Empty table slots hold MORSE_CHAR_UNKNOWN, so the search must not match it. */
  if (character == MORSE_CHAR_UNKNOWN) {
    return MORSE_PATTERN_INVALID;
  }
  for (pattern = 2U; pattern < MORSE_TABLE_SIZE; ++pattern) {
    if (morse_table[pattern] == character) {
      return pattern;
    }
  }
  return MORSE_PATTERN_INVALID;
}

uint8_t morse_format(char decoded, uint8_t style, char MEM_XDATA* text) {
  /* char is signed on the host and unsigned on SDCC, so compare as uint8_t. Code 0 wraps to 255. */
  const uint8_t index = (uint8_t)((uint8_t)decoded - (uint8_t)MORSE_CHAR_SK);
  uint8_t length = 1U;

  if (index < MORSE_PROSIGN_COUNT) {
    /* Read the table before the writes, so the generated code keeps the values in registers. */
    const uint8_t offset = (uint8_t)(index * MORSE_PROSIGN_FIELDS);
    const char first = morse_prosigns[offset];
    const char second = morse_prosigns[(uint8_t)(offset + 1U)];
    const char mark = morse_prosigns[(uint8_t)(offset + 2U)];

    if ((mark != MORSE_NO_MARK) && (style == MORSE_STYLE_PUNCTUATION)) {
      text[0] = mark;
    } else {
      text[0] = '<';
      text[1] = first;
      text[2] = second;
      length = 3U;
      /* Every style above MORSE_STYLE_NAME acts as MORSE_STYLE_BOTH. */
      if ((mark != MORSE_NO_MARK) && (style != MORSE_STYLE_NAME)) {
        text[3] = '/';
        text[4] = mark;
        length = 5U;
      }
      text[length] = '>';
      ++length;
    }
  } else {
    text[0] = (((uint8_t)decoded >= MORSE_PRINTABLE_FIRST) && ((uint8_t)decoded <= MORSE_PRINTABLE_LAST))
                  ? decoded
                  : MORSE_TEXT_UNKNOWN;
  }
  text[length] = '\0';
  return length;
}
