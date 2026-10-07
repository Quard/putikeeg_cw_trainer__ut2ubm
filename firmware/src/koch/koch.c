#include "koch/koch.h"

#include "cw/morse.h"
#include "mcu/compiler.h"

#define KOCH_NO_CHAR '\0'
/* koch_split() result for a level of the final word phase; 0..2 is the level index modulo 3. */
#define KOCH_SPLIT_FINAL 3U
#define KOCH_SPLIT_WORD 2U
#define KOCH_FINAL_GROW_LEVELS (KOCH_FINAL_ITEMS_MAX - KOCH_WORD_ITEMS)

/* User order (training_spec.md section 2.1). */
static const MEM_CODE char koch_order[] = {
    'K', 'M', 'R', 'S', 'U', 'A', 'P', 'T', 'L', 'O', 'W',           'I',           '.',           'N', 'J',
    'E', 'F', '0', 'Y', ',', 'V', 'G', '5', '/', 'Q', '9',           'Z',           'H',           '3', '8',
    'B', '?', '4', '2', '7', 'C', '1', 'D', '6', 'X', MORSE_CHAR_BT, MORSE_CHAR_SK, MORSE_CHAR_AR,
};

/* Order of the Receiving group keyboard. */
static const MEM_CODE char koch_keyboard_order[] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K',           'L',           'M',           'N', 'O',
    'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z',           '0',           '1',           '2', '3',
    '4', '5', '6', '7', '8', '9', '.', ',', '?', '/', MORSE_CHAR_BT, MORSE_CHAR_SK, MORSE_CHAR_AR,
};

_Static_assert(sizeof(koch_order) == KOCH_CHAR_COUNT, "the Koch table needs KOCH_CHAR_COUNT entries");
_Static_assert(sizeof(koch_keyboard_order) == KOCH_CHAR_COUNT, "the keyboard table needs KOCH_CHAR_COUNT entries");
_Static_assert(KOCH_WORD_ALPHABET_MAX <= KOCH_CHAR_COUNT, "the word alphabet must fit the Koch table");
_Static_assert(KOCH_FINAL_LEVEL <= 255U, "levels below KOCH_FINAL_LEVEL must fit uint8_t");

/* Level index / 3 from the last koch_split() call below KOCH_FINAL_LEVEL. */
static MEM_XDATA uint8_t koch_group;

/* Below KOCH_FINAL_LEVEL the level fits uint8_t, so both divisions are one DIV AB each. */
static uint8_t koch_split(uint16_t level) {
  uint8_t small;

  if (level >= KOCH_FINAL_LEVEL) {
    return KOCH_SPLIT_FINAL;
  }
  small = (uint8_t)level;
  koch_group = (uint8_t)(small / 3U);
  return (uint8_t)(small % 3U);
}

char koch_char(uint8_t index) { return (index < KOCH_CHAR_COUNT) ? koch_order[index] : KOCH_NO_CHAR; }

char koch_keyboard_char(uint8_t index) { return (index < KOCH_CHAR_COUNT) ? koch_keyboard_order[index] : KOCH_NO_CHAR; }

uint8_t koch_index_of(char character) {
  uint8_t index;

  for (index = 0U; index < KOCH_CHAR_COUNT; ++index) {
    if (koch_order[index] == character) {
      break;
    }
  }
  return index;
}

koch_kind_t koch_kind(uint16_t level) {
  return (koch_split(level) >= KOCH_SPLIT_WORD) ? KOCH_KIND_WORD : KOCH_KIND_CHAR;
}

uint8_t koch_learned_count(uint16_t level) {
  const uint8_t split = koch_split(level);
  uint8_t count;

  if (split == KOCH_SPLIT_FINAL) {
    return KOCH_CHAR_COUNT;
  }
  /* Each group of three levels adds two characters: L % 3 == 0 gives 2 + 2 (L / 3), else 3 + 2 (L / 3). */
  count = (uint8_t)((uint8_t)(koch_group << 1) + KOCH_FIRST_LEARNED);
  if (split != 0U) {
    ++count;
  }
  return count;
}

uint8_t koch_item_count(uint16_t level) {
  const uint8_t split = koch_split(level);

  if (split == KOCH_SPLIT_FINAL) {
    /* Compare first, so level - KOCH_FINAL_LEVEL fits uint8_t below. */
    if (level >= (uint16_t)(KOCH_FINAL_LEVEL + KOCH_FINAL_GROW_LEVELS)) {
      return KOCH_FINAL_ITEMS_MAX;
    }
    return (uint8_t)((uint8_t)(level - KOCH_FINAL_LEVEL) + KOCH_WORD_ITEMS);
  }
  return (split == KOCH_SPLIT_WORD) ? KOCH_WORD_ITEMS : KOCH_CHAR_ITEMS;
}

uint8_t koch_word_length(uint16_t level, MEM_XDATA uint8_t max_word_len) {
  const uint8_t split = koch_split(level);
  uint8_t length;

  if (max_word_len < KOCH_WORD_LEN_MIN) {
    max_word_len = KOCH_WORD_LEN_MIN;
  } else if (max_word_len > KOCH_WORD_LEN_MAX) {
    max_word_len = KOCH_WORD_LEN_MAX;
  }
  if (split < KOCH_SPLIT_WORD) {
    return 0U;
  }
  if (split == KOCH_SPLIT_WORD) {
    /* Word session number w = L / 3 + 1; the length grows by one every two sessions: 2 + (w - 1) / 2. */
    length = (uint8_t)(KOCH_WORD_LEN_MIN + (uint8_t)(koch_group >> 1));
    if (length < max_word_len) {
      return length;
    }
  }
  return max_word_len;
}

uint8_t koch_alphabet_count(uint16_t level) {
  const uint8_t count = koch_learned_count(level);

  if ((koch_kind(level) == KOCH_KIND_WORD) && (count > KOCH_WORD_ALPHABET_MAX)) {
    return KOCH_WORD_ALPHABET_MAX;
  }
  return count;
}

uint16_t koch_shown_level(uint16_t level) {
  return (level != KOCH_LEVEL_MAX) ? (uint16_t)(level + 1U) : (uint16_t)KOCH_LEVEL_MAX;
}

uint8_t koch_pass_threshold(koch_kind_t kind, MEM_XDATA uint8_t item_count) {
  uint8_t numerator = 9U;
  uint8_t denominator = 10U;

  if (kind == KOCH_KIND_WORD) {
    numerator = 4U;
    denominator = 5U;
  }
  /* ceil(p n / q) = (p n + p) / q for p / q = 9 / 10 or 4 / 5; 9 * 255 + 9 fits uint16_t. */
  return (uint8_t)((uint16_t)((uint16_t)((uint16_t)numerator * (uint16_t)item_count) + numerator) / denominator);
}
