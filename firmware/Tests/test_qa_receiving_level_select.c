/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the level selection in Receiving, Sending and the main menu (references/internal/training_spec.md
 * D-T35 to D-T50, sections 12.6 and 17.6, AC-T32 to AC-T35, AC-T41 to AC-T45, tasks Q5 and Q7; ui-ux.md sections
 * 5.11.1, 5.15, 5.16, 6.1, 6.8, 6.9, 9.4, UX-65 to UX-70).
 *
 * Golden screens: each expected screen is written here as the 25-column text of the ui-ux.md mockups, plus the large
 * text, the code pattern and the band label. The QA renderer turns that into a 128 x 64 image and compares every
 * pixel of the framebuffer. The 5 x 8 text comes from the golden font (fixtures/font_golden.h). The code marks and
 * the band come from the formulas of ui-ux.md 5.16.3 and SL-5 (qa/qa_training_screens.h). The large text uses the
 * reference rendering of BIG (ui-ux.md 9.4); its ink box is checked against the ui-ux.md tables. The renderer does
 * not use the firmware layout code. The sessions run through the real menu, buttons, modes, engine and progress; the
 * answers go to the engine directly, so the end screen and the start screen are the screens under test.
 */

#include "koch/koch.h"
#include "modes/training.h"
#include "qa/qa_training_screens.h"
#include "support/training_harness.h"

#define FB_SIZE ((size_t)LCD_PAGE_COUNT * LCD_WIDTH)
#define NO_ROW (-1)
#define STEP_MS 5U
#define HOLD_MS 2000U
/* UX-70: a Receiving step with a change of the label (L1 and L2, a change of kind): pages 0-4. */
#define RX_KIND_PAGES ((uint8_t)(PAGE(0) | PAGE(1) | PAGE(2) | PAGE(3) | PAGE(4)))
/* UX-70: the end screen to the start screen with no code row: pages 0-4 and 7. */
#define RX_REOPEN_PAGES ((uint8_t)(RX_KIND_PAGES | PAGE(7)))

/* ---------- QA golden renderer ---------- */

typedef struct {
  const char* rows[8]; /* 5 x 8 text of each row; NULL: blank row */
  const char* big;     /* large text in rows 3 and 4 at x = 64 - 4 m (ui-ux.md 5.16.3), or NULL */
  const char* code;    /* CODE(p) of row 5, or NULL */
  const char* band;    /* label of the row 7 band (SL-5): "Start", "OK", or NULL */
  int selected_row;    /* main menu band x 0-127, y 8r+1..8r+7, or NO_ROW */
} golden_t;

typedef struct {
  bool big[16][LCD_WIDTH];
  bool code[LCD_WIDTH];
  char band_row[26];
  int band_x0;
  int band_x1;
} golden_parts_t;

#define START_SCREEN(header, selector, label, big, code) \
  {{(header), (selector), (label), NULL, NULL, NULL, NULL, NULL}, (big), (code), "Start", NO_ROW}
#define END_SCREEN(header, status_row, score, result) \
  {{(header), (status_row), (score), (result), NULL, NULL, NULL, NULL}, NULL, NULL, "OK", NO_ROW}
#define MENU_SCREEN(r1, r2, selected)                                                          \
  {{"CW Trainer", (r1), (r2), "Free Input", "Settings                >", "About", NULL, NULL}, \
   NULL,                                                                                       \
   NULL,                                                                                       \
   NULL,                                                                                       \
   (selected)}

static void golden_prepare(const golden_t* golden, golden_parts_t* parts) {
  memset(parts, 0, sizeof(*parts));
  if (golden->big != NULL) {
    qa_big_reference(golden->big, qa_big_x(golden->big), parts->big);
  }
  if (golden->code != NULL) {
    (void)qa_code_columns(golden->code, parts->code);
  }
  parts->band_x0 = -1;
  if (golden->band != NULL) {
    const int length = (int)strlen(golden->band);
    const int column = (25 - length) / 2;
    (void)snprintf(parts->band_row, sizeof(parts->band_row), "%*s%s", column, "", golden->band);
    parts->band_x0 = 2 + 5 * (column - 1);
    parts->band_x1 = 2 + 5 * (column + length + 1) - 1;
  }
}

static bool golden_text_ink(const char* text, int x, int y) {
  const int column = (x - 2) / 5;
  if ((text == NULL) || (x < 2) || (column >= 25) || (column >= (int)strlen(text))) {
    return false;
  }
  return golden_ink(text[column], (x - 2) % 5, y % 8);
}

static bool golden_pixel(const golden_t* golden, const golden_parts_t* parts, int x, int y) {
  const int row = y / 8;
  const int cell_y = y % 8;
  const bool is_band_row = (row == 7) && (golden->band != NULL);
  const char* text = is_band_row ? parts->band_row : golden->rows[row];
  const bool is_ink = golden_text_ink(text, x, y);

  if ((golden->big != NULL) && ((row == 3) || (row == 4))) {
    return parts->big[y - 24][x];
  }
  if ((golden->code != NULL) && (row == 5)) {
    return ((cell_y == 3) || (cell_y == 4)) && parts->code[x];
  }
  if (row == 0) {
    return !is_ink; /* title bar or header: inverted over x 0-127, y 0-7 */
  }
  if (row == golden->selected_row) {
    return (cell_y != 0) && !is_ink;
  }
  if (is_band_row && (x >= parts->band_x0) && (x <= parts->band_x1)) {
    return (cell_y != 0) && !is_ink;
  }
  return is_ink;
}

static void print_rows(const golden_t* golden, const golden_parts_t* parts, int row) {
  int y;
  int x;
  for (y = 8 * row; y < 8 * row + 8; ++y) {
    fprintf(stderr, "  y%2d want ", y);
    for (x = 0; x < 128; ++x) {
      fputc(golden_pixel(golden, parts, x, y) ? '#' : '.', stderr);
    }
    fprintf(stderr, "\n  y%2d got  ", y);
    for (x = 0; x < 128; ++x) {
      fputc(pixel(x, y) ? '#' : '.', stderr);
    }
    fputc('\n', stderr);
  }
}

static bool screen_is(const golden_t* golden, const char* name) {
  static golden_parts_t parts;
  int row;
  bool is_ok = true;

  golden_prepare(golden, &parts);
  for (row = 0; row < 8; ++row) {
    int y;
    bool is_row_ok = true;
    for (y = 8 * row; (y < 8 * row + 8) && is_row_ok; ++y) {
      int x;
      for (x = 0; x < 128; ++x) {
        if (pixel(x, y) != golden_pixel(golden, &parts, x, y)) {
          is_row_ok = false;
          break;
        }
      }
    }
    if (!is_row_ok) {
      fprintf(stderr, "golden %s: row %d differs (want \"%s\")\n", name, row,
              (golden->rows[row] != NULL) ? golden->rows[row] : "");
      print_rows(golden, &parts, row);
      is_ok = false;
    }
  }
  return is_ok;
}

#define SCREEN_IS(golden) TEST_ASSERT(screen_is(&(golden), #golden))

/* ---------- Framebuffer helpers ---------- */

static void snapshot(uint8_t* buffer) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&buffer[(size_t)page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
  }
}

static bool are_pages_equal(const uint8_t* first, const uint8_t* second, uint8_t mask) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    if (((mask & (1U << page)) != 0U) &&
        (memcmp(&first[(size_t)page * LCD_WIDTH], &second[(size_t)page * LCD_WIDTH], LCD_WIDTH) != 0)) {
      fprintf(stderr, "page %u differs\n", (unsigned)page);
      return false;
    }
  }
  return true;
}

/* AC-T42 (2): the framebuffer equals a full menu_render() of the same state. The dirty flags stay. */
static bool is_full_render_equal(void) {
  uint8_t before[FB_SIZE];
  uint8_t after[FB_SIZE];
  const uint8_t dirty = lcd_fb_dirty_mask();
  uint8_t page;

  snapshot(before);
  menu_render();
  snapshot(after);
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    if ((dirty & (1U << page)) == 0U) {
      lcd_fb_clear_dirty(page);
    }
  }
  return are_pages_equal(before, after, QA_ALL_PAGES);
}

/* The pages whose bytes differ between the frames @p before and @p after. */
static uint8_t changed_pages(const uint8_t* before, const uint8_t* after) {
  uint8_t page;
  uint8_t mask = 0U;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    if (memcmp(&before[(size_t)page * LCD_WIDTH], &after[(size_t)page * LCD_WIDTH], LCD_WIDTH) != 0) {
      mask = (uint8_t)(mask | (1U << page));
    }
  }
  return mask;
}

/*
 * DIRTYCHG of ui-ux.md 9.4 (UX-70, D-T49): sends @p event; exactly the pages whose bytes changed are dirty. Returns
 * the changed pages.
 */
static uint8_t event_dirtychg(button_event_t event) {
  uint8_t before[FB_SIZE];
  uint8_t after[FB_SIZE];
  uint8_t changed;

  harness_clear_dirty();
  snapshot(before);
  harness_event(event);
  snapshot(after);
  changed = changed_pages(before, after);
  if (!TEST_ASSERT(dirty_is(changed))) {
    fprintf(stderr, "DIRTYCHG: changed pages 0x%02X\n", (unsigned)changed);
  }
  return changed;
}

/*
 * Sends @p event with the exact page set @p mask of a 9.4 vector: DIRTY(mask) and DIRTYCHG. After a change the frame
 * equals a full render (AC-T42).
 */
