/*
 * QA tests for the Tone preview of the menu (references/internal/ui-ux.md section 6.5, sidetone_spec.md 10.3).
 * A small model of the preview (running or stopped, start time, Tone value) predicts the sidetone calls of each
 * step; the tests check the timeout at every start tick across the 16-bit wrap and random event sequences.
 */

#include <string.h>

#include "board/button_pins.h"
#include "buttons/buttons.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_menu_outputs.h"
#include "fake_sidetone.h"
#include "fake_tick.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "menu/menu.h"
#include "menu_internal.h"
#include "settings/settings.h"
#include "support/test_assert.h"
#include "util/crc8.h"

#define REPEAT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))
#define QA_LOG_RESET_LEVEL 900U

static const uint8_t qa_default_record[SETTINGS_RECORD_SIZE] = {0x43U, 0x57U, 0x01U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U,
                                                                0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0x54U};

static struct {
  bool is_running;
  bool is_edit;
  bool is_message;
  uint16_t start;
  uint16_t tone_hz;
} preview;

static size_t slog_checked;
static unsigned long qa_step;

/* Power-on as main.c does it, without the sidetone module; Settings > CW, cursor on Tone. */
static void qa_cw_tone(uint16_t tone_hz) {
  uint8_t record[SETTINGS_RECORD_SIZE];

  fake_iap_reset();
  memcpy(record, qa_default_record, sizeof(record));
  record[3] = (uint8_t)(tone_hz & 0xFFU);
  record[4] = (uint8_t)(tone_hz >> 8);
  record[14] = crc8_compute(record, 14U);
  memcpy(fake_iap_memory(), record, sizeof(record));

  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_delay_reset();
  lcd_fb_clear();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  (void)settings_load();
  menu_init();
  menu_handle(BUTTON_EVENT_PREV);
  menu_handle(BUTTON_EVENT_ENTER);
  menu_handle(BUTTON_EVENT_ENTER);
  TEST_ASSERT_EQUAL(MENU_NODE_CW, menu_node());
  TEST_ASSERT_EQUAL(0U, menu_cursor());
  TEST_ASSERT_EQUAL(tone_hz, settings_get(SETTINGS_ID_TONE_HZ));

  fake_sidetone_reset();
  slog_checked = 0U;
  preview.is_running = false;
  preview.is_edit = false;
  preview.is_message = false;
  preview.tone_hz = tone_hz;
}

static bool qa_expect_call(uint8_t kind, uint16_t value) {
  const fake_sidetone_call_t call = fake_sidetone_call_at(slog_checked);
  ++slog_checked;
  if (!TEST_ASSERT_EQUAL(kind, call.kind) || !TEST_ASSERT_EQUAL(value, call.value)) {
    printf("  step %lu\n", qa_step);
    return false;
  }
  return true;
}

static void qa_expect_end(void) {
  if (!TEST_ASSERT_EQUAL(slog_checked, fake_sidetone_call_count())) {
    printf("  step %lu\n", qa_step);
  }
  slog_checked = fake_sidetone_call_count();
  if (slog_checked > QA_LOG_RESET_LEVEL) {
    TEST_ASSERT(!fake_sidetone_is_log_overflowed());
    fake_sidetone_reset();
    slog_checked = 0U;
  }
}

static void qa_model_start(uint16_t now) {
  (void)qa_expect_call(FAKE_SIDETONE_CALL_TONE, preview.tone_hz);
  (void)qa_expect_call(FAKE_SIDETONE_CALL_ON, 1U);
  preview.is_running = true;
  preview.start = now;
}

static void qa_model_stop(void) {
  (void)qa_expect_call(FAKE_SIDETONE_CALL_ON, 0U);
  preview.is_running = false;
}

/* menu_poll() at @p now: one tick read while running, none while stopped; stop at >= MENU_TONE_PREVIEW_MS. */
static void qa_poll(uint16_t now) {
  const size_t reads = fake_tick_read_count();
  const bool was_running = preview.is_running;
  fake_tick_set_now(now);
  menu_poll();
  TEST_ASSERT_EQUAL(reads + (was_running ? 1U : 0U), fake_tick_read_count());
  if (was_running && ((uint16_t)(now - preview.start) >= MENU_TONE_PREVIEW_MS)) {
    qa_model_stop();
  }
  qa_expect_end();
}

static void qa_event(uint16_t now, button_event_t event) {
  const uint8_t code = (uint8_t)(event & (uint8_t)~BUTTON_EVENT_REPEAT_FLAG);
  fake_tick_set_now(now);
  menu_handle(event);
  if (menu_needs_redraw()) {
    menu_render();
  }
  if (preview.is_message) {
    if (code == BUTTON_EVENT_ENTER || code == BUTTON_EVENT_MENU) {
      preview.is_message = false;
    }
  } else if (!preview.is_edit) {
    if (code == BUTTON_EVENT_ENTER) {
      preview.is_edit = true;
      qa_model_start(now);
    }
  } else if ((code == BUTTON_EVENT_ENTER) || (code == BUTTON_EVENT_MENU)) {
    preview.is_edit = false;
    qa_model_stop();
  } else if ((code == BUTTON_EVENT_NEXT) && (preview.tone_hz < 1000U)) {
    preview.tone_hz = (uint16_t)(preview.tone_hz + 10U);
    qa_model_start(now);
  } else if ((code == BUTTON_EVENT_PREV) && (preview.tone_hz > 400U)) {
    preview.tone_hz = (uint16_t)(preview.tone_hz - 10U);
    qa_model_start(now);
  }
  qa_expect_end();
  TEST_ASSERT_EQUAL(preview.is_edit ? MENU_SCREEN_EDIT : (preview.is_message ? MENU_SCREEN_MESSAGE : MENU_SCREEN_LIST),
                    menu_screen());
  TEST_ASSERT_EQUAL(preview.tone_hz, settings_get(SETTINGS_ID_TONE_HZ));
}

