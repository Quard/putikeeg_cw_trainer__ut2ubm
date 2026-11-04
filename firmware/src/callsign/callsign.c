/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "callsign/callsign.h"

#include <stdbool.h>
#include <stdint.h>

#include "koch/koch.h"
#include "mcu/compiler.h"
#include "util/rng.h"

/* callsign_work before a counting scan: above every item count, so the scan finds no item and counts them all. */
#define CALLSIGN_SCAN_ALL 0xFFU
/* In a character scan, bit 7 of callsign_index selects the letters, and bits 0-6 hold the Koch index. */
#define CALLSIGN_SCAN_DIGITS 0x00U
#define CALLSIGN_SCAN_LETTERS 0x80U
#define CALLSIGN_KOCH_INDEX_MASK 0x7FU
/* After the first suffix letter: a pick of 5 and then a pick of 2 give the lengths 1:2:2 (D-T68). */
#define CALLSIGN_SECOND_LETTER_PICK 5U
#define CALLSIGN_THIRD_LETTER_PICK 2U
#define CALLSIGN_DIGIT_COUNT 10U
#define CALLSIGN_LETTER_COUNT 26U

/*
 * The prefix list of training_spec.md section 23.2, in table order. A 3-character prefix holds its own area digit
 * (D-T71) and has no NUL; a shorter prefix gets NUL bytes. Single characters, because a string literal of 3
 * characters raises a compiler warning for a 3-byte array.
 */
static const MEM_CODE char callsign_prefixes[][CALLSIGN_PREFIX_SIZE] = {
    {'U', 'R'}, {'U', 'S'},      {'U', 'T'},      {'U', 'U'},      {'U', 'V'},      {'U', 'W'},      {'U', 'X'},
    {'U', 'Y'}, {'U', 'Z'},      {'E', 'M'},      {'E', 'N'},      {'E', 'O'},      {'K'},           {'W'},
    {'N'},      {'A', 'A'},      {'K', 'B'},      {'K', 'H', '6'}, {'K', 'L', '7'}, {'K', 'P', '4'}, {'K', 'H', '2'},
    {'V', 'E'}, {'V', 'A'},      {'X', 'E'},      {'C', 'O'},      {'P', '4'},      {'H', 'K'},      {'P', 'Y'},
    {'P', 'U'}, {'L', 'U'},      {'C', 'E'},      {'C', 'X'},      {'D', 'L'},      {'D', 'K'},      {'D', 'J'},
    {'D', 'O'}, {'S', 'P'},      {'S', 'Q'},      {'O', 'K'},      {'O', 'M'},      {'H', 'A'},      {'Y', 'O'},
    {'L', 'Z'}, {'Y', 'U'},      {'9', 'A'},      {'S', '5'},      {'E', '7'},      {'Z', '3'},      {'S', 'V'},
    {'5', 'B'}, {'T', 'A'},      {'E', 'R'},      {'O', 'E'},      {'H', 'B'},      {'H', 'B', '0'}, {'I'},
    {'I', 'K'}, {'I', 'Z'},      {'I', 'S', '0'}, {'9', 'H'},      {'F'},           {'O', 'N'},      {'P', 'A'},
    {'L', 'X'}, {'G'},           {'M'},           {'G', 'M'},      {'G', 'W'},      {'E', 'I'},      {'E', 'A'},
    {'E', 'C'}, {'E', 'A', '6'}, {'E', 'A', '8'}, {'C', 'T'},      {'C', 'T', '3'}, {'O', 'Z'},      {'S', 'M'},
    {'L', 'A'}, {'O', 'H'},      {'O', 'H', '0'}, {'T', 'F'},      {'E', 'S'},      {'Y', 'L'},      {'L', 'Y'},
    {'4', 'X'}, {'A', '6'},      {'U', 'N'},      {'V', 'U'},      {'B', 'Y'},      {'H', 'L'},      {'J', 'A'},
    {'J', 'H'}, {'J', 'R'},      {'D', 'U'},      {'Y', 'B'},      {'V', 'K'},      {'Z', 'L'},      {'Z', 'S'},
    {'C', 'N'}, {'D', '4'},
};

#define CALLSIGN_PREFIX_COUNT ((uint8_t)(sizeof(callsign_prefixes) / sizeof(callsign_prefixes[0])))
_Static_assert((sizeof(callsign_prefixes) / sizeof(callsign_prefixes[0])) < CALLSIGN_SCAN_ALL,
               "a prefix index must fit uint8_t, and a counting scan must never find an item");
_Static_assert(KOCH_WORD_ALPHABET_MAX <= CALLSIGN_KOCH_INDEX_MASK, "a Koch index must fit bits 0-6");
_Static_assert(CALLSIGN_LENGTH_MAX <= KOCH_WORD_LEN_MAX, "a callsign must fit the item buffer and the player");

/* Work values in xdata, so the calls below need no internal RAM to keep them (6 B with the text parameter). */
static MEM_XDATA uint8_t callsign_count; /* characters of the set, capped at KOCH_WORD_ALPHABET_MAX */
/* The write position in the text. Before the first write, a prefix scan uses it for the character of the prefix. */
static MEM_XDATA uint8_t callsign_position;
static MEM_XDATA uint8_t callsign_index; /* the item of a scan */
/* A count, a pick, the allowed items that a scan still has to pass, or the character to write. */
static MEM_XDATA uint8_t callsign_work;

/* The Koch characters 40 to 42 are prosigns, so a count above 40 gives the same callsigns as 40. */
static void callsign_set_count(uint8_t char_count) {
  callsign_count = (char_count > KOCH_WORD_ALPHABET_MAX) ? KOCH_WORD_ALPHABET_MAX : char_count;
}

