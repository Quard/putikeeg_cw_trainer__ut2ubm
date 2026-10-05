/*
 * Host tests of the Free Input mode with a fake keyer (references/internal/keyer_spec.md section 6.11, AC-K42 to
 * AC-K44; references/internal/ui-ux.md section 5.9).
 */

#include <string.h>

#include "board/button_pins.h"
#include "buttons/buttons.h"
#include "cw/keyer.h"
#include "cw/keyer_shared.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_keyer.h"
#include "fake_sidetone.h"
#include "fake_tick.h"
#include "gfx/gfx.h"
#include "lcd/lcd_fb.h"
#include "modes/free_input.h"
#include "settings/settings.h"
#include "support/free_input_screen.h"
#include "support/test_assert.h"

static uint16_t now;

static void start(void) {
  fake_iap_reset();
  (void)settings_load();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_delay_reset();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  now = 2000U;
  gfx_init();
  lcd_fb_clear();
  fake_keyer_reset();
  fake_sidetone_reset();
  free_input_enter();
  /* The menu draws the title bar; a full page 0 shows any write to it. */
  memset(lcd_fb_page(0U), 0xFF, LCD_WIDTH);
  lcd_fb_clear_dirty(0U);
}

static void poll_all(void) {
  size_t guard;
  for (guard = 0U; (guard < 1000U) && (fake_keyer_pending_count() != 0U); ++guard) {
    free_input_poll();
  }
}

