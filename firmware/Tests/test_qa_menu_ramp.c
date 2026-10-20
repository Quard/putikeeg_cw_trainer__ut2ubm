/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA tests of the CW scroll and the Ramp preview (references/internal/sidetone_spec.md section 24.6 check 7,
 * menu_subsystem_spec.md sections 6.2 and 18.4 to 18.5, ui-ux.md sections 5.5 and 6.5). A random walk over the CW
 * list and its edits runs in lockstep with an independent model of the window rule and of the hooks H1 to H6. After
 * every event and every main-loop pass the test compares the exact sidetone call log, the cursor and window top of
 * the menu stack, and the title mark and window rows on the frame buffer.
 */

#include "menu_internal.h"
#include "support/training_harness.h"

#define QA_CW_COUNT 8U
#define QA_ROWS 7U
#define QA_PREVIEW_MS 300U
#define QA_TONE 0U
#define QA_RAMP 1U
#define QA_KEY_TYPE 6U
#define QA_PADDLE 7U
#define QA_MAX_EXPECTED 64U

static const char* const qa_labels[QA_CW_COUNT] = {"Tone",     "Ramp",          "RX Speed", "RX Eff. Speed",
                                                   "TX Speed", "TX Eff. Speed", "Key Type", "Paddle"};
static const settings_id_t qa_ids[QA_CW_COUNT] = {
    SETTINGS_ID_TONE_HZ, SETTINGS_ID_RAMP_MS,    SETTINGS_ID_RX_WPM,   SETTINGS_ID_RX_EFF_WPM,
    SETTINGS_ID_TX_WPM,  SETTINGS_ID_TX_EFF_WPM, SETTINGS_ID_KEY_TYPE, SETTINGS_ID_PADDLE_SWAP};

/* ---------- Model ---------- */

typedef struct {
  bool is_in_cw; /* false: the Settings list */
  bool is_edit;
  unsigned cursor; /* 0-based */
  unsigned top;
  unsigned tone;
  unsigned ramp;
  bool is_preview_running;
  uint16_t preview_start;
} qa_model_t;

static qa_model_t model;
static fake_sidetone_call_t qa_expected[QA_MAX_EXPECTED];
static size_t qa_expected_count;
static size_t qa_log_checked;
static unsigned long qa_step;

static void qa_expect(uint8_t kind, uint16_t value) {
  if (TEST_ASSERT(qa_expected_count < QA_MAX_EXPECTED)) {
    qa_expected[qa_expected_count].kind = kind;
    qa_expected[qa_expected_count].value = value;
    ++qa_expected_count;
  }
}

/* The calls since the last check must be exactly the expected ones. */
static void qa_check_log(const char* where) {
  const size_t count = fake_sidetone_call_count();
  size_t i;
  bool is_ok = TEST_ASSERT_EQUAL(qa_expected_count, count - qa_log_checked);
  for (i = 0U; is_ok && (i < qa_expected_count); ++i) {
    const fake_sidetone_call_t call = fake_sidetone_call_at(qa_log_checked + i);
    is_ok = TEST_ASSERT_EQUAL(qa_expected[i].kind, call.kind) && TEST_ASSERT_EQUAL(qa_expected[i].value, call.value);
  }
  if (!is_ok) {
    fprintf(stderr, "  sidetone log at %s, step %lu, time %u\n", where, qa_step, (unsigned)harness_now);
  }
  qa_log_checked = count;
  qa_expected_count = 0U;
  TEST_ASSERT(!fake_sidetone_is_log_overflowed());
  if (count > (FAKE_SIDETONE_LOG_SIZE / 2U)) {
    fake_sidetone_reset();
    qa_log_checked = 0U;
  }
}

/* menu_subsystem_spec.md section 6.2: the window moves only as far as the cursor needs. */
static void model_move(bool is_down, bool is_repeat) {
  const unsigned last = QA_CW_COUNT - 1U;
  if (is_down) {
    if (model.cursor < last) {
      ++model.cursor;
    } else if (!is_repeat) {
      model.cursor = 0U;
    }
  } else {
    if (model.cursor > 0U) {
      --model.cursor;
    } else if (!is_repeat) {
      model.cursor = last;
    }
  }
  if (model.cursor < model.top) {
    model.top = model.cursor;
  } else if (model.cursor > (model.top + QA_ROWS - 1U)) {
    model.top = model.cursor - (QA_ROWS - 1U);
  }
}

