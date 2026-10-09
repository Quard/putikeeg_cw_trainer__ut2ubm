/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "lcd_controller_model.h"

#include <string.h>

static lcd_model_t model;

void lcd_model_reset(void) {
  memset(&model, 0, sizeof(model));
  memset(model.ram, LCD_MODEL_POWER_ON_PATTERN, sizeof(model.ram));
}

static void model_apply_command(uint8_t value) {
  if (model.is_contrast_pending) {
    model.contrast = (uint8_t)(value & 0x3FU);
    model.is_contrast_pending = false;
  } else if (value <= 0x0FU) {
    model.column = (uint16_t)((model.column & 0xF0U) | value);
  } else if (value <= 0x1FU) {
    model.column = (uint16_t)((model.column & 0x0FU) | ((value & 0x0FU) << 4));
  } else if (value <= 0x27U) {
    model.regulation_ratio = (uint8_t)(value & 0x07U);
  } else if (value <= 0x2FU) {
    model.power_control = (uint8_t)(value & 0x07U);
  } else if ((value >= 0x40U) && (value <= 0x7FU)) {
    model.start_line = (uint8_t)(value & 0x3FU);
  } else if (value == 0x81U) {
    model.is_contrast_pending = true;
  } else if ((value == 0xA0U) || (value == 0xA1U)) {
    model.is_seg_reversed = value == 0xA1U;
  } else if ((value == 0xA2U) || (value == 0xA3U)) {
    model.bias_command = value;
  } else if ((value == 0xA4U) || (value == 0xA5U)) {
    model.is_all_pixels_on = value == 0xA5U;
  } else if ((value == 0xA6U) || (value == 0xA7U)) {
    model.is_inverted = value == 0xA7U;
  } else if ((value == 0xAEU) || (value == 0xAFU)) {
    model.is_display_on = value == 0xAFU;
  } else if ((value >= 0xB0U) && (value <= 0xB8U)) {
    model.page = (uint8_t)(value & 0x0FU);
  } else if ((value == 0xC0U) || (value == 0xC8U)) {
    model.is_com_reversed = value == 0xC8U;
  } else {
    ++model.unknown_command_count;
  }
}

static void model_apply_data(uint8_t value) {
  if (model.is_contrast_pending) {
    ++model.data_in_command_count;
    model.is_contrast_pending = false;
  }
  if ((model.page < LCD_MODEL_PAGE_COUNT) && (model.column < LCD_MODEL_COLUMN_COUNT)) {
    model.ram[model.page][model.column] = value;
  } else {
    ++model.ram_overflow_count;
  }
  if (model.column < 0xFFFFU) {
    ++model.column;
  }
}

void lcd_model_apply(const lcd_decoded_byte_t* bytes, size_t count) {
  size_t i;
  for (i = 0U; i < count; ++i) {
    if (bytes[i].is_data) {
      model_apply_data(bytes[i].value);
    } else {
      model_apply_command(bytes[i].value);
    }
  }
}

const lcd_model_t* lcd_model_state(void) { return &model; }

bool lcd_model_pixel(uint8_t x, uint8_t y, uint8_t column_offset) {
  const unsigned column = (unsigned)x + column_offset;
  const unsigned page = (unsigned)y >> 3;
  if ((column >= LCD_MODEL_COLUMN_COUNT) || (page >= LCD_MODEL_PAGE_COUNT)) {
    return false;
  }
  return (model.ram[page][column] & (1U << (y & 7U))) != 0U;
}
