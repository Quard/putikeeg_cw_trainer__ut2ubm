/*
 * QA scenarios of the Receiving mode with the four-button keyboard (references/internal/training_spec.md D-T27 to
 * D-T29, sections 8 and 12.2, AC-T8, AC-T19, AC-T23, AC-T27, R-T7; ui-ux.md sections 5.12.1, 6.8 and 9.4, UX-51).
 * The real menu, buttons, engine, keyboard and progress run with a fake player and fake items. The oracles come
 * from the documents: the run split is computed here from (c + 3 - k) / 4, and the label fields from their pixel
 * ranges, not from receiving.c or keyboard.c.
 */

#include "koch/keyboard.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define LEVEL_KM 0U
#define LEVEL_KMRS 3U
#define LEVEL_WORDS_13 17U /* KMRSUAPTLOWI., words of 4 */
#define LEVEL_ALL_CHARS 61U
#define LEVEL_ALL_WORDS 62U
#define ROW7_Y 56
#define FB_SIZE ((size_t)LCD_PAGE_COUNT * LCD_WIDTH)

/* Label fields of ui-ux.md section 5.12.1 in pixels, left to right: menu/back, prev, next, enter. */
static const int field_x0[KEYBOARD_GROUP_COUNT] = {2, 30, 58, 87};
static const int field_x1[KEYBOARD_GROUP_COUNT] = {26, 54, 82, 126};
/* The run under each button, left to right (D-T27): menu/back, prev, next, enter. */
static const button_event_t run_taps[KEYBOARD_GROUP_COUNT] = {L_TAP, M_TAP, R_TAP, ENTER_EVENT};
static const uint8_t run_pins[KEYBOARD_GROUP_COUNT] = {BUTTON_BIT_MENU, BUTTON_BIT_PREV, BUTTON_BIT_NEXT,
                                                       BUTTON_BIT_ENTER};

static uint8_t screen_before[FB_SIZE];

static void snapshot(uint8_t* buffer) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&buffer[(size_t)page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
}

static bool is_screen_unchanged(void) {
  uint8_t now_image[FB_SIZE];
  snapshot(now_image);
  return memcmp(screen_before, now_image, FB_SIZE) == 0;
}

static void clear_dirty_pages(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
}

/* Takes the screen and clears the dirty pages, so a later check can prove "no redraw". */
static void mark_quiet_start(void) {
  snapshot(screen_before);
  clear_dirty_pages();
}

static bool is_quiet_since_mark(void) { return is_screen_unchanged() && (lcd_fb_dirty_mask() == 0U); }

static void rx_open_items(uint16_t level, uint8_t prosign_style, const char* const* items, size_t count) {
  size_t index;

  fake_iap_reset();
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, level);
  }
  harness_power_on();
  (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, prosign_style);
  fake_koch_item_reset();
  for (index = 0U; index < count; ++index) {
    fake_koch_item_push(items[index]);
  }
  harness_open_mode(0U);
}

static void rx_open(uint16_t level, uint8_t prosign_style, const char* item) {
  const char* const items[1] = {item};
  rx_open_items(level, prosign_style, items, 1U);
}

static void tap(button_event_t event) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(event);
}

static void run_to_phase(training_phase_t phase) {
  unsigned guard = 0U;
  while ((training_phase() != phase) && (guard < 20000U)) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  }
  TEST_ASSERT_EQUAL(phase, training_phase());
}

static void run_to_item(void) { run_to_phase(TRAINING_PHASE_ITEM); }

/* A real press of @p pin for @p duration ms, then the release through the debounce. */
static void pin_press(uint8_t pin, uint16_t duration) {
  harness_pins_for(pin, duration);
  harness_pins_for(0U, (uint16_t)(BUTTONS_DEBOUNCE_MS + 10U));
}

/* A character level with @p learned characters (training_spec.md section 6: two levels per new character). */
static uint16_t char_level_of(uint8_t learned) {
  unsigned level;
  for (level = 0U; level < KOCH_FINAL_LEVEL; ++level) {
    if ((koch_kind((uint16_t)level) == KOCH_KIND_CHAR) && (koch_learned_count((uint16_t)level) == learned)) {
      return (uint16_t)level;
    }
  }
  TEST_ASSERT(false);
  return 0U;
}

/* ---------- Independent model of section 8 ---------- */

