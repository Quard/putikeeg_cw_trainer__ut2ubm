# Menu subsystem: implementation specification

Owner: `architector`. Status: approved for implementation. Readers: `c-embedded-developer`, `qa-embedded`,
`ui-ux-embedded-designer`, `technical-writer`.

## 1. Sources and precedence

| Source | Role |
| --- | --- |
| This document | Interfaces, data formats, behavior, task split and acceptance criteria. It refines the plan and does not change any user decision. |
| `references/internal/ui-ux.md` (owner `ui-ux-embedded-designer`) | Screen layout, label strings, value formats, edit-mode marker, dialog and placeholder layout, auto-repeat and debounce timing values. |
| `references/context/STC8H_datasheet.md` | Register facts. Section 8 lists the facts that the developer must check. |
| `AGENTS.md` | Project rules (memory, software SPI, interrupts, team workflow). |

Precedence rules:

- For visual details (pixel positions, strings, markers) and timing constants, `ui-ux.md` wins.
- For behavior (states, events, persistence, ranges, defaults, record format, interfaces), this document and the
  plan win.
- When `ui-ux.md` describes a different behavior, stop and report the conflict to `architector`. Do not choose one
  yourself.
- After implementation starts, only `architector` changes this document.

`AGENTS.md` names `llm.md`. That file does not exist (`llm.txt` is empty). Do not create it.

## 2. Scope

In scope:

- Text output on `lcd_fb` with a project u8g2 font decoder and the `spleen5x8` font.
- Button input: four buttons, debounce, auto-repeat on prev and next.
- Backlight PWM on P0.3, levels 0-10.
- IAP EEPROM driver and the EEPROM map.
- Settings with ranges, defaults, clamps, the effective-speed rule and persistence in EEPROM page 0.
- A table-driven menu: main menu, Settings, CW and Display lists, edit mode, placeholder mode screens,
  reset-statistics dialog, message screen.
- An empty `stats_reset()` hook.
- The start-up recovery: MENU held at power-on gives default contrast and backlight for that session.
- A low-supply guard for EEPROM writes with the MCU low-voltage detector.
- `main.c` start-up and main loop.

Out of scope:

- The receiving, sending and free-input modes.
- Statistics storage, keyer, sidetone, power and sleep handling.

These items must pass before the work is done: `firmware/build.sh`, `firmware/test.sh`, `check_asm.sh`, the QA
verdict and the architect review. Device checks in section 13 stay open until the user runs them.

## 3. Plan refinements found during specification

These items are necessary to make the approved plan work. They add no new user-facing behavior.

### 3.1 The third-party font driver is not used (user decision)

`firmware/drivers/u8g2_font_render` stays unchanged and out of the build. Reasons, measured with SDCC 4.6.0:

- The driver compiles only with `--stack-auto` (error 92 on the 7-byte pixel callback).
- In a `--stack-auto` file, calls from normal-model files and calls to SDCC math helpers (`%`, `*`, struct copy)
  get their arguments in the wrong place. The linker gives only a warning or no message at all.
- A safe driver needs a larger rewrite.

`gfx` gets its own small u8g2 decoder for the 8051 (sections 4.1 and 4.2). It uses no callbacks, no
`--stack-auto`, no 32-bit arithmetic and no struct copies. It draws straight into `lcd_fb`.

### 3.2 Text placement

The u8g2 format gives each glyph a y offset from the baseline. The third-party driver ignores the bounding box
y offset, so for `spleen5x8` its text is one row too low. The project decoder uses the full formula (section 4.2).
Thus a text row `r` drawn at `y = 8r` covers pixel rows `8r` to `8r + 7` (`ui-ux.md` UX-18).

`ui-ux.md` section 3.3 describes the result as "the cell for row `r` is drawn at `y = 8r - 1`" with the old
driver. The visible result is the same and binding: cell row 0 is on pixel row `8r`. With the project decoder,
`gfx` and `menu` use `y = 8r`.

### 3.3 `inc/u8g2_fonts.h` fails the host build

`const static uint8_t u8g2_font_spleen5x8_mf[...]` causes `-Werror=old-style-declaration` (GCC 16, `-Wextra`).

Decision: change that one declaration to `static const`. The file is not in `firmware/drivers/`. Make no other
change to the file. With the fix, both arrays compile with the host flags. SDCC puts both arrays in the `CONST`
(code) area (tested).

### 3.4 Task order: `stats` before `menu`

The firmware build links every file under `src/`. `menu.c` calls `stats_reset()`. Thus the `stats` step comes
before the `menu` step. All other steps keep the plan order.

### 3.5 Alignment with `ui-ux.md`

The designer wrote `ui-ux.md` at the same time as this document. This document takes these designer decisions.
None of them changes a user decision of the plan.

- The tree has the submenus CW and Display under Settings (`ui-ux.md` section 5.3). No list has more than 7
  items, so no list scrolls (section 6.2). The plan stack depth of 3 is fully used.
- After `Yes` in the reset dialog, a message screen `Statistics reset` / `OK` shows (UX-8).
- A failed save shows the message `Not saved` / `OK`. This answers `ui-ux.md` Q-4.
- The auto-repeat follows the button that was pressed last. Events that come in the same poll come out in the
  order MENU, ENTER, PREV, NEXT (`ui-ux.md` section 6.2).
- A slow main loop loses no repeat step (UX-16). Thus the repeat time moves by the period, and the main loop
  handles up to 4 events before a redraw (sections 4.5 and 4.11). The plan says "get a button event". This is a
  small extension of it.

### 3.6 Decisions that the user added after the plan

- Display recovery start (`ui-ux.md` UX-19, section 8.1): when MENU (P0.2) is held for 50 ms from the first
  start-up sample, contrast and backlight use their defaults for that session, and the message `Display Defaults`
  shows. The button is sampled before contrast and backlight are applied (sections 4.5 and 4.11).
- Save rule (`ui-ux.md` UX-15): the save on Settings exit needs both conditions. (1) At least one edit step
  changed a value since the last load or successful save. (2) The current values differ from the stored record.
  Thus a recovery start stays temporary until the user edits a value (section 4.8).
- The four speed rows use the labels `RX` and `TX` (`ui-ux.md` Q-2).
- After `Yes` in the reset dialog, the screen `Statistics reset` / `OK` shows.
- `gfx` places a text row `r` on pixel rows `8r` to `8r + 7` (`ui-ux.md` UX-18, section 3.2).
- The third-party font driver stays unchanged and out of the build. `gfx` has its own decoder (section 3.1).

Architect answers to the open designer points:

- Q-4: `eeprom_write` reads back each programmed byte. A mismatch, a CMD_FAIL or a low supply stops the save with
  `SETTINGS_SAVE_FAILED`. The menu shows `Not saved` / `OK`. The values stay active and the baseline does not
  change, so the next exit from Settings tries again. The magic is written last, so a stopped save leaves no valid
  record, and the next power-on uses the defaults.
- Q-5: the MCU runs from an HT7833 3.3 V regulator on the battery (`hardware/putikeeg_cw_trainer.kicad_sch`). The
  firmware sets the low-voltage detector to 3.0 V without the reset function. `eeprom_init` clears `ENLVR`,
  because the ISP options of the chip can leave it set (section 4.7). No LVD interrupt.
  `eeprom.c` reads the detector flag before each erase and each program command. A set flag stops the
  operation with `EEPROM_ERR_LOW_SUPPLY` (section 4.7). A low-voltage reset is not used: at a low battery it
  can cause a reset loop, and the supply behavior is not measured. The residual risk is a supply drop during the
  one 4-6 ms erase (section 14).

## 4. Interfaces

Rules for all new code:

- C11 as SDCC accepts it (`--std=sdcc11`), small model. No file gets `--stack-auto`.
- Module state is in xdata (`MEM_XDATA`). Tables and strings are in code memory (`const`, `MEM_CODE`).
- Constants are `#define` values with a `U` suffix. Types are `typedef uint8_t`, as in `lcd`. Interfaces use no
  `enum` types.
- Boolean names start with `is`, `has`, `should`, `can`, `does` or `was`.
- Parameters after the first of a non-leaf, non-reentrant function can have the storage class `MEM_XDATA`. SDCC
  then keeps them in xdata, not in internal RAM. The prototype and the definition use the same qualifiers. Do not
  use it on the first parameter (it goes in registers), on interrupt code, or on leaf functions (SDCC overlays
  their parameters). For a pointer parameter, `MEM_XDATA` after the `*` places the pointer itself. `MEM_XDATA`
  before the type tells where the pointed-to data is. The host build sees no change.
- Only the main loop calls these modules. There is no new interrupt handler. No module clears `EA`.
- Hardware access goes through small headers in `inc/board/` or `inc/mcu/`. Each has a host replacement in
  `firmware/Tests/fakes/` under the same relative path, as `board/lcd_pins.h` does.

### 4.1 Font decoder interface `src/gfx/font_u8g2.h`, `src/gfx/font_u8g2.c`

The decoder is private to `gfx`. Only `gfx.c` includes `src/gfx/font_u8g2.h` (the same pattern as
`src/lcd/lcd_commands.h`). Host tests include it from `firmware/src/gfx/`.

