#include "koch/keyboard.h"

#include "koch/koch.h"
#include "mcu/compiler.h"

/*
 * The largest group of c characters has ceil(c / 4), so KEYBOARD_DEPTH_MAX selects reach one character in 4^3 = 64.
 * The last select picks, so at most KEYBOARD_DEPTH_MAX - 1 narrowings go on the history.
 */
_Static_assert(KEYBOARD_GROUP_COUNT == 4U, "the reach below and the size formula need four groups");
_Static_assert(KEYBOARD_DEPTH_MAX == 3U, "update the reach below with KEYBOARD_DEPTH_MAX");
_Static_assert(KOCH_CHAR_COUNT <= (4U * 4U * 4U), "KEYBOARD_DEPTH_MAX selects must reach every character");
#define KEYBOARD_HISTORY_SIZE (KEYBOARD_DEPTH_MAX - 1U)

static MEM_XDATA char keyboard_list[KOCH_CHAR_COUNT];
static MEM_XDATA uint8_t keyboard_list_count;
static MEM_XDATA uint8_t keyboard_range_first;
static MEM_XDATA uint8_t keyboard_range_count;
static MEM_XDATA uint8_t keyboard_history_depth;
static MEM_XDATA uint8_t keyboard_history_first[KEYBOARD_HISTORY_SIZE];
static MEM_XDATA uint8_t keyboard_history_count[KEYBOARD_HISTORY_SIZE];
static MEM_XDATA uint8_t keyboard_sizes[KEYBOARD_GROUP_COUNT];
/* Work values in xdata, so the calls in the loops need no internal RAM to keep them. */
static MEM_XDATA uint8_t work_index;
static MEM_XDATA uint8_t work_limit;
static MEM_XDATA char work_char;

/*
 * Group k of a range of c has (c + 3 - k) / 4 characters (D-T27): the sizes differ by at most 1, larger groups first,
 * and their sum is c. count <= KOCH_CHAR_COUNT, so count + 3 does not overflow.
 */
static void keyboard_set_range(uint8_t first, MEM_XDATA uint8_t count) {
  uint8_t group;

  keyboard_range_first = first;
  keyboard_range_count = count;
  count = (uint8_t)(count + (KEYBOARD_GROUP_COUNT - 1U));
  for (group = 0U; group < KEYBOARD_GROUP_COUNT; ++group) {
    keyboard_sizes[group] = (uint8_t)(count / KEYBOARD_GROUP_COUNT);
    --count;
  }
}

void keyboard_reset(uint8_t koch_count) {
  if (koch_count < KOCH_FIRST_LEARNED) {
    koch_count = KOCH_FIRST_LEARNED;
  } else if (koch_count > KOCH_CHAR_COUNT) {
    koch_count = KOCH_CHAR_COUNT;
  }
  work_limit = koch_count;
  keyboard_list_count = 0U;
  for (work_index = 0U; work_index < KOCH_CHAR_COUNT; ++work_index) {
    work_char = koch_keyboard_char(work_index);
    if (koch_index_of(work_char) < work_limit) {
      keyboard_list[keyboard_list_count] = work_char;
      ++keyboard_list_count;
    }
  }
  keyboard_top();
}

uint8_t keyboard_count(void) { return keyboard_list_count; }

char keyboard_char(uint8_t index) { return (index < keyboard_list_count) ? keyboard_list[index] : KEYBOARD_NO_PICK; }

uint8_t keyboard_group_size(uint8_t group) { return (group < KEYBOARD_GROUP_COUNT) ? keyboard_sizes[group] : 0U; }

/* An unknown group gives the end of the range. */
uint8_t keyboard_group_first(uint8_t group) {
  uint8_t first = keyboard_range_first;
  uint8_t index;

  for (index = 0U; (index < group) && (index < KEYBOARD_GROUP_COUNT); ++index) {
    first += keyboard_sizes[index];
  }
  return first;
}

char keyboard_select(uint8_t group) {
  work_limit = keyboard_group_size(group);
  if (work_limit == 0U) {
    return KEYBOARD_NO_PICK;
  }
  work_index = keyboard_group_first(group);
  if (work_limit == 1U) {
    work_char = keyboard_list[work_index];
    keyboard_top();
    return work_char;
  }
  keyboard_history_first[keyboard_history_depth] = keyboard_range_first;
  keyboard_history_count[keyboard_history_depth] = keyboard_range_count;
  ++keyboard_history_depth;
  keyboard_set_range(work_index, work_limit);
  return KEYBOARD_NO_PICK;
}

bool keyboard_back(void) {
  if (keyboard_history_depth == 0U) {
    return false;
  }
  --keyboard_history_depth;
  keyboard_set_range(keyboard_history_first[keyboard_history_depth], keyboard_history_count[keyboard_history_depth]);
  return true;
}

void keyboard_top(void) {
  keyboard_history_depth = 0U;
  keyboard_set_range(0U, keyboard_list_count);
}

uint8_t keyboard_depth(void) { return keyboard_history_depth; }