/* Run k of a range of c has (c + 3 - k) / 4 characters (D-T27). */
static uint8_t model_run_size(uint8_t count, uint8_t run) { return (uint8_t)((count + 3U - run) / 4U); }

/* The keyboard list of @p learned Koch characters: keyboard order, Koch index below @p learned. */
static uint8_t model_list(uint8_t learned, char* list) {
  uint8_t count = 0U;
  uint8_t k;
  for (k = 0U; k < KOCH_CHAR_COUNT; ++k) {
    const char c = koch_keyboard_char(k);
    if (koch_index_of(c) < learned) {
      list[count++] = c;
    }
  }
  return count;
}

/* The runs (buttons) that pick list offset @p target of a list of @p count. Returns the number of selects. */
static uint8_t model_path(uint8_t count, uint8_t target, uint8_t* runs) {
  uint8_t first = 0U;
  uint8_t selects = 0U;
  for (;;) {
    uint8_t run;
    uint8_t run_first = first;
    for (run = 0U; run < KEYBOARD_GROUP_COUNT; ++run) {
      const uint8_t size = model_run_size(count, run);
      if ((target >= run_first) && (target < (uint8_t)(run_first + size))) {
        break;
      }
      run_first = (uint8_t)(run_first + size);
    }
    runs[selects++] = run;
    if (model_run_size(count, run) == 1U) {
      return selects;
    }
    first = run_first;
    count = model_run_size(count, run);
  }
}

/* ---------- Tests ---------- */

/*
 * Long prev (section 12.2, ui-ux.md 6.8 undo): step out one level per hold, first out of every narrowing, and only at
 * the whole set undo the last pick. A 40-character word keyboard gives two narrowings (10 -> 3 -> 1).
 */
static void test_long_prev_steps_out_before_undo(void) {
  rx_open(LEVEL_ALL_WORDS, SETTINGS_PROSIGN_BOTH, "DEFG");
  run_to_item();
  TEST_ASSERT(keys("A-J", "K-T", "U-3", "4-/"));
  tap(L_TAP);
  TEST_ASSERT(keys("ABC", "DEF", "GH", "IJ"));
  tap(M_TAP);
  TEST_ASSERT(keys("D", "E", "F", ""));
  tap(L_TAP); /* picks D */
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  TEST_ASSERT(keys("A-J", "K-T", "U-3", "4-/"));
  tap(L_TAP);
  tap(M_TAP);
  TEST_ASSERT_EQUAL(2U, keyboard_depth());

  pin_press(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 50U));
  TEST_ASSERT_EQUAL(1U, keyboard_depth());
  TEST_ASSERT(keys("ABC", "DEF", "GH", "IJ"));
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  pin_press(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 50U));
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  TEST_ASSERT(keys("A-J", "K-T", "U-3", "4-/"));
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  pin_press(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 50U));
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  item_expect_reset();
  item_expect(ITEM_PART_BLOCK, 36, NULL);
  item_expect(ITEM_PART_BAR, 52, NULL);
  item_expect(ITEM_PART_BAR, 68, NULL);
  item_expect(ITEM_PART_BAR, 84, NULL);
  TEST_ASSERT(item_line_is());

  /* Nothing left to undo: no redraw, no dirty page, no player call. */
  {
    const size_t calls = fake_player_call_count();
    mark_quiet_start();
    tap(LONG_EVENT(M_TAP));
    TEST_ASSERT(is_quiet_since_mark());
    TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  }
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
}

/* The field of run @p run holds pixel x. */
static bool is_in_field(int x, uint8_t run) { return (x >= field_x0[run]) && (x <= field_x1[run]); }

static bool field_has_ink(uint8_t run) {
  int x;
  int y;
  for (y = ROW7_Y; y < ROW7_Y + 8; ++y) {
    for (x = field_x0[run]; x <= field_x1[run]; ++x) {
      if (pixel(x, y)) {
        return true;
      }
    }
  }
  return false;
}

