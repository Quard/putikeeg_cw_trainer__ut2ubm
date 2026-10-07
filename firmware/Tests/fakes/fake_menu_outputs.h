#ifndef FAKE_MENU_OUTPUTS_H
#define FAKE_MENU_OUTPUTS_H

/*
 * Host fakes of the outputs that the menu drives: lcd_set_contrast() (fake_lcd_contrast.c),
 * backlight_init() and backlight_set_level() (fake_backlight.c) and stats_reset() (fake_stats.c).
 * Each records its calls.
 */

#include <stdbool.h>
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

/** Clears the count and sets the stats_reset() result back to true. */
void fake_stats_reset(void);
size_t fake_stats_reset_count(void);
/** Sets the result of the next stats_reset() calls. */
void fake_stats_set_reset_result(bool is_stored);

#endif /* FAKE_MENU_OUTPUTS_H */
