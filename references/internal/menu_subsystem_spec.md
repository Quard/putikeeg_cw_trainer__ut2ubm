# Menu subsystem: implementation specification

Owner: `architector`. Status: approved for implementation. Change request D-A1 to D-A15 (firmware version,
start-up screen, `About` item, approved plan of 2026-10-08): specification published on 2026-10-08, section 17.
Readers: `c-embedded-developer`, `qa-embedded`, `ui-ux-embedded-designer`, `technical-writer`.

## 1. Sources and precedence

| Source | Role |
| --- | --- |
| This document | Interfaces, data formats, behavior, task split and acceptance criteria. It refines the plan and does not change any user decision. |
| `references/internal/ui-ux.md` (owner `ui-ux-embedded-designer`) | Screen layout, label strings, value formats, edit-mode marker, dialog and placeholder layout, auto-repeat and debounce timing values. |
| `references/context/STC8H_datasheet.md` | Register facts. Section 8 lists the facts that the developer must check. |
| `references/internal/keyer_spec.md` (owner `architector`) | Adds long-press events, P3.2 ownership, the `Paddle` setting (flags bit 1) and the Free Input mode screen. Its sections 6.12 to 6.14 are binding for these items. Its section 17 adds the prosign display setting (flags bits 2-3, layout version 2, the Settings item) and is binding for it. This document repeats these items in sections 4.5, 4.8, 4.10, 5, 6.1, 6.3, 6.4, 7 and 14. |
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
#define BUTTON_EVENT_LONG_FLAG 0x40U
#define BUTTON_EVENT_CODE(event) ((uint8_t)((event) & 0x3FU))   /* removes both flags */

/** Configures the pins and pull-ups, checks for the display recovery start and takes the start state. */
void buttons_init(void);

/** Returns true when MENU was held through the recovery window of buttons_init(). */
bool buttons_is_recovery_start(void);

/** Returns at most one event. The main loop calls it until it returns BUTTON_EVENT_NONE (section 4.11). */
button_event_t buttons_poll(void);

/** Gives P3.2 to the menu (true) or takes it away (false). Main loop only. */
void buttons_set_enter_enabled(bool is_enabled);
```

`inc/board/buttons_config.h`. Every value can be changed with `-D`. The values are from `ui-ux.md` section 6.2.

```c
#define BUTTONS_DEBOUNCE_MS 20U
#define BUTTONS_REPEAT_DELAY_MS 400U
#define BUTTONS_REPEAT_PERIOD_MS 80U
#define BUTTONS_SETTLE_MS 1U            /* pull-up settle time before the first sample */
#define BUTTONS_RECOVERY_HOLD_MS 50U     /* ui-ux.md section 8.1 */
#define BUTTONS_LONG_PRESS_MS 800U       /* ui-ux.md section 6.2, decision UX-23 */
```

A compile-time check keeps `BUTTONS_LONG_PRESS_MS` above 0.

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
There are no release events.

#### Long presses and P3.2 ownership

`keyer_spec.md` section 6.12 adds these steps to the poll algorithm above. That section is binding. Summary:

- Buttons with a long press: MENU, PREV and NEXT. ENTER has no long press, because it is the paddle tip.
- Each of the three buttons has its own long-pending bit and press time in xdata. A released-to-pressed change of
  the debounced bit sets the long-pending bit and the press time = now. A pressed-to-released change clears the
  long-pending bit.
- Long events come after the press events (step 3) and before the repeat (step 4). The order is MENU, PREV, NEXT.
  A button gives its long event when its long-pending bit is set and `(uint16_t)(now - press time) >=
  BUTTONS_LONG_PRESS_MS`. The poll then clears the bit and returns the event code with `BUTTON_EVENT_LONG_FLAG`.
  One long event comes per press. A repeat that falls due in the same poll comes on a later poll.
- The hold counts on the debounced state (D-K4). Both edges of the debounced state lag the pin by
  `BUTTONS_DEBOUNCE_MS`. Measured from the press event, the pin must still be closed at 780 ms. A release bounce
  shorter than the debounce does not end the hold.
- A held PREV or NEXT gives its repeats and also one long event. Lists use the repeats. Mode screens use the long
  event. When PREV and NEXT presses debounce in the same poll, the repeat belongs to PREV (D-K3).
- `buttons_init()` clears the long-pending bits and sets `buttons_is_enter_enabled` to true. A button held at
  start-up gives no long event, because it has no press edge.

P3.2 ownership (`keyer_spec.md` section 6.12, `ui-ux.md` UX-33):

- `buttons_set_enter_enabled(false)`: `buttons_is_enter_enabled` = false. The pending ENTER bit is cleared. While
  ENTER is disabled, each poll clears the ENTER bit of the raw sample before the debounce, so ENTER gives no press
  edge.
- `buttons_set_enter_enabled(true)`: the function reads the raw pins and `tick_now()` once. The ENTER candidate
  bit and the ENTER debounced bit take the raw ENTER bit (reseed). The ENTER change time = now. The pending ENTER
  bit is cleared. Then `buttons_is_enter_enabled` = true. The other buttons do not change.
- Result: a contact held at the end of Free Input gives no ENTER until it opens and closes again.
- Free Input calls `buttons_set_enter_enabled(false)` at its start and `buttons_set_enter_enabled(true)` at its
  end. The keyer runs only while ENTER is disabled, so one P3.2 closure is never both ENTER and a key element.

### 4.6 Backlight `inc/backlight/backlight.h`, `src/backlight/backlight.c`

```c
#define BACKLIGHT_LEVEL_MAX 10U

/** Configures PWMB channel 8 on P0.3 and sets @p level. */
void backlight_init(uint8_t level);

/** Sets the duty to level x 10 %. Values above BACKLIGHT_LEVEL_MAX become BACKLIGHT_LEVEL_MAX. */
void backlight_set_level(uint8_t level);
```

`inc/board/backlight_config.h`. Both values are `#ifndef` defaults. `firmware/build.sh` passes no `-D`, so to change
a value, edit its default in this header.

```c
#define BACKLIGHT_PWM_HZ 1000U        /* not checked on the device */
#define BACKLIGHT_IS_ACTIVE_LOW 0     /* 1 sets CC8P; polarity from the corrected schematic */
```

P0.3 drives the base of the NPN switch Q6 through R2 (120 Ω). R10 (47 kΩ) pulls P0.3 down
(`components_and_wiring.md`, section Backlight). A high P0.3 turns the backlight on. So the channel is active high:
CC8P = 0. Level 0 gives a constant low pin (off). Level 10 gives a constant high pin (on).

