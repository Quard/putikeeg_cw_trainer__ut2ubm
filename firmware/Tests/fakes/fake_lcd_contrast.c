#include "fake_menu_outputs.h"
#include "lcd/lcd.h"

static uint8_t fake_values[FAKE_OUTPUT_HISTORY];
static size_t fake_count;

void fake_lcd_contrast_reset(void) { fake_count = 0U; }

size_t fake_lcd_contrast_count(void) { return fake_count; }

uint8_t fake_lcd_contrast_at(size_t index) { return (index < fake_count) ? fake_values[index] : 0xFFU; }

uint8_t fake_lcd_contrast_last(void) { return (fake_count != 0U) ? fake_values[fake_count - 1U] : 0xFFU; }

void lcd_set_contrast(uint8_t level) {
  if (fake_count < FAKE_OUTPUT_HISTORY) {
    fake_values[fake_count] = level;
    ++fake_count;
  }
}
