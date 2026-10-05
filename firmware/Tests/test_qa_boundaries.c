/*
 * QA boundary and invalid-argument tests for every public lcd_spi, lcd_fb and lcd function. Integer arguments
 * are swept over their whole range where the cost allows. Buffers given to lcd_write() are exactly as long as the
 * clipped length, so AddressSanitizer stops any read past the clip.
 */
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "board/lcd_config.h"
#include "fake_delay.h"
#include "fake_lcd_pins.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "lcd/lcd_spi.h"
#include "lcd_protocol_decoder.h"
#include "qa/qa_bus.h"
#include "support/test_assert.h"

#define FB_SIZE (LCD_PAGE_COUNT * LCD_WIDTH)
#define BYTE_CAPACITY 4096U
#define EVENTS_PER_BYTE 24U /* SCK low, SDA, SCK high per bit */
#define INIT_CONTRAST_INDEX 6U

static qa_byte_t bytes[BYTE_CAPACITY];
static uint8_t shadow[FB_SIZE];
static uint8_t source[256];

/* Decodes all logged events with the QA oracle. Returns the byte count; rule breaks are checked here. */
static size_t decode_all(qa_bus_t* bus) {
  qa_bus_start(bus, QA_ALL_HIGH, bytes, BYTE_CAPACITY);
  qa_bus_feed_events(bus, 0U, fake_lcd_pins_event_count());
  TEST_ASSERT_EQUAL(0U, qa_rules_total(&bus->rules));
  TEST_ASSERT_EQUAL(0U, bus->bit_count);
  TEST_ASSERT(bus->byte_count <= BYTE_CAPACITY);
  return (bus->byte_count <= BYTE_CAPACITY) ? bus->byte_count : BYTE_CAPACITY;
}

static void reset_log(void) {
  fake_lcd_pins_reset();
  fake_delay_reset();
}

static uint8_t column_offset(uint8_t orientation) {
  return (orientation == LCD_ORIENTATION_ROTATED) ? LCD_COLUMN_OFFSET_ROTATED : LCD_COLUMN_OFFSET_NORMAL;
}

static void restore_defaults(void) {
  lcd_set_contrast(LCD_DEFAULT_CONTRAST);
  lcd_set_inverted(false);
  lcd_set_all_pixels_on(false);
  lcd_set_orientation(LCD_ORIENTATION_NORMAL);
  reset_log();
}

/* Must run first: the driver state is still the power-on state. */
static void test_calls_before_init_are_safe(void) {
  qa_bus_t bus;
  size_t count;

  reset_log();
  TEST_ASSERT(!lcd_has_pending_flush());
  lcd_flush_step();
  lcd_flush();
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_event_count());

  lcd_write(0U, 0U, source, 128U);
  lcd_set_display_on(true);
  lcd_set_contrast(255U);
  lcd_set_inverted(true);
  lcd_set_all_pixels_on(true);
  lcd_sleep();
  lcd_wake();
  lcd_set_orientation(LCD_ORIENTATION_ROTATED);
  lcd_fb_set_pixel(1, 1, LCD_PIXEL_ON);
  lcd_flush();
  count = decode_all(&bus);
  TEST_ASSERT(count > 0U);
  TEST_ASSERT(!lcd_has_pending_flush());

  /* The settings stored before init are the ones lcd_init() applies. */
  reset_log();
  lcd_init();
  count = decode_all(&bus);
  if (TEST_ASSERT(count > 9U)) {
    TEST_ASSERT_EQUAL(LCD_SEG_CMD_ROTATED, bytes[1].value);
    TEST_ASSERT_EQUAL(LCD_COM_CMD_ROTATED, bytes[2].value);
    TEST_ASSERT_EQUAL(63U, bytes[INIT_CONTRAST_INDEX].value);
    TEST_ASSERT_EQUAL(0xA7U, bytes[8].value);
    TEST_ASSERT_EQUAL(0xA5U, bytes[9].value);
  }
  lcd_flush();
  restore_defaults();
}

static void spi_setup(void) {
  reset_log();
  lcd_spi_init();
  lcd_spi_set_reset(false);
  reset_log();
}