```c
#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

/** Selects the u8g2 font at @p font and reads its header. Returns false for NULL and keeps the current font. */
bool font_u8g2_select(const uint8_t MEM_CODE *font);

/** Returns the cell height of the selected font (bounding box height), or 0 with no font. */
uint8_t font_u8g2_cell_height(void);

/**
 * Draws glyph @p encoding with its cell top-left at (@p x, @p y). Glyph pixels get @p fg_color, the other pixels
 * of the glyph box get @p bg_color (LCD_PIXEL_OFF, LCD_PIXEL_ON or LCD_PIXEL_INVERT).
 * Returns the glyph pitch. Returns 0 and draws nothing when no font is selected or the font has no such glyph.
 */
int8_t font_u8g2_draw_glyph(int16_t x, MEM_XDATA int16_t y, MEM_XDATA uint8_t encoding,
                            MEM_XDATA uint8_t fg_color, MEM_XDATA uint8_t bg_color);

/** Returns the pitch of glyph @p encoding without drawing it. Returns 0 for a missing glyph or no font. */
int8_t font_u8g2_glyph_pitch(uint8_t encoding);
```

`(x, y)` is the top-left pixel of the text cell. For `spleen5x8`, a glyph at `y` covers rows `y .. y + 7` exactly,
so text row `r` uses `y = 8r` (`ui-ux.md` UX-18).

### 4.2 Font decoder rules

Format (u8g2 font format, as in `inc/u8g2_fonts.h`):

- Header of 23 bytes. Byte 0 glyph count. Byte 1 bounding box mode. Bytes 2-8 bit widths: 0-run, 1-run, glyph
  width, glyph height, glyph x offset, glyph y offset, pitch. Bytes 9-12 bounding box width, height, x offset,
  y offset (bytes 11 and 12 are signed). Bytes 13-16 ascent and descent values (not used). Bytes 17-18, 19-20
  and 21-22: start offsets of `A`, `a` and the Unicode table, big-endian, counted from byte 23.
- Glyph list from byte 23: encoding byte, size byte (offset to the next glyph), then an LSB-first bitstream. A size
  byte of 0 ends the list.
- Glyph bitstream: width, height (unsigned), x offset, y offset, pitch (signed: the raw value minus
  `1 << (bits - 1)`), then the RLE runs. Each run has a 0-run count and a 1-run count. Then each 1 bit adds one
  repeat of the run, and a 0 bit ends the repeats. Pixels fill the glyph box row by row, 0-runs with
  `bg_color` and 1-runs with `fg_color`. The glyph ends after `width * height` pixels.

Placement:

- Glyph left = `x + glyph_x_offset`.
- Glyph top = `y + (bbox_height + bbox_y_offset) - (glyph_height + glyph_y_offset)`. This applies the bounding box
  y offset, so the cell top is `y` for each glyph and each font.

Search:

- Encodings `A` to `Z` start at the `A` offset. Encodings `a` to `z` start at the `a` offset. Other encodings
  start at byte 23. Step by the size byte
  until the encoding matches or the size byte is 0.
- `encoding` is `uint8_t`, so the result does not depend on the signedness of `char`. The Unicode table is not
  used.

Implementation rules:

- Normal small model. No `--stack-auto`, no `__reentrant`, no function pointers.
- No 32-bit arithmetic and no struct copies. Row and column counters are `uint8_t`. The pixel loop has no `%`,
  `/` or `*`.
- Font data is read through `const uint8_t MEM_CODE *` (MOVC on the target). The selected-font state (font
  pointer and the header fields that it needs) is in xdata, not more than 16 B.
- Working variables that SDCC puts in DSEG go in xdata (`static MEM_XDATA`), so that the decoder adds at most
  8 B of static internal RAM.
- It writes pixels with `lcd_fb_set_pixel()` or directly into `lcd_fb_page()` bytes with `lcd_fb_mark_dirty()`. It
  never writes outside the framebuffer. Pixels outside the screen are dropped.
- A glyph with width 0 or height 0 draws nothing and returns its pitch.
- Main loop only. Not reentrant.

Golden fixture (test oracle):

- `firmware/Tests/tools/gen_font_golden.py` decodes the u8g2 format from `inc/u8g2_fonts.h` with its own Python
  code (written from the format description above, not from the C decoder).
- It writes `firmware/Tests/fixtures/font_golden.h`: for each glyph 0x20-0x7E of `spleen5x8`, 8 row bytes (bit 4 =
  column 0) and the pitch. For `spleen8x16`, the glyphs `A`, `g` and `0` (16 rows each) and the pitch.
- Commit the generator and the fixture. The file header names the command that makes it.
- Hand-checked vectors in the test (the `A` cell in T1) protect against an error that the generator and the
  decoder share.

### 4.3 Fonts `inc/gfx/fonts.h`, `src/gfx/fonts.c`

```c
#define FONTS_ID_5X8 0U   /* u8g2_font_spleen5x8_mf, the UI font */
#define FONTS_ID_8X16 1U  /* u8g2_font_spleen8x16_mf */
#define FONTS_COUNT 2U

/** Returns the u8g2 font data of @p font_id, or NULL for an unknown id. */
const uint8_t MEM_CODE *fonts_get(uint8_t font_id);
```

`fonts.c` is the only file that includes `u8g2_fonts.h`. Before the include, it defines `U8G2_FONT_SECTION(name)`
as empty.

SDCC puts both `const` arrays in code memory (section 3.3). When SDCC needs a cast from the array to
`const uint8_t MEM_CODE *`, the cast is only in `fonts_get`.

### 4.4 Graphics `inc/gfx/gfx.h`, `src/gfx/gfx.c`

```c
#define GFX_TEXT_NORMAL 0U    /* glyph pixels on, cell background off */
#define GFX_TEXT_INVERTED 1U  /* glyph pixels off, cell background on */
#define GFX_U16_TEXT_SIZE 6U  /* "65535" plus NUL */
#define GFX_FONT_5X8_WIDTH 5U
#define GFX_FONT_5X8_HEIGHT 8U

void gfx_init(void);
bool gfx_set_font(uint8_t font_id);
int16_t gfx_draw_text(int16_t x, MEM_XDATA int16_t y, const char *MEM_XDATA text, MEM_XDATA uint8_t color);
uint16_t gfx_text_width(const char *text);
void gfx_fill_rect(int16_t x, MEM_XDATA int16_t y, MEM_XDATA int16_t width, MEM_XDATA int16_t height,
                   MEM_XDATA uint8_t color);
uint8_t gfx_format_u16(uint16_t value, char *buffer);
```

`gfx_init`: the same as `gfx_set_font(FONTS_ID_5X8)`.

`gfx_set_font`:

- For an unknown id, it returns false and keeps the current font.
- Else it calls `font_u8g2_select(fonts_get(font_id))` and returns true.

`gfx_draw_text`:

- `y` is the top row of the text cell. A 5x8 string at `y` covers rows `y .. y + 7` exactly (section 3.2).
- For each character, it calls `font_u8g2_draw_glyph` with the color pair of `color`: `GFX_TEXT_NORMAL` gives
  fg `LCD_PIXEL_ON` and bg `LCD_PIXEL_OFF`, `GFX_TEXT_INVERTED` gives fg `LCD_PIXEL_OFF` and bg `LCD_PIXEL_ON`.
- It returns the x after the last character: x plus the sum of the pitches (5 per character for 5x8).
- Pixels outside the screen are dropped.
- A character without a glyph gives pitch 0, so x does not move.
- When no font is set, `text` is NULL or `color` is not one of the two values, it draws nothing and returns `x`.
- Strings use ASCII 0x20-0x7E only.

`gfx_text_width`: the sum of the positive pitches of `text` (`font_u8g2_glyph_pitch`). With no font set or a NULL
`text`, it returns 0.

`gfx_fill_rect`:

- `color` is `LCD_PIXEL_OFF`, `LCD_PIXEL_ON` or `LCD_PIXEL_INVERT`. For other values, it does nothing.
- When `width <= 0` or `height <= 0`, it does nothing.
- It clips the rectangle to the screen first and visits only visible pixels.
- It can write framebuffer bytes directly for speed. In that case, it calls `lcd_fb_mark_dirty()` for every page
  that it changes.

`gfx_format_u16`: it writes the decimal digits of `value` without leading zeros, and a NUL, into `buffer`. The
buffer has at least `GFX_U16_TEXT_SIZE` bytes. It returns the digit count, 1..5. It does not use `printf`.

### 4.5 Buttons `inc/buttons/buttons.h`, `src/buttons/buttons.c`

```c
typedef uint8_t button_event_t;

#define BUTTON_EVENT_NONE 0x00U
#define BUTTON_EVENT_NEXT 0x01U
#define BUTTON_EVENT_PREV 0x02U
#define BUTTON_EVENT_MENU 0x03U   /* menu/back */
#define BUTTON_EVENT_ENTER 0x04U
#define BUTTON_EVENT_REPEAT_FLAG 0x80U
#define BUTTON_EVENT_CODE(event) ((uint8_t)((event) & 0x7FU))

/** Configures the pins and pull-ups, checks for the display recovery start and takes the start state. */
void buttons_init(void);

/** Returns true when MENU was held through the recovery window of buttons_init(). */
bool buttons_is_recovery_start(void);

/** Returns at most one event. The main loop calls it until it returns BUTTON_EVENT_NONE (section 4.11). */
button_event_t buttons_poll(void);
```

`inc/board/buttons_config.h`. Every value can be changed with `-D`. The values are from `ui-ux.md` section 6.2.

```c
#define BUTTONS_DEBOUNCE_MS 20U
#define BUTTONS_REPEAT_DELAY_MS 400U
#define BUTTONS_REPEAT_PERIOD_MS 80U
#define BUTTONS_SETTLE_MS 1U            /* pull-up settle time before the first sample */
#define BUTTONS_RECOVERY_HOLD_MS 50U     /* ui-ux.md section 8.1 */
```

`inc/board/button_pins.h` (target). The host replacement is `Tests/fakes/board/button_pins.h`.

