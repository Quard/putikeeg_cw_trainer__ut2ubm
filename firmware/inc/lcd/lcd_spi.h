/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LCD_LCD_SPI_H
#define LCD_LCD_SPI_H

/*
 * Bit-banged, write-only SPI to the LCD controller: mode 3 (SCK idles high, data sampled on the rising edge),
 * MSB first. Interrupts stay enabled; an interrupt only stretches a clock phase.
 * Call these functions from the main loop only. They are not reentrant.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#define LCD_SPI_COMMAND 0U /* A0 low */
#define LCD_SPI_DATA 1U    /* A0 high */

/** Sets the idle pin levels with the controller held in reset, then enables the pin outputs. */
void lcd_spi_init(void);

/** Drives RST low when @p is_asserted is true, high otherwise. */
void lcd_spi_set_reset(bool is_asserted);

/**
 * Sets A0 from @p kind and pulls CS low. When CS is already low, only A0 changes.
 * Any @p kind other than LCD_SPI_COMMAND counts as LCD_SPI_DATA.
 */
void lcd_spi_select(uint8_t kind);

/** Releases CS. SCK stays high. */
void lcd_spi_deselect(void);

/** Sends one byte. Call only while selected. */
void lcd_spi_write_byte(uint8_t value);

/** Sends @p length bytes from xdata. A length of 0 sends nothing. */
void lcd_spi_write_xdata(const MEM_XDATA uint8_t* data, uint8_t length);

/** Sends @p length bytes from code memory. A length of 0 sends nothing. */
void lcd_spi_write_code(const MEM_CODE uint8_t* data, uint8_t length);

/** Sends @p value @p count times. A count of 0 sends nothing. */
void lcd_spi_write_repeat(uint8_t value, uint8_t count);

#endif /* LCD_LCD_SPI_H */