static void test_spi_select_every_kind(void) {
  unsigned kind;
  size_t wrong = 0U;
  size_t non_a0_events = 0U;
  qa_bus_t bus;

  for (kind = 0U; kind <= UINT8_MAX; ++kind) {
    size_t start;
    size_t i;
    spi_setup();
    lcd_spi_select((uint8_t)kind);
    lcd_spi_write_byte((uint8_t)kind);
    start = fake_lcd_pins_event_count();
    /* A second select with the other kind changes only A0. */
    lcd_spi_select((kind == LCD_SPI_COMMAND) ? LCD_SPI_DATA : LCD_SPI_COMMAND);
    for (i = start; i < fake_lcd_pins_event_count(); ++i) {
      non_a0_events += (fake_lcd_pins_event(i)->pin == FAKE_LCD_PIN_A0) ? 0U : 1U;
    }
    lcd_spi_write_byte(0x5AU);
    lcd_spi_deselect();
    if ((decode_all(&bus) != 2U) || (bytes[0].is_data != (kind != LCD_SPI_COMMAND)) ||
        (bytes[0].value != (uint8_t)kind) || (bytes[1].is_data != (kind == LCD_SPI_COMMAND))) {
      ++wrong;
    }
    TEST_ASSERT_EQUAL(1U, bus.cs_fall_count);
  }
  TEST_ASSERT_EQUAL(0U, wrong);
  TEST_ASSERT_EQUAL(0U, non_a0_events);
}

static void test_spi_deselect_when_idle(void) {
  qa_bus_t bus;
  spi_setup();
  lcd_spi_deselect();
  lcd_spi_deselect();
  lcd_spi_select(LCD_SPI_DATA);
  lcd_spi_write_byte(0x81U);
  lcd_spi_deselect();
  TEST_ASSERT_EQUAL(1U, decode_all(&bus));
  TEST_ASSERT_EQUAL(0x81U, bytes[0].value);
  TEST_ASSERT_EQUAL(1U, bus.cs_fall_count);
}

typedef void (*spi_write_t)(uint8_t length);
static void write_xdata(uint8_t length) { lcd_spi_write_xdata(source, length); }
static void write_code(uint8_t length) { lcd_spi_write_code(source, length); }
static void write_repeat(uint8_t length) { lcd_spi_write_repeat((uint8_t)(length ^ 0xA5U), length); }

static void check_every_length(spi_write_t write, bool is_repeat) {
  unsigned length;
  size_t wrong = 0U;
  qa_bus_t bus;

  for (length = 0U; length <= UINT8_MAX; ++length) {
    size_t start;
    size_t count;
    size_t i;
    spi_setup();
    lcd_spi_select(LCD_SPI_DATA);
    start = fake_lcd_pins_event_count();
    write((uint8_t)length);
    wrong += ((fake_lcd_pins_event_count() - start) == (length * EVENTS_PER_BYTE)) ? 0U : 1U;
    lcd_spi_deselect();
    count = decode_all(&bus);
    wrong += (count == length) ? 0U : 1U;
    for (i = 0U; i < count; ++i) {
      const uint8_t expected = is_repeat ? (uint8_t)(length ^ 0xA5U) : source[i];
      wrong += ((bytes[i].value == expected) && bytes[i].is_data) ? 0U : 1U;
    }
  }
  TEST_ASSERT_EQUAL(0U, wrong);
}

static void test_spi_every_length(void) {
  check_every_length(write_xdata, false);
  check_every_length(write_code, false);
  check_every_length(write_repeat, true);
}

static void test_contrast_every_level(void) {
  unsigned level;
  size_t wrong = 0U;
  qa_bus_t bus;

  for (level = 0U; level <= UINT8_MAX; ++level) {
    const uint8_t expected = (level > 63U) ? 63U : (uint8_t)level;
    reset_log();
    lcd_set_contrast((uint8_t)level);
    wrong += ((decode_all(&bus) == 2U) && (bytes[0].value == 0x81U) && (bytes[1].value == expected) &&
              !bytes[0].is_data && !bytes[1].is_data)
                 ? 0U
                 : 1U;
    reset_log();
    lcd_init();
    wrong += ((decode_all(&bus) > INIT_CONTRAST_INDEX) && (bytes[INIT_CONTRAST_INDEX].value == expected)) ? 0U : 1U;
  }
  TEST_ASSERT_EQUAL(0U, wrong);
  lcd_flush();
  restore_defaults();
}