static void event_with_budget(button_event_t event, uint8_t mask) {
  const uint8_t changed = event_dirtychg(event);
  if (!TEST_ASSERT_EQUAL(mask, changed)) {
    fprintf(stderr, "expected pages 0x%02X\n", (unsigned)mask);
  }
  if (mask != 0U) {
    TEST_ASSERT(is_full_render_equal());
  }
}

/*
 * The step between indexes 65534 and 65535 (D-T50 (1)): only the bytes of page 1 change, and DIRTY(0,1) is the
 * accepted deviation from DIRTYCHG. The header bytes must still be equal.
 */
static void saturation_step(button_event_t event) {
  uint8_t before[FB_SIZE];
  uint8_t after[FB_SIZE];

  harness_clear_dirty();
  snapshot(before);
  harness_event(event);
  snapshot(after);
  TEST_ASSERT_EQUAL(PAGE(1), changed_pages(before, after));
  TEST_ASSERT(dirty_is((uint8_t)(PAGE(0) | PAGE(1))));
  TEST_ASSERT(is_full_render_equal());
}

/*
 * A step with no vector mask (UX-70, SL-4): DIRTYCHG inside pages 0-5, never page 5 in Receiving, page 1 (the
 * selector) always. At a limit: DIRTY(). After a change the frame equals a full render.
 */
static void step_by_rule(button_event_t event, bool is_tx, bool can_step) {
  const uint8_t changed = event_dirtychg(event);
  const uint8_t bound = is_tx ? QA_PAGES_0_TO_5 : (uint8_t)(QA_PAGES_0_TO_5 & ~PAGE(5));
  if (!can_step) {
    TEST_ASSERT_EQUAL(0U, changed);
    return;
  }
  if (!TEST_ASSERT_EQUAL(0U, changed & (uint8_t)~bound)) {
    fprintf(stderr, "step pages 0x%02X outside 0x%02X\n", (unsigned)changed, (unsigned)bound);
  }
  TEST_ASSERT((changed & PAGE(1)) != 0U);
  TEST_ASSERT(is_full_render_equal());
}

/* ---------- Scenario helpers ---------- */

static uint32_t walk_state_steps;

static void power_on(void) {
  harness_power_on();
  fake_koch_item_reset();
  fake_koch_item_push("K");
}

static void set_rx_speeds(uint16_t wpm, uint16_t eff) {
  (void)settings_set(SETTINGS_ID_RX_WPM, wpm);
  (void)settings_set(SETTINGS_ID_RX_EFF_WPM, eff);
}

static void set_tx_speeds(uint16_t wpm, uint16_t eff) {
  (void)settings_set(SETTINGS_ID_TX_WPM, wpm);
  (void)settings_set(SETTINGS_ID_TX_EFF_WPM, eff);
}

/* Fresh EEPROM with one earned level, then power-on. */
static void earn(uint8_t mode, uint8_t wpm, uint16_t level) {
  fake_iap_reset();
  progress_fixture_write(mode, wpm, level);
  power_on();
}

static void step_ms(uint16_t ms) {
  harness_set_time((uint16_t)(harness_now + ms));
  harness_pass();
}

static void until_phase(training_phase_t phase) {
  unsigned guard = 0U;
  while ((training_phase() != phase) && (guard < 60000U)) {
    step_ms(STEP_MS);
    ++guard;
  }
  TEST_ASSERT_EQUAL(phase, training_phase());
}

/* Answers the current item through the engine: right, or one wrong attempt first. */
static void answer_item(bool is_first_wrong) {
  uint8_t index;
  training_check_t check = TRAINING_CHECK_PARTIAL;

  if (is_first_wrong) {
    check = training_add_char((training_item_text()[0] == 'K') ? 'M' : 'K');
    for (index = 1U; (check == TRAINING_CHECK_PARTIAL) && (index < training_item_length()); ++index) {
      check = training_add_char(training_item_text()[index]);
    }
    TEST_ASSERT_EQUAL(TRAINING_CHECK_WRONG, check);
    until_phase(TRAINING_PHASE_ITEM);
  }
  for (index = 0U; index < training_item_length(); ++index) {
    check = training_add_char(training_item_text()[index]);
  }
  TEST_ASSERT_EQUAL(TRAINING_CHECK_CORRECT, check);
}

/* Runs the session of the open mode up to the last answer, with @p first_correct first-attempt answers. */
static void run_to_last_answer(unsigned first_correct) {
  unsigned number;
  const unsigned count = training_item_count();
  for (number = 1U; number <= count; ++number) {
    until_phase(TRAINING_PHASE_ITEM);
    answer_item(number > first_correct);
  }
}

static void run_session(unsigned first_correct) {
  run_to_last_answer(first_correct);
  until_phase(TRAINING_PHASE_END);
}

/* Steps the start screen selection with prev taps down to index @p level. */
static void select_down_to(uint16_t level) {
  while (training_selected_level() > level) {
    harness_event(M_TAP);
  }
  TEST_ASSERT_EQUAL(level, training_selected_level());
}

/*
 * The pages of the switch from the end screen to the start screen (UX-70, D-T49): 0, 1, 2, 3, 4 and 7, and page 5
 * in Sending at L1 or a character level (the code). Page 6 stays clean.
 */
static uint8_t reopen_pages(void) {
  const bool has_code = (training_mode() == TRAINING_MODE_SENDING) &&
                        ((training_selected_level() == 0U) || (koch_kind(training_selected_level()) == KOCH_KIND_CHAR));
  return (uint8_t)(PAGE(0) | PAGE(1) | PAGE(2) | PAGE(3) | PAGE(4) | PAGE(7) | (has_code ? PAGE(5) : 0U));
}

/* The enter press on the end screen (UX-69): the start screen with DIRTYCHG and the UX-70 pages, no session. */
static void reopen_with_enter(void) {
  const size_t player_calls = fake_player_call_count();
  const size_t keyer_starts = fake_keyer_start_count();
  uint8_t changed;

  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  changed = event_dirtychg(ENTER_EVENT);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  if (!TEST_ASSERT_EQUAL(reopen_pages(), changed)) {
    fprintf(stderr, "reopen at level %u: pages 0x%02X\n", (unsigned)training_selected_level(), (unsigned)changed);
  }
  TEST_ASSERT(is_full_render_equal());
  TEST_ASSERT_EQUAL(player_calls, fake_player_call_count());
  TEST_ASSERT_EQUAL(keyer_starts, fake_keyer_start_count());
}

static bool is_tap_mode_off_on_main_menu(void) {
  /* Main menu, prev: the cursor moves at the debounced press when tap mode is off (ui-ux.md 6.1). */
  const uint8_t cursor = menu_cursor();
  bool is_off;
  harness_pins_for(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + 5U));
  is_off = (menu_cursor() != cursor);
  harness_pins_for(0U, 40U);
  /* Put the cursor back. */
  if (is_off) {
    harness_event(BUTTON_EVENT_NEXT);
  }
  return is_off;
}

/* ---------- QA oracle self-checks against the ui-ux.md tables (pass before the firmware change) ---------- */

static bool box_of(bool pixels[16][LCD_WIDTH], qa_box_t* box) {
  int x;
  int y;
  bool has_ink = false;
  for (y = 0; y < 16; ++y) {
    for (x = 0; x < QA_WIDTH; ++x) {
      if (pixels[y][x]) {
        if (!has_ink) {
          *box = (qa_box_t){x, x, y + 24, y + 24};
          has_ink = true;
        }
        box->x0 = (x < box->x0) ? x : box->x0;
        box->x1 = (x > box->x1) ? x : box->x1;
        box->y0 = ((y + 24) < box->y0) ? (y + 24) : box->y0;
        box->y1 = ((y + 24) > box->y1) ? (y + 24) : box->y1;
      }
    }
  }
  return has_ink;
}

/* The reference rendering of the large text at x = 64 - 4 m has the ink boxes of ui-ux.md 5.16.1 and 5.16.3. */
static void test_oracle_big_text_ink(void) {
  static const struct {
    const char* text;
    qa_box_t ink;
  } cases[] = {
      {"J", {60, 66, 26, 35}},    {"K M", {52, 74, 26, 35}}, {"<AR/+>", {41, 86, 25, 36}},
      {"<AR>", {49, 78, 26, 35}}, {"+", {61, 66, 29, 33}},   {"Words of 4", {24, 102, 26, 35}},
  };
  static bool pixels[16][LCD_WIDTH];
  size_t index;

  power_on();
  for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
    qa_box_t box;
    qa_big_reference(cases[index].text, qa_big_x(cases[index].text), pixels);
    TEST_ASSERT(box_of(pixels, &box));
    if (!TEST_ASSERT((box.x0 == cases[index].ink.x0) && (box.x1 == cases[index].ink.x1) &&
                     (box.y0 == cases[index].ink.y0) && (box.y1 == cases[index].ink.y1))) {
      fprintf(stderr, "\"%s\": ink x %d-%d y %d-%d\n", cases[index].text, box.x0, box.x1, box.y0, box.y1);
    }
  }
  /* K and M of "K M" are the slots of a word of 2: K ink 52-58, M ink 68-74, the blank cell 60-67 has no ink. */
  qa_big_reference("K M", qa_big_x("K M"), pixels);
  {
    int x;
    int y;
    bool has_middle_ink = false;
    for (y = 0; y < 16; ++y) {
      for (x = 59; x <= 67; ++x) {
        has_middle_ink = has_middle_ink || pixels[y][x];
      }
    }
    TEST_ASSERT(!has_middle_ink);
  }
}

