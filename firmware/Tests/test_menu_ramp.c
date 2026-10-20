/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * CW list scroll, the Ramp item and the Ramp preview (references/internal/ui-ux.md section 9.6, V-173 to V-191;
 * references/internal/menu_subsystem_spec.md section 18, AC-M7 to AC-M10).
 */

#include "menu_internal.h"
#include "support/training_harness.h"
#include "util/crc8.h"

#define CW_ITEM_COUNT 8
#define CMD_PROGRAM 2U
#define CMD_ERASE 3U
#define ALL_PAGES 0xFFU

static const char* const cw_labels[CW_ITEM_COUNT] = {"Tone",     "Ramp",          "RX Speed", "RX Eff. Speed",
                                                     "TX Speed", "TX Eff. Speed", "Key Type", "Paddle"};
static const char* const cw_defaults[CW_ITEM_COUNT] = {"600 Hz", "6 ms",   "32 WPM",   "32 WPM",
                                                       "20 WPM", "20 WPM", "Iambic B", "Normal"};

/* Layout version 3 default record; tests change the flags byte and recompute the CRC. */
static const uint8_t default_record_v3[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x03U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                                0x14U, 0x02U, 0x69U, 0x05U, 0x08U, 0x20U, 0x0BU};

/* Index of the first SLOG entry that no ADD check has seen yet. */
static size_t slog_checked;

static size_t eeprom_writes(void) { return fake_iap_command_count(CMD_PROGRAM) + fake_iap_command_count(CMD_ERASE); }

/* ---------- Probes ---------- */

/* A list row: label in column 0, an optional value right-aligned to column 22. */
static bool list_row_is(int r, const char* label, const char* value, bool is_selected) {
  row_text_t texts[2] = {{0, label, is_selected}, {0, "", is_selected}};
  size_t count = 1U;
  if (value != NULL) {
    texts[1].column = 23 - (int)strlen(value);
    texts[1].text = value;
    count = 2U;
  }
  return row_matches(r, is_selected ? 0 : HARNESS_NO_BAND, 127, texts, count);
}

static bool edit_row_is(int r, const char* label, const char* value, int fw, bool has_less, bool has_more) {
  row_text_t texts[4];
  size_t count = 0U;
  texts[count++] = (row_text_t){0, label, false};
  texts[count++] = (row_text_t){23 - (int)strlen(value), value, true};
  if (has_less) {
    texts[count++] = (row_text_t){21 - fw, "<", true};
  }
  if (has_more) {
    texts[count++] = (row_text_t){24, ">", true};
  }
  return row_matches(r, 106 - 5 * fw, 127, texts, count);
}

/* MARK(s): the CW title, blank cells after it, and @p mark inverted with its last character in column 24. */
static bool mark_is(const char* mark) {
  const row_text_t texts[2] = {{0, "Settings > CW", true}, {25 - (int)strlen(mark), mark, true}};
  return row_matches(0, 0, 127, texts, 2U);
}

/* MARK("<position>/8") */
static bool mark_at(int position) {
  char mark[8];
  (void)snprintf(mark, sizeof(mark), "%d/8", position);
  return mark_is(mark);
}

/* WIN(t) with @p values and SEL(@p selected_row): rows 1-7 show the CW items t to t + 6. */
static bool window_with(int top, int selected_row, const char* const values[CW_ITEM_COUNT]) {
  int r;
  bool is_ok = true;
  for (r = 1; r <= 7; ++r) {
    const int item = top - 1 + r - 1;
    is_ok = is_ok && list_row_is(r, cw_labels[item], values[item], r == selected_row);
  }
  return is_ok;
}

static bool window_is(int top, int selected_row) { return window_with(top, selected_row, cw_defaults); }

/* MARK, WIN and SEL of one CW screen; @p cursor is 1-based. */
static bool cw_screen_is(int cursor, int top) { return mark_at(cursor) && window_is(top, cursor - top + 1); }

/* The title bar of a list with no mark: the title, then blank cells up to column 24. */
static bool plain_title_is(const char* title) { return hdr(title); }

/* ---------- Steps ---------- */

static void step(button_event_t event) { harness_event(event); }