static bool model_is_preview_item(void) { return (model.cursor == QA_TONE) || (model.cursor == QA_RAMP); }

/* H1, H2 (D-RM8): Tone gives tone, on; Ramp gives ramp, tone, on. */
static void model_play(void) {
  if (model.cursor == QA_RAMP) {
    qa_expect(FAKE_SIDETONE_CALL_RAMP, (uint16_t)model.ramp);
  }
  qa_expect(FAKE_SIDETONE_CALL_TONE, (uint16_t)model.tone);
  qa_expect(FAKE_SIDETONE_CALL_ON, 1U);
  model.is_preview_running = true;
  model.preview_start = harness_now;
}

/* An edit step on the item under the cursor; returns true when the model value changed. */
static bool model_edit_step(bool is_up) {
  unsigned* value;
  unsigned min;
  unsigned max;
  unsigned step;
  if (model.cursor == QA_TONE) {
    value = &model.tone;
    min = 400U;
    max = 1000U;
    step = 10U;
  } else if (model.cursor == QA_RAMP) {
    value = &model.ramp;
    min = 4U;
    max = 12U;
    step = 1U;
  } else {
    return false;
  }
  if (is_up && (*value < max)) {
    *value += step;
    return true;
  }
  if (!is_up && (*value > min)) {
    *value -= step;
    return true;
  }
  return false;
}

/* ---------- Frame probes ---------- */

static void qa_value_text(unsigned item, char* text, size_t size) {
  static const char* const key_types[3] = {"Straight", "Iambic A", "Iambic B"};
  static const char* const paddles[2] = {"Normal", "Reversed"};
  const unsigned value = settings_get(qa_ids[item]);
  if (item == QA_TONE) {
    (void)snprintf(text, size, "%u Hz", value);
  } else if (item == QA_RAMP) {
    (void)snprintf(text, size, "%u ms", value);
  } else if (item == QA_KEY_TYPE) {
    (void)snprintf(text, size, "%s", key_types[value]);
  } else if (item == QA_PADDLE) {
    (void)snprintf(text, size, "%s", paddles[value]);
  } else {
    (void)snprintf(text, size, "%u WPM", value);
  }
}

static bool qa_cw_title_is(unsigned position) {
  char mark[8];
  row_text_t texts[2] = {{0, "Settings > CW", true}, {0, mark, true}};
  (void)snprintf(mark, sizeof(mark), "%u/8", position);
  texts[1].column = 25 - (int)strlen(mark);
  return row_matches(0, 0, 127, texts, 2U);
}

static bool qa_list_row_is(int r, unsigned item, bool is_selected) {
  char value[16];
  row_text_t texts[2] = {{0, qa_labels[item], is_selected}, {0, value, is_selected}};
  qa_value_text(item, value, sizeof(value));
  texts[1].column = 23 - (int)strlen(value);
  return row_matches(r, is_selected ? 0 : HARNESS_NO_BAND, 127, texts, 2U);
}

static void qa_check_menu(void) {
  bool is_ok = true;
  if (!model.is_in_cw) {
    is_ok &= TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
    is_ok &= TEST_ASSERT_EQUAL(0U, menu_stack_cursors[menu_depth - 1U]);
    is_ok &= TEST_ASSERT_EQUAL(0U, menu_stack_tops[menu_depth - 1U]);
    /* Six items: no mark, the cells after the title stay blank. */
    is_ok &= TEST_ASSERT(hdr("Settings"));
  } else {
    unsigned r;
    is_ok &= TEST_ASSERT_EQUAL(MENU_NODE_CW, menu_node());
    is_ok &= TEST_ASSERT_EQUAL(model.is_edit ? MENU_SCREEN_EDIT : MENU_SCREEN_LIST, menu_screen());
    is_ok &= TEST_ASSERT_EQUAL(model.cursor, menu_stack_cursors[menu_depth - 1U]);
    is_ok &= TEST_ASSERT_EQUAL(model.top, menu_stack_tops[menu_depth - 1U]);
    is_ok &= TEST_ASSERT(model.cursor >= model.top);
    is_ok &= TEST_ASSERT(model.cursor <= (model.top + QA_ROWS - 1U));
    is_ok &= TEST_ASSERT(qa_cw_title_is(model.cursor + 1U));
    for (r = 1U; r <= QA_ROWS; ++r) {
      const unsigned item = model.top + r - 1U;
      if (model.is_edit && (item == model.cursor)) {
        continue; /* the edit row has its own layout (V-179) */
      }
      is_ok &= TEST_ASSERT(qa_list_row_is((int)r, item, item == model.cursor));
    }
  }
  is_ok &= TEST_ASSERT_EQUAL(model.tone, settings_get(SETTINGS_ID_TONE_HZ));
  is_ok &= TEST_ASSERT_EQUAL(model.ramp, settings_get(SETTINGS_ID_RAMP_MS));
  if (!is_ok) {
    fprintf(stderr, "  menu state at step %lu: cursor %u top %u edit %d\n", qa_step, model.cursor, model.top,
            (int)model.is_edit);
  }
}