- `BUTTON_BIT_NEXT 0x01U` (P0.0), `BUTTON_BIT_PREV 0x02U` (P0.1), `BUTTON_BIT_MENU 0x04U` (P0.2),
  `BUTTON_BIT_ENTER 0x08U` (P3.2).
- `BUTTON_PINS_CONFIGURE()`:
  1. `P_SW2 |= 0x80` (EAXFR).
  2. `P0PU |= 0x07` and `P3PU |= 0x04`.
  3. High-impedance input mode for the four pins: `P0M1 |= 0x07`, `P0M0 &= ~0x07`, `P3M1 |= 0x04`,
     `P3M0 &= ~0x04`. Each mode register gets one `ORL` or `ANL`.
- `BUTTON_PINS_READ_PRESSED()` returns a `uint8_t` mask. A pin that reads low gives a 1 bit. It reads pin bits only.

Poll algorithm. It is binding, so that the tests are deterministic.

1. Read the raw mask and call `tick_now()` exactly once. Elapsed time is `(uint16_t)(now - start)`. Do not call
   `tick_elapsed_since()` or `tick_has_elapsed()`, because the host fake gives only `tick_now()`.
2. For each button:
   - When the raw bit is not equal to the candidate bit, store the new candidate and its change time.
   - When the raw bit is equal to the candidate, the candidate is not equal to the debounced bit, and at least
     `BUTTONS_DEBOUNCE_MS` passed since the change time, the debounced bit takes the candidate value. The same
     rule applies to press and release.
   - A released-to-pressed change sets the pending bit of the button.
   - On a released-to-pressed change of NEXT or PREV, that button becomes the repeat button, also while the
     other one is held. Set the repeat time to now and the wait to `BUTTONS_REPEAT_DELAY_MS`.
   - A pressed-to-released change of the repeat button clears the repeat button. The other button does not
     start to repeat again until the user presses it again.
3. When a pending bit is set, clear one and return its event. The order is MENU, ENTER, PREV, NEXT.
4. Look for a repeat. All of these must be true: no pending bit is set, a repeat button exists, and at least the
   wait time passed since the repeat time. Then add the wait time to the repeat time. Do not set it to now.
   Set the wait to `BUTTONS_REPEAT_PERIOD_MS`. Return the event of the repeat button with
   `BUTTON_EVENT_REPEAT_FLAG`. Because the repeat time moves by the period, a slow main loop gets the repeats
   that are due on its next pass. Over time the rate stays one repeat per period, and no step is lost
   (`ui-ux.md` UX-16).
5. Else return `BUTTON_EVENT_NONE`.

`buttons_init()` does these steps:

1. `BUTTON_PINS_CONFIGURE()`.
2. `delay_ms(BUTTONS_SETTLE_MS)`.
3. Recovery window. Read the raw mask and `tick_now()` as the first sample and its time. Repeat once per tick
   (wait until `tick_now()` changes, then read the raw mask):
   - When MENU is not pressed in a sample, the window ends at once. The start is normal.
   - When MENU is pressed in the sample at `BUTTONS_RECOVERY_HOLD_MS` or later after the first sample, the window
     ends. The recovery start is active.
   - Only MENU counts. ENTER, PREV and NEXT have no start-up function.
   The window adds at most 51 ms, and only while MENU is held.
4. The last raw mask read becomes the candidate and debounced bits. Change times = now. Pending and repeat state
   are clear.

`delay_ms()` and the window need the tick and `EA = 1`, so `buttons_init()` comes after them (section 4.11). A
button that is held at power-up gives no event until the user releases it and presses it again. Thus the held
MENU of a recovery start does not close the message screen.
MENU and ENTER never repeat. A held ENTER (for example a closed key on the paddle socket) gives one event only.
There are no release events and no long-press events.

### 4.6 Backlight `inc/backlight/backlight.h`, `src/backlight/backlight.c`

```c
#define BACKLIGHT_LEVEL_MAX 10U

/** Configures PWMB channel 8 on P0.3 and sets @p level. */
void backlight_init(uint8_t level);

/** Sets the duty to level x 10 %. Values above BACKLIGHT_LEVEL_MAX become BACKLIGHT_LEVEL_MAX. */
void backlight_set_level(uint8_t level);
```

`inc/board/backlight_config.h`. Both values can be changed with `-D`.

```c
#define BACKLIGHT_PWM_HZ 1000U        /* not checked on the device */
#define BACKLIGHT_IS_ACTIVE_LOW 0     /* 1 sets CC8P; polarity not checked on the device */
```

Derived values. They are compile-time values with `#error` checks.

- Counter clock 1 MHz. Prescaler `MCU_CLOCK_HZ / 1000000 - 1` (23 at 24 MHz).
- `BACKLIGHT_PWM_PERIOD` 1000 counts, `ARR = 999`.
- Compare value = `level * 100`. Level 0 gives compare 0 (always inactive). Level 10 gives compare 1000, which is
  ARR + 1 (always active).

`inc/board/backlight_pwm.h` (target). The host replacement `Tests/fakes/board/backlight_pwm.h` records the calls.

`BACKLIGHT_PWM_CONFIGURE()` does these steps in this order:

1. `P_SW2 |= 0x80`.
2. Set `PWMB_ENO`, `PWMB_CCER2` and `PWMB_CR1` to off.
3. `PWMB_PS = 0x80`: C8PS = 10b (P0.3). C5PS-C7PS = 00b, and these channels stay disabled.
4. Prescaler, then ARR, high byte first.
5. `PWMB_CCMR4` = PWM mode 1 with preload. Compare value 0.
6. `PWMB_CCER2`: CC8E. For `BACKLIGHT_IS_ACTIVE_LOW` 1, also CC8P.
7. `PWMB_ENO = 0x40` (ENO8P only).
8. `PWMB_BKR`: MOE.
9. Update event (`PWMB_EGR` UG) to load the prescaler.
10. Counter on: `PWMB_CR1` CEN with ARPE.
11. P0.3 push-pull: `P0M0 |= 0x08`, `P0M1 &= ~0x08`.

`BACKLIGHT_PWM_SET_COMPARE(value)` writes `PWMB_CCR8H`, then `PWMB_CCR8L`.

Hard rules:

- No PWMB output goes to port 2. P2.3 is the PWM8 reset pin and the LCD SCLK.
- Write `PWMB_PS` before any `ENO` bit is set.
- `ENO5P`, `ENO6P` and `ENO7P` stay 0.

### 4.7 EEPROM `inc/eeprom/eeprom.h`, `inc/eeprom/eeprom_map.h`, `src/eeprom/eeprom.c`

```c
typedef uint8_t eeprom_status_t;

#define EEPROM_OK 0U
#define EEPROM_ERR_RANGE 1U  /* address or length outside the EEPROM; nothing done */
#define EEPROM_ERR_FAIL 2U   /* CMD_FAIL was set or the read-back did not match */
#define EEPROM_ERR_LOW_SUPPLY 3U  /* the low-voltage flag was set; the command was not started */

#define EEPROM_SIZE 4096U
#define EEPROM_PAGE_SIZE 512U
#define EEPROM_PAGE_COUNT 8U

eeprom_status_t eeprom_read(uint16_t address, MEM_XDATA uint8_t *MEM_XDATA data, MEM_XDATA uint16_t length);
eeprom_status_t eeprom_write(uint16_t address, const MEM_XDATA uint8_t *MEM_XDATA data,
                             MEM_XDATA uint16_t length);
eeprom_status_t eeprom_erase_page(uint8_t page);

/** Sets the low-voltage detector to 3.0 V without reset and clears its flag. Call once at start-up. */
void eeprom_init(void);

/** Clears the low-voltage flag. Call it right before a group of erase and write calls. */
void eeprom_clear_low_supply(void);
```

`eeprom_read`:

- Range check `length <= EEPROM_SIZE && address <= EEPROM_SIZE - length`. The check cannot overflow.
- It reads byte by byte with the IAP read command.
- On CMD_FAIL, it stops with `EEPROM_ERR_FAIL`.
- Length 0 gives `EEPROM_OK`.

`eeprom_write`:

- The same range check.
- For each byte: the IAP program command, then an IAP read of the same address.
- At the first CMD_FAIL or read-back mismatch, it stops with `EEPROM_ERR_FAIL`. Earlier bytes stay programmed.
- Before each program command, it reads the low-voltage flag. When the flag is set, it stops with
  `EEPROM_ERR_LOW_SUPPLY` and does not start the command.
- It never erases. Programming can only clear bits.

`eeprom_erase_page`:

- When `page >= EEPROM_PAGE_COUNT`, it returns `EEPROM_ERR_RANGE`.
- When the low-voltage flag is set, it returns `EEPROM_ERR_LOW_SUPPLY` and does not erase.
- It erases the 512-byte page at `page * 512` to 0xFF.
- On CMD_FAIL, it returns `EEPROM_ERR_FAIL`.

`eeprom_init`:

1. `RSTCFG &= (uint8_t)~(RSTCFG_ENLVR | RSTCFG_LVDS_MASK)` with `RSTCFG_ENLVR 0x40U` and `RSTCFG_LVDS_MASK 0x03U`.
   One `ANL`. It clears the low-voltage reset and the threshold bits.
2. `RSTCFG |= RSTCFG_LVDS_3V0` (0x03). One `ORL`.
3. Clear `LVDF` in `PCON` with one `ANL`.

It keeps `P54RST` (bit 4) and the other bits. It does not enable the LVD interrupt.

Reason (decision of 2026-10-05): `flash.sh` does not name the low-voltage ISP options, and for such options
`stcgal` keeps the values that are already in the chip. The reset value of `RSTCFG` comes from these options, so
`ENLVR` can be 1. With `ENLVR` = 1 and LVDS = 3.0 V, the MCU resets near 3.0 V. Then the save guard never runs,
and a low battery gives a reset loop. Thus the
firmware sets the bits itself and does not depend on the programmer. `flash.sh` does not change.