static void test_orientation_every_value(void) {
  unsigned value;
  size_t wrong = 0U;
  qa_bus_t bus;
  uint8_t current = LCD_ORIENTATION_NORMAL;

  lcd_init();
  lcd_flush();
  for (value = 0U; value <= UINT8_MAX; ++value) {
    reset_log();
    lcd_set_orientation((uint8_t)value);
    if (value <= LCD_ORIENTATION_ROTATED) {
      const uint8_t seg = (value == LCD_ORIENTATION_ROTATED) ? LCD_SEG_CMD_ROTATED : LCD_SEG_CMD_NORMAL;
      const uint8_t com = (value == LCD_ORIENTATION_ROTATED) ? LCD_COM_CMD_ROTATED : LCD_COM_CMD_NORMAL;
      current = (uint8_t)value;
      wrong += ((decode_all(&bus) == 2U) && (bytes[0].value == seg) && (bytes[1].value == com)) ? 0U : 1U;
      wrong += (lcd_fb_dirty_mask() == 0xFFU) ? 0U : 1U;
      lcd_flush();
    } else {
      wrong += (fake_lcd_pins_event_count() == 0U) ? 0U : 1U;
      wrong += (lcd_fb_dirty_mask() == 0U) ? 0U : 1U;
    }
    /* The stored orientation decides the column offset. */
    reset_log();
    lcd_write(0U, 0U, source, 0U);
    wrong += ((decode_all(&bus) == 3U) && (bytes[1].value == column_offset(current))) ? 0U : 1U;
  }
  TEST_ASSERT_EQUAL(0U, wrong);
  restore_defaults();
}

/* Checks one lcd_write() call with a heap buffer of exactly the clipped length. */
static size_t check_write(uint8_t orientation, uint8_t page, uint8_t column, uint8_t length) {
  const bool is_valid = (page < LCD_PAGE_COUNT) && (column < LCD_WIDTH);
  const size_t clipped = is_valid ? ((length < (LCD_WIDTH - column)) ? length : (LCD_WIDTH - column)) : 0U;
  const unsigned raw_column = (unsigned)column + column_offset(orientation);
  uint8_t* data = malloc((clipped == 0U) ? 1U : clipped);
  size_t wrong = 0U;
  size_t count;
  size_t i;
  qa_bus_t bus;

  if (data == NULL) {
    return 1U;
  }
  for (i = 0U; i < clipped; ++i) {
    data[i] = (uint8_t)((i * 13U) + page + column);
  }
  reset_log();
  lcd_write(page, column, data, length);
  count = decode_all(&bus);
  if (!is_valid) {
    wrong += (fake_lcd_pins_event_count() == 0U) ? 0U : 1U;
  } else {
    wrong += (count == (3U + clipped)) ? 0U : 1U;
    wrong += (bus.cs_fall_count == 1U) ? 0U : 1U;
    if (count >= 3U) {
      wrong += ((bytes[0].value == (0xB0U | page)) && !bytes[0].is_data) ? 0U : 1U;
      wrong += ((bytes[1].value == (raw_column & 0x0FU)) && !bytes[1].is_data) ? 0U : 1U;
      wrong += ((bytes[2].value == (0x10U | (raw_column >> 4))) && !bytes[2].is_data) ? 0U : 1U;
    }
    for (i = 3U; (i < count) && ((i - 3U) < clipped); ++i) {
      wrong += ((bytes[i].value == data[i - 3U]) && bytes[i].is_data) ? 0U : 1U;
    }
  }
  free(data);
  return wrong;
}

static void test_write_arguments(void) {
  static const uint8_t lengths[] = {0U, 1U, 2U, 127U, 128U, 129U, 254U, 255U};
  static const uint8_t columns[] = {0U, 1U, 63U, 64U, 120U, 126U, 127U};
  uint8_t orientation;
  size_t wrong = 0U;
  uint8_t dirty;
  unsigned a;
  size_t i;

  lcd_init();
  lcd_flush();
  for (orientation = LCD_ORIENTATION_NORMAL; orientation <= LCD_ORIENTATION_ROTATED; ++orientation) {
    lcd_set_orientation(orientation);
    lcd_flush();
    memcpy(shadow, lcd_fb_page(0U), FB_SIZE);
    dirty = lcd_fb_dirty_mask();
    for (a = 0U; a <= UINT8_MAX; ++a) {
      wrong += check_write(orientation, (uint8_t)a, 0U, 1U);
      for (i = 0U; i < sizeof(lengths); ++i) {
        wrong += check_write(orientation, 3U, (uint8_t)a, lengths[i]);
      }
      for (i = 0U; i < sizeof(columns); ++i) {
        wrong += check_write(orientation, 7U, columns[i], (uint8_t)a);
      }
    }
    /* lcd_write() bypasses the framebuffer. */
    TEST_ASSERT(memcmp(shadow, lcd_fb_page(0U), FB_SIZE) == 0);
    TEST_ASSERT_EQUAL(dirty, lcd_fb_dirty_mask());
  }
  TEST_ASSERT_EQUAL(0U, wrong);
  restore_defaults();
}

