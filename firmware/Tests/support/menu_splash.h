/*
 * SPDX-FileCopyrightText: 2026 Vadym Zakovinko
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef MENU_SPLASH_H
#define MENU_SPLASH_H

/*
 * Setup helper for the menu tests that start after the startup screen (references/internal/menu_subsystem_spec.md
 * section 17.8). Include it in one test source per executable.
 */

#include "menu/menu.h"
#include "support/test_assert.h"

/** Ends the start-up screen with a menu/back press and draws the result, as the main loop does. */
static inline void menu_test_skip_splash(void) {
  TEST_ASSERT_EQUAL(MENU_SCREEN_SPLASH, menu_screen());
  menu_handle(BUTTON_EVENT_MENU);
  TEST_ASSERT(menu_screen() != MENU_SCREEN_SPLASH);
  if (menu_needs_redraw()) {
    menu_render();
  }
}

#endif /* MENU_SPLASH_H */