/* Every ink pixel of row 7 is in the field of a non-empty run, and every non-empty run shows a label. */
static bool labels_fit_fields(void) {
  int x;
  int y;
  uint8_t run;

  for (y = ROW7_Y; y < ROW7_Y + 8; ++y) {
    for (x = 0; x < 128; ++x) {
      bool is_allowed = false;
      if (!pixel(x, y)) {
        continue;
      }
      for (run = 0U; run < KEYBOARD_GROUP_COUNT; ++run) {
        if ((keyboard_group_size(run) != 0U) && is_in_field(x, run)) {
          is_allowed = true;
        }
      }
      if (!is_allowed) {
        fprintf(stderr, "label ink outside its field: (%d, %d)\n", x, y);
        return false;
      }
    }
  }
  for (run = 0U; run < KEYBOARD_GROUP_COUNT; ++run) {
    if ((keyboard_group_size(run) != 0U) != field_has_ink(run)) {
      fprintf(stderr, "run %u: size %u, label ink %d\n", run, keyboard_group_size(run), field_has_ink(run));
      return false;
    }
  }
  return true;
}

/* The run sizes of the current range follow (c + 3 - k) / 4 for c = the sum of the four sizes. */
static bool sizes_follow_split(void) {
  uint8_t count = 0U;
  uint8_t run;
  for (run = 0U; run < KEYBOARD_GROUP_COUNT; ++run) {
    count = (uint8_t)(count + keyboard_group_size(run));
  }
  for (run = 0U; run < KEYBOARD_GROUP_COUNT; ++run) {
    if (keyboard_group_size(run) != model_run_size(count, run)) {
      return false;
    }
  }
  return true;
}

static unsigned label_checks;

/* Visits every range below the current one with the narrowing taps, and steps back with a long prev. */
static void visit_ranges(unsigned depth) {
  uint8_t run;

  ++label_checks;
  if (!TEST_ASSERT(labels_fit_fields()) || !TEST_ASSERT(key_bars()) || !TEST_ASSERT(margins_off()) ||
      !TEST_ASSERT(sizes_follow_split())) {
    return;
  }
  TEST_ASSERT(depth < KEYBOARD_DEPTH_MAX);
  for (run = 0U; run < KEYBOARD_GROUP_COUNT; ++run) {
    if (keyboard_group_size(run) < 2U) {
      continue; /* a run of one picks */
    }
    tap(run_taps[run]);
    if (!TEST_ASSERT_EQUAL(depth + 1U, keyboard_depth())) {
      return;
    }
    visit_ranges(depth + 1U);
    tap(LONG_EVENT(M_TAP));
    if (!TEST_ASSERT_EQUAL(depth, keyboard_depth())) {
      return;
    }
  }
}

/*
 * ui-ux.md section 5.12.1, LC-1 to LC-6: in every reachable range of every Koch prefix (character levels for 2..43,
 * the 40-character word keyboard), for all three Prosigns values, each label stays in its field, an empty run has no
 * ink, the key bars stay, x 0, 1 and 127 stay off, and the sizes follow section 8.
 */
static void test_labels_fit_every_range_of_every_prefix(void) {
  static const uint8_t styles[] = {SETTINGS_PROSIGN_PUNCTUATION, SETTINGS_PROSIGN_NAME, SETTINGS_PROSIGN_BOTH};
  uint8_t learned;
  size_t style;

  label_checks = 0U;
  for (learned = KOCH_FIRST_LEARNED; learned <= KOCH_CHAR_COUNT + 1U; ++learned) {
    const bool is_words = (learned > KOCH_CHAR_COUNT);
    const uint16_t level = is_words ? LEVEL_ALL_WORDS : char_level_of(learned);
    for (style = 0U; style < sizeof(styles); ++style) {
      rx_open(level, styles[style], is_words ? "KMRS" : "K");
      run_to_item();
      if (!TEST_ASSERT_EQUAL(is_words ? KOCH_WORD_ALPHABET_MAX : learned, keyboard_count())) {
        return;
      }
      visit_ranges(0U);
      TEST_ASSERT_EQUAL(0U, training_answer_length());
      TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
    }
  }
  printf("label screens: %u\n", label_checks);
  TEST_ASSERT(label_checks > 1000U);
}

/*
 * AC-T8 and AC-T27 at the mode level: for 2, 3, 4, 5, 9, 13, 16, 17 and 43 learned characters, every character of
 * the list is picked by the button path that the section 8 split gives, with exactly that number of taps. Every
 * fifth character uses the real pins. Each pick is the item, so each gives Correct and counts in the score.
 */
