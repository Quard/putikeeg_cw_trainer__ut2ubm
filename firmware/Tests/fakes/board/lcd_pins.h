/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_LCD_PINS_H
#define BOARD_LCD_PINS_H

/* Host replacement for firmware/inc/board/lcd_pins.h. Every macro is one logged fake pin operation. */

#include "board/lcd_config.h"
#include "fake_lcd_pins.h"

#define LCD_CS_HIGH() fake_lcd_pins_write(FAKE_LCD_PIN_CS, 1U)
#define LCD_CS_LOW() fake_lcd_pins_write(FAKE_LCD_PIN_CS, 0U)
#define LCD_RST_HIGH() fake_lcd_pins_write(FAKE_LCD_PIN_RST, 1U)
#define LCD_RST_LOW() fake_lcd_pins_write(FAKE_LCD_PIN_RST, 0U)
#define LCD_A0_HIGH() fake_lcd_pins_write(FAKE_LCD_PIN_A0, 1U)
#define LCD_A0_LOW() fake_lcd_pins_write(FAKE_LCD_PIN_A0, 0U)
#define LCD_SCK_HIGH() fake_lcd_pins_write(FAKE_LCD_PIN_SCK, 1U)
#define LCD_SCK_LOW() fake_lcd_pins_write(FAKE_LCD_PIN_SCK, 0U)
#define LCD_SDA_WRITE(is_high) fake_lcd_pins_write(FAKE_LCD_PIN_SDA, (is_high) ? 1U : 0U)
#define LCD_PINS_CONFIGURE_OUTPUTS() fake_lcd_pins_configure_outputs()
#define LCD_PACE() ((void)0)

#endif /* BOARD_LCD_PINS_H */
