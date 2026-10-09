/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "progress/progress.h"

#include "eeprom/eeprom.h"
#include "eeprom/eeprom_map.h"
#include "mcu/compiler.h"
#include "util/crc8.h"

/* Record layout (references/internal/training_spec.md section 10.1). Levels are low byte first. */
#define RECORD_MAGIC_0 0U
#define RECORD_MAGIC_1 1U
#define RECORD_VERSION 2U
#define RECORD_LEVELS 3U
#define RECORD_MODE_SIZE (2U * PROGRESS_SPEED_COUNT)
#define RECORD_CRC (RECORD_LEVELS + (2U * RECORD_MODE_SIZE))
/* The magic goes last, so an interrupted save never leaves a valid record. */
#define RECORD_BODY_OFFSET RECORD_VERSION
#define RECORD_MAGIC_SIZE 2U
#define PROGRESS_NO_OFFSET 0U

_Static_assert((PROGRESS_WPM_MAX - PROGRESS_WPM_MIN + 1U) == PROGRESS_SPEED_COUNT, "one level per speed");
_Static_assert((RECORD_CRC + 1U) == PROGRESS_RECORD_SIZE, "the CRC is the last record byte");
_Static_assert(PROGRESS_RECORD_SIZE <= EEPROM_PAGE_SIZE, "the record must fit one page");
_Static_assert(EEPROM_MAP_PROGRESS_ADDRESS == (EEPROM_MAP_PROGRESS_PAGE * EEPROM_PAGE_SIZE),
               "the record starts at its page");

/* D-T9: the record image is the level cache. */
static MEM_XDATA uint8_t progress_record[PROGRESS_RECORD_SIZE];

/* Offset of the low level byte, or PROGRESS_NO_OFFSET for an unknown mode. */
static uint8_t progress_offset(uint8_t mode, MEM_XDATA uint8_t wpm) {
  uint8_t offset;

  if (mode > PROGRESS_MODE_SENDING) {
    return PROGRESS_NO_OFFSET;
  }
  if (wpm < PROGRESS_WPM_MIN) {
    wpm = PROGRESS_WPM_MIN;
  } else if (wpm > PROGRESS_WPM_MAX) {
    wpm = PROGRESS_WPM_MAX;
  }
  offset = (uint8_t)(RECORD_LEVELS + (uint8_t)((uint8_t)(wpm - PROGRESS_WPM_MIN) << 1));
  if (mode == PROGRESS_MODE_SENDING) {
    offset += RECORD_MODE_SIZE;
  }
  return offset;
}

static void progress_clear_levels(void) {
  uint8_t index;

  for (index = RECORD_LEVELS; index < (uint8_t)RECORD_CRC; ++index) {
    progress_record[index] = 0U;
  }
}

progress_status_t progress_load(void) {
  progress_status_t status = PROGRESS_OK;

  if (eeprom_read(EEPROM_MAP_PROGRESS_ADDRESS, progress_record, PROGRESS_RECORD_SIZE) != EEPROM_OK) {
    status = PROGRESS_READ_FAILED;
  } else if ((progress_record[RECORD_MAGIC_0] != PROGRESS_MAGIC_0) ||
             (progress_record[RECORD_MAGIC_1] != PROGRESS_MAGIC_1)) {
    status = PROGRESS_NO_RECORD;
  } else if (progress_record[RECORD_VERSION] != PROGRESS_LAYOUT_VERSION) {
    status = PROGRESS_BAD_VERSION;
  } else if (crc8_compute(progress_record, RECORD_CRC) != progress_record[RECORD_CRC]) {
    status = PROGRESS_BAD_CRC;
  }
  if (status != PROGRESS_OK) {
    progress_clear_levels();
  }
  progress_record[RECORD_MAGIC_0] = PROGRESS_MAGIC_0;
  progress_record[RECORD_MAGIC_1] = PROGRESS_MAGIC_1;
  progress_record[RECORD_VERSION] = PROGRESS_LAYOUT_VERSION;
  return status;
}

uint16_t progress_level(uint8_t mode, MEM_XDATA uint8_t wpm) {
  const uint8_t offset = progress_offset(mode, wpm);

  if (offset == PROGRESS_NO_OFFSET) {
    return 0U;
  }
  return (uint16_t)(((uint16_t)progress_record[offset + 1U] << 8) | progress_record[offset]);
}

bool progress_advance(uint8_t mode, MEM_XDATA uint8_t wpm) {
  const uint8_t offset = progress_offset(mode, wpm);

  if ((offset == PROGRESS_NO_OFFSET) ||
      ((progress_record[offset] == 0xFFU) && (progress_record[offset + 1U] == 0xFFU))) {
    return false;
  }
  ++progress_record[offset];
  if (progress_record[offset] == 0U) {
    ++progress_record[offset + 1U];
  }
  return true;
}

progress_status_t progress_save(void) {
  eeprom_clear_low_supply();
  progress_record[RECORD_CRC] = crc8_compute(progress_record, RECORD_CRC);
  if ((eeprom_erase_page(EEPROM_MAP_PROGRESS_PAGE) != EEPROM_OK) ||
      (eeprom_write(EEPROM_MAP_PROGRESS_ADDRESS + RECORD_BODY_OFFSET, &progress_record[RECORD_BODY_OFFSET],
                    PROGRESS_RECORD_SIZE - RECORD_BODY_OFFSET) != EEPROM_OK) ||
      (eeprom_write(EEPROM_MAP_PROGRESS_ADDRESS, progress_record, RECORD_MAGIC_SIZE) != EEPROM_OK)) {
    return PROGRESS_SAVE_FAILED;
  }
  return PROGRESS_OK;
}

progress_status_t progress_reset(void) {
  progress_clear_levels();
  return progress_save();
}
