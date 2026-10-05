#ifndef MCU_STC8H_H
#define MCU_STC8H_H

/* STC8H SFRs that SDCC <8051.h> does not declare. Source: references/context/STC8H_datasheet.md, SFR table. */
#if !defined(__SDCC_mcs51)
#error "mcu/stc8h.h is for the SDCC mcs51 target only"
#endif

#include <stdint.h>

__sfr __at(0x93) P0M1;
__sfr __at(0x94) P0M0;
__sfr __at(0x95) P2M1;
__sfr __at(0x96) P2M0;
__sfr __at(0xB1) P3M1;
__sfr __at(0xB2) P3M0;

/* Datasheet section 3.1.2: EAXFR must be 1 for any XFR (0xFE00-0xFEFF) access. */
__sfr __at(0xBA) P_SW2;
#define P_SW2_EAXFR 0x80U

/* XFRs are in xdata space and need EAXFR. Pull-up enables, datasheet section 9.1.3: one bit per pin. */
#define P0PU (*(volatile __xdata uint8_t*)0xFE10U)
#define P3PU (*(volatile __xdata uint8_t*)0xFE13U)

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

/*
 * PWMB (advanced PWM, channels 5-8) XFRs, datasheet sections 3.1.5 and 20.7. PWMB_CCMR4, PWMB_CCER2 and
 * PWMB_CCR8 belong to channel 8.
 */
#define PWMB_ENO (*(volatile __xdata uint8_t*)0xFEB5U)
#define PWMB_PS (*(volatile __xdata uint8_t*)0xFEB6U)
#define PWMB_CR1 (*(volatile __xdata uint8_t*)0xFEE0U)
#define PWMB_EGR (*(volatile __xdata uint8_t*)0xFEE7U)
#define PWMB_CCMR4 (*(volatile __xdata uint8_t*)0xFEEBU)
#define PWMB_CCER2 (*(volatile __xdata uint8_t*)0xFEEDU)
#define PWMB_PSCRH (*(volatile __xdata uint8_t*)0xFEF0U)
#define PWMB_PSCRL (*(volatile __xdata uint8_t*)0xFEF1U)
#define PWMB_ARRH (*(volatile __xdata uint8_t*)0xFEF2U)
#define PWMB_ARRL (*(volatile __xdata uint8_t*)0xFEF3U)
#define PWMB_CCR8H (*(volatile __xdata uint8_t*)0xFEFBU)
#define PWMB_CCR8L (*(volatile __xdata uint8_t*)0xFEFCU)
#define PWMB_BKR (*(volatile __xdata uint8_t*)0xFEFDU)

#define PWMB_ENO_ENO8P 0x40U
#define PWMB_PS_C8PS_P03 0x80U /* C8PS[1:0] = 10: PWM8 on P0.3; C5PS-C7PS = 00 */
#define PWMB_CR1_CEN 0x01U
#define PWMB_CR1_ARPE 0x80U
#define PWMB_EGR_UG 0x01U
#define PWMB_CCMR4_PWM1_PRELOAD 0x68U /* OC8M = 110 (PWM mode 1), OC8PE = 1, CC8S = 00 (output) */
#define PWMB_CCER2_CC8E 0x10U
#define PWMB_CCER2_CC8P 0x20U
#define PWMB_BKR_MOE 0x80U

/* IAP/EEPROM, datasheet section 16.2. IAP_TPS is a normal SFR, not an XFR. */
__sfr __at(0xC2) IAP_DATA;
__sfr __at(0xC3) IAP_ADDRH;
__sfr __at(0xC4) IAP_ADDRL;
__sfr __at(0xC5) IAP_CMD;
__sfr __at(0xC6) IAP_TRIG;
__sfr __at(0xC7) IAP_CONTR;
__sfr __at(0xF5) IAP_TPS;

#define IAP_CONTR_IAPEN 0x80U
#define IAP_CONTR_CMD_FAIL 0x10U

/*
 * Reset configuration, datasheet section 6.3.3: bit 6 ENLVR (low-voltage reset), bit 4 P54RST, bits 1:0 LVDS.
 * LVDS = 11 selects the 3.0 V low-voltage threshold. The ISP options set the reset value.
 */
__sfr __at(0xFF) RSTCFG;
#define RSTCFG_ENLVR 0x40U
#define RSTCFG_LVDS_MASK 0x03U
#define RSTCFG_LVDS_3V0 0x03U

/* PCON (declared in <8051.h>), datasheet section 6.5.1: the low-voltage flag; hardware sets it, software clears it. */
#define PCON_LVDF 0x20U

#endif /* MCU_STC8H_H */
