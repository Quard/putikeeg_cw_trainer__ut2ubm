#include <8051.h>
#include <stdint.h>

#include "lcd/lcd.h"
#include "lcd/lcd_fb.h"
#include "tick/tick.h"

/* Bring-up image: border, top-left triangle and a diagonal, to check orientation and pixel mapping on the panel. */
static void draw_test_pattern(void) {
  int16_t i;
  int16_t j;

  for (i = 0; i < (int16_t)LCD_WIDTH; ++i) {
    lcd_fb_set_pixel(i, 0, LCD_PIXEL_ON);
    lcd_fb_set_pixel(i, (int16_t)LCD_HEIGHT - 1, LCD_PIXEL_ON);
  }
  for (i = 0; i < (int16_t)LCD_HEIGHT; ++i) {
    lcd_fb_set_pixel(0, i, LCD_PIXEL_ON);
    lcd_fb_set_pixel((int16_t)LCD_WIDTH - 1, i, LCD_PIXEL_ON);
    lcd_fb_set_pixel(i, i, LCD_PIXEL_ON);
  }
  for (i = 0; i < 16; ++i) {
    for (j = 0; (i + j) < 16; ++j) {
      lcd_fb_set_pixel(j, i, LCD_PIXEL_ON);
    }
  }
}

void main(void) {
  tick_init();
  /* The only place that enables interrupts. delay_ms() inside lcd_init() needs the tick. */
  EA = 1;
  lcd_init();
  draw_test_pattern();
  lcd_flush();
  for (;;) {
  }
}
