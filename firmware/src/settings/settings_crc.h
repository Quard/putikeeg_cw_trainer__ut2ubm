#ifndef SETTINGS_SETTINGS_CRC_H
#define SETTINGS_SETTINGS_CRC_H

/* Private to settings; host tests include it from firmware/src/settings. */

#include <stdint.h>

#include "mcu/compiler.h"

/** CRC-8: polynomial 0x07, initial value 0, no reflection, no final XOR. "123456789" gives 0xF4. */
uint8_t settings_crc8(const MEM_XDATA uint8_t* data, uint8_t length);

#endif /* SETTINGS_SETTINGS_CRC_H */