/* ---------- Driver ---------- */

/* One main-loop pass per ms, with H4 predicted at the exact pass. */
static void qa_wait(unsigned ms) {
  unsigned i;
  for (i = 0U; i < ms; ++i) {
    harness_set_time((uint16_t)(harness_now + 1U));
    if (model.is_preview_running && ((uint16_t)(harness_now - model.preview_start) >= QA_PREVIEW_MS)) {
      qa_expect(FAKE_SIDETONE_CALL_ON, 0U);
      model.is_preview_running = false;
    }
    harness_pass();
    qa_check_log("pass");
  }
}

static void qa_event(button_event_t event) {
  const bool is_repeat = (event & BUTTON_EVENT_REPEAT_FLAG) != 0U;
  const button_event_t code = (button_event_t)(event & (button_event_t)~BUTTON_EVENT_REPEAT_FLAG);

  if (!model.is_in_cw) {
    if (code == BUTTON_EVENT_ENTER) {
      model.is_in_cw = true;
      model.cursor = 0U;
      model.top = 0U;
    }
  } else if (!model.is_edit) {
    if ((code == BUTTON_EVENT_NEXT) || (code == BUTTON_EVENT_PREV)) {
      model_move(code == BUTTON_EVENT_NEXT, is_repeat);
    } else if (code == BUTTON_EVENT_ENTER) {
      model.is_edit = true;
      if (model_is_preview_item()) {
        model_play();
      }
    } else if (code == BUTTON_EVENT_MENU) {
      model.is_in_cw = false;
    }
  } else {
    if ((code == BUTTON_EVENT_NEXT) || (code == BUTTON_EVENT_PREV)) {
      if (model_edit_step(code == BUTTON_EVENT_NEXT)) {
        model_play(); /* H2; a step at a limit is H3: no call */
      }
    } else if ((code == BUTTON_EVENT_ENTER) || (code == BUTTON_EVENT_MENU)) {
      model.is_edit = false;
      if (model_is_preview_item()) {
        qa_expect(FAKE_SIDETONE_CALL_ON, 0U); /* H5, also after a timeout */
        model.is_preview_running = false;
      }
    }
  }
  harness_event(event);
  qa_check_log("event");
  qa_check_menu();
}

static void qa_open_cw(uint32_t seed) {
  fake_iap_reset();
  harness_power_on();
  harness_set_time((uint16_t)(seed * 7919U));
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_SETTINGS, menu_node());
  memset(&model, 0, sizeof(model));
  model.tone = 600U;
  model.ramp = 6U;
  qa_log_checked = fake_sidetone_call_count();
  qa_expected_count = 0U;
  qa_event(BUTTON_EVENT_ENTER);
}

