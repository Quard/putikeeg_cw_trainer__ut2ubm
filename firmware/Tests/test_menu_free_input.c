/*
 * Menu scenarios V-54 to V-73, V-79, V-88 and V-89 of references/internal/ui-ux.md sections 9.2 and 9.3 (AC-K51,
 * AC-P17, AC-P20), with the real menu,
 * buttons and Free Input, a fake keyer and a fake sidetone. A long press is measured on the debounced state: the
 * hold time is the time from the press event to the debounced release.
 */

#include <string.h>

#include "backlight/backlight.h"
#include "board/button_pins.h"
#include "board/buttons_config.h"
#include "buttons/buttons.h"
#include "cw/keyer.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_keyer.h"
#include "fake_menu_outputs.h"
#include "fake_sidetone.h"
#include "fake_tick.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "menu/menu.h"
#include "settings/settings.h"
#include "support/free_input_screen.h"
#include "support/test_assert.h"

#define LOG_CAPACITY 4096U
#define MAIN_EVENTS_PER_PASS 4U
#define NEXT_PIN BUTTON_BIT_NEXT
#define PREV_PIN BUTTON_BIT_PREV
#define MENU_PIN BUTTON_BIT_MENU
#define ENTER_PIN BUTTON_BIT_ENTER

static uint16_t now;
static uint8_t pins;
static button_event_t event_log[LOG_CAPACITY];
static uint16_t event_times[LOG_CAPACITY];
static size_t event_count;
/* Redraws since the last reset_counters(). */
static size_t render_count;
/* The prosign style of the record that reset_eeprom() writes (V-89). */
static uint8_t record_style = SETTINGS_PROSIGN_BOTH;

/* Layout version 2 records of keyer_spec.md section 17.4.2, by SETTINGS_PROSIGN_*. */
static const uint8_t style_records[3][SETTINGS_RECORD_SIZE] = {
    {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0xB3U},
    {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x05U, 0x05U, 0x08U, 0x20U, 0xEBU},
    {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x03U},
};

/* An empty EEPROM for Both, else the record of record_style. */
static void reset_eeprom(void) {
  fake_iap_reset();
  if (record_style != SETTINGS_PROSIGN_BOTH) {
    memcpy(fake_iap_memory(), style_records[record_style], SETTINGS_RECORD_SIZE);
  }
}

static void power_on(void) {
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  fake_keyer_reset();
  fake_sidetone_reset();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_delay_reset();
  lcd_fb_clear();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  now = 2000U;
  pins = 0U;
  (void)settings_load();
  fake_iap_clear_counters();
  lcd_set_contrast((uint8_t)settings_get(SETTINGS_ID_CONTRAST));
  backlight_init((uint8_t)settings_get(SETTINGS_ID_BACKLIGHT));
  gfx_init();
  menu_init();
  menu_render();
  event_count = 0U;
  render_count = 0U;
}

/* One main-loop pass per ms, as main.c runs it, with @p mask pressed. */
static void run(uint8_t mask, uint32_t duration) {
  uint32_t i;
  unsigned events;
  button_event_t event;

  pins = mask;
  fake_button_pins_set(mask);
  for (i = 0U; i < duration; ++i) {
    fake_tick_set_now(now);
    for (events = 0U; events < MAIN_EVENTS_PER_PASS; ++events) {
      event = buttons_poll();
      if (event == BUTTON_EVENT_NONE) {
        break;
      }
      if (event_count < LOG_CAPACITY) {
        event_log[event_count] = event;
        event_times[event_count] = now;
      }
      ++event_count;
      menu_handle(event);
    }
    menu_poll();
    if (menu_needs_redraw()) {
      menu_render();
      ++render_count;
    }
    now = (uint16_t)(now + 1U);
  }
}

static size_t count_events(button_event_t event) {
  size_t count = 0U;
  size_t i;
  for (i = 0U; (i < event_count) && (i < LOG_CAPACITY); ++i) {
    if (event_log[i] == event) {
      ++count;
    }
  }
  return count;
}

/* A short press: low until the press event, then released through the debounce. */
static void press(uint8_t mask) {
  run((uint8_t)(pins | mask), BUTTONS_DEBOUNCE_MS + 5U);
  run((uint8_t)(pins & (uint8_t)~mask), BUTTONS_DEBOUNCE_MS + 5U);
}

/*
 * Holds @p mask debounced-pressed for @p pressed_ms after its press event, then releases it. Both edges lag the pin
 * by one debounce time, so the pin stays low for @p pressed_ms.
 */
