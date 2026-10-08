#ifndef TRAINING_HARNESS_H
#define TRAINING_HARNESS_H

/*
 * Harness and probes for the training mode tests (references/internal/ui-ux.md section 9.4). The real menu, buttons,
 * modes, engine and progress run with fake keyer, player, sidetone, tick and items. Include it in one test source per
 * executable.
 *
 * Time: harness_now is the fake tick. The session starts at HARNESS_T0, so "AT t" of section 9.4 is HARNESS_T0 + t.
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "board/button_pins.h"
#include "board/buttons_config.h"
#include "buttons/buttons.h"
#include "cw/keyer.h"
#include "fake_button_pins.h"
#include "fake_delay.h"
#include "fake_iap.h"
#include "fake_keyer.h"
#include "fake_koch_item.h"
#include "fake_menu_outputs.h"
#include "fake_player.h"
#include "fake_sidetone.h"
#include "fake_tick.h"
#include "fixtures/font_golden.h"
#include "gfx/fonts.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "menu/menu.h"
#include "progress/progress.h"
#include "settings/settings.h"
#include "support/menu_splash.h"
#include "support/progress_fixture.h"
#include "support/test_assert.h"

#define HARNESS_T0 10000U
#define HARNESS_EVENTS_PER_PASS 4U
#define HARNESS_NO_BAND (-1)
#define L_TAP BUTTON_EVENT_MENU
#define M_TAP BUTTON_EVENT_PREV
#define R_TAP BUTTON_EVENT_NEXT
#define ENTER_EVENT BUTTON_EVENT_ENTER
#define LONG_EVENT(code) ((button_event_t)((code) | BUTTON_EVENT_LONG_FLAG))
#define REPEAT_EVENT(code) ((button_event_t)((code) | BUTTON_EVENT_REPEAT_FLAG))

static uint16_t harness_now;
static uint8_t harness_pins;
static size_t harness_render_count;
/* AC-T21: the fake keyer and the fake player never run at the same time. */
static size_t harness_overlap_count;

static inline void harness_check_overlap(void) {
  if (fake_keyer_is_running() && fake_player_is_running()) {
    ++harness_overlap_count;
  }
}

static inline void harness_set_time(uint16_t time) {
  harness_now = time;
  fake_tick_set_now(time);
}

/* Power-on with the fake EEPROM as it is: settings and progress load, the main menu shows. */
static inline void harness_power_on(void) {
  fake_lcd_contrast_reset();
  fake_backlight_reset();
  fake_stats_reset();
  fake_keyer_reset();
  fake_player_reset();
  fake_sidetone_reset();
  fake_tick_reset(1000U);
  fake_tick_set_advance_per_read(1U);
  fake_button_pins_reset();
  fake_delay_reset();
  lcd_fb_clear();
  buttons_init();
  fake_tick_set_advance_per_read(0U);
  harness_set_time(2000U);
  harness_pins = 0U;
  (void)settings_load();
  (void)progress_load();
  fake_iap_clear_counters();
  gfx_init();
  menu_init();
  menu_test_skip_splash();
  menu_render();
  harness_render_count = 0U;
  harness_overlap_count = 0U;
  fake_keyer_set_start_hook(harness_check_overlap);
  fake_player_set_start_hook(harness_check_overlap);
}

/* One main-loop pass at harness_now, as main.c runs it. */
static inline void harness_pass(void) {
  unsigned events;
  button_event_t event;

  fake_tick_set_now(harness_now);
  for (events = 0U; events < HARNESS_EVENTS_PER_PASS; ++events) {
    event = buttons_poll();
    if (event == BUTTON_EVENT_NONE) {
      break;
    }
    menu_handle(event);
  }
  menu_poll();
  if (menu_needs_redraw()) {
    menu_render();
    ++harness_render_count;
  }
  harness_check_overlap();
}

/* Main-loop passes, one per ms, from harness_now up to and including @p time. */
static inline void harness_run_to(uint16_t time) {
  while ((int16_t)(time - harness_now) > 0) {
    harness_set_time((uint16_t)(harness_now + 1U));
    harness_pass();
  }
}

