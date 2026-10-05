#include "settings/settings.h"

#include "board/lcd_config.h"
#include "eeprom/eeprom.h"
#include "eeprom/eeprom_map.h"
#include "settings_crc.h"

/* Record layout (references/internal/menu_subsystem_spec.md section 5.1). */
#define RECORD_MAGIC_0 0U
#define RECORD_MAGIC_1 1U
#define RECORD_VERSION 2U
#define RECORD_TONE_LOW 3U
#define RECORD_TONE_HIGH 4U
#define RECORD_FLAGS 10U
#define RECORD_CRC 14U
#define RECORD_FIRST_FIELD RECORD_TONE_LOW
#define RECORD_FIELD_COUNT 11U /* bytes 3..13 */
#define RECORD_FLAG_SHOW_CW_CODE 0x01U
#define RECORD_FLAG_PADDLE_SWAP 0x02U

#define SETTINGS_BACKLIGHT_DEFAULT 8U

/* The current values live in record form; field offsets by id. Show CW Code is flags bit 0, Paddle is bit 1. */
static const MEM_CODE uint8_t settings_offsets[SETTINGS_ID_COUNT] = {
    RECORD_TONE_LOW, 5U, 6U, 7U, 8U, 9U, RECORD_FLAGS, 11U, 12U, 13U, RECORD_FLAGS,
};
/* Flags-byte bit by id, or 0 for an id with a byte of its own. A table, so settings_store() stays a leaf. */
static const MEM_CODE uint8_t settings_flag_masks[SETTINGS_ID_COUNT] = {
    0U, 0U, 0U, 0U, 0U, 0U, RECORD_FLAG_SHOW_CW_CODE, 0U, 0U, 0U, RECORD_FLAG_PADDLE_SWAP,
};
static const MEM_CODE uint16_t settings_mins[SETTINGS_ID_COUNT] = {400U, 5U, 5U, 5U, 5U, 0U, 0U, 4U, 0U, 0U, 0U};
static const MEM_CODE uint16_t settings_maxes[SETTINGS_ID_COUNT] = {1000U, 50U, 50U, 50U, 50U, 2U,
                                                                    1U,    8U,  10U, 63U, 1U};
static const MEM_CODE uint8_t settings_steps[SETTINGS_ID_COUNT] = {10U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U};
static const MEM_CODE uint16_t settings_defaults[SETTINGS_ID_COUNT] = {
    600U, 32U, 32U, 20U, 20U, SETTINGS_KEY_IAMBIC_B, 1U, 5U, SETTINGS_BACKLIGHT_DEFAULT, LCD_DEFAULT_CONTRAST, 0U,
};

static MEM_XDATA uint8_t settings_record[SETTINGS_RECORD_SIZE];
/* Fields (record bytes 3..13) of the stored record, or of the defaults after a failed load. */
static MEM_XDATA uint8_t settings_baseline[RECORD_FIELD_COUNT];
static MEM_XDATA bool settings_is_edited_flag;

uint8_t settings_crc8(const MEM_XDATA uint8_t* data, uint8_t length) {
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

/* Effective speed id of base speed @p id, or SETTINGS_ID_COUNT for other ids. */
static settings_id_t settings_effective_of(settings_id_t id) {
  if ((id == SETTINGS_ID_RX_WPM) || (id == SETTINGS_ID_TX_WPM)) {
    return (settings_id_t)(id + 1U);
  }
  return SETTINGS_ID_COUNT;
}

static void settings_store(settings_id_t id, uint16_t value) {
  const uint8_t offset = settings_offsets[id];
  const uint8_t flag_mask = settings_flag_masks[id];

  if (flag_mask != 0U) {
    if (value != 0U) {
      settings_record[offset] |= flag_mask;
    } else {
      settings_record[offset] &= (uint8_t)~flag_mask;
    }
    return;
  }
  settings_record[offset] = (uint8_t)value;
  if (id == SETTINGS_ID_TONE_HZ) {
    settings_record[RECORD_TONE_HIGH] = (uint8_t)(value >> 8);
  }
}

/* Limits @p value to [min, max] of @p id and rounds it down to the step grid min + k * step. */
static uint16_t settings_clamp(settings_id_t id, MEM_XDATA uint16_t value) {
  const uint16_t min = settings_mins[id];
  const uint16_t max = settings_max(id);

  if (value < min) {
    value = min;
  }
  if (value > max) {
    value = max;
  }
  if (settings_steps[id] > 1U) {
    value = (uint16_t)(value - (uint16_t)((uint16_t)(value - min) % settings_steps[id]));
  }
  return value;
}

static void settings_set_defaults(void) {
  settings_id_t id;
  /* The flag ids change only their own bits, so bits 2-7 of a bad record need this clear. */
  settings_record[RECORD_FLAGS] = 0U;
  for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
    settings_store(id, settings_defaults[id]);
  }
}

static void settings_take_baseline(void) {
  uint8_t index;
  for (index = 0U; index < RECORD_FIELD_COUNT; ++index) {
    settings_baseline[index] = settings_record[(uint8_t)(RECORD_FIRST_FIELD + index)];
  }
  settings_is_edited_flag = false;
}

