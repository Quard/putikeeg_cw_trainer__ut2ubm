/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "progress_fixture.h"

#include <string.h>

#include "eeprom/eeprom_map.h"
#include "fake_iap.h"
#include "progress/progress.h"
#include "util/crc8.h"

#define FIXTURE_LEVELS 3U
#define FIXTURE_MODE_SIZE (2U * PROGRESS_SPEED_COUNT)
#define FIXTURE_CRC (PROGRESS_RECORD_SIZE - 1U)

static unsigned fixture_offset(uint8_t mode, uint8_t wpm) {
  return FIXTURE_LEVELS + (2U * (unsigned)(wpm - PROGRESS_WPM_MIN)) +
         ((mode == PROGRESS_MODE_SENDING) ? FIXTURE_MODE_SIZE : 0U);
}

void progress_fixture_write(uint8_t mode, uint8_t wpm, uint16_t level) {
  uint8_t record[PROGRESS_RECORD_SIZE];

  memset(record, 0, sizeof(record));
  record[0] = PROGRESS_MAGIC_0;
  record[1] = PROGRESS_MAGIC_1;
  record[2] = PROGRESS_LAYOUT_VERSION;
  memcpy(&fake_iap_memory()[EEPROM_MAP_PROGRESS_ADDRESS], record, sizeof(record));
  progress_fixture_add(mode, wpm, level);
}

void progress_fixture_add(uint8_t mode, uint8_t wpm, uint16_t level) {
  uint8_t* record = &fake_iap_memory()[EEPROM_MAP_PROGRESS_ADDRESS];
  const unsigned offset = fixture_offset(mode, wpm);

  record[offset] = (uint8_t)(level & 0xFFU);
  record[offset + 1U] = (uint8_t)(level >> 8);
  record[FIXTURE_CRC] = crc8_compute(record, FIXTURE_CRC);
}