static void steps(button_event_t event, unsigned count) {
  unsigned i;
  for (i = 0U; i < count; ++i) {
    step(event);
  }
}

/* From Receiving, prev goes to About, then to Settings. */
static void open_settings(void) {
  steps(BUTTON_EVENT_PREV, 2U);
  step(ENTER_EVENT);
}

static void open_cw(void) {
  open_settings();
  step(ENTER_EVENT);
}

/* CW: power-on with an empty fake EEPROM, open Settings, then CW. */
static void cw(void) {
  fake_iap_reset();
  harness_power_on();
  open_cw();
}

static void put_record_v3(uint8_t flags) {
  uint8_t record[SETTINGS_RECORD_SIZE];
  memcpy(record, default_record_v3, sizeof(record));
  record[10] = flags;
  record[14] = crc8_compute(record, 14U);
  fake_iap_reset();
  memcpy(fake_iap_memory(), record, sizeof(record));
}

static void reset_slog(void) {
  fake_sidetone_reset();
  slog_checked = 0U;
}

/* CWRAMP(ms): Ramp = @p ramp_ms, Tone 600 Hz, cursor on Ramp in browse mode, empty SLOG. */
static void cw_ramp(uint8_t ramp_ms) {
  if (ramp_ms == 6U) {
    fake_iap_reset();
  } else {
    put_record_v3((uint8_t)(0x09U | (uint8_t)(ramp_ms << 4)));
  }
  harness_power_on();
  open_cw();
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT_EQUAL(1U, menu_cursor());
  TEST_ASSERT_EQUAL(ramp_ms, settings_get(SETTINGS_ID_RAMP_MS));
  reset_slog();
}

/* AT t: X */
static void at(uint16_t time, button_event_t event) {
  harness_set_time(time);
  harness_event(event);
}

/* AT t: poll */
static void at_poll(uint16_t time) {
  harness_set_time(time);
  menu_poll();
}

/* POLL t..u */
static void poll_range(uint16_t from, uint16_t to) {
  uint16_t time = from;
  for (;;) {
    at_poll(time);
    if (time == to) {
      break;
    }
    time = (uint16_t)(time + 1U);
  }
}

/* ---------- SLOG ---------- */

#define TONE(hz) {FAKE_SIDETONE_CALL_TONE, (hz)}
#define RAMP(ms) {FAKE_SIDETONE_CALL_RAMP, (ms)}
#define ON {FAKE_SIDETONE_CALL_ON, 1U}
#define OFF {FAKE_SIDETONE_CALL_ON, 0U}

/* ADD[...]: since the previous check, exactly these entries came into SLOG, in this order. */
static bool slog_add(const fake_sidetone_call_t* expected, size_t count) {
  size_t i;
  if (fake_sidetone_call_count() != slog_checked + count) {
    fprintf(stderr, "SLOG has %zu new entries, expected %zu\n", fake_sidetone_call_count() - slog_checked, count);
    slog_checked = fake_sidetone_call_count();
    return false;
  }
  for (i = 0U; i < count; ++i) {
    const fake_sidetone_call_t call = fake_sidetone_call_at(slog_checked + i);
    if ((call.kind != expected[i].kind) || (call.value != expected[i].value)) {
      fprintf(stderr, "SLOG entry %zu is kind %u value %u\n", slog_checked + i, call.kind, call.value);
      slog_checked = fake_sidetone_call_count();
      return false;
    }
  }
  slog_checked += count;
  return true;
}

static bool slog_none(void) { return slog_add(NULL, 0U); }

static bool slog_ramp_start(uint16_t ramp_ms) {
  const fake_sidetone_call_t expected[3] = {RAMP(ramp_ms), TONE(600U), ON};
  return slog_add(expected, 3U);
}

static bool slog_off(void) {
  const fake_sidetone_call_t expected[1] = {OFF};
  return slog_add(expected, 1U);
}