Pin mode (decision D-BL1): open drain with the internal pull-up (`P0PU` bit 3, about 4.1 kΩ, 3-5 kΩ). A high pin is
the pull-up, so the base current is (VCC - 0.75 V) / (pull-up + 120 Ω): 0.5-1.1 mA over the supply and pull-up
ranges. Push-pull would source (VCC - 0.75 V) / 120 Ω: about 21 mA at 3.3 V up to 29 mA at 4.2 V. Both are above
the 20 mA pin limit (datasheet section 9.2). The supply is not certain: section 8 names an HT7833 3.3 V regulator,
but P1.1 measured about 4 V HIGH (`components_and_wiring.md`). The weak pull-up of the quasi-bidirectional mode
(150-270 µA) may not saturate Q6. The datasheet does not say clearly that the `PxPU` pull-up works in open-drain
mode (section 9.2 mode table, section 9.3.5). The backlight diagnostic checks it on the device. If it fails, the
first fallback is the quasi-bidirectional mode with `P0PU` bit 3; `architector` confirms it after the diagnostic.
While the pin is low, the pull-up draws 0.7-1.4 mA.

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
11. P0.3 open drain with the pull-up, in this order: `P0M1 |= 0x08`, `P0M0 |= 0x08`, `P0PU |= 0x08`. The pin goes
    from high-impedance to open drain while the PWM output is low, so the backlight stays off. `P0PU` is a
    read-modify-write of an XFR in the main loop; no ISR writes `P0PU`.

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
#define SETTINGS_ID_PADDLE_SWAP 10U  /* 0 Normal, 1 Reversed (keyer_spec.md section 6.13) */
#define SETTINGS_ID_PROSIGN_STYLE 11U  /* SETTINGS_PROSIGN_* (keyer_spec.md section 17) */
#define SETTINGS_ID_COUNT 12U

#define SETTINGS_KEY_STRAIGHT 0U
#define SETTINGS_KEY_IAMBIC_A 1U
#define SETTINGS_KEY_IAMBIC_B 2U

#define SETTINGS_PROSIGN_PUNCTUATION 0U
#define SETTINGS_PROSIGN_NAME 1U
#define SETTINGS_PROSIGN_BOTH 2U

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
#define SETTINGS_LAYOUT_VERSION 2U

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
- Layout versions 1 and 2 pass the version check. A version 1 record loads as section 5.1 says: its values stay,
  and `Prosigns` takes its default.
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
#define MENU_SCREEN_FREE_INPUT 5U          /* keyer_spec.md section 6.14 */

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
| 2 | 1 | layout version | `0x02`. A load also accepts `0x01` (see below). |
| 3 | 2 | tone, Hz | little-endian (low byte at 3) |
| 5 | 1 | RX speed, WPM | |
| 6 | 1 | effective RX speed, WPM | |
| 7 | 1 | TX speed, WPM | |
| 8 | 1 | effective TX speed, WPM | |
| 9 | 1 | key type | 0 straight, 1 iambic A, 2 iambic B |
| 10 | 1 | flags | Bit 0 = show CW code. Bit 1 = paddle swap (1 = `Reversed`). Bits 2-3 = prosign style (`SETTINGS_PROSIGN_*`, 3 loads as 2). Bits 4-7 are written as 0 and ignored on load. |
| 11 | 1 | max word length | |
| 12 | 1 | backlight level | |
| 13 | 1 | contrast | |
| 14 | 1 | CRC-8 over bytes 0..13 | |

Serialize and parse byte by byte. Never copy a C struct. Bytes 0x00F-0x1FF of page 0 stay erased and unused.

CRC-8: polynomial `0x07`, initial value `0x00`, no reflection, no final XOR, MSB first, bitwise (no table). Check
value: ASCII `123456789` gives `0xF4`.

Default record (test vector): `43 57 02 58 02 20 20 14 14 02 09 05 08 20 03`.

`keyer_spec.md` section 17.4.2 gives the other test vectors of version 2 and version 1.

Flag fields: `settings_get()` returns `(flags & mask) >> shift` of the id: bit 0 for `SETTINGS_ID_SHOW_CW_CODE`,
bit 1 for `SETTINGS_ID_PADDLE_SWAP`, bits 2-3 for `SETTINGS_ID_PROSIGN_STYLE`. `settings_set()` for these ids changes
only the bits of the id. The `Paddle` setting changed neither the layout nor the version.

Layout version 2 (`keyer_spec.md` section 17, decision D-P6). The layout of version 1 is the same, but bits 2-3 of
its flags byte have no meaning. Load of a version 1 record with a valid CRC:

1. Keep flags bits 0-1, clear bits 2-7. Set `Prosigns` to its default, `Both`.
2. Clamp every field as for version 2.
3. Return `SETTINGS_LOAD_OK`. The record in RAM gets version 2. The baseline takes these values, so no save happens
   until an edit step (UX-15). The load never writes the EEPROM.

Every save writes version 2. Earlier firmware reads a version 2 record as `SETTINGS_LOAD_BAD_VERSION` and uses its
defaults. This is accepted.

Version 1 test vectors, for load tests only: `43 57 01 58 02 20 20 14 14 02 01 05 08 20 54` (defaults, loads as
`Both`) and `43 57 01 58 02 20 20 14 14 02 03 05 08 20 78` (`Paddle` = `Reversed`, loads as `Reversed` and `Both`).

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
| `PADDLE_SWAP` | Paddle | 0 | 1 | 1 | 0 (Normal) | choice (0 Normal, 1 Reversed) |
| `PROSIGN_STYLE` | Prosigns | 0 | 2 | 1 | 2 (Both) | choice (0 Punctuation, 1 Prosign, 2 Both) |

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
| 2 | `Free Input` | mode | `MENU_MODE_FREE_INPUT` (2), screen `MENU_SCREEN_FREE_INPUT` |
| 3 | `Settings` | submenu | `MENU_NODE_SETTINGS` |

Node `MENU_NODE_SETTINGS` (title `Settings`, flag: save on exit):

| Index | Item | Kind | Target |
| --- | --- | --- | --- |
| 0 | `CW` | submenu | `MENU_NODE_CW` |
| 1 | `Show CW Code` | choice | `SETTINGS_ID_SHOW_CW_CODE` (`Off`, `On`) |
| 2 | `Prosigns` | choice | `SETTINGS_ID_PROSIGN_STYLE` (`Punctuation`, `Prosign`, `Both`), field width 11 (`keyer_spec.md` section 17.4.3) |
| 3 | `Max Word Length` | value | `SETTINGS_ID_MAX_WORD_LEN` |
| 4 | `Display` | submenu | `MENU_NODE_DISPLAY` |
| 5 | `Reset Statistics` | action | `MENU_ACTION_RESET_STATS` (0) |

Node `MENU_NODE_CW` (title `Settings > CW`):

| Index | Item | Kind | Target |
| --- | --- | --- | --- |
| 0 | `Tone` | value | `SETTINGS_ID_TONE_HZ` |
| 1 | `RX Speed` | value | `SETTINGS_ID_RX_WPM` |
| 2 | `RX Eff. Speed` | value | `SETTINGS_ID_RX_EFF_WPM` |
| 3 | `TX Speed` | value | `SETTINGS_ID_TX_WPM` |
| 4 | `TX Eff. Speed` | value | `SETTINGS_ID_TX_EFF_WPM` |
| 5 | `Key Type` | choice | `SETTINGS_ID_KEY_TYPE` (`Straight`, `Iambic A`, `Iambic B`) |
| 6 | `Paddle` | choice | `SETTINGS_ID_PADDLE_SWAP` (`Normal`, `Reversed`), field width 8 |

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

