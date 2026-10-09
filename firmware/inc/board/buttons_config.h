/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef BOARD_BUTTONS_CONFIG_H
#define BOARD_BUTTONS_CONFIG_H

/* Button timing in ms (references/internal/ui-ux.md section 6.2). Every value can be overridden with -D. */

#ifndef BUTTONS_DEBOUNCE_MS
#define BUTTONS_DEBOUNCE_MS 20U
#endif
#ifndef BUTTONS_REPEAT_DELAY_MS
#define BUTTONS_REPEAT_DELAY_MS 400U
#endif
#ifndef BUTTONS_REPEAT_PERIOD_MS
#define BUTTONS_REPEAT_PERIOD_MS 80U
#endif
/* Pull-up settle time before the first sample. */
#ifndef BUTTONS_SETTLE_MS
#define BUTTONS_SETTLE_MS 1U
#endif
/* MENU held this long from the first sample gives the display recovery start (ui-ux.md section 8.1). */
#ifndef BUTTONS_RECOVERY_HOLD_MS
#define BUTTONS_RECOVERY_HOLD_MS 50U
#endif

#ifndef BUTTONS_LONG_PRESS_MS
#define BUTTONS_LONG_PRESS_MS 800U /* ui-ux.md section 6.2, decision UX-23 */
#endif

#if (BUTTONS_REPEAT_PERIOD_MS == 0) || (BUTTONS_REPEAT_DELAY_MS == 0)
#error "The repeat delay and period must be above 0"
#endif
#if BUTTONS_LONG_PRESS_MS == 0
#error "The long-press time must be above 0"
#endif

#endif /* BOARD_BUTTONS_CONFIG_H */
