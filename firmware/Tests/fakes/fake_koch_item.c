#include "fake_koch_item.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "koch/koch.h"

static char fake_items[FAKE_KOCH_ITEM_FIFO_SIZE][KOCH_WORD_LEN_MAX + 1U];
static size_t fake_item_count;
static size_t fake_calls;

static void fake_fail(const char* reason) {
  fprintf(stderr, "fake_koch_item: %s\n", reason);
  exit(EXIT_FAILURE);
}

void fake_koch_item_reset(void) {
  fake_item_count = 0U;
  fake_calls = 0U;
}

void fake_koch_item_push(const char* text) {
  const size_t length = strlen(text);

  if (fake_item_count >= FAKE_KOCH_ITEM_FIFO_SIZE) {
    fake_fail("FIFO overflow");
  }
  if ((length == 0U) || (length > KOCH_WORD_LEN_MAX)) {
    fake_fail("item length out of range");
  }
  memcpy(fake_items[fake_item_count], text, length + 1U);
  ++fake_item_count;
}

size_t fake_koch_item_count(void) { return fake_calls; }

uint8_t koch_make_item(uint16_t level, uint8_t max_word_len, char* text) {
  const size_t index = (fake_calls < fake_item_count) ? fake_calls : (fake_item_count - 1U);
  const size_t length = (fake_item_count == 0U) ? 0U : strlen(fake_items[index]);

  (void)level;
  (void)max_word_len;
  if (fake_item_count == 0U) {
    fake_fail("no item pushed");
  }
  ++fake_calls;
  memcpy(text, fake_items[index], length + 1U);
  return (uint8_t)length;
}
