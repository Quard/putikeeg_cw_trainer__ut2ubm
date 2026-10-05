#include <string.h>

#include "fake_delay.h"
#include "fake_lcd_pins.h"
#include "lcd/lcd_spi.h"
#include "lcd_protocol_decoder.h"
#include "support/test_assert.h"

#define DECODE_CAPACITY 1024U

static lcd_decoded_byte_t decoded[DECODE_CAPACITY];
static uint8_t xdata_buffer[255];
static const uint8_t code_buffer[255] = {0x11U, 0x22U, 0x33U};

/* Events per sent bit: SCK low, SDA, SCK high. */
#define EVENTS_PER_BIT 3U

static void setup(void) {
  fake_lcd_pins_reset();
  fake_delay_reset();
  lcd_spi_init();
}

static size_t decode_from(size_t first_event, size_t* violations) {
  return lcd_protocol_decode(first_event, fake_lcd_pins_event_count(), decoded, DECODE_CAPACITY, violations);
}

static void test_init_sets_latches_before_outputs(void) {
  size_t i;
  size_t marker_index = 0U;
  bool has_marker = false;

  fake_lcd_pins_reset();
  lcd_spi_init();

  for (i = 0U; i < fake_lcd_pins_event_count(); ++i) {
    if (fake_lcd_pins_event(i)->pin == FAKE_LCD_EVENT_CONFIGURE_OUTPUTS) {
      TEST_ASSERT(!has_marker);
      has_marker = true;
      marker_index = i;
    }
  }
  TEST_ASSERT(has_marker);
  TEST_ASSERT_EQUAL(fake_lcd_pins_event_count() - 1U, marker_index);
  TEST_ASSERT(fake_lcd_pins_are_outputs());
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_CS));
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_SCK));
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_A0));
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_level(FAKE_LCD_PIN_SDA));
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_level(FAKE_LCD_PIN_RST));
  /* CS goes high before RST goes low, so the controller never sees reset while selected. */
  TEST_ASSERT_EQUAL(FAKE_LCD_PIN_CS, fake_lcd_pins_event(0U)->pin);
}

static void test_init_twice_keeps_idle_state(void) {
  size_t violations = 99U;
  size_t start;

  setup();
  lcd_spi_select(LCD_SPI_DATA);
  lcd_spi_write_byte(0x42U);
  lcd_spi_deselect();
  start = fake_lcd_pins_event_count();
  lcd_spi_init();
  TEST_ASSERT_EQUAL(0U, decode_from(start, &violations));
  TEST_ASSERT_EQUAL(0U, violations);
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_level(FAKE_LCD_PIN_RST));
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_CS));
}

static void test_set_reset(void) {
  setup();
  lcd_spi_set_reset(false);
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_RST));
  lcd_spi_set_reset(true);
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_level(FAKE_LCD_PIN_RST));
  lcd_spi_set_reset(false);
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_RST));
}

static void test_bytes_are_sent_msb_first(void) {
  static const uint8_t values[] = {0x00U, 0xFFU, 0xA5U, 0x5AU, 0x01U, 0x80U};
  size_t violations = 99U;
  size_t start;
  size_t count;
  size_t i;

  setup();
  lcd_spi_set_reset(false);
  start = fake_lcd_pins_event_count();
  lcd_spi_select(LCD_SPI_DATA);
  for (i = 0U; i < sizeof(values); ++i) {
    lcd_spi_write_byte(values[i]);
  }
  lcd_spi_deselect();

  count = decode_from(start, &violations);
  TEST_ASSERT_EQUAL(sizeof(values), count);
  TEST_ASSERT_EQUAL(0U, violations);
  for (i = 0U; (i < count) && (i < sizeof(values)); ++i) {
    TEST_ASSERT_EQUAL(values[i], decoded[i].value);
    TEST_ASSERT(decoded[i].is_data);
  }
}

static void test_byte_has_exact_bit_pattern(void) {
  size_t start;
  size_t bit;

  setup();
  lcd_spi_set_reset(false);
  lcd_spi_select(LCD_SPI_DATA);
  start = fake_lcd_pins_event_count();
  lcd_spi_write_byte(0xA5U);
  TEST_ASSERT_EQUAL(8U * EVENTS_PER_BIT, fake_lcd_pins_event_count() - start);
  for (bit = 0U; bit < 8U; ++bit) {
    const fake_lcd_pin_event_t* sck_low = fake_lcd_pins_event(start + (bit * EVENTS_PER_BIT));
    const fake_lcd_pin_event_t* sda = fake_lcd_pins_event(start + (bit * EVENTS_PER_BIT) + 1U);
    const fake_lcd_pin_event_t* sck_high = fake_lcd_pins_event(start + (bit * EVENTS_PER_BIT) + 2U);
    TEST_ASSERT(sck_low->pin == FAKE_LCD_PIN_SCK);
    TEST_ASSERT_EQUAL(0U, sck_low->level);
    TEST_ASSERT(sda->pin == FAKE_LCD_PIN_SDA);
    TEST_ASSERT_EQUAL((0xA5U >> (7U - bit)) & 1U, sda->level);
    TEST_ASSERT(sck_high->pin == FAKE_LCD_PIN_SCK);
    TEST_ASSERT_EQUAL(1U, sck_high->level);
  }
  lcd_spi_deselect();
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_SCK));
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_CS));
}

