#ifndef BOARD_LCD_PINS_H
#define BOARD_LCD_PINS_H

/*
 * LCD pins on port 2 (references/internal/components_and_wiring.md): P2.0 CS1, P2.1 RST, P2.2 A0, P2.3 SCLK,
 * P2.4 SDA. Interrupt handlers can change other port 2 pins, so every macro is one bit operation
 * (SETB, CLR or MOV bit,C) and never a read-modify-write of the whole port.
 * Host tests replace this header with firmware/Tests/fakes/board/lcd_pins.h.
 */
#if !defined(__SDCC_mcs51)
#error "board/lcd_pins.h is for the target; host tests use firmware/Tests/fakes/board/lcd_pins.h"
#endif

#include <8051.h>
#include <stdint.h>

#include "board/lcd_config.h"
#include "mcu/stc8h.h"

#define LCD_CS_HIGH() (P2_0 = 1)
#define LCD_CS_LOW() (P2_0 = 0)
#define LCD_RST_HIGH() (P2_1 = 1)
#define LCD_RST_LOW() (P2_1 = 0)
#define LCD_A0_HIGH() (P2_2 = 1)
#define LCD_A0_LOW() (P2_2 = 0)
#define LCD_SCK_HIGH() (P2_3 = 1)
#define LCD_SCK_LOW() (P2_3 = 0)
#define LCD_SDA_WRITE(is_high) (P2_4 = (is_high))

/* P2.0-P2.4 push-pull: PnM1 = 0, PnM0 = 1. Each register gets one ORL or ANL, so P2.5-P2.7 keep their mode. */
#define LCD_PINS_MASK 0x1FU
#define LCD_PINS_CONFIGURE_OUTPUTS() \
  do {                               \
    P2M0 |= LCD_PINS_MASK;           \
    P2M1 &= (uint8_t)~LCD_PINS_MASK; \
  } while (0)

#if LCD_SPI_PHASE_NOPS == 0
#define LCD_PACE() ((void)0)
#elif LCD_SPI_PHASE_NOPS == 1
#define LCD_PACE() __asm__("nop")
#elif LCD_SPI_PHASE_NOPS == 2
#define LCD_PACE() __asm__("nop\n\tnop")
#elif LCD_SPI_PHASE_NOPS == 3
#define LCD_PACE() __asm__("nop\n\tnop\n\tnop")
#elif LCD_SPI_PHASE_NOPS == 4
#define LCD_PACE() __asm__("nop\n\tnop\n\tnop\n\tnop")
#elif LCD_SPI_PHASE_NOPS == 5
#define LCD_PACE() __asm__("nop\n\tnop\n\tnop\n\tnop\n\tnop")
#elif LCD_SPI_PHASE_NOPS == 6
#define LCD_PACE() __asm__("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop")
#elif LCD_SPI_PHASE_NOPS == 7
#define LCD_PACE() __asm__("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop")
#elif LCD_SPI_PHASE_NOPS == 8
#define LCD_PACE() __asm__("nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop")
#else
#error "LCD_SPI_PHASE_NOPS must be in the range 0..8"
#endif

#endif /* BOARD_LCD_PINS_H */