/* The CODE(p) oracle gives the mark ranges of ui-ux.md 5.16.1, 5.16.3 and V-142. */
static void test_oracle_code_ranges(void) {
  static const struct {
    const char* pattern;
    int width;
    int x0;
    int x1;
  } cases[] = {
      {"--..--", 43, 42, 84}, {"-----", 42, 43, 84},  {".-.-.-", 39, 44, 82},
      {".----", 38, 45, 82},  {"...-.-", 35, 46, 80}, {"-..-.", 30, 49, 78},
      {".---", 29, 49, 77},   {"-.- --", 44, 42, 85}, {"-.", 11, 58, 68},
  };
  size_t index;

  for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
    bool is_on[LCD_WIDTH];
    int first = -1;
    int last = -1;
    int x;
    TEST_ASSERT_EQUAL(cases[index].width, qa_code_columns(cases[index].pattern, is_on));
    for (x = 0; x < QA_WIDTH; ++x) {
      if (is_on[x]) {
        first = (first < 0) ? x : first;
        last = x;
      }
    }
    TEST_ASSERT_EQUAL(cases[index].x0, first);
    TEST_ASSERT_EQUAL(cases[index].x1, last);
  }
  /* V-137: the L1 group, dash 42-47, dot 51-52, dash 56-61, dash 71-76, dash 80-85. */
  {
    bool is_on[LCD_WIDTH];
    bool expected[LCD_WIDTH];
    static const int ranges[5][2] = {{42, 47}, {51, 52}, {56, 61}, {71, 76}, {80, 85}};
    size_t range;
    int x;
    memset(expected, 0, sizeof(expected));
    for (range = 0U; range < 5U; ++range) {
      for (x = ranges[range][0]; x <= ranges[range][1]; ++x) {
        expected[x] = true;
      }
    }
    (void)qa_code_columns("-.- --", is_on);
    TEST_ASSERT_EQUAL(0, memcmp(expected, is_on, sizeof(is_on)));
  }
}

/* ---------- Golden start screens (AC-T35, AC-T41, AC-T42, ui-ux.md 5.16.1 to 5.16.3) ---------- */

static void test_golden_start_screens(void) {
  static const golden_t rx_first_use =
      START_SCREEN("RX 32/32 L1          1/50", "         L1 of 1", "        New chars", "K M", NULL);
  static const golden_t tx_first_use =
      START_SCREEN("TX 20/20 L1          1/50", "         L1 of 1", "        New chars", "K M", "-.- --");
  static const golden_t tx_l20 =
      START_SCREEN("TX 20/20 L20         1/50", "     <  L20 of 20", "        New char", "J", ".---");
  static const golden_t rx_l20 =
      START_SCREEN("RX 32/32 L20         1/50", "     <  L20 of 20", "        New char", "J", NULL);
  static const golden_t rx_words =
      START_SCREEN("RX 32/32 L18         1/10", "     <  L18 of 18", NULL, "Words of 4", NULL);
  static const golden_t tx_words =
      START_SCREEN("TX 20/20 L18         1/10", "     <  L18 of 18", NULL, "Words of 4", NULL);
  static const golden_t tx_final_top =
      START_SCREEN("TX 20/20 L65         1/11", "     <  L65 of 65", NULL, "Words of 5", NULL);
  static const golden_t tx_final_below =
      START_SCREEN("TX 20/20 L64         1/10", "     <  L64 of 65  >", NULL, "Words of 5", NULL);
  static const golden_t rx_l2_of_2 =
      START_SCREEN("RX 32/32 L2          1/50", "      <  L2 of 2", "        New char", "R", NULL);
  static const golden_t rx_l1_of_2 =
      START_SCREEN("RX 32/32 L1          1/50", "         L1 of 2  >", "        New chars", "K M", NULL);
  static const golden_t rx_top =
      START_SCREEN("RX 50/50 L65535     1/100", "  <  L65535 of 65535", NULL, "Words of 5", NULL);
  static const golden_t rx_top_below =
      START_SCREEN("RX 50/50 L65535     1/100", "  <  L65535 of 65535  >", NULL, "Words of 5", NULL);

  /* V-137: L1 at first use. Every event at the limits gives DIRTY(). */
  fake_iap_reset();
  power_on();
  harness_open_start_screen(0U);
  SCREEN_IS(rx_first_use);
  TEST_ASSERT(qa_big_ink_is(52, 74, 26, 35));
  TEST_ASSERT_EQUAL(0U, fake_player_call_count());
  event_with_budget(M_TAP, 0U);
  event_with_budget(R_TAP, 0U);
  event_with_budget(REPEAT_EVENT(R_TAP), 0U);
  event_with_budget(LONG_EVENT(M_TAP), 0U);
  SCREEN_IS(rx_first_use);

  fake_iap_reset();
  power_on();
  harness_open_start_screen(1U);
  SCREEN_IS(tx_first_use);
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
  event_with_budget(M_TAP, 0U);
  event_with_budget(R_TAP, 0U);

  earn(PROGRESS_MODE_SENDING, 20U, 19U);
  harness_open_start_screen(1U);
  SCREEN_IS(tx_l20);
  TEST_ASSERT(qa_big_ink_is(60, 66, 26, 35));
  TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());

  earn(PROGRESS_MODE_RECEIVING, 32U, 19U);
  harness_open_start_screen(0U);
  SCREEN_IS(rx_l20);

  earn(PROGRESS_MODE_RECEIVING, 32U, 17U);
  harness_open_start_screen(0U);
  SCREEN_IS(rx_words);
  TEST_ASSERT(qa_big_ink_is(24, 102, 26, 35));

  earn(PROGRESS_MODE_SENDING, 20U, 17U);
  harness_open_start_screen(1U);
  SCREEN_IS(tx_words);

  earn(PROGRESS_MODE_SENDING, 20U, 64U);
  harness_open_start_screen(1U);
  SCREEN_IS(tx_final_top);
  /* V-172 run 4: "Words of 5" stays; only the header and the selector change. */
  event_with_budget(M_TAP, (uint8_t)(PAGE(0) | PAGE(1)));
  SCREEN_IS(tx_final_below);

  earn(PROGRESS_MODE_RECEIVING, 32U, 1U);
  harness_open_start_screen(0U);
  SCREEN_IS(rx_l2_of_2);
  event_with_budget(R_TAP, 0U);
  /* UX-70 table, L1 and L2 in Receiving: pages 0-4. */
  event_with_budget(M_TAP, RX_KIND_PAGES);
  SCREEN_IS(rx_l1_of_2);
  event_with_budget(M_TAP, 0U);
  event_with_budget(R_TAP, RX_KIND_PAGES);
  SCREEN_IS(rx_l2_of_2);

  earn(PROGRESS_MODE_RECEIVING, 50U, 65535U);
  set_rx_speeds(50U, 50U);
  harness_open_start_screen(0U);
  SCREEN_IS(rx_top);
  /*
   * D-T5 saturation: indexes 65535 and 65534 both show L65535 with 100 items, so only the '>' of the selector
   * changes. The header draw still dirties page 0: the accepted deviation of D-T50 (1).
   */
  saturation_step(M_TAP);
  SCREEN_IS(rx_top_below);
  saturation_step(R_TAP);
  SCREEN_IS(rx_top);
}

/* The description in the three Prosigns styles (ui-ux.md 5.16.3, V-140, V-171), as whole screens in both modes. */
static void test_golden_prosign_descriptions(void) {
  static const uint8_t styles[3] = {SETTINGS_PROSIGN_BOTH, SETTINGS_PROSIGN_NAME, SETTINGS_PROSIGN_PUNCTUATION};
  static const char* const bt_texts[3] = {"<BT/=>", "<BT>", "="};
  static const char* const ar_texts[3] = {"<AR/+>", "<AR>", "+"};
  static const qa_box_t ar_ink[3] = {{41, 86, 25, 36}, {49, 78, 26, 35}, {61, 66, 29, 33}};
  size_t index;
  uint8_t mode;

  for (mode = PROGRESS_MODE_RECEIVING; mode <= PROGRESS_MODE_SENDING; ++mode) {
    const bool is_tx = (mode == PROGRESS_MODE_SENDING);
    const uint8_t wpm = is_tx ? 20U : 32U;
    const uint8_t row = is_tx ? 1U : 0U;
    for (index = 0U; index < 3U; ++index) {
      const golden_t bt =
          START_SCREEN(is_tx ? "TX 20/20 L59         1/50" : "RX 32/32 L59         1/50", "     <  L59 of 59",
                       "        New char", bt_texts[index], is_tx ? "-...-" : NULL);
      const golden_t ar =
          START_SCREEN(is_tx ? "TX 20/20 L62         1/50" : "RX 32/32 L62         1/50", "     <  L62 of 62",
                       "        New char", ar_texts[index], is_tx ? ".-.-." : NULL);
      earn(mode, wpm, 58U);
      (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, styles[index]);
      harness_open_start_screen(row);
      TEST_ASSERT(screen_is(&bt, bt_texts[index]));
      earn(mode, wpm, 61U);
      (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, styles[index]);
      harness_open_start_screen(row);
      TEST_ASSERT(screen_is(&ar, ar_texts[index]));
      TEST_ASSERT(qa_big_ink_is(ar_ink[index].x0, ar_ink[index].x1, ar_ink[index].y0, ar_ink[index].y1));
    }
  }
}