static void expect_pixel(int16_t x, int16_t y, uint8_t color, size_t* wrong) {
  const bool is_on_screen = (x >= 0) && (x < (int16_t)LCD_WIDTH) && (y >= 0) && (y < (int16_t)LCD_HEIGHT);
  const uint8_t dirty_before = lcd_fb_dirty_mask();
  uint8_t expected_dirty = dirty_before;

  if (is_on_screen && (color <= LCD_PIXEL_INVERT)) {
    const size_t index = ((size_t)y >> 3) * LCD_WIDTH + (size_t)x;
    const uint8_t mask = (uint8_t)(1U << ((unsigned)y & 7U));
    const uint8_t old_value = shadow[index];
    if (color == LCD_PIXEL_ON) {
      shadow[index] = (uint8_t)(old_value | mask);
    } else if (color == LCD_PIXEL_OFF) {
      shadow[index] = (uint8_t)(old_value & (uint8_t)~mask);
    } else {
      shadow[index] = (uint8_t)(old_value ^ mask);
    }
    if (shadow[index] != old_value) {
      expected_dirty = (uint8_t)(expected_dirty | (1U << ((unsigned)y >> 3)));
    }
  }
  lcd_fb_set_pixel(x, y, color);
  *wrong += (lcd_fb_dirty_mask() == expected_dirty) ? 0U : 1U;
  *wrong += (lcd_fb_get_pixel(x, y) ==
             (is_on_screen && ((shadow[((size_t)y >> 3) * LCD_WIDTH + (size_t)x] >> ((unsigned)y & 7U)) & 1U)))
                ? 0U
                : 1U;
}

static void test_set_pixel_every_color_and_edge(void) {
  static const int16_t far_values[] = {INT16_MIN, INT16_MIN + 1, -129, -128, -2, 129, 255, 256, 1000, INT16_MAX};
  size_t wrong = 0U;
  unsigned color;
  int16_t x;
  int16_t y;
  size_t i;
  size_t j;
  uint8_t page;

  lcd_fb_clear();
  memset(shadow, 0, sizeof(shadow));
  for (color = 0U; color <= UINT8_MAX; ++color) {
    for (x = -1; x <= (int16_t)LCD_WIDTH; ++x) {
      for (y = -1; y <= (int16_t)LCD_HEIGHT; ++y) {
        expect_pixel(x, y, (uint8_t)color, &wrong);
      }
      for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
        lcd_fb_clear_dirty(page);
      }
    }
    wrong += (memcmp(shadow, lcd_fb_page(0U), FB_SIZE) == 0) ? 0U : 1U;
  }
  for (i = 0U; i < sizeof(far_values) / sizeof(far_values[0]); ++i) {
    for (j = 0U; j < sizeof(far_values) / sizeof(far_values[0]); ++j) {
      for (color = 0U; color <= 3U; ++color) {
        expect_pixel(far_values[i], far_values[j], (uint8_t)color, &wrong);
        expect_pixel(far_values[i], 5, (uint8_t)color, &wrong);
        expect_pixel(5, far_values[j], (uint8_t)color, &wrong);
      }
    }
  }
  TEST_ASSERT(memcmp(shadow, lcd_fb_page(0U), FB_SIZE) == 0);
  TEST_ASSERT_EQUAL(0U, wrong);
}

static void test_page_and_dirty_every_value(void) {
  unsigned page;
  size_t wrong = 0U;

  for (page = 0U; page <= UINT8_MAX; ++page) {
    uint8_t p;
    for (p = 0U; p < LCD_PAGE_COUNT; ++p) {
      lcd_fb_clear_dirty(p);
    }
    lcd_fb_mark_dirty((uint8_t)page);
    if (page < LCD_PAGE_COUNT) {
      wrong += (lcd_fb_page((uint8_t)page) == (lcd_fb_page(0U) + (page * LCD_WIDTH))) ? 0U : 1U;
      wrong += (lcd_fb_dirty_mask() == (1U << page)) ? 0U : 1U;
      lcd_fb_mark_all_dirty();
      lcd_fb_clear_dirty((uint8_t)page);
      wrong += (lcd_fb_dirty_mask() == (0xFFU & ~(1U << page))) ? 0U : 1U;
    } else {
      wrong += (lcd_fb_page((uint8_t)page) == NULL) ? 0U : 1U;
      wrong += (lcd_fb_dirty_mask() == 0U) ? 0U : 1U;
      lcd_fb_mark_all_dirty();
      lcd_fb_clear_dirty((uint8_t)page);
      wrong += (lcd_fb_dirty_mask() == 0xFFU) ? 0U : 1U;
    }
  }
  TEST_ASSERT_EQUAL(0U, wrong);
}

