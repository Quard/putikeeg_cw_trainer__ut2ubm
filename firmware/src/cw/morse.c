#include "cw/morse.h"

#include "mcu/compiler.h"

#define MORSE_TABLE_SIZE 128U

/*
 * Index: start at 1, shift left by 1 per element, add 1 for a dah (keyer_spec.md section 6.10). Prosigns with the
 * code of a sign decode as that sign: AR '+', BT '=', KN '(', AS '&'. All other slots are MORSE_CHAR_UNKNOWN.
 */
static const MEM_CODE char morse_table[MORSE_TABLE_SIZE] = {
    [2] = 'E',   [3] = 'T',   [4] = 'I',   [5] = 'A',   [6] = 'N',   [7] = 'M',   [8] = 'S',
    [9] = 'U',   [10] = 'R',  [11] = 'W',  [12] = 'D',  [13] = 'K',  [14] = 'G',  [15] = 'O',
    [16] = 'H',  [17] = 'V',  [18] = 'F',  [20] = 'L',  [22] = 'P',  [23] = 'J',  [24] = 'B',
    [25] = 'X',  [26] = 'C',  [27] = 'Y',  [28] = 'Z',  [29] = 'Q',  [32] = '5',  [33] = '4',
    [35] = '3',  [39] = '2',  [40] = '&',  [42] = '+',  [47] = '1',  [48] = '6',  [49] = '=',
    [50] = '/',  [54] = '(',  [56] = '7',  [60] = '8',  [62] = '9',  [63] = '0',  [69] = MORSE_CHAR_SK,
    [76] = '?',  [77] = '_',  [82] = '"',  [85] = '.',  [90] = '@',  [94] = '\'', [97] = '-',
    [106] = ';', [107] = '!', [109] = ')', [115] = ',', [120] = ':',
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
