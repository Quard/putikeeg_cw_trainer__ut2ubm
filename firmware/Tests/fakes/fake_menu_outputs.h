#ifndef FAKE_MENU_OUTPUTS_H
#define FAKE_MENU_OUTPUTS_H

/*
 * Host fakes of the outputs that the menu drives: lcd_set_contrast() (fake_lcd_contrast.c),
 * backlight_init() and backlight_set_level() (fake_backlight.c) and stats_reset() (fake_stats.c).
 * Each records its calls.
 */

#include <stddef.h>
#include <stdint.h>

#define FAKE_OUTPUT_HISTORY 128U

void fake_lcd_contrast_reset(void);
size_t fake_lcd_contrast_count(void);
/** Returns call @p index, or 0xFF when out of range. */
uint8_t fake_lcd_contrast_at(size_t index);
/** Returns the last value, or 0xFF without a call. */
uint8_t fake_lcd_contrast_last(void);

void fake_backlight_reset(void);
size_t fake_backlight_init_count(void);
/** Returns the level count: backlight_init() and backlight_set_level() calls. */
size_t fake_backlight_count(void);
uint8_t fake_backlight_at(size_t index);
uint8_t fake_backlight_last(void);

void fake_stats_reset(void);
size_t fake_stats_reset_count(void);

#endif /* FAKE_MENU_OUTPUTS_H */
