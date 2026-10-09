/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lcd/lcd.h"

#include "board/lcd_config.h"
#include "delay/delay.h"
#include "lcd/lcd_fb.h"
#include "lcd/lcd_spi.h"
#include "lcd_commands.h"

#define LCD_NO_PAGE 0xFFU

/* Fixed parts of the init sequence; lcd_init() sends the orientation and contrast bytes between them. */
#define LCD_INIT_BIAS_INDEX 0U
#define LCD_INIT_MIDDLE_INDEX 1U
#define LCD_INIT_MIDDLE_LENGTH 3U
static const MEM_CODE uint8_t lcd_init_commands[] = {
    LCD_BIAS_CMD,
    LCD_CMD_START_LINE_0,
    LCD_REGULATION_RATIO_CMD,
    LCD_CMD_SET_CONTRAST,
};

/* SEG and COM direction commands, indexed by orientation. */
static const MEM_CODE uint8_t lcd_orientation_commands[2][2] = {
    {LCD_SEG_CMD_NORMAL, LCD_COM_CMD_NORMAL},
    {LCD_SEG_CMD_ROTATED, LCD_COM_CMD_ROTATED},
};

static uint8_t lcd_contrast = LCD_DEFAULT_CONTRAST;
static uint8_t lcd_orientation = LCD_ORIENTATION_NORMAL;
static bool lcd_is_inverted;
static bool lcd_is_all_on;

/* Page being sent, or LCD_NO_PAGE. */
static uint8_t lcd_flush_page = LCD_NO_PAGE;
static uint8_t lcd_flush_column;
/* Round-robin start: the scan for the next dirty page begins after this page. */
static uint8_t lcd_last_flushed_page = LCD_PAGE_COUNT - 1U;

static uint8_t lcd_column_offset(void) {
  return (lcd_orientation == LCD_ORIENTATION_ROTATED) ? LCD_COLUMN_OFFSET_ROTATED : LCD_COLUMN_OFFSET_NORMAL;
}

/* Sends the RAM address and leaves the bus selected for data. @p column is a raw controller column. */
static void lcd_select_ram(uint8_t page, uint8_t column) {
  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_byte((uint8_t)(LCD_CMD_PAGE_ADDRESS | page));
  lcd_spi_write_byte((uint8_t)(LCD_CMD_COLUMN_LOW | (column & 0x0FU)));
  lcd_spi_write_byte((uint8_t)(LCD_CMD_COLUMN_HIGH | (column >> 4)));
  lcd_spi_select(LCD_SPI_DATA);
}

static void lcd_send_command(uint8_t command) {
  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_byte(command);
  lcd_spi_deselect();
}

static void lcd_send_orientation_commands(void) { lcd_spi_write_code(lcd_orientation_commands[lcd_orientation], 2U); }

static uint8_t lcd_all_pixels_command(void) {
  return lcd_is_all_on ? LCD_CMD_ALL_PIXELS_ON : LCD_CMD_ALL_PIXELS_NORMAL;
}

static void lcd_clear_ram(void) {
  uint8_t page;
  for (page = 0U; page < LCD_RAM_PAGE_COUNT; ++page) {
    lcd_select_ram(page, 0U);
    lcd_spi_write_repeat(0U, LCD_RAM_COLUMN_COUNT);
  }
}

void lcd_init(void) {
  lcd_spi_init();
  delay_ms(LCD_RESET_PULSE_MS);
  lcd_spi_set_reset(false);
  delay_ms(LCD_RESET_WAIT_MS);

  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_code(&lcd_init_commands[LCD_INIT_BIAS_INDEX], 1U);
  lcd_send_orientation_commands();
  lcd_spi_write_code(&lcd_init_commands[LCD_INIT_MIDDLE_INDEX], LCD_INIT_MIDDLE_LENGTH);
  lcd_spi_write_byte(lcd_contrast);
  lcd_spi_write_byte(LCD_CMD_POWER_ALL_ON);
  lcd_spi_deselect();
  /* The booster and regulator need time to reach the LCD drive voltage. */
  delay_ms(LCD_POWER_SETTLE_MS);

  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_byte(lcd_is_inverted ? LCD_CMD_INVERSE_ON : LCD_CMD_INVERSE_OFF);
  lcd_spi_write_byte(lcd_all_pixels_command());
  /* The RAM holds random data after power-on, including the columns and the icon page that are not shown. */
  lcd_clear_ram();
  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_byte(LCD_CMD_DISPLAY_ON);
  lcd_spi_deselect();

  lcd_flush_page = LCD_NO_PAGE;
  lcd_last_flushed_page = LCD_PAGE_COUNT - 1U;
  lcd_fb_mark_all_dirty();
}

