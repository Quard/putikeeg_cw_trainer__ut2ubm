#ifndef MCU_IAP_PORT_H
#define MCU_IAP_PORT_H

/*
 * IAP (EEPROM) and low-voltage detector registers. The only file that names them; host tests replace it with
 * firmware/Tests/fakes/mcu/iap_port.h. Source: references/context/STC8H_datasheet.md sections 16.2, 6.3.3, 6.5.1.
 * IAP_PORT_ENABLE() writes EEPROM_IAP_TPS, which the including file (eeprom.c) defines.
 */
#if !defined(__SDCC_mcs51)
#error "mcu/iap_port.h is for the target; host tests use firmware/Tests/fakes/mcu/iap_port.h"
#endif

#include <8051.h>
#include <stdint.h>

#include "mcu/stc8h.h"

#define IAP_CMD_READ 1U
#define IAP_CMD_PROGRAM 2U
#define IAP_CMD_ERASE 3U

#define IAP_PORT_ENABLE()              \
  do {                                 \
    IAP_CONTR = IAP_CONTR_IAPEN;       \
    IAP_TPS = (uint8_t)EEPROM_IAP_TPS; \
  } while (0)
#define IAP_PORT_SET_COMMAND(cmd) (IAP_CMD = (cmd))
#define IAP_PORT_SET_ADDRESS(address)                \
  do {                                               \
    IAP_ADDRL = (uint8_t)(address);                  \
    IAP_ADDRH = (uint8_t)((uint16_t)(address) >> 8); \
  } while (0)
#define IAP_PORT_WRITE_DATA(value) (IAP_DATA = (value))
#define IAP_PORT_READ_DATA() (IAP_DATA)
/* The CPU idles until the operation ends; the datasheet examples put one nop after the trigger. */
#define IAP_PORT_TRIGGER() \
  do {                     \
    IAP_TRIG = 0x5AU;      \
    IAP_TRIG = 0xA5U;      \
    __asm__("nop");        \
  } while (0)
#define IAP_PORT_HAS_FAILED() ((IAP_CONTR & IAP_CONTR_CMD_FAIL) != 0U)
#define IAP_PORT_CLEAR_FAIL() (IAP_CONTR &= (uint8_t)~IAP_CONTR_CMD_FAIL)
/* Idle state of the datasheet example IapIdle(): the address points outside the EEPROM. */
#define IAP_PORT_IDLE() \
  do {                  \
    IAP_CONTR = 0U;     \
    IAP_CMD = 0U;       \
    IAP_TRIG = 0U;      \
    IAP_ADDRH = 0x80U;  \
    IAP_ADDRL = 0U;     \
  } while (0)

/*
 * Low-voltage detection at 3.0 V without the low-voltage reset: the ISP options can leave ENLVR set, and then the
 * MCU would reset near 3.0 V before the save guard sees LVDF. One ANL and one ORL keep P54RST and the other bits.
 */
#define IAP_PORT_LVD_INIT()                                \
  do {                                                     \
    RSTCFG &= (uint8_t)~(RSTCFG_ENLVR | RSTCFG_LVDS_MASK); \
    RSTCFG |= RSTCFG_LVDS_3V0;                             \
  } while (0)
#define IAP_PORT_IS_SUPPLY_LOW() ((PCON & PCON_LVDF) != 0U)
#define IAP_PORT_CLEAR_SUPPLY_LOW() (PCON &= (uint8_t)~PCON_LVDF)

#endif /* MCU_IAP_PORT_H */
