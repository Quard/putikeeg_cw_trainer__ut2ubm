/*
 * QA tests of Free Input (references/internal/keyer_spec.md sections 6.10 and 6.11, ui-ux.md section 5.9;
 * AC-K40 to AC-K43). A random sequence of keyer events, clears and delimiters runs against an independent model:
 * a 7 x 25 character grid with delimiter rows and its own Morse table built from the spec code strings. After
 * every step the framebuffer must equal the model rendered with the golden font, page 0 must stay untouched, and
 * the margin pixels must stay off. Sequences are biased to <SK> and long characters at the line end of row 7, so
 * the split of <SK> across a scroll is covered many times.
 */

#include <string.h>

#include "board/button_pins.h"
#include "buttons/buttons.h"
#include "cw/keyer.h"
#include "cw/morse.h"
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

/* keyer_spec.md section 6.10, as code strings. "SK" stands for the prosign. */
typedef struct {
  const char* code;
  const char* text;
} spec_entry_t;

static const spec_entry_t spec_table[] = {
    {".", "E"},       {"-", "T"},       {"..", "I"},      {".-", "A"},      {"-.", "N"},      {"--", "M"},
    {"...", "S"},     {"..-", "U"},     {".-.", "R"},     {".--", "W"},     {"-..", "D"},     {"-.-", "K"},
    {"--.", "G"},     {"---", "O"},     {"....", "H"},    {"...-", "V"},    {"..-.", "F"},    {".-..", "L"},
    {".--.", "P"},    {".---", "J"},    {"-...", "B"},    {"-..-", "X"},    {"-.-.", "C"},    {"-.--", "Y"},
    {"--..", "Z"},    {"--.-", "Q"},    {".....", "5"},   {"....-", "4"},   {"...--", "3"},   {"..---", "2"},
    {".----", "1"},   {"-....", "6"},   {"--...", "7"},   {"---..", "8"},   {"----.", "9"},   {"-----", "0"},
    {".-...", "&"},   {".-.-.", "+"},   {"-...-", "="},   {"-..-.", "/"},   {"-.--.", "("},   {"...-.-", "SK"},
    {"..--..", "?"},  {"..--.-", "_"},  {".-..-.", "\""}, {".-.-.-", "."},  {".--.-.", "@"},  {".----.", "'"},
    {"-....-", "-"},  {"-.-.-.", ";"},  {"-.-.--", "!"},  {"-.--.-", ")"},  {"--..--", ","},  {"---...", ":"},
};
#define SPEC_COUNT (sizeof(spec_table) / sizeof(spec_table[0]))

static uint8_t pattern_of(const char* code) {
  uint8_t pattern = 1U;
  for (; *code != '\0'; ++code) {
    pattern = (uint8_t)((pattern << 1) | ((*code == '-') ? 1U : 0U));
  }
  return pattern;
}

/* AC-K40 against the spec table: the 54 entries, every other index unknown, 128..255 unknown. */
static void test_table_against_spec(void) {
  bool is_listed[256];
  unsigned i;
  memset(is_listed, 0, sizeof(is_listed));
  TEST_ASSERT_EQUAL(54U, SPEC_COUNT);
  for (i = 0U; i < SPEC_COUNT; ++i) {
    const uint8_t pattern = pattern_of(spec_table[i].code);
    const char expected = (strcmp(spec_table[i].text, "SK") == 0) ? MORSE_CHAR_SK : spec_table[i].text[0];
    TEST_ASSERT(!is_listed[pattern]);
    is_listed[pattern] = true;
    TEST_ASSERT_EQUAL((unsigned char)expected, (unsigned char)morse_decode(pattern));
  }
  for (i = 0U; i < 256U; ++i) {
    if (!is_listed[i]) {
      TEST_ASSERT_EQUAL(MORSE_CHAR_UNKNOWN, morse_decode((uint8_t)i));
    }
  }
  /* AC-K41: every pattern of 1..6 elements builds its index; a 7th element and INVALID give INVALID. */
  for (i = 0U; i < 256U; ++i) {
    const uint8_t built_dit = morse_add_element((uint8_t)i, false);
    const uint8_t built_dah = morse_add_element((uint8_t)i, true);
    if ((i == MORSE_PATTERN_INVALID) || (i >= 64U)) {
      TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, built_dit);
      TEST_ASSERT_EQUAL(MORSE_PATTERN_INVALID, built_dah);
    } else {
      TEST_ASSERT_EQUAL((i << 1) & 0xFFU, built_dit);
      TEST_ASSERT_EQUAL(((i << 1) | 1U) & 0xFFU, built_dah);
    }
  }
}

