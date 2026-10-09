/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/* Host tests of the Receiving group keyboard (references/internal/training_spec.md section 8, AC-T8). */

#include <stdbool.h>
#include <stdint.h>

#include "cw/morse.h"
#include "koch/keyboard.h"
#include "koch/koch.h"
#include "test_assert.h"

#define SELECTS_MAX 3U

static uint8_t keyboard_order_index(char character) {
  uint8_t index;

  for (index = 0U; index < KOCH_CHAR_COUNT; ++index) {
    if (koch_keyboard_char(index) == character) {
      break;
    }
  }
  return index;
}

static uint8_t range_count(void) {
  return (uint8_t)(keyboard_group_size(KEYBOARD_GROUP_MENU) + keyboard_group_size(KEYBOARD_GROUP_PREV) +
                   keyboard_group_size(KEYBOARD_GROUP_NEXT) + keyboard_group_size(KEYBOARD_GROUP_ENTER));
}

/* Section 8 (D-T27): run k of a range of c has (c + 3 - k) / 4 characters. */
static uint8_t model_size(uint8_t count, uint8_t group) { return (uint8_t)((count + 3U - group) / 4U); }

/* Selects that pick list offset @p offset of a range of @p count, by the section 8 split alone. */
static unsigned model_selects(uint8_t count, uint8_t offset) {
  unsigned selects = 0U;

  for (;;) {
    uint8_t group;
    uint8_t first = 0U;
    uint8_t size = 0U;

    ++selects;
    for (group = 0U; group < 4U; ++group) {
      size = model_size(count, group);
      if (offset < (uint8_t)(first + size)) {
        break;
      }
      first = (uint8_t)(first + size);
    }
    if ((size <= 1U) || (selects > 8U)) {
      return selects;
    }
    offset = (uint8_t)(offset - first);
    count = size;
  }
}

/* The four runs follow the section 8 sizes and are contiguous from the range start. */
static void check_groups(uint8_t first, uint8_t count) {
  const uint8_t s0 = model_size(count, 0U);
  const uint8_t s1 = model_size(count, 1U);
  const uint8_t s2 = model_size(count, 2U);
  const uint8_t s3 = model_size(count, 3U);

  TEST_ASSERT_EQUAL(count, s0 + s1 + s2 + s3);
  TEST_ASSERT(s0 >= s1);
  TEST_ASSERT(s1 >= s2);
  TEST_ASSERT(s2 >= s3);
  TEST_ASSERT((s3 + 1U) >= s0);
  TEST_ASSERT_EQUAL(s0, keyboard_group_size(KEYBOARD_GROUP_MENU));
  TEST_ASSERT_EQUAL(s1, keyboard_group_size(KEYBOARD_GROUP_PREV));
  TEST_ASSERT_EQUAL(s2, keyboard_group_size(KEYBOARD_GROUP_NEXT));
  TEST_ASSERT_EQUAL(s3, keyboard_group_size(KEYBOARD_GROUP_ENTER));
  TEST_ASSERT_EQUAL(first, keyboard_group_first(KEYBOARD_GROUP_MENU));
  TEST_ASSERT_EQUAL(first + s0, keyboard_group_first(KEYBOARD_GROUP_PREV));
  TEST_ASSERT_EQUAL(first + s0 + s1, keyboard_group_first(KEYBOARD_GROUP_NEXT));
  TEST_ASSERT_EQUAL(first + s0 + s1 + s2, keyboard_group_first(KEYBOARD_GROUP_ENTER));
  TEST_ASSERT_EQUAL(0U, keyboard_group_size(KEYBOARD_GROUP_COUNT));
  TEST_ASSERT_EQUAL(0U, keyboard_group_size(255U));
  TEST_ASSERT_EQUAL(first + count, keyboard_group_first(KEYBOARD_GROUP_COUNT));
  TEST_ASSERT_EQUAL(first + count, keyboard_group_first(255U));
}

