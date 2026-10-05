#include <8051.h>
#include <stdint.h>

#include "backlight/backlight.h"
#include "buttons/buttons.h"
#include "cw/keyer.h"
#include "eeprom/eeprom.h"
#include "gfx/gfx.h"
#include "lcd/lcd.h"
#include "menu/menu.h"
#include "settings/settings.h"
#include "sidetone/sidetone.h"
#include "tick/tick.h"

/* Repeats that came due during a slow redraw take effect before the next redraw; the limit keeps a pass short. */
#define MAIN_EVENTS_PER_PASS 4U

void main(void) {
  uint8_t event_count;
  button_event_t event;

  tick_init();
  /* The only place that enables interrupts. delay_ms() in buttons_init() and lcd_init() needs the tick. */
  EA = 1;
  eeprom_init();
  /* The buttons are sampled first, so a recovery start never shows the stored contrast and backlight. */
  buttons_init();
  (void)settings_load();
  if (buttons_is_recovery_start()) {
    settings_use_display_defaults();
  }
  sidetone_init(settings_get(SETTINGS_ID_TONE_HZ));
  keyer_init();
  lcd_init();
  lcd_set_contrast((uint8_t)settings_get(SETTINGS_ID_CONTRAST));
  backlight_init((uint8_t)settings_get(SETTINGS_ID_BACKLIGHT));
  gfx_init();
  menu_init();
  if (buttons_is_recovery_start()) {
    menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS);
  }

  for (;;) {
    for (event_count = 0U; event_count < MAIN_EVENTS_PER_PASS; ++event_count) {
      event = buttons_poll();
      if (event == BUTTON_EVENT_NONE) {
        break;
      }
      menu_handle(event);
    }
    menu_poll();
    sidetone_poll();
    if (menu_needs_redraw()) {
      menu_render();
    }
    lcd_flush_step();
  }
}