Residual risk: with `ENLVR` = 0 there is no low-voltage reset, only the power-on reset. See device check 12.

`eeprom_clear_low_supply`: clears `LVDF` with one `ANL`. The flag stays set after a voltage drop until software
clears it, so this call removes an old drop before a save. `eeprom_read` does not look at the flag.

Each IAP operation does these steps:

1. Enable IAP (`IAPEN`) and set `IAP_TPS`.
2. Set the command, the address and, for a program, the data.
3. Write `0x5A`, then `0xA5`, to the trigger register. Then one `nop`.
4. Read and clear CMD_FAIL.
5. Go idle before the return, on success and on error: `IAP_CONTR = 0`, `IAP_CMD = 0`, `IAP_TRIG = 0`,
   `IAP_ADDRH = 0x80`, `IAP_ADDRL = 0`.

`EA` is never touched. The CPU stops during each IAP operation (an erase takes 4-6 ms). Interrupts wait until the
operation ends.

Compile-time values:

- `EEPROM_IAP_TPS` = `(MCU_CLOCK_HZ + 500000UL) / 1000000UL` (24 at 24 MHz). Outside 1..63, `#error`.
- CMake adds `FLASH_CODE_SIZE` as a compile definition. When `65536 - FLASH_CODE_SIZE != EEPROM_SIZE`, `eeprom.c`
  stops with `#error`.

`inc/mcu/iap_port.h` (target) is the only file that names IAP registers. The host replacement is
`Tests/fakes/mcu/iap_port.h`. Macros: `IAP_PORT_ENABLE()`, `IAP_PORT_SET_COMMAND(cmd)`,
`IAP_PORT_SET_ADDRESS(address)`, `IAP_PORT_WRITE_DATA(value)`, `IAP_PORT_READ_DATA()`, `IAP_PORT_TRIGGER()`,
`IAP_PORT_HAS_FAILED()`, `IAP_PORT_CLEAR_FAIL()`, `IAP_PORT_IDLE()`, `IAP_PORT_LVD_INIT()`,
`IAP_PORT_IS_SUPPLY_LOW()`, `IAP_PORT_CLEAR_SUPPLY_LOW()`. Constants: `IAP_CMD_READ 1U`,
`IAP_CMD_PROGRAM 2U`, `IAP_CMD_ERASE 3U`.

`inc/eeprom/eeprom_map.h`:

```c
#define EEPROM_MAP_SETTINGS_PAGE 0U
#define EEPROM_MAP_SETTINGS_ADDRESS 0x0000U
#define EEPROM_MAP_FIRST_FREE_PAGE 1U        /* pages 1-7 stay free, for example for statistics */
#define EEPROM_MAP_FREE_ADDRESS 0x0200U
#define EEPROM_MAP_FREE_SIZE 0x0E00U         /* 0x0200-0x0FFF */
```

### 4.8 Settings `inc/settings/settings.h`, `src/settings/settings.c`

```c
typedef uint8_t settings_id_t;

#define SETTINGS_ID_TONE_HZ 0U
#define SETTINGS_ID_RX_WPM 1U
#define SETTINGS_ID_RX_EFF_WPM 2U
#define SETTINGS_ID_TX_WPM 3U
#define SETTINGS_ID_TX_EFF_WPM 4U
#define SETTINGS_ID_KEY_TYPE 5U
#define SETTINGS_ID_SHOW_CW_CODE 6U
#define SETTINGS_ID_MAX_WORD_LEN 7U
#define SETTINGS_ID_BACKLIGHT 8U
#define SETTINGS_ID_CONTRAST 9U
#define SETTINGS_ID_COUNT 10U

#define SETTINGS_KEY_STRAIGHT 0U
#define SETTINGS_KEY_IAMBIC_A 1U
#define SETTINGS_KEY_IAMBIC_B 2U

typedef uint8_t settings_load_status_t;
#define SETTINGS_LOAD_OK 0U
#define SETTINGS_LOAD_READ_FAILED 1U
#define SETTINGS_LOAD_NO_RECORD 2U    /* magic mismatch, for example an erased page */
#define SETTINGS_LOAD_BAD_VERSION 3U
#define SETTINGS_LOAD_BAD_CRC 4U

typedef uint8_t settings_save_status_t;
#define SETTINGS_SAVE_NOT_NEEDED 0U
#define SETTINGS_SAVE_OK 1U
#define SETTINGS_SAVE_FAILED 2U

#define SETTINGS_RECORD_SIZE 15U
#define SETTINGS_MAGIC_0 0x43U        /* 'C' */
#define SETTINGS_MAGIC_1 0x57U        /* 'W' */
#define SETTINGS_LAYOUT_VERSION 1U

settings_load_status_t settings_load(void);
settings_save_status_t settings_save_if_changed(void);
bool settings_has_unsaved_changes(void);
bool settings_is_edited(void);
uint16_t settings_get(settings_id_t id);
bool settings_set(settings_id_t id, MEM_XDATA uint16_t value);
uint16_t settings_min(settings_id_t id);
uint16_t settings_max(settings_id_t id);
uint16_t settings_step(settings_id_t id);
void settings_use_display_defaults(void);
```

`settings_load`:

- Call it once at start-up, before any other settings call.
- It reads the record (section 5). It checks in this order: read status, magic, version, CRC.
- On a failure, all values take their defaults and the function returns the matching status.
- On success, every field goes through the clamp rule (section 5.3). Base speeds come before effective speeds.
- It never writes the EEPROM.
- It sets the change baseline to the resulting values and clears the edited flag.

`settings_save_if_changed`:

- When the edited flag is clear, it returns `SETTINGS_SAVE_NOT_NEEDED` and does not access the EEPROM.
- When the current values are equal to the baseline, it clears the edited flag, returns
  `SETTINGS_SAVE_NOT_NEEDED` and does not access the EEPROM.
- Else it calls `eeprom_clear_low_supply()`, serializes the record, erases page 0, programs bytes 2..14 and then bytes 0..1. The magic comes last, so
  an interrupted write never has a valid magic.
- On an EEPROM error (`EEPROM_ERR_FAIL`, `EEPROM_ERR_LOW_SUPPLY` or `EEPROM_ERR_RANGE`), it stops at once,
  returns `SETTINGS_SAVE_FAILED` and keeps the baseline and the edited flag. The next Settings exit tries again,
  also without a new edit.
- On success, the baseline becomes the current values, the edited flag is cleared, and it returns
  `SETTINGS_SAVE_OK`. The record holds all current values, also contrast and backlight from a recovery start.
- It touches only page 0, and it programs only bytes 0x000-0x00E.

`settings_has_unsaved_changes`: it returns true for current values that differ from the baseline.

`settings_is_edited`: it returns the edited flag. Only `settings_set` sets the flag, after a change of a stored
value. This is the "edit step" of `ui-ux.md` UX-15. The baseline is the stored record. After a failed load, the
baseline is the defaults.

`settings_get`: the current value. For an unknown id, 0.

`settings_set`:

- For an unknown id, it returns false and changes nothing.
- Else it clamps the value (section 5.3), applies the effective-speed rule and stores the value.
- After a change of the stored value of `id`, it sets the edited flag and returns true. Else it returns false.
- When a base speed goes below its effective speed, the effective speed goes down to the base speed.

`settings_min`, `settings_max`, `settings_step`: the values from section 5.2. The max of an effective speed is the
current base speed. For an unknown id, 0.

`settings_use_display_defaults`: sets `SETTINGS_ID_CONTRAST` and `SETTINGS_ID_BACKLIGHT` to their defaults. It
does not change the baseline and does not set the edited flag. Thus the stored record keeps the old values until
the user makes an edit step and leaves Settings. Call it after `settings_load()`.

`settings_crc8` (the CRC-8 of section 5.1) is in the private header `src/settings/settings_crc.h`. Only
`settings.c` and the host tests use it. It is not part of the public interface.

`settings.c` does not call `lcd`, `backlight` or `menu`. The caller applies contrast and backlight.

### 4.9 Statistics `inc/stats/stats.h`, `src/stats/stats.c`

```c
/** Clears the training statistics. Empty hook until statistics exist. */
void stats_reset(void);
```

### 4.10 Menu `inc/menu/menu.h`, `src/menu/menu.c`, `src/menu/menu_tree.c`, `src/menu/menu_render.c`

A private header `src/menu/menu_internal.h` shares the tree types and the state between the three files. This
follows the pattern of `src/lcd/lcd_commands.h`.

```c
#include "buttons/buttons.h"

#define MENU_SCREEN_LIST 0U
#define MENU_SCREEN_EDIT 1U
#define MENU_SCREEN_PLACEHOLDER 2U
#define MENU_SCREEN_DIALOG 3U
#define MENU_SCREEN_MESSAGE 4U

#define MENU_NODE_MAIN 0U
#define MENU_NODE_SETTINGS 1U
#define MENU_NODE_CW 2U
#define MENU_NODE_DISPLAY 3U

#define MENU_DIALOG_NO 0U
#define MENU_DIALOG_YES 1U

#define MENU_MESSAGE_STATS_RESET 0U
#define MENU_MESSAGE_NOT_SAVED 1U
#define MENU_MESSAGE_DISPLAY_DEFAULTS 2U  /* ui-ux.md section 8.1 */

#define MENU_VISIBLE_ROWS 7U
#define MENU_STACK_DEPTH 3U

void menu_init(void);
void menu_handle(button_event_t event);
bool menu_needs_redraw(void);
void menu_render(void);

uint8_t menu_screen(void);         /* MENU_SCREEN_* */
uint8_t menu_node(void);           /* list node on top of the stack */
uint8_t menu_cursor(void);         /* item index in that node */
uint8_t menu_dialog_choice(void);  /* MENU_DIALOG_*; meaningful on MENU_SCREEN_DIALOG */
uint8_t menu_message(void);        /* MENU_MESSAGE_*; meaningful on MENU_SCREEN_MESSAGE */

/** Shows screen MESSAGE with @p message_id over the current list. Ignores unknown ids. */
void menu_show_message(uint8_t message_id);
```

