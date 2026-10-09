/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <string.h>

#include "board/lcd_config.h"
#include "fake_delay.h"
#include "fake_lcd_pins.h"
#include "fixtures/lcd_dump_capture.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "lcd_controller_model.h"
#include "lcd_protocol_decoder.h"
#include "support/test_assert.h"

#define DECODE_CAPACITY 16384U
#define RAM_PAGES 9U
#define RAM_COLUMNS 132U
#define CHUNKS_PER_PAGE (LCD_WIDTH / LCD_FLUSH_CHUNK_COLUMNS)

static lcd_decoded_byte_t decoded[DECODE_CAPACITY];
static lcd_decoded_byte_t expected[DECODE_CAPACITY];
static uint8_t write_buffer[LCD_WIDTH + 8U];

static void reset_bus(void) {
  fake_lcd_pins_reset();
  fake_delay_reset();
  lcd_model_reset();
}

/* Decodes the events since @p start, checks for protocol violations and returns the byte count. */
static size_t decode_since(size_t start) {
  size_t violations = 99U;
  const size_t count = lcd_protocol_decode(start, fake_lcd_pins_event_count(), decoded, DECODE_CAPACITY, &violations);
  TEST_ASSERT(count <= DECODE_CAPACITY);
  TEST_ASSERT_EQUAL(0U, violations);
  return (count <= DECODE_CAPACITY) ? count : DECODE_CAPACITY;
}

static void apply_since(size_t start) { lcd_model_apply(decoded, decode_since(start)); }

static size_t add_byte(size_t count, bool is_data, uint8_t value) {
  expected[count].is_data = is_data;
  expected[count].value = value;
  return count + 1U;
}

static bool are_bytes_equal(size_t count, size_t expected_count) {
  size_t i;
  if (!TEST_ASSERT_EQUAL(expected_count, count)) {
    return false;
  }
  for (i = 0U; i < count; ++i) {
    if (!TEST_ASSERT_EQUAL(expected[i].is_data, decoded[i].is_data) ||
        !TEST_ASSERT_EQUAL(expected[i].value, decoded[i].value)) {
      fprintf(stderr, "  first difference at byte %zu\n", i);
      return false;
    }
  }
  return true;
}

/* Checks that the events since @p start decode to the given command bytes. */
static void check_commands(size_t start, const uint8_t* commands, size_t command_count) {
  size_t count = 0U;
  size_t i;
  for (i = 0U; i < command_count; ++i) {
    count = add_byte(count, false, commands[i]);
  }
  are_bytes_equal(decode_since(start), count);
}

static void restore_settings(void) {
  lcd_set_contrast(LCD_DEFAULT_CONTRAST);
  lcd_set_inverted(false);
  lcd_set_all_pixels_on(false);
  lcd_set_orientation(LCD_ORIENTATION_NORMAL);
  reset_bus();
}

static uint8_t column_offset(uint8_t orientation) {
  return (orientation == LCD_ORIENTATION_ROTATED) ? LCD_COLUMN_OFFSET_ROTATED : LCD_COLUMN_OFFSET_NORMAL;
}

static size_t build_init_stream(uint8_t orientation, uint8_t contrast, bool is_inverted, bool is_all_on) {
  size_t count = 0U;
  uint8_t page;
  size_t column;

  count = add_byte(count, false, LCD_BIAS_CMD);
  count = add_byte(count, false, (orientation == LCD_ORIENTATION_ROTATED) ? LCD_SEG_CMD_ROTATED : LCD_SEG_CMD_NORMAL);
  count = add_byte(count, false, (orientation == LCD_ORIENTATION_ROTATED) ? LCD_COM_CMD_ROTATED : LCD_COM_CMD_NORMAL);
  count = add_byte(count, false, 0x40U);
  count = add_byte(count, false, LCD_REGULATION_RATIO_CMD);
  count = add_byte(count, false, 0x81U);
  count = add_byte(count, false, contrast);
  count = add_byte(count, false, 0x2FU);
  count = add_byte(count, false, is_inverted ? 0xA7U : 0xA6U);
  count = add_byte(count, false, is_all_on ? 0xA5U : 0xA4U);
  for (page = 0U; page < RAM_PAGES; ++page) {
    count = add_byte(count, false, (uint8_t)(0xB0U | page));
    count = add_byte(count, false, 0x00U);
    count = add_byte(count, false, 0x10U);
    for (column = 0U; column < RAM_COLUMNS; ++column) {
      count = add_byte(count, true, 0x00U);
    }
  }
  return add_byte(count, false, 0xAFU);
}

