/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "util/crc8.h"

#include "mcu/compiler.h"

uint8_t crc8_compute(const MEM_XDATA uint8_t* data, uint8_t length) {
  uint8_t crc = 0U;
  uint8_t bit;

  while (length != 0U) {
    crc ^= *data;
    for (bit = 0U; bit < 8U; ++bit) {
      if ((crc & 0x80U) != 0U) {
        crc = (uint8_t)((uint8_t)(crc << 1) ^ 0x07U);
      } else {
        crc = (uint8_t)(crc << 1);
      }
    }
    ++data;
    --length;
  }
  return crc;
}