static void test_every_character_by_its_button_path(void) {
  static const uint8_t counts[] = {2U, 3U, 4U, 5U, 9U, 13U, 16U, 17U, 43U};
  size_t count_index;
  unsigned pin_picks = 0U;

  for (count_index = 0U; count_index < sizeof(counts); ++count_index) {
    const uint8_t learned = counts[count_index];
    char list[KOCH_CHAR_COUNT];
    char texts[KOCH_CHAR_COUNT][2];
    const char* items[KOCH_CHAR_COUNT];
    uint8_t target;
    uint8_t max_selects = 0U;

    TEST_ASSERT_EQUAL(learned, model_list(learned, list));
    for (target = 0U; target < learned; ++target) {
      texts[target][0] = list[target];
      texts[target][1] = '\0';
      items[target] = texts[target];
    }
    rx_open_items(char_level_of(learned), SETTINGS_PROSIGN_BOTH, items, learned);
    for (target = 0U; target < learned; ++target) {
      uint8_t runs[KEYBOARD_DEPTH_MAX + 1U];
      const uint8_t selects = model_path(learned, target, runs);
      uint8_t step;

      run_to_item();
      if (!TEST_ASSERT_EQUAL((uint8_t)list[target], (uint8_t)training_item_text()[0])) {
        return;
      }
      if (selects > max_selects) {
        max_selects = selects;
      }
      for (step = 0U; step < selects; ++step) {
        if (!TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase()) ||
            !TEST_ASSERT_EQUAL(0U, training_answer_length())) {
          return;
        }
        if ((target % 5U) == 0U) {
          pin_press(run_pins[runs[step]], 40U);
        } else {
          tap(run_taps[runs[step]]);
        }
      }
      if (!TEST_ASSERT(status("Correct"))) {
        fprintf(stderr, "%u learned, target %u (%c), %u selects\n", learned, target, list[target], selects);
        return;
      }
      if ((target % 5U) == 0U) {
        ++pin_picks;
      }
      TEST_ASSERT_EQUAL(0U, keyboard_depth());
    }
    TEST_ASSERT_EQUAL(learned, training_score());
    /* Section 8: 1 select for 2..4, at most 2 for 5..16, 3 for 17 and more (11 -> 3 -> 1 for 43). */
    TEST_ASSERT_EQUAL((learned <= 4U) ? 1U : ((learned <= 16U) ? 2U : 3U), max_selects);
    TEST_ASSERT_EQUAL(0U, harness_overlap_count);
  }
  TEST_ASSERT(pin_picks > 20U);
}

/*
 * R-T7, D-T29 (4): a plug-insertion glitch on P3.2 (a short closure) is an enter tap. With four characters the enter
 * run holds one character: the glitch picks it into the last slot, the check comes at once and an attempt is used.
 * A long prev cannot undo the check.
 */
static void test_glitch_fills_last_slot_and_uses_attempt(void) {
  rx_open(LEVEL_KMRS, SETTINGS_PROSIGN_BOTH, "K");
  run_to_item();
  TEST_ASSERT(keys("K", "M", "R", "S"));
  pin_press(BUTTON_BIT_ENTER, 35U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());
  TEST_ASSERT(status("Wrong"));
  item_expect_reset();
  item_expect(ITEM_PART_BIG, 60, "S");
  TEST_ASSERT(item_line_is());
  TEST_ASSERT_EQUAL(FAKE_PLAYER_CALL_STOP, fake_player_call_at(fake_player_call_count() - 1U));

  /* A long prev in RESULT does nothing. */
  mark_quiet_start();
  tap(LONG_EVENT(M_TAP));
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(1U, training_wrong_attempts());

  /* The retry: empty slot, top labels, the second attempt; the right pick still counts as not first-attempt. */
  run_to_item();
  TEST_ASSERT(status("Try 2 of 3"));
  TEST_ASSERT(keys("K", "M", "R", "S"));
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  tap(L_TAP);
  TEST_ASSERT(status("Correct"));
  TEST_ASSERT_EQUAL(0U, training_score());
}

/*
 * R-T7: a glitch that only narrows, or picks into an earlier slot, can be undone with long prev and uses no attempt.
 * A glitch in the start pause does nothing.
 */