/* Fills the framebuffer with a fixed pseudo-random pattern. */
static void draw_pattern(uint32_t seed) {
  uint8_t page;
  size_t column;
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    for (column = 0U; column < LCD_WIDTH; ++column) {
      seed = (seed * 1103515245U) + 12345U;
      lcd_fb_page(page)[column] = (uint8_t)(seed >> 16);
    }
  }
  lcd_fb_mark_all_dirty();
}

static void check_model_matches_framebuffer(uint8_t orientation) {
  const uint8_t offset = column_offset(orientation);
  size_t mismatches = 0U;
  uint8_t x;
  uint8_t y;
  for (y = 0U; y < LCD_HEIGHT; ++y) {
    for (x = 0U; x < LCD_WIDTH; ++x) {
      if (lcd_model_pixel(x, y, offset) != lcd_fb_get_pixel(x, y)) {
        ++mismatches;
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, mismatches);
  TEST_ASSERT_EQUAL(0U, lcd_model_state()->ram_overflow_count);
  TEST_ASSERT_EQUAL(0U, lcd_model_state()->unknown_command_count);
}

static void test_nothing_pending_before_init(void) {
  size_t start;
  reset_bus();
  start = fake_lcd_pins_event_count();
  TEST_ASSERT(!lcd_has_pending_flush());
  lcd_flush_step();
  lcd_flush();
  TEST_ASSERT_EQUAL(start, fake_lcd_pins_event_count());
}

static void test_init_stream_defaults(void) {
  restore_settings();
  lcd_init();
  are_bytes_equal(decode_since(0U), build_init_stream(LCD_ORIENTATION_NORMAL, LCD_DEFAULT_CONTRAST, false, false));
}

static void test_init_stream_uses_stored_settings(void) {
  restore_settings();
  lcd_set_orientation(LCD_ORIENTATION_ROTATED);
  lcd_set_contrast(0x3FU);
  lcd_set_inverted(true);
  lcd_set_all_pixels_on(true);
  reset_bus();
  lcd_init();
  are_bytes_equal(decode_since(0U), build_init_stream(LCD_ORIENTATION_ROTATED, 0x3FU, true, true));
  restore_settings();
}

static void test_init_delays(void) {
  size_t violations = 99U;
  size_t end;

  restore_settings();
  lcd_init();
  if (!TEST_ASSERT_EQUAL(3U, fake_delay_count())) {
    return;
  }
  TEST_ASSERT_EQUAL(LCD_RESET_PULSE_MS, fake_delay_ms_at(0U));
  TEST_ASSERT_EQUAL(LCD_RESET_WAIT_MS, fake_delay_ms_at(1U));
  TEST_ASSERT_EQUAL(LCD_POWER_SETTLE_MS, fake_delay_ms_at(2U));

  /* Reset pulse: right after the outputs are enabled with RST low. */
  end = fake_delay_event_index_at(0U);
  TEST_ASSERT(fake_lcd_pins_event(end - 1U)->pin == FAKE_LCD_EVENT_CONFIGURE_OUTPUTS);
  TEST_ASSERT_EQUAL(0U, lcd_protocol_decode(0U, end, decoded, DECODE_CAPACITY, &violations));

  /* Reset wait: right after RST goes high, before any byte. */
  end = fake_delay_event_index_at(1U);
  TEST_ASSERT(fake_lcd_pins_event(end - 1U)->pin == FAKE_LCD_PIN_RST);
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_event(end - 1U)->level);
  TEST_ASSERT_EQUAL(0U, lcd_protocol_decode(0U, end, decoded, DECODE_CAPACITY, &violations));

  /* Power settle: after the 8 configuration bytes ending with 0x2F, with CS high. */
  end = fake_delay_event_index_at(2U);
  TEST_ASSERT(fake_lcd_pins_event(end - 1U)->pin == FAKE_LCD_PIN_CS);
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_event(end - 1U)->level);
  TEST_ASSERT_EQUAL(8U, lcd_protocol_decode(0U, end, decoded, DECODE_CAPACITY, &violations));
  TEST_ASSERT_EQUAL(0x2FU, decoded[7].value);
  TEST_ASSERT_EQUAL(0U, violations);
}