`menu_init`: the stack holds the main node with cursor 0. The screen is LIST. A redraw is pending. Call
`settings_load()` and `gfx_init()` before it.

`menu_handle`: applies section 6.3. `BUTTON_EVENT_NONE` does nothing. A change of a visible item sets the redraw
flag. Other events do not set it.

`menu_needs_redraw`: it returns the redraw-pending flag.

`menu_render`: draws the full screen for the current state into `lcd_fb`, as `ui-ux.md` specifies. It clears the
redraw flag. It does not flush.

Tree tables are `const` data in code memory. No part of the tree is in xdata or internal RAM. The stack holds
(node, cursor) for each level. An item has these fields:

- Label.
- Kind: submenu, mode, value, choice or action.
- Target: node id, mode id, settings id or action id.
- For a value item: unit text (can be empty), the text for value 0 (only Backlight: `Off`), field width `fw`.
- For a choice item: the table of choice texts, field width `fw`.

A node has a title, its item table and its item count, and a save-on-exit flag. The texts and field widths are
from `ui-ux.md` section 5.3.

### 4.11 `main.c`

Start-up order:

1. `tick_init()`
2. `EA = 1`
3. `eeprom_init()`
4. `buttons_init()`
5. `settings_load()`
6. When `buttons_is_recovery_start()` is true: `settings_use_display_defaults()`.
7. `lcd_init()`
8. `lcd_set_contrast((uint8_t)settings_get(SETTINGS_ID_CONTRAST))`
9. `backlight_init((uint8_t)settings_get(SETTINGS_ID_BACKLIGHT))`
10. `gfx_init()`
11. `menu_init()`
12. When step 6 applied: `menu_show_message(MENU_MESSAGE_DISPLAY_DEFAULTS)`. ENTER or MENU then shows the main
    menu on `Receiving`.

The held MENU button gives no event after start-up until the user releases it and presses it again
(section 4.5).

Main loop, forever:

1. Up to `MAIN_EVENTS_PER_PASS` (4) times: `event = buttons_poll()`. When it is `BUTTON_EVENT_NONE`, stop. Else
   `menu_handle(event)`.
2. When `menu_needs_redraw()` is true, `menu_render()`.
3. `lcd_flush_step()`

Step 1 lets the repeats that came due during a slow redraw take effect before the next redraw. The limit of 4
keeps each pass short. The loop has no blocking `lcd_flush()`. The test pattern is removed. `main.c` stays the
only file that sets `EA`.

## 5. Settings data

### 5.1 Record layout (EEPROM page 0, offset 0x000)

| Offset | Size | Field | Encoding |
| --- | --- | --- | --- |
| 0 | 1 | magic 0 | `0x43` |
| 1 | 1 | magic 1 | `0x57` |
| 2 | 1 | layout version | `0x01` |
| 3 | 2 | tone, Hz | little-endian (low byte at 3) |
| 5 | 1 | RX speed, WPM | |
| 6 | 1 | effective RX speed, WPM | |
| 7 | 1 | TX speed, WPM | |
| 8 | 1 | effective TX speed, WPM | |
| 9 | 1 | key type | 0 straight, 1 iambic A, 2 iambic B |
| 10 | 1 | flags | bit 0 = show CW code. Bits 1-7 are written as 0 and ignored on load. |
| 11 | 1 | max word length | |
| 12 | 1 | backlight level | |
| 13 | 1 | contrast | |
| 14 | 1 | CRC-8 over bytes 0..13 | |

Serialize and parse byte by byte. Never copy a C struct. Bytes 0x00F-0x1FF of page 0 stay erased and unused.

CRC-8: polynomial `0x07`, initial value `0x00`, no reflection, no final XOR, MSB first, bitwise (no table). Check
value: ASCII `123456789` gives `0xF4`.

Default record (test vector): `43 57 01 58 02 20 20 14 14 02 01 05 08 20 54`.

### 5.2 Parameters

| Id | Setting | Min | Max | Step | Default | Unit shown |
| --- | --- | --- | --- | --- | --- | --- |
| `TONE_HZ` | Tone | 400 | 1000 | 10 | 600 | Hz |
| `RX_WPM` | Receiving speed | 5 | 50 | 1 | 32 | WPM |
| `RX_EFF_WPM` | Effective receiving speed | 5 | current `RX_WPM` | 1 | 32 | WPM |
| `TX_WPM` | Sending speed | 5 | 50 | 1 | 20 | WPM |
| `TX_EFF_WPM` | Effective sending speed | 5 | current `TX_WPM` | 1 | 20 | WPM |
| `KEY_TYPE` | Key type | 0 | 2 | 1 | 2 (Iambic B) | choice |
| `SHOW_CW_CODE` | Show CW code | 0 | 1 | 1 | 1 (On) | choice (0 Off, 1 On) |
| `MAX_WORD_LEN` | Max word length | 4 | 8 | 1 | 5 | none |
| `BACKLIGHT` | Backlight | 0 | 10 | 1 | 8 | none |
| `CONTRAST` | Contrast | 0 | 63 | 1 | `LCD_DEFAULT_CONTRAST` (32) | none |

The final unit and choice strings come from `ui-ux.md`.

### 5.3 Clamp rule

`settings_set` and a valid load use this rule:

1. Limit the value to [min, max]. For an effective speed, max is the current base speed.
2. Round down to the step grid `min + k * step`.

On load, clamp the base speeds first, then the effective speeds. When a base speed goes up, its effective speed
does not change.

## 6. Menu

### 6.1 Tree

The structure is binding. It is the tree of `ui-ux.md` section 5.3. The strings are from `ui-ux.md`.

Node `MENU_NODE_MAIN` (root, title `CW Trainer`):

| Index | Item | Kind | Target |
| --- | --- | --- | --- |
| 0 | `Receiving` | mode | `MENU_MODE_RECEIVING` (0) |
| 1 | `Sending` | mode | `MENU_MODE_SENDING` (1) |
| 2 | `Free Input` | mode | `MENU_MODE_FREE_INPUT` (2) |
| 3 | `Settings` | submenu | `MENU_NODE_SETTINGS` |

Node `MENU_NODE_SETTINGS` (title `Settings`, flag: save on exit):

| Index | Item | Kind | Target |
| --- | --- | --- | --- |
| 0 | `CW` | submenu | `MENU_NODE_CW` |
| 1 | `Show CW Code` | choice | `SETTINGS_ID_SHOW_CW_CODE` (`Off`, `On`) |
| 2 | `Max Word Length` | value | `SETTINGS_ID_MAX_WORD_LEN` |
| 3 | `Display` | submenu | `MENU_NODE_DISPLAY` |
| 4 | `Reset Statistics` | action | `MENU_ACTION_RESET_STATS` (0) |

Node `MENU_NODE_CW` (title `Settings > CW`):

| Index | Item | Kind | Target |
| --- | --- | --- | --- |
| 0 | `Tone` | value | `SETTINGS_ID_TONE_HZ` |
| 1 | `RX Speed` | value | `SETTINGS_ID_RX_WPM` |
| 2 | `RX Eff. Speed` | value | `SETTINGS_ID_RX_EFF_WPM` |
| 3 | `TX Speed` | value | `SETTINGS_ID_TX_WPM` |
| 4 | `TX Eff. Speed` | value | `SETTINGS_ID_TX_EFF_WPM` |
| 5 | `Key Type` | choice | `SETTINGS_ID_KEY_TYPE` (`Straight`, `Iambic A`, `Iambic B`) |

Node `MENU_NODE_DISPLAY` (title `Settings > Display`):

| Index | Item | Kind | Target |
| --- | --- | --- | --- |
| 0 | `Backlight` | value | `SETTINGS_ID_BACKLIGHT` (value 0 shows `Off`) |
| 1 | `Contrast` | value | `SETTINGS_ID_CONTRAST` |

`MENU_MODE_*` and `MENU_ACTION_*` are private constants in `menu_internal.h`. The maximum depth is 3 (main,
Settings, CW or Display), which is `MENU_STACK_DEPTH`.

### 6.2 No scrolling

No node has more than `MENU_VISIBLE_ROWS` (7) items. For a node with more than 7 items, a compile-time check in
`menu_tree.c` stops the build. Thus the menu has no scroll state and no `i/n` indicator. When a later list
needs more items, `architector` adds the scroll rule of `ui-ux.md` section 5.5 to this document first.

### 6.3 State machine

The table uses `BUTTON_EVENT_CODE(event)`. "Press" means that the repeat flag is clear.