/* Sets the pins and runs @p duration passes. */
static inline void harness_pins_for(uint8_t mask, uint16_t duration) {
  harness_pins = mask;
  fake_button_pins_set(mask);
  harness_run_to((uint16_t)(harness_now + duration));
}

/* Sends @p event at the current time as the main loop does, then one menu_poll(). */
static inline void harness_event(button_event_t event) {
  fake_tick_set_now(harness_now);
  menu_handle(event);
  menu_poll();
  if (menu_needs_redraw()) {
    menu_render();
    ++harness_render_count;
  }
  harness_check_overlap();
}

/* AT t: X of ui-ux.md section 9.4: passes up to HARNESS_T0 + t - 1, then the event at HARNESS_T0 + t. */
static inline void harness_at(uint16_t t, button_event_t event) {
  harness_run_to((uint16_t)(HARNESS_T0 + t - 1U));
  harness_set_time((uint16_t)(HARNESS_T0 + t));
  harness_event(event);
}

/* OPEN(m) of ui-ux.md section 9.4: moves the main menu cursor to @p row and opens the mode. The start screen shows. */
static inline void harness_open_start_screen(uint8_t row) {
  uint8_t index;

  for (index = 0U; index < row; ++index) {
    menu_handle(BUTTON_EVENT_NEXT);
  }
  menu_render();
  harness_set_time(HARNESS_T0);
  harness_event(ENTER_EVENT);
}

/*
 * RX() and TX() of ui-ux.md section 9.4: opens the mode at HARNESS_T0, then the enter press on the start screen starts
 * the session at the earned level, also at HARNESS_T0.
 */
static inline void harness_open_mode(uint8_t row) {
  harness_open_start_screen(row);
  harness_event(ENTER_EVENT);
}

/* ---------- Probes of ui-ux.md sections 9 and 9.4 ---------- */

static inline bool pixel(int x, int y) { return lcd_fb_get_pixel((int16_t)x, (int16_t)y); }

typedef struct {
  int column;
  const char* text;
  bool is_inverted;
} row_text_t;

static inline bool golden_ink(char character, int column, int row) {
  const uint8_t code = (uint8_t)character;
  if ((code < FONT_GOLDEN_5X8_FIRST) || (code > FONT_GOLDEN_5X8_LAST)) {
    return false;
  }
  return (font_golden_5x8[code - FONT_GOLDEN_5X8_FIRST].rows[row] & (1U << (4 - column))) != 0U;
}

/* Every pixel of text row @p r: a band on x [band_x0, band_x1] (rows 8r+1..8r+7, all rows for row 0), the texts. */
static inline bool row_matches(int r, int band_x0, int band_x1, const row_text_t* texts, size_t count) {
  int x;
  int y;
  size_t i;
  for (y = 8 * r; y < 8 * r + 8; ++y) {
    for (x = 0; x < 128; ++x) {
      const int cell_row = y - 8 * r;
      bool expected = (band_x0 >= 0) && (x >= band_x0) && (x <= band_x1) && ((r == 0) || (cell_row != 0));
      for (i = 0U; i < count; ++i) {
        const int first_x = 2 + 5 * texts[i].column;
        const int length = (int)strlen(texts[i].text);
        if ((x >= first_x) && (x < first_x + 5 * length)) {
          const bool is_ink = golden_ink(texts[i].text[(x - first_x) / 5], (x - first_x) % 5, cell_row);
          expected = texts[i].is_inverted ? !is_ink : is_ink;
          if ((r != 0) && (cell_row == 0)) {
            expected = expected && !texts[i].is_inverted;
          }
        }
      }
      if (pixel(x, y) != expected) {
        fprintf(stderr, "row %d: pixel (%d, %d) is %d\n", r, x, y, (int)pixel(x, y));
        return false;
      }
    }
  }
  return true;
}

/* HDR(s): TITLE holds and TEXT(0,0,s,inv). */
static inline bool hdr(const char* text) {
  const row_text_t row_text = {0, text, true};
  return row_matches(0, 0, 127, &row_text, 1U);
}

/* TEXT(r, c, s) in normal video, the rest of row r blank. */
static inline bool text_at(int r, int column, const char* text) {
  const row_text_t row_text = {column, text, false};
  return row_matches(r, HARNESS_NO_BAND, 0, &row_text, 1U);
}

