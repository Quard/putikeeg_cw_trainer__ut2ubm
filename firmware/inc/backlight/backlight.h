#ifndef BACKLIGHT_BACKLIGHT_H
#define BACKLIGHT_BACKLIGHT_H

/* LCD backlight brightness, levels 0 (off) to 10 (full). Call from the main loop only. */

#include <stdint.h>

#define BACKLIGHT_LEVEL_MAX 10U

/** Configures PWMB channel 8 on P0.3 and sets @p level. */
void backlight_init(uint8_t level);

/** Sets the duty to level x 10 %. Values above BACKLIGHT_LEVEL_MAX become BACKLIGHT_LEVEL_MAX. */
void backlight_set_level(uint8_t level);

#endif /* BACKLIGHT_BACKLIGHT_H */