The table uses `BUTTON_EVENT_CODE(event)`. "Press" means that the repeat flag and the long flag are clear. "Long"
means that the long flag is set (section 4.5).

Lists, edit mode, the dialog and the message screen ignore long events: no state change and no redraw request. The
press of the same hold already acted (`ui-ux.md` UX-24). The mode screens (PLACEHOLDER and FREE_INPUT) act only on
long events (`keyer_spec.md` section 6.14, `ui-ux.md` UX-31).

| Screen | Event | Action |
| --- | --- | --- |
| LIST | NEXT | Cursor + 1. At the last item, a press goes to item 0 and a repeat stays. |
| LIST | PREV | Cursor - 1. At item 0, a press goes to the last item and a repeat stays. |
| LIST | ENTER on submenu | Push the child node with cursor 0. |
| LIST | ENTER on mode `Free Input` | Screen FREE_INPUT with a redraw request, then `free_input_enter()`. |
| LIST | ENTER on mode `Receiving` or `Sending` | Screen PLACEHOLDER for that mode. |
| LIST | ENTER on value or choice | Screen EDIT on the same row. |
| LIST | ENTER on action | Screen DIALOG with choice No (each time it opens). |
| LIST | MENU, depth > 1, node without the save-on-exit flag | Pop to the parent. The parent cursor stays on the item that opened the node. |
| LIST | MENU, depth > 1, node with the save-on-exit flag | Call `settings_save_if_changed()`, then pop to the parent. For `SETTINGS_SAVE_FAILED`, then show screen MESSAGE with `MENU_MESSAGE_NOT_SAVED`. |
| LIST | MENU at root | Nothing, no redraw. |
| EDIT | NEXT | `settings_set(id, value + step)`. At max, nothing changes. |
| EDIT | PREV | For `value < step`, set 0. Else set `value - step`. Both go through `settings_set`. At min, nothing changes. |
| EDIT | after a change | Contrast: `lcd_set_contrast(value)`. Backlight: `backlight_set_level(value)`. Other ids: nothing. |
| EDIT | ENTER or MENU | Screen LIST. The value stays. MENU in EDIT does not leave the node. |
| PLACEHOLDER | Long MENU | Screen LIST, same cursor. |
| PLACEHOLDER | Long NEXT or long PREV, and every event without the long flag | Nothing. |
| FREE_INPUT | Long MENU | `free_input_leave()`, then screen LIST with a redraw request. The cursor stays on `Free Input`. |
| FREE_INPUT | Long PREV | `free_input_clear()`. |
| FREE_INPUT | Long NEXT | `free_input_delimiter()`. |
| FREE_INPUT | Every event without the long flag | Nothing. |
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
- The field widths. The value text buffer of `menu_render.c` (`VALUE_TEXT_SIZE`) holds the longest value or choice
  text of the tree plus the NUL: 12 bytes for `Punctuation` (`keyer_spec.md` decision D-P7).
- The edit mode: the highlight shrinks to the value field, with the `<` and `>` limit markers.
- The placeholder, the dialog and the message screen.

The message screen has two texts. `Statistics reset` / `OK` has the title `Reset Statistics` (`ui-ux.md` section
5.8). `Not saved` / `OK` has the title `Settings` (`ui-ux.md` Q-4).

Free Input screen (`ui-ux.md` section 5.9, `keyer_spec.md` sections 6.11 and 6.14):

- `menu_render()` draws only the title bar `Free Input`, once at the mode start, after its usual full clear. It
  uses the code that draws the placeholder title. No other path requests a redraw while Free Input is open.
- `free_input` draws the decoded text into rows 1-7 of `lcd_fb`. `menu_poll()` calls `free_input_poll()` only on
  screen FREE_INPUT with no redraw pending, so a character never comes before the title bar.
- `menu_show_message()` on screen FREE_INPUT calls `free_input_leave()` first.

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

- There is no new interrupt handler. ISRs do not touch LCD pins, button pins, P0.3, IAP or PWMB. Exception from
  `keyer_spec.md` section 8: the keyer part of the Timer 0 ISR reads the P3.2 and P5.5 pin bits. It never writes
  them.
- These registers change only with `ORL` or `ANL` and a constant mask of the bits of the module: `P0M0`, `P0M1`,
  `P3M0`, `P3M1`, `P_SW2`, `PCON` and `RSTCFG`. The new modules never write port 2.
- `P0PU` and `P3PU` are XFRs, so a change is a read-modify-write (`movx` read, `ORL`, `movx` write) in the main loop
  only. It sets only the bits of the module: `BUTTON_PINS_CONFIGURE()` bits 0-2 of `P0PU` and bit 2 of `P3PU`,
  `BACKLIGHT_PWM_CONFIGURE()` bit 3 of `P0PU`. No ISR reads or writes `P0PU` or `P3PU`.
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
| Port mode | `P0M1` 0x93, `P0M0` 0x94, `P3M1` 0xB1, `P3M0` 0xB2. High-impedance input: M1 = 1, M0 = 0. Push-pull: M1 = 0, M0 = 1. Open drain: M1 = 1, M0 = 1 (P0.3, D-BL1). |
| Pull-ups | `P0PU` XFR 0xFE10, `P3PU` XFR 0xFE13. One bit for each pin, about 4.1 kOhm. Check that the pull-up works in high-impedance input mode (buttons) and in open-drain mode (P0.3, D-BL1; unverified on the device). |
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
| ENTER shares P3.2 with the paddle tip, so a plugged key can press ENTER | In the menu this stays: a held key gives one ENTER (section 4.5). In Free Input the keyer owns P3.2 (`keyer_spec.md` section 6.12, `ui-ux.md` UX-33). |
| EEPROM operation at low battery voltage | The LVD flag check before each erase and program (section 3.6, Q-5). A drop during the one 4-6 ms erase is not caught. Then the page is blank and the defaults load. CRC and magic-last protect the record. |
| The reverse-engineered backlight circuit is wrong, or the LED needs a different PWM frequency | Change the default of `BACKLIGHT_IS_ACTIVE_LOW` or `BACKLIGHT_PWM_HZ` in `inc/board/backlight_config.h`, then run `firmware/build.sh`. Device check 3. |

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

## 17. Change request: firmware version, start-up screen and `About` (2026-10-08)

Owner: `architector`. Status: A0-A3, QA-A, fixes R1-R3 and F2 are delivered. The final review on
2026-10-08 gave `approved` (section 17.15). The 5 s start-up screen change (user decision, 2026-10-08) is
approved (section 17.15). W-A and F1 are open.

### 17.1 Sources and precedence