static void test_init_is_repeatable(void) {
  static qa_byte_t first[BYTE_CAPACITY];
  size_t first_count;
  size_t count;
  qa_bus_t bus;
  size_t i;

  restore_defaults();
  lcd_init();
  first_count = decode_all(&bus);
  memcpy(first, bytes, first_count * sizeof(bytes[0]));
  TEST_ASSERT_EQUAL(3U, fake_delay_count());
  TEST_ASSERT_EQUAL(LCD_RESET_PULSE_MS, fake_delay_ms_at(0U));
  TEST_ASSERT_EQUAL(LCD_RESET_WAIT_MS, fake_delay_ms_at(1U));
  TEST_ASSERT_EQUAL(LCD_POWER_SETTLE_MS, fake_delay_ms_at(2U));
  TEST_ASSERT(LCD_RESET_PULSE_MS >= 1U);
  TEST_ASSERT(LCD_POWER_SETTLE_MS >= 10U);
  /* Idle after init: CS high, SCK high, RST high. */
  TEST_ASSERT_EQUAL(QA_BIT_CS | QA_BIT_SCK | QA_BIT_RST, bus.levels & (QA_BIT_CS | QA_BIT_SCK | QA_BIT_RST));

  /* Re-init in the middle of a page transfer gives the same bus stream and restarts the flush. */
  lcd_flush_step();
  reset_log();
  lcd_init();
  count = decode_all(&bus);
  TEST_ASSERT_EQUAL(first_count, count);
  for (i = 0U; (i < count) && (i < first_count); ++i) {
    TEST_ASSERT_EQUAL(first[i].value, bytes[i].value);
    TEST_ASSERT_EQUAL(first[i].is_data, bytes[i].is_data);
  }
  TEST_ASSERT_EQUAL(0xFFU, lcd_fb_dirty_mask());
  TEST_ASSERT(lcd_has_pending_flush());
  reset_log();
  lcd_flush_step();
  if (TEST_ASSERT(decode_all(&bus) >= 3U)) {
    TEST_ASSERT_EQUAL(0xB0U, bytes[0].value);
    TEST_ASSERT_EQUAL(LCD_COLUMN_OFFSET_NORMAL & 0x0FU, bytes[1].value);
    TEST_ASSERT_EQUAL(0x10U | (LCD_COLUMN_OFFSET_NORMAL >> 4), bytes[2].value);
  }
  lcd_flush();
  TEST_ASSERT(!lcd_has_pending_flush());
}

/*
 * Codex M1: 0x2F must follow the reset release within 5 ms (unverified on the device). delay_ms(ms) can wait
 * ms + 1, so the only delay in that window must be LCD_RESET_WAIT_MS with LCD_RESET_WAIT_MS + 1 <= 3, and only the
 * init commands may be sent there.
 */