| Screen | Event | Action |
| --- | --- | --- |
| LIST | NEXT | Cursor + 1. At the last item, a press goes to item 0 and a repeat stays. |
| LIST | PREV | Cursor - 1. At item 0, a press goes to the last item and a repeat stays. |
| LIST | ENTER on submenu | Push the child node with cursor 0. |
| LIST | ENTER on mode | Screen PLACEHOLDER for that mode. |
| LIST | ENTER on value or choice | Screen EDIT on the same row. |
| LIST | ENTER on action | Screen DIALOG with choice No (each time it opens). |
| LIST | MENU, depth > 1, node without the save-on-exit flag | Pop to the parent. The parent cursor stays on the item that opened the node. |
| LIST | MENU, depth > 1, node with the save-on-exit flag | Call `settings_save_if_changed()`, then pop to the parent. For `SETTINGS_SAVE_FAILED`, then show screen MESSAGE with `MENU_MESSAGE_NOT_SAVED`. |
| LIST | MENU at root | Nothing, no redraw. |
| EDIT | NEXT | `settings_set(id, value + step)`. At max, nothing changes. |
| EDIT | PREV | For `value < step`, set 0. Else set `value - step`. Both go through `settings_set`. At min, nothing changes. |
| EDIT | after a change | Contrast: `lcd_set_contrast(value)`. Backlight: `backlight_set_level(value)`. Other ids: nothing. |
| EDIT | ENTER or MENU | Screen LIST. The value stays. MENU in EDIT does not leave the node. |
| PLACEHOLDER | MENU | Screen LIST, same cursor. |
| PLACEHOLDER | NEXT, PREV, ENTER | Nothing. |
| DIALOG | NEXT | Choice Yes. |
| DIALOG | PREV | Choice No. |
| DIALOG | ENTER, choice No | Screen LIST, same cursor. |
| DIALOG | ENTER, choice Yes | Call `stats_reset()`. Then screen MESSAGE with `MENU_MESSAGE_STATS_RESET`. |
| DIALOG | MENU | The same as ENTER with choice No. |
| MESSAGE | ENTER or MENU | Screen LIST with the stack and cursor unchanged. After `STATS_RESET`, this is Settings on `Reset Statistics`. After `NOT_SAVED`, this is the main menu on `Settings`. |
| MESSAGE | NEXT, PREV | Nothing. |

Values and choices never go around from max to min. Repeat events act as presses, except at the ends of a list.
Only one event saves: MENU from the Settings list to the main menu. `settings_save_if_changed()` then writes
only after an edit step and only with values that differ from the stored record (section 4.8). MENU from CW or Display to Settings does not save. On a power loss inside Settings, the unsaved
changes are lost (user decision). After a failed save, the values stay active, and the next exit from Settings
tries the save again.

### 6.4 Screens

`ui-ux.md` sections 4 and 5 define the pixels of these elements:

- The 8 x 25 grid with x origin 2.
- The inverted title bar.
- The 7-row highlight band with the gap line.
- The field widths.
- The edit mode: the highlight shrinks to the value field, with the `<` and `>` limit markers.
- The placeholder, the dialog and the message screen.

The message screen has two texts. `Statistics reset` / `OK` has the title `Reset Statistics` (`ui-ux.md` section
5.8). `Not saved` / `OK` has the title `Settings` (`ui-ux.md` Q-4).

`menu_render` can redraw the full screen each time. It paints every pixel of the screen, so no pixel of the earlier
screen stays.

Gap line (decision of 2026-10-05, conflict between `ui-ux.md` sections 4.2 and 9):

- `menu_render` forces pixel row `8r` off only in the x range of a highlight band. In browse mode the band is
  x 0-127. In edit mode the band is the value field only (`ui-ux.md` section 5.4).
- Outside a band, text is in normal video and keeps the ink of its cell row 0. In edit mode this applies to the
  label. Affected labels: `RX Eff. Speed`, `Show CW Code`, `Backlight` and `Contrast` (letters such as `f`, `d`,
  `h` and `t`).
- Reason: the gap line separates a band from the row above it. A normal-video label has an off background, so
  it needs no gap. Clearing the label's cell row 0 cuts its letters (the rejected alternative of UX-2).
- Tests: the `EDIT(r, fw)` probe checks row `8r` only for x from `106 - 5 * fw` to 127. The label cells are
  checked with `TEXT` in normal video.

## 7. Interaction with other rules

- There is no new interrupt handler. ISRs do not touch LCD pins, button pins, P0.3, IAP or PWMB.
- These registers change only with `ORL` or `ANL` and a constant mask of the bits of the module: `P0M0`, `P0M1`,
  `P3M0`, `P3M1`, `P_SW2`, `PCON` and `RSTCFG`. The new modules never write port 2.
- `BUTTON_PINS_CONFIGURE()` and `BACKLIGHT_PWM_CONFIGURE()` set `EAXFR`. It stays set. No code clears it. The xdata
  RAM (0x0000-0x0BFF) does not overlap the XFR area (0xFE00-0xFEFF).
- `inc/mcu/stc8h.h` declares XFRs as macros, for example
  `#define PWMB_PS (*(volatile __xdata uint8_t *)0xFEB6U)`. Do not put `__at` variables in headers. Normal SFRs use
  `__sfr __at(...)` as today.

## 8. Hardware facts to check in `references/context/STC8H_datasheet.md`

For each fact, give the section or line in the step report. When a fact is different, stop and report it to
`architector`.

| Fact | Expected value (from the datasheet scan) |
| --- | --- |
| IAP SFRs | `IAP_DATA` 0xC2. `IAP_ADDRH` 0xC3. `IAP_ADDRL` 0xC4. `IAP_CMD` 0xC5 with CMD[1:0] 1 read, 2 program, 3 erase. `IAP_TRIG` 0xC6, write 0x5A then 0xA5. `IAP_CONTR` 0xC7: bit 7 IAPEN, bit 4 CMD_FAIL (software clears it). `IAP_TPS` 0xF5: IAPTPS[5:0] = clock in MHz, rounded. |
| IAP addresses | IAP address 0x0000 is the first EEPROM byte. With `program_eeprom_split=61440`, the EEPROM is 4 KB: IAP 0x0000-0x0FFF, 8 sectors of 512 B. A program clears bits only. An erase sets a sector to 0xFF. Erase 4-6 ms, program 30-40 us. The CPU idles during the operation. |
| Port mode | `P0M1` 0x93, `P0M0` 0x94, `P3M1` 0xB1, `P3M0` 0xB2. High-impedance input: M1 = 1, M0 = 0. Push-pull: M1 = 0, M0 = 1. |
| Pull-ups | `P0PU` XFR 0xFE10, `P3PU` XFR 0xFE13. One bit for each pin, about 4.1 kOhm. Check that the pull-up works in high-impedance input mode. |
| XFR access | `P_SW2` 0xBA. Bit 7 `EAXFR` must be 1 for XFR read and write. |
| PWMB pin select | `PWMB_PS` XFR 0xFEB6. C8PS[1:0] in bits 7:6: 00 P2.3, 01 P3.4, 10 P0.3, 11 P7.7. |
| PWMB registers | `PWMB_ENO` 0xFEB5 (ENO8P bit 6). `PWMB_CR1` 0xFEE0 (CEN, ARPE). `PWMB_EGR` 0xFEE7 (UG). `PWMB_CCMR4` 0xFEEB (OC8M[2:0], OC8PE, CC8S). `PWMB_CCER2` 0xFEED (CC8E bit 4, CC8P bit 5). `PWMB_PSCRH/L` 0xFEF0/1. `PWMB_ARRH/L` 0xFEF2/3. `PWMB_CCR8H/L` 0xFEFB/C. `PWMB_BKR` 0xFEFD (MOE bit 7). |
| Low-voltage detector | `RSTCFG` 0xFF: bit 6 `ENLVR`, bit 4 `P54RST`, bits 1:0 `LVDS` (00 2.0 V, 01 2.4 V, 10 2.7 V, 11 3.0 V). `PCON` 0x87: bit 5 `LVDF`, set by hardware on a low-voltage event, cleared by software. Check that `LVDF` is set with `ENLVR` = 0 and the LVD interrupt off, and check the effect of the ISP options on `RSTCFG`. |
| Supply | The MCU supply is the HT7833 3.3 V regulator on the battery (schematic). |
| PWMB behavior | Check these points. The high byte of a 16-bit register is written first. In PWM mode 1, the output is active while the counter is below CCR8. CCR8 above ARR gives 100 % duty. The prescaler loads at the update event. |

## 9. Build and static checks

### 9.1 CMake (`firmware/CMakeLists.txt`)

- The source glob of `src/` already takes `src/gfx/font_u8g2.c` and the other new files.
- Add `FLASH_CODE_SIZE=${FLASH_CODE_SIZE}UL` to the compile definitions.
- Do not add `firmware/drivers/u8g2_font_render` to the build. No file gets `--stack-auto`.

### 9.2 `check_asm.sh`: extended R5

- Add `P0M1` 0x93, `P0M0` 0x94, `P3M1` 0xB1, `P3M0` 0xB2, `P_SW2` 0xBA, `PCON` 0x87 and `RSTCFG` 0xFF to R5. Their bytes change only with `ORL` or `ANL`.
- Add predefined names for them.
- Add self-test cases for each register: one accept case and one `r5_` reject case.

## 10. Host test infrastructure

Add CMake patterns in `firmware/Tests/CMakeLists.txt`, so that each new test group links its own set. QA tests use
`test_qa_<group>*.c` and get the same set. Do not run the new groups for each LCD chunk size.

| Pattern | Links |
| --- | --- |
| `^test_(qa_)?gfx` | `font_u8g2.c`, `fonts.c`, `gfx.c`, `lcd_fb.c`, `firmware/src/gfx` include directory |
| `^test_(qa_)?eeprom` | `eeprom.c`, `fakes/fake_iap.c` |
| `^test_(qa_)?settings` | `settings.c`, `eeprom.c`, `fakes/fake_iap.c` |
| `^test_(qa_)?buttons` | `buttons.c`, `fakes/fake_tick.c`, `fakes/fake_delay.c`, `fakes/fake_button_pins.c` |
| `^test_(qa_)?backlight` | `backlight.c`, `fakes/fake_backlight_pwm.c` |
| `^test_(qa_)?menu` | `menu*.c`, `gfx.c`, `fonts.c`, `font_u8g2.c`, `lcd_fb.c`, `settings.c`, `eeprom.c`, `fakes/fake_iap.c`, `fakes/fake_lcd_contrast.c`, `fakes/fake_backlight.c`, `fakes/fake_stats.c` |

