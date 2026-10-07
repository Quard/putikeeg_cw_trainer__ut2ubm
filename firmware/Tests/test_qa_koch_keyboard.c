/*
 * QA tests of the four-run group keyboard (references/internal/training_spec.md section 8, D-T27, AC-T8). A random
 * walk of keyboard_select(), keyboard_back() and keyboard_top() with known and unknown groups runs against a
 * reference model built from section 8 alone: list in keyboard order, run k of c = (c + 3 - k) / 4, a history stack.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "koch/keyboard.h"
#include "koch/koch.h"
#include "support/test_assert.h"

#define MODEL_STACK_MAX 8U

typedef struct {
  char list[KOCH_CHAR_COUNT];
  uint8_t count;
  uint8_t first;
  uint8_t range;
  uint8_t stack_first[MODEL_STACK_MAX];
  uint8_t stack_range[MODEL_STACK_MAX];
  uint8_t depth;
} model_t;

static model_t model;

static uint32_t rng_state;
static uint32_t rng_next(void) {
  rng_state = rng_state * 1103515245U + 12345U;
  return rng_state >> 8;
}

static uint8_t model_size(uint8_t run) {
  return (run < KEYBOARD_GROUP_COUNT) ? (uint8_t)((model.range + 3U - run) / 4U) : 0U;
}

static uint8_t model_first(uint8_t run) {
  uint8_t first = model.first;
  uint8_t k;
  for (k = 0U; (k < run) && (k < KEYBOARD_GROUP_COUNT); ++k) {
    first = (uint8_t)(first + model_size(k));
  }
  return first;
}

static void model_reset(uint8_t learned) {
  uint8_t k;
  model.count = 0U;
  for (k = 0U; k < KOCH_CHAR_COUNT; ++k) {
    const char c = koch_keyboard_char(k);
    if (koch_index_of(c) < learned) {
      model.list[model.count++] = c;
    }
  }
  model.first = 0U;
  model.range = model.count;
  model.depth = 0U;
}

static char model_select(uint8_t run) {
  const uint8_t size = model_size(run);
  const uint8_t first = model_first(run);
  if (size == 0U) {
    return KEYBOARD_NO_PICK;
  }
  if (size == 1U) {
    model.first = 0U;
    model.range = model.count;
    model.depth = 0U;
    return model.list[first];
  }
  model.stack_first[model.depth] = model.first;
  model.stack_range[model.depth] = model.range;
  ++model.depth;
  model.first = first;
  model.range = size;
  return KEYBOARD_NO_PICK;
}

static bool model_back(void) {
  if (model.depth == 0U) {
    return false;
  }
  --model.depth;
  model.first = model.stack_first[model.depth];
  model.range = model.stack_range[model.depth];
  return true;
}

static bool state_matches(void) {
  uint8_t run;
  if (keyboard_depth() != model.depth) {
    return false;
  }
  for (run = 0U; run <= KEYBOARD_GROUP_COUNT; ++run) {
    if ((keyboard_group_size(run) != model_size(run)) || (keyboard_group_first(run) != model_first(run))) {
      return false;
    }
  }
  return (keyboard_group_size(255U) == 0U) && (keyboard_group_first(255U) == (uint8_t)(model.first + model.range));
}

/* Random walk of 4000 operations for every learned count 2..43, against the section 8 model. */
static void test_random_walk_against_model(void) {
  static const uint8_t groups[] = {0U, 1U, 2U, 3U, 3U, 4U, 5U, 255U};
  uint8_t learned;
  unsigned picks = 0U;
  unsigned narrowings = 0U;

  rng_state = 20261006U;
  for (learned = KOCH_FIRST_LEARNED; learned <= KOCH_CHAR_COUNT; ++learned) {
    unsigned step;
    uint8_t index;

    keyboard_reset(learned);
    model_reset(learned);
    if (!TEST_ASSERT_EQUAL(model.count, keyboard_count())) {
      return;
    }
    for (index = 0U; index < model.count; ++index) {
      TEST_ASSERT_EQUAL((uint8_t)model.list[index], (uint8_t)keyboard_char(index));
    }
    TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_char(model.count));
    for (step = 0U; step < 4000U; ++step) {
      const uint32_t choice = rng_next() % 12U;
      if (choice < 8U) {
        const uint8_t group = groups[choice];
        const uint8_t depth_before = model.depth;
        const char expected = model_select(group);
        const char actual = keyboard_select(group);
        if (!TEST_ASSERT_EQUAL((uint8_t)expected, (uint8_t)actual)) {
          fprintf(stderr, "learned %u step %u group %u\n", learned, step, group);
          return;
        }
        if (expected != KEYBOARD_NO_PICK) {
          ++picks;
        } else if (model.depth > depth_before) {
          ++narrowings;
        }
      } else if (choice < 11U) {
        TEST_ASSERT_EQUAL(model_back(), keyboard_back());
      } else {
        keyboard_top();
        model.first = 0U;
        model.range = model.count;
        model.depth = 0U;
      }
      if (!TEST_ASSERT(state_matches())) {
        fprintf(stderr, "learned %u step %u\n", learned, step);
        return;
      }
      TEST_ASSERT(keyboard_depth() <= (KEYBOARD_DEPTH_MAX - 1U));
    }
  }
  printf("random walk: %u picks, %u narrowings\n", picks, narrowings);
  TEST_ASSERT(picks > 10000U);
  TEST_ASSERT(narrowings > 10000U);
}

/* Out-of-range counts clamp to 2..43; a reset after a deep narrowing starts at the whole list at depth 0. */
static void test_reset_clamps_and_clears_history(void) {
  keyboard_reset(KOCH_CHAR_COUNT);
  (void)keyboard_select(KEYBOARD_GROUP_ENTER);
  (void)keyboard_select(KEYBOARD_GROUP_ENTER);
  TEST_ASSERT_EQUAL(2U, keyboard_depth());
  keyboard_reset(0U);
  TEST_ASSERT_EQUAL(KOCH_FIRST_LEARNED, keyboard_count());
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  TEST_ASSERT(!keyboard_back());
  keyboard_reset(255U);
  TEST_ASSERT_EQUAL(KOCH_CHAR_COUNT, keyboard_count());
  TEST_ASSERT_EQUAL(11U, keyboard_group_size(KEYBOARD_GROUP_MENU));
  TEST_ASSERT_EQUAL(10U, keyboard_group_size(KEYBOARD_GROUP_ENTER));
}

int main(void) {
  TEST_RUN(test_random_walk_against_model);
  TEST_RUN(test_reset_clamps_and_clears_history);
  return TEST_FINISH();
}