void lcd_set_display_on(bool is_on) { lcd_send_command(is_on ? LCD_CMD_DISPLAY_ON : LCD_CMD_DISPLAY_OFF); }

void lcd_set_contrast(uint8_t level) {
  if (level > LCD_CONTRAST_MAX) {
    level = LCD_CONTRAST_MAX;
  }
  lcd_contrast = level;
  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_byte(LCD_CMD_SET_CONTRAST);
  lcd_spi_write_byte(level);
  lcd_spi_deselect();
}

void lcd_set_inverted(bool is_inverted) {
  lcd_is_inverted = is_inverted;
  lcd_send_command(is_inverted ? LCD_CMD_INVERSE_ON : LCD_CMD_INVERSE_OFF);
}

void lcd_set_all_pixels_on(bool is_all_on) {
  lcd_is_all_on = is_all_on;
  lcd_send_command(lcd_all_pixels_command());
}

void lcd_set_orientation(uint8_t orientation) {
  if (orientation > LCD_ORIENTATION_ROTATED) {
    return;
  }
  lcd_orientation = orientation;
  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_send_orientation_commands();
  lcd_spi_deselect();
  /* The visible column window moves with the orientation, so every page must be sent again. */
  lcd_fb_mark_all_dirty();
}

void lcd_sleep(void) {
  /* ST756x enters power save on display off followed by all pixels on. */
  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_byte(LCD_CMD_DISPLAY_OFF);
  lcd_spi_write_byte(LCD_CMD_ALL_PIXELS_ON);
  lcd_spi_deselect();
}

void lcd_wake(void) {
  lcd_spi_select(LCD_SPI_COMMAND);
  lcd_spi_write_byte(lcd_all_pixels_command());
  lcd_spi_write_byte(LCD_CMD_DISPLAY_ON);
  lcd_spi_deselect();
}

void lcd_write(uint8_t page, uint8_t column, const MEM_XDATA uint8_t* data, uint8_t length) {
  uint8_t room;

  if ((page >= LCD_PAGE_COUNT) || (column >= LCD_WIDTH)) {
    return;
  }
  room = (uint8_t)(LCD_WIDTH - column);
  if (length > room) {
    length = room;
  }
  lcd_select_ram(page, (uint8_t)(column + lcd_column_offset()));
  lcd_spi_write_xdata(data, length);
  lcd_spi_deselect();
}

static void lcd_start_next_page(void) {
  uint8_t dirty_pages = lcd_fb_dirty_mask();
  uint8_t page = lcd_last_flushed_page;

  if (dirty_pages == 0U) {
    return;
  }
  do {
    page = (uint8_t)((page + 1U) & (LCD_PAGE_COUNT - 1U));
  } while ((dirty_pages & (uint8_t)(1U << page)) == 0U);
  lcd_flush_page = page;
  lcd_flush_column = 0U;
  lcd_last_flushed_page = page;
  /* Clear before sending: a page redrawn during its transfer stays dirty and is sent again. */
  lcd_fb_clear_dirty(page);
}

void lcd_flush_step(void) {
  if (lcd_flush_page == LCD_NO_PAGE) {
    lcd_start_next_page();
    if (lcd_flush_page == LCD_NO_PAGE) {
      return;
    }
  }
  lcd_write(lcd_flush_page, lcd_flush_column, &lcd_fb_page(lcd_flush_page)[lcd_flush_column], LCD_FLUSH_CHUNK_COLUMNS);
  lcd_flush_column = (uint8_t)(lcd_flush_column + LCD_FLUSH_CHUNK_COLUMNS);
  if (lcd_flush_column >= LCD_WIDTH) {
    lcd_flush_page = LCD_NO_PAGE;
  }
}

bool lcd_has_pending_flush(void) {
  if (lcd_flush_page != LCD_NO_PAGE) {
    return true;
  }
  return (bool)lcd_fb_dirty_mask();
}

void lcd_flush(void) {
  while (lcd_has_pending_flush()) {
    lcd_flush_step();
  }
}
