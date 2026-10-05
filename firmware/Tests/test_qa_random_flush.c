/*
 * QA random-mutation flush test. A fixed-seed generator changes the framebuffer, the orientation and the dirty
 * pages between lcd_flush_step() calls, the way a main loop that draws while it flushes would. Every step is
 * decoded with the QA bus oracle and applied to the QA RAM model. Whenever nothing is pending, the visible RAM
 * window must equal the framebuffer.
 */
#include <string.h>

#include "board/lcd_config.h"
#include "fake_delay.h"
#include "fake_lcd_pins.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "lcd_protocol_decoder.h"
#include "qa/qa_bus.h"
#include "support/test_assert.h"

#define SEED 0xC0FFEEU
#define ITERATIONS 30000U
#define BYTE_CAPACITY 4096U
#define CHUNKS_PER_PAGE (LCD_WIDTH / LCD_FLUSH_CHUNK_COLUMNS)
/* Worst case to drain: a page in progress plus all 8 pages. */
#define MAX_DRAIN_STEPS ((LCD_PAGE_COUNT + 1U) * CHUNKS_PER_PAGE)

static uint32_t random_state = SEED;
static qa_byte_t bytes[BYTE_CAPACITY];
static lcd_decoded_byte_t developer_bytes[BYTE_CAPACITY];
static uint8_t bus_levels = QA_ALL_HIGH;
static qa_rules_t bus_rules;
static size_t bus_open_bytes; /* calls that ended inside a byte or with CS low */
static size_t total_bytes;
static qa_ram_t ram;
static uint8_t orientation = LCD_ORIENTATION_NORMAL;

static size_t step_count;
static size_t bad_step_count;
static size_t developer_violations;
static size_t window_checks;
static size_t window_mismatches;
static size_t unbounded_drains;

static uint32_t next_random(void) {
  /* xorshift32 */
  random_state ^= random_state << 13;
  random_state ^= random_state >> 17;
  random_state ^= random_state << 5;
  return random_state;
}

static uint8_t column_offset(void) {
  return (orientation == LCD_ORIENTATION_ROTATED) ? LCD_COLUMN_OFFSET_ROTATED : LCD_COLUMN_OFFSET_NORMAL;
}

/*
 * Feeds the events of the last call into the oracle and the RAM model, then empties the event log. Returns the
 * number of bytes the call sent; they are in bytes[].
 */
static size_t consume_bus(size_t max_bytes, size_t max_cs_falls) {
  qa_bus_t bus;
  size_t violations = 0U;
  size_t count;

  qa_bus_start(&bus, bus_levels, bytes, BYTE_CAPACITY);
  qa_bus_feed_events(&bus, 0U, fake_lcd_pins_event_count());
  bus_levels = bus.levels;
  bus_rules.sda_change_while_sck_high += bus.rules.sda_change_while_sck_high;
  bus_rules.cs_change_while_sck_low += bus.rules.cs_change_while_sck_low;
  bus_rules.a0_change_inside_byte += bus.rules.a0_change_inside_byte;
  bus_rules.cs_rise_inside_byte += bus.rules.cs_rise_inside_byte;
  bus_rules.rst_low_while_selected += bus.rules.rst_low_while_selected;
  bus_open_bytes += ((bus.bit_count != 0U) || ((bus.levels & QA_BIT_CS) == 0U)) ? 1U : 0U;
  count = (bus.byte_count <= BYTE_CAPACITY) ? bus.byte_count : BYTE_CAPACITY;
  total_bytes += bus.byte_count;
  qa_ram_apply(&ram, bytes, count);
  if ((bus.byte_count > max_bytes) || (bus.cs_fall_count > max_cs_falls)) {
    ++bad_step_count;
  }

  (void)lcd_protocol_decode(0U, fake_lcd_pins_event_count(), developer_bytes, BYTE_CAPACITY, &violations);
  developer_violations += violations;
  fake_lcd_pins_reset();
  return count;
}

static void check_window(void) {
  const uint8_t offset = column_offset();
  uint8_t page;
  size_t column;
  ++window_checks;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    for (column = 0U; column < LCD_WIDTH; ++column) {
      window_mismatches += (ram.ram[page][column + offset] != lcd_fb_page(page)[column]) ? 1U : 0U;
    }
  }
}

static void flush_step(void) {
  lcd_flush_step();
  ++step_count;
  consume_bus(3U + LCD_FLUSH_CHUNK_COLUMNS, 1U);
}

static void drain(void) {
  size_t steps = 0U;
  while (lcd_has_pending_flush() && (steps <= MAX_DRAIN_STEPS)) {
    flush_step();
    ++steps;
  }
  unbounded_drains += (steps > MAX_DRAIN_STEPS) ? 1U : 0U;
  check_window();
}

