#ifndef MCU_STC8H_H
#define MCU_STC8H_H

/* STC8H SFRs that SDCC <8051.h> does not declare. Source: references/context/STC8H_datasheet.md, SFR table. */
#if !defined(__SDCC_mcs51)
#error "mcu/stc8h.h is for the SDCC mcs51 target only"
#endif

__sfr __at(0x95) P2M1;
__sfr __at(0x96) P2M0;

/* Timer 3: 16-bit auto-reload; writing T3H/T3L while T3R = 0 also sets the reload value. */
__sfr __at(0xD1) T4T3M;
__sfr __at(0xD4) T3H;
__sfr __at(0xD5) T3L;
__sfr __at(0xAF) IE2;
__sfr __at(0xEF) AUXINTIF;

#define T4T3M_T3R 0x08U     /* Timer 3 run */
#define T4T3M_T3_MASK 0x0FU /* T3R, T3_C/T, T3x12, T3CLKO */
#define IE2_ET3 0x20U
#define AUXINTIF_T3IF 0x02U /* write-only: reads as 0 */

/* Vector address = 8 * number + 3 = 0x009B. */
#define TIMER3_INTERRUPT 19

#endif /* MCU_STC8H_H */
