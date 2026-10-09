/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MCU_IAP_PORT_H
#define MCU_IAP_PORT_H

/* Host replacement for firmware/inc/mcu/iap_port.h. Every macro is one call into the fake EEPROM (fake_iap.c). */

#include "fake_iap.h"

#define IAP_CMD_READ 1U
#define IAP_CMD_PROGRAM 2U
#define IAP_CMD_ERASE 3U

#define IAP_PORT_ENABLE() fake_iap_enable((uint8_t)EEPROM_IAP_TPS)
#define IAP_PORT_SET_COMMAND(cmd) fake_iap_set_command(cmd)
#define IAP_PORT_SET_ADDRESS(address) fake_iap_set_address(address)
#define IAP_PORT_WRITE_DATA(value) fake_iap_write_data(value)
#define IAP_PORT_READ_DATA() fake_iap_read_data()
#define IAP_PORT_TRIGGER() fake_iap_trigger()
#define IAP_PORT_HAS_FAILED() fake_iap_has_failed()
#define IAP_PORT_CLEAR_FAIL() fake_iap_clear_fail()
#define IAP_PORT_IDLE() fake_iap_idle()
/* The same masks as the target macro, applied to the fake RSTCFG: one AND, then one OR. */
#define IAP_PORT_LVD_INIT()                         \
  do {                                              \
    fake_iap_rstcfg_and((uint8_t)~(0x40U | 0x03U)); \
    fake_iap_rstcfg_or(0x03U);                      \
  } while (0)
#define IAP_PORT_IS_SUPPLY_LOW() fake_iap_is_supply_low()
#define IAP_PORT_CLEAR_SUPPLY_LOW() fake_iap_clear_supply_low()

#endif /* MCU_IAP_PORT_H */