/*
 * V-171, UX-67: the Sending code row for each level kind, with Show CW Code On and Off (M8). The keyer never starts.
 * Receiving shows no code row at the same levels (M9).
 */
static void test_code_row_every_kind(void) {
  static const struct {
    uint16_t level;
    const char* code;
  } cases[] = {
      {0U, "-.- --"}, {1U, ".-."},     {19U, ".---"},  {24U, "-----"}, {27U, "--..--"},
      {58U, "-...-"}, {60U, "...-.-"}, {61U, ".-.-."}, {62U, ""},      {63U, ""},
  };
  size_t index;
  uint16_t show;

  for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
    for (show = 0U; show <= 1U; ++show) {
      earn(PROGRESS_MODE_SENDING, 20U, cases[index].level);
      (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, SETTINGS_PROSIGN_BOTH);
      (void)settings_set(SETTINGS_ID_SHOW_CW_CODE, show);
      harness_open_start_screen(1U);
      if (!TEST_ASSERT(qa_code(cases[index].code))) {
        fprintf(stderr, "V-171 level %u, Show CW Code %u\n", (unsigned)cases[index].level, (unsigned)show);
      }
      TEST_ASSERT(blank_row(6));
      TEST_ASSERT(margins_off());
      TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
    }
    earn(PROGRESS_MODE_RECEIVING, 32U, cases[index].level);
    harness_open_start_screen(0U);
    TEST_ASSERT(qa_code(""));
  }
  earn(PROGRESS_MODE_SENDING, 20U, 61U);
  (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, SETTINGS_PROSIGN_BOTH);
  harness_open_start_screen(1U);
  TEST_ASSERT(qa_desc("New char", "<AR/+>"));
}

/*
 * AC-T42, F9, M13, M14: steps from the earned level L20 down to L1 and up again, in both modes. Each step dirties
 * exactly the changed pages inside 0-5 (never page 5 in Receiving) and equals a full render (UX-70, D-T49). The walk
 * crosses the word levels (code group to blank in Sending) and L1 (K M and the L1 code group). A step at a limit makes
 * no page dirty.
 */
static void test_step_walk_equals_full_render(void) {
  uint8_t mode;

  for (mode = PROGRESS_MODE_RECEIVING; mode <= PROGRESS_MODE_SENDING; ++mode) {
    const uint8_t row = (mode == PROGRESS_MODE_SENDING) ? 1U : 0U;
    earn(mode, (mode == PROGRESS_MODE_SENDING) ? 20U : 32U, 19U);
    harness_open_start_screen(row);
    event_with_budget(REPEAT_EVENT(R_TAP), 0U);
    while (training_selected_level() > 0U) {
      step_by_rule(REPEAT_EVENT(M_TAP), mode == PROGRESS_MODE_SENDING, true);
      TEST_ASSERT(qa_lvl((unsigned)training_selected_level() + 1U, 20U));
    }
    TEST_ASSERT(qa_desc("New chars", "K M"));
    TEST_ASSERT(qa_code((mode == PROGRESS_MODE_SENDING) ? "-.- --" : ""));
    event_with_budget(REPEAT_EVENT(M_TAP), 0U);
    event_with_budget(M_TAP, 0U);
    while (training_selected_level() < 19U) {
      step_by_rule(R_TAP, mode == PROGRESS_MODE_SENDING, true);
      if (koch_kind(training_selected_level()) == KOCH_KIND_WORD) {
        TEST_ASSERT(qa_label(""));
        TEST_ASSERT(qa_code(""));
      }
    }
    event_with_budget(R_TAP, 0U);
    TEST_ASSERT(qa_desc("New char", "J"));
    TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
    TEST_ASSERT_EQUAL(0U, fake_player_call_count());
  }
}

/*
 * UX-70 step table of ui-ux.md 5.16.4, V-138, V-142 and V-172 runs 1 to 4: the exact dirty pages of each step, in
 * both modes, the same in both directions. Each step also satisfies DIRTYCHG.
 */
static void test_step_pages_per_vector(void) {
  static const struct {
    uint16_t earned;
    uint8_t max_word;
    button_event_t events[5];
    uint8_t rx_pages[5];
    uint8_t tx_pages[5];
    size_t count;
  } cases[] = {
      /* V-138, V-142: L20 J, L19 N (label stays), L18 Words of 4 (kind change), back up. */
      {19U,
       5U,
       {R_TAP, M_TAP, M_TAP, R_TAP, R_TAP},
       {0x00U, 0x1BU, 0x1FU, 0x1FU, 0x1BU},
       {0x00U, 0x3BU, 0x3FU, 0x3FU, 0x3BU},
       5U},
      /* L2 R and L1 K M. */
      {1U, 5U, {M_TAP, R_TAP}, {0x1FU, 0x1FU}, {0x3FU, 0x3FU}, 2U},
      /* V-172 runs 1 and 2: L23 F and L22 E; the top halves of E and F are equal, so page 3 stays clean. */
      {22U, 5U, {M_TAP, R_TAP}, {0x13U, 0x13U}, {0x33U, 0x33U}, 2U},
      /* V-172 runs 3 and 4: L65, L64, L63 with Words of 5 everywhere. */
      {64U, 5U, {M_TAP, M_TAP, R_TAP, R_TAP}, {0x03U, 0x03U, 0x03U, 0x03U}, {0x03U, 0x03U, 0x03U, 0x03U}, 4U},
  };
  size_t index;
  uint8_t mode;

  for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
    for (mode = PROGRESS_MODE_RECEIVING; mode <= PROGRESS_MODE_SENDING; ++mode) {
      const bool is_tx = (mode == PROGRESS_MODE_SENDING);
      size_t step;
      earn(mode, is_tx ? 20U : 32U, cases[index].earned);
      (void)settings_set(SETTINGS_ID_MAX_WORD_LEN, cases[index].max_word);
      harness_open_start_screen(is_tx ? 1U : 0U);
      for (step = 0U; step < cases[index].count; ++step) {
        const uint8_t pages = is_tx ? cases[index].tx_pages[step] : cases[index].rx_pages[step];
        const uint16_t before = training_selected_level();
        const unsigned long failures = test_failure_count;
        event_with_budget(cases[index].events[step], pages);
        if (test_failure_count != failures) {
          fprintf(stderr, "  case earned %u, %s, step %u from index %u\n", (unsigned)cases[index].earned,
                  is_tx ? "TX" : "RX", (unsigned)step, (unsigned)before);
        }
      }
    }
  }
}

/*
 * V-172 run 5: 200 random prev and next presses and repeats from EARN(m, 64), every Prosigns value, Max Word Length
 * 4 and 8, both modes. Every step gives DIRTYCHG inside pages 0-5, never page 5 in Receiving; a limit gives DIRTY().
 */
static void test_step_walk_dirtychg(void) {
  static const uint8_t styles[3] = {SETTINGS_PROSIGN_BOTH, SETTINGS_PROSIGN_NAME, SETTINGS_PROSIGN_PUNCTUATION};
  static const uint8_t max_words[2] = {4U, 8U};
  const unsigned long failures_before = test_failure_count;
  uint8_t mode;
  size_t style;
  size_t max_word;

  walk_state_steps = 7U;
  for (mode = PROGRESS_MODE_RECEIVING; mode <= PROGRESS_MODE_SENDING; ++mode) {
    for (style = 0U; style < 3U; ++style) {
      for (max_word = 0U; max_word < 2U; ++max_word) {
        const bool is_tx = (mode == PROGRESS_MODE_SENDING);
        unsigned step;
        earn(mode, is_tx ? 20U : 32U, 64U);
        (void)settings_set(SETTINGS_ID_PROSIGN_STYLE, styles[style]);
        (void)settings_set(SETTINGS_ID_MAX_WORD_LEN, max_words[max_word]);
        harness_open_start_screen(is_tx ? 1U : 0U);
        for (step = 0U; step < 200U; ++step) {
          const uint16_t selected = training_selected_level();
          bool is_up;
          walk_state_steps = (walk_state_steps * 1103515245U) + 12345U;
          /* Biased down, so the walk covers the whole range from L65 to L1. */
          is_up = ((walk_state_steps >> 16) % 8U) < 3U;
          step_by_rule(
              ((walk_state_steps >> 20) & 1U) != 0U ? REPEAT_EVENT(is_up ? R_TAP : M_TAP) : (is_up ? R_TAP : M_TAP),
              is_tx, is_up ? (selected < 64U) : (selected > 0U));
        }
        if (test_failure_count != failures_before) {
          fprintf(stderr, "step walk: first failure in mode %u, style %u, max %u\n", (unsigned)mode,
                  (unsigned)styles[style], (unsigned)max_words[max_word]);
          return;
        }
      }
    }
  }
}

/*
 * AC-T41 (6), SL-8, M7: the start screen does not use the item length of the last session. A word session of
 * 5 characters, then the start screen at a character level; a character session, then the start screen at L1.
 */
