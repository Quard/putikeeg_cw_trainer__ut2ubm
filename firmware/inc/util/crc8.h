#ifndef UTIL_CRC8_H
#define UTIL_CRC8_H

/* CRC-8 of the EEPROM records (settings and progress). Main loop only. */

#include <stdint.h>

#include "mcu/compiler.h"

/** CRC-8: polynomial 0x07, initial value 0, no reflection, no final XOR. "123456789" gives 0xF4. */
uint8_t crc8_compute(const MEM_XDATA uint8_t* data, uint8_t length);

#endif /* UTIL_CRC8_H */