static inline bool blank_row(int r) { return row_matches(r, HARNESS_NO_BAND, 0, NULL, 0U); }

/* STATUS(s), or STATUS() for NULL. */
static inline bool status(const char* text) {
  if (text == NULL) {
    return blank_row(1);
  }
  return text_at(1, (25 - (int)strlen(text)) / 2, text);
}

static inline bool centered(int r, const char* text) { return text_at(r, (25 - (int)strlen(text)) / 2, text); }

/* BODYOFF(a, b) */
static inline bool body_off(int first_row, int last_row) {
  int x;
  int y;
  for (y = 8 * first_row; y < 8 * last_row + 8; ++y) {
    for (x = 0; x < 128; ++x) {
      if (pixel(x, y)) {
        fprintf(stderr, "body: pixel (%d, %d) is on\n", x, y);
        return false;
      }
    }
  }
  return true;
}

/* KEYBARS: pixels (x, 55) on over the four label fields, every other pixel of row 6 off. */
static inline bool key_bars(void) {
  int x;
  int y;
  for (y = 48; y < 56; ++y) {
    for (x = 0; x < 128; ++x) {
      const bool is_bar_x =
          ((x >= 2) && (x <= 26)) || ((x >= 30) && (x <= 54)) || ((x >= 58) && (x <= 82)) || ((x >= 87) && (x <= 126));
      const bool expected = (y == 55) && is_bar_x;
      if (pixel(x, y) != expected) {
        fprintf(stderr, "key bar: pixel (%d, %d) is %d\n", x, y, (int)pixel(x, y));
        return false;
      }
    }
  }
  return true;
}

/* Pixel x of label @p text of run @p run (ui-ux.md section 5.12.1). */
static inline int label_x(int run, const char* text) {
  const int length = (int)strlen(text);
  if (run == 0) {
    return 2;
  }
  if (run == 3) {
    return 127 - 5 * length;
  }
  return 2 + 28 * run + (5 * (5 - length)) / 2;
}

/* Row 7 holds exactly the four labels; @p labels[i] empty means no ink in field i. */
static inline bool labels_are(const char* const labels[4]) {
  int x;
  int y;
  int run;
  for (y = 56; y < 64; ++y) {
    for (x = 0; x < 128; ++x) {
      bool expected = false;
      for (run = 0; run < 4; ++run) {
        const int first_x = label_x(run, labels[run]);
        const int length = (int)strlen(labels[run]);
        if ((x >= first_x) && (x < first_x + 5 * length)) {
          expected = golden_ink(labels[run][(x - first_x) / 5], (x - first_x) % 5, y - 56);
        }
      }
      if (pixel(x, y) != expected) {
        fprintf(stderr, "row 7: pixel (%d, %d) is %d\n", x, y, (int)pixel(x, y));
        return false;
      }
    }
  }
  return true;
}

/* KEYS(a, b, c, d) of ui-ux.md section 9.4: the labels of menu/back, prev, next and enter, and KEYBARS. */
static inline bool keys(const char* a, const char* b, const char* c, const char* d) {
  const char* const labels[4] = {a, b, c, d};
  return key_bars() && labels_are(labels);
}

/* STARTBTN: the band x 47-81 on y 57-63 with "Start" inverted in columns 10-14; y 56 and the rest of row 7 off. */
static inline bool start_button(void) {
  const row_text_t text = {10, "Start", true};
  return row_matches(7, 47, 81, &text, 1U) && !pixel(46, 60) && !pixel(82, 60);
}

/*
 * LVL(s, e): row 4 holds the selector line of ui-ux.md section 5.16.2. The markers follow the indexes: '<' when
 * @p has_prev, '>' when @p has_next (they differ from s > 1 and s < e only at the D-T5 saturation).
 */
static inline bool lvl_markers(unsigned shown, unsigned earned, bool has_prev, bool has_next) {
  char text[24];
  row_text_t texts[3];
  size_t count = 1U;
  int length;
  int column;

  (void)snprintf(text, sizeof(text), "L%u of %u", shown, earned);
  length = (int)strlen(text);
  column = (19 - length) / 2;
  texts[0].column = column + 3;
  texts[0].text = text;
  texts[0].is_inverted = false;
  if (has_prev) {
    texts[count].column = column;
    texts[count].text = "<";
    texts[count].is_inverted = false;
    ++count;
  }
  if (has_next) {
    texts[count].column = column + length + 5;
    texts[count].text = ">";
    texts[count].is_inverted = false;
    ++count;
  }
  return row_matches(4, HARNESS_NO_BAND, 0, texts, count);
}

