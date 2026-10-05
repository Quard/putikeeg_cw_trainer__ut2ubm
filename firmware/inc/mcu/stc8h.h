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

/* Port 1 modes, datasheet section 9.1.2: M1 = 0, M0 = 1 is push-pull; M1 = 1, M0 = 0 is high-impedance input. */
__sfr __at(0x91) P1M1;
__sfr __at(0x92) P1M0;
#define P1PU (*(volatile __xdata uint8_t*)0xFE11U)

/*
 * Port 5, datasheet sections 9.1.1 to 9.1.3. P5 is bit-addressable, so P5.5 is bit 0xCD; bits 7:6 do not exist.
 * M1 = 1, M0 = 0 is a high-impedance input. P5PU bit 5 is the P5.5 pull-up.
 */
__sfr __at(0xC8) P5;
__sbit __at(0xCD) P5_5;
__sfr __at(0xC9) P5M1;
__sfr __at(0xCA) P5M0;
#define P5PU (*(volatile __xdata uint8_t*)0xFE15U)

/* Datasheet sections 7.2.4 and 13.2.12: bit 7 T0x12 selects SYSclk / 1 for Timer 0; bit 1 EXTRAM must stay 0. */
__sfr __at(0x8E) AUXR;
#define AUXR_T0X12 0x80U
#define TMOD_T0_MASK 0x0FU /* Timer 0 GATE, C/T, M1, M0; the high nibble is Timer 1 */

/* Vector address = 8 * number + 3 = 0x000B. */
#define TIMER0_INTERRUPT 1

/* PWMA (advanced PWM, channels 1-4) XFRs, datasheet sections 3.1.5 and 20.7. */
#define PWMA_ENO (*(volatile __xdata uint8_t*)0xFEB1U)
#define PWMA_PS (*(volatile __xdata uint8_t*)0xFEB2U)
#define PWMA_IOAUX (*(volatile __xdata uint8_t*)0xFEB3U)
#define PWMA_CR1 (*(volatile __xdata uint8_t*)0xFEC0U)
#define PWMA_CR2 (*(volatile __xdata uint8_t*)0xFEC1U)
#define PWMA_IER (*(volatile __xdata uint8_t*)0xFEC4U)
#define PWMA_SR1 (*(volatile __xdata uint8_t*)0xFEC5U)
#define PWMA_EGR (*(volatile __xdata uint8_t*)0xFEC7U)
#define PWMA_CCMR1 (*(volatile __xdata uint8_t*)0xFEC8U)
#define PWMA_CCMR2 (*(volatile __xdata uint8_t*)0xFEC9U)
#define PWMA_CCER1 (*(volatile __xdata uint8_t*)0xFECCU)
#define PWMA_PSCRH (*(volatile __xdata uint8_t*)0xFED0U)
#define PWMA_PSCRL (*(volatile __xdata uint8_t*)0xFED1U)
#define PWMA_ARRH (*(volatile __xdata uint8_t*)0xFED2U)
#define PWMA_ARRL (*(volatile __xdata uint8_t*)0xFED3U)
#define PWMA_CCR1H (*(volatile __xdata uint8_t*)0xFED5U)
#define PWMA_CCR1L (*(volatile __xdata uint8_t*)0xFED6U)
#define PWMA_CCR2H (*(volatile __xdata uint8_t*)0xFED7U)
#define PWMA_CCR2L (*(volatile __xdata uint8_t*)0xFED8U)
#define PWMA_BKR (*(volatile __xdata uint8_t*)0xFEDDU)

#define PWMA_ENO_ENO1N 0x02U
#define PWMA_ENO_ENO2P 0x04U
#define PWMA_PS_P1 0x00U /* C1PS-C4PS = 00: PWM1N on P1.1, PWM2P on P1.2 */
#define PWMA_CR1_CEN 0x01U
#define PWMA_CR1_ARPE 0x80U
#define PWMA_EGR_UG 0x01U
#define PWMA_SR1_UIF 0x01U
#define PWMA_CCMR_PWM1_PRELOAD 0x68U /* OCiM = 110 (PWM mode 1), OCiPE = 1, CCiS = 00 (output) */
#define PWMA_CCER1_CC1NE 0x04U
#define PWMA_CCER1_CC1NP 0x08U
#define PWMA_CCER1_CC2E 0x10U
#define PWMA_CCER1_CC2P 0x20U
#define PWMA_BKR_MOE 0x80U

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