static void test_reset_release_window(void) {
  const uint8_t expected[] = {LCD_BIAS_CMD,
                              LCD_SEG_CMD_NORMAL,
                              LCD_COM_CMD_NORMAL,
                              0x40U,
                              LCD_REGULATION_RATIO_CMD,
                              0x81U,
                              LCD_DEFAULT_CONTRAST,
                              0x2FU};
  size_t rst_rise = 0U;
  size_t power_on_end = 0U;
  bool has_rst_rise = false;
  bool has_power_on = false;
  size_t delays_in_window = 0U;
  uint8_t window_delay_ms = 0U;
  qa_bus_t bus;
  size_t i;

  TEST_ASSERT((LCD_RESET_WAIT_MS + 1U) <= 3U);
  restore_defaults();
  lcd_init();
  qa_bus_start(&bus, QA_ALL_HIGH, bytes, BYTE_CAPACITY);
  for (i = 0U; (i < fake_lcd_pins_event_count()) && !has_power_on; ++i) {
    const fake_lcd_pin_event_t* event = fake_lcd_pins_event(i);
    const size_t count_before = bus.byte_count;
    if ((event->pin == FAKE_LCD_PIN_RST) && (event->level != 0U) && ((bus.levels & QA_BIT_RST) == 0U)) {
      rst_rise = i;
      has_rst_rise = true;
    }
    qa_bus_feed_events(&bus, i, i + 1U);
    if ((bus.byte_count > count_before) && (bus.byte_count <= BYTE_CAPACITY) &&
        (bytes[bus.byte_count - 1U].value == 0x2FU) && !bytes[bus.byte_count - 1U].is_data) {
      power_on_end = i;
      has_power_on = true;
    }
  }
  if (!TEST_ASSERT(has_rst_rise && has_power_on && (rst_rise < power_on_end))) {
    return;
  }
  for (i = 0U; i < fake_delay_count(); ++i) {
    const size_t at = fake_delay_event_index_at(i);
    if ((at > rst_rise) && (at <= power_on_end)) {
      ++delays_in_window;
      window_delay_ms = fake_delay_ms_at(i);
    }
  }
  TEST_ASSERT_EQUAL(1U, delays_in_window);
  TEST_ASSERT_EQUAL(LCD_RESET_WAIT_MS, window_delay_ms);
  if (TEST_ASSERT_EQUAL(sizeof(expected), bus.byte_count)) {
    for (i = 0U; i < sizeof(expected); ++i) {
      TEST_ASSERT_EQUAL(expected[i], bytes[i].value);
      TEST_ASSERT(!bytes[i].is_data);
    }
  }
  lcd_flush();
}

static void test_sleep_wake_sequences(void) {
  static const struct {
    bool is_all_on;
    uint8_t wake_first;
  } cases[] = {{false, 0xA4U}, {true, 0xA5U}};
  qa_bus_t bus;
  size_t i;

  for (i = 0U; i < sizeof(cases) / sizeof(cases[0]); ++i) {
    restore_defaults();
    lcd_set_all_pixels_on(cases[i].is_all_on);
    reset_log();
    lcd_sleep();
    lcd_sleep();
    TEST_ASSERT_EQUAL(4U, decode_all(&bus));
    TEST_ASSERT_EQUAL(0xAEU, bytes[0].value);
    TEST_ASSERT_EQUAL(0xA5U, bytes[1].value);
    TEST_ASSERT_EQUAL(2U, bus.cs_fall_count);
    reset_log();
    lcd_wake();
    lcd_wake();
    TEST_ASSERT_EQUAL(4U, decode_all(&bus));
    TEST_ASSERT_EQUAL(cases[i].wake_first, bytes[0].value);
    TEST_ASSERT_EQUAL(0xAFU, bytes[1].value);
    TEST_ASSERT_EQUAL(cases[i].wake_first, bytes[2].value);
    TEST_ASSERT_EQUAL(0xAFU, bytes[3].value);
  }
  restore_defaults();
}

static void test_pending_flush_transitions(void) {
  const size_t chunks_per_page = LCD_WIDTH / LCD_FLUSH_CHUNK_COLUMNS;
  size_t step;

  restore_defaults();
  lcd_init();
  lcd_flush();
  TEST_ASSERT(!lcd_has_pending_flush());
  lcd_fb_mark_dirty(4U);
  TEST_ASSERT(lcd_has_pending_flush());
  for (step = 0U; step < chunks_per_page; ++step) {
    lcd_flush_step();
    TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
    /* Still pending while the page is in progress. */
    TEST_ASSERT_EQUAL(step + 1U < chunks_per_page, lcd_has_pending_flush());
  }
  reset_log();
  lcd_flush_step();
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_event_count());
}

int main(void) {
  size_t i;
  for (i = 0U; i < sizeof(source); ++i) {
    source[i] = (uint8_t)((i * 37U) ^ 0x5CU);
  }
  TEST_RUN(test_calls_before_init_are_safe);
  TEST_RUN(test_spi_select_every_kind);
  TEST_RUN(test_spi_deselect_when_idle);
  TEST_RUN(test_spi_every_length);
  TEST_RUN(test_contrast_every_level);
  TEST_RUN(test_orientation_every_value);
  TEST_RUN(test_write_arguments);
  TEST_RUN(test_set_pixel_every_color_and_edge);
  TEST_RUN(test_page_and_dirty_every_value);
  TEST_RUN(test_init_is_repeatable);
  TEST_RUN(test_reset_release_window);
  TEST_RUN(test_sleep_wake_sequences);
  TEST_RUN(test_pending_flush_transitions);
  return TEST_FINISH();
}