static void test_glitch_narrow_or_early_pick_is_undone(void) {
  rx_open(LEVEL_WORDS_13, SETTINGS_PROSIGN_BOTH, "KMRS");
  /* Start pause. */
  harness_run_to(HARNESS_T0 + 300U);
  mark_quiet_start();
  pin_press(BUTTON_BIT_ENTER, 35U);
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());

  run_to_item();
  TEST_ASSERT(keys("AIKL", "MOP", "RST", "UW."));
  pin_press(BUTTON_BIT_ENTER, 35U);
  TEST_ASSERT_EQUAL(1U, keyboard_depth());
  TEST_ASSERT(keys("U", "W", ".", ""));
  pin_press(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 50U));
  TEST_ASSERT_EQUAL(0U, keyboard_depth());
  TEST_ASSERT(keys("AIKL", "MOP", "RST", "UW."));

  tap(L_TAP);
  TEST_ASSERT(keys("A", "I", "K", "L"));
  pin_press(BUTTON_BIT_ENTER, 35U); /* picks L into slot 1 */
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  TEST_ASSERT_EQUAL((uint8_t)'L', (uint8_t)training_answer_text()[0]);
  pin_press(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 50U));
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
}

/* Section 8 and ui-ux.md 5.12.1: a tap of an empty run inside a narrowed range changes nothing. */
static void test_empty_runs_in_narrowed_range(void) {
  rx_open(LEVEL_ALL_CHARS, SETTINGS_PROSIGN_NAME, "K");
  run_to_item();
  tap(ENTER_EVENT);
  tap(ENTER_EVENT);
  TEST_ASSERT(keys("<SK>", "<AR>", "", ""));
  TEST_ASSERT_EQUAL(2U, keyboard_depth());
  {
    const size_t calls = fake_player_call_count();
    mark_quiet_start();
    tap(R_TAP);
    tap(ENTER_EVENT);
    pin_press(BUTTON_BIT_NEXT, 40U);
    pin_press(BUTTON_BIT_ENTER, 40U);
    TEST_ASSERT(is_quiet_since_mark());
    TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  }
  TEST_ASSERT_EQUAL(2U, keyboard_depth());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  tap(M_TAP); /* AR */
  TEST_ASSERT(status("Wrong"));
  item_expect_reset();
  item_expect(ITEM_PART_BIG, 48, "<AR>");
  TEST_ASSERT(item_line_is());
}

/*
 * ui-ux.md 6.8, AC-T19: a long enter does nothing in the start pause, in RESULT (also during the answer playback
 * after the third wrong attempt), in the blank moment and on the end screen. A long next does nothing in ITEM.
 */
static void test_long_enter_outside_item_and_long_next(void) {
  const char* const items[2] = {"K", "K"};
  size_t calls;

  rx_open_items(LEVEL_KM, SETTINGS_PROSIGN_BOTH, items, 2U);
  harness_run_to(HARNESS_T0 + 200U);
  mark_quiet_start();
  tap(LONG_EVENT(ENTER_EVENT));
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());

  run_to_item();
  calls = fake_player_call_count();
  mark_quiet_start();
  tap(LONG_EVENT(R_TAP));
  pin_press(BUTTON_BIT_NEXT, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 300U));
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(calls, fake_player_call_count());

  /* Three wrong attempts: the answer plays; a long enter must not restart it or add a call. */
  tap(M_TAP);
  run_to_item();
  tap(M_TAP);
  run_to_item();
  tap(M_TAP);
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(fake_player_is_running());
  calls = fake_player_call_count();
  mark_quiet_start();
  tap(LONG_EVENT(ENTER_EVENT));
  pin_press(BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 50U));
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());

  /* Blank moment before item 2. */
  fake_player_finish();
  run_to_phase(TRAINING_PHASE_GAP);
  calls = fake_player_call_count();
  mark_quiet_start();
  tap(LONG_EVENT(ENTER_EVENT));
  tap(ENTER_EVENT);
  TEST_ASSERT(is_quiet_since_mark());
  TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  run_to_item();
  TEST_ASSERT_EQUAL(calls + 1U, fake_player_call_count());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
}

/* Answers every KM item right on the first attempt, then runs to the end screen. */
static void run_km_session_to_end(void) {
  unsigned guard = 0U;
  while ((training_phase() != TRAINING_PHASE_END) && (guard < 60U)) {
    run_to_item();
    fake_player_finish();
    tap((training_item_text()[0] == 'K') ? L_TAP : M_TAP);
    while ((training_phase() == TRAINING_PHASE_RESULT) && (guard < 60U)) {
      harness_set_time((uint16_t)(harness_now + 1U));
      harness_pass();
    }
    ++guard;
  }
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
}

