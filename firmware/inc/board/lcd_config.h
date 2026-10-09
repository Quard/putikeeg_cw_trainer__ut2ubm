/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_LCD_CONFIG_H
#define BOARD_LCD_CONFIG_H

/*
 * Device-tuning values for the 128 x 64 LCD. The controller is probably an ST7567, but the repository has no
 * datasheet for it. All command values come from ST756x-family knowledge and are not verified on the device.
 * Every value can be overridden with -D for tests or tuning.
 */

/*
 * NOPs added to each SCK phase. One clock is 41.7 ns at 24 MHz; NOP, SETB, CLR and MOV bit,C take 1 clock and a
 * taken DJNZ takes 3 (STC8H manual, Instruction Set). For the bit loop in lcd_spi.c as SDCC 4.6 compiles it,
 * N NOPs give SCK low = 4 + N clocks, SCK high = 7 + N clocks and SDA setup = 1 + N clocks.
 * N = 3 gives 292 ns, 417 ns and 167 ns: at least one clock above the 250 / 250 / 125 ns minimums.
 */
#ifndef LCD_SPI_PHASE_NOPS
#define LCD_SPI_PHASE_NOPS 3
#endif

#ifndef LCD_BIAS_CMD
#define LCD_BIAS_CMD 0xA2U
#endif

#ifndef LCD_SEG_CMD_NORMAL
#define LCD_SEG_CMD_NORMAL 0xA0U
#endif
#ifndef LCD_SEG_CMD_ROTATED
#define LCD_SEG_CMD_ROTATED 0xA1U
#endif

#ifndef LCD_COM_CMD_NORMAL
#define LCD_COM_CMD_NORMAL 0xC8U
#endif
#ifndef LCD_COM_CMD_ROTATED
#define LCD_COM_CMD_ROTATED 0xC0U
#endif

/* The controller RAM has 132 columns; the panel shows 128 of them. Mirroring SEG moves the visible window. */
#ifndef LCD_COLUMN_OFFSET_NORMAL
#define LCD_COLUMN_OFFSET_NORMAL 0U
#endif
#ifndef LCD_COLUMN_OFFSET_ROTATED
#define LCD_COLUMN_OFFSET_ROTATED 4U
#endif

#ifndef LCD_REGULATION_RATIO_CMD
#define LCD_REGULATION_RATIO_CMD 0x25U
#endif

/* Electronic volume, range 0..63. */
#ifndef LCD_DEFAULT_CONTRAST
#define LCD_DEFAULT_CONTRAST 0x20U
#endif

#ifndef LCD_RESET_PULSE_MS
#define LCD_RESET_PULSE_MS 1U
#endif
/* 0x2F must follow the reset release within 5 ms (unverified); delay_ms() waits ms to ms + 1, and 0 waits nothing. */
#ifndef LCD_RESET_WAIT_MS
#define LCD_RESET_WAIT_MS 1U
#endif
#ifndef LCD_POWER_SETTLE_MS
#define LCD_POWER_SETTLE_MS 10U
#endif

/* Columns sent per lcd_flush_step() call. Smaller chunks keep each main-loop pass short. */
#ifndef LCD_FLUSH_CHUNK_COLUMNS
#define LCD_FLUSH_CHUNK_COLUMNS 128U
#endif

#if (LCD_FLUSH_CHUNK_COLUMNS != 32) && (LCD_FLUSH_CHUNK_COLUMNS != 64) && (LCD_FLUSH_CHUNK_COLUMNS != 128)
#error "LCD_FLUSH_CHUNK_COLUMNS must be 32, 64 or 128"
#endif

#if (LCD_RESET_WAIT_MS < 1) || (LCD_RESET_WAIT_MS > 2)
#error "LCD_RESET_WAIT_MS must be 1 or 2: the controller needs recovery time and a 5 ms reset-to-power-on window"
#endif

#if LCD_DEFAULT_CONTRAST > 63
#error "LCD_DEFAULT_CONTRAST must be in the range 0..63"
#endif

#endif /* BOARD_LCD_CONFIG_H */