static settings_load_status_t settings_check_record(void) {
  if (eeprom_read(EEPROM_MAP_SETTINGS_ADDRESS, settings_record, SETTINGS_RECORD_SIZE) != EEPROM_OK) {
    return SETTINGS_LOAD_READ_FAILED;
  }
  if ((settings_record[RECORD_MAGIC_0] != SETTINGS_MAGIC_0) || (settings_record[RECORD_MAGIC_1] != SETTINGS_MAGIC_1)) {
    return SETTINGS_LOAD_NO_RECORD;
  }
  if (settings_record[RECORD_VERSION] != SETTINGS_LAYOUT_VERSION) {
    return SETTINGS_LOAD_BAD_VERSION;
  }
  if (settings_crc8(settings_record, RECORD_CRC) != settings_record[RECORD_CRC]) {
    return SETTINGS_LOAD_BAD_CRC;
  }
  return SETTINGS_LOAD_OK;
}

settings_load_status_t settings_load(void) {
  const settings_load_status_t status = settings_check_record();
  settings_id_t id;

  if (status == SETTINGS_LOAD_OK) {
    settings_record[RECORD_FLAGS] &= (uint8_t)(RECORD_FLAG_SHOW_CW_CODE | RECORD_FLAG_PADDLE_SWAP);
    /* Id order clamps each base speed before its effective speed. */
    for (id = 0U; id < SETTINGS_ID_COUNT; ++id) {
      settings_store(id, settings_clamp(id, settings_get(id)));
    }
  } else {
    settings_set_defaults();
  }
  settings_record[RECORD_MAGIC_0] = SETTINGS_MAGIC_0;
  settings_record[RECORD_MAGIC_1] = SETTINGS_MAGIC_1;
  settings_record[RECORD_VERSION] = SETTINGS_LAYOUT_VERSION;
  settings_take_baseline();
  return status;
}

bool settings_has_unsaved_changes(void) {
  uint8_t index;
  for (index = 0U; index < RECORD_FIELD_COUNT; ++index) {
    if (settings_baseline[index] != settings_record[(uint8_t)(RECORD_FIRST_FIELD + index)]) {
      return true;
    }
  }
  return false;
}

settings_save_status_t settings_save_if_changed(void) {
  if (!settings_is_edited_flag) {
    return SETTINGS_SAVE_NOT_NEEDED;
  }
  if (!settings_has_unsaved_changes()) {
    settings_is_edited_flag = false;
    return SETTINGS_SAVE_NOT_NEEDED;
  }
  eeprom_clear_low_supply();
  settings_record[RECORD_CRC] = settings_crc8(settings_record, RECORD_CRC);
  /* The magic goes last, so an interrupted save never leaves a valid record. */
  if ((eeprom_erase_page(EEPROM_MAP_SETTINGS_PAGE) != EEPROM_OK) ||
      (eeprom_write(EEPROM_MAP_SETTINGS_ADDRESS + RECORD_VERSION, &settings_record[RECORD_VERSION],
                    SETTINGS_RECORD_SIZE - RECORD_VERSION) != EEPROM_OK) ||
      (eeprom_write(EEPROM_MAP_SETTINGS_ADDRESS, settings_record, RECORD_VERSION) != EEPROM_OK)) {
    return SETTINGS_SAVE_FAILED;
  }
  settings_take_baseline();
  return SETTINGS_SAVE_OK;
}

bool settings_is_edited(void) { return settings_is_edited_flag; }

uint16_t settings_get(settings_id_t id) {
  uint16_t value;
  uint8_t flag_mask;

  if (id >= SETTINGS_ID_COUNT) {
    return 0U;
  }
  value = settings_record[settings_offsets[id]];
  flag_mask = settings_flag_masks[id];
  if (flag_mask != 0U) {
    return ((value & flag_mask) != 0U) ? 1U : 0U;
  }
  if (id == SETTINGS_ID_TONE_HZ) {
    value |= (uint16_t)((uint16_t)settings_record[RECORD_TONE_HIGH] << 8);
  }
  return value;
}

bool settings_set(settings_id_t id, MEM_XDATA uint16_t value) {
  settings_id_t effective_id;

  if (id >= SETTINGS_ID_COUNT) {
    return false;
  }
  value = settings_clamp(id, value);
  if (value == settings_get(id)) {
    return false;
  }
  settings_store(id, value);
  effective_id = settings_effective_of(id);
  if ((effective_id != SETTINGS_ID_COUNT) && (settings_get(effective_id) > value)) {
    settings_store(effective_id, value);
  }
  settings_is_edited_flag = true;
  return true;
}

uint16_t settings_min(settings_id_t id) { return (id < SETTINGS_ID_COUNT) ? settings_mins[id] : 0U; }

uint16_t settings_max(settings_id_t id) {
  if (id >= SETTINGS_ID_COUNT) {
    return 0U;
  }
  if ((id == SETTINGS_ID_RX_EFF_WPM) || (id == SETTINGS_ID_TX_EFF_WPM)) {
    return settings_get((settings_id_t)(id - 1U));
  }
  return settings_maxes[id];
}

uint16_t settings_step(settings_id_t id) { return (id < SETTINGS_ID_COUNT) ? settings_steps[id] : 0U; }

void settings_use_display_defaults(void) {
  settings_store(SETTINGS_ID_CONTRAST, settings_defaults[SETTINGS_ID_CONTRAST]);
  settings_store(SETTINGS_ID_BACKLIGHT, settings_defaults[SETTINGS_ID_BACKLIGHT]);
}