/* Every cursor position, both directions, presses and repeats, both wraps (AC-M8, AC-M9). */
static void test_scroll_rule_exhaustive(void) {
  unsigned start;
  for (start = 0U; start < QA_CW_COUNT; ++start) {
    unsigned from_top;
    for (from_top = 0U; from_top < 2U; ++from_top) {
      unsigned kind;
      for (kind = 0U; kind < 4U; ++kind) {
        static const button_event_t events[4] = {BUTTON_EVENT_NEXT, BUTTON_EVENT_PREV, REPEAT_EVENT(BUTTON_EVENT_NEXT),
                                                 REPEAT_EVENT(BUTTON_EVENT_PREV)};
        unsigned n;
        qa_step = start * 100U + from_top * 10U + kind;
        qa_open_cw(qa_step);
        /* Reach the start cursor from the first window or from the last window. */
        if (from_top != 0U) {
          qa_event(BUTTON_EVENT_PREV); /* wrap: last window */
          while (model.cursor != start) {
            qa_event(BUTTON_EVENT_PREV);
          }
        } else {
          while (model.cursor != start) {
            qa_event(BUTTON_EVENT_NEXT);
          }
        }
        for (n = 0U; n < 10U; ++n) {
          qa_event(events[kind]);
        }
      }
    }
  }
}

/* Random walk: list moves, edits of every item, preview timeouts, leaves and re-entries. */
static void test_random_walk(void) {
  uint32_t rng = 0xACE1U;
  unsigned run;
  for (run = 0U; run < 60U; ++run) {
    unsigned n;
    qa_step = run;
    qa_open_cw(run);
    for (n = 0U; n < 600U; ++n) {
      unsigned pick;
      rng = rng * 1664525U + 1013904223U;
      pick = (rng >> 8) % 100U;
      qa_step = run * 1000U + n;
      if (pick < 22U) {
        qa_event(BUTTON_EVENT_NEXT);
      } else if (pick < 44U) {
        qa_event(BUTTON_EVENT_PREV);
      } else if (pick < 54U) {
        qa_event(REPEAT_EVENT(BUTTON_EVENT_NEXT));
      } else if (pick < 64U) {
        qa_event(REPEAT_EVENT(BUTTON_EVENT_PREV));
      } else if (pick < 76U) {
        qa_event(BUTTON_EVENT_ENTER);
      } else if (pick < 80U) {
        qa_event(model.is_in_cw ? BUTTON_EVENT_MENU : BUTTON_EVENT_ENTER);
      } else if (pick < 92U) {
        qa_wait(1U + ((rng >> 16) % 80U));
      } else {
        qa_wait(250U + ((rng >> 16) % 120U)); /* crosses the 300 ms timeout */
      }
      if (!model.is_in_cw) {
        qa_event(BUTTON_EVENT_ENTER); /* the walk stays in Settings > CW */
      }
    }
  }
}

/* A message over a Ramp edit stops the tone once, running or timed out; then the menu stays silent. */
static void test_message_over_ramp_edit(void) {
  unsigned delay;
  for (delay = 0U; delay <= 600U; delay += 25U) {
    qa_step = delay;
    qa_open_cw(delay);
    qa_event(BUTTON_EVENT_NEXT);
    qa_event(BUTTON_EVENT_ENTER);
    qa_event(BUTTON_EVENT_NEXT);
    qa_wait(delay);
    qa_expect(FAKE_SIDETONE_CALL_ON, 0U);
    model.is_preview_running = false;
    menu_show_message(MENU_MESSAGE_NOT_SAVED);
    qa_check_log("message");
    TEST_ASSERT_EQUAL(MENU_SCREEN_MESSAGE, menu_screen());
    qa_wait(500U);
    harness_event(BUTTON_EVENT_ENTER);
    qa_wait(10U);
    TEST_ASSERT_EQUAL(7U, settings_get(SETTINGS_ID_RAMP_MS));
  }
}

/* Lists with 7 items or fewer show no mark (AC-M9). */
static void test_short_lists_have_no_mark(void) {
  fake_iap_reset();
  harness_power_on();
  TEST_ASSERT(hdr("CW Trainer"));
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT(hdr("Settings"));
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_PREV);
  harness_event(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_DISPLAY, menu_node());
  TEST_ASSERT(hdr("Settings > Display"));
  TEST_ASSERT_EQUAL(0U, menu_stack_tops[menu_depth - 1U]);
  TEST_ASSERT_EQUAL(0U, fake_sidetone_call_count());
}

int main(void) {
  TEST_RUN(test_scroll_rule_exhaustive);
  TEST_RUN(test_random_walk);
  TEST_RUN(test_message_over_ramp_edit);
  TEST_RUN(test_short_lists_have_no_mark);
  return TEST_FINISH();
}