static inline bool lvl(unsigned shown, unsigned earned) {
  return lvl_markers(shown, earned, shown > 1U, shown < earned);
}

/*
 * A main menu row (ui-ux.md section 5.16.6): @p label in column 0 and, when @p level is not NULL, the level
 * right-aligned to column 22, both inverted inside a full-width band on the selected row.
 */
static inline bool menu_row_is(int r, const char* label, const char* level, bool is_selected) {
  row_text_t texts[2];
  size_t count = 1U;
  texts[0].column = 0;
  texts[0].text = label;
  texts[0].is_inverted = is_selected;
  if (level != NULL) {
    texts[1].column = 23 - (int)strlen(level);
    texts[1].text = level;
    texts[1].is_inverted = is_selected;
    count = 2U;
  }
  return row_matches(r, is_selected ? 0 : HARNESS_NO_BAND, 127, texts, count);
}

/* DESC(s) */
static inline bool desc(const char* text) { return centered(5, text); }

/* DIRTY(p, ...): exactly the pages of @p mask are dirty. */
static inline bool dirty_is(uint8_t mask) {
  if (lcd_fb_dirty_mask() != mask) {
    fprintf(stderr, "dirty pages 0x%02X, expected 0x%02X\n", (unsigned)lcd_fb_dirty_mask(), (unsigned)mask);
    return false;
  }
  return true;
}

#define PAGE(p) ((uint8_t)(1U << (p)))

/* Clears every dirty flag, so a later DIRTY() sees only the pages of the next event. */
static inline void harness_clear_dirty(void) {
  uint8_t page;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    lcd_fb_clear_dirty(page);
  }
}

#define ITEM_PART_BIG 0U
#define ITEM_PART_BLOCK 1U
#define ITEM_PART_BAR 2U
#define ITEM_PARTS_MAX 24U

typedef struct {
  uint8_t kind;
  int x;
  const char* text; /* ITEM_PART_BIG only */
} item_part_t;

static item_part_t item_parts[ITEM_PARTS_MAX];
static size_t item_part_count;

static inline void item_expect_reset(void) { item_part_count = 0U; }

static inline void item_expect(uint8_t kind, int x, const char* text) {
  if (item_part_count < ITEM_PARTS_MAX) {
    item_parts[item_part_count].kind = kind;
    item_parts[item_part_count].x = x;
    item_parts[item_part_count].text = text;
    ++item_part_count;
  }
}

/*
 * ITEMLINE() with the expected BIG, BLOCK and BAR parts: y 24-39 equals a reference drawing of the parts. The BIG
 * reference uses gfx_draw_text() with FONTS_ID_8X16; the font decoder has its own golden tests.
 */
static inline bool item_line_is(void) {
  uint8_t saved[2U * LCD_WIDTH];
  uint8_t expected[2U * LCD_WIDTH];
  const uint8_t dirty = lcd_fb_dirty_mask();
  size_t index;
  bool is_equal;

  memcpy(saved, lcd_fb_page(3U), LCD_WIDTH);
  memcpy(&saved[LCD_WIDTH], lcd_fb_page(4U), LCD_WIDTH);
  gfx_fill_rect(0, 24, 128, 16, LCD_PIXEL_OFF);
  for (index = 0U; index < item_part_count; ++index) {
    const item_part_t* part = &item_parts[index];
    if (part->kind == ITEM_PART_BIG) {
      (void)gfx_set_font(FONTS_ID_8X16);
      (void)gfx_draw_text((int16_t)part->x, 24, part->text, GFX_TEXT_NORMAL);
      (void)gfx_set_font(FONTS_ID_5X8);
    } else if (part->kind == ITEM_PART_BLOCK) {
      gfx_fill_rect((int16_t)part->x, 26, 7, 14, LCD_PIXEL_ON);
    } else {
      gfx_fill_rect((int16_t)part->x, 38, 7, 2, LCD_PIXEL_ON);
    }
  }
  memcpy(expected, lcd_fb_page(3U), LCD_WIDTH);
  memcpy(&expected[LCD_WIDTH], lcd_fb_page(4U), LCD_WIDTH);
  memcpy(lcd_fb_page(3U), saved, LCD_WIDTH);
  memcpy(lcd_fb_page(4U), &saved[LCD_WIDTH], LCD_WIDTH);
  for (index = 0U; index < LCD_PAGE_COUNT; ++index) {
    if ((dirty & (1U << index)) == 0U) {
      lcd_fb_clear_dirty((uint8_t)index);
    }
  }
  is_equal = (memcmp(saved, expected, sizeof(saved)) == 0);
  if (!is_equal) {
    fprintf(stderr, "item line differs\n");
  }
  return is_equal;
}

