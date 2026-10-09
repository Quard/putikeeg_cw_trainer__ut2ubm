/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

/*
 * QA golden-capture test. The reference is lcd_dump.sr decoded by QA from the raw samples (qa/qa_lcd_dump_edges.h)
 * with the QA bus oracle (qa/qa_bus.h). It does not use fixtures/lcd_dump_capture.h or the developer decoder as the
 * reference; it only cross-checks them.
 */
#include <string.h>

#include "fake_delay.h"
#include "fake_lcd_pins.h"
#include "fixtures/lcd_dump_capture.h"
#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "lcd_protocol_decoder.h"
#include "qa/qa_bus.h"
#include "qa/qa_lcd_dump_edges.h"
#include "support/test_assert.h"

#define CAPACITY 512U
#define CAPTURE_COMMAND_COUNT 3U

static qa_byte_t capture_bytes[CAPACITY];
static size_t capture_count;
static uint8_t capture_page;
static uint8_t capture_column;
static uint8_t capture_data[LCD_WIDTH];
static size_t capture_data_count;

static qa_byte_t driver_bytes[CAPACITY];
static lcd_decoded_byte_t developer_bytes[CAPACITY];

static void decode_capture(void) {
  qa_bus_t bus;
  size_t i;

  qa_bus_start(&bus, qa_dump_edges[0].levels, capture_bytes, CAPACITY);
  for (i = 1U; i < QA_DUMP_EDGE_COUNT; ++i) {
    qa_bus_apply_sample(&bus, qa_dump_edges[i].levels);
  }
  capture_count = bus.byte_count;
  TEST_ASSERT_EQUAL(0U, qa_rules_total(&bus.rules));
  TEST_ASSERT_EQUAL(91U, bus.cs_fall_count);
  TEST_ASSERT_EQUAL(0U, bus.bit_count);
  TEST_ASSERT_EQUAL(QA_ALL_HIGH & (uint8_t)~QA_BIT_SDA, bus.levels & (uint8_t)~QA_BIT_SDA);
}

/* The capture holds 3 address commands then one data run, so page, column and length come from the bytes. */
static void test_capture_decodes_to_one_ram_run(void) {
  size_t i;

  decode_capture();
  if (!TEST_ASSERT_EQUAL(91U, capture_count)) {
    return;
  }
  for (i = 0U; i < capture_count; ++i) {
    TEST_ASSERT_EQUAL(i >= CAPTURE_COMMAND_COUNT, capture_bytes[i].is_data);
  }
  TEST_ASSERT_EQUAL(0xB2U, capture_bytes[0].value);
  TEST_ASSERT_EQUAL(0x08U, capture_bytes[1].value);
  TEST_ASSERT_EQUAL(0x12U, capture_bytes[2].value);

  capture_page = (uint8_t)(capture_bytes[0].value & 0x0FU);
  capture_column = (uint8_t)((capture_bytes[1].value & 0x0FU) | ((capture_bytes[2].value & 0x0FU) << 4));
  capture_data_count = capture_count - CAPTURE_COMMAND_COUNT;
  TEST_ASSERT_EQUAL(2U, capture_page);
  TEST_ASSERT_EQUAL(40U, capture_column);
  TEST_ASSERT_EQUAL(88U, capture_data_count);
  /* The run ends exactly at the last visible column: it fits the 128-column window at offset 0. */
  TEST_ASSERT_EQUAL(LCD_WIDTH, capture_column + capture_data_count);
  for (i = 0U; i < capture_data_count; ++i) {
    capture_data[i] = capture_bytes[CAPTURE_COMMAND_COUNT + i].value;
  }
}

/* The developer fixture must hold the same bytes as the QA extraction. */
static void test_developer_fixture_matches_capture(void) {
  size_t i;

  TEST_ASSERT_EQUAL(capture_page, LCD_DUMP_PAGE);
  TEST_ASSERT_EQUAL(capture_column, LCD_DUMP_COLUMN);
  TEST_ASSERT_EQUAL(capture_data_count, LCD_DUMP_DATA_COUNT);
  for (i = 0U; i < CAPTURE_COMMAND_COUNT; ++i) {
    TEST_ASSERT_EQUAL(capture_bytes[i].value, lcd_dump_commands[i]);
  }
  for (i = 0U; (i < capture_data_count) && (i < LCD_DUMP_DATA_COUNT); ++i) {
    TEST_ASSERT_EQUAL(capture_data[i], lcd_dump_data[i]);
  }
}

