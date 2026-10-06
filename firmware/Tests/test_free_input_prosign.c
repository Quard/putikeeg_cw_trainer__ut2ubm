/*
 * Host tests of the prosign texts in Free Input with a fake keyer: ui-ux.md section 9.3, V-84 to V-87
 * (references/internal/keyer_spec.md section 17.4.4, AC-P18 to AC-P20).
 */

#include <string.h>

#include "buttons/buttons.h"
#include "cw/keyer.h"
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

#define AR ".-.-."
#define BT "-...-"
#define KN "-.--."
#define AS ".-..."
#define SK "...-.-"
#define STYLE_COUNT 3U

/* Layout version 2 records of keyer_spec.md section 17.4.2, by SETTINGS_PROSIGN_*. */
static const uint8_t style_records[STYLE_COUNT][SETTINGS_RECORD_SIZE] = {
    {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x01U, 0x05U, 0x08U, 0x20U, 0xB3U},
    {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x05U, 0x05U, 0x08U, 0x20U, 0xEBU},
    {0x43U, 0x57U, 0x02U, 0x58U, 0x02U, 0x20U, 0x20U, 0x14U, 0x14U, 0x02U, 0x09U, 0x05U, 0x08U, 0x20U, 0x03U},
};

/* FI with a record of @p style; the empty EEPROM gives Both. */
static void start(uint8_t style) {
  fake_iap_reset();
  if (style != SETTINGS_PROSIGN_BOTH) {
    memcpy(fake_iap_memory(), style_records[style], SETTINGS_RECORD_SIZE);
  }
  (void)settings_load();
  TEST_ASSERT_EQUAL(style, settings_get(SETTINGS_ID_PROSIGN_STYLE));
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_delay_reset();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
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

static void keyp(const char* code) {
  for (; *code != '\0'; ++code) {
    fake_keyer_push_event((*code == '-') ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
  fake_keyer_push_event(KEYER_EVENT_CHAR_GAP);
  poll_all();
}

static void key_times(char character, unsigned count) {
  unsigned i;
  for (i = 0U; i < count; ++i) {
    keyp(morse_code_of(character));
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

/* Rows 1 and 2 hold @p first and @p second; rows 3-7 are blank. */
static bool rows_are(const char* first, const char* second) {
  const char* const rows[7] = {first, second, NULL, NULL, NULL, NULL, NULL};
  return text_area_is(rows);
}

/* A row of @p count letters E followed by @p tail. */
static const char* e_row(unsigned count, const char* tail, char* buffer) {
  memset(buffer, 'E', count);
  strcpy(&buffer[count], tail);
  return buffer;
}

/* V-84 (AC-P18) */
static void test_v84_text_per_style(void) {
  static const char* const first_rows[STYLE_COUNT] = {"+=(&<SK>", "<AR><BT><KN><AS><SK>", "<AR/+><BT/=><KN/(><AS/&><"};
  static const char* const second_rows[STYLE_COUNT] = {NULL, NULL, "SK>"};
  uint8_t style;

  for (style = 0U; style < STYLE_COUNT; ++style) {
    start(style);
    keyp(AR);
    keyp(BT);
    keyp(KN);
    keyp(AS);
    keyp(SK);
    if (!TEST_ASSERT(rows_are(first_rows[style], second_rows[style]))) {
      fprintf(stderr, "style %u\n", style);
    }
    TEST_ASSERT(is_page0_untouched());
  }
}

/* V-85 (AC-P20): a word gap after a prosign text adds one blank cell. */
static void test_v85_word_gaps(void) {
  static const char* const first_rows[STYLE_COUNT] = {"+ = ( & <SK>", "<AR> <BT> <KN> <AS> <SK>",
                                                      "<AR/+> <BT/=> <KN/(> <AS/"};
  static const char* const second_rows[STYLE_COUNT] = {NULL, NULL, "&> <SK>"};
  uint8_t style;

  for (style = 0U; style < STYLE_COUNT; ++style) {
    start(style);
    keyp(AR);
    word_gap();
    keyp(BT);
    word_gap();
    keyp(KN);
    word_gap();
    keyp(AS);
    word_gap();
    keyp(SK);
    if (!TEST_ASSERT(rows_are(first_rows[style], second_rows[style]))) {
      fprintf(stderr, "style %u\n", style);
    }
  }

  start(SETTINGS_PROSIGN_BOTH);
  key_times('A', 1U);
  keyp(AR);
  word_gap();
  key_times('K', 1U);
  TEST_ASSERT(rows_are("A<AR/+> K", NULL));
}

/* V-86 (AC-P19): each glyph of a text wraps on its own. */
static void test_v86_split_at_line_end(void) {
  char line[26];
  char row[26];

  start(SETTINGS_PROSIGN_BOTH);
  key_times('E', 19U);
  keyp(AR);
  key_times('T', 1U);
  TEST_ASSERT(rows_are(e_row(19U, "<AR/+>", row), "T"));

  start(SETTINGS_PROSIGN_BOTH);
  key_times('E', 22U);
  keyp(AR);
  TEST_ASSERT(rows_are(e_row(22U, "<AR", row), "/+>"));

  start(SETTINGS_PROSIGN_BOTH);
  key_times('E', 23U);
  keyp(AR);
  TEST_ASSERT(rows_are(e_row(23U, "<A", row), "R/+>"));

  start(SETTINGS_PROSIGN_BOTH);
  key_times('E', 25U);
  keyp(AR);
  TEST_ASSERT(rows_are(full_line('E', line), "<AR/+>"));

  start(SETTINGS_PROSIGN_BOTH);
  key_times('E', 23U);
  keyp(KN);
  TEST_ASSERT(rows_are(e_row(23U, "<K", row), "N/(>"));

  start(SETTINGS_PROSIGN_NAME);
  key_times('E', 23U);
  keyp(BT);
  TEST_ASSERT(rows_are(e_row(23U, "<B", row), "T>"));
}

/* V-87 (AC-P19): a split in row 7 scrolls in the middle of the text. */
static void test_v87_split_in_last_row(void) {
  char lines[6][26];
  char row[26];
  char letter;

  start(SETTINGS_PROSIGN_BOTH);
  for (letter = 'A'; letter <= 'F'; ++letter) {
    key_times(letter, 25U);
  }
  key_times('G', 22U);
  keyp(AR);
  memset(row, 'G', 22U);
  strcpy(&row[22], "<AR");
  {
    const char* const rows[7] = {full_line('B', lines[0]),
                                 full_line('C', lines[1]),
                                 full_line('D', lines[2]),
                                 full_line('E', lines[3]),
                                 full_line('F', lines[4]),
                                 row,
                                 "/+>"};
    TEST_ASSERT(text_area_is(rows));
  }
  TEST_ASSERT(is_page0_untouched());
}

int main(void) {
  TEST_RUN(test_v84_text_per_style);
  TEST_RUN(test_v85_word_gaps);
  TEST_RUN(test_v86_split_at_line_end);
  TEST_RUN(test_v87_split_in_last_row);
  return TEST_FINISH();
}