static void qa_message(uint16_t now) {
  fake_tick_set_now(now);
  menu_show_message(MENU_MESSAGE_NOT_SAVED);
  if (preview.is_edit) {
    preview.is_edit = false;
    qa_model_stop();
  }
  preview.is_message = true;
  qa_expect_end();
}

/* H4: the stop comes at exactly start + MENU_TONE_PREVIEW_MS for each start tick, also across the wrap. */
static void test_timeout_at_every_start_tick(void) {
  uint32_t start;

  qa_cw_tone(600U);
  for (start = 0U; start <= 65535U; start += 1U) {
    const uint16_t s = (uint16_t)start;
    qa_step = start;
    qa_event(s, BUTTON_EVENT_ENTER);
    qa_poll((uint16_t)(s + 1U));
    qa_poll((uint16_t)(s + MENU_TONE_PREVIEW_MS - 1U));
    TEST_ASSERT(preview.is_running);
    qa_poll((uint16_t)(s + MENU_TONE_PREVIEW_MS));
    TEST_ASSERT(!preview.is_running);
    qa_poll((uint16_t)(s + MENU_TONE_PREVIEW_MS + 1U));
    qa_event((uint16_t)(s + MENU_TONE_PREVIEW_MS + 2U), BUTTON_EVENT_ENTER);
  }

  /* A restart near the wrap moves the stop to 300 ms after the restart. */
  for (start = 65000U; start <= 65535U; start += 1U) {
    const uint16_t s = (uint16_t)start;
    qa_step = start;
    qa_event(s, BUTTON_EVENT_ENTER);
    qa_event((uint16_t)(s + 250U), BUTTON_EVENT_NEXT);
    qa_poll((uint16_t)(s + MENU_TONE_PREVIEW_MS));
    qa_poll((uint16_t)(s + 250U + MENU_TONE_PREVIEW_MS - 1U));
    TEST_ASSERT(preview.is_running);
    qa_poll((uint16_t)(s + 250U + MENU_TONE_PREVIEW_MS));
    TEST_ASSERT(!preview.is_running);
    qa_event((uint16_t)(s + 600U), BUTTON_EVENT_PREV);
    qa_event((uint16_t)(s + 601U), BUTTON_EVENT_MENU);
    qa_event((uint16_t)(s + 602U), BUTTON_EVENT_ENTER);
    qa_event((uint16_t)(s + 603U), BUTTON_EVENT_ENTER);
  }
}

static uint32_t qa_random_state = 0xC0FFEEUL;

static uint32_t qa_random(uint32_t limit) {
  qa_random_state = qa_random_state * 1103515245UL + 12345UL;
  return (qa_random_state >> 8) % limit;
}

/* Random presses, repeats, polls and messages in the Tone row; the model predicts every sidetone call. */
static void test_random_preview_sequences(void) {
  static const uint16_t start_tones[4] = {400U, 600U, 990U, 1000U};
  static const button_event_t events[8] = {
      BUTTON_EVENT_NEXT,  BUTTON_EVENT_PREV, REPEAT(BUTTON_EVENT_NEXT), REPEAT(BUTTON_EVENT_PREV),
      BUTTON_EVENT_ENTER, BUTTON_EVENT_MENU, BUTTON_EVENT_NEXT,         REPEAT(BUTTON_EVENT_NEXT)};
  uint16_t now = 60000U;
  size_t round;

  for (round = 0U; round < 4U; ++round) {
    unsigned long step;
    qa_cw_tone(start_tones[round]);
    for (step = 0U; step < 60000UL; ++step) {
      const uint32_t action = qa_random(100U);
      qa_step = step;
      now = (uint16_t)(now + qa_random((qa_random(10U) == 0U) ? 700U : 90U));
      if (action < 45U) {
        qa_poll(now);
      } else if (action < 47U) {
        qa_message(now);
      } else if (!preview.is_edit) {
        qa_event(now, BUTTON_EVENT_ENTER); /* opens the edit, or closes the message */
      } else {
        qa_event(now, events[qa_random(8U)]);
      }
    }
  }
  TEST_ASSERT(!fake_sidetone_is_log_overflowed());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_poll_count());
  TEST_ASSERT_EQUAL(0U, fake_sidetone_service_count());
}

int main(void) {
  TEST_RUN(test_timeout_at_every_start_tick);
  TEST_RUN(test_random_preview_sequences);
  return TEST_FINISH();
}