| Source | Role |
| --- | --- |
| Approved plan `~/.claude/plans/quirky-wandering-quill.md` | User decisions: version `0.1.0` in a plain `firmware/VERSION` file for CI, a 2 s start-up screen that any button skips (a later user decision of 2026-10-08 changes it to 5 s, section 17.15, D-A6), the `About` item after `Settings` with the link on three text rows, a version rule in `AGENTS.md`. |
| This section | Interfaces, screen ids, state machine, build path, memory budget, tasks and acceptance criteria. It refines the plan. It does not change a user decision. Where it differs from sections 4.10, 6.1 and 6.3, this section wins. |
| `references/internal/ui-ux.md` sections 5.17 "Startup screen" and 5.18 "About screen" (owner `ui-ux-embedded-designer`, UX-58 to UX-63) | Pixel positions, fonts of each text, row strings, the `About` row marker in the main menu (none, UX-61), the reaction to buttons (UX-59), the recovery-start order (UX-60, section 8.1). |
| `references/internal/training_spec.md` | D-T8 (screen id 2 free) and the mode screens that this section renumbers (D-A3). |

The precedence rules of section 1 apply. If `ui-ux.md` describes a behavior that differs from section 17.5 (for
example a different recovery order, or a reaction to a button), stop and report it to `architector`.

### 17.2 Decisions

| ID | Decision | Reason |
| --- | --- | --- |
| D-A1 | `firmware/VERSION` is the only place that holds the version. It has one line, `MAJOR.MINOR.PATCH`, for example `0.1.0`, with a final LF. No C file, test, script or CMake file contains the version digits. | User decision (plan). CI reads it with `cat firmware/VERSION` and needs no C parser. |
| D-A2 | A shared CMake script `firmware/cmake/firmware_version.cmake` reads and checks the file and generates `version.h` in the build directory. The firmware project and the host-test project both include it (section 17.3). The template uses `@FIRMWARE_VERSION@`, not `@PROJECT_VERSION@`. | The host-test project is another CMake project. Its `PROJECT_VERSION` is not the firmware version. |
| D-A3 | Screen ids: `MENU_SCREEN_SPLASH` 2, `MENU_SCREEN_ABOUT` 5. The mode screens move up by one: `MENU_SCREEN_FREE_INPUT` 6, `MENU_SCREEN_RECEIVING` 7, `MENU_SCREEN_SENDING` 8. This supersedes the numbers in `keyer_spec.md` section 6.14 and `training_spec.md` section 5. | Both new screens must be below `MENU_SCREEN_FREE_INPUT`, because `menu_handle()` takes every id from it on as a mode screen. Only id 2 is free (D-T8). Screen ids live only in RAM, so the renumber has no EEPROM or compatibility effect. |
| D-A4 | Item kind `MENU_KIND_ABOUT` (5) with target 0. No generic "info screen" kind. | One info screen. A generic kind needs an id table and costs flash for no user benefit. |
| D-A5 | `menu_init()` shows the splash, not the list. The list, the stack and the cursor (main menu, `Receiving`) are set as before, under the splash. | Plan. The splash end needs no stack work. |
| D-A6 | The splash ends at `MENU_SPLASH_MS` (5000 ms) after `menu_init()`, measured with `(uint16_t)(tick_now() - menu_splash_start)` in `menu_poll()`, or at the first button event. The time counts from `menu_init()`, not from the first flush. | User decision of 2026-10-08 (the plan said 2 s). Same pattern as the Tone preview. The `uint16_t` difference wraps at 65 535 ms, far above 5000 ms. A flush takes a part of the 5 s; the device check DC-A2 measures it. |
| D-A7 | Every event on the splash (each code, with or without the repeat or long flag) ends the splash and has no other effect. The event does not go to the list. | Plan: "any button skips it". |
| D-A8 | Only the skip event has no other effect. The events after it act on the next screen as usual: the repeats of a held `next` move the main menu cursor from 400 ms, and the long event does nothing in a list. No skip-hold state. | `ui-ux.md` section 5.17.2, UX-59 (the designer owns the reaction to a button). It needs no state and no code. |
| D-A9 | Recovery start order: the splash first, then `Display Defaults`. `menu_show_message()` on screen SPLASH stores the message id in `menu_current_message` and changes nothing else (no screen change, no redraw request). The splash end shows screen MESSAGE when a message id is stored, else screen LIST. A second call during the splash replaces the stored id. An unknown id is ignored, as before. `main.c` does not change. | Plan, recommended order. The stored id reuses the existing `menu_current_message` byte with the sentinel `MENU_MESSAGE_NONE`, so the deferral costs no xdata. `ui-ux.md` section 8.1 confirms the order; a different order there is a conflict for `architector`. |
| D-A10 | `menu_render()` always ends with `FONTS_ID_5X8` selected. The splash code selects `FONTS_ID_8X16` only around the `CW Trainer` text, as `training_screen_draw_item_text()` does. | Every other screen assumes the 5 x 8 font. |
| D-A11 | Text constants live in `menu_tree.c` as `MEM_CODE` arrays. The splash and `About` share them. The version strings are compile-time literal joins: `"v" FIRMWARE_VERSION` and `"Version " FIRMWARE_VERSION`. Only `menu_tree.c` includes `version.h`. | Plan. One copy of each string in code memory; no run-time formatting. |
| D-A12 | `About` controls: the existing final branch of `menu_handle_other_screen()`. `enter` or `menu/back` shows screen LIST with the stack unchanged, so the cursor stays on `About`. `prev`, `next` and long events do nothing and request no redraw. | Plan. No new handler code. |
| D-A13 | `firmware/build.sh` prints `Firmware version: <x>` (the first line of `firmware/VERSION`) after the memory report. It does not copy the image to a versioned file name. | CI can name the artifact with `cat firmware/VERSION`. A versioned copy leaves stale images in `build/`. |
| D-A14 | No host test contains the version digits. The version test reads `firmware/VERSION` at run time and compares it with `FIRMWARE_VERSION` (section 17.8). | D-A1 and the `AGENTS.md` rule ("never write the version anywhere else"). A test with `"0.1.0"` breaks at each version change. This replaces the plan item "`FIRMWARE_VERSION` is `"0.1.0"`" and still proves that the header follows the file. |
| D-A15 | The version has at most 17 characters (`ui-ux.md` UX-63, Q-16). Two checks stop the build: (1) `firmware_version.cmake` stops the configure step when `string(LENGTH)` of the version is above `FIRMWARE_VERSION_MAX_LENGTH` (17), next to the format check; (2) `menu_tree.c` has `_Static_assert(sizeof(FIRMWARE_VERSION) - 1U <= 17U, ...)`. | The CMake check is the primary one: it fails first, with a clear message, for the firmware, the host tests and CI. The C check guards the screen code itself, also when a build gets `version.h` some other way. Both cost no flash. |

### 17.3 Version build path

New file `firmware/VERSION`: the line `0.1.0` and LF.

New file `firmware/cmake/version.h.in`:

```c
#ifndef FIRMWARE_VERSION_H
#define FIRMWARE_VERSION_H

/* Generated by CMake from firmware/VERSION. Do not edit. */
#define FIRMWARE_VERSION "@FIRMWARE_VERSION@"

#endif /* FIRMWARE_VERSION_H */
```

New file `firmware/cmake/firmware_version.cmake`. It uses no command that needs `project()`, so the firmware can
include it before `project()`. It does these steps:

1. `FIRMWARE_VERSION_FILE` = `${CMAKE_CURRENT_LIST_DIR}/../VERSION`, as an absolute normalized path.
2. `file(STRINGS "${FIRMWARE_VERSION_FILE}" FIRMWARE_VERSION LIMIT_COUNT 1)`. Also read the raw bytes of the file
   (`file(READ)`). When they are not exactly the version plus at most one LF (a UTF-8 BOM, a CR, a second line or
   spaces): `message(FATAL_ERROR ...)` with the file path and the reason (review decision R1, section 17.15).
   `file(STRINGS)` drops a BOM and a CR, but `cat firmware/VERSION` in CI and `build.sh` prints them.
3. When `FIRMWARE_VERSION` does not match `^[0-9]+\.[0-9]+\.[0-9]+$`: `message(FATAL_ERROR ...)` with the file
   path, the value read and the expected format `MAJOR.MINOR.PATCH`. When its length is above
   `FIRMWARE_VERSION_MAX_LENGTH` (17, set in this script): `message(FATAL_ERROR ...)` with the file path, the value,
   its length and the limit (D-A15).
4. `set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${FIRMWARE_VERSION_FILE}")`, so an edit of the
   file reconfigures the build at the next `cmake --build`.
5. `FIRMWARE_VERSION_INCLUDE_DIR` = `${CMAKE_CURRENT_BINARY_DIR}/generated`.
6. `configure_file("${CMAKE_CURRENT_LIST_DIR}/version.h.in" "${FIRMWARE_VERSION_INCLUDE_DIR}/version.h" @ONLY)`.
   `configure_file` does not touch an unchanged output, so an unchanged version causes no rebuild.

Results for the includer: `FIRMWARE_VERSION`, `FIRMWARE_VERSION_FILE`, `FIRMWARE_VERSION_INCLUDE_DIR`.

`firmware/CMakeLists.txt`:

- `include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/firmware_version.cmake")` before `project()`.
- `project(putikeeg_cw_trainer VERSION ${FIRMWARE_VERSION} LANGUAGES C)`.
- `target_include_directories(putikeeg_cw_trainer PRIVATE "${FIRMWARE_VERSION_INCLUDE_DIR}")`.

`firmware/Tests/CMakeLists.txt`:

- `include("${FIRMWARE_DIR}/cmake/firmware_version.cmake")` after `FIRMWARE_DIR` is set.
- `menu_group` gets `"${FIRMWARE_VERSION_INCLUDE_DIR}"` as a PUBLIC include directory and the PUBLIC compile
  definition `FIRMWARE_VERSION_FILE="${FIRMWARE_VERSION_FILE}"` (a path, not the version).
- No other group changes. Host flags stay as they are.

No generated file goes into the source tree. `version.h` is not added to `.gitignore`, because it is only in build
directories, which are ignored already.

`firmware/build.sh`: after `cat ... .mem`, one line `echo "Firmware version: $(head -n 1 "${firmware_dir}/VERSION")"`.
No other change (D-A13). `firmware/flash.sh` and `firmware/test.sh` do not change.

### 17.4 Interfaces

`firmware/inc/menu/menu.h` (public):

```c
#define MENU_SCREEN_LIST 0U
#define MENU_SCREEN_EDIT 1U
#define MENU_SCREEN_SPLASH 2U
#define MENU_SCREEN_DIALOG 3U
#define MENU_SCREEN_MESSAGE 4U
#define MENU_SCREEN_ABOUT 5U
#define MENU_SCREEN_FREE_INPUT 6U
#define MENU_SCREEN_RECEIVING 7U
#define MENU_SCREEN_SENDING 8U

#ifndef MENU_SPLASH_MS
#define MENU_SPLASH_MS 5000U /* ui-ux.md section 5.17.2, UX-59 */
#endif
```

- The comment "2 is unused (training_spec.md D-T8)" goes away.
- No function signature changes. The doc comments of `menu_init()`, `menu_handle()`, `menu_poll()` and
  `menu_show_message()` change to tell the splash behavior (D-A5 to D-A9).
- `menu.c` extends its `#error` check: `MENU_SCREEN_SPLASH`, `MENU_SCREEN_ABOUT`, `MENU_SCREEN_MESSAGE` and
  `MENU_SCREEN_DIALOG` are below `MENU_SCREEN_FREE_INPUT`, and `MENU_SCREEN_RECEIVING` and `MENU_SCREEN_SENDING` are
  above it.

`firmware/src/menu/menu_internal.h` (private):

```c
#define MENU_KIND_ABOUT 5U
#define MENU_MESSAGE_NONE 0xFFU /* no message stored, D-A9 */
#define MENU_ABOUT_LINK_ROWS 3U

extern const char MEM_CODE menu_text_brand[];         /* "Putikeeg" */
extern const char MEM_CODE menu_text_product[];       /* "CW Trainer" */
extern const char MEM_CODE menu_text_mod[];           /* "mod by UT2UBM" */
extern const char MEM_CODE menu_text_version_short[]; /* "v" FIRMWARE_VERSION */
extern const char MEM_CODE menu_text_version_long[];  /* "Version " FIRMWARE_VERSION */
extern const menu_text_t MEM_CODE menu_about_link_rows[MENU_ABOUT_LINK_ROWS];
```

- The comments above give the plan strings. The binding strings and their rows are in `ui-ux.md` sections
  5.17 and 5.18. The plan link rows are `https://github.com/Quard/`, `putikeeg_cw_trainer__` and `ut2ubm`.
- The main node title may use `menu_text_product` in place of its own `"CW Trainer"` literal. It is optional; do it
  when it saves flash.
- `menu_tree.c` has `_Static_assert(sizeof(FIRMWARE_VERSION) - 1U <= 17U, ...)` with a reason that names UX-63
  (D-A15). Then `Version ` + version ends in column 24 at most, and `v` + version is at most 18 cells.
- The splash version text is right-aligned with its last character in column 23 (`ui-ux.md` section 5.17.4). Its
  length is a compile-time constant (`sizeof(menu_text_version_short) - 1U` where the array is visible, or a
  macro in `menu_internal.h`), so no run-time measure is needed.

`firmware/src/menu/menu_tree.c`: the row `{"About", NULL, NULL, MENU_KIND_ABOUT, 0U, 0U}` after `Settings` in
`menu_main_items`. The main menu has 5 items (index 4 is `About`), below `MENU_VISIBLE_ROWS`.

New private state in `menu.c`, all `MEM_XDATA`:

| Name | Type | Use |
| --- | --- | --- |
| `menu_splash_start` | `uint16_t` | `tick_now()` at `menu_init()` |