/* ---- Grid model of ui-ux.md section 5.9 ---- */

#define ROWS 7
#define COLUMNS 25

static char grid[ROWS][COLUMNS + 1];
static bool is_line[ROWS];
static int cursor_row; /* 0..6 for screen rows 1..7 */
static int cursor_column;
static int model_pattern; /* -1 = invalid, 1 = empty, else the index */
static unsigned long scrolls;
static unsigned long sk_splits;

static void model_reset(void) {
  int r;
  for (r = 0; r < ROWS; ++r) {
    memset(grid[r], ' ', COLUMNS);
    grid[r][COLUMNS] = '\0';
    is_line[r] = false;
  }
  cursor_row = 0;
  cursor_column = 0;
  model_pattern = 1;
}

static void model_new_line(void) {
  if (cursor_row < ROWS - 1) {
    ++cursor_row;
  } else {
    int r;
    for (r = 0; r < ROWS - 1; ++r) {
      memcpy(grid[r], grid[r + 1], COLUMNS);
      is_line[r] = is_line[r + 1];
    }
    memset(grid[ROWS - 1], ' ', COLUMNS);
    is_line[ROWS - 1] = false;
    ++scrolls;
  }
  cursor_column = 0;
}

static void model_glyph(char glyph) {
  if (cursor_column == COLUMNS) {
    model_new_line();
  }
  grid[cursor_row][cursor_column] = glyph;
  ++cursor_column;
}

static const char* model_text_of(int pattern) {
  unsigned i;
  if (pattern < 0) {
    return "*";
  }
  for (i = 0U; i < SPEC_COUNT; ++i) {
    if ((int)pattern_of(spec_table[i].code) == pattern) {
      return (strcmp(spec_table[i].text, "SK") == 0) ? "<SK>" : spec_table[i].text;
    }
  }
  return "*";
}

static void model_emit(void) {
  const char* text;
  if (model_pattern == 1) {
    return;
  }
  text = model_text_of(model_pattern);
  if ((strlen(text) == 4U) && (cursor_column > COLUMNS - 4) && (cursor_column < COLUMNS)) {
    ++sk_splits;
  }
  for (; *text != '\0'; ++text) {
    model_glyph(*text);
  }
  model_pattern = 1;
}

static void model_event(keyer_event_t event) {
  switch (event) {
    case KEYER_EVENT_DIT:
    case KEYER_EVENT_DAH:
      if ((model_pattern < 0) || (model_pattern >= 64)) {
        model_pattern = -1;
      } else {
        model_pattern = (model_pattern << 1) | ((event == KEYER_EVENT_DAH) ? 1 : 0);
      }
      break;
    case KEYER_EVENT_CHAR_GAP:
      model_emit();
      break;
    case KEYER_EVENT_WORD_GAP:
      model_emit();
      if ((cursor_column != 0) && (cursor_column < COLUMNS)) {
        ++cursor_column;
      }
      break;
    case KEYER_EVENT_OVERFLOW:
      model_pattern = -1;
      break;
    default:
      break;
  }
}

static void model_delimiter(void) {
  if (cursor_column != 0) {
    model_new_line();
  }
  is_line[cursor_row] = true;
  cursor_column = COLUMNS;
}

/* ---- Harness ---- */

