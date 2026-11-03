/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA scenarios of the TX Char Marker setting in Sending (references/internal/training_spec.md D-T63 to D-T65,
 * section 17.9, AC-T55, AC-T56, Q9 fault cases F10 to F14; ui-ux.md UX-79, UX-80, section 5.13.5). The real menu,
 * buttons, engine, settings and progress run with a fake keyer and fake items. Two oracles: the item rows (pages 3
 * and 4, y 24-39) against a reference drawing of the 17.9 behavior table, and a differential check of each screen
 * with On against the same screen with Off. Only the ITEM bar may differ; every other pixel is the same.
 */

#include "cw/morse.h"
#include "koch/koch.h"
#include "modes/training.h"
#include "support/training_harness.h"

#define LEVEL_KM 0U
#define LEVEL_WORDS_13 17U /* KMRSUAPTLOWI., words of 4, 10 items */
#define FB_SIZE ((size_t)LCD_PAGE_COUNT * LCD_WIDTH)
#define LONG_HOLD ((uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 100U))
#define NO_BAR (-1)
#define ITEM_PAGE_TOP 3U
#define ITEM_PAGE_BAR 4U
#define CHECKPOINTS_MAX 40U

/* The kind of a checkpoint in the 17.9 behavior table. */
typedef enum {
  CP_ITEM,     /* ITEM of a word item: bar at the slot with On, no bar with Off */
  CP_ERROR,    /* RESULT after WRONG or FAILED, also a skip: bar at the slot with On and Off */
  CP_NO_BAR,   /* RESULT after CORRECT, or a character item: no bar with On and Off */
  CP_NOT_ITEM, /* SELECT, GAP, END: the same screen with On and Off */
} cp_kind_t;

typedef struct {
  cp_kind_t kind;
  int slot;
  const char* word;
  uint8_t frame[FB_SIZE];
} checkpoint_t;

static checkpoint_t checkpoints[2][CHECKPOINTS_MAX];
static size_t checkpoint_count[2];
static size_t run_index;

static void snapshot(uint8_t* buffer) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&buffer[(size_t)page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
}

/* A valid settings record of layout @p version (1 to 4) with the default values and byte 14 = @p training_byte. */
static void write_settings_record(uint8_t version, uint8_t training_byte) {
  uint8_t* record = &fake_iap_memory()[EEPROM_MAP_SETTINGS_ADDRESS];
  static const uint8_t flags_by_version[5] = {0x00U, 0x01U, 0x09U, 0x69U, 0x69U};
  static const uint8_t fields[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x00U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                       0x14U, 0x02U, 0x00U, 0x05U, 0x08U, 0x20U, 0x00U, 0xFFU};
  memcpy(record, fields, SETTINGS_RECORD_SIZE);
  record[2] = version;
  record[10] = flags_by_version[version];
  if (version == 4U) {
    record[14] = training_byte;
    record[15] = crc8_compute(record, 15U);
  } else {
    record[14] = crc8_compute(record, 14U);
  }
}

static void prepare(bool is_marker_shown, bool is_unlimited, uint16_t level, const char* const* items, size_t count) {
  size_t index;
  fake_iap_reset();
  harness_marker(is_marker_shown);
  if (level != 0U) {
    progress_fixture_write(PROGRESS_MODE_SENDING, 20U, level);
  }
  harness_power_on();
  TEST_ASSERT_EQUAL(is_marker_shown ? 0U : 1U, settings_get(SETTINGS_ID_TX_MARKER_HIDDEN));
  (void)settings_set(SETTINGS_ID_TX_UNLIMITED, is_unlimited ? 1U : 0U);
  fake_koch_item_reset();
  for (index = 0U; index < count; ++index) {
    fake_koch_item_push(items[index]);
  }
}

static void tx_open(bool is_marker_shown, bool is_unlimited, uint16_t level, const char* const* items, size_t count) {
  prepare(is_marker_shown, is_unlimited, level, items, count);
  harness_open_mode(1U);
}