static void test_list_for_every_count(void) {
  uint8_t count;
  uint8_t index;

  for (count = KOCH_FIRST_LEARNED; count <= KOCH_CHAR_COUNT; ++count) {
    keyboard_reset(count);
    TEST_ASSERT_EQUAL(count, keyboard_count());
    TEST_ASSERT_EQUAL(0U, keyboard_depth());
    for (index = 0U; index < count; ++index) {
      TEST_ASSERT(koch_index_of(keyboard_char(index)) < count);
      if (index > 0U) {
        TEST_ASSERT(keyboard_order_index(keyboard_char((uint8_t)(index - 1U))) <
                    keyboard_order_index(keyboard_char(index)));
      }
    }
    TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_char(count));
    TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_char(255U));
    check_groups(0U, count);
  }
}

/* The results listed in section 8. */
static void test_reference_group_sizes(void) {
  static const uint8_t rows[][5] = {
      {2U, 1U, 1U, 0U, 0U},  {3U, 1U, 1U, 1U, 0U},      {4U, 1U, 1U, 1U, 1U},
      {5U, 2U, 1U, 1U, 1U},  {9U, 3U, 2U, 2U, 2U},      {13U, 4U, 3U, 3U, 3U},
      {17U, 5U, 4U, 4U, 4U}, {40U, 10U, 10U, 10U, 10U}, {43U, 11U, 11U, 11U, 10U},
  };
  size_t row;

  for (row = 0U; row < sizeof(rows) / sizeof(rows[0]); ++row) {
    keyboard_reset(rows[row][0]);
    TEST_ASSERT_EQUAL(rows[row][1], keyboard_group_size(KEYBOARD_GROUP_MENU));
    TEST_ASSERT_EQUAL(rows[row][2], keyboard_group_size(KEYBOARD_GROUP_PREV));
    TEST_ASSERT_EQUAL(rows[row][3], keyboard_group_size(KEYBOARD_GROUP_NEXT));
    TEST_ASSERT_EQUAL(rows[row][4], keyboard_group_size(KEYBOARD_GROUP_ENTER));
  }
}

/* Selects per pick: 1 for 2..4 characters, at most 2 for 5..16, at most 3 for 17..64 (section 8). */
static unsigned selects_limit(uint8_t count) {
  if (count <= 4U) {
    return 1U;
  }
  return (count <= 16U) ? 2U : 3U;
}

/*
 * Every character is reachable with exactly the select count of the section 8 split, at most SELECTS_MAX, and a pick
 * returns to the whole list.
 */
static void test_every_character_reachable(void) {
  uint8_t count;
  uint8_t target;

  for (count = KOCH_FIRST_LEARNED; count <= KOCH_CHAR_COUNT; ++count) {
    keyboard_reset(count);
    for (target = 0U; target < count; ++target) {
      unsigned selects = 0U;
      char picked = KEYBOARD_NO_PICK;

      while ((picked == KEYBOARD_NO_PICK) && (selects < 8U)) {
        uint8_t group;
        uint8_t first = 0U;
        uint8_t size = 0U;

        for (group = 0U; group < KEYBOARD_GROUP_COUNT; ++group) {
          first = keyboard_group_first(group);
          size = keyboard_group_size(group);
          if ((target >= first) && (target < (uint8_t)(first + size))) {
            break;
          }
        }
        if (!TEST_ASSERT(group < KEYBOARD_GROUP_COUNT)) {
          return;
        }
        picked = keyboard_select(group);
        ++selects;
        if (picked == KEYBOARD_NO_PICK) {
          TEST_ASSERT_EQUAL(selects, keyboard_depth());
          TEST_ASSERT(keyboard_depth() <= (KEYBOARD_DEPTH_MAX - 1U));
          check_groups(first, size);
        }
      }
      TEST_ASSERT_EQUAL((uint8_t)keyboard_char(target), (uint8_t)picked);
      TEST_ASSERT_EQUAL(model_selects(count, target), selects);
      TEST_ASSERT(selects <= selects_limit(count));
      TEST_ASSERT(selects <= SELECTS_MAX);
      TEST_ASSERT_EQUAL(0U, keyboard_depth());
      TEST_ASSERT_EQUAL(count, range_count());
      check_groups(0U, count);
    }
  }
}