static void test_init_clears_whole_ram(void) {
  const lcd_model_t* model = lcd_model_state();
  size_t nonzero = 0U;
  size_t page;
  size_t column;
  size_t count;

  restore_settings();
  lcd_init();
  count = decode_since(0U);
  lcd_model_apply(decoded, count);
  for (page = 0U; page < RAM_PAGES; ++page) {
    for (column = 0U; column < RAM_COLUMNS; ++column) {
      if (model->ram[page][column] != 0U) {
        ++nonzero;
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, nonzero);
  TEST_ASSERT(model->is_display_on);
  TEST_ASSERT(count > 0U);
  TEST_ASSERT(!decoded[count - 1U].is_data);
  TEST_ASSERT_EQUAL(0xAFU, decoded[count - 1U].value);
  TEST_ASSERT_EQUAL(0U, model->ram_overflow_count);
  TEST_ASSERT_EQUAL(0U, model->unknown_command_count);
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_CS));
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_RST));
}

static void test_contrast_is_clamped_and_stored(void) {
  static const uint8_t levels[][2] = {{0U, 0U}, {1U, 1U}, {63U, 63U}, {64U, 63U}, {200U, 63U}, {255U, 63U}};
  size_t i;

  for (i = 0U; i < sizeof(levels) / sizeof(levels[0]); ++i) {
    const uint8_t commands[] = {0x81U, levels[i][1]};
    restore_settings();
    lcd_set_contrast(levels[i][0]);
    check_commands(0U, commands, sizeof(commands));
    reset_bus();
    lcd_init();
    are_bytes_equal(decode_since(0U), build_init_stream(LCD_ORIENTATION_NORMAL, levels[i][1], false, false));
  }
  restore_settings();
}

static void test_display_on_off(void) {
  static const uint8_t on[] = {0xAFU};
  static const uint8_t off[] = {0xAEU};
  restore_settings();
  lcd_set_display_on(false);
  check_commands(0U, off, sizeof(off));
  reset_bus();
  lcd_set_display_on(true);
  check_commands(0U, on, sizeof(on));
}

static void test_inverted_and_all_pixels(void) {
  static const uint8_t inverse_on[] = {0xA7U};
  static const uint8_t inverse_off[] = {0xA6U};
  static const uint8_t all_on[] = {0xA5U};
  static const uint8_t all_off[] = {0xA4U};
  restore_settings();
  lcd_set_inverted(true);
  check_commands(0U, inverse_on, sizeof(inverse_on));
  reset_bus();
  lcd_set_inverted(false);
  check_commands(0U, inverse_off, sizeof(inverse_off));
  reset_bus();
  lcd_set_all_pixels_on(true);
  check_commands(0U, all_on, sizeof(all_on));
  reset_bus();
  lcd_set_all_pixels_on(false);
  check_commands(0U, all_off, sizeof(all_off));
}

static void test_orientation_commands_and_offsets(void) {
  static const uint8_t rotated[] = {LCD_SEG_CMD_ROTATED, LCD_COM_CMD_ROTATED};
  static const uint8_t normal[] = {LCD_SEG_CMD_NORMAL, LCD_COM_CMD_NORMAL};
  size_t count;

  restore_settings();
  lcd_init();
  lcd_flush();
  reset_bus();
  lcd_set_orientation(LCD_ORIENTATION_ROTATED);
  check_commands(0U, rotated, sizeof(rotated));
  TEST_ASSERT_EQUAL(0xFFU, lcd_fb_dirty_mask());
  lcd_flush();

  reset_bus();
  write_buffer[0] = 0x99U;
  lcd_write(3U, 0U, write_buffer, 1U);
  count = decode_since(0U);
  count = (count == 4U) ? count : 0U;
  TEST_ASSERT_EQUAL(4U, count);
  if (count == 4U) {
    TEST_ASSERT_EQUAL(0xB3U, decoded[0].value);
    TEST_ASSERT_EQUAL(LCD_COLUMN_OFFSET_ROTATED & 0x0FU, decoded[1].value);
    TEST_ASSERT_EQUAL(0x10U | (LCD_COLUMN_OFFSET_ROTATED >> 4), decoded[2].value);
  }

  /* Unknown values change nothing and send nothing. */
  reset_bus();
  lcd_set_orientation(2U);
  lcd_set_orientation(0xFFU);
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_event_count());
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  lcd_write(0U, 127U, write_buffer, 1U);
  count = decode_since(0U);
  TEST_ASSERT_EQUAL(4U, count);
  if (count == 4U) {
    TEST_ASSERT_EQUAL((127U + LCD_COLUMN_OFFSET_ROTATED) & 0x0FU, decoded[1].value);
    TEST_ASSERT_EQUAL(0x10U | ((127U + LCD_COLUMN_OFFSET_ROTATED) >> 4), decoded[2].value);
  }

  reset_bus();
  lcd_set_orientation(LCD_ORIENTATION_NORMAL);
  check_commands(0U, normal, sizeof(normal));
  TEST_ASSERT_EQUAL(0xFFU, lcd_fb_dirty_mask());
  lcd_flush();
}