static void drain(void) {
  unsigned guard = 0U;
  do {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
    ++guard;
  } while ((fake_keyer_pending_count() != 0U) && (guard < 100U));
}

static void push(keyer_event_t event) {
  fake_keyer_push_event(event);
  drain();
}

/* The dot-dash text of @p character, from the morse table. */
static const char* code_of(char character) {
  static char text[8];
  const uint8_t pattern = morse_encode(character);
  uint8_t shift = 0U;
  size_t length = 0U;
  while ((uint8_t)(pattern >> shift) > 1U) {
    ++shift;
  }
  while (shift != 0U) {
    --shift;
    text[length] = (((pattern >> shift) & 1U) != 0U) ? '-' : '.';
    ++length;
  }
  text[length] = '\0';
  return text;
}

static void elements(const char* pattern) {
  size_t index;
  for (index = 0U; pattern[index] != '\0'; ++index) {
    push((pattern[index] == '-') ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
}

static void key_pattern(const char* pattern) {
  elements(pattern);
  push(KEYER_EVENT_CHAR_GAP);
}

static void key(const char* text) {
  size_t index;
  for (index = 0U; text[index] != '\0'; ++index) {
    key_pattern(code_of(text[index]));
  }
}

static void key_unknown(void) { key_pattern("........"); }

static void word_gap(void) { push(KEYER_EVENT_WORD_GAP); }

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

static void skip_now(void) {
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(LONG_EVENT(R_TAP));
}

/* Item line of ui-ux.md 5.13.1: slot s of a word of n characters at x = 68 - 8n + 16s; the bar under @p bar_slot. */
static bool item_line_with_bar(const char* word, int bar_slot) {
  static char glyphs[KOCH_WORD_LEN_MAX][2];
  const int count = (int)strlen(word);
  int slot;
  item_expect_reset();
  for (slot = 0; slot < count; ++slot) {
    glyphs[slot][0] = word[slot];
    glyphs[slot][1] = '\0';
    item_expect(ITEM_PART_BIG, 68 - 8 * count + 16 * slot, glyphs[slot]);
  }
  if (bar_slot >= 0) {
    item_expect(ITEM_PART_BAR, 68 - 8 * count + 16 * bar_slot, NULL);
  }
  return item_line_is();
}

static bool is_marker_run(void) { return run_index == 0U; }

/* The bar of a checkpoint in the current run, from the 17.9 table. */
static int expected_bar(cp_kind_t kind, int slot) {
  if (kind == CP_ERROR) {
    return slot;
  }
  if ((kind == CP_ITEM) && is_marker_run()) {
    return slot;
  }
  return NO_BAR;
}

/* Records a checkpoint and checks the item rows against the 17.9 table. */
static void checkpoint(cp_kind_t kind, const char* word, int slot) {
  checkpoint_t* cp;
  if (!TEST_ASSERT(checkpoint_count[run_index] < CHECKPOINTS_MAX)) {
    return;
  }
  cp = &checkpoints[run_index][checkpoint_count[run_index]];
  cp->kind = kind;
  cp->word = word;
  cp->slot = slot;
  snapshot(cp->frame);
  if (kind != CP_NOT_ITEM) {
    if (!TEST_ASSERT(item_line_with_bar(word, expected_bar(kind, slot)))) {
      fprintf(stderr, "run %s, checkpoint %zu (%s, slot %d)\n", is_marker_run() ? "On" : "Off",
              checkpoint_count[run_index], word, slot);
    }
  }
  ++checkpoint_count[run_index];
}

/* The bar rectangle of x0..x0+6, y 38-39 off in a copy of page 4. */
static void erase_bar(uint8_t* page, const char* word, int slot) {
  const int count = (int)strlen(word);
  const int x0 = 68 - 8 * count + 16 * slot;
  int x;
  for (x = x0; x < x0 + 7; ++x) {
    page[x] = (uint8_t)(page[x] & 0x3FU);
  }
}

/*
 * The On and Off runs of one scenario: every page other than 4 is the same at each checkpoint. Page 4 is the same,
 * except that an On ITEM of a word item has its bar. With Off, two ITEM checkpoints in a row (an accepted character)
 * have the same item rows (V-209, N11).
 */
static void compare_runs(const char* name) {
  size_t index;
  if (!TEST_ASSERT_EQUAL(checkpoint_count[0], checkpoint_count[1])) {
    return;
  }
  for (index = 0U; index < checkpoint_count[0]; ++index) {
    const checkpoint_t* on = &checkpoints[0][index];
    const checkpoint_t* off = &checkpoints[1][index];
    uint8_t page_on[LCD_WIDTH];
    uint8_t page;
    for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
      if (page == ITEM_PAGE_BAR) {
        continue;
      }
      if (!TEST_ASSERT_EQUAL(0, memcmp(&on->frame[page * LCD_WIDTH], &off->frame[page * LCD_WIDTH], LCD_WIDTH))) {
        fprintf(stderr, "%s checkpoint %zu: page %u differs\n", name, index, (unsigned)page);
      }
    }
    memcpy(page_on, &on->frame[ITEM_PAGE_BAR * LCD_WIDTH], LCD_WIDTH);
    if ((on->kind == CP_ITEM) && (strlen(on->word) > 1U)) {
      erase_bar(page_on, on->word, on->slot);
    }
    if (!TEST_ASSERT_EQUAL(0, memcmp(page_on, &off->frame[ITEM_PAGE_BAR * LCD_WIDTH], LCD_WIDTH))) {
      fprintf(stderr, "%s checkpoint %zu: page 4 differs by more than the ITEM bar\n", name, index);
    }
    if ((index > 0U) && (on->kind == CP_ITEM) && (checkpoints[0][index - 1U].kind == CP_ITEM) &&
        (strcmp(on->word, checkpoints[0][index - 1U].word) == 0)) {
      const uint8_t* before = checkpoints[1][index - 1U].frame;
      TEST_ASSERT_EQUAL(
          0, memcmp(&before[ITEM_PAGE_TOP * LCD_WIDTH], &off->frame[ITEM_PAGE_TOP * LCD_WIDTH], 2U * LCD_WIDTH));
      /* With On, the bar moves to the next slot: page 4 changes, page 3 does not. */
      TEST_ASSERT(memcmp(&checkpoints[0][index - 1U].frame[ITEM_PAGE_BAR * LCD_WIDTH],
                         &on->frame[ITEM_PAGE_BAR * LCD_WIDTH], LCD_WIDTH) != 0);
      TEST_ASSERT_EQUAL(0, memcmp(&checkpoints[0][index - 1U].frame[ITEM_PAGE_TOP * LCD_WIDTH],
                                  &on->frame[ITEM_PAGE_TOP * LCD_WIDTH], LCD_WIDTH));
    }
  }
}

/*
 * Scenario A, a word item with 3 attempts: ITEM, accepted characters, a wrong character, RETRY, a word-space error,
 * RETRY, an unknown pattern as the third wrong attempt (FAILED), GAP, the next item keyed correctly (CORRECT), GAP.
 * Also the start screen (SELECT).
 */
static void scenario_a(bool is_marker_shown) {
  const char* const items[2] = {"KMRS", "UAPT"};
  prepare(is_marker_shown, false, LEVEL_WORDS_13, items, 2U);
  harness_open_start_screen(1U);
  checkpoint(CP_NOT_ITEM, "", 0);
  harness_event(ENTER_EVENT);
  run_to_item();
  checkpoint(CP_ITEM, "KMRS", 0);
  key("K");
  checkpoint(CP_ITEM, "KMRS", 1);
  key("N");
  TEST_ASSERT(status("Wrong: N"));
  checkpoint(CP_ERROR, "KMRS", 1);
  run_to_item();
  checkpoint(CP_ITEM, "KMRS", 0);
  key("K");
  checkpoint(CP_ITEM, "KMRS", 1);
  key("M");
  checkpoint(CP_ITEM, "KMRS", 2);
  key("R");
  checkpoint(CP_ITEM, "KMRS", 3);
  word_gap();
  TEST_ASSERT(status("Wrong: pause too long"));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
  checkpoint(CP_ERROR, "KMRS", 3);
  run_to_item();
  checkpoint(CP_ITEM, "KMRS", 0);
  key("KM");
  checkpoint(CP_ITEM, "KMRS", 2);
  key_unknown();
  TEST_ASSERT(status("Wrong: *"));
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  checkpoint(CP_ERROR, "KMRS", 2);
  run_to_phase(TRAINING_PHASE_GAP);
  checkpoint(CP_NOT_ITEM, "", 0);
  run_to_item();
  checkpoint(CP_ITEM, "UAPT", 0);
  key("UAP");
  checkpoint(CP_ITEM, "UAPT", 3);
  key("T");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  checkpoint(CP_NO_BAR, "UAPT", 0);
  run_to_phase(TRAINING_PHASE_GAP);
  checkpoint(CP_NOT_ITEM, "", 0);
}

/*
 * Scenario B: an unknown pattern (WRONG), a wrong character, a word-space error as the third attempt (FAILED). Then a
 * skip in ITEM while a character is half keyed (F10); the first character of the next item counts.
 */
static void scenario_b(bool is_marker_shown) {
  const char* const items[3] = {"KMRS", "UAPT", "LOWI"};
  prepare(is_marker_shown, false, LEVEL_WORDS_13, items, 3U);
  harness_open_mode(1U);
  run_to_item();
  key("K");
  key_unknown();
  TEST_ASSERT(status("Wrong: *"));
  checkpoint(CP_ERROR, "KMRS", 1);
  run_to_item();
  key("KM");
  key("E");
  checkpoint(CP_ERROR, "KMRS", 2);
  run_to_item();
  key("K");
  word_gap();
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  TEST_ASSERT(status("Wrong: pause too long"));
  checkpoint(CP_ERROR, "KMRS", 1);
  run_to_item();
  checkpoint(CP_ITEM, "UAPT", 0);
  key("U");
  elements(".-"); /* half of A */
  skip_now();
  TEST_ASSERT(training_is_skipped());
  TEST_ASSERT(status("Skipped"));
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  checkpoint(CP_ERROR, "UAPT", 1);
  elements("-");
  push(KEYER_EVENT_CHAR_GAP); /* the rest of the half-keyed character, dropped */
  run_to_item();
  TEST_ASSERT_EQUAL(3U, training_item_number());
  checkpoint(CP_ITEM, "LOWI", 0);
  key("L");
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(1U, training_answer_length());
  checkpoint(CP_ITEM, "LOWI", 1);
  key("OWI");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  checkpoint(CP_NO_BAR, "LOWI", 0);
}

/* Scenario C: a skip in ITEM with nothing keyed, and a skip in a Wrong result (FAILED by skip). */
static void scenario_c(bool is_marker_shown) {
  const char* const items[3] = {"KMRS", "UAPT", "LOWI"};
  prepare(is_marker_shown, false, LEVEL_WORDS_13, items, 3U);
  harness_open_mode(1U);
  run_to_item();
  checkpoint(CP_ITEM, "KMRS", 0);
  skip_now();
  TEST_ASSERT(training_is_skipped());
  checkpoint(CP_ERROR, "KMRS", 0);
  run_to_item();
  checkpoint(CP_ITEM, "UAPT", 0);
  key("UE");
  TEST_ASSERT(status("Wrong: E"));
  checkpoint(CP_ERROR, "UAPT", 1);
  skip_now();
  TEST_ASSERT(training_is_skipped());
  TEST_ASSERT(status("Skipped"));
  checkpoint(CP_ERROR, "UAPT", 1);
  run_to_item();
  checkpoint(CP_ITEM, "LOWI", 0);
}

/* Scenario D, character items: no bar in ITEM, WRONG, RETRY, FAILED, skip and CORRECT; then the END screen. */
static void scenario_d(bool is_marker_shown) {
  const char* const items[3] = {"K", "M", "K"};
  unsigned guard = 0U;
  prepare(is_marker_shown, false, LEVEL_KM, items, 3U);
  harness_open_mode(1U);
  run_to_item();
  checkpoint(CP_NO_BAR, "K", 0);
  key("E");
  checkpoint(CP_NO_BAR, "K", 0);
  run_to_item();
  checkpoint(CP_NO_BAR, "K", 0);
  key("E");
  run_to_item();
  key("E");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
  checkpoint(CP_NO_BAR, "K", 0);
  run_to_item();
  checkpoint(CP_NO_BAR, "M", 0);
  skip_now();
  TEST_ASSERT(training_is_skipped());
  checkpoint(CP_NO_BAR, "M", 0);
  run_to_item();
  key("K");
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
  checkpoint(CP_NO_BAR, "K", 0);
  while ((training_phase() != TRAINING_PHASE_END) && (guard < KOCH_CHAR_ITEMS)) {
    run_to_item();
    key("K");
    run_to_phase((training_item_number() < training_item_count()) ? TRAINING_PHASE_GAP : TRAINING_PHASE_END);
    ++guard;
  }
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  checkpoint(CP_NOT_ITEM, "", 0);
  /* The end screen opens the start screen; the setting does not act on it either. */
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(ENTER_EVENT);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  checkpoint(CP_NOT_ITEM, "", 0);
}

typedef void (*scenario_t)(bool is_marker_shown);

static void run_both(scenario_t scenario, const char* name) {
  for (run_index = 0U; run_index < 2U; ++run_index) {
    checkpoint_count[run_index] = 0U;
    scenario(run_index == 0U);
  }
  compare_runs(name);
}

/* AC-T55, D-T63, D-T64 (6): every row of the 17.9 behavior table with On and Off, and the On/Off frame diff. */
static void test_behavior_table(void) {
  run_both(scenario_a, "A");
  run_both(scenario_b, "B");
  run_both(scenario_c, "C");
  run_both(scenario_d, "D");
}

/* F10: Off, a skip while a character is half keyed; the next ITEM has no bar, and its first character counts. */
static void test_f10_skip_half_keyed_off(void) {
  const char* const items[2] = {"KMRS", "MRSK"};
  unsigned variant;
  for (variant = 0U; variant < 2U; ++variant) {
    tx_open(false, variant == 1U, LEVEL_WORDS_13, items, 2U);
    run_to_item();
    TEST_ASSERT(item_line_with_bar("KMRS", NO_BAR));
    if (variant == 1U) {
      key("K");
    }
    elements("-.");
    skip_now();
    TEST_ASSERT(training_is_skipped());
    TEST_ASSERT_EQUAL(TRAINING_CHECK_FAILED, training_last_check());
    TEST_ASSERT(item_line_with_bar("KMRS", (int)training_answer_length()));
    TEST_ASSERT_EQUAL(variant, training_answer_length());
    elements(".");
    push(KEYER_EVENT_CHAR_GAP);
    run_to_item();
    TEST_ASSERT_EQUAL(2U, training_item_number());
    TEST_ASSERT(item_line_with_bar("MRSK", NO_BAR));
    key("M");
    TEST_ASSERT_EQUAL(1U, training_answer_length());
    TEST_ASSERT_EQUAL(0U, training_wrong_attempts());
    TEST_ASSERT(item_line_with_bar("MRSK", NO_BAR));
    key("RSK");
    TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, training_last_check());
    TEST_ASSERT(item_line_with_bar("MRSK", NO_BAR));
  }
}