`main.c` does not change. The start-up order of section 4.11 stays.

### 17.5 Behavior

Additions to the state table of section 6.3. The SPLASH step runs in `menu_handle()` after the `BUTTON_EVENT_NONE`
check and before the long-event filter, so a long event also ends the splash.

| Screen | Event | Action |
| --- | --- | --- |
| SPLASH | any event | End the splash. The event has no other effect (D-A7, D-A8). |
| SPLASH | `menu_poll()` with `(uint16_t)(tick_now() - menu_splash_start) >= MENU_SPLASH_MS` | End the splash. |
| LIST | ENTER on `About` | Screen ABOUT with a redraw request. This branch comes before the branch that opens EDIT. |
| ABOUT | ENTER or MENU | Screen LIST, stack and cursor unchanged (cursor on `About`). |
| ABOUT | PREV, NEXT, every long event | Nothing, no redraw. |

End the splash: show screen MESSAGE when `menu_current_message != MENU_MESSAGE_NONE`, else screen LIST. Both
request a redraw.

`menu_init()`: stack as before, `menu_current_message = MENU_MESSAGE_NONE`, `menu_splash_start = tick_now()`,
then screen SPLASH with a redraw request. It reads `tick_now()` exactly once.

`menu_poll()`: the splash check runs on each call while the screen is SPLASH, before the early return of the Tone
preview part. On other screens, the splash check reads no tick.

`menu_show_message(id)`: unknown id, ignore (as before). Screen SPLASH: `menu_current_message = id`, then return
(D-A9). Other screens: as before.

`menu_render()`:

- SPLASH: draw the start-up screen as `ui-ux.md` section 5.17 says. Use `gfx_draw_text()` with the pixel
  positions given there. Select `FONTS_ID_8X16` for `CW Trainer` only, then `FONTS_ID_5X8` again (D-A10).
- ABOUT: title bar `About` with `draw_title()` (the final branch of `menu_render()` already draws the label of the
  item under the cursor, which is `About`). Then the text rows of `ui-ux.md` section 5.18 with `draw_text()` and
  `render_row`. A row may be drawn with more than one `draw_text()` call, for example `menu_text_brand` and
  `menu_text_product` on one row.
- The `About` row in the main menu: its label only, no marker and no value (`ui-ux.md` UX-61).
- Both screens paint every pixel (the full clear at the start of `menu_render()` stays).

The splash does not call the Tone preview, the modes, the sidetone or the settings. Interrupt code does not change.
`check_asm.sh` rules do not change.

### 17.6 Memory budget

Baseline (`firmware/build/putikeeg_cw_trainer.mem`, before this change): flash 27 576 / 61 440 B, xdata 1814 /
3072 B, stack starts at 0x41 with 191 B free. A0 measures it again.

| Resource | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- |
| Flash change | at most +600 B | +1024 B |
| xdata change | +2 B (`menu_splash_start`) | +6 B |
| New internal RAM (DSEG, overlay, bits) | 0 B, stack start stays 0x41 | +4 B |
| Stack margin (free stack minus the deepest main-loop use minus the largest ISR frame, `llm.md`) | | at least 64 B |
| Strings and the tree | code memory only | no string in xdata or internal RAM |

The deepest new path is `menu_render` -> splash draw -> `gfx_draw_text` -> `font_u8g2_draw_glyph` ->
`lcd_fb_set_pixel`. It must not be deeper than the list text path of section 11. Count it from the generated
`.asm` and tell how you counted it.

Each task report has the table of section 11 before and after the task, and the line `Firmware version:` of
`build.sh`.

### 17.7 Changed and new files

| File | Change |
| --- | --- |
| `firmware/VERSION` | new |
| `firmware/cmake/firmware_version.cmake`, `firmware/cmake/version.h.in` | new |
| `firmware/CMakeLists.txt`, `firmware/Tests/CMakeLists.txt`, `firmware/build.sh` | section 17.3 |
| `firmware/inc/menu/menu.h` | screen ids, `MENU_SPLASH_MS`, doc comments |
| `firmware/src/menu/menu_internal.h`, `menu.c`, `menu_tree.c`, `menu_render.c` | sections 17.4 and 17.5 |
| `firmware/Tests/support/menu_splash.h` | new test helper (section 17.8) |
| `firmware/Tests/test_menu*.c`, `test_qa_menu*.c`, `support/training_harness.h` | setup uses the helper |
| `firmware/Tests/test_menu_splash.c`, `firmware/Tests/test_menu_about.c` | new |

No change to `main.c`, the ISR files, `check_asm.sh`, `firmware/drivers/`, `ui-ux.md` or `AGENTS.md`.

### 17.8 Tests

Setup helper, `firmware/Tests/support/menu_splash.h`:

```c
/** Ends the start-up screen with a menu/back press and draws the result, as the main loop does. */
static inline void menu_test_skip_splash(void);
```

- It checks that the screen is SPLASH, calls `menu_handle(BUTTON_EVENT_MENU)`, checks that the screen is not
  SPLASH, then calls `menu_render()` when a redraw is pending.
- It reads no tick, so the time base of each test stays as it is.
- Each existing setup calls it after `menu_init()` and after the optional `menu_show_message()`, before the
  setup's own render and counter resets. The sites are: `test_menu.c` (two), `test_menu_free_input.c`,
  `test_menu_preview.c`, `test_menu_prosign.c`, `test_qa_menu.c`, `test_qa_menu_preview.c`,
  `test_qa_menu_prosign.c` and `support/training_harness.h`. After it, each setup has the same state as before
  this change: the main menu on `Receiving`, or the `Display Defaults` message on a recovery start.
- `menu_init()` now reads the tick once. Tests that count `fake_tick_read_count()` reset or adjust their count after
  setup. Report each such change.
- Expected values that change because the tree has a fifth item (for example, a `prev` press on `Receiving` now
  goes to `About`, index 4) are updated, and each is listed in the report. No test is deleted, skipped or weakened.

New host tests, `firmware/Tests/test_menu_splash.c`:

1. After `menu_init()`: screen SPLASH, a redraw is pending, `menu_node()` is MAIN, `menu_cursor()` is 0.
2. `menu_poll()` at `MENU_SPLASH_MS - 1` ms keeps SPLASH and requests no redraw; at `MENU_SPLASH_MS` it shows LIST
   with a redraw request. Also with a start tick near 65535 (wrap).
3. Each of the 4 codes, each as a press, a repeat and a long event: the splash ends, the screen is LIST on
   `Receiving`, no setting, sidetone, contrast, backlight or stats call happens, and no mode starts.
4. Held button after a skip (UX-59): a `next` press skips; the following `next` repeats move the cursor from
   `Receiving`; the long `next` changes nothing. A `prev` press skips; its repeats stay on `Receiving` (no wrap on a
   repeat).