/* Global SLOG rule of ui-ux.md section 9.1: on(true) right after tone(); ramp() right before that tone(). */
static void check_slog_rules(void) {
  size_t i;
  for (i = 0U; i < fake_sidetone_call_count(); ++i) {
    const fake_sidetone_call_t call = fake_sidetone_call_at(i);
    if ((call.kind == FAKE_SIDETONE_CALL_ON) && (call.value == 1U)) {
      TEST_ASSERT((i > 0U) && (fake_sidetone_call_at(i - 1U).kind == FAKE_SIDETONE_CALL_TONE));
    }
    if (call.kind == FAKE_SIDETONE_CALL_RAMP) {
      TEST_ASSERT_EQUAL(FAKE_SIDETONE_CALL_TONE, fake_sidetone_call_at(i + 1U).kind);
      TEST_ASSERT_EQUAL(FAKE_SIDETONE_CALL_ON, fake_sidetone_call_at(i + 2U).kind);
    }
  }
  TEST_ASSERT(!fake_sidetone_is_log_overflowed());
}

/* ---------- Tree ---------- */

/* AC-M7: CW has 8 items in the order of menu_subsystem_spec.md section 18.1. */
static void test_cw_tree(void) {
  const menu_node_t* node = &menu_nodes[MENU_NODE_CW];
  const menu_item_t* ramp = &node->items[1];
  int index;
  TEST_ASSERT_EQUAL(CW_ITEM_COUNT, node->item_count);
  for (index = 0; index < CW_ITEM_COUNT; ++index) {
    TEST_ASSERT_EQUAL(0, strcmp(cw_labels[index], node->items[index].label));
  }
  TEST_ASSERT_EQUAL(MENU_KIND_VALUE, ramp->kind);
  TEST_ASSERT_EQUAL(SETTINGS_ID_RAMP_MS, ramp->target);
  TEST_ASSERT_EQUAL(0, strcmp("ms", ramp->unit));
  TEST_ASSERT(ramp->texts == NULL);
  TEST_ASSERT_EQUAL(5U, ramp->field_width);
  /* The CW title with the longest mark of the tree fits in 24 - len("8/8") columns (ui-ux.md section 5.1). */
  TEST_ASSERT(strlen(node->title) <= 24U - 3U);
}

/* ---------- Scroll ---------- */

/* V-173 */
static void test_v173_cw_opens(void) {
  int r;
  cw();
  TEST_ASSERT(cw_screen_is(1, 1));
  TEST_ASSERT(list_row_is(2, "Ramp", "6 ms", false));
  TEST_ASSERT(list_row_is(7, "Key Type", "Iambic B", false));
  for (r = 1; r <= 7; ++r) {
    TEST_ASSERT(!list_row_is(r, "Paddle", "Normal", false));
  }
  TEST_ASSERT_EQUAL(0U, menu_stack_tops[2]);
}

/* V-174, V-175 */
static void test_v174_v175_next_through_the_list(void) {
  int k;
  cw();
  for (k = 1; k <= 6; ++k) {
    harness_mark_frame();
    step(BUTTON_EVENT_NEXT);
    TEST_ASSERT(cw_screen_is(k + 1, 1));
    TEST_ASSERT_EQUAL(PAGE(0) | PAGE(k) | PAGE(k + 1), harness_changed_pages());
  }
  harness_mark_frame();
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT(cw_screen_is(8, 2));
  TEST_ASSERT(list_row_is(7, "Paddle", "Normal", true));
  TEST_ASSERT(list_row_is(1, "Ramp", "6 ms", false));
  TEST_ASSERT_EQUAL(ALL_PAGES, harness_changed_pages());
  TEST_ASSERT_EQUAL(1U, menu_stack_tops[2]);
}

/* V-176 */
static void test_v176_prev_back_to_the_first_window(void) {
  int k;
  cw();
  steps(BUTTON_EVENT_NEXT, 7U);
  for (k = 1; k <= 6; ++k) {
    harness_mark_frame();
    step(BUTTON_EVENT_PREV);
    TEST_ASSERT(cw_screen_is(8 - k, 2));
    TEST_ASSERT_EQUAL(PAGE(0) | PAGE(8 - k) | PAGE(7 - k), harness_changed_pages());
  }
  TEST_ASSERT(list_row_is(1, "Ramp", "6 ms", true));
  harness_mark_frame();
  step(BUTTON_EVENT_PREV);
  TEST_ASSERT(cw_screen_is(1, 1));
  TEST_ASSERT(list_row_is(1, "Tone", "600 Hz", true));
  TEST_ASSERT_EQUAL(ALL_PAGES, harness_changed_pages());
}

