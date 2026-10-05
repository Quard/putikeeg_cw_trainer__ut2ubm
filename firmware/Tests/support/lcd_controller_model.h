#ifndef LCD_CONTROLLER_MODEL_H
#define LCD_CONTROLLER_MODEL_H

/*
 * Behavioral model of an ST756x-family controller as the driver uses it: 132 x 65 RAM (9 pages), page and column
 * address commands, column auto-increment, and the state of the display commands. One global instance.
 * Not a hardware reference: it implements the command set the driver assumes.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "lcd_protocol_decoder.h"

#define LCD_MODEL_PAGE_COUNT 9U
#define LCD_MODEL_COLUMN_COUNT 132U
/* RAM content after lcd_model_reset(). Not zero, so that a missing clear is visible. */
#define LCD_MODEL_POWER_ON_PATTERN 0x5AU

typedef struct {
  uint8_t ram[LCD_MODEL_PAGE_COUNT][LCD_MODEL_COLUMN_COUNT];
  uint8_t page;
  uint16_t column;
  bool is_seg_reversed;  /* 0xA1 */
  bool is_com_reversed;  /* 0xC8 */
  bool is_display_on;    /* 0xAF */
  bool is_inverted;      /* 0xA7 */
  bool is_all_pixels_on; /* 0xA5 */
  uint8_t bias_command;  /* last 0xA2 / 0xA3 */
  uint8_t regulation_ratio;
  uint8_t power_control;
  uint8_t start_line;
  uint8_t contrast;
  bool is_contrast_pending;     /* 0x81 seen, value byte expected */
  size_t ram_overflow_count;    /* data bytes outside the RAM */
  size_t unknown_command_count; /* command bytes the model does not know */
  size_t data_in_command_count; /* data byte while the contrast value was expected */
} lcd_model_t;

/** Fills the RAM with LCD_MODEL_POWER_ON_PATTERN and sets the reset state (all flags false, counters 0). */
void lcd_model_reset(void);

/** Applies @p count decoded bytes in order. */
void lcd_model_apply(const lcd_decoded_byte_t* bytes, size_t count);

const lcd_model_t* lcd_model_state(void);

/** Returns the RAM bit for screen pixel (@p x, @p y) at RAM column x + @p column_offset. False outside the RAM. */
bool lcd_model_pixel(uint8_t x, uint8_t y, uint8_t column_offset);

#endif /* LCD_CONTROLLER_MODEL_H */