5. Recovery order: the section 4.11 start sequence with MENU held: SPLASH shows, `menu_message()` is
   `MENU_MESSAGE_DISPLAY_DEFAULTS`; at the timeout, and in a second run at a button event, screen MESSAGE
   `Display Defaults` shows; `enter` then shows the main menu on `Receiving`. Run it once with real button pins
   (held `menu/back`, release, press).
6. `menu_show_message()` with an unknown id on the splash changes nothing; two calls keep the last id.
7. Render: the splash frame matches `ui-ux.md` section 5.17: the 5 x 8 texts with the existing text probes, the
   `CW Trainer` cells with 8 x 16 golden pixels from `fixtures/font_golden.h` (the method of the training-screen
   tests), all other pixels off. After the splash, a list render matches the existing list goldens (the font is
   5 x 8 again).

New host tests, `firmware/Tests/test_menu_about.c`:

1. The main menu has 5 items; item 4 is `About` with kind `MENU_KIND_ABOUT`.
2. `enter` on `About` shows ABOUT with a redraw request, and no EDIT, dialog or mode starts.
3. `enter` and `menu/back` on ABOUT return to LIST with the cursor on `About`. `prev`, `next`, repeats and long
   events on ABOUT change nothing and request no redraw.
4. `menu_show_message()` on ABOUT shows the message; `enter` returns to the list on `About`.
5. Render: the title bar and the rows of `ui-ux.md` section 5.18, cell by cell; the version row is
   `"Version " FIRMWARE_VERSION`. The main-menu row `About` has no `>` in cell (5,24) (UX-61, V-162).
6. Version: `FIRMWARE_VERSION` matches `MAJOR.MINOR.PATCH` (digits and two dots), and it equals the first line of
   the file `FIRMWARE_VERSION_FILE`, read at run time (D-A14). The splash short text is `"v" FIRMWARE_VERSION`.

Scenarios: the two files together cover V-152 to V-169 of `ui-ux.md` section 9.5, with its probes (`TEXT5`,
`BIG16`, `SPLASH`, `ABOUT`, `MAIN`). Three scenarios run in a fixed way:

- V-166: the expected texts are built from `FIRMWARE_VERSION`, and the version check is item 6 (D-A14). No test
  holds `"0.1.0"`.
- V-167: the probes compute the start x of the splash text from the length of `FIRMWARE_VERSION`. A1 runs
  `test.sh` with `firmware/VERSION` = `10.12.3` and = `12345.12345.12345`, and both runs pass.
- V-168: an A1 build step, not a host test (D-A15).

### 17.9 Tasks

`c-embedded-developer` owns A0-A3. Each task ends with the items of section 12 (build, tests, memory table,
assumptions, open device checks). Comments follow the project rules.

#### A0 Baseline

- Outputs: the memory table and the stack estimate of the current tree, the `test.sh` result.
- Acceptance: both scripts pass. The numbers are recorded.

#### A1 Version build path

- Inputs: sections 17.2 (D-A1, D-A2, D-A13, D-A14) and 17.3.
- Outputs: `VERSION`, `firmware_version.cmake`, `version.h.in`, the two `CMakeLists.txt` changes, `build.sh`.
- Acceptance:
  - `build.sh` and `test.sh` pass and print `Firmware version: 0.1.0`.
  - Edit `firmware/VERSION` to `0.1.1`, run only `cmake --build firmware/build` and `cmake --build
    firmware/Tests/build`: both reconfigure without a manual step. The firmware `.asm` of `menu_tree.c` (after A2)
    holds the new strings, and the version test passes.
  - `firmware/VERSION` = `0.1`: the configure step stops with the message of section 17.3.
  - `firmware/VERSION` = `10.12.3`, then `12345.12345.12345` (17 characters): `build.sh` and `test.sh` pass (V-167).
  - `firmware/VERSION` = `123456.12345.12345` (18 characters): the configure step of `build.sh` and of `test.sh`
    stops with the length message (V-168). With the CMake length check removed in a scratch copy, the
    `_Static_assert` of `menu_tree.c` stops the compile.
  - `firmware/VERSION` is `0.1.0` again at the end.
  - `grep -rn "0\.1\.0" firmware --exclude-dir=build --exclude-dir=drivers` finds only `firmware/VERSION`.

#### A2 Start-up screen and `About`

- Inputs: sections 17.4 and 17.5, `ui-ux.md` sections 5.17 and 5.18.
- Dependency: the logic, the ids and the tests 17.8 items without pixels can start at once. The render waits for
  the two `ui-ux.md` sections. If they are not published, or they conflict with section 17.5, report `blocked` to
  `architector`.
- Outputs: the menu files, the helper, the setup changes, `test_menu_splash.c`, `test_menu_about.c`.
- Acceptance: the tests of section 17.8 pass; all earlier tests pass with only the changes that section 17.8
  allows; strings and the tree are in code memory (`.asm` or `.map`); the memory table is in the limits of 17.6.

#### A3 Developer close-out

- Output: the final report: memory tables of A0-A2, the stack count, the list of changed test expectations,
  `clang-format --dry-run --Werror` on the changed C files, assumptions, and the open device checks of 17.11.

### 17.10 QA checks by `qa-embedded` (QA-A)

QA runs after A3. QA adds its tests as `firmware/Tests/test_qa_menu_splash.c` and `test_qa_menu_about.c`.

- An independent check of each item of section 17.12.
- Boundaries: `MENU_SPLASH_MS - 1`, `MENU_SPLASH_MS`, tick wrap, a build with `-DMENU_SPLASH_MS=` another value.
- Event fuzz on the splash and on ABOUT (random codes and flags): the splash always ends on the first event; no
  call to settings, sidetone, contrast, backlight, stats or a mode.
- A held `prev` or `next` after the skip, with real button pins: the press only skips, the repeats act on the list
  as `ui-ux.md` section 5.17.2 says.
- Recovery start with real button pins: release before and after the 5 s timeout.
- An independent decode of the 8 x 16 `CW Trainer` cells from the `u8g2_fonts.h` bytes.
- The version path checks of A1, run again by QA, and a review that no file other than `firmware/VERSION` holds
  the version.
- A review of the screen-id renumber: no code or test uses a screen number literal.
- Verdict: `approved`, `changes_requested` or `blocked`.

### 17.11 Device checks (open until the user tests on the device)

| ID | Check |
| --- | --- |
| DC-A1 | The start-up screen looks like the mockup; the 8 x 16 `CW Trainer` is legible. |
| DC-A2 | The start-up screen shows for about 5 s, measured from power-on with a stopwatch or the logic analyzer. Note the full-frame flush time. |
| DC-A3 | Each button skips the screen, and the skip press has no other effect. |
| DC-A4 | The `About` text and the link rows are legible and complete. |
| DC-A5 | Recovery start: the start-up screen, then `Display Defaults`, then the main menu. |

### 17.12 Acceptance criteria

1. `firmware/VERSION` is the only source of the version (D-A1, A1 grep).
2. An edit of `firmware/VERSION` reaches the firmware and the host tests without a manual reconfigure. A bad format
   or a version longer than 17 characters stops the build (D-A15, V-168).