static void hold(uint8_t mask, uint32_t pressed_ms) {
  run((uint8_t)(pins | mask), pressed_ms);
  run((uint8_t)(pins & (uint8_t)~mask), BUTTONS_DEBOUNCE_MS + 10U);
}

/* LONG(X): hold until the long event comes, then release. */
static void long_press(uint8_t mask) { hold(mask, BUTTONS_LONG_PRESS_MS + 5U); }

static void pump(void) { run(pins, 5U); }

static void push_pattern(const char* code) {
  for (; *code != '\0'; ++code) {
    fake_keyer_push_event((*code == '-') ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
}

static void key(const char* text) {
  for (; *text != '\0'; ++text) {
    if (*text == ' ') {
      fake_keyer_push_event(KEYER_EVENT_WORD_GAP);
    } else {
      push_pattern(morse_code_of(*text));
      fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
    }
    pump();
  }
}

static void keyp(const char* code) {
  push_pattern(code);
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  pump();
}

static void key_times(char character, unsigned count) {
  char text[2] = {character, '\0'};
  unsigned i;
  for (i = 0U; i < count; ++i) {
    key(text);
  }
}

static void single(keyer_event_t event) {
  fake_keyer_push_event(event);
  pump();
}

/* FI: power-on with an empty EEPROM (V-89: the record of record_style), cursor to Free Input, press enter. */
static void open_free_input(void) {
  reset_eeprom();
  power_on();
  press(NEXT_PIN);
  press(NEXT_PIN);
  press(ENTER_PIN);
}

static bool title_is(const char* title) {
  const int length = (int)strlen(title);
  int x;
  int y;
  for (y = 0; y < 8; ++y) {
    for (x = 0; x < 128; ++x) {
      bool expected = true;
      if ((x >= 2) && (x < 2 + 5 * length)) {
        expected = !screen_golden_ink(title[(x - 2) / 5], (x - 2) % 5, y);
      }
      if (screen_pixel(x, y) != expected) {
        fprintf(stderr, "title: pixel (%d, %d) is %d\n", x, y, (int)screen_pixel(x, y));
        return false;
      }
    }
  }
  return true;
}

static bool probe_sel(int r) {
  int x;
  int y;
  for (y = 8 * r + 1; y <= 8 * r + 7; ++y) {
    if (!screen_pixel(0, y) || !screen_pixel(1, y) || !screen_pixel(127, y)) {
      return false;
    }
  }
  for (x = 0; x < 128; ++x) {
    if (screen_pixel(x, 8 * r)) {
      return false;
    }
  }
  return true;
}

static bool is_free_input_screen(void) { return (menu_screen() == MENU_SCREEN_FREE_INPUT) && title_is("Free Input"); }

static bool is_main_menu_on_free_input(void) {
  return (menu_screen() == MENU_SCREEN_LIST) && (menu_node() == MENU_NODE_MAIN) && (menu_cursor() == 2U) &&
         title_is("CW Trainer") && probe_sel(3);
}

static bool is_lcd_untouched(const uint8_t* before) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    if (memcmp(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH) != 0) {
      return false;
    }
  }
  return lcd_fb_dirty_mask() == 0U;
}

static void snapshot(uint8_t* before) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(&before[page * LCD_WIDTH], lcd_fb_page(page), LCD_WIDTH);
    lcd_fb_clear_dirty(page);
  }
}