/* F11: Off, a word-space error after the second-to-last character; the bar at the RESULT, gone at the RETRY. */
static void test_f11_word_space_off(void) {
  const char* const items[1] = {"KMRS"};
  uint8_t before[FB_SIZE];
  uint8_t after[FB_SIZE];
  tx_open(false, false, LEVEL_WORDS_13, items, 1U);
  run_to_item();
  snapshot(before);
  key("KMR");
  snapshot(after);
  /* No pixel of the item rows changes while the user keys. */
  TEST_ASSERT_EQUAL(0, memcmp(&before[ITEM_PAGE_TOP * LCD_WIDTH], &after[ITEM_PAGE_TOP * LCD_WIDTH], 2U * LCD_WIDTH));
  word_gap();
  TEST_ASSERT(status("Wrong: pause too long"));
  TEST_ASSERT(item_line_with_bar("KMRS", 3));
  snapshot(after);
  TEST_ASSERT_EQUAL(0, memcmp(&before[ITEM_PAGE_TOP * LCD_WIDTH], &after[ITEM_PAGE_TOP * LCD_WIDTH], LCD_WIDTH));
  TEST_ASSERT(memcmp(&before[ITEM_PAGE_BAR * LCD_WIDTH], &after[ITEM_PAGE_BAR * LCD_WIDTH], LCD_WIDTH) != 0);
  run_to_item();
  TEST_ASSERT(status("Try 2 of 3"));
  TEST_ASSERT(item_line_with_bar("KMRS", NO_BAR));
  snapshot(after);
  TEST_ASSERT_EQUAL(0, memcmp(&before[ITEM_PAGE_TOP * LCD_WIDTH], &after[ITEM_PAGE_TOP * LCD_WIDTH], 2U * LCD_WIDTH));
}