static void test_empty_and_unknown_group(void) {
  /* 2 characters: groups 1/1/0/0, so NEXT and ENTER select nothing. */
  keyboard_reset(2U);
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_NEXT));
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_ENTER));
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  check_groups(0U, 2U);

  /* 3 characters: groups 1/1/1/0, so ENTER selects nothing. */
  keyboard_reset(3U);
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_ENTER));
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  check_groups(0U, 3U);
  TEST_ASSERT_EQUAL('R', keyboard_select(KEYBOARD_GROUP_NEXT));
  TEST_ASSERT_EQUAL(0U, keyboard_depth());

  /* 5 characters: 2/1/1/1; the MENU group has 2, so 1/1/0/0 inside it. */
  keyboard_reset(5U);
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_MENU));
  TEST_ASSERT_EQUAL(1U, keyboard_depth());
  check_groups(0U, 2U);
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_NEXT));
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_ENTER));
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_COUNT));
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(255U));
  TEST_ASSERT_EQUAL(1U, keyboard_depth());
  check_groups(0U, 2U);
  /* An unknown group starts at the end of the range. */
  TEST_ASSERT_EQUAL(2U, keyboard_group_first(KEYBOARD_GROUP_COUNT));
  TEST_ASSERT_EQUAL(2U, keyboard_group_first(255U));
}

static void test_back_restores_in_reverse_order(void) {
  keyboard_reset(KOCH_CHAR_COUNT);
  TEST_ASSERT(!keyboard_back());
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_NEXT)); /* 11/11/11/10: 22..32 */
  check_groups(22U, 11U);
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_ENTER)); /* 11: 3/3/3/2, ENTER 31..32 */
  check_groups(31U, 2U);
  TEST_ASSERT_EQUAL(KEYBOARD_DEPTH_MAX - 1U, keyboard_depth());

  TEST_ASSERT(keyboard_back());
  TEST_ASSERT_EQUAL(1U, keyboard_depth());
  check_groups(22U, 11U);
  TEST_ASSERT(keyboard_back());
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  check_groups(0U, KOCH_CHAR_COUNT);
  TEST_ASSERT(!keyboard_back());
  check_groups(0U, KOCH_CHAR_COUNT);

  /* A back after a pick: the pick returned to the whole list, so there is nothing to restore. */
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_ENTER)); /* 33..42 */
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_ENTER)); /* 10: 3/3/2/2, ENTER 41..42 */
  TEST_ASSERT_EQUAL((uint8_t)MORSE_CHAR_AR, (uint8_t)keyboard_select(KEYBOARD_GROUP_PREV));
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  TEST_ASSERT(!keyboard_back());
  check_groups(0U, KOCH_CHAR_COUNT);

  /* keyboard_top() drops the history. */
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_MENU));
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_MENU));
  TEST_ASSERT_EQUAL(2U, keyboard_depth());
  keyboard_top();
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  check_groups(0U, KOCH_CHAR_COUNT);
  TEST_ASSERT(!keyboard_back());
}