static void test_v54_open(void) {
  open_free_input();
  TEST_ASSERT_EQUAL(record_style, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  TEST_ASSERT(is_free_input_screen());
  TEST_ASSERT(text_area_is_blank());
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(SETTINGS_KEY_IAMBIC_B, fake_keyer_last_key_type());
  TEST_ASSERT(!fake_keyer_last_is_swapped());
  TEST_ASSERT_EQUAL(20U, fake_keyer_last_wpm());
  TEST_ASSERT_EQUAL(20U, fake_keyer_last_eff_wpm());
  TEST_ASSERT(fake_keyer_is_running());
  /* The menu enter is off. */
  event_count = 0U;
  press(ENTER_PIN);
  TEST_ASSERT_EQUAL(0U, event_count);
}

static void test_v55_closed_contact_at_start(void) {
  uint8_t before[LCD_PAGE_COUNT * LCD_WIDTH];

  reset_eeprom();
  power_on();
  press(NEXT_PIN);
  press(NEXT_PIN);
  run(ENTER_PIN, BUTTONS_DEBOUNCE_MS + 5U);
  TEST_ASSERT(is_free_input_screen());
  snapshot(before);
  event_count = 0U;
  fake_sidetone_reset();
  run(ENTER_PIN, 2000U);
  run(0U, 100U);
  TEST_ASSERT_EQUAL(0U, count_events(BUTTON_EVENT_ENTER));
  TEST_ASSERT_EQUAL(0U, fake_sidetone_call_count());
  TEST_ASSERT(is_lcd_untouched(before));
  TEST_ASSERT(is_free_input_screen());
}

static void test_v56_text(void) {
  const char* const rows[7] = {"CQ DE", NULL, NULL, NULL, NULL, NULL, NULL};
  open_free_input();
  key("CQ DE");
  TEST_ASSERT(text_area_is(rows));
}

static void test_v57_no_space_at_line_start(void) {
  open_free_input();
  single(KEYER_EVENT_WORD_GAP);
  key("E");
  TEST_ASSERT(row_is(1, "E"));
  TEST_ASSERT(row_is(2, NULL));
}

static void test_v58_v59_space_at_line_end(void) {
  char line[26];

  open_free_input();
  key_times('E', 25U);
  single(KEYER_EVENT_WORD_GAP);
  key("T");
  TEST_ASSERT(row_is(1, full_line('E', line)));
  TEST_ASSERT(row_is(2, "T"));

  open_free_input();
  key_times('E', 24U);
  single(KEYER_EVENT_WORD_GAP);
  key("T");
  TEST_ASSERT(row_is(1, "EEEEEEEEEEEEEEEEEEEEEEEE"));
  TEST_ASSERT(row_is(2, "T"));
}

static void key_seven_lines(void) {
  char letter;
  for (letter = 'A'; letter <= 'G'; ++letter) {
    key_times(letter, 25U);
  }
}

static void test_v60_scroll(void) {
  char lines[7][26];
  uint8_t page;

  open_free_input();
  key_seven_lines();
  {
    const char* const rows[7] = {full_line('A', lines[0]), full_line('B', lines[1]), full_line('C', lines[2]),
                                 full_line('D', lines[3]), full_line('E', lines[4]), full_line('F', lines[5]),
                                 full_line('G', lines[6])};
    TEST_ASSERT(text_area_is(rows));
  }
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
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
  TEST_ASSERT(title_is("Free Input"));
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask() & 0x01U);
}

static void test_v61_unknown(void) {
  open_free_input();
  keyp("........");
  keyp(".......");
  keyp("..--");
  fake_keyer_push_event(KEYER_EVENT_OVERFLOW);
  keyp(".-");
  TEST_ASSERT(row_is(1, "****"));
}

/* V-62: the record sets Prosigns = Punctuation. */
static void test_v62_prosigns(void) {
  record_style = SETTINGS_PROSIGN_PUNCTUATION;
  open_free_input();
  record_style = SETTINGS_PROSIGN_BOTH;
  keyp(".-.-.");
  keyp("-...-");
  keyp("...-.-");
  TEST_ASSERT(row_is(1, "+=<SK>"));
}

static void test_v63_sk(void) {
  char line[26];

  open_free_input();
  key_times('E', 23U);
  keyp("...-.-");
  TEST_ASSERT(row_is(1, "EEEEEEEEEEEEEEEEEEEEEEE<S"));
  TEST_ASSERT(row_is(2, "K>"));

  open_free_input();
  key_times('E', 25U);
  single(KEYER_EVENT_WORD_GAP);
  keyp("...-.-");
  TEST_ASSERT(row_is(1, full_line('E', line)));
  TEST_ASSERT(row_is(2, "<SK>"));
}

static void test_v64_v65_clear(void) {
  uint8_t page;

  open_free_input();
  key("ABC");
  single(KEYER_EVENT_DIT);
  single(KEYER_EVENT_DIT);
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
  long_press(PREV_PIN);
  TEST_ASSERT(text_area_is_blank());
  TEST_ASSERT(title_is("Free Input"));
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask() & 0x01U);
  single(KEYER_EVENT_DAH);
  single(KEYER_EVENT_CHAR_GAP);
  TEST_ASSERT(row_is(1, "T"));
  TEST_ASSERT_EQUAL(0U, fake_keyer_stop_count());

  open_free_input();
  long_press(PREV_PIN);
  single(KEYER_EVENT_CHAR_GAP);
  single(KEYER_EVENT_WORD_GAP);
  TEST_ASSERT(text_area_is_blank());
  key("A");
  TEST_ASSERT(row_is(1, "A"));
}