/* V-177: a press wraps to the last 7 or the first 7 items. */
static void test_v177_wrap(void) {
  cw();
  step(BUTTON_EVENT_PREV);
  TEST_ASSERT(cw_screen_is(8, 2));
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT(cw_screen_is(1, 1));
}

/* V-178: a hold never wraps. */
static void test_v178_hold_stops_at_the_ends(void) {
  cw();
  harness_pins_for(BUTTON_BIT_NEXT, 1500U);
  harness_pins_for(0U, 100U);
  TEST_ASSERT(cw_screen_is(8, 2));
  harness_pins_for(BUTTON_BIT_PREV, 1500U);
  harness_pins_for(0U, 100U);
  TEST_ASSERT(cw_screen_is(1, 1));
}

/* V-179 */
static void test_v179_ramp_edit_limits(void) {
  static const char* const values[9] = {"4 ms", "5 ms", "6 ms", "7 ms", "8 ms", "9 ms", "10 ms", "11 ms", "12 ms"};
  int value;
  cw();
  step(BUTTON_EVENT_NEXT);
  step(ENTER_EVENT);
  TEST_ASSERT_EQUAL(MENU_SCREEN_EDIT, menu_screen());
  TEST_ASSERT(edit_row_is(2, "Ramp", "6 ms", 5, true, true));
  TEST_ASSERT(mark_is("2/8"));
  for (value = 7; value <= 12; ++value) {
    step(BUTTON_EVENT_NEXT);
    TEST_ASSERT(edit_row_is(2, "Ramp", values[value - 4], 5, true, value < 12));
  }
  menu_handle(BUTTON_EVENT_NEXT);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(12U, settings_get(SETTINGS_ID_RAMP_MS));
  for (value = 11; value >= 4; --value) {
    step(BUTTON_EVENT_PREV);
    TEST_ASSERT(edit_row_is(2, "Ramp", values[value - 4], 5, value > 4, true));
  }
  menu_handle(BUTTON_EVENT_PREV);
  TEST_ASSERT(!menu_needs_redraw());
  TEST_ASSERT_EQUAL(4U, settings_get(SETTINGS_ID_RAMP_MS));
  step(ENTER_EVENT);
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  TEST_ASSERT(list_row_is(2, "Ramp", "4 ms", true));
  TEST_ASSERT(mark_is("2/8"));
  TEST_ASSERT_EQUAL(0U, menu_stack_tops[2]);
}

/* V-180, F-4 */
static void test_v180_ramp_saves_on_settings_exit(void) {
  cw();
  step(BUTTON_EVENT_NEXT);
  step(ENTER_EVENT);
  steps(BUTTON_EVENT_NEXT, 3U);
  step(BUTTON_EVENT_MENU);
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(0U, eeprom_writes());
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(1U, fake_iap_command_count(CMD_ERASE));
  TEST_ASSERT_EQUAL(SETTINGS_RECORD_SIZE, fake_iap_command_count(CMD_PROGRAM));
  TEST_ASSERT_EQUAL(0x99U, fake_iap_memory()[10]);
  harness_power_on();
  TEST_ASSERT_EQUAL(9U, settings_get(SETTINGS_ID_RAMP_MS));
  open_cw();
  TEST_ASSERT(list_row_is(2, "Ramp", "9 ms", false));
  step(BUTTON_EVENT_NEXT);
  step(ENTER_EVENT);
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT_EQUAL(10U, settings_get(SETTINGS_ID_RAMP_MS));
  harness_power_on();
  TEST_ASSERT_EQUAL(9U, settings_get(SETTINGS_ID_RAMP_MS));
}

