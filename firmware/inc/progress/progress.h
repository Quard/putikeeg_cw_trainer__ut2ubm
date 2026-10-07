#ifndef PROGRESS_PROGRESS_H
#define PROGRESS_PROGRESS_H

/*
 * Training level per mode and character speed, stored in EEPROM page 1 (references/internal/training_spec.md
 * section 10). The RAM copy of the record is the level cache. Main loop only.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#define PROGRESS_MODE_RECEIVING 0U
#define PROGRESS_MODE_SENDING 1U
#define PROGRESS_WPM_MIN 5U  /* equal to settings_min(SETTINGS_ID_RX_WPM) and TX */
#define PROGRESS_WPM_MAX 50U /* equal to settings_max(SETTINGS_ID_RX_WPM) and TX */
#define PROGRESS_SPEED_COUNT 46U
#define PROGRESS_RECORD_SIZE 188U
#define PROGRESS_MAGIC_0 0x4BU /* 'K' */
#define PROGRESS_MAGIC_1 0x50U /* 'P' */
#define PROGRESS_LAYOUT_VERSION 1U

typedef uint8_t progress_status_t;
#define PROGRESS_OK 0U
#define PROGRESS_READ_FAILED 1U
#define PROGRESS_NO_RECORD 2U /* magic mismatch, for example an erased page */
#define PROGRESS_BAD_VERSION 3U
#define PROGRESS_BAD_CRC 4U
#define PROGRESS_SAVE_FAILED 5U

/** Reads and checks the record. On any failure every level is 0. Never writes the EEPROM. Call once at start-up. */
progress_status_t progress_load(void);
/** Level index of @p mode at @p wpm (clamped to 5..50). 0 for an unknown mode. */
uint16_t progress_level(uint8_t mode, MEM_XDATA uint8_t wpm);
/** Adds 1 to the level in RAM. Returns false and changes nothing at 65535 or for an unknown mode. */
bool progress_advance(uint8_t mode, MEM_XDATA uint8_t wpm);
/** Erases page 1 and writes the record. Returns PROGRESS_OK or PROGRESS_SAVE_FAILED. */
progress_status_t progress_save(void);
/** Sets every level to 0 in RAM, then saves. Returns PROGRESS_OK or PROGRESS_SAVE_FAILED. */
progress_status_t progress_reset(void);

#endif /* PROGRESS_PROGRESS_H */