static void start(void) {
  fake_iap_reset();
  (void)settings_load();
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
  memset(lcd_fb_page(0U), 0xFF, LCD_WIDTH);
  lcd_fb_clear_dirty(0U);
  model_reset();
}

static bool screen_matches_model(void) {
  int r;
  size_t x;
  bool is_ok = true;
  const uint8_t* page0 = lcd_fb_page(0U);
  for (r = 0; r < ROWS; ++r) {
    if (is_line[r]) {
      is_ok = row_is(r + 1, SCREEN_LINE_ROW) && is_ok;
    } else {
      is_ok = row_is(r + 1, grid[r]) && is_ok;
    }
  }
  for (x = 0U; x < LCD_WIDTH; ++x) {
    is_ok = is_ok && (page0[x] == 0xFFU);
  }
  return is_ok && ((lcd_fb_dirty_mask() & 0x01U) == 0U);
}

static uint32_t rng_state;
static uint32_t rng_next(void) {
  rng_state = rng_state * 1664525U + 1013904223U;
  return rng_state >> 8;
}

static void push_and_model(keyer_event_t event) {
  fake_keyer_push_event(event);
  model_event(event);
}

static void push_code(const char* code) {
  for (; *code != '\0'; ++code) {
    push_and_model((*code == '-') ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
  }
}

static void drain(void) {
  size_t guard;
  for (guard = 0U; (guard < 1000U) && (fake_keyer_pending_count() != 0U); ++guard) {
    free_input_poll();
  }
  TEST_ASSERT_EQUAL(0U, fake_keyer_pending_count());
}

/*
 * Random walk: characters of the spec table (SK weighted), unknown and too-long patterns, overflow in a
 * character, empty character gaps, word gaps, clears and delimiters. Events go into the fake FIFO in random batch
 * sizes before a drain, so one poll handles mixed events (at most KEYER_QUEUE_SIZE + 1 per poll).
 */
static void test_random_against_grid_model(void) {
  unsigned walk;
  unsigned long steps = 0U;
  unsigned long failures = 0U;
  rng_state = 0x5EEDU;
  scrolls = 0U;
  sk_splits = 0U;
  for (walk = 0U; walk < 200U; ++walk) {
    unsigned step;
    start();
    for (step = 0U; step < 600U; ++step) {
      const uint32_t kind = rng_next() % 100U;
      if (kind < 50U) {
        push_code(spec_table[rng_next() % SPEC_COUNT].code);
        push_and_model(KEYER_EVENT_CHAR_GAP);
      } else if (kind < 62U) {
        push_code("...-.-");
        push_and_model(KEYER_EVENT_CHAR_GAP);
      } else if (kind < 66U) {
        /* 7..9 elements: unknown. */
        unsigned n = 7U + (rng_next() % 3U);
        while (n-- != 0U) {
          push_and_model(((rng_next() & 1U) != 0U) ? KEYER_EVENT_DAH : KEYER_EVENT_DIT);
        }
        push_and_model(KEYER_EVENT_CHAR_GAP);
      } else if (kind < 69U) {
        push_code(".-");
        push_and_model(KEYER_EVENT_OVERFLOW);
        push_code("-");
        push_and_model(KEYER_EVENT_CHAR_GAP);
      } else if (kind < 72U) {
        push_and_model(KEYER_EVENT_CHAR_GAP);
      } else if (kind < 88U) {
        push_and_model(KEYER_EVENT_WORD_GAP);
      } else if (kind < 92U) {
        /* An element in progress when the user acts. */
        if ((rng_next() & 1U) != 0U) {
          push_code(".");
        }
        drain();
        if ((rng_next() % 3U) == 0U) {
          free_input_clear();
          model_reset();
        } else {
          free_input_delimiter();
          {
            const int saved = model_pattern;
            model_delimiter();
            model_pattern = saved;
          }
        }
      } else {
        /* Pad the line to its end with E, so the next token lands at the line end. */
        while ((cursor_column < COLUMNS - 1 - (int)(rng_next() % 3U)) && (cursor_column != COLUMNS)) {
          push_code(".");
          push_and_model(KEYER_EVENT_CHAR_GAP);
          if (fake_keyer_pending_count() > 200U) {
            drain();
          }
        }
      }
      if (((rng_next() % 4U) == 0U) || (fake_keyer_pending_count() > 200U)) {
        drain();
        ++steps;
        if (!TEST_ASSERT(screen_matches_model())) {
          ++failures;
          fprintf(stderr, "walk %u step %u: cursor (%d, %d)\n", walk, step, cursor_row + 1, cursor_column);
          if (failures > 3U) {
            return;
          }
        }
      }
    }
    drain();
    TEST_ASSERT(screen_matches_model());
  }
  printf("grid model: %lu checks, %lu scrolls, %lu <SK> splits at a line end\n", steps, scrolls, sk_splits);
  TEST_ASSERT(!fake_keyer_is_overflowed());
  TEST_ASSERT(scrolls >= 1000U);
  TEST_ASSERT(sk_splits >= 100U);
}

/* <SK> at each start column 21..25 on row 7: the split across a scroll keeps every glyph in order. */
static void test_sk_at_line_end_of_row_7(void) {
  int start_column;
  for (start_column = 21; start_column <= 25; ++start_column) {
    int i;
    start();
    for (i = 0; i < 6 * COLUMNS + start_column; ++i) {
      push_code(".");
      push_and_model(KEYER_EVENT_CHAR_GAP);
      if (fake_keyer_pending_count() > 200U) {
        drain();
      }
    }
    drain();
    TEST_ASSERT_EQUAL(6, cursor_row);
    TEST_ASSERT_EQUAL(start_column, cursor_column);
    push_code("...-.-");
    push_and_model(KEYER_EVENT_CHAR_GAP);
    push_and_model(KEYER_EVENT_WORD_GAP);
    push_code("-");
    push_and_model(KEYER_EVENT_CHAR_GAP);
    drain();
    TEST_ASSERT(screen_matches_model());
  }
}

/* Delimiter and clear in every cursor state of row 7, including column 25 and right after a delimiter. */
static void test_delimiter_and_clear_on_row_7(void) {
  int column;
  for (column = 0; column <= 25; ++column) {
    int i;
    start();
    for (i = 0; i < 6 * COLUMNS + column; ++i) {
      push_code("-");
      push_and_model(KEYER_EVENT_CHAR_GAP);
      if (fake_keyer_pending_count() > 200U) {
        drain();
      }
    }
    drain();
    free_input_delimiter();
    model_delimiter();
    TEST_ASSERT(screen_matches_model());
    free_input_delimiter();
    model_delimiter();
    TEST_ASSERT(screen_matches_model());
    push_code(".-");
    push_and_model(KEYER_EVENT_CHAR_GAP);
    drain();
    TEST_ASSERT(screen_matches_model());
    free_input_clear();
    model_reset();
    TEST_ASSERT(screen_matches_model());
    TEST_ASSERT(text_area_is_blank());
  }
}

/* A clear drops the character in progress, also an overflowed one: the next character starts clean. */
static void test_clear_drops_invalid_pattern(void) {
  start();
  push_code(".-");
  push_and_model(KEYER_EVENT_OVERFLOW);
  drain();
  free_input_clear();
  model_reset();
  push_code("-");
  push_and_model(KEYER_EVENT_CHAR_GAP);
  drain();
  TEST_ASSERT(row_is(1, "T"));
  TEST_ASSERT(screen_matches_model());
}

int main(void) {
  TEST_RUN(test_table_against_spec);
  TEST_RUN(test_random_against_grid_model);
  TEST_RUN(test_sk_at_line_end_of_row_7);
  TEST_RUN(test_delimiter_and_clear_on_row_7);
  TEST_RUN(test_clear_drops_invalid_pattern);
  return TEST_FINISH();
}