/* Section 8: a word session with all characters learned has 40 characters, A-J, K-T, U-3, 4-/. */
static void test_word_keyboard_has_no_prosign(void) {
  uint8_t index;

  keyboard_reset(koch_alphabet_count(62U));
  TEST_ASSERT_EQUAL(40U, keyboard_count());
  for (index = 0U; index < keyboard_count(); ++index) {
    TEST_ASSERT(keyboard_char(index) != MORSE_CHAR_BT);
    TEST_ASSERT(keyboard_char(index) != MORSE_CHAR_SK);
    TEST_ASSERT(keyboard_char(index) != MORSE_CHAR_AR);
  }
  check_groups(0U, 40U);
  TEST_ASSERT_EQUAL('A', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_MENU)));
  TEST_ASSERT_EQUAL('J', keyboard_char(9U));
  TEST_ASSERT_EQUAL('K', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_PREV)));
  TEST_ASSERT_EQUAL('T', keyboard_char(19U));
  TEST_ASSERT_EQUAL('U', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_NEXT)));
  TEST_ASSERT_EQUAL('3', keyboard_char(29U));
  TEST_ASSERT_EQUAL('4', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_ENTER)));
  TEST_ASSERT_EQUAL('/', keyboard_char(39U));

  /* A character session with all characters has the three prosigns at the end of the keyboard order. */
  keyboard_reset(koch_alphabet_count(61U));
  TEST_ASSERT_EQUAL(43U, keyboard_count());
  TEST_ASSERT_EQUAL((uint8_t)MORSE_CHAR_BT, (uint8_t)keyboard_char(40U));
  TEST_ASSERT_EQUAL((uint8_t)MORSE_CHAR_SK, (uint8_t)keyboard_char(41U));
  TEST_ASSERT_EQUAL((uint8_t)MORSE_CHAR_AR, (uint8_t)keyboard_char(42U));

  /* Section 8 example: A-K, L-V, W-6, then the ENTER run 7-AR of 10 splits 3/3/2/2. */
  check_groups(0U, 43U);
  TEST_ASSERT_EQUAL('A', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_MENU)));
  TEST_ASSERT_EQUAL('K', keyboard_char(10U));
  TEST_ASSERT_EQUAL('L', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_PREV)));
  TEST_ASSERT_EQUAL('V', keyboard_char(21U));
  TEST_ASSERT_EQUAL('W', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_NEXT)));
  TEST_ASSERT_EQUAL('6', keyboard_char(32U));
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_ENTER));
  check_groups(33U, 10U);
  TEST_ASSERT_EQUAL('7', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_MENU)));
  TEST_ASSERT_EQUAL('.', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_PREV)));
  TEST_ASSERT_EQUAL('/', keyboard_char(keyboard_group_first(KEYBOARD_GROUP_NEXT)));
  TEST_ASSERT_EQUAL((uint8_t)MORSE_CHAR_BT, (uint8_t)keyboard_char(40U));
  TEST_ASSERT_EQUAL((uint8_t)MORSE_CHAR_SK, (uint8_t)keyboard_char(keyboard_group_first(KEYBOARD_GROUP_ENTER)));
}

static void test_count_clamp(void) {
  keyboard_reset(0U);
  TEST_ASSERT_EQUAL(2U, keyboard_count());
  TEST_ASSERT_EQUAL('K', keyboard_char(0U));
  TEST_ASSERT_EQUAL('M', keyboard_char(1U));
  keyboard_reset(1U);
  TEST_ASSERT_EQUAL(2U, keyboard_count());
  keyboard_reset(KOCH_CHAR_COUNT + 1U);
  TEST_ASSERT_EQUAL(KOCH_CHAR_COUNT, keyboard_count());
  keyboard_reset(255U);
  TEST_ASSERT_EQUAL(KOCH_CHAR_COUNT, keyboard_count());
  /* A reset in a narrowed range shows the whole new list. */
  TEST_ASSERT_EQUAL(KEYBOARD_NO_PICK, keyboard_select(KEYBOARD_GROUP_ENTER));
  keyboard_reset(3U);
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  check_groups(0U, 3U);
}

int main(void) {
  TEST_RUN(test_list_for_every_count);
  TEST_RUN(test_reference_group_sizes);
  TEST_RUN(test_every_character_reachable);
  TEST_RUN(test_empty_and_unknown_group);
  TEST_RUN(test_back_restores_in_reverse_order);
  TEST_RUN(test_word_keyboard_has_no_prosign);
  TEST_RUN(test_count_clamp);
  return TEST_FINISH();
}