/* Real traffic from the original firmware replayed through the developer decoder: same bytes, no violations. */
static void test_developer_decoder_accepts_real_traffic(void) {
  uint8_t levels = qa_dump_edges[0].levels;
  size_t violations = 99U;
  size_t count;
  size_t i;
  unsigned pin;

  fake_lcd_pins_reset();
  for (pin = 0U; pin < (unsigned)FAKE_LCD_PIN_COUNT; ++pin) {
    fake_lcd_pins_write((fake_lcd_pin_t)pin, (uint8_t)((levels >> pin) & 1U));
  }
  for (i = 1U; i < QA_DUMP_EDGE_COUNT; ++i) {
    const uint8_t next = qa_dump_edges[i].levels;
    const uint8_t changed = (uint8_t)(levels ^ next);
    const bool is_cs_falling = ((changed & QA_BIT_CS) != 0U) && ((next & QA_BIT_CS) == 0U);
    if (((changed & QA_BIT_CS) != 0U) && !is_cs_falling) {
      fake_lcd_pins_write(FAKE_LCD_PIN_CS, 1U);
    }
    for (pin = 1U; pin < (unsigned)FAKE_LCD_PIN_COUNT; ++pin) {
      if ((changed & (1U << pin)) != 0U) {
        fake_lcd_pins_write((fake_lcd_pin_t)pin, (uint8_t)((next >> pin) & 1U));
      }
    }
    if (is_cs_falling) {
      fake_lcd_pins_write(FAKE_LCD_PIN_CS, 0U);
    }
    levels = next;
  }
  count = lcd_protocol_decode(0U, fake_lcd_pins_event_count(), developer_bytes, CAPACITY, &violations);
  TEST_ASSERT_EQUAL(0U, violations);
  if (TEST_ASSERT_EQUAL(capture_count, count)) {
    for (i = 0U; i < count; ++i) {
      TEST_ASSERT_EQUAL(capture_bytes[i].is_data, developer_bytes[i].is_data);
      TEST_ASSERT_EQUAL(capture_bytes[i].value, developer_bytes[i].value);
    }
  }
}

/* lcd_write() at the captured address must put the captured bytes on the bus, in order and with the same A0. */
static void test_driver_reproduces_capture(void) {
  static uint8_t data[LCD_WIDTH];
  qa_bus_t bus;
  size_t start;
  size_t violations = 99U;
  size_t i;

  fake_lcd_pins_reset();
  fake_delay_reset();
  lcd_set_orientation(LCD_ORIENTATION_NORMAL);
  lcd_init();
  lcd_flush();
  memset(data, 0xEE, sizeof(data));
  memcpy(data, capture_data, capture_data_count);

  start = fake_lcd_pins_event_count();
  qa_bus_start(&bus, QA_ALL_HIGH, NULL, 0U);
  qa_bus_feed_events(&bus, 0U, start);
  TEST_ASSERT_EQUAL(0U, qa_rules_total(&bus.rules));
  qa_bus_start(&bus, bus.levels, driver_bytes, CAPACITY);
  lcd_write(capture_page, capture_column, data, (uint8_t)capture_data_count);
  qa_bus_feed_events(&bus, start, fake_lcd_pins_event_count());

  TEST_ASSERT_EQUAL(0U, qa_rules_total(&bus.rules));
  /* One CS transaction for the whole run. */
  TEST_ASSERT_EQUAL(1U, bus.cs_fall_count);
  if (TEST_ASSERT_EQUAL(capture_count, bus.byte_count)) {
    for (i = 0U; i < capture_count; ++i) {
      if (!TEST_ASSERT_EQUAL(capture_bytes[i].is_data, driver_bytes[i].is_data) ||
          !TEST_ASSERT_EQUAL(capture_bytes[i].value, driver_bytes[i].value)) {
        fprintf(stderr, "  first difference at byte %zu\n", i);
        break;
      }
    }
  }
  (void)lcd_protocol_decode(start, fake_lcd_pins_event_count(), developer_bytes, CAPACITY, &violations);
  TEST_ASSERT_EQUAL(0U, violations);
  /* Idle after the write: CS high, SCK high, RST high. */
  TEST_ASSERT_EQUAL(QA_BIT_CS | QA_BIT_SCK | QA_BIT_RST, bus.levels & (QA_BIT_CS | QA_BIT_SCK | QA_BIT_RST));
}

/* A length past the window is cut at column 127, so the captured run is also produced by a too-long request. */
static void test_driver_clips_capture_run_at_window_end(void) {
  static uint8_t data[255];
  qa_bus_t bus;
  size_t start;

  fake_lcd_pins_reset();
  memset(data, 0x3C, sizeof(data));
  memcpy(data, capture_data, capture_data_count);
  start = fake_lcd_pins_event_count();
  qa_bus_start(&bus, QA_ALL_HIGH, driver_bytes, CAPACITY);
  lcd_write(capture_page, capture_column, data, 255U);
  qa_bus_feed_events(&bus, start, fake_lcd_pins_event_count());
  TEST_ASSERT_EQUAL(capture_count, bus.byte_count);
  TEST_ASSERT_EQUAL(0U, qa_rules_total(&bus.rules));
}

int main(void) {
  TEST_RUN(test_capture_decodes_to_one_ram_run);
  TEST_RUN(test_developer_fixture_matches_capture);
  TEST_RUN(test_developer_decoder_accepts_real_traffic);
  TEST_RUN(test_driver_reproduces_capture);
  TEST_RUN(test_driver_clips_capture_run_at_window_end);
  return TEST_FINISH();
}
