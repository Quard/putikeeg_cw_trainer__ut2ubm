#ifndef SETTINGS_SETTINGS_H
#define SETTINGS_SETTINGS_H

/*
 * User settings: ranges, defaults, clamps, the effective-speed rule and the record in EEPROM page 0
 * (references/internal/menu_subsystem_spec.md sections 4.8 and 5). Call from the main loop only; not reentrant.
 * The caller applies contrast and backlight.
 */

#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

typedef uint8_t settings_id_t;

#define SETTINGS_ID_TONE_HZ 0U
#define SETTINGS_ID_RX_WPM 1U
#define SETTINGS_ID_RX_EFF_WPM 2U
#define SETTINGS_ID_TX_WPM 3U
#define SETTINGS_ID_TX_EFF_WPM 4U
#define SETTINGS_ID_KEY_TYPE 5U
#define SETTINGS_ID_SHOW_CW_CODE 6U
#define SETTINGS_ID_MAX_WORD_LEN 7U
#define SETTINGS_ID_BACKLIGHT 8U
#define SETTINGS_ID_CONTRAST 9U
#define SETTINGS_ID_COUNT 10U

#define SETTINGS_KEY_STRAIGHT 0U
#define SETTINGS_KEY_IAMBIC_A 1U
#define SETTINGS_KEY_IAMBIC_B 2U

typedef uint8_t settings_load_status_t;
#define SETTINGS_LOAD_OK 0U
#define SETTINGS_LOAD_READ_FAILED 1U
#define SETTINGS_LOAD_NO_RECORD 2U /* magic mismatch, for example an erased page */
#define SETTINGS_LOAD_BAD_VERSION 3U
#define SETTINGS_LOAD_BAD_CRC 4U

typedef uint8_t settings_save_status_t;
#define SETTINGS_SAVE_NOT_NEEDED 0U
#define SETTINGS_SAVE_OK 1U
#define SETTINGS_SAVE_FAILED 2U

#define SETTINGS_RECORD_SIZE 15U
#define SETTINGS_MAGIC_0 0x43U /* 'C' */
#define SETTINGS_MAGIC_1 0x57U /* 'W' */
#define SETTINGS_LAYOUT_VERSION 1U

/**
 * Reads the record and checks read status, magic, version and CRC in this order. On a failure all values take
 * their defaults. On success every field is clamped. Never writes the EEPROM. Sets the change baseline to the
 * resulting values and clears the edited flag. Call once at start-up, before any other settings call.
 */
settings_load_status_t settings_load(void);

/**
 * Saves when the edited flag is set and the values differ from the baseline: erase page 0, program bytes 2..14,
 * then the magic. Without a difference it clears the edited flag. On an EEPROM error it keeps the baseline and the
 * edited flag and returns SETTINGS_SAVE_FAILED.
 */
settings_save_status_t settings_save_if_changed(void);

/** Returns true when a current value differs from the baseline. */
bool settings_has_unsaved_changes(void);

/** Returns the edited flag: a settings_set() call changed a value since the last load or successful save. */
bool settings_is_edited(void);

/** Returns the current value of @p id, or 0 for an unknown id. */
uint16_t settings_get(settings_id_t id);

/**
 * Clamps @p value to the range and step grid of @p id, applies the effective-speed rule and stores it. Returns
 * true and sets the edited flag when the value of @p id changed. Returns false for an unknown id.
 */
bool settings_set(settings_id_t id, MEM_XDATA uint16_t value);

/** Range and step of @p id. The max of an effective speed is its current base speed. 0 for an unknown id. */
uint16_t settings_min(settings_id_t id);
uint16_t settings_max(settings_id_t id);
uint16_t settings_step(settings_id_t id);

/** Sets contrast and backlight to their defaults without a change of the baseline or the edited flag. */
void settings_use_display_defaults(void);

#endif /* SETTINGS_SETTINGS_H */