/* BLOCK(x) and BAR(x) edges of ui-ux.md section 9.4. */
static inline bool block_at(int x) {
  int y;
  for (y = 26; y <= 39; ++y) {
    if (!pixel(x, y) || !pixel(x + 6, y)) {
      return false;
    }
  }
  return !pixel(x - 1, 30) && !pixel(x + 7, 30);
}

/* MARKS(r, x0, p): the marks of pattern @p p from x0, all other pixels of row r off. */
static inline bool marks_are(int r, int x0, const char* pattern) {
  bool is_on[128];
  int x = x0;
  int y;
  size_t index;
  memset(is_on, 0, sizeof(is_on));
  for (index = 0U; pattern[index] != '\0'; ++index) {
    const int width = (pattern[index] == '-') ? 6 : 2;
    int k;
    for (k = 0; k < width; ++k) {
      if ((x + k) < 128) {
        is_on[x + k] = true;
      }
    }
    x += width + 3;
  }
  for (y = 8 * r; y < 8 * r + 8; ++y) {
    for (x = 0; x < 128; ++x) {
      const bool expected = ((y == 8 * r + 3) || (y == 8 * r + 4)) && is_on[x];
      if (pixel(x, y) != expected) {
        fprintf(stderr, "marks row %d: pixel (%d, %d) is %d\n", r, x, y, (int)pixel(x, y));
        return false;
      }
    }
  }
  return true;
}

/* Global rule of V-90 to V-121: in rows 1-7, the pixels with x = 0, 1 and 127 are off. */
static inline bool margins_off(void) {
  int y;
  for (y = 8; y < 64; ++y) {
    if (pixel(0, y) || pixel(1, y) || pixel(127, y)) {
      return false;
    }
  }
  return true;
}

/*
 * End screen of ui-ux.md section 5.15 for @p score of @p count with the pass threshold @p threshold, after a session
 * at the earned level with shown level @p shown_level. A pass erases page 1 once, writes the record once and
 * preselects the next level; a fail writes nothing and preselects the same level.
 */
static inline bool end_screen_is(unsigned score, unsigned count, unsigned threshold, unsigned shown_level) {
  const bool is_passed = score >= threshold;
  char text[32];
  bool is_ok;

  (void)snprintf(text, sizeof(text), "Score %u/%u", score, count);
  is_ok = centered(2, text) && blank_row(1) && blank_row(6);
  if (is_passed) {
    is_ok = is_ok && centered(3, "Passed, new level") && lvl(shown_level + 1U, shown_level + 1U) &&
            (fake_iap_erase_count(1U) == 1U) && (fake_iap_program_count(0x0200U) == 1U);
  } else {
    (void)snprintf(text, sizeof(text), "Not passed, need %u", threshold);
    is_ok = is_ok && centered(3, text) && lvl(shown_level, shown_level) && (fake_iap_command_count(3U) == 0U) &&
            (fake_iap_command_count(2U) == 0U);
  }
  return is_ok && start_button() && margins_off();
}

/* Index of the last fake player call of @p kind, or -1. */
static inline long last_player_call(uint8_t kind) {
  long index;
  for (index = (long)fake_player_call_count() - 1; index >= 0; --index) {
    if (fake_player_call_at((size_t)index) == kind) {
      return index;
    }
  }
  return -1;
}

#endif /* TRAINING_HARNESS_H */