/*
 * End screen (ui-ux.md 6.8, UX-51): a long enter and a long tip closure do nothing; a short tip closure (also a plug
 * glitch, D-5) starts the next session at its release; tap mode stays on in the new session.
 */
static void test_end_screen_long_enter_and_short_tip(void) {
  rx_open(LEVEL_KM, SETTINGS_PROSIGN_BOTH, "K");
  run_km_session_to_end();
  {
    const size_t calls = fake_player_call_count();
    mark_quiet_start();
    tap(LONG_EVENT(ENTER_EVENT));
    pin_press(BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 10U));
    TEST_ASSERT(is_quiet_since_mark());
    TEST_ASSERT_EQUAL(calls, fake_player_call_count());
  }
  TEST_ASSERT(next_button());
  harness_pins_for(BUTTON_BIT_ENTER, 60U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase()); /* nothing at the press */
  harness_pins_for(0U, BUTTONS_DEBOUNCE_MS + 1U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT(hdr("RX 32/32 L2          1/50"));
  TEST_ASSERT(keys("K", "M", "R", ""));
  /* Tap mode still on: a menu/back press gives nothing until its release, and then selects run 0 only in ITEM. */
  run_to_item();
  harness_pins_for(BUTTON_BIT_MENU, 60U);
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  harness_pins_for(0U, BUTTONS_DEBOUNCE_MS + 1U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
}

/*
 * V-125 variant: the enter that opens Receiving stays closed into ITEM (1300 ms) and ends with a release bounce. No
 * pick and no replay come from it. A new short closure then picks.
 */
static void test_opening_enter_held_into_item(void) {
  fake_iap_reset();
  progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, LEVEL_KMRS);
  harness_power_on();
  fake_koch_item_reset();
  fake_koch_item_push("S");
  harness_set_time((uint16_t)(HARNESS_T0 - BUTTONS_DEBOUNCE_MS - 1U));
  harness_pins_for(BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 1U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  harness_pins_for(BUTTON_BIT_ENTER, 1300U);
  harness_pins_for(0U, 4U);
  harness_pins_for(BUTTON_BIT_ENTER, 3U);
  harness_pins_for(0U, 200U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(1U, fake_player_call_count());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT(keys("K", "M", "R", "S"));
  pin_press(BUTTON_BIT_ENTER, 50U);
  TEST_ASSERT(status("Correct"));
}

/* AC-T19: a short menu/back is a run-0 select in ITEM, never a leave; only at END does it leave. */
static void test_short_menu_never_leaves_in_session(void) {
  rx_open(LEVEL_ALL_CHARS, SETTINGS_PROSIGN_BOTH, "K");
  harness_run_to(HARNESS_T0 + 300U);
  pin_press(BUTTON_BIT_MENU, 50U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  run_to_item();
  pin_press(BUTTON_BIT_MENU, 50U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT(keys("ABC", "DEF", "GHI", "JK"));
  pin_press(BUTTON_BIT_MENU, 50U);
  TEST_ASSERT(keys("A", "B", "C", ""));
  pin_press(BUTTON_BIT_MENU, 50U); /* picks A */
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT(status("Wrong"));
  pin_press(BUTTON_BIT_MENU, 50U); /* RESULT: nothing */
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT(status("Wrong"));
}

int main(void) {
  TEST_RUN(test_long_prev_steps_out_before_undo);
  TEST_RUN(test_labels_fit_every_range_of_every_prefix);
  TEST_RUN(test_every_character_by_its_button_path);
  TEST_RUN(test_glitch_fills_last_slot_and_uses_attempt);
  TEST_RUN(test_glitch_narrow_or_early_pick_is_undone);
  TEST_RUN(test_empty_runs_in_narrowed_range);
  TEST_RUN(test_long_enter_outside_item_and_long_next);
  TEST_RUN(test_end_screen_long_enter_and_short_tip);
  TEST_RUN(test_opening_enter_held_into_item);
  TEST_RUN(test_short_menu_never_leaves_in_session);
  return TEST_FINISH();
}