/* V-181: UX-14 in the last window; edit steps never move the window. */
static void test_v181_base_and_effective_speed_in_the_last_window(void) {
  static const char* const values[CW_ITEM_COUNT] = {"600 Hz", "6 ms",   "24 WPM",   "24 WPM",
                                                    "20 WPM", "20 WPM", "Iambic B", "Normal"};
  unsigned i;
  cw();
  steps(BUTTON_EVENT_NEXT, 3U);
  step(ENTER_EVENT);
  for (i = 0U; i < 7U; ++i) {
    step(BUTTON_EVENT_PREV);
  }
  step(ENTER_EVENT);
  TEST_ASSERT_EQUAL(25U, settings_get(SETTINGS_ID_RX_EFF_WPM));
  steps(BUTTON_EVENT_PREV, 3U); /* back to Tone */
  step(BUTTON_EVENT_PREV);      /* wrap to Paddle, last window */
  steps(BUTTON_EVENT_PREV, 5U);
  TEST_ASSERT_EQUAL(2U, menu_cursor());
  TEST_ASSERT(mark_is("3/8"));
  step(ENTER_EVENT);
  TEST_ASSERT(edit_row_is(2, "RX Speed", "32 WPM", 6, true, true));
  steps(BUTTON_EVENT_PREV, 8U);
  TEST_ASSERT(edit_row_is(2, "RX Speed", "24 WPM", 6, true, true));
  TEST_ASSERT(list_row_is(3, "RX Eff. Speed", "24 WPM", false));
  TEST_ASSERT_EQUAL(1U, menu_stack_tops[2]);
  step(ENTER_EVENT);
  TEST_ASSERT(mark_is("3/8") && window_with(2, 2, values));
}

/* V-182: lists with 7 items or fewer have no mark, and a cursor step changes only the two row pages. */
static void test_v182_short_lists_have_no_mark(void) {
  fake_iap_reset();
  harness_power_on();
  TEST_ASSERT(plain_title_is("CW Trainer"));
  harness_mark_frame();
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT_EQUAL(PAGE(1) | PAGE(2), harness_changed_pages());
  step(BUTTON_EVENT_PREV);
  open_settings();
  TEST_ASSERT(plain_title_is("Settings"));
  harness_mark_frame();
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT_EQUAL(PAGE(1) | PAGE(2), harness_changed_pages());
  steps(BUTTON_EVENT_NEXT, 3U);
  step(ENTER_EVENT);
  TEST_ASSERT(plain_title_is("Settings > Display"));
  harness_mark_frame();
  step(BUTTON_EVENT_NEXT);
  TEST_ASSERT_EQUAL(PAGE(1) | PAGE(2), harness_changed_pages());
}

/* V-183: a push gives top 0; a pop keeps the parent cursor and top. */
static void test_v183_window_resets_on_open(void) {
  cw();
  step(BUTTON_EVENT_PREV);
  TEST_ASSERT(cw_screen_is(8, 2));
  step(BUTTON_EVENT_MENU);
  TEST_ASSERT(plain_title_is("Settings"));
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT_EQUAL(0U, menu_stack_tops[1]);
  step(ENTER_EVENT);
  TEST_ASSERT(cw_screen_is(1, 1));
}

/* V-184, F-26: a layout version 1 or 2 record shows 6 ms and saves nothing. */
static void test_v184_older_records_show_6_ms(void) {
  static const uint8_t records[2][SETTINGS_RECORD_SIZE] = {
      {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x05U, 0x05U, 0x08U, 0x20U, 0xEBU},
      {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x03U, 0x05U, 0x08U, 0x20U, 0x78U},
  };
  size_t index;
  for (index = 0U; index < 2U; ++index) {
    fake_iap_reset();
    memcpy(fake_iap_memory(), records[index], SETTINGS_RECORD_SIZE);
    harness_power_on();
    TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
    open_cw();
    TEST_ASSERT(list_row_is(2, "Ramp", "6 ms", false));
    TEST_ASSERT_EQUAL((index == 0U) ? SETTINGS_PROSIGN_NAME : SETTINGS_PROSIGN_BOTH,
                      settings_get(SETTINGS_ID_PROSIGN_STYLE));
    TEST_ASSERT_EQUAL((index == 0U) ? 0U : 1U, settings_get(SETTINGS_ID_PADDLE_SWAP));
    step(BUTTON_EVENT_MENU);
    step(BUTTON_EVENT_MENU);
    TEST_ASSERT_EQUAL(0U, eeprom_writes());
  }
}