static void test_sleep_and_wake(void) {
  static const uint8_t sleep[] = {0xAEU, 0xA5U};
  static const uint8_t wake_normal[] = {0xA4U, 0xAFU};
  static const uint8_t wake_all_on[] = {0xA5U, 0xAFU};

  restore_settings();
  lcd_sleep();
  check_commands(0U, sleep, sizeof(sleep));
  reset_bus();
  lcd_wake();
  check_commands(0U, wake_normal, sizeof(wake_normal));

  lcd_set_all_pixels_on(true);
  reset_bus();
  lcd_sleep();
  check_commands(0U, sleep, sizeof(sleep));
  reset_bus();
  lcd_wake();
  check_commands(0U, wake_all_on, sizeof(wake_all_on));
  restore_settings();
}

static void check_write(uint8_t page, uint8_t column, uint8_t length, size_t expected_data) {
  size_t count = 0U;
  size_t i;

  reset_bus();
  lcd_write(page, column, write_buffer, length);
  count = add_byte(count, false, (uint8_t)(0xB0U | page));
  count = add_byte(count, false, (uint8_t)(column & 0x0FU));
  count = add_byte(count, false, (uint8_t)(0x10U | (column >> 4)));
  for (i = 0U; i < expected_data; ++i) {
    count = add_byte(count, true, write_buffer[i]);
  }
  are_bytes_equal(decode_since(0U), count);
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_CS));
}

static void test_write_clipping(void) {
  size_t i;
  uint8_t dirty;

  restore_settings();
  lcd_init();
  lcd_flush();
  for (i = 0U; i < sizeof(write_buffer); ++i) {
    write_buffer[i] = (uint8_t)(i + 1U);
  }
  dirty = lcd_fb_dirty_mask();

  reset_bus();
  lcd_write(8U, 0U, write_buffer, 1U);
  lcd_write(255U, 0U, write_buffer, 1U);
  lcd_write(0U, 128U, write_buffer, 1U);
  lcd_write(0U, 255U, write_buffer, 1U);
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_event_count());

  check_write(0U, 0U, 128U, 128U);
  check_write(7U, 0U, 200U, 128U);
  check_write(5U, 120U, 20U, 8U);
  check_write(1U, 127U, 1U, 1U);
  check_write(1U, 127U, 255U, 1U);
  check_write(2U, 17U, 0U, 0U);
  TEST_ASSERT_EQUAL(dirty, lcd_fb_dirty_mask());
}

static size_t flush_counting_steps(void) {
  size_t steps = 0U;
  while (lcd_has_pending_flush() && (steps < 1000U)) {
    lcd_flush_step();
    ++steps;
  }
  return steps;
}