static void mutate(void) {
  const uint32_t r = next_random();
  const uint32_t choice = r % 100U;
  const int16_t x = (int16_t)((int32_t)((r >> 8) % 144U) - 8);
  const int16_t y = (int16_t)((int32_t)((r >> 16) % 80U) - 8);

  if (choice < 55U) {
    lcd_fb_set_pixel(x, y, (uint8_t)((r >> 24) % 4U)); /* color 3 is invalid and must be ignored */
  } else if (choice < 70U) {
    const uint8_t page = (uint8_t)((r >> 8) % LCD_PAGE_COUNT);
    lcd_fb_page(page)[(r >> 12) % LCD_WIDTH] = (uint8_t)(r >> 24);
    lcd_fb_mark_dirty(page);
  } else if (choice < 72U) {
    lcd_fb_clear();
  } else if (choice < 74U) {
    const uint8_t requested = (uint8_t)((r >> 8) % 3U); /* 2 is unknown and must be ignored */
    lcd_set_orientation(requested);
    if (requested <= LCD_ORIENTATION_ROTATED) {
      orientation = requested;
    }
    consume_bus(2U, 1U);
  } else if (choice < 76U) {
    lcd_fb_mark_dirty((uint8_t)((r >> 8) % 12U));
  } else if (choice < 77U) {
    lcd_fb_mark_all_dirty();
  } else if (choice < 78U) {
    /* Direct write outside the framebuffer: the next flush of that page must repair it. */
    static uint8_t junk[16];
    const uint8_t page = (uint8_t)((r >> 8) % LCD_PAGE_COUNT);
    memset(junk, (int)(r >> 24), sizeof(junk));
    lcd_write(page, (uint8_t)((r >> 12) % LCD_WIDTH), junk, sizeof(junk));
    consume_bus(3U + sizeof(junk), 1U);
    lcd_fb_mark_dirty(page);
  } else {
    /* No change this time. */
  }
}

static void test_random_mutation_between_steps(void) {
  size_t i;

  fake_lcd_pins_reset();
  fake_delay_reset();
  lcd_set_orientation(LCD_ORIENTATION_NORMAL);
  orientation = LCD_ORIENTATION_NORMAL;
  lcd_fb_clear();
  lcd_init();
  qa_ram_reset(&ram, 0xA5U);
  consume_bus(BYTE_CAPACITY, 3U);
  drain();

  for (i = 0U; i < ITERATIONS; ++i) {
    const uint32_t extra = next_random() % 8U;
    mutate();
    if (extra == 0U) {
      mutate();
      mutate();
    }
    flush_step();
    if ((i % 997U) == 0U) {
      drain();
    }
  }
  drain();

  TEST_ASSERT_EQUAL(0U, qa_rules_total(&bus_rules));
  TEST_ASSERT_EQUAL(0U, bus_open_bytes);
  TEST_ASSERT_EQUAL(0U, developer_violations);
  TEST_ASSERT_EQUAL(0U, ram.out_of_ram_writes);
  TEST_ASSERT_EQUAL(0U, bad_step_count);
  TEST_ASSERT_EQUAL(0U, unbounded_drains);
  TEST_ASSERT_EQUAL(0U, window_mismatches);
  TEST_ASSERT(!lcd_has_pending_flush());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  printf("  seed 0x%X: %zu steps, %zu window checks, %zu bytes\n", SEED, step_count, window_checks, total_bytes);
}

/* One page redrawn after every step must not starve another dirty page. */
static void test_hot_page_does_not_starve_others(void) {
  size_t steps = 0U;
  bool is_page_six_sent = false;

  lcd_flush();
  (void)consume_bus(BYTE_CAPACITY, 64U);
  lcd_fb_mark_dirty(0U);
  lcd_fb_mark_dirty(6U);
  while (!is_page_six_sent && (steps < (4U * CHUNKS_PER_PAGE))) {
    size_t count;
    size_t i;
    lcd_flush_step();
    count = consume_bus(3U + LCD_FLUSH_CHUNK_COLUMNS, 1U);
    for (i = 0U; i < count; ++i) {
      is_page_six_sent = is_page_six_sent || (!bytes[i].is_data && (bytes[i].value == 0xB6U));
    }
    lcd_fb_page(0U)[steps % LCD_WIDTH] ^= 0x01U;
    lcd_fb_mark_dirty(0U);
    ++steps;
  }
  TEST_ASSERT(is_page_six_sent);
  lcd_flush();
}

int main(void) {
  TEST_RUN(test_random_mutation_between_steps);
  TEST_RUN(test_hot_page_does_not_starve_others);
  return TEST_FINISH();
}
