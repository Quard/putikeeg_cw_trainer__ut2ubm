#include "koch/koch.h"
#include "mcu/compiler.h"
#include "util/rng.h"

/* Work values in xdata, so the calls below need no internal RAM to keep them. */
static MEM_XDATA uint8_t item_length;
static MEM_XDATA uint8_t item_position;
/* CHAR level: Koch index of the item. WORD level: characters that a word can hold. */
static MEM_XDATA uint8_t item_value;

uint8_t koch_make_item(uint16_t level, MEM_XDATA uint8_t max_word_len, char MEM_XDATA* MEM_XDATA text) {
  if (koch_kind(level) == KOCH_KIND_CHAR) {
    /* Half of the items are the new character; the older characters share the other half evenly. */
    item_value = (uint8_t)(koch_learned_count(level) - 1U);
    if (rng_below(2U) != 0U) {
      item_value = rng_below(item_value);
    }
    text[0] = koch_char(item_value);
    item_length = 1U;
  } else {
    item_value = koch_alphabet_count(level);
    item_length = koch_word_length(level, max_word_len);
    for (item_position = 0U; item_position < item_length; ++item_position) {
      text[item_position] = koch_char(rng_below(item_value));
    }
  }
  text[item_length] = '\0';
  return item_length;
}