static void test_start_screen_ignores_last_item_length(void) {
  earn(PROGRESS_MODE_RECEIVING, 32U, 19U);
  fake_koch_item_reset();
  fake_koch_item_push("KMRSU");
  harness_open_start_screen(0U);
  select_down_to(17U);
  harness_event(ENTER_EVENT);
  until_phase(TRAINING_PHASE_ITEM);
  TEST_ASSERT_EQUAL(5U, training_item_length());
  run_session(10U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
  reopen_with_enter();
  TEST_ASSERT(qa_lvl(19U, 20U));
  TEST_ASSERT(qa_desc("New char", "N"));

  earn(PROGRESS_MODE_SENDING, 20U, 0U);
  harness_open_mode(1U);
  until_phase(TRAINING_PHASE_ITEM);
  TEST_ASSERT_EQUAL(1U, training_item_length());
  run_session(0U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
  reopen_with_enter();
  TEST_ASSERT(qa_start_screen_is("TX 20/20 L1          1/50", 1U, 1U, "New chars", "K M", "-.- --"));
}

/* ---------- Golden end screens (AC-T35, AC-T43, AC-T44, ui-ux.md 5.15) ---------- */

static void test_golden_end_pass_fail_not_saved(void) {
  static const golden_t pass =
      END_SCREEN("RX 32/32 L1         50/50", NULL, "       Score 45/50", "    Passed, new level");
  static const golden_t fail =
      END_SCREEN("RX 32/32 L1         50/50", NULL, "       Score 44/50", "   Not passed, need 45");
  static const golden_t not_saved =
      END_SCREEN("RX 32/32 L1         50/50", "        Not saved", "       Score 50/50", "    Passed, new level");
  static const golden_t start_l2 =
      START_SCREEN("RX 32/32 L2          1/50", "      <  L2 of 2", "        New char", "R", NULL);
  static const golden_t start_l1_of_2 =
      START_SCREEN("RX 32/32 L1          1/50", "         L1 of 2  >", "        New chars", "K M", NULL);
  static const golden_t start_l1 =
      START_SCREEN("RX 32/32 L1          1/50", "         L1 of 1", "        New chars", "K M", NULL);

  /* V-105: pass, then the first enter opens the start screen at L2. */
  fake_iap_reset();
  power_on();
  harness_open_mode(0U);
  run_session(45U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS, training_outcome());
  SCREEN_IS(pass);
  TEST_ASSERT(qa_end_screen_is(NULL, "Score 45/50", "Passed, new level"));
  /* A-L6, M1: no step on the end screen; the header keeps the session that ended. */
  event_with_budget(M_TAP, 0U);
  event_with_budget(R_TAP, 0U);
  TEST_ASSERT_EQUAL(1U, training_selected_level());
  SCREEN_IS(pass);
  reopen_with_enter();
  SCREEN_IS(start_l2);
  event_with_budget(M_TAP, RX_KIND_PAGES);
  SCREEN_IS(start_l1_of_2);
  event_with_budget(R_TAP, RX_KIND_PAGES);
  SCREEN_IS(start_l2);

  /* V-106: fail, the start screen at the same level. */
  fake_iap_reset();
  power_on();
  harness_open_mode(0U);
  run_session(44U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_FAIL, training_outcome());
  SCREEN_IS(fail);
  reopen_with_enter();
  SCREEN_IS(start_l1);

  /* V-107, F6: the save fails; the start screen shows the new level from RAM. */
  fake_iap_reset();
  power_on();
  harness_open_mode(0U);
  fake_iap_set_fail_operation(1U); /* the erase fails */
  run_session(50U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_NOT_SAVED, training_outcome());
  SCREEN_IS(not_saved);
  reopen_with_enter();
  SCREEN_IS(start_l2);
}

static void test_golden_end_replay(void) {
  static const golden_t rx_pass =
      END_SCREEN("RX 32/32 L5         50/50", NULL, "       Score 50/50", "         Passed");
  static const golden_t rx_fail =
      END_SCREEN("RX 32/32 L5         50/50", NULL, "       Score 44/50", "   Not passed, need 45");
  static const golden_t rx_start_l6 =
      START_SCREEN("RX 32/32 L6          1/10", "     <  L6 of 20  >", NULL, "Words of 2", NULL);
  static const golden_t rx_start_l5 =
      START_SCREEN("RX 32/32 L5          1/50", "     <  L5 of 20  >", "        New char", "U", NULL);
  static const golden_t tx_pass =
      END_SCREEN("TX 20/20 L1         50/50", NULL, "       Score 47/50", "         Passed");
  static const golden_t tx_start_l2 =
      START_SCREEN("TX 20/20 L2          1/50", "     <  L2 of 20  >", "        New char", "R", ".-.");

  /* V-145: replay pass at L5 with L20 earned, the start screen at L6. */
  earn(PROGRESS_MODE_RECEIVING, 32U, 19U);
  harness_open_start_screen(0U);
  select_down_to(4U);
  harness_event(ENTER_EVENT);
  run_session(50U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
  SCREEN_IS(rx_pass);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  event_with_budget(R_TAP, 0U);
  event_with_budget(R_TAP, 0U);
  event_with_budget(R_TAP, 0U);
  reopen_with_enter();
  SCREEN_IS(rx_start_l6);
  /* Back down to L5 on the start screen (a change of kind), start, fail (V-146): the start screen at L5. */
  event_with_budget(M_TAP, RX_KIND_PAGES);
  SCREEN_IS(rx_start_l5);
  harness_clear_dirty();
  harness_event(ENTER_EVENT);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(4U, training_level());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask() & PAGE(0));
  run_session(44U);
  SCREEN_IS(rx_fail);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  reopen_with_enter();
  SCREEN_IS(rx_start_l5);

  /* V-147 run 1: a Sending replay at L1 with L20 earned, the start screen at L2 with its code. */
  earn(PROGRESS_MODE_SENDING, 20U, 19U);
  harness_open_start_screen(1U);
  select_down_to(0U);
  harness_event(ENTER_EVENT);
  run_session(47U);
  SCREEN_IS(tx_pass);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  TEST_ASSERT_EQUAL(19U, progress_level(PROGRESS_MODE_SENDING, 20U));
  reopen_with_enter();
  SCREEN_IS(tx_start_l2);
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
}

/* V-148, F7: PASS_TOP at index 65535; the start screen shows L65535 of 65535 with no '>'. */
static void test_golden_end_top(void) {
  static const golden_t top = END_SCREEN("RX 50/50 L65535   100/100", NULL, "      Score 80/100", "         Passed");
  static const golden_t start_top =
      START_SCREEN("RX 50/50 L65535     1/100", "  <  L65535 of 65535", NULL, "Words of 5", NULL);
  earn(PROGRESS_MODE_RECEIVING, 50U, 65535U);
  set_rx_speeds(50U, 50U);
  harness_open_mode(0U);
  run_session(80U);
  TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_TOP, training_outcome());
  SCREEN_IS(top);
  TEST_ASSERT_EQUAL(0U, fake_iap_command_count(2U) + fake_iap_command_count(3U));
  reopen_with_enter();
  SCREEN_IS(start_top);
  TEST_ASSERT(qa_lvl_markers(65535U, 65535U, true, false));
}

/* ---------- End screen controls (AC-T44, AC-T45, ui-ux.md 5.15, section 12.6) ---------- */

/*
 * AC-T44 (1) to (4), V-105, V-108 run 1: on the end screen prev, next, their repeats and long events, and a long
 * enter do nothing. Held prev and next for 2000 ms do nothing. The enter press opens the start screen; the rest of
 * its hold gives nothing. A second press starts the session at the selected level with page 0 clean (B-2, B-4).
 */
static void test_rx_end_screen_controls(void) {
  static const button_event_t quiet[] = {M_TAP,
                                         R_TAP,
                                         REPEAT_EVENT(M_TAP),
                                         REPEAT_EVENT(R_TAP),
                                         LONG_EVENT(M_TAP),
                                         LONG_EVENT(R_TAP),
                                         LONG_EVENT(ENTER_EVENT)};
  static const uint8_t held[2] = {BUTTON_BIT_NEXT, BUTTON_BIT_PREV};
  size_t index;
  size_t player_calls;
  uint8_t before[FB_SIZE];
  uint8_t after[FB_SIZE];

  fake_iap_reset();
  power_on();
  harness_open_mode(0U);
  run_session(45U);
  player_calls = fake_player_call_count();
  for (index = 0U; index < sizeof(quiet) / sizeof(quiet[0]); ++index) {
    event_with_budget(quiet[index], 0U);
    TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
    TEST_ASSERT_EQUAL(1U, training_selected_level());
  }
  for (index = 0U; index < 2U; ++index) {
    harness_clear_dirty();
    snapshot(before);
    harness_pins_for(held[index], HOLD_MS);
    harness_pins_for(0U, 50U);
    snapshot(after);
    TEST_ASSERT(dirty_is(0U));
    TEST_ASSERT(are_pages_equal(before, after, QA_ALL_PAGES));
    TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  }
  TEST_ASSERT(hdr("RX 32/32 L1         50/50"));
  TEST_ASSERT(qa_end_screen_is(NULL, "Score 45/50", "Passed, new level"));
  TEST_ASSERT_EQUAL(player_calls, fake_player_call_count());

  /* TIP(t, 2000) of V-108 run 3: the press opens the start screen; the long time and the release give nothing. */
  harness_clear_dirty();
  harness_pins_for(BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  /* V-108 run 3: DIRTY(0,1,2,3,4,7). */
  TEST_ASSERT(dirty_is(RX_REOPEN_PAGES));
  TEST_ASSERT(qa_start_screen_is("RX 32/32 L2          1/50", 2U, 2U, "New char", "R", ""));
  harness_clear_dirty();
  harness_pins_for(BUTTON_BIT_ENTER, HOLD_MS);
  harness_pins_for(0U, 50U);
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT_EQUAL(player_calls, fake_player_call_count());
  /* Tap mode stays off on that start screen: a prev press steps at the press. */
  harness_pins_for(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  TEST_ASSERT_EQUAL(0U, training_selected_level());
  harness_pins_for(0U, 50U);
  harness_event(R_TAP);
  TEST_ASSERT_EQUAL(1U, training_selected_level());

  /* The second press starts at L2; page 0 stays clean; the held contact gives no tap and no replay (B-4). */
  harness_clear_dirty();
  harness_pins_for(BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(1U, training_level());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask() & PAGE(0));
  TEST_ASSERT(hdr("RX 32/32 L2          1/50"));
  harness_pins_for(BUTTON_BIT_ENTER, 1500U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  harness_pins_for(0U, 60U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(player_calls + 1U, fake_player_call_count());
}

/*
 * AC-T44 (3), (4), V-108 run 5, V-118, F1: in Sending, a short tip closure on the end screen shows the start screen
 * with the code and starts no keyer. The second closure starts the keyer once and disables enter.
 */
static void test_tx_end_screen_tip(void) {
  fake_iap_reset();
  power_on();
  harness_open_mode(1U);
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  run_session(50U);
  TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
  TEST_ASSERT(hdr("TX 20/20 L1         50/50"));
  TEST_ASSERT(qa_end_screen_is(NULL, "Score 50/50", "Passed, new level"));
  event_with_budget(M_TAP, 0U);
  harness_clear_dirty();
  harness_pins_for(BUTTON_BIT_ENTER, 100U);
  harness_pins_for(0U, 50U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  /* V-108 run 5: DIRTY(0,1,2,3,4,5,7). */
  TEST_ASSERT(dirty_is((uint8_t)(RX_REOPEN_PAGES | PAGE(5))));
  TEST_ASSERT(qa_start_screen_is("TX 20/20 L2          1/50", 2U, 2U, "New char", "R", ".-."));
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT(!fake_keyer_is_running());
  harness_pins_for(BUTTON_BIT_ENTER, 100U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(2U, fake_keyer_start_count());
  TEST_ASSERT(fake_keyer_is_running());
  harness_pins_for(0U, 50U);
  TEST_ASSERT_EQUAL(1U, training_level());
  TEST_ASSERT_EQUAL(0U, harness_overlap_count);
}

/*
 * F3, AC-T45 (1), B-1: prev held from the last RESULT across END and across the enter that opens the start screen.
 * No step and no dirty page come until the release; a new press then steps once.
 */
static void test_rx_prev_held_across_end_and_reopen(void) {
  static const golden_t end =
      END_SCREEN("RX 32/32 L6         10/10", NULL, "       Score 10/10", "    Passed, new level");
  earn(PROGRESS_MODE_RECEIVING, 32U, 5U);
  harness_open_mode(0U);
  run_to_last_answer(10U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  harness_pins_for(BUTTON_BIT_PREV, 1500U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(6U, training_selected_level());
  SCREEN_IS(end);
  harness_pins_for(BUTTON_BIT_PREV | BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT(qa_lvl(7U, 7U));
  harness_clear_dirty();
  harness_pins_for(BUTTON_BIT_PREV | BUTTON_BIT_ENTER, 1000U);
  harness_pins_for(BUTTON_BIT_PREV, 1000U);
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT_EQUAL(6U, training_selected_level());
  harness_pins_for(0U, 60U);
  harness_pins_for(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  TEST_ASSERT_EQUAL(5U, training_selected_level());
  harness_pins_for(0U, 60U);
  TEST_ASSERT_EQUAL(5U, training_selected_level());
  TEST_ASSERT(qa_lvl(6U, 7U));
}

/*
 * F2, AC-T45 (2), B-3: next pressed and held in END does nothing there. Then enter: on the start screen the
 * repeats of that hold step the selector, and each frame equals a full render.
 */
static void test_rx_next_held_in_end_then_enter(void) {
  earn(PROGRESS_MODE_RECEIVING, 32U, 19U);
  harness_open_start_screen(0U);
  select_down_to(4U);
  harness_event(ENTER_EVENT);
  run_session(50U);
  TEST_ASSERT_EQUAL(5U, training_selected_level());
  harness_clear_dirty();
  harness_pins_for(BUTTON_BIT_NEXT, 100U);
  TEST_ASSERT(dirty_is(0U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  TEST_ASSERT_EQUAL(5U, training_selected_level());
  harness_pins_for(BUTTON_BIT_NEXT | BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT_EQUAL(5U, training_selected_level());
  harness_pins_for(BUTTON_BIT_NEXT | BUTTON_BIT_ENTER, 600U);
  TEST_ASSERT(training_selected_level() > 5U);
  TEST_ASSERT(training_selected_level() <= 19U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  harness_pins_for(0U, 60U);
  TEST_ASSERT(qa_lvl((unsigned)training_selected_level() + 1U, 20U));
  TEST_ASSERT(is_full_render_equal());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
}

/*
 * F4, B-6, AC-T44 (6): a long menu/back on the end screen and on the reopened start screen leaves at the press. The
 * long event of the same hold does nothing on the main menu. No EEPROM access.
 */
static void test_long_menu_on_end_and_reopened_start(void) {
  uint8_t mode;
  bool is_reopened;

  for (mode = PROGRESS_MODE_RECEIVING; mode <= PROGRESS_MODE_SENDING; ++mode) {
    for (is_reopened = false;; is_reopened = true) {
      size_t stops;
      earn(mode, (mode == PROGRESS_MODE_SENDING) ? 20U : 32U, 2U);
      harness_open_mode(mode);
      run_session(10U);
      fake_iap_clear_counters();
      if (is_reopened) {
        reopen_with_enter();
      }
      stops = fake_keyer_stop_count();
      harness_pins_for(BUTTON_BIT_MENU, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
      TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
      TEST_ASSERT_EQUAL(mode, menu_cursor());
      harness_pins_for(BUTTON_BIT_MENU, 1000U);
      harness_pins_for(0U, 60U);
      TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
      TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
      TEST_ASSERT_EQUAL(mode, menu_cursor());
      TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
      TEST_ASSERT_EQUAL(stops, fake_keyer_stop_count());
      TEST_ASSERT_EQUAL(0U, fake_iap_access_count());
      TEST_ASSERT(!fake_keyer_is_running());
      TEST_ASSERT(!fake_player_is_running());
      if (is_reopened) {
        break;
      }
    }
  }
}

/* ---------- Golden main menu (AC-T34, ui-ux.md 5.16.6, V-01, V-149) ---------- */

static void test_golden_main_menu(void) {
  static const golden_t empty = MENU_SCREEN("Receiving            L1", "Sending              L1", 1);
  static const golden_t levels = MENU_SCREEN("Receiving           L12", "Sending              L1", 1);
  static const golden_t top_on_sending = MENU_SCREEN("Receiving        L65535", "Sending          L65535", 2);
  static const golden_t on_free_input = MENU_SCREEN("Receiving        L65535", "Sending          L65535", 3);

  fake_iap_reset();
  power_on();
  SCREEN_IS(empty);

  fake_iap_reset();
  progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, 11U);
  power_on();
  SCREEN_IS(levels);

  fake_iap_reset();
  progress_fixture_write(PROGRESS_MODE_RECEIVING, 32U, 65534U);
  progress_fixture_add(PROGRESS_MODE_SENDING, 20U, 65535U);
  power_on();
  harness_event(BUTTON_EVENT_NEXT);
  SCREEN_IS(top_on_sending);
  harness_event(BUTTON_EVENT_NEXT);
  SCREEN_IS(on_free_input);
}

/*
 * Q5 menu case: the level key is the base speed of the mode itself, never the effective speed and never the speed
 * of the other mode; the mode entry and the menu agree. A speed change shows at the next render and entry.
 */
static void test_level_key_is_own_base_speed(void) {
  fake_iap_reset();
  progress_fixture_write(PROGRESS_MODE_RECEIVING, 20U, 4U);
  progress_fixture_add(PROGRESS_MODE_RECEIVING, 15U, 30U); /* RX Eff. Speed */
  progress_fixture_add(PROGRESS_MODE_RECEIVING, 25U, 2U);  /* TX Speed */
  progress_fixture_add(PROGRESS_MODE_SENDING, 25U, 9U);
  progress_fixture_add(PROGRESS_MODE_SENDING, 20U, 40U); /* RX Speed */
  progress_fixture_add(PROGRESS_MODE_SENDING, 15U, 50U);
  power_on();
  set_rx_speeds(20U, 15U);
  set_tx_speeds(25U, 15U);
  menu_render();
  TEST_ASSERT(menu_row_is(1, "Receiving", "L5", true));
  TEST_ASSERT(menu_row_is(2, "Sending", "L10", false));
  harness_open_start_screen(0U);
  TEST_ASSERT(hdr("RX 20/15 L5          1/50"));
  TEST_ASSERT(qa_lvl(5U, 5U));
  harness_event(L_TAP);
  harness_open_start_screen(1U);
  TEST_ASSERT(hdr("TX 25/15 L10         1/50"));
  TEST_ASSERT(qa_lvl(10U, 10U));
  harness_event(L_TAP);
  /* Speed change between the menu and the mode. */
  set_rx_speeds(25U, 25U);
  set_tx_speeds(20U, 20U);
  menu_render();
  TEST_ASSERT(menu_row_is(1, "Receiving", "L3", false));
  TEST_ASSERT(menu_row_is(2, "Sending", "L41", true));
  harness_event(BUTTON_EVENT_PREV);
  harness_open_start_screen(0U);
  TEST_ASSERT(hdr("RX 25/25 L3          1/10"));
  TEST_ASSERT(qa_lvl(3U, 3U));
}

/* ---------- Tap mode, held buttons and the keyer (Q5 and Q7 mode cases) ---------- */

/*
 * AC-T32 (5), D-T39, B-6: after a leave from each phase, tap mode is off on the main menu and on the next start
 * screen. The phases include the start screen that the end screen opens.
 */
static void test_rx_tap_mode_off_after_leave_from_each_phase(void) {
  static const training_phase_t phases[6] = {TRAINING_PHASE_SELECT, TRAINING_PHASE_GAP, TRAINING_PHASE_ITEM,
                                             TRAINING_PHASE_RESULT, TRAINING_PHASE_END, TRAINING_PHASE_SELECT};
  size_t index;

  for (index = 0U; index < 6U; ++index) {
    const bool is_after_session = (index >= 4U);
    earn(PROGRESS_MODE_RECEIVING, 32U, 5U);
    harness_open_start_screen(0U);
    if (index != 0U) {
      harness_event(ENTER_EVENT);
    }
    if (phases[index] == TRAINING_PHASE_ITEM) {
      until_phase(TRAINING_PHASE_ITEM);
    } else if (phases[index] == TRAINING_PHASE_RESULT) {
      until_phase(TRAINING_PHASE_ITEM);
      answer_item(false);
    } else if (is_after_session) {
      run_session(50U);
      if (phases[index] == TRAINING_PHASE_SELECT) {
        reopen_with_enter();
      }
    }
    TEST_ASSERT_EQUAL(phases[index], training_phase());
    if ((phases[index] == TRAINING_PHASE_SELECT) || (phases[index] == TRAINING_PHASE_END)) {
      /* A real menu/back press leaves at the press; its long event later does nothing. */
      harness_pins_for(BUTTON_BIT_MENU, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
      TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
      harness_pins_for(BUTTON_BIT_MENU, 1000U);
    } else {
      /* A real menu/back hold: the long event leaves; the release then gives nothing on the main menu. */
      harness_pins_for(BUTTON_BIT_MENU, (uint16_t)(BUTTONS_DEBOUNCE_MS + BUTTONS_LONG_PRESS_MS + 5U));
      TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    }
    harness_pins_for(0U, 50U);
    TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
    TEST_ASSERT_EQUAL(0U, menu_cursor());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_IDLE, training_phase());
    TEST_ASSERT(!fake_player_is_running());
    TEST_ASSERT(is_tap_mode_off_on_main_menu());
    /* The next start screen: a prev press steps at the press. */
    harness_open_start_screen(0U);
    harness_pins_for(BUTTON_BIT_PREV, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
    /* A run with a session passed at the earned level, so its earned level is one higher. */
    TEST_ASSERT(qa_lvl(training_earned_level(), training_earned_level() + 1U));
    TEST_ASSERT_EQUAL(is_after_session ? 6U : 5U, training_earned_level());
    harness_pins_for(0U, 50U);
    harness_event(L_TAP);
  }
}

/* D-T39, ui-ux.md 5.16.4: prev held with repeats across the session start gives no event in the session. */
static void test_rx_prev_held_across_start(void) {
  uint16_t level;

  earn(PROGRESS_MODE_RECEIVING, 32U, 19U);
  harness_open_start_screen(0U);
  harness_pins_for(BUTTON_BIT_PREV, 500U); /* the press and one repeat */
  TEST_ASSERT_EQUAL(17U, training_selected_level());
  harness_pins_for(BUTTON_BIT_PREV | BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  level = training_selected_level();
  TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
  TEST_ASSERT_EQUAL(level, training_level());
  /* Hold both through the start pause and into ITEM, past the long-press time; then release in ITEM. */
  harness_pins_for(BUTTON_BIT_PREV | BUTTON_BIT_ENTER, 1500U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  harness_pins_for(0U, 60U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_ITEM, training_phase());
  TEST_ASSERT_EQUAL(0U, training_answer_length());
  TEST_ASSERT_EQUAL(level, training_level());
  /* One play of item 1 only: no replay from the held enter. */
  TEST_ASSERT_EQUAL(1U, fake_player_call_count());
}

/* AC-T32 (6): menu/back held from the last RESULT across the END event gives no event, also no long event. */
static void test_rx_menu_held_across_end(void) {
  static const golden_t end =
      END_SCREEN("RX 32/32 L6         10/10", NULL, "       Score 10/10", "    Passed, new level");
  earn(PROGRESS_MODE_RECEIVING, 32U, 5U);
  harness_open_mode(0U);
  run_to_last_answer(10U);
  TEST_ASSERT_EQUAL(TRAINING_PHASE_RESULT, training_phase());
  harness_pins_for(BUTTON_BIT_MENU, 1500U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
  harness_pins_for(0U, 60U);
  TEST_ASSERT_EQUAL(MENU_SCREEN_RECEIVING, menu_screen());
  SCREEN_IS(end);
  harness_pins_for(BUTTON_BIT_MENU, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  harness_pins_for(0U, 60U);
}

/*
 * D-T40, AC-T33 (1), (5), B-5, F5: the keyer runs only in a Sending session. A leave from SELECT, END or the start
 * screen that END opens does not touch the keyer and leaves enter with the menu; a long menu/back from GAP, ITEM or
 * RESULT stops it once and gives P3.2 back.
 */
static void test_tx_keyer_ownership_on_leave(void) {
  static const training_phase_t phases[6] = {TRAINING_PHASE_SELECT, TRAINING_PHASE_GAP, TRAINING_PHASE_ITEM,
                                             TRAINING_PHASE_RESULT, TRAINING_PHASE_END, TRAINING_PHASE_SELECT};
  size_t index;

  for (index = 0U; index < 6U; ++index) {
    const bool is_after_session = (index >= 4U);
    size_t stops;
    earn(PROGRESS_MODE_SENDING, 20U, 2U);
    harness_open_start_screen(1U);
    TEST_ASSERT_EQUAL(0U, fake_keyer_start_count());
    if (index != 0U) {
      harness_event(ENTER_EVENT);
      TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
    }
    if (phases[index] == TRAINING_PHASE_ITEM) {
      until_phase(TRAINING_PHASE_ITEM);
    } else if (phases[index] == TRAINING_PHASE_RESULT) {
      until_phase(TRAINING_PHASE_ITEM);
      answer_item(false);
    } else if (is_after_session) {
      run_session(10U);
      TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
      if (phases[index] == TRAINING_PHASE_SELECT) {
        reopen_with_enter();
        TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
        TEST_ASSERT(!fake_keyer_is_running());
      }
    }
    TEST_ASSERT_EQUAL(phases[index], training_phase());
    stops = fake_keyer_stop_count();
    if ((phases[index] == TRAINING_PHASE_SELECT) || (phases[index] == TRAINING_PHASE_END)) {
      harness_event(L_TAP);
      TEST_ASSERT_EQUAL(stops, fake_keyer_stop_count());
    } else {
      harness_event(LONG_EVENT(L_TAP));
      TEST_ASSERT_EQUAL(stops + 1U, fake_keyer_stop_count());
    }
    TEST_ASSERT(!fake_keyer_is_running());
    TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    TEST_ASSERT_EQUAL(1U, menu_cursor());
    /* P3.2 is enter on the main menu: a real enter press opens Sending again. */
    harness_pins_for(BUTTON_BIT_ENTER, (uint16_t)(BUTTONS_DEBOUNCE_MS + 2U));
    harness_pins_for(0U, 40U);
    TEST_ASSERT_EQUAL(MENU_SCREEN_SENDING, menu_screen());
    TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
    TEST_ASSERT(!fake_keyer_is_running());
    harness_event(L_TAP);
  }
}

/*
 * F8, B-6, ui-ux.md 5.16.4: re-entry always preselects the earned level. The preselection of the end screen is lost
 * after a leave from the end screen, and a selection made on the start screen that the end screen opened is lost too.
 */
static void test_reentry_preselects_earned(void) {
  bool is_reopened;

  for (is_reopened = false;; is_reopened = true) {
    earn(PROGRESS_MODE_RECEIVING, 32U, 9U);
    harness_open_start_screen(0U);
    select_down_to(7U);
    harness_event(ENTER_EVENT);
    run_session(50U);
    TEST_ASSERT_EQUAL(TRAINING_OUTCOME_PASS_REPLAY, training_outcome());
    TEST_ASSERT_EQUAL(8U, training_selected_level());
    event_with_budget(M_TAP, 0U);
    if (is_reopened) {
      reopen_with_enter();
      TEST_ASSERT(qa_lvl(9U, 10U));
      harness_event(M_TAP);
      TEST_ASSERT(qa_lvl(8U, 10U));
    }
    harness_event(L_TAP);
    TEST_ASSERT(menu_row_is(1, "Receiving", "L10", true));
    harness_open_start_screen(0U);
    TEST_ASSERT(qa_lvl(10U, 10U));
    TEST_ASSERT(hdr("RX 32/32 L10         1/50"));
    if (is_reopened) {
      break;
    }
  }
}

/* ---------- Random walk across END, SELECT and sessions (Q7: AC-T40 (5), AC-T42 to AC-T45, B-1 to B-6) ---------- */

static uint32_t walk_state;
static unsigned long walk_reopens;
static unsigned long walk_starts;
static unsigned long walk_end_leaves;
static unsigned long walk_select_leaves;
static unsigned long walk_quiet_end_events;
static unsigned long walk_steps;

static uint32_t walk_random(void) {
  walk_state = (walk_state * 1103515245U) + 12345U;
  return (walk_state >> 16) & 0x7FFFU;
}

/* SL-6: the SELECT header, "<tag> <speeds> L<selected>" from column 0 and "1/<n>" right-aligned to column 24. */
static bool is_select_header(bool is_tx) {
  char left[26];
  char item[12];
  char text[48];
  (void)snprintf(left, sizeof(left), "%s L%u", is_tx ? "TX 20/20" : "RX 32/32", (unsigned)training_new_shown_level());
  (void)snprintf(item, sizeof(item), "1/%u", (unsigned)koch_item_count(training_selected_level()));
  (void)snprintf(text, sizeof(text), "%-*s%s", 25 - (int)strlen(item), left, item);
  return hdr(text);
}

static void walk_open_mode(bool is_tx) {
  const size_t starts = fake_keyer_start_count();
  harness_event(ENTER_EVENT);
  TEST_ASSERT_EQUAL(is_tx ? MENU_SCREEN_SENDING : MENU_SCREEN_RECEIVING, menu_screen());
  TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
  TEST_ASSERT_EQUAL(training_earned_level(), training_selected_level());
  TEST_ASSERT_EQUAL(starts, fake_keyer_start_count());
}

static void walk_session(bool is_tx) {
  const unsigned count = training_item_count();
  const unsigned threshold = training_pass_threshold();
  const unsigned first_correct = ((walk_random() & 1U) != 0U) ? count : (threshold - 1U);
  TEST_ASSERT(fake_keyer_is_running() == is_tx);
  run_session(first_correct);
  /* B-1, SL-7: the end screen; the keyer stopped. */
  TEST_ASSERT(body_off(4, 6));
  TEST_ASSERT(qa_ok_button());
  TEST_ASSERT(margins_off());
  TEST_ASSERT(!fake_keyer_is_running());
  TEST_ASSERT(training_selected_level() <= training_earned_level());
}

/* The fake player starts so far: a leave may stop the player (receiving_leave()), but no switch plays. */
static size_t player_start_count(void) {
  size_t index;
  size_t starts = 0U;
  for (index = 0U; index < fake_player_call_count(); ++index) {
    starts += (fake_player_call_at(index) == FAKE_PLAYER_CALL_START) ? 1U : 0U;
  }
  return starts;
}

static void walk_end_event(bool is_tx) {
  static const button_event_t quiet[] = {M_TAP,
                                         R_TAP,
                                         REPEAT_EVENT(M_TAP),
                                         REPEAT_EVENT(R_TAP),
                                         LONG_EVENT(M_TAP),
                                         LONG_EVENT(R_TAP),
                                         LONG_EVENT(ENTER_EVENT)};
  const uint32_t choice = walk_random() % 16U;
  const uint16_t selected = training_selected_level();
  const size_t keyer_starts = fake_keyer_start_count();
  const size_t player_starts = player_start_count();

  if (choice < 8U) {
    event_with_budget(quiet[walk_random() % (sizeof(quiet) / sizeof(quiet[0]))], 0U);
    TEST_ASSERT_EQUAL(TRAINING_PHASE_END, training_phase());
    TEST_ASSERT_EQUAL(selected, training_selected_level());
    ++walk_quiet_end_events;
  } else if (choice < 15U) {
    reopen_with_enter();
    TEST_ASSERT_EQUAL(selected, training_selected_level());
    TEST_ASSERT(is_select_header(is_tx));
    TEST_ASSERT(qa_lvl(training_new_shown_level(), koch_shown_level(training_earned_level())));
    TEST_ASSERT(qa_start_button());
    TEST_ASSERT(blank_row(6));
    ++walk_reopens;
  } else {
    harness_event(L_TAP);
    TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    TEST_ASSERT_EQUAL(is_tx ? 1U : 0U, menu_cursor());
    walk_open_mode(is_tx);
    ++walk_end_leaves;
  }
  TEST_ASSERT_EQUAL(keyer_starts, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(player_starts, player_start_count());
}

static void walk_select_event(bool is_tx) {
  const uint32_t choice = walk_random() % 24U;
  const uint16_t selected = training_selected_level();
  const uint16_t earned = training_earned_level();

  if (choice < 18U) {
    const bool is_up = (choice & 1U) != 0U;
    const bool is_repeat = (choice & 2U) != 0U;
    const button_event_t base = is_up ? R_TAP : M_TAP;
    const bool can_step = is_up ? (selected < earned) : (selected > 0U);
    step_by_rule(is_repeat ? REPEAT_EVENT(base) : base, is_tx, can_step);
    TEST_ASSERT_EQUAL(can_step ? (is_up ? selected + 1U : selected - 1U) : selected, training_selected_level());
    TEST_ASSERT(is_select_header(is_tx));
    ++walk_steps;
  } else if (choice < 22U) {
    const size_t keyer_starts = fake_keyer_start_count();
    /* UX-70: the session start dirties at most pages 1-7, exactly the changed ones; page 0 stays clean. */
    const uint8_t changed = event_dirtychg(ENTER_EVENT);
    TEST_ASSERT_EQUAL(TRAINING_PHASE_GAP, training_phase());
    TEST_ASSERT_EQUAL(selected, training_level());
    TEST_ASSERT_EQUAL(0U, changed & PAGE(0));
    TEST_ASSERT_EQUAL(keyer_starts + (is_tx ? 1U : 0U), fake_keyer_start_count());
    ++walk_starts;
    walk_session(is_tx);
  } else {
    const size_t stops = fake_keyer_stop_count();
    harness_event(L_TAP);
    TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    TEST_ASSERT_EQUAL(stops, fake_keyer_stop_count());
    walk_open_mode(is_tx);
    ++walk_select_leaves;
  }
}

/*
 * A random walk through the end screen, the start screen it opens, and the sessions, in both modes. Every switch
 * keeps the redraw budget, the selection invariant and the keyer ownership (B-5); a step in END never acts (M1).
 */
static void test_random_walk_across_sessions(void) {
  uint32_t seed;
  const unsigned long failures_before = test_failure_count;

  walk_reopens = 0U;
  walk_starts = 0U;
  walk_end_leaves = 0U;
  walk_select_leaves = 0U;
  walk_quiet_end_events = 0U;
  walk_steps = 0U;
  for (seed = 1U; seed <= 12U; ++seed) {
    const bool is_tx = (seed & 1U) != 0U;
    const uint16_t earned = (uint16_t)(2U + 3U * (seed % 5U)); /* a word level: 10 items */
    unsigned action;
    walk_state = seed;
    earn(is_tx ? PROGRESS_MODE_SENDING : PROGRESS_MODE_RECEIVING, is_tx ? 20U : 32U, earned);
    harness_open_start_screen(is_tx ? 1U : 0U);
    for (action = 0U; action < 60U; ++action) {
      TEST_ASSERT(training_selected_level() <= training_earned_level());
      TEST_ASSERT_EQUAL(0U, harness_overlap_count);
      if (training_phase() == TRAINING_PHASE_END) {
        walk_end_event(is_tx);
      } else {
        TEST_ASSERT_EQUAL(TRAINING_PHASE_SELECT, training_phase());
        walk_select_event(is_tx);
      }
    }
    if (test_failure_count != failures_before) {
      fprintf(stderr, "random walk: first failure with seed %u\n", (unsigned)seed);
      return;
    }
  }
  printf("walk: %lu starts, %lu reopens, %lu quiet END events, %lu END leaves, %lu SELECT leaves, %lu steps\n",
         walk_starts, walk_reopens, walk_quiet_end_events, walk_end_leaves, walk_select_leaves, walk_steps);
  TEST_ASSERT(walk_starts > 20U);
  TEST_ASSERT(walk_reopens > 15U);
  TEST_ASSERT(walk_quiet_end_events > 20U);
  TEST_ASSERT(walk_end_leaves > 0U);
  TEST_ASSERT(walk_select_leaves > 0U);
}

int main(void) {
  TEST_RUN(test_oracle_big_text_ink);
  TEST_RUN(test_oracle_code_ranges);
  TEST_RUN(test_golden_start_screens);
  TEST_RUN(test_golden_prosign_descriptions);
  TEST_RUN(test_code_row_every_kind);
  TEST_RUN(test_step_walk_equals_full_render);
  TEST_RUN(test_step_pages_per_vector);
  TEST_RUN(test_step_walk_dirtychg);
  TEST_RUN(test_start_screen_ignores_last_item_length);
  TEST_RUN(test_golden_end_pass_fail_not_saved);
  TEST_RUN(test_golden_end_replay);
  TEST_RUN(test_golden_end_top);
  TEST_RUN(test_rx_end_screen_controls);
  TEST_RUN(test_tx_end_screen_tip);
  TEST_RUN(test_rx_prev_held_across_end_and_reopen);
  TEST_RUN(test_rx_next_held_in_end_then_enter);
  TEST_RUN(test_long_menu_on_end_and_reopened_start);
  TEST_RUN(test_golden_main_menu);
  TEST_RUN(test_level_key_is_own_base_speed);
  TEST_RUN(test_rx_tap_mode_off_after_leave_from_each_phase);
  TEST_RUN(test_rx_prev_held_across_start);
  TEST_RUN(test_rx_menu_held_across_end);
  TEST_RUN(test_tx_keyer_ownership_on_leave);
  TEST_RUN(test_reentry_preselects_earned);
  TEST_RUN(test_random_walk_across_sessions);
  return TEST_FINISH();
}
