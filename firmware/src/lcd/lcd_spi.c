/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lcd/lcd_spi.h"

#include "board/lcd_pins.h"

/* Software copy of CS, so that a second select changes only A0. */
static bool lcd_spi_is_selected;

void lcd_spi_init(void) {
  /* The pins are still high-Z here, so the outputs start at idle levels with the controller in reset. */
  LCD_CS_HIGH();
  LCD_SCK_HIGH();
  LCD_A0_HIGH();
  LCD_SDA_WRITE(0);
  LCD_RST_LOW();
  LCD_PINS_CONFIGURE_OUTPUTS();
  lcd_spi_is_selected = false;
}

void lcd_spi_set_reset(bool is_asserted) {
  if (is_asserted) {
    LCD_RST_LOW();
  } else {
    LCD_RST_HIGH();
  }
}

void lcd_spi_select(uint8_t kind) {
  if (kind == LCD_SPI_COMMAND) {
    LCD_A0_LOW();
  } else {
    LCD_A0_HIGH();
  }
  if (!lcd_spi_is_selected) {
    LCD_CS_LOW();
    lcd_spi_is_selected = true;
  }
  /* A0 setup and CS fall to the first SCK fall. */
  LCD_PACE();
}

void lcd_spi_deselect(void) {
  /* Last SCK rise to CS rise. */
  LCD_PACE();
  LCD_CS_HIGH();
  lcd_spi_is_selected = false;
}

void lcd_spi_write_byte(uint8_t value) {
  uint8_t bit_count = 8U;
  do {
    /* SDA changes only while SCK is low, so the controller never samples a changing bit. */
    LCD_SCK_LOW();
    /* The bool cast compiles to RLC A + MOV P2.4,C on SDCC. */
    LCD_SDA_WRITE((bool)(value & 0x80U));
    LCD_PACE();
    LCD_SCK_HIGH();
    value = (uint8_t)(value << 1);
    LCD_PACE();
    --bit_count;
  } while (bit_count != 0U);
}

void lcd_spi_write_xdata(const MEM_XDATA uint8_t* data, uint8_t length) {
  while (length != 0U) {
    lcd_spi_write_byte(*data);
    ++data;
    --length;
  }
}

void lcd_spi_write_code(const MEM_CODE uint8_t* data, uint8_t length) {
  while (length != 0U) {
    lcd_spi_write_byte(*data);
    ++data;
    --length;
  }
}

void lcd_spi_write_repeat(uint8_t value, uint8_t count) {
  while (count != 0U) {
    lcd_spi_write_byte(value);
    --count;
  }
}