static void test_select_sets_a0_from_kind(void) {
  size_t violations = 99U;
  size_t start;
  size_t count;

  setup();
  lcd_spi_set_reset(false);
  start = fake_lcd_pins_event_count();
  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_byte(0x10U);
  lcd_spi_deselect();
  lcd_spi_select(LCD_SPI_DATA);
  lcd_spi_write_byte(0x20U);
  lcd_spi_deselect();
  lcd_spi_select(7U);
  lcd_spi_write_byte(0x30U);
  lcd_spi_deselect();

  count = decode_from(start, &violations);
  TEST_ASSERT_EQUAL(3U, count);
  TEST_ASSERT_EQUAL(0U, violations);
  TEST_ASSERT(!decoded[0].is_data);
  TEST_ASSERT(decoded[1].is_data);
  TEST_ASSERT(decoded[2].is_data);
}

static void test_select_order_is_a0_then_cs(void) {
  size_t start;

  setup();
  lcd_spi_set_reset(false);
  start = fake_lcd_pins_event_count();
  lcd_spi_select(LCD_SPI_COMMAND);
  TEST_ASSERT_EQUAL(2U, fake_lcd_pins_event_count() - start);
  TEST_ASSERT(fake_lcd_pins_event(start)->pin == FAKE_LCD_PIN_A0);
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_event(start)->level);
  TEST_ASSERT(fake_lcd_pins_event(start + 1U)->pin == FAKE_LCD_PIN_CS);
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_event(start + 1U)->level);
  lcd_spi_deselect();
}

static void test_double_select_changes_only_a0(void) {
  size_t violations = 99U;
  size_t start;
  size_t select_start;
  size_t i;

  setup();
  lcd_spi_set_reset(false);
  start = fake_lcd_pins_event_count();
  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_byte(0xB0U);
  select_start = fake_lcd_pins_event_count();
  lcd_spi_select(LCD_SPI_DATA);
  for (i = select_start; i < fake_lcd_pins_event_count(); ++i) {
    TEST_ASSERT(fake_lcd_pins_event(i)->pin == FAKE_LCD_PIN_A0);
  }
  TEST_ASSERT_EQUAL(0U, fake_lcd_pins_level(FAKE_LCD_PIN_CS));
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_A0));
  lcd_spi_write_byte(0x55U);
  lcd_spi_deselect();

  TEST_ASSERT_EQUAL(2U, decode_from(start, &violations));
  TEST_ASSERT_EQUAL(0U, violations);
  TEST_ASSERT(!decoded[0].is_data);
  TEST_ASSERT(decoded[1].is_data);
}

static void test_deselect_keeps_sck_high(void) {
  setup();
  lcd_spi_set_reset(false);
  lcd_spi_select(LCD_SPI_DATA);
  lcd_spi_write_byte(0x00U);
  lcd_spi_deselect();
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_SCK));
  TEST_ASSERT_EQUAL(1U, fake_lcd_pins_level(FAKE_LCD_PIN_CS));
}

typedef void (*write_function_t)(uint8_t length);

static void write_xdata(uint8_t length) { lcd_spi_write_xdata(xdata_buffer, length); }
static void write_code(uint8_t length) { lcd_spi_write_code(code_buffer, length); }
static void write_repeat(uint8_t length) { lcd_spi_write_repeat(0xC3U, length); }

static void check_lengths(write_function_t write, const uint8_t* expected) {
  static const uint8_t lengths[] = {0U, 1U, 255U};
  size_t length_index;

  for (length_index = 0U; length_index < sizeof(lengths); ++length_index) {
    const uint8_t length = lengths[length_index];
    size_t violations = 99U;
    size_t start;
    size_t count;
    size_t i;

    setup();
    lcd_spi_set_reset(false);
    start = fake_lcd_pins_event_count();
    lcd_spi_select(LCD_SPI_DATA);
    write(length);
    lcd_spi_deselect();
    count = decode_from(start, &violations);
    TEST_ASSERT_EQUAL(length, count);
    TEST_ASSERT_EQUAL(0U, violations);
    for (i = 0U; (i < count) && (i < length); ++i) {
      TEST_ASSERT_EQUAL((expected != NULL) ? expected[i] : 0xC3U, decoded[i].value);
    }
  }
}

static void test_write_lengths(void) {
  size_t i;
  for (i = 0U; i < sizeof(xdata_buffer); ++i) {
    xdata_buffer[i] = (uint8_t)(i * 7U + 3U);
  }
  check_lengths(write_xdata, xdata_buffer);
  check_lengths(write_code, code_buffer);
  check_lengths(write_repeat, NULL);
}

int main(void) {
  memset(xdata_buffer, 0, sizeof(xdata_buffer));
  TEST_RUN(test_init_sets_latches_before_outputs);
  TEST_RUN(test_init_twice_keeps_idle_state);
  TEST_RUN(test_set_reset);
  TEST_RUN(test_bytes_are_sent_msb_first);
  TEST_RUN(test_byte_has_exact_bit_pattern);
  TEST_RUN(test_select_sets_a0_from_kind);
  TEST_RUN(test_select_order_is_a0_then_cs);
  TEST_RUN(test_double_select_changes_only_a0);
  TEST_RUN(test_deselect_keeps_sck_high);
  TEST_RUN(test_write_lengths);
  return TEST_FINISH();
}