/* ---------- Ramp preview ---------- */

/* V-185 */
static void test_v185_preview_at_edit_start(void) {
  cw_ramp(6U);
  at(10000U, ENTER_EVENT);
  TEST_ASSERT(slog_ramp_start(6U));
  poll_range(10001U, 10299U);
  TEST_ASSERT(slog_none());
  at_poll(10300U);
  TEST_ASSERT(slog_off());
  check_slog_rules();
}

/* V-186 */
static void test_v186_preview_restarts_on_change(void) {
  cw_ramp(6U);
  at(10000U, ENTER_EVENT);
  TEST_ASSERT(slog_ramp_start(6U));
  at(10100U, BUTTON_EVENT_NEXT);
  TEST_ASSERT(slog_ramp_start(7U));
  poll_range(10101U, 10399U);
  TEST_ASSERT(slog_none());
  at_poll(10400U);
  TEST_ASSERT(slog_off());
  at(11000U, BUTTON_EVENT_PREV);
  TEST_ASSERT(slog_ramp_start(6U));
  at_poll(11300U);
  TEST_ASSERT(slog_off());
  check_slog_rules();
}

/* V-187: a step at a limit plays nothing new and keeps the timer. */
static void limit_case(uint8_t ramp_ms, button_event_t event) {
  cw_ramp(ramp_ms);
  at(10000U, ENTER_EVENT);
  TEST_ASSERT(slog_ramp_start(ramp_ms));
  at(10100U, event);
  TEST_ASSERT(slog_none());
  poll_range(10101U, 10299U);
  TEST_ASSERT(slog_none());
  at_poll(10300U);
  TEST_ASSERT(slog_off());
  at(10500U, event);
  poll_range(10501U, 11000U);
  TEST_ASSERT(slog_none());
  check_slog_rules();
}

static void test_v187_limits(void) {
  limit_case(12U, BUTTON_EVENT_NEXT);
  limit_case(4U, BUTTON_EVENT_PREV);
}

/* V-188: a held next from 4 ms keeps the tone on until 300 ms after the last change. */
static void test_v188_held_next(void) {
  uint16_t k;
  uint16_t time;
  cw_ramp(4U);
  at(10000U, ENTER_EVENT);
  TEST_ASSERT(slog_ramp_start(4U));
  at_poll(10300U);
  TEST_ASSERT(slog_off());
  at(11000U, BUTTON_EVENT_NEXT);
  TEST_ASSERT(slog_ramp_start(5U));
  poll_range(11001U, 11399U);
  TEST_ASSERT(slog_off());
  for (k = 0U; k <= 7U; ++k) {
    time = (uint16_t)(11400U + 80U * k);
    at(time, REPEAT_EVENT(BUTTON_EVENT_NEXT));
    if (k <= 6U) {
      TEST_ASSERT(slog_ramp_start((uint16_t)(6U + k)));
    } else {
      TEST_ASSERT(slog_none());
    }
    poll_range((uint16_t)(time + 1U), (uint16_t)(time + 79U));
    TEST_ASSERT(slog_none());
  }
  poll_range(12040U, 12179U);
  TEST_ASSERT(slog_none());
  at_poll(12180U);
  TEST_ASSERT(slog_off());
  TEST_ASSERT_EQUAL(12U, settings_get(SETTINGS_ID_RAMP_MS));
  check_slog_rules();
}

