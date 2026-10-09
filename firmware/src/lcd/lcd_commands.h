/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef LCD_LCD_COMMANDS_H
#define LCD_LCD_COMMANDS_H

/* ST756x-family command bytes that do not need tuning. Not verified against a datasheet. */
#define LCD_CMD_COLUMN_LOW 0x00U
#define LCD_CMD_COLUMN_HIGH 0x10U
#define LCD_CMD_POWER_ALL_ON 0x2FU
#define LCD_CMD_START_LINE_0 0x40U
#define LCD_CMD_SET_CONTRAST 0x81U
#define LCD_CMD_ALL_PIXELS_NORMAL 0xA4U
#define LCD_CMD_ALL_PIXELS_ON 0xA5U
#define LCD_CMD_INVERSE_OFF 0xA6U
#define LCD_CMD_INVERSE_ON 0xA7U
#define LCD_CMD_DISPLAY_OFF 0xAEU
#define LCD_CMD_DISPLAY_ON 0xAFU
#define LCD_CMD_PAGE_ADDRESS 0xB0U

/* Controller RAM: 132 columns x 9 pages; page 8 holds the icon row. */
#define LCD_RAM_PAGE_COUNT 9U
#define LCD_RAM_COLUMN_COUNT 132U

#define LCD_CONTRAST_MAX 63U

#endif /* LCD_LCD_COMMANDS_H */