static void check_flush_for_orientation(uint8_t orientation) {
  const lcd_model_t* model = lcd_model_state();
  size_t page;
  size_t column;
  size_t stray = 0U;

  restore_settings();
  lcd_set_orientation(orientation);
  draw_pattern(0x1234U + orientation);
  reset_bus();
  lcd_init();
  TEST_ASSERT_EQUAL(LCD_PAGE_COUNT * CHUNKS_PER_PAGE, flush_counting_steps());
  TEST_ASSERT(!lcd_has_pending_flush());
  apply_since(0U);
  check_model_matches_framebuffer(orientation);

  /* Columns outside the visible window and the icon page keep the cleared value. */
  for (page = 0U; page < RAM_PAGES; ++page) {
    for (column = 0U; column < RAM_COLUMNS; ++column) {
      const bool is_visible = (page < LCD_PAGE_COUNT) && (column >= column_offset(orientation)) &&
                              (column < (column_offset(orientation) + LCD_WIDTH));
      if (!is_visible && (model->ram[page][column] != 0U)) {
        ++stray;
      }
    }
  }
  TEST_ASSERT_EQUAL(0U, stray);
  restore_settings();
}

static void test_flush_matches_framebuffer(void) {
  check_flush_for_orientation(LCD_ORIENTATION_NORMAL);
  check_flush_for_orientation(LCD_ORIENTATION_ROTATED);
}

static void test_flush_step_sends_one_chunk(void) {
  size_t count;
  size_t data_count = 0U;
  size_t i;

  restore_settings();
  lcd_fb_clear();
  lcd_init();
  lcd_flush();
  lcd_fb_set_pixel(0, 9, LCD_PIXEL_ON);
  reset_bus();
  lcd_flush_step();
  count = decode_since(0U);
  TEST_ASSERT_EQUAL(3U + LCD_FLUSH_CHUNK_COLUMNS, count);
  TEST_ASSERT_EQUAL(0xB1U, decoded[0].value);
  for (i = 3U; i < count; ++i) {
    data_count += decoded[i].is_data ? 1U : 0U;
  }
  TEST_ASSERT_EQUAL(LCD_FLUSH_CHUNK_COLUMNS, data_count);
  /* One CS transaction per step. */
  data_count = 0U;
  for (i = 0U; i < fake_lcd_pins_event_count(); ++i) {
    const fake_lcd_pin_event_t* event = fake_lcd_pins_event(i);
    data_count += ((event->pin == FAKE_LCD_PIN_CS) && (event->level == 0U)) ? 1U : 0U;
  }
  TEST_ASSERT_EQUAL(1U, data_count);
  lcd_flush();
}

static void test_page_redrawn_during_flush_is_sent_again(void) {
  size_t page_starts = 0U;
  size_t start;
  size_t count;
  size_t i;

  restore_settings();
  lcd_fb_clear();
  lcd_init();
  lcd_flush();
  apply_since(0U);

  start = fake_lcd_pins_event_count();
  lcd_fb_set_pixel(3, 2, LCD_PIXEL_ON);
  lcd_flush_step();
  TEST_ASSERT_EQUAL(0U, lcd_fb_dirty_mask());
  /* Redraw a column that this page transfer has already sent. */
  lcd_fb_set_pixel(3, 4, LCD_PIXEL_ON);
  TEST_ASSERT_EQUAL(0x01U, lcd_fb_dirty_mask());
  lcd_flush();
  count = decode_since(start);
  lcd_model_apply(decoded, count);
  check_model_matches_framebuffer(LCD_ORIENTATION_NORMAL);
  for (i = 0U; i < count; ++i) {
    page_starts += (!decoded[i].is_data && (decoded[i].value == 0xB0U)) ? 1U : 0U;
  }
  TEST_ASSERT_EQUAL(2U * CHUNKS_PER_PAGE, page_starts);
}

static void test_round_robin_is_fair(void) {
  size_t step;
  size_t sent = 0U;
  uint8_t pages[2U * LCD_PAGE_COUNT * CHUNKS_PER_PAGE];

  restore_settings();
  lcd_init();
  lcd_flush();
  /* Keep every page dirty: each page must still get its turn in order 0..7. */
  for (step = 0U; step < sizeof(pages); ++step) {
    size_t start = fake_lcd_pins_event_count();
    size_t count;
    lcd_fb_mark_all_dirty();
    lcd_flush_step();
    count = decode_since(start);
    if (TEST_ASSERT(count >= 3U)) {
      pages[sent] = (uint8_t)(decoded[0].value & 0x0FU);
      ++sent;
    }
  }
  for (step = 0U; step < sent; ++step) {
    TEST_ASSERT_EQUAL((step / CHUNKS_PER_PAGE) % LCD_PAGE_COUNT, pages[step]);
  }
  lcd_flush();
}