`fakes/fake_iap.c` models the 4 KB EEPROM behind `Tests/fakes/mcu/iap_port.h`:

- Erased state 0xFF. A program is a bitwise AND. An erase sets 512 bytes to 0xFF.
- CMD_FAIL for addresses outside the EEPROM.
- A low-voltage flag that tests set, also from operation N on. The fake records each clear of the flag.
- Inspection: access to the byte array, the erase count of each page, the program count of each address, the IAP
  register state after each call.
- Fault injection:
  - Power loss after N operations. Later program and erase calls have no effect.
  - CMD_FAIL on operation N.
  - An interrupted erase that leaves a given byte pattern in the page.

Other fakes:

- `fake_lcd_contrast.c` records the `lcd_set_contrast()` calls.
- `fake_backlight.c` records the `backlight_set_level()` calls.
- `fake_stats.c` counts the `stats_reset()` calls.
- `fake_button_pins.c` gives a raw mask for each read from a list that the test sets, so that the samples of
  the recovery window can change. Button tests set the fake tick to move by 1 ms on each read, so that the
  window loop of `buttons_init()` ends.

All host flags stay as they are (`-Wall -Wextra -Wpedantic -Werror`, ASan, UBSan). Add no warning suppression.

## 11. Memory budget and reports

Baseline (`build/putikeeg_cw_trainer.mem`): flash 1921 / 61440 B, xdata 1024 / 3072 B, stack starts at 0x1E with
226 B free. T0 measures it again.

Flash estimate for the whole change:

| Part | Flash |
| --- | --- |
| Fonts (SDCC keeps the unused 8x16 font) | about 4.9 KB |
| Decoder | under 1 KB |
| `gfx` | about 0.6 KB |
| EEPROM | about 0.5 KB |
| Settings | about 1.5 KB |
| Buttons | about 0.7 KB |
| Backlight | about 0.3 KB |
| Menu | about 3.5 KB |
| Main and stats | about 0.2 KB |
| Total with the baseline | about 14-15 KB |

The plan estimate of 17-20 KB included the third-party driver (about 5.3 KB).

| Resource | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- |
| Flash, total | at most 15 KB | 18 432 B |
| Flash, `font_u8g2.c` | about 600 B | 1024 B |
| xdata, total | about 1150 B | 1280 B |
| New static internal RAM: DSEG, overlay and bits (the stack start moves up) | a few bytes | 32 B (at least 194 B stack free) |
| Stack margin: free stack minus the deepest main-loop stack use | | at least 64 B |

Measured after T10 (2026-10-05): flash 14 438 B, xdata 1193 B, stack free 212 B, `font_u8g2.c` 763 B. All are
in the hard limits. xdata is 43 B above the target and `font_u8g2.c` is 163 B above the target. `architector`
accepts both. No change is required.

Each step report has this table from `firmware/build.sh`, before and after the step:

| Step | Flash B | xdata B | Stack start | Stack free B | Change |
| --- | --- | --- | --- | --- | --- |

From T2, the report also gives the deepest stack use of the text path: `menu_render` -> `gfx_draw_text` ->
`font_u8g2_draw_glyph` -> `lcd_fb_set_pixel`. Count it from the generated assembly. Tell how you counted it.

## 12. Tasks

`c-embedded-developer` owns T0-T10. Each step ends with these items:

- `firmware/build.sh` passes, with `check_asm.sh`.
- `firmware/test.sh` passes.
- The memory table.
- The list of assumptions and open device checks.

Comments follow the project rules: short, the reason and not the action, simple English, no status narration.

### T0 Baseline

- Inputs: the current tree.
- Outputs: the memory table of the baseline and the `test.sh` result.
- Acceptance: the numbers are recorded and both scripts pass.

### T1 Font decoder and fonts

- Inputs: sections 3.1-3.3, 4.1-4.3, 9.
- Outputs:
  - `static const` in `inc/u8g2_fonts.h`.
  - `fonts.h`, `fonts.c`.
  - `src/gfx/font_u8g2.h`, `src/gfx/font_u8g2.c`.
  - `check_asm.sh` R5 extension, with self-tests.
  - The golden fixture and its generator (section 4.2).
  - `Tests/test_gfx_font_u8g2.c`.
- Host tests:
  - Each glyph 0x20-0x7E of `spleen5x8`, drawn at (0, 0) with fg ON and bg OFF: the 5 x 8 cell is equal to the
    golden fixture, and the return value is the golden pitch.
  - The same glyphs with fg OFF and bg ON give the exact inverse in the cell, and fg INVERT over a known pattern
    gives the XOR.
  - Placement: `'A'` at (0, 0) gives these rows 0-7 in columns 0-4 (`#` is on): `.....`, `.##..`, `#..#.`,
    `#..#.`, `####.`, `#..#.`, `#..#.`, `.....`. Row 8 and column 5 do not change. `'A'` at (10, 16) gives the
    same cell moved by (10, 16).
  - Descenders: `g`, `j`, `p`, `q`, `y` use cell row 7 and never row 8.
  - Some glyphs of `spleen8x16` (`A`, `g`, `0`) against the fixture, and the cell height of the 8x16 font.
  - Missing glyph: an encoding that the font does not have (for example 0x1F) returns 0 and changes no pixel.
  - `font_u8g2_glyph_pitch` gives the golden pitch for each glyph, and 0 for a missing one.
  - Clipping: x = -3, x = 125, y = -5, y = 60 and fully outside the screen. No crash, no write outside the
    framebuffer (ASan), and the visible part is equal to the fixture.
  - `font_u8g2_select(NULL)` returns false and keeps the state. Before any select, draw returns 0.
- Acceptance:
  - The tests pass.
  - `firmware/drivers/` has no change, and the build does not compile it.
  - `font_u8g2.c` is at most 1024 B of flash (from the `.map` or `.rst`), with a target of about 600 B. Its
    static internal RAM is at most 8 B.
  - The decoder `.asm` has no 32-bit helper call (`__mullong`, `__divulong`, `__modulong` and others) and no
    indirect call.
  - The flash growth is in the report (fonts about 4.9 KB plus the decoder).

### T2 Graphics

- Inputs: 4.4.
- Outputs: `gfx.h`, `gfx.c`, `Tests/test_gfx.c`.
- Host tests:
  - `gfx_draw_text` with `GFX_TEXT_NORMAL` and `GFX_TEXT_INVERTED`: the cells are equal to the fixture or its
    inverse. Text row `r` at `y = 8r` covers pixel rows `8r` to `8r + 7` only.
  - The return value is x + 5 * n. `gfx_text_width("Agjy_|")` is 30.
  - A missing character is skipped and does not move x.
  - Clipping at the four screen edges and fully outside.
  - No font set, NULL text, unknown color, unknown font id.
  - `gfx_fill_rect` with all three colors, clipping, zero and negative sizes. The changed pages are dirty.
  - `gfx_format_u16` for 0, 9, 10, 600, 1000, 65535.
- Acceptance: the tests pass. The stack depth of the text path is in the report.

### T3 EEPROM

- Inputs: 4.7, 8.
- Outputs: `eeprom.h`, `eeprom_map.h`, `eeprom.c`, `inc/mcu/iap_port.h`, IAP SFRs in `stc8h.h`,
  `Tests/fakes/mcu/iap_port.h`, `Tests/fakes/fake_iap.c`, `Tests/fakes/fake_iap.h`, `Tests/test_eeprom.c`.
- Host tests:
  - Range errors for each function, also overflow cases such as address 0xFFFF with length 2. No IAP access occurs.
  - Read, write and erase on the fake.
  - A write of 1 over a programmed 0 bit gives `EEPROM_ERR_FAIL`.
  - CMD_FAIL on read, write and erase.
  - The IAP idle state after each return.
  - `IAP_TPS` is 24 for 24 MHz.
  - Low-voltage flag set: erase and write return `EEPROM_ERR_LOW_SUPPLY` and start no IAP command. Read still
    works. `eeprom_init` and `eeprom_clear_low_supply` clear the flag.
  - `eeprom_init` with a fake `RSTCFG` of 0x50 (`ENLVR` and `P54RST` set, LVDS 00) gives 0x13: `ENLVR` 0,
    `P54RST` kept, LVDS 3.0 V.
- Acceptance: the tests pass. The datasheet facts have citations. No code writes `EA`.

### T4 Settings

- Inputs: 4.8, 5.
- Outputs: `settings.h`, `settings.c`, `Tests/test_settings.c`.
- Host tests:
  - An empty EEPROM gives the defaults and `SETTINGS_LOAD_NO_RECORD`.
  - The CRC-8 check value 0xF4.
  - Record bytes: set one value away from its default, save, and compare the record with the default record of
    5.1. Only the changed field and the CRC are different.
  - Each min, max and step. Clamp and step-grid rounding.
  - The effective-speed rule in both directions.
  - Save and load round trip.
  - The record is only at 0x000-0x00E. Pages 1-7 are never erased or programmed.
  - No change gives no EEPROM access.
  - After a failed save, the baseline stays and the next save tries again.
  - Bad CRC, wrong version, erased page, page erased but not written.
  - Power loss after each operation of a save, for every N.
  - A valid record with out-of-range fields gives clamped values.
  - A low-voltage flag that comes on at each operation of a save gives `SETTINGS_SAVE_FAILED`, a kept baseline
    and a next load with old, new or default values, never a mix. An old flag from before the save does not
    stop it.
  - `settings_use_display_defaults` sets contrast 32 and backlight 8, keeps the other values, and does not set
    the edited flag. A save right after it gives `SETTINGS_SAVE_NOT_NEEDED`.
  - Edited flag: a change sets it. A change back to the stored value gives `NOT_NEEDED` and clears it. A
    successful save clears it. A failed save keeps it.