3. The behavior of section 17.5, proved by the tests of section 17.8 and QA-A.
4. The screens match `ui-ux.md` sections 5.17 and 5.18.
5. All earlier host tests pass; changed expectations are only those that section 17.8 allows.
6. `build.sh`, `test.sh` and `check_asm.sh` pass. Host flags unchanged, no new warning suppression.
7. Memory and stack in the limits of section 17.6.
8. QA verdict `approved`, then the `architector` review `approved`.

### 17.13 Other work

- W-A (`technical-writer`, after the `architector` approval): `docs/user_manual.md` (start-up screen, skip,
  `About`) and `docs/release_notes.md` (version 0.1.0).
- The version rule in `AGENTS.md` (plan section "Version rule") is a user-maintained file. The coordinating session
  adds it with the user's approval. No firmware agent edits `AGENTS.md`.
- After the work, ask the user if the version must change (the new rule). This change keeps `0.1.0`.

### 17.14 Risks

| Risk | Handling |
| --- | --- |
| `ui-ux.md` gives another recovery order or button reaction | Stop and report to `architector` (section 17.1). |
| The SDCC build does not rebuild `menu_tree.c` after a version change | A1 acceptance checks the `.asm`. If it fails, report to `architector`; the fallback is an explicit `OBJECT_DEPENDS` on `version.h`. |
| Older specs show the old screen numbers | D-A3 supersedes them. Code and tests use only the macros (QA-A review). |
| One stored message slot during the splash | Only `main.c` calls `menu_show_message()` before the first event; the last id wins (D-A9). |
| The flush shortens the visible splash time | DC-A2 measures it. A change needs a user decision. |

### 17.15 Final review decisions (2026-10-08)

Verdict: `changes_requested`. The review ran `build.sh` (flash 28 083 B, xdata 1817 B, stack start 0x41 with
191 B free), `test.sh` (79 of 79 pass), `tools/check_asm.sh` on the firmware objects (no violations) and the
version grep (only `firmware/VERSION`). A clean SDCC build gives no warnings.

Deviations of the developer:

| ID | Item | Decision |
| --- | --- | --- |
| DV-1 | SDCC 4.6.0 gives only warning 215 for a failed `_Static_assert` and still links. | Accepted for this change. The CMake length check is the binding guard for the target build. The `_Static_assert` of `menu_tree.c` stops the host build (`-Werror`). Follow-up F1. |
| DV-2 | The check is `sizeof(menu_text_version_short) - 2U <= 17U`. | Accepted. It is the same limit as D-A15 (the array holds `v`, the version and NUL). |
| DV-3 | The extra code-memory constant `menu_text_version_short_length`. | Accepted. It keeps `version.h` out of `menu_render.c` (D-A11) and costs 1 byte of flash. |
| DV-4 | The 5 x 8 splash texts use grid cells. | Accepted. The cells give the pixel positions of `ui-ux.md` section 5.17.1 (x = 2 + 5 x column). |
| DV-5 | V-153 checks only the pages that change. | Accepted. Page 6 is blank on both screens, so the dirty-page logic does not send it. The full-frame compare proves the result. Follow-up F2. |
| DV-6 | The test helper `support/menu_frame.h` and the new 8 x 16 glyphs in `font_golden.h`. | Accepted. The 5 x 8 data and the 3 old 8 x 16 glyphs do not change. |

QA findings:

| ID | Finding | Decision |
| --- | --- | --- |
| L1 | The `MEM_XDATA` parameter of `draw_plain()` adds a third xdata byte. | Accepted. +3 B is in the +6 B limit of section 17.6. It keeps internal RAM at 0 B, and other modules use the same pattern. |
| L2 | `file(STRINGS)` accepts a CRLF line or a UTF-8 BOM, but `cat` prints them. | Fix R1. |
| Info | `test_menu_training.c:204` and `test_qa_menu.c:247` compare `menu_screen()` with `2U`. | Fix R2 and R3 (section 17.10: no screen number literal). |
| Info | `test_menu_training.c:92-93` expect 7 and 8. | Accepted. The two lines pin the D-A3 values on purpose. They do not compare a run-time screen with a literal. |

Required fixes:

| ID | Owner | Fix | Evidence |
| --- | --- | --- | --- |
| R1 | `c-embedded-developer` | `firmware_version.cmake` checks the raw bytes as section 17.3 step 2 says. | The configure step stops for a CRLF file and for a BOM file, and passes for `0.1.0` with LF and without LF. `build.sh` and `test.sh` pass, with the memory table. |
| R2 | `c-embedded-developer` | `test_menu_training.c:204` uses `MENU_SCREEN_SPLASH`. | `test.sh` passes. |
| R3 | `qa-embedded` | `test_qa_menu.c:247` uses `MENU_SCREEN_SPLASH`. | `test.sh` passes. |

Follow-ups. Each needs a ticket before work starts.

- F1: make SDCC stop on a failed `_Static_assert` for the whole target build, for example with `--Werror`. It
  changes the build of all modules and the 24 other `_Static_assert` checks in `src/`, so it is a separate change.
  The clean build has no warnings now, so the cost is low.
- F2: `ui-ux-embedded-designer` changes V-153 in `ui-ux.md` section 9.5 from "all 8 pages redrawn" to "every
  changed page sent, full frame equal to `MAIN(1)`".

Re-review (2026-10-08): `approved`. R1: the review ran `firmware_version.cmake` on test files. `0.1.0` with LF and
without LF passes. CRLF, BOM, two LF and a trailing space stop the script. R2 and R3 use `MENU_SCREEN_SPLASH`. F2:
V-153 in `ui-ux.md` section 9.5 names pages 0-5 and 7. `build.sh` gives flash 28 083 B, xdata 1817 B and stack
start 0x41 with 191 B free. `test.sh` passes 79 of 79 tests. `tools/check_asm.sh` finds no violations. F1 stays
open and needs a ticket.

Change (2026-10-08, user decision): the start-up screen time changes from 2 s to 5 s. `MENU_SPLASH_MS` in `menu.h`
changes from `2000U` to `5000U`. D-A6, the section 17.4 listing, the QA-A recovery-start check and DC-A2 now say
5 s. Nothing else changes: the tests use the macro, and the `uint16_t` tick difference wraps only at 65 535 ms.

Review of the 5 s change (2026-10-08): `approved`. QA verdict `approved`. `menu.h` sets `5000U`; the `menu.c`
object compares with `0x1388`. The splash tests use times relative to `MENU_SPLASH_MS`. `build.sh` gives flash
28 083 B, xdata 1817 B and stack start 0x41 with 191 B free, as before. `test.sh` passes 79 of 79 tests and the
168 `check_asm` self-test cases. `ui-ux.md` (UX-59, V-152 to V-161), `docs/user_manual.md` and
`docs/release_notes.md` say 5 s; the other 2 s values there are unrelated hold times. DC-A2 stays open.