static void push_pattern(const char* code) {
  for (; *code != '\0'; ++code) {
    fake_keyer_push_event((*code == '-') ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
}

/* KEY(s) of ui-ux.md section 9.2: a space is one WORD_GAP. */
static void key(const char* text) {
  for (; *text != '\0'; ++text) {
    if (*text == ' ') {
      fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
    } else {
      push_pattern(morse_code_of(*text));
      fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
    }
    poll_all();
  }
}

static void keyp(const char* code) {
  push_pattern(code);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  poll_all();
}

static void key_times(char character, unsigned count) {
  char text[2] = {character, '\0'};
  unsigned i;
  for (i = 0U; i < count; ++i) {
    key(text);
  }
}

static void word_gap(void) {
  fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
  poll_all();
}

static bool is_page0_untouched(void) {
  size_t x;
  const uint8_t* page = lcd_fb_page(0U);
  for (x = 0U; x < LCD_WIDTH; ++x) {
    if (page[x] != 0xFFU) {
      return false;
    }
  }
  return (lcd_fb_dirty_mask() & 0x01U) == 0U;
}

/* AC-K42: cursor rules, word spaces and the lazy wrap. */
static void test_text_and_word_spaces(void) {
  char line[26];

  start();
  key("CQ DE");
  {
    const char* const rows[7] = {"CQ DE", NULL, NULL, NULL, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }

  start();
  word_gap();
  key("E");
  TEST_ASSERT(row_is(1, "E"));

  /* A full line stays the last line until the next character comes. */
  start();
  key_times('E', 25U);
  TEST_ASSERT(row_is(1, full_line('E', line)));
  TEST_ASSERT(row_is(2, NULL));
  word_gap();
  key("T");
  {
    const char* const rows[7] = {full_line('E', line), "T", NULL, NULL, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }

  start();
  key_times('E', 24U);
  word_gap();
  key("T");
  {
    const char* const rows[7] = {"EEEEEEEEEEEEEEEEEEEEEEEE", "T", NULL, NULL, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }

  /* An empty CHAR_GAP and a second word space draw nothing and keep the cursor. */
  start();
  key("A");
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  word_gap();
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  poll_all();
  key("B");
  TEST_ASSERT(row_is(1, "A B"));
  TEST_ASSERT(is_page0_untouched());
}

/* AC-K42: '*' for an unknown pattern, an invalid pattern and an overflow. */
static void test_unknown_glyph(void) {
  start();
  keyp("........");
  keyp(".......");
  keyp("..--");
  fake_keyer_push_event(KEYER_EVENT_DIT);
  fake_keyer_push_event(KEYER_EVENT_OVERFLOW);
  keyp(".-");
  TEST_ASSERT(row_is(1, "****"));
}

/* AC-K42: <SK> takes four cells and can split across a line end. */
static void test_sk_token(void) {
  char line[26];

  start();
  keyp(".-.-.");
  keyp("-...-");
  keyp("...-.-");
  TEST_ASSERT(row_is(1, "+=<SK>"));

  start();
  key_times('E', 23U);
  keyp("...-.-");
  TEST_ASSERT(row_is(1, "EEEEEEEEEEEEEEEEEEEEEEE<S"));
  TEST_ASSERT(row_is(2, "K>"));

  start();
  key_times('E', 25U);
  word_gap();
  keyp("...-.-");
  TEST_ASSERT(row_is(1, full_line('E', line)));
  TEST_ASSERT(row_is(2, "<SK>"));
}

static void key_seven_lines(void) {
  char letter;
  for (letter = 'A'; letter <= 'G'; ++letter) {
    key_times(letter, 25U);
  }
}

/* AC-K43: a scroll copies pages 1-6 and clears page 7; page 0 is never written or dirty. */
static void test_scroll(void) {
  char lines[7][26];
  int r;

  start();
  key_seven_lines();
  {
    const char* const rows[7] = {full_line('A', lines[0]), full_line('B', lines[1]), full_line('C', lines[2]),
                                 full_line('D', lines[3]), full_line('E', lines[4]), full_line('F', lines[5]),
                                 full_line('G', lines[6])};
    TEST_ASSERT(text_area_is(rows));
  }
  for (r = 1; r < 8; ++r) {
    lcd_fb_clear_dirty((uint8_t)r);
  }
  key("H");
  {
    const char* const rows[7] = {full_line('B', lines[0]),
                                 full_line('C', lines[1]),
                                 full_line('D', lines[2]),
                                 full_line('E', lines[3]),
                                 full_line('F', lines[4]),
                                 full_line('G', lines[5]),
                                 "H"};
    TEST_ASSERT(text_area_is(rows));
  }
  TEST_ASSERT_EQUAL(0xFEU, lcd_fb_dirty_mask());
  TEST_ASSERT(is_page0_untouched());
}

/* AC-K43: the clear drops the character in progress; the keyer and the sidetone do not change. */
static void test_clear(void) {
  size_t sidetone_calls;
  uint8_t page;

  start();
  key("ABC");
  fake_keyer_push_event(KEYER_EVENT_DIT);
  fake_keyer_push_event(KEYER_EVENT_DIT);
  poll_all();
  sidetone_calls = fake_sidetone_call_count();
  free_input_clear();
  TEST_ASSERT(text_area_is_blank());
  TEST_ASSERT(is_page0_untouched());
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(0U, fake_keyer_stop_count());
  TEST_ASSERT_EQUAL(sidetone_calls, fake_sidetone_call_count());
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  poll_all();
  TEST_ASSERT(row_is(1, "T"));

  /* Clear on an empty area changes no page. */
  start();
  for (page = 1U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
  free_input_clear();
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  word_gap();
  key("A");
  TEST_ASSERT(row_is(1, "A"));
}

/* AC-K43: delimiter rules. */
static void test_delimiter(void) {
  char lines[7][26];

  start();
  key("AB");
  free_input_delimiter();
  key("C");
  {
    const char* const rows[7] = {"AB", SCREEN_LINE_ROW, "C", NULL, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }

  start();
  free_input_delimiter();
  key("C");
  free_input_delimiter();
  free_input_delimiter();
  {
    const char* const rows[7] = {SCREEN_LINE_ROW, "C", SCREEN_LINE_ROW, SCREEN_LINE_ROW, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }

  /* The delimiter keeps the character in progress. */
  start();
  fake_keyer_push_event(KEYER_EVENT_DIT);
  poll_all();
  free_input_delimiter();
  fake_keyer_push_event(KEYER_EVENT_DAH);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  poll_all();
  {
    const char* const rows[7] = {SCREEN_LINE_ROW, "A", NULL, NULL, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }

  /* On row 7 the delimiter scrolls, and it scrolls with the text. */
  start();
  key_seven_lines();
  free_input_delimiter();
  {
    const char* const rows[7] = {full_line('B', lines[0]), full_line('C', lines[1]), full_line('D', lines[2]),
                                 full_line('E', lines[3]), full_line('F', lines[4]), full_line('G', lines[5]),
                                 SCREEN_LINE_ROW};
    TEST_ASSERT(text_area_is(rows));
  }
  key("H");
  {
    const char* const rows[7] = {full_line('C', lines[0]),
                                 full_line('D', lines[1]),
                                 full_line('E', lines[2]),
                                 full_line('F', lines[3]),
                                 full_line('G', lines[4]),
                                 SCREEN_LINE_ROW,
                                 "H"};
    TEST_ASSERT(text_area_is(rows));
  }
  TEST_ASSERT(is_page0_untouched());
}

/* AC-K44: enter call order and arguments. */
static size_t sidetone_calls_at_start;
static uint16_t tone_at_start;
static size_t pin_reads_at_stop;

static void on_keyer_start(void) {
  sidetone_calls_at_start = fake_sidetone_call_count();
  tone_at_start = fake_sidetone_last_tone();
}

static void on_keyer_stop(void) { pin_reads_at_stop = fake_button_pins_read_count(); }

static void test_enter_and_leave_calls(void) {
  size_t reads;
  button_event_t event;
  size_t guard;
  bool has_enter = false;

  fake_iap_reset();
  (void)settings_load();
  TEST_ASSERT(settings_set(SETTINGS_ID_KEY_TYPE, SETTINGS_KEY_STRAIGHT));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TX_WPM, 25U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TX_EFF_WPM, 15U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TONE_HZ, 750U));
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  lcd_fb_clear();
  for (guard = 0U; guard < LCD_PAGE_COUNT; ++guard) {
    lcd_fb_clear_dirty((uint8_t)guard);
  }
  fake_keyer_reset();
  fake_sidetone_reset();
  fake_keyer_set_start_hook(on_keyer_start);
  fake_keyer_set_stop_hook(on_keyer_stop);

  free_input_enter();
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(SETTINGS_KEY_STRAIGHT, fake_keyer_last_key_type());
  TEST_ASSERT(fake_keyer_last_is_swapped());
  TEST_ASSERT_EQUAL(25U, fake_keyer_last_wpm());
  TEST_ASSERT_EQUAL(15U, fake_keyer_last_eff_wpm());
  TEST_ASSERT_EQUAL(1U, sidetone_calls_at_start);
  TEST_ASSERT_EQUAL(750U, tone_at_start);
  TEST_ASSERT_EQUAL(1U, fake_sidetone_call_count());
  TEST_ASSERT_EQUAL(0U, fake_keyer_stop_count());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());

  /* The menu enter is off: a P3.2 press gives no event. */
  now = 3000U;
  fake_button_pins_set(BUTTON_BIT_ENTER);
  for (guard = 0U; guard < 200U; ++guard) {
    fake_tick_set_now((uint16_t)(now + guard));
    event = buttons_poll();
    has_enter = has_enter || (event == BUTTON_EVENT_ENTER);
  }
  fake_button_pins_set(0U);
  for (guard = 200U; guard < 300U; ++guard) {
    fake_tick_set_now((uint16_t)(now + guard));
    (void)buttons_poll();
  }
  TEST_ASSERT(!has_enter);

  /* Leave: the keyer stops before the menu gets P3.2 back, which reads the pins once. */
  reads = fake_button_pins_read_count();
  free_input_leave();
  TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
  TEST_ASSERT(!fake_keyer_is_running());
  TEST_ASSERT_EQUAL(reads, pin_reads_at_stop);
  TEST_ASSERT_EQUAL(reads + 1U, fake_button_pins_read_count());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  fake_button_pins_set(BUTTON_BIT_ENTER);
  has_enter = false;
  for (guard = 300U; guard < 400U; ++guard) {
    fake_tick_set_now((uint16_t)(now + guard));
    has_enter = has_enter || (buttons_poll() == BUTTON_EVENT_ENTER);
  }
  TEST_ASSERT(has_enter);
  fake_keyer_set_start_hook(NULL);
  fake_keyer_set_stop_hook(NULL);
}

/* AC-K44: at most KEYER_QUEUE_SIZE + 1 events per poll. */
static void test_events_per_poll(void) {
  unsigned i;

  start();
  for (i = 0U; i < 20U; ++i) {
    fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  }
  free_input_poll();
  TEST_ASSERT_EQUAL(KEYER_QUEUE_SIZE + 1U, fake_keyer_read_count());
  TEST_ASSERT_EQUAL(20U - (KEYER_QUEUE_SIZE + 1U), fake_keyer_pending_count());
  free_input_poll();
  free_input_poll();
  TEST_ASSERT_EQUAL(0U, fake_keyer_pending_count());
  /* An empty queue stops the poll at the first NONE. */
  free_input_poll();
  TEST_ASSERT_EQUAL(21U + 1U, fake_keyer_read_count());
  TEST_ASSERT(!fake_keyer_is_overflowed());
}

int main(void) {
  TEST_RUN(test_text_and_word_spaces);
  TEST_RUN(test_unknown_glyph);
  TEST_RUN(test_sk_token);
  TEST_RUN(test_scroll);
  TEST_RUN(test_clear);
  TEST_RUN(test_delimiter);
  TEST_RUN(test_enter_and_leave_calls);
  TEST_RUN(test_events_per_poll);
  return TEST_FINISH();
}