/* V-189: each leave path stops the tone exactly once, also after a timeout. */
static void test_v189_leave_paths(void) {
  cw_ramp(6U);
  at(10000U, ENTER_EVENT);
  TEST_ASSERT(slog_ramp_start(6U));
  at(10100U, ENTER_EVENT);
  TEST_ASSERT(slog_off());
  TEST_ASSERT_EQUAL(MENU_SCREEN_LIST, menu_screen());
  poll_range(10101U, 10600U);
  TEST_ASSERT(slog_none());

  cw_ramp(6U);
  at(10000U, ENTER_EVENT);
  TEST_ASSERT(slog_ramp_start(6U));
  at(10100U, BUTTON_EVENT_MENU);
  TEST_ASSERT(slog_off());
  TEST_ASSERT_EQUAL(MENU_NODE_CW, menu_node());
  poll_range(10101U, 10600U);
  TEST_ASSERT(slog_none());
  at(11000U, ENTER_EVENT);
  TEST_ASSERT(slog_ramp_start(6U));
  at_poll(11300U);
  TEST_ASSERT(slog_off());
  at(11500U, BUTTON_EVENT_MENU);
  TEST_ASSERT(slog_off());
  poll_range(11501U, 12000U);
  TEST_ASSERT(slog_none());

  cw_ramp(6U);
  at(10000U, ENTER_EVENT);
  TEST_ASSERT(slog_ramp_start(6U));
  harness_set_time(10100U);
  menu_show_message(MENU_MESSAGE_NOT_SAVED);
  menu_poll();
  TEST_ASSERT(slog_off());
  poll_range(10101U, 10600U);
  TEST_ASSERT(slog_none());
  step(ENTER_EVENT);
  TEST_ASSERT(slog_none());
  check_slog_rules();
}

/* V-190: browse mode over Ramp makes no sound. */
static void test_v190_browse_is_silent(void) {
  fake_iap_reset();
  harness_power_on();
  reset_slog();
  open_cw();
  steps(BUTTON_EVENT_NEXT, 3U);
  steps(BUTTON_EVENT_PREV, 3U);
  steps(REPEAT_EVENT(BUTTON_EVENT_NEXT), 9U);
  steps(REPEAT_EVENT(BUTTON_EVENT_PREV), 9U);
  step(BUTTON_EVENT_PREV);
  step(BUTTON_EVENT_NEXT);
  step(BUTTON_EVENT_NEXT);
  poll_range(10000U, 10500U);
  TEST_ASSERT_EQUAL(0U, fake_sidetone_call_count());
}

/* V-191: the Tone edit sends no ramp; the sidetone keeps the last Ramp value. */
static void test_v191_tone_edit_after_ramp_edit(void) {
  cw_ramp(6U);
  at(10000U, ENTER_EVENT);
  at(10100U, BUTTON_EVENT_NEXT);
  at(10200U, BUTTON_EVENT_NEXT);
  at(10300U, BUTTON_EVENT_NEXT);
  at(10400U, BUTTON_EVENT_NEXT);
  at(10500U, ENTER_EVENT);
  poll_range(10501U, 11000U);
  TEST_ASSERT_EQUAL(10U, fake_sidetone_last_ramp());
  at(11100U, BUTTON_EVENT_PREV);
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  slog_checked = fake_sidetone_call_count();
  at(11200U, ENTER_EVENT);
  {
    const fake_sidetone_call_t expected[2] = {TONE(600U), ON};
    TEST_ASSERT(slog_add(expected, 2U));
  }
  TEST_ASSERT_EQUAL(10U, fake_sidetone_last_ramp());
  check_slog_rules();
}

int main(void) {
  TEST_RUN(test_cw_tree);
  TEST_RUN(test_v173_cw_opens);
  TEST_RUN(test_v174_v175_next_through_the_list);
  TEST_RUN(test_v176_prev_back_to_the_first_window);
  TEST_RUN(test_v177_wrap);
  TEST_RUN(test_v178_hold_stops_at_the_ends);
  TEST_RUN(test_v179_ramp_edit_limits);
  TEST_RUN(test_v180_ramp_saves_on_settings_exit);
  TEST_RUN(test_v181_base_and_effective_speed_in_the_last_window);
  TEST_RUN(test_v182_short_lists_have_no_mark);
  TEST_RUN(test_v183_window_resets_on_open);
  TEST_RUN(test_v184_older_records_show_6_ms);
  TEST_RUN(test_v185_preview_at_edit_start);
  TEST_RUN(test_v186_preview_restarts_on_change);
  TEST_RUN(test_v187_limits);
  TEST_RUN(test_v188_held_next);
  TEST_RUN(test_v189_leave_paths);
  TEST_RUN(test_v190_browse_is_silent);
  TEST_RUN(test_v191_tone_edit_after_ramp_edit);
  return TEST_FINISH();
}