- Acceptance: the tests pass. For each power-loss point, the next load gives the old values in full, the new
  values in full or the defaults. It never gives a mix.

### T5 Buttons

- Inputs: 4.5 and `ui-ux.md` section 6.2.
- Outputs: `buttons.h`, `buttons.c`, `board/buttons_config.h`, `board/button_pins.h`, SFR and XFR declarations,
  fakes, `Tests/test_buttons.c`.
- Host tests:
  - A bounce shorter than the debounce time gives no event (`ui-ux.md` V-26).
  - A stable press gives one event. A release gives no event. Release bounce gives no second press.
  - Repeat delay and period for NEXT and PREV, with the flag: 1 press and 8 repeats in 1000 ms (V-08).
  - No repeat for MENU and ENTER. A held ENTER gives one event (V-24).
  - The repeat follows the button that was pressed last. After its release, the other held button does not
    repeat (V-31).
  - Repeats that came due during a long gap between polls come out on the next polls, one per call.
  - Tick wrap at 65535.
  - Two presses in one poll come out in two calls, in the order MENU, ENTER, PREV, NEXT (V-30).
  - A button held at `buttons_init` gives no event (V-25).
  - Recovery window: MENU low in every sample up to 50 ms gives a recovery start (V-33). MENU low for 30 ms and
    then high gives a normal start that ends at the first high sample (V-35). ENTER held gives a normal start
    (V-39). A start without MENU held reads only one window sample.
  - `buttons_init` calls `delay_ms(BUTTONS_SETTLE_MS)` before the first sample.
  - Exactly one `tick_now()` call per poll.
- Acceptance: the tests pass. R5 holds for the mode registers.

### T6 Backlight

- Inputs: 4.6, 8.
- Outputs: `backlight.h`, `backlight.c`, `board/backlight_config.h`, `board/backlight_pwm.h`, PWMB and `P_SW2`
  declarations, fakes, `Tests/test_backlight.c`.
- Host tests:
  - Level to compare value for 0..10.
  - Levels above 10 give level 10.
  - `backlight_init` configures one time and sets the level.
- Acceptance: the tests pass. A review of `BACKLIGHT_PWM_CONFIGURE()` against the order and the port-2 rule of
  section 4.6, with datasheet citations.

### T7 Statistics hook

- Outputs: `stats.h`, `stats.c` (empty body).
- Acceptance: the build passes. The flash change is in the report.

### T8 Menu

- Inputs: 4.10, 6, `ui-ux.md`.
- Outputs: `menu.h`, `menu.c`, `menu_tree.c`, `menu_render.c`, `menu_internal.h`, fakes, `Tests/test_menu.c`.
- Host tests:
  - Each row of the table in section 6.3.
  - The scenarios V-01 to V-23, V-27 to V-29 and V-32 of `ui-ux.md` section 9, with its probes.
  - Full navigation over all four nodes. Wrap on a press and stop on a repeat.
  - Edit of each value and choice to min and max.
  - Live contrast and backlight calls, and no call for other ids.
  - Only a Settings exit after an edit step, with values that differ from the stored record, saves. Count the
    erases on the fake.
  - MENU in EDIT, and MENU from CW or Display, do not save.
  - A failed save (fault injection on the fake) shows `Not saved`. ENTER goes to the main menu on `Settings`. The
    next Settings exit tries the save again.
  - The dialog opens with No each time. Yes calls `stats_reset` one time and shows `Statistics reset`. No and MENU
    never call it.
  - No redraw flag for events that change nothing.
  - `menu_show_message` shows each known message, ignores an unknown id, and ENTER or MENU returns to the list.
  - The recovery scenarios V-33 to V-38 of `ui-ux.md`, with a test start sequence that follows section 4.11.
- Acceptance: the tests pass. The tree and the strings are in code memory (look in the `.asm` or `.map` file).
  xdata and internal RAM are in the budget.

### T9 Main

- Inputs: 4.11.
- Outputs: the new `main.c`.
- Acceptance: start-up order and loop as specified. The settings are applied after the buttons are sampled.
  The build and `check_asm.sh` pass. The final memory table is
  in the limits of section 11.

### T10 Developer close-out

- Output: the final report with all step tables, the stack-depth analysis, the datasheet citations, the test lists,
  the assumptions and the open device checks of section 13.

## 13. QA checks by `qa-embedded`

QA runs two times: after T2 (font decoder and text output, the highest risk) and after T10.

- An independent review of each acceptance criterion. QA adds its own tests where necessary
  (`Tests/test_qa_<group>*.c`).
- Font decoder: an independent check of the golden fixture. Two possible methods:
  - Decode a few glyphs by hand from the `u8g2_fonts.h` bytes.
  - Compile the unmodified third-party driver in a scratch directory outside the repository build and compare.
    Its output is one row lower (section 3.2).
- Fault injection on the fake EEPROM:
  - Power loss at each operation of a save.
  - CMD_FAIL at each operation.
  - Interrupted erase patterns.
  - Records with a valid CRC and out-of-range fields.
  - A record of a future version.
- Button bounce patterns and irregular poll intervals.
- Review of the stack-depth count of the text path and of the decoder `.asm` (no 32-bit helpers, no indirect
  calls).
- Verdict: `approved`, `changes_requested` or `blocked`.

### Device checks that stay open (the user runs device tests on demand)

1. Settings stay after power-off. An erased EEPROM gives the defaults. Does `stcgal` erase the EEPROM on each
   download?
2. IAP with `IAP_TPS = 24`. The save time. The tick loses about one erase time (4-6 ms) for each save. A button
   pressed during the save is not lost (`ui-ux.md` D-7).
3. Backlight: PWM on P0.3, polarity, 11 visible levels, no flicker and no noise at 1 kHz (D-3).
4. P2.3 and the other LCD pins show no PWM. The LCD works during and after backlight changes.
5. Internal pull-ups: a released button reads high and a pressed button reads low. Bounce of the buttons and of
   the paddle tip is shorter than 20 ms (D-4, D-5).
6. Time from a press to the change on the glass, full redraw time and auto-repeat feel (D-6). When a full redraw
   takes more than 100 ms, report it to `architector`. The fallback is a redraw of changed rows only.
7. A live contrast change is visible and safe over the full range 0-63 (D-2).
8. Legibility of the font, the highlight, the gap line and the markers (D-1). Order of prev and next on the case
   (D-8).
9. No stack overflow with the tick interrupt active, on a full menu redraw.
10. Recovery: with a bad contrast and backlight `Off` saved, MENU held at power-on gives a readable screen with
    contrast 32 and backlight 8. A power-on without MENU held gives the saved values again.
11. Low-voltage guard: with a lab supply on the battery input, `LVDF` sets near 3.0 V at the MCU, and a save
    at that voltage shows `Not saved` and leaves the old record or no valid record. The MCU does not reset at
    3.0 V. Also find out how `LVDF` acts while the supply stays below 3.0 V: does it set again within about
    50 us after a clear? `settings_save_if_changed` clears it and reads it about 50 us later, before the erase.
    If it does not set again, report it to `architector`. The fallback is a wait after the clear, or no clear.
12. Without the low-voltage reset: lower the lab supply slowly to 0 V and back. The MCU starts normally and the
    record stays valid. Note the voltage where the screen stops.

## 14. Risks

| Risk | Handling |
| --- | --- |
| A decoder error that the golden generator shares | Hand-checked vectors (T1) and the independent QA check (section 13). |
| Stack use of the text path | The margin rule of section 11 and the count from the assembly. |
| Full-screen render time in software | Device check 6. The fallback is a row-level redraw (architect decision). |
| ENTER shares P3.2 with the paddle tip, so a plugged key can press ENTER | Out of scope. Recorded for the keyer work. |
| EEPROM operation at low battery voltage | The LVD flag check before each erase and program (section 3.6, Q-5). A drop during the one 4-6 ms erase is not caught. Then the page is blank and the defaults load. CRC and magic-last protect the record. |
| HX4004A needs a different PWM frequency or polarity | Both are configurable with `-D`. Device check 3. |

## 15. Final review decisions (2026-10-05)

- xdata 1193 B: accepted (section 11).
- QA O1, the time between the `LVDF` clear and the read: no change now. The datasheet gives no settle time, and
  its example has no wait. Device check 11 decides.
- QA O4, a `check_asm.sh` check of the `RSTCFG` masks: not required. R5 already allows only `ANL` and `ORL`, and
  a host test checks the result. A later check needs a ticket first.
- `settings_crc8` stays private (section 4.8).
- Formatting: `clang-format --dry-run --Werror` with the repository `.clang-format` gives no finding on the 55
  changed and new C files. `clang-tidy` gives only `bugprone-easily-swappable-parameters` on coordinate and
  color parameters. These follow the existing `lcd_fb_set_pixel(x, y, color)` pattern and are accepted.

## 16. Decisions for the user

All user decisions are in section 3.6. One item stays open and does not block this work:

- `ui-ux.md` Q-3: tone preview during a Tone edit, after the sidetone exists. Later feature.

Items to tell the user, no decision needed:

- `inc/u8g2_fonts.h` gets `static const` in place of `const static` (section 3.3).
- The `stats` step comes before the `menu` step (section 3.4).
- The main loop handles up to 4 button events before a redraw (section 3.5).