/*
 * Fair pick (D-T84): replaces n in callsign_work with 0..n-1, each with the same chance. The limit is a multiple of
 * n, so each result gets 255 / n of the accepted high bytes. n = 0 stays 0 and reads no RNG.
 */
static void callsign_pick(void) {
  uint8_t value;

  if (callsign_work == 0U) {
    return;
  }
  do {
    value = (uint8_t)(rng_next() >> 8);
  } while (value >= (uint8_t)(callsign_work * (uint8_t)((uint8_t)CALLSIGN_SCAN_ALL / callsign_work)));
  callsign_work = (uint8_t)(value % callsign_work);
}

/*
 * The scans walk the allowed items in list order or in Koch order. Each allowed item counts callsign_work down by
 * one; the scan stops on the allowed item where callsign_work is 0. So CALLSIGN_SCAN_ALL counts the items, and a
 * pick k finds the k-th item.
 */

/* A scan that finds no prefix ends past the list; NUL then keeps a call against the rule of callsign.h in bounds. */
static char callsign_prefix_char(void) {
  return (callsign_index < CALLSIGN_PREFIX_COUNT) ? callsign_prefixes[callsign_index][callsign_position] : '\0';
}

/* A prefix is allowed when each of its characters is in the set. Stops with callsign_index on the prefix found. */
static void callsign_scan_prefixes(void) {
  char character;

  for (callsign_index = 0U; callsign_index < CALLSIGN_PREFIX_COUNT; ++callsign_index) {
    for (callsign_position = 0U; callsign_position < CALLSIGN_PREFIX_SIZE; ++callsign_position) {
      character = callsign_prefix_char();
      if ((character != '\0') && (koch_index_of(character) >= callsign_count)) {
        break;
      }
    }
    if (callsign_position == CALLSIGN_PREFIX_SIZE) {
      if (callsign_work == 0U) {
        return;
      }
      --callsign_work;
    }
  }
}

/* Digits or letters by bit 7 of callsign_index. Puts the character found in callsign_work. */
static void callsign_scan_chars(void) {
  char character;
  char first;
  uint8_t span;

  for (; (uint8_t)(callsign_index & CALLSIGN_KOCH_INDEX_MASK) < callsign_count; ++callsign_index) {
    character = koch_char((uint8_t)(callsign_index & CALLSIGN_KOCH_INDEX_MASK));
    first = '0';
    span = CALLSIGN_DIGIT_COUNT;
    if ((callsign_index & CALLSIGN_SCAN_LETTERS) != 0U) {
      first = 'A';
      span = CALLSIGN_LETTER_COUNT;
    }
    if ((uint8_t)(character - first) < span) {
      if (callsign_work == 0U) {
        callsign_work = (uint8_t)character;
        return;
      }
      --callsign_work;
    }
  }
}

static uint8_t callsign_count_prefixes(void) {
  callsign_work = CALLSIGN_SCAN_ALL;
  callsign_scan_prefixes();
  return (uint8_t)(CALLSIGN_SCAN_ALL - callsign_work);
}

/* @p family is CALLSIGN_SCAN_DIGITS or CALLSIGN_SCAN_LETTERS. It stays in bit 7 of callsign_index after the scan. */
static uint8_t callsign_count_chars(uint8_t family) {
  callsign_index = family;
  callsign_work = CALLSIGN_SCAN_ALL;
  callsign_scan_chars();
  return (uint8_t)(CALLSIGN_SCAN_ALL - callsign_work);
}

/* Puts a character of @p family, each with the same chance, in callsign_work. */
static void callsign_pick_char(uint8_t family) {
  callsign_work = callsign_count_chars(family);
  callsign_pick();
  callsign_index &= CALLSIGN_SCAN_LETTERS;
  callsign_scan_chars();
}

/* Writes the character in callsign_work at the write position and moves the position. */
static void callsign_put(char MEM_XDATA* text) {
  text[callsign_position] = (char)callsign_work;
  ++callsign_position;
}

bool callsign_is_possible(uint8_t char_count) {
  callsign_set_count(char_count);
  if ((callsign_count_chars(CALLSIGN_SCAN_DIGITS) == 0U) || (callsign_count_chars(CALLSIGN_SCAN_LETTERS) == 0U) ||
      (callsign_count_prefixes() == 0U)) {
    return false;
  }
  return true;
}

/* The steps and the RNG order of training_spec.md section 23.3. */
uint8_t callsign_make(uint8_t char_count, char MEM_XDATA* MEM_XDATA text) {
  callsign_set_count(char_count);
  callsign_work = callsign_count_prefixes();
  callsign_pick();
  callsign_scan_prefixes();
  callsign_position = 0U;
  do {
    callsign_work = (uint8_t)callsign_prefix_char();
    if (callsign_work == 0U) {
      break;
    }
    callsign_put(text);
  } while (callsign_position < CALLSIGN_PREFIX_SIZE);
  if (callsign_position < CALLSIGN_PREFIX_SIZE) {
    callsign_pick_char(CALLSIGN_SCAN_DIGITS);
    callsign_put(text);
  }
  callsign_pick_char(CALLSIGN_SCAN_LETTERS);
  callsign_put(text);
  callsign_work = CALLSIGN_SECOND_LETTER_PICK;
  callsign_pick();
  if (callsign_work != 0U) {
    callsign_pick_char(CALLSIGN_SCAN_LETTERS);
    callsign_put(text);
    callsign_work = CALLSIGN_THIRD_LETTER_PICK;
    callsign_pick();
    if (callsign_work != 0U) {
      callsign_pick_char(CALLSIGN_SCAN_LETTERS);
      callsign_put(text);
    }
  }
  callsign_work = (uint8_t)'\0';
  callsign_put(text);
  return (uint8_t)(callsign_position - 1U);
}