static void test_v66_to_v69_delimiter(void) {
  char lines[7][26];

  open_free_input();
  key("AB");
  long_press(NEXT_PIN);
  {
    const char* const rows[7] = {"AB", SCREEN_LINE_ROW, NULL, NULL, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }
  key("C");
  {
    const char* const rows[7] = {"AB", SCREEN_LINE_ROW, "C", NULL, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }

  open_free_input();
  long_press(NEXT_PIN);
  key("C");
  long_press(NEXT_PIN);
  long_press(NEXT_PIN);
  {
    const char* const rows[7] = {SCREEN_LINE_ROW, "C", SCREEN_LINE_ROW, SCREEN_LINE_ROW, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }

  open_free_input();
  key_seven_lines();
  long_press(NEXT_PIN);
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

  open_free_input();
  single(KEYER_EVENT_DIT);
  long_press(NEXT_PIN);
  single(KEYER_EVENT_DAH);
  single(KEYER_EVENT_CHAR_GAP);
  {
    const char* const rows[7] = {SCREEN_LINE_ROW, "A", NULL, NULL, NULL, NULL, NULL};
    TEST_ASSERT(text_area_is(rows));
  }
}

static void test_v70_short_presses_do_nothing(void) {
  uint8_t before[LCD_PAGE_COUNT * LCD_WIDTH];

  open_free_input();
  key("AB");
  snapshot(before);
  render_count = 0U;
  event_count = 0U;
  press(PREV_PIN);
  press(NEXT_PIN);
  press(MENU_PIN);
  run(ENTER_PIN, 100U);
  run(0U, 50U);
  hold(PREV_PIN, 600U);
  hold(NEXT_PIN, 600U);
  TEST_ASSERT_EQUAL(0U, render_count);
  TEST_ASSERT(is_lcd_untouched(before));
  TEST_ASSERT(is_free_input_screen());
  TEST_ASSERT_EQUAL(0U, fake_keyer_stop_count());
  TEST_ASSERT(fake_keyer_is_running());
  TEST_ASSERT_EQUAL(0U, count_events(BUTTON_EVENT_ENTER));
}

static void test_v71_exit_and_reopen(void) {
  open_free_input();
  key("AB");
  long_press(MENU_PIN);
  TEST_ASSERT(is_main_menu_on_free_input());
  TEST_ASSERT_EQUAL(1U, fake_keyer_stop_count());
  TEST_ASSERT(!fake_keyer_is_running());
  event_count = 0U;
  press(ENTER_PIN);
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_ENTER));
  TEST_ASSERT(is_free_input_screen());
  TEST_ASSERT(text_area_is_blank());
  TEST_ASSERT_EQUAL(2U, fake_keyer_start_count());
}

static void test_v72_held_contact_at_exit(void) {
  open_free_input();
  run(ENTER_PIN, 50U);
  long_press(MENU_PIN);
  TEST_ASSERT(is_main_menu_on_free_input());
  event_count = 0U;
  run(ENTER_PIN, 2000U);
  TEST_ASSERT_EQUAL(0U, count_events(BUTTON_EVENT_ENTER));
  TEST_ASSERT(is_main_menu_on_free_input());
  run(0U, 50U);
  TEST_ASSERT_EQUAL(0U, count_events(BUTTON_EVENT_ENTER));
  press(ENTER_PIN);
  TEST_ASSERT_EQUAL(1U, count_events(BUTTON_EVENT_ENTER));
  TEST_ASSERT(is_free_input_screen());
}

static void test_v73_long_threshold(void) {
  uint16_t press_time;
  size_t events_after_exit;

  open_free_input();
  hold(MENU_PIN, BUTTONS_LONG_PRESS_MS - 10U);
  TEST_ASSERT(is_free_input_screen());
  TEST_ASSERT_EQUAL(0U, fake_keyer_stop_count());

  event_count = 0U;
  run(MENU_PIN, BUTTONS_DEBOUNCE_MS);
  press_time = now;
  /* Passes at press_time .. press_time + 799, then the pass at press_time + 800. */
  run(MENU_PIN, BUTTONS_LONG_PRESS_MS);
  TEST_ASSERT(is_free_input_screen());
  run(MENU_PIN, 1U);
  TEST_ASSERT(is_main_menu_on_free_input());
  TEST_ASSERT_EQUAL((uint16_t)(press_time + BUTTONS_LONG_PRESS_MS), event_times[event_count - 1U]);
  run(MENU_PIN, 10U);
  run(0U, 30U);

  /* A 5 s hold exits once; the rest of the hold and the release give the menu nothing. */
  press(ENTER_PIN);
  TEST_ASSERT(is_free_input_screen());
  event_count = 0U;
  run(MENU_PIN, 5000U);
  TEST_ASSERT(is_main_menu_on_free_input());
  events_after_exit = event_count;
  run(0U, 100U);
  TEST_ASSERT_EQUAL(2U, events_after_exit);
  TEST_ASSERT_EQUAL(2U, event_count);
  TEST_ASSERT_EQUAL(1U, count_events((button_event_t)(BUTTON_EVENT_MENU | BUTTON_EVENT_LONG_FLAG)));
  TEST_ASSERT(is_main_menu_on_free_input());
}

static void test_v79_keyer_settings(void) {
  fake_iap_reset();
  power_on();
  TEST_ASSERT(settings_set(SETTINGS_ID_KEY_TYPE, SETTINGS_KEY_STRAIGHT));
  TEST_ASSERT(settings_set(SETTINGS_ID_PADDLE_SWAP, 1U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TX_WPM, 25U));
  TEST_ASSERT(settings_set(SETTINGS_ID_TX_EFF_WPM, 15U));
  press(NEXT_PIN);
  press(NEXT_PIN);
  press(ENTER_PIN);
  TEST_ASSERT(is_free_input_screen());
  TEST_ASSERT_EQUAL(1U, fake_keyer_start_count());
  TEST_ASSERT_EQUAL(SETTINGS_KEY_STRAIGHT, fake_keyer_last_key_type());
  TEST_ASSERT(fake_keyer_last_is_swapped());
  TEST_ASSERT_EQUAL(25U, fake_keyer_last_wpm());
  TEST_ASSERT_EQUAL(15U, fake_keyer_last_eff_wpm());
}

/* V-88 (AC-P17): the style is read once at each Free Input start. */
static void test_v88_style_read_at_start(void) {
  open_free_input();
  keyp(".-.-.");
  TEST_ASSERT(settings_set(SETTINGS_ID_PROSIGN_STYLE, SETTINGS_PROSIGN_PUNCTUATION));
  keyp(".-.-.");
  TEST_ASSERT(row_is(1, "<AR/+><AR/+>"));

  long_press(MENU_PIN);
  TEST_ASSERT(is_main_menu_on_free_input());
  press(ENTER_PIN);
  TEST_ASSERT(is_free_input_screen());
  keyp(".-.-.");
  TEST_ASSERT(row_is(1, "+"));

  /* Settings > Prosigns (index 2): Punctuation -> Prosign, then leave Settings. */
  long_press(MENU_PIN);
  press(NEXT_PIN);
  press(ENTER_PIN);
  press(NEXT_PIN);
  press(NEXT_PIN);
  press(ENTER_PIN);
  press(NEXT_PIN);
  press(ENTER_PIN);
  press(MENU_PIN);
  TEST_ASSERT_EQUAL(MENU_NODE_MAIN, menu_node());
  TEST_ASSERT_EQUAL(SETTINGS_PROSIGN_NAME, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  press(PREV_PIN);
  press(ENTER_PIN);
  TEST_ASSERT(is_free_input_screen());
  keyp(".-.-.");
  TEST_ASSERT(row_is(1, "<AR>"));
  TEST_ASSERT(row_is(2, NULL));
}

int main(void) {
  /* V-89: the scenarios that key no AR, BT, KN or AS give the same result for each prosign style. */
  for (record_style = SETTINGS_PROSIGN_PUNCTUATION; record_style <= SETTINGS_PROSIGN_BOTH; ++record_style) {
    printf("prosign style %u\n", record_style);
    TEST_RUN(test_v54_open);
    TEST_RUN(test_v55_closed_contact_at_start);
    TEST_RUN(test_v56_text);
    TEST_RUN(test_v57_no_space_at_line_start);
    TEST_RUN(test_v58_v59_space_at_line_end);
    TEST_RUN(test_v60_scroll);
    TEST_RUN(test_v61_unknown);
    TEST_RUN(test_v63_sk);
    TEST_RUN(test_v64_v65_clear);
    TEST_RUN(test_v66_to_v69_delimiter);
    TEST_RUN(test_v70_short_presses_do_nothing);
    TEST_RUN(test_v71_exit_and_reopen);
    TEST_RUN(test_v72_held_contact_at_exit);
    TEST_RUN(test_v73_long_threshold);
  }
  record_style = SETTINGS_PROSIGN_BOTH;
  TEST_RUN(test_v62_prosigns);
  TEST_RUN(test_v79_keyer_settings);
  TEST_RUN(test_v88_style_read_at_start);
  TEST_ASSERT(!fake_keyer_is_overflowed());
  return TEST_FINISH();
}