static void test_round_robin_starts_after_last_page(void) {
  size_t count;

  restore_settings();
  lcd_init();
  lcd_flush();
  lcd_fb_mark_dirty(5U);
  lcd_flush();
  lcd_fb_mark_dirty(2U);
  lcd_fb_mark_dirty(6U);
  reset_bus();
  lcd_flush_step();
  count = decode_since(0U);
  TEST_ASSERT(count >= 1U);
  TEST_ASSERT_EQUAL(0xB6U, decoded[0].value);
  lcd_flush();
}

static void test_no_bus_activity_when_clean(void) {
  restore_settings();
  lcd_init();
  lcd_flush();
  reset_bus();
  TEST_ASSERT(!lcd_has_pending_flush());
  lcd_flush_step();
  lcd_flush();
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_event_count());
}

static void test_init_keeps_framebuffer_and_cancels_flush(void) {
  uint8_t copy[LCD_PAGE_COUNT][LCD_WIDTH];
  uint8_t page;
  size_t count;

  restore_settings();
  draw_pattern(0xBEEFU);
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    memcpy(copy[page], lcd_fb_page(page), LCD_WIDTH);
  }
  lcd_init();
  lcd_flush_step();
  lcd_flush_step();
  lcd_flush_step();
  reset_bus();
  lcd_init();
  TEST_ASSERT_EQUAL(0xFFU, lcd_fb_dirty_mask());
  TEST_ASSERT(lcd_has_pending_flush());
  for (page = 0U; page < LCD_PAGE_COUNT; ++page) {
    TEST_ASSERT(memcmp(copy[page], lcd_fb_page(page), LCD_WIDTH) == 0);
  }
  /* The next transfer starts again at page 0, column 0. */
  reset_bus();
  lcd_flush_step();
  count = decode_since(0U);
  TEST_ASSERT(count >= 3U);
  TEST_ASSERT_EQUAL(0xB0U, decoded[0].value);
  TEST_ASSERT_EQUAL(0x00U, decoded[1].value);
  TEST_ASSERT_EQUAL(0x10U, decoded[2].value);
  lcd_flush();
}

/* Replays the original firmware RAM write from lcd_dump.sr and compares every byte. */
static void test_golden_capture(void) {
  size_t count = 0U;
  size_t i;

  restore_settings();
  lcd_init();
  lcd_flush();
  memcpy(write_buffer, lcd_dump_data, LCD_DUMP_DATA_COUNT);
  reset_bus();
  lcd_write(LCD_DUMP_PAGE, LCD_DUMP_COLUMN, write_buffer, LCD_DUMP_DATA_COUNT);
  for (i = 0U; i < LCD_DUMP_COMMAND_COUNT; ++i) {
    count = add_byte(count, false, lcd_dump_commands[i]);
  }
  for (i = 0U; i < LCD_DUMP_DATA_COUNT; ++i) {
    count = add_byte(count, true, lcd_dump_data[i]);
  }
  are_bytes_equal(decode_since(0U), count);
}

int main(void) {
  TEST_RUN(test_nothing_pending_before_init);
  TEST_RUN(test_init_stream_defaults);
  TEST_RUN(test_init_stream_uses_stored_settings);
  TEST_RUN(test_init_delays);
  TEST_RUN(test_init_clears_whole_ram);
  TEST_RUN(test_contrast_is_clamped_and_stored);
  TEST_RUN(test_display_on_off);
  TEST_RUN(test_inverted_and_all_pixels);
  TEST_RUN(test_orientation_commands_and_offsets);
  TEST_RUN(test_sleep_and_wake);
  TEST_RUN(test_write_clipping);
  TEST_RUN(test_flush_matches_framebuffer);
  TEST_RUN(test_flush_step_sends_one_chunk);
  TEST_RUN(test_page_redrawn_during_flush_is_sent_again);
  TEST_RUN(test_round_robin_is_fair);
  TEST_RUN(test_round_robin_starts_after_last_page);
  TEST_RUN(test_no_bus_activity_when_clean);
  TEST_RUN(test_init_keeps_framebuffer_and_cancels_flush);
  TEST_RUN(test_golden_capture);
  return TEST_FINISH();
}