/* F12: Off, TX Attempts unlimited, five wrong answers in a row at different slots; each RESULT bar, each RETRY none. */
static void test_f12_unlimited_off(void) {
  const char* const items[1] = {"KMRS"};
  static const char* const prefixes[5] = {"", "K", "KM", "KMR", "K"};
  unsigned attempt;
  tx_open(false, true, LEVEL_WORDS_13, items, 1U);
  for (attempt = 0U; attempt < 5U; ++attempt) {
    run_to_item();
    TEST_ASSERT(item_line_with_bar("KMRS", NO_BAR));
    key(prefixes[attempt]);
    TEST_ASSERT(item_line_with_bar("KMRS", NO_BAR));
    if (attempt == 4U) {
      word_gap();
    } else {
      key("E");
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, training_last_check());
    if (!TEST_ASSERT(item_line_with_bar("KMRS", (int)strlen(prefixes[attempt])))) {
      fprintf(stderr, "attempt %u\n", attempt + 1U);
    }
  }
  run_to_item();
  TEST_ASSERT(status("Try 6"));
  TEST_ASSERT(item_line_with_bar("KMRS", NO_BAR));
}

/* Leaves the mode with a long menu/back and opens Sending again from the main menu; the session starts. */
static void reopen_sending(void) {
  harness_pins_for(BUTTON_BIT_MENU, LONG_HOLD);
  harness_pins_for(0U, (uint16_t)(BUTTONS_DEBOUNCE_MS + 10U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(ENTER_EVENT);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  harness_set_time((uint16_t)(harness_now + 1U));
  harness_event(ENTER_EVENT);
}

/*
 * F13, AC-T56, M21: settings_set() of id 15 in a session, not through the menu. The screen keeps the value of the
 * entry, also in a new session from the end screen; the next sending_enter() reads the new value.
 */
static void test_f13_set_in_session(void) {
  const char* const items[1] = {"KMRS"};
  unsigned direction;
  for (direction = 0U; direction < 2U; ++direction) {
    const bool is_entry_shown = (direction == 0U);
    const int entry_bar = is_entry_shown ? 1 : NO_BAR;
    const int new_bar = is_entry_shown ? NO_BAR : 1;
    unsigned guard = 0U;
    tx_open(is_entry_shown, false, LEVEL_WORDS_13, items, 1U);
    run_to_item();
    TEST_ASSERT(settings_set(SETTINGS_ID_TX_MARKER_HIDDEN, is_entry_shown ? 1U : 0U));
    key("K");
    TEST_ASSERT(item_line_with_bar("KMRS", entry_bar));
    key("N");
    run_to_item();
    TEST_ASSERT(item_line_with_bar("KMRS", is_entry_shown ? 0 : NO_BAR));
    key("K");
    TEST_ASSERT(item_line_with_bar("KMRS", entry_bar));
    /* A redraw of the whole screen keeps the value of the entry. */
    menu_render();
    TEST_ASSERT(item_line_with_bar("KMRS", entry_bar));
    /* To the end screen and a new session from it. */
    key("MRS");
    while ((training_phase() != TRAINING_PHASE_END) && (guard < 20U)) {
      run_to_item();
      key("KMRS");
      run_to_phase((training_item_number() < training_item_count()) ? TRAINING_PHASE_GAP : TRAINING_PHASE_END);
      ++guard;
    }
    TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_event(ENTER_EVENT);
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_event(ENTER_EVENT);
    run_to_item();
    key("K");
    TEST_ASSERT(item_line_with_bar("KMRS", entry_bar));
    /* The next sending_enter() reads the new value. */
    reopen_sending();
    run_to_item();
    key("K");
    if (!TEST_ASSERT(item_line_with_bar("KMRS", new_bar))) {
      fprintf(stderr, "direction %u: the new value does not act after a new entry\n", direction);
    }
    /* The session saves progress; the settings record (page 0) is never written. */
    TEST_ASSERT_EQUAL(0U, fake_iap_erase_count(0U));
    TEST_ASSERT(settings_has_unsaved_changes());
  }
}

/*
 * F14, AC-M24: a version 1, 2 or 3 record and a version 4 record with byte 14 = 0x03 show the bar in ITEM (On), and
 * leave no unsaved change; a version 4 record with byte 14 = 0xFC shows no bar in ITEM. No load or visit writes.
 */
static void test_f14_old_records(void) {
  static const struct {
    uint8_t version;
    uint8_t training_byte;
    bool is_shown;
  } cases[5] = {{1U, 0U, true}, {2U, 0U, true}, {3U, 0U, true}, {4U, 0x03U, true}, {4U, 0xFCU, false}};
  const char* const items[1] = {"KMRS"};
  size_t index;
  for (index = 0U; index < 5U; ++index) {
    fake_iap_reset();
    write_settings_record(cases[index].version, cases[index].training_byte);
    progress_fixture_write(PROGRESS_MODE_SENDING, 20U, LEVEL_WORDS_13);
    harness_power_on();
    TEST_ASSERT_EQUAL(cases[index].is_shown ? 0U : 1U, settings_get(SETTINGS_ID_TX_MARKER_HIDDEN));
    TEST_ASSERT(!settings_has_unsaved_changes());
    fake_koch_item_reset();
    fake_koch_item_push(items[0]);
    harness_open_mode(1U);
    run_to_item();
    if (!TEST_ASSERT(item_line_with_bar("KMRS", cases[index].is_shown ? 0 : NO_BAR))) {
      fprintf(stderr, "case %zu\n", index);
    }
    key("KM");
    TEST_ASSERT(item_line_with_bar("KMRS", cases[index].is_shown ? 2 : NO_BAR));
    TEST_ASSERT(!settings_has_unsaved_changes());
    TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  }
}

int main(void) {
  TEST_RUN(test_behavior_table);
  TEST_RUN(test_f10_skip_half_keyed_off);
  TEST_RUN(test_f11_word_space_off);
  TEST_RUN(test_f12_unlimited_off);
  TEST_RUN(test_f13_set_in_session);
  TEST_RUN(test_f14_old_records);
  return TEST_FINISH();
}
