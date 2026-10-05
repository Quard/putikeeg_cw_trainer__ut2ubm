# UI/UX decision record

Scope: the whole device (one product, one display, one set of buttons). Owner: `ui-ux-embedded-designer`.
This file is the durable UI/UX contract. The implementation spec (`menu_subsystem_spec.md`, owned by `architector`)
refers to this file for all visual and interaction details.

Status of the content:

- **Fact**: comes from a repository source that is named in the text.
- **Decision**: binding for the implementation. Decisions marked "plan" come from the plan that the user approved.
  Decisions marked "UX" are made in this file.
- **Proposed**: needs approval from the user before anybody implements it.
- **Unverified**: an assumption that needs a check on the device. Host tests cannot prove it.

## 1. Scope

- Product: Putikeeg CW Trainer, board CW_MINI_V2.0D, custom firmware.
- UI scope in this version: the menu tree, the Settings editor, placeholder screens for the three modes, the
  Reset Statistics dialog, button input, the live effect of contrast and backlight, and the Tone preview
  (section 6.5).
- Out of scope in this version: the Receiving, Sending and Free Input modes, paddle keying, any sound other than the
  Tone preview, statistics.
- Users: amateur radio operators who learn or practise Morse code. They know the terms WPM, RX, TX, Iambic A/B and
  Farnsworth (effective) speed.
- Environment: a handheld device or a device on a desk, indoor light, viewing distance about 30-60 cm
  (unverified). Long practice sessions.
- Frequent tasks: start a mode from the main menu (most frequent), change a speed or the tone (sometimes),
  change contrast or backlight (rare), reset statistics (rare).

## 2. Sources

| Source | What it gives |
| --- | --- |
| `references/internal/components_and_wiring.md` | Buttons, pins, display, paddle socket, backlight pin. |
| `firmware/inc/u8g2_fonts.h` (`u8g2_font_spleen5x8_mf`) | Font header bytes and glyph bitmaps (decoded, see section 3.3). |
| `firmware/drivers/u8g2_font_render/u8g2_font_render.c` | How the driver places a glyph relative to the `y` argument. |
| `firmware/inc/lcd/lcd_fb.h`, `firmware/inc/board/lcd_config.h` | 128 x 64 framebuffer, 8 pages of 8 rows, contrast range 0-63, default 0x20. |
| `references/context/STC8H_datasheet.md` | EEPROM sector erase about 4-6 ms, byte program about 30-40 us. Avoid EEPROM writes at low voltage. |
| `references/internal/sidetone_spec.md` (owner `architector`) | Sidetone API, 8 ms rise and fall, retune through silence, automatic route, menu hooks H1-H6 (section 10.3). |

`llm.md` is not present at the repository root (AGENTS.md asks agents to read it).

## 3. Inventories

### 3.1 Controls (fact, unless marked)

| ID | Control | Pin | Behaviour | Notes |
| --- | --- | --- | --- | --- |
| C-1 | `next` button | P0.0 | Active low, no external pull-up. | Physical position on the case is not documented (unverified). |
| C-2 | `prev` button | P0.1 | Active low, no external pull-up. | Same as C-1. |
| C-3 | `menu/back` button | P0.2 | Active low, no external pull-up. | |
| C-4 | `enter` button | P3.2 | Active low. Shared with the tip of the paddle/key socket. | A paddle dit contact or a straight key also presses `enter`. Inserting a plug can short the tip for a short time (unverified). |
| C-5 | Paddle ring | P5.5 | Dah contact. | Not used by the menu. The menu ignores it. |
| C-6 | `on/off` button | none | Not read by the MCU (no pin in the wiring table). | Unverified: the effect of the button on the MCU power is not known. |
| C-7 | Volume potentiometer | analog | Not read by the MCU. Sets the sidetone loudness in the analog path (`components_and_wiring.md`). | The firmware has no volume control. At the lowest setting the Tone preview can be silent. |

Semantic assumption (unverified, D-8 in section 11): `prev` means up, left and decrease. `next` means down, right
and increase. If the buttons on the case are in a different order, this file needs a review.

### 3.2 Outputs

| ID | Output | Budget | Notes |
| --- | --- | --- | --- |
| O-1 | LCD, 128 x 64, monochrome, probably ST7567 | 8 text rows x 25 columns with the 5 x 8 font | Contrast 0-63 (`lcd_set_contrast`), default 0x20 = 32. The full-screen transfer time is not measured yet. |
| O-2 | LCD backlight, HX4004A on P0.3 | Levels 0-10, duty = level x 10 % (plan) | Perceived brightness per level is unverified. |
| O-3 | Sidetone, PWM on P1.1 (speaker) or P1.2 (headphones) | One tone at a time, 400-1000 Hz. Rise 8 ms, fall 8 ms. A tone change while sounding falls to silence, waits for the carrier update (at most 3 ms) and rises again: a dip of about 17-19 ms. | Used only for the Tone preview (section 6.5). The route is automatic. With the plug in (P1.0 LOW), the headphones play. Otherwise the speaker plays. A plug change takes effect at the next silence. Loudness comes from C-7. Facts from `sidetone_spec.md` sections 2, 6.3, 10.3. The sound at the ear is unverified (D-10 to D-14). |

### 3.3 Font facts (decoded from `u8g2_fonts.h`)

Header bytes of `u8g2_font_spleen5x8_mf`:

| Field | Value |
| --- | --- |
| Glyph count | 192 |
| Bounding box mode | 2 (monospace) |
| Bounding box | 5 x 8, x offset 0, y offset -1 |
| Ascent of `A`, descent of `g` | 6, -1 |
| Ascent and descent of `(` | 7, -1 |

Decoding every glyph with the same arithmetic as the driver gives:

- Every glyph is a full 5 x 8 bitmap with pitch 5. Ink is in cell columns 0-3. Column 4 is blank, except for
  `# & + T ^ ~`. Letters next to these glyphs can touch.
- Cell rows: capital letters and digits use rows 1-6. These glyphs also use row 0: `! " $ % & ' ( ) / ? [ \ ] b d f h k l t { | }`
  and the backtick. These glyphs also use row 7: `$ % ( ) , / ; Q [ \ ] _ g j p q y { | }`.
- **Driver offset (decision UX-18, user):** the driver computes the first glyph row as
  `y + bbx_height - glyph_height - glyph_y_offset` = `y + 1`. It does not use the bounding box y offset (-1). So a
  glyph drawn at `y` covers pixel rows `y + 1` to `y + 8`. The driver stays unchanged. The `gfx` module compensates:
  it draws at the row top minus the font y offset, so a text cell for row `r` is drawn at `y = 8r - 1`. The visible
  result is binding: cell row 0 is on pixel row `8r`.
- Use only ASCII 0x20-0x7E in UI strings (decision UX-11). The glyph search compares a `char` with a byte. A
  character above 0x7F depends on the signedness of `char`.

## 4. Global display conventions

### 4.1 Grid (decision UX-1)

- Text cell `(r, c)`, row `r` = 0..7, column `c` = 0..24: pixels `x = 2 + 5c` to `6 + 5c`, `y = 8r` to `8r + 7`.
- Margins: x 0-1 on the left, x 127 on the right (the blank glyph column at x 126 adds to it). Text never uses
  x 0, 1 or 127. Tests use these margin columns as font-independent probes.
- Row 0 is the title bar. Rows 1-7 are content. Every text row and its highlight stay inside one LCD page, so a
  cursor move dirties two pages and a value change dirties one page.
- Text never wraps. A string that is too long is cut at column 24. All strings in this file fit. A cut is a defect.

### 4.2 Video modes (decision UX-2)

| Element | Pixel rows | x range | Video |
| --- | --- | --- | --- |
| Title bar | 0 to 7 (full 8 rows) | 0-127 | Inverted: background on, glyph ink off. |
| Normal row | `8r` to `8r+7` | 0-127 | Normal: background off, glyph ink on. |
| Highlight band | `8r+1` to `8r+7` (7 rows) | depends on the element | Inverted: background on, glyph ink off. |
| Gap line | `8r` | the x range of the highlight band (browse: 0-127, edit: `106 - 5 * fw` to 127, button: its band) | Off across the band, also where a glyph has ink in cell row 0. Outside a band, row `8r` is normal video and keeps its glyph ink (for example the label in edit mode). |

The gap line keeps a selected row 1 apart from the title bar. Inside a band, glyph ink in cell row 0 is off. So the
tops of `l`, `t` and `h` join the gap line. The letters stay readable on the mockup. The device check is D-1.

There is exactly one highlight band on a screen at a time. The highlight is the only focus indicator. Colour does
not exist on this display. The band is a shape, not a shade.

### 4.3 Text rules

- Item labels, screen titles and choice values use Title Case: `Show CW Code`, `Iambic B`. Sentences in dialogs use
  sentence case: `Reset statistics?` (decision UX-10).
- Numbers: decimal, no leading zeros, no plus sign. A unit follows after one space: `600 Hz`, `32 WPM`.
- Values are right-aligned so that the last character is in column 22. Column 23 is always blank. Column 24 holds
  the submenu marker `>` or the edit marker `>` (section 5.4).

## 5. Screen types

### 5.1 Title bar

- Row 0, inverted, x 0-127, y 0-7.
- The title starts in column 0.
- Title of the main menu: `CW Trainer`. Title of a list: the path from Settings, joined with ` > `
  (`Settings`, `Settings > CW`, `Settings > Display`). Title of a placeholder, dialog or message screen: the label
  of the item that opened it.
- Scroll position (section 5.5): when a list has more than 7 items, the title bar also shows `i/n` right-aligned
  so that its last character is in column 24 (`i` = 1-based cursor index, `n` = item count). The title is then cut
  so that at least one blank column stays before `i/n`.

### 5.2 List rows (browse mode)

```
    0         1         2
    0123456789012345678901234
0  |Settings > CW            | title bar, inverted
1  |Tone             600 Hz  | selected: highlight x 0-127, y 9-15
2  |RX Speed         32 WPM  |
3  |RX Eff. Speed    32 WPM  |
4  |TX Speed         20 WPM  |
5  |TX Eff. Speed    20 WPM  |
6  |Key Type       Iambic B  |
7  |                         |
```

- Label in column 0.
- Value item: value right-aligned, last character in column 22.
- Submenu item: `>` in column 24, no value.
- Mode item and action item: label only, no marker.
- Selected row: highlight band over x 0-127. The label, value and marker of that row are inverted.
- Rows below the last item are blank (all pixels off).

### 5.3 Labels, values and field widths (decision UX-3)

Each value item has a fixed field width `fw` = the length of its longest value text. The field does not move while
the value changes.

| Level | Label | Kind | Values shown | fw | Default (plan) |
| --- | --- | --- | --- | --- | --- |
| Main | `Receiving` | mode | - | - | - |
| Main | `Sending` | mode | - | - | - |
| Main | `Free Input` | mode | - | - | - |
| Main | `Settings` | submenu | - | - | - |
| Settings | `CW` | submenu | - | - | - |
| Settings | `Show CW Code` | choice | `Off`, `On` | 3 | `On` |
| Settings | `Max Word Length` | value | `4` .. `8`, step 1 | 1 | `5` |
| Settings | `Display` | submenu | - | - | - |
| Settings | `Reset Statistics` | action | - | - | - |
| CW | `Tone` | value | `400 Hz` .. `1000 Hz`, step 10 | 7 | `600 Hz` |
| CW | `RX Speed` | value | `5 WPM` .. `50 WPM`, step 1 | 6 | `32 WPM` |
| CW | `RX Eff. Speed` | value | `5 WPM` .. RX Speed, step 1 | 6 | = RX Speed |
| CW | `TX Speed` | value | `5 WPM` .. `50 WPM`, step 1 | 6 | `20 WPM` |
| CW | `TX Eff. Speed` | value | `5 WPM` .. TX Speed, step 1 | 6 | = TX Speed |
| CW | `Key Type` | choice | `Straight`, `Iambic A`, `Iambic B` | 8 | `Iambic B` |
| Display | `Backlight` | value | `Off` (level 0), `1` .. `10` | 3 | `8` |
| Display | `Contrast` | value | `0` .. `63`, step 1 | 2 | `32` |

Order of the items in each list is the order in this table. Choice order (prev = towards the first entry, next =
towards the last entry) is the order in the "Values shown" column.

Fit rule: `len(label) <= 20 - fw`. This keeps one blank column between the label and the `<` edit marker. Every
row above passes. The tightest rows are `RX Eff. Speed` and `TX Eff. Speed` (13 of 14 allowed).

`RX` = receive, `TX` = send (decision UX-3, decided by the user). The full words do not fit with the edit markers.

### 5.4 Edit mode (decision UX-4)

```
    0         1         2
    0123456789012345678901234
0  |Settings > CW            |
1  |Tone             600 Hz  |
2  |RX Speed         32 WPM  |
3  |RX Eff. Speed  < 25 WPM >| edit: highlight x 76-127, y 25-31; label normal video
4  |TX Speed         20 WPM  |
5  |TX Eff. Speed    20 WPM  |
6  |Key Type       Iambic B  |
7  |                         |
```

- The highlight moves from the whole row to the value field. The label goes back to normal video. This change of
  the highlight shape is the edit-mode marker.
- Field layout: value in columns `23 - fw` to 22 (right-aligned inside the field), `<` in column `21 - fw`,
  column `22 - fw` blank, column 23 blank, `>` in column 24.
- Highlight band: x from `106 - 5 * fw` to 127 (one pixel left of the `<` cell), y `8r + 1` to `8r + 7`.
  Examples (fw -> x): 1 -> 101, 2 -> 96, 3 -> 91, 6 -> 76, 7 -> 71, 8 -> 66.
- Condition for `<`: prev can change the value (value > its minimum, or not the first choice).
  Condition for `>`: next can change the value (value < its current maximum, or not the last choice).
  At a limit the marker cell is blank but stays inside the highlight.
- In browse mode both marker cells are blank and are not highlighted separately (the row highlight covers them).

Examples at the limits:

```
1  |Tone          < 1000 Hz  |   at the maximum: no '>'
1  |Backlight           Off >|   at the minimum: no '<'
3  |RX Eff. Speed  < 32 WPM  |   RX Speed is 32: the effective maximum is reached, no '>'
```

### 5.5 Scrolling (decision UX-5)

No list in this version has more than 7 items (main 4, Settings 5, CW 6, Display 2), so no list scrolls now.
The rule applies to later lists:

- The window shows 7 items. Minimum movement: the window moves only by the rows that keep the cursor inside it.
- After a wrap (section 6.3) the window shows the first 7 or the last 7 items.
- The title bar shows `i/n` (section 5.1). There is no scroll bar.

### 5.6 Placeholder screen (decision plan + UX-6)

```
0  |Receiving                | title bar, inverted
3  |        Not ready        | column 8
```

- Title: `Receiving`, `Sending` or `Free Input`. Body: `Not ready` in row 3, column 8. All other rows blank.
- No highlight on the screen.

### 5.7 Reset Statistics dialog (decision plan + UX-7)

```
0  |Reset Statistics         | title bar, inverted
3  |    Reset statistics?    | column 4
5  |        No    Yes        | No: columns 8-9, Yes: columns 14-16
```

- The focused button has a highlight band in row 5: `No` x 37-56 (cells 7-10), `Yes` x 67-91 (cells 13-17).
- The focus is on `No` each time the dialog opens.
- Centering rule for all body text: start column = floor((25 - length) / 2).

### 5.8 Message screen (decision UX-8)

Shown after `Yes` in the dialog:

```
0  |Reset Statistics         | title bar, inverted
3  |    Statistics reset     | column 4
5  |           OK            | columns 11-12, highlight x 52-71 (cells 10-13), always focused
```

Shown after a display recovery start (section 8.1):

```
0  |Display Defaults         | title bar, inverted
2  |  Default contrast and   | column 2
3  |  backlight are in use.  | column 2
5  |           OK            | columns 11-12, highlight x 52-71 (cells 10-13), always focused
```

Message screen rules: body text in rows 2-3 (one line: row 3), each line centered. `OK` in row 5, always focused.
`enter` or `menu/back` closes the screen. The same layout is used for any later one-button message (for example a
save error, see question Q-4).

## 6. Control mapping

### 6.1 Per state (decision plan + UX-9)

`press` = a debounced press event. `repeat` = an auto-repeat event (prev and next only, section 6.2).

| State | `prev` press | `next` press | `prev`/`next` repeat | `enter` | `menu/back` |
| --- | --- | --- | --- | --- | --- |
| List, browse | Cursor up. Wraps from first to last | Cursor down. Wraps from last to first | Same as press, but stops at the first/last item (no wrap) | Mode: open placeholder. Submenu: open the list, cursor on its first item. Value/choice: enter edit mode. Action: open the dialog. | Main menu: nothing (no redraw). Settings list: go to the main menu with the cursor on `Settings`. Save under the rule in section 7 (UX-15). CW or Display list: go to Settings with the cursor on `CW` or `Display`. No save. |
| List, edit | Decrease the value by one step, or the previous choice. Stops at the limit. Applies at once. | Increase, or the next choice. Stops at the limit. Applies at once. | Same as press, stops at the limit | Leave edit mode, keep the value. Cursor stays on the row. | Leave edit mode, keep the value (same as `enter`). Does not leave the list. |
| Placeholder | nothing | nothing | nothing | nothing | Back to the main menu, cursor on the mode item that opened it. |
| Dialog | Focus `No` (no wrap) | Focus `Yes` (no wrap) | Same as press | Activate the focused button. `No`: back to Settings. `Yes`: reset statistics, show the message screen. | Same as `No`. |
| Message | nothing | nothing | nothing | `Statistics reset`: back to Settings, cursor on `Reset Statistics`. `Display Defaults`: the main menu, cursor on `Receiving`. | Same as `enter`. |

Other rules:

- "Applies at once": contrast goes to the LCD and the backlight level goes to the PWM at each step. The other
  values change only in memory until the save. They have no visible effect in this version. Tone is the only value
  with an audible effect: the Tone preview (section 6.5).
- Effective-speed rule (plan): an effective speed can never be higher than its base speed. Lowering a base speed
  below its effective speed lowers the effective speed in the same step. Raising a base speed never changes the
  effective speed.
- A list opened from its parent always starts with the cursor on its first item. Going back restores the cursor of
  the parent on the item that was opened.
- After power-on the main menu shows with the cursor on `Receiving`. A display recovery start shows the
  `Display Defaults` message first (section 8.1).
- There is no inactivity timeout anywhere in this version. There is no long-press function. The Tone preview
  timeout (section 6.5) is not a menu timeout: it ends the sound and changes no screen.

### 6.2 Button timing (decision UX-12)

| Parameter | Value | Notes |
| --- | --- | --- |
| Debounce, press | Pin low without interruption for 20 ms. The press event comes at that moment. | Bounce time of the buttons and the paddle is unverified (D-4). |
| Debounce, release | Pin high without interruption for 20 ms. | A bounce on release never gives a second press. |
| Auto-repeat buttons | `prev` and `next` only | A held `enter` or `menu/back` gives one event per press. |
| First repeat | 400 ms after the press event | |
| Repeat period | 80 ms (12.5 steps per second) | Full Tone range: 60 steps, about 5.1 s. Full Contrast range: 63 steps, about 5.4 s. No acceleration. |
| Two of prev/next held | The repeat belongs to the one pressed last. Releasing it stops the repeat. The other one does not start repeating again until it is pressed again. | |
| Button held at start-up | Gives no event until it is released and pressed again. | Protects against a closed key/paddle at power-on. After the display recovery gesture (section 8.1), the held `menu/back` does not close the message screen. |
| Two press events due in the same poll | Order: `menu/back`, `enter`, `prev`, `next`. | Deterministic for tests. |
| Timing tolerance | +/- 2 ms against the 1 ms tick | For host tests. |

### 6.3 Wrap and limits (decision UX-13)

- The list cursor wraps on a press event, so the last item is one press away from the first.
- The list cursor does not wrap on a repeat event. Holding a button stops at the end of the list and does not spin.
- Values and choices never wrap. At a limit, a press or repeat changes nothing. The missing `<` or `>` marker
  shows the limit (section 5.4). No message. A step at a limit does not start or restart the Tone preview
  (section 6.5, H3).

### 6.4 Shared enter and paddle

- In the menu, a paddle dit contact or a straight key acts as `enter`. This is a hardware fact and the menu accepts
  it.
- A key held closed gives one `enter` event only. The other three buttons keep working.
- If inserting a plug causes a short tip contact longer than 20 ms, it gives one `enter` (unverified, D-5). The
  dialog focus starts on `No`, so such an event never resets statistics. In a list it can open an item or enter
  edit mode. `menu/back` undoes that without a change.

### 6.5 Tone preview (decision UX-20, user Q-3)

User decision (Q-3): a Tone edit plays the tone. This is the only sound in this version. There is no keyer.

Facts that the design uses (`sidetone_spec.md` sections 3.9, 6.3, 10.3):

- A start rises in 8 ms. A stop falls in 8 ms. The envelope prevents clicks (unverified, DC-6).
- A tone change while the tone sounds goes through silence: fall 8 ms, wait for the carrier update (one carrier
  period, 1 ms at 1000 Hz to 2.5 ms at 400 Hz, at most 3 ms), rise 8 ms at the new tone. The dip is about
  17-19 ms.
- A request for the tone that the sidetone already has does nothing.
- The buttons give press and repeat events only. There is no release event.

#### Preview state

The preview is `running` or `stopped`. It is `stopped` after power-on and after a display recovery start. Only the
Tone EDIT screen can start it. While it is `running`, the menu keeps one start time.

`MENU_TONE_PREVIEW_MS` = **300** (decision UX-21).

#### Hook behaviour

`tone(hz)` = `sidetone_set_tone_hz(hz)`. `on(x)` = `sidetone_set_on(x)`. `now` = `tick_now()`.

| Hook | Event | Sidetone calls, in this order | Preview state after the hook |
| --- | --- | --- | --- |
| H1 | `enter` opens the EDIT screen on the Tone item. | `tone(current value)`, `on(true)` | `running`, start = now |
| H2 | A `prev` or `next` press or repeat in Tone edit changes the value. | `tone(new value)`, `on(true)` | `running`, start = now. This is a restart. A `stopped` preview starts again. |
| H3 | A `prev` or `next` press or repeat in Tone edit changes nothing (limit). | none | unchanged: a `running` preview keeps its start time, a `stopped` preview stays `stopped` |
| H4 | `menu_poll()` while `running`, and `(uint16_t)(now - start) >= MENU_TONE_PREVIEW_MS`. | `on(false)`, once | `stopped` |
| H5 | The Tone EDIT screen ends by any path: `enter`, `menu/back`, or a message screen shown over it (`menu_show_message`). | `on(false)`, exactly once. A `stopped` preview gets this call too. | `stopped` |
| H6 | Every other screen, item, edit and event. This includes the Tone row in browse mode. | none | `stopped` |

Rules:

- A hook that starts the tone always calls `tone()` first and `on(true)` second.
- `menu_poll()` while `stopped` calls nothing and reads no tick.
- H4 uses `>=`. At exactly 300 ms after the start, the stop call comes.
- The timer works across the 16-bit tick wrap.
- `menu_poll()` runs once per main-loop pass. A slow redraw delays the stop by up to one pass (unverified, D-13).
- When one main-loop pass handles several events, each event is its own hook. The newest tone wins, and the start
  time is the time of the last change.
- No hook depends on the sound. The display shows the value at all times. With the volume at its minimum, the edit
  works the same.

#### What the user hears (illustrative, from the sidetone timing)

Single press (`enter` to edit, then one `next`):

| Time (ms) | Event | Sound |
| --- | --- | --- |
| 0 | `enter` (H1) | Rise to 600 Hz |
| 8-300 | | 600 Hz at full level |
| 300-308 | timeout (H4) | Fall, then silence |
| 1000 | `next` (H2) | Rise to 610 Hz |
| 1008-1300 | | 610 Hz at full level |
| 1300-1308 | timeout (H4) | Fall, then silence |

Held `next` (press at 0, repeats at 400, 480, 560, ...):

```text
0-300     610 Hz, one beep of about 300 ms
300-400   fall, then about 92 ms silence (the repeat delay)
400-...   one step every 80 ms: a 17-19 ms dip to the next pitch, then about 61-63 ms at full level
release   the last pitch plays 300 ms after the last change, then falls
limit     the last change reaches the limit. 300 ms later the tone ends, while the button is still held
```

The silence at the limit and the missing `<` or `>` marker both show that the value cannot move further.

#### Held repeat and the retune dip (decision UX-22)

A repeat event follows the same rule as a press (H2). The retune dip during a held repeat is accepted. Reasons:

- The user hears the pitch move while holding and can release near the wanted pitch by ear. Audio feedback is
  faster than the display during a repeat (UX-16 allows the display to skip a value).
- The dip is a shaped fall and rise, not a click. It makes each 10 Hz step audible as one short tone.
- The user decision already accepts the 80 ms repeat. The preview does not change that rhythm.

The preview needs no change of the sidetone interface for this. If the device check D-12 finds the chain
unpleasant, the options are in section 10 (UX-22, rejected alternatives). Each of them needs an `architector`
decision first.

#### Interruptions and recovery

| Situation | Behaviour |
| --- | --- |
| Paddle dit or straight key on the shared `enter` pin in Tone edit | It is an `enter` event: the edit ends, the tone stops (H5). The user presses `enter` again to edit. |
| Plug insert or remove while the preview sounds | The tone continues on the old route. The next start uses the new route. During a held repeat each step passes through silence, so the route changes at the next step. |
| Leaving Settings (save) | No sound. The edit already ended with H5. |
| Power loss | No preview state survives. The start-up is silent. |
| Display recovery start | Silent. The `Display Defaults` message screen makes no sound. |
| Volume at its minimum | No sound. The edit works as before. |

## 7. Feedback

| Situation | What the user sees | Decision |
| --- | --- | --- |
| Cursor moves | The highlight band moves to the new row. | UX-2 |
| Enter edit mode | The highlight shrinks to the value field. `<`/`>` appear. | UX-4 |
| Value at its min/max | The `<` or `>` marker disappears. Further presses change nothing. | UX-13 |
| Effective speed at its base speed | `>` disappears on the effective-speed row, the same as a hard maximum. The base value is in the row above on the same screen. | UX-14 |
| Base speed lowered below the effective speed | The effective-speed row changes in the same redraw as the base row. Both rows are on the CW screen. | UX-14 |
| Contrast or backlight edit | The LCD contrast or backlight changes at each step. | plan |
| Tone edit | The user hears the tone for 300 ms at the edit start and after each change. Leaving the edit stops it at once (8 ms fall). At a limit, nothing new sounds. | UX-20, UX-21, UX-22 |
| Save on leaving Settings | Nothing extra. The main menu appears with the cursor on `Settings`. The erase and write take about 5-7 ms (datasheet: erase 4-6 ms, 15 bytes x 40 us), which the user does not notice. | UX-15 |
| Save not needed | Same screen, no EEPROM access. A save needs two conditions: at least one edit step changed a value during this Settings visit, and the current values differ from the stored record. A value changed and changed back does not cause a write. A visit without an edit step does not cause a write, also after a display recovery start. | UX-15 |
| Reset Statistics `Yes` | The message screen `Statistics reset` / `OK`. | UX-8 |
| Response time | Target: the change is visible within 100 ms after the press event (unverified, D-6). A redraw can be slower than 80 ms during auto-repeat. Then the screen skips an intermediate value, but no step is lost. The value advances once per event, and the last value shows within 100 ms after release. | UX-16 |

## 8. Start-up, faults and recovery

| ID | Condition | What the user sees | Device behaviour | Recovery |
| --- | --- | --- | --- | --- |
| F-1 | First power-on, empty EEPROM | The main menu, no message. Settings show the defaults. | Defaults load. Nothing is written until the user changes a value and leaves Settings. | None needed. |
| F-2 | Record with a wrong magic, version or CRC (for example a power loss during a save) | Same as F-1, no message. | Defaults load. | The user sets the values again. |
| F-3 | Valid record with a value out of range | The clamped value. | Each field is clamped. An effective speed above its base speed becomes the base speed. | None needed. |
| F-4 | Power loss while inside Settings | After power-on, the values from before the visit. | Unsaved changes are lost (plan). | The user sets the values again. |
| F-5 | Contrast set to a value that makes the screen unreadable | Blank or black screen. | The value applies at once. If the user leaves Settings, it is saved. | During the edit: press the opposite button. After a save: the display recovery start (section 8.1). |
| F-6 | Backlight `Off` in the dark | No light. | Level 0 is a valid setting. | Same as F-5. |
| F-8 | `menu/back` held at start-up | The message screen `Display Defaults`, then the main menu. | Section 8.1. | - |
| F-7 | EEPROM write fails (read-back mismatch, CMD_FAIL or low supply) | The message screen: title `Settings`, text `Not saved`, `OK`. `enter` or `menu/back` closes it to the main menu with the cursor on `Settings`. | `eeprom_write` reads back each byte. The save stops with `SETTINGS_SAVE_FAILED`. The values stay active. The magic is written last, so a stopped save leaves no valid record (`menu_subsystem_spec.md` sections 3.5 and 3.6). | The next exit from Settings tries the save again. If the user switches off first, the next power-on uses the defaults (F-2). |

Decision UX-17: F-1 and F-2 are silent. Reasons: the user can see and fix every value in Settings. A message on
first power-on is noise. A corruption needs a power loss inside a 6 ms window. Rejected: a one-time
`Settings reset to defaults` message (costs a state and a string for a very rare event).

### 8.1 Display recovery start (decision UX-19, user)

Purpose: a way back from a contrast or backlight setting that makes the screen unusable.

Gesture: hold `menu/back`, then switch the device on. Keep holding until the screen shows (about 0.1 s after
power-on is enough).

Detection window:

- Sampling of P0.2 starts 1 ms after the internal pull-up is on, before contrast and backlight are first applied.
- P0.2 is sampled once per 1 ms tick. If P0.2 is low in every sample from the first sample up to and including the
  sample at 50 ms, the recovery start is active.
- A high sample in that window ends the detection at once. Then the start is normal and adds no further delay.
- The check adds at most 51 ms to the start-up, and only while `menu/back` is held.
- Only P0.2 counts. `enter` (paddle tip), `prev` and `next` have no start-up function.

Effect:

- The live contrast is `LCD_DEFAULT_CONTRAST` (32) and the live backlight level is 8. The LCD and the backlight
  get these values before the first frame shows. So the user never sees the stored bad state.
- All other settings come from the stored record as usual.
- The stored record does not change at start-up. Settings > Display shows `8` and `32`.
- The record changes only under the normal save rule (UX-15): the user makes at least one edit step in Settings and
  then leaves Settings. That save writes all live values, also the default contrast and backlight. If the user
  does not do that, the next normal start uses the stored values again.

What the user sees:

1. The message screen `Display Defaults` / `Default contrast and` / `backlight are in use.` / `OK` (section 5.8).
2. The held `menu/back` gives no event (section 6.2). After the release, `enter` or `menu/back` closes the message.
3. The main menu with the cursor on `Receiving`.

How to keep the defaults: the user manual explains it (`technical-writer`). The screen has no space for the hint.

Unverified: the effect of the `on/off` button on the MCU power (C-6). If the MCU does not start from a power-off
when the user switches on, the gesture needs a review. Device check D-9.

## 9. Acceptance scenarios for host tests

Probes (all coordinates are pixels in the framebuffer):

- `TITLE`: pixels (0, y), (1, y) and (127, y) are on for y = 0..7.
- `SEL(r)`: pixels (0, y), (1, y), (127, y) are on for y = 8r+1..8r+7, and all pixels with y = 8r are off.
- `PLAIN(r)`: pixels (0, y), (1, y), (127, y) are off for y = 8r..8r+7.
- `EDIT(r, fw)`: for y = 8r+1..8r+7: (0, y) and (1, y) are off, (105 - 5fw, y) is off, (106 - 5fw, y) is on,
  (127, y) is on. Pixels with y = 8r are off for x = 106 - 5fw..127. Pixels with y = 8r and x < 106 - 5fw are
  not checked by this probe (they belong to the normal-video label).
- `TEXT(r, c, s, inv)`: the cells from (r, c) match a reference rendering of `s` with the same font in normal
  (`inv` = false) or inverted (`inv` = true) video. Every other cell of row r that is outside a highlight band is
  blank.
- `BUTTON(x0, x1)`: in row 5, pixels (x0, y) and (x1, y) are on for y = 41..47, (x0 - 1, 44) and (x1 + 1, 44) are
  off.

Unless a scenario says otherwise, the start state is a power-on with an empty fake EEPROM, and "press X" means one
debounced press and release.

| ID | Setup / action | Expected result |
| --- | --- | --- |
| V-01 | Power-on, empty EEPROM. | `TITLE`, `TEXT(0,0,"CW Trainer",inv)`. `SEL(1)`, `TEXT(1,0,"Receiving",inv)`. `TEXT(2,0,"Sending")`, `TEXT(3,0,"Free Input")`, `TEXT(4,0,"Settings")` and `TEXT(4,24,">")`. Rows 5-7 have all pixels off. No EEPROM erase or write. |
| V-02 | Power-on with a record that has a bad CRC. Open Settings > CW and Settings > Display. | No message screen. CW shows `600 Hz`, `32 WPM`, `32 WPM`, `20 WPM`, `20 WPM`, `Iambic B`. Settings shows `On`, `5`. Display shows `8`, `32`. |
| V-03 | Main menu: press prev, then press next. | After prev: `SEL(4)`, `PLAIN(1)`. After next: `SEL(1)`. |
| V-04 | Main menu: hold next for 1000 ms. | Cursor stops on `Settings` (`SEL(4)`). It does not wrap to `Receiving`. |
| V-05 | Enter Settings. Enter CW. Press menu/back. | Title `Settings`, `SEL(1)` on `CW`. Then title `Settings > CW`, `SEL(1)` on `Tone`. Then title `Settings`, `SEL(1)` on `CW`. No EEPROM access. |
| V-06 | CW: enter on Tone. Next. Prev. Prev. Enter. | `EDIT(1,7)`, `TEXT(1,14,"<",inv)`, `TEXT(1,24,">",inv)`, `TEXT(1,0,"Tone")` normal video. Values 610, 600, 590 Hz in turn. After enter: `SEL(1)`, `TEXT(1,17,"590 Hz",inv)`. |
| V-07 | Tone at 1000 Hz in edit mode. Press next. Tone at 400 Hz. Press prev. | At 1000: cell (1,24) is blank inside the band, `<` shown. Next changes nothing. At 400: cell (1,14) blank inside the band, `>` shown. Prev changes nothing. |
| V-08 | Tone 600 Hz in edit mode. Hold next. Release at 1000 ms after the press event. | 1 press + 8 repeats (at 400, 480, ..., 960 ms): Tone = 690 Hz (+/- one step for the tick tolerance at the edges). |
| V-09 | RX Speed 32, RX Eff. Speed 32. Edit RX Eff. Speed. Press next. | `EDIT(3,6)`, `<` shown, cell (3,24) blank. Value stays `32 WPM`. |
| V-10 | RX Eff. Speed 25. Edit RX Speed from 32 down to 24 (8 presses). Then up to 30. | After the 8th press, the same frame shows `24 WPM` in rows 2 and 3. After raising, row 2 shows `30 WPM`, row 3 still `24 WPM`. |
| V-11 | Edit Key Type (`Iambic B`). Prev. Prev. Prev. | `EDIT(6,8)`. `Iambic B`: only `<`. `Iambic A`: both markers. `Straight`: only `>`. The third prev changes nothing. |
| V-12 | Edit Show CW Code (`On`). Prev. Prev. | `On`: only `<`. `Off`: only `>`. The second prev changes nothing. |
| V-13 | Edit Backlight from 8 down to 0, then up to 10. | Shown values `7` .. `1`, `Off`, then `1` .. `10`. The fake backlight receives each level at once. `Off`: only `>`. `10`: only `<`. |
| V-14 | Edit Contrast to 0, then to 63. | The fake LCD receives each contrast value at once. Limits 0 and 63 hold. |
| V-15 | Change Tone 600 -> 610. Menu/back x3 (leave edit, leave CW, leave Settings). | Exactly one erase of page 0 and one record write, at the last menu/back only. Main menu, `SEL(4)` on `Settings`. The reloaded record gives Tone 610 Hz. |
| V-16 | Open Settings, change nothing, go back to the main menu. Then change Tone 600 -> 610 -> 600 and go back. | No EEPROM erase or write in both cases. |
| V-17 | Edit Tone 600 -> 620. Press menu/back once. | Still on `Settings > CW`, `SEL(1)`, `620 Hz`. No EEPROM access. |
| V-18 | Main menu: press menu/back. | Framebuffer unchanged, no dirty page. |
| V-19 | Main menu: enter on Receiving. Press prev, next, enter. Press menu/back. | Title `Receiving`, `TEXT(3,8,"Not ready")`, `PLAIN(1)`..`PLAIN(7)`. prev/next/enter change nothing. menu/back: main menu with `SEL(1)` on `Receiving`. Same for `Sending` (`SEL(2)` after back) and `Free Input` (`SEL(3)`). |
| V-20 | Settings: enter on Reset Statistics. Enter. | Dialog: title `Reset Statistics`, `TEXT(3,4,"Reset statistics?")`, `BUTTON(37,56)` with `No` inverted, `Yes` normal. After enter: Settings, `SEL(5)` on `Reset Statistics`. `stats_reset()` not called. |
| V-21 | Open the dialog. Next. Next. Enter. Enter. | `BUTTON(67,91)` after the first next. The second next changes nothing. First enter: `stats_reset()` called once. `TEXT(3,4,"Statistics reset")`, `BUTTON(52,71)` with `OK`. Second enter: Settings, `SEL(5)`. |
| V-22 | Open the dialog. Next. Menu/back. | Same as No: Settings, `stats_reset()` not called. |
| V-23 | Open the dialog. Next. Menu/back. Open the dialog again. | Focus is on `No`. |
| V-24 | Hold enter (P3.2 low) for 5 s in a list. Meanwhile press next. | Exactly one enter event. The next press works. |
| V-25 | P3.2 low and P0.2 low at `buttons_init()`. Release both. Press menu/back. | No event until the release. Then one menu/back event. |
| V-26 | Pin pulses low for 5 ms, high 3 ms, repeated for 15 ms, then stable low 20 ms, then a release with 5 ms bounce. | Exactly one press event, at the end of the 20 ms stable low. No second press. |
| V-27 | Valid record with Tone 1500, RX Eff. Speed 40 and RX Speed 30. | Tone shows `1000 Hz`, RX Eff. Speed shows `30 WPM`. |
| V-28 | Change Tone 600 -> 610 inside CW. Simulate power-off (re-init from the fake EEPROM) without leaving Settings. | Tone 600 Hz. |
| V-29 | Main menu with the cursor on row 1. | All pixels with y = 8 are off. `TITLE` holds. |
| V-30 | menu/back and next become stable in the same poll inside a list. | The menu/back event comes first. |
| V-31 | Hold next until it repeats. Press prev and hold. Release prev. Keep next held for 500 ms. | prev gives a press and then repeats. After prev is released, next gives no repeat events. |
| V-33 | Stored contrast 0, backlight 0. P0.2 low from `buttons_init()` for 60 ms and longer. | The fake LCD gets contrast 32 and the fake backlight gets level 8 before the first flush. The stored contrast 0 is never applied. Screen: title `Display Defaults`, `TEXT(2,2,"Default contrast and")`, `TEXT(3,2,"backlight are in use.")`, `BUTTON(52,71)` with `OK`. No EEPROM erase or write. |
| V-34 | Continue V-33: hold P0.2 for 2 s, release, then press menu/back. | No event while held. After the press: main menu, `SEL(1)` on `Receiving`. Settings > Display shows `8` and `32`. |
| V-35 | Stored contrast 0, backlight 0. P0.2 low for 30 ms after the first sample, then high. | Normal start: the fake LCD gets contrast 0, the backlight gets level 0, no message screen. |
| V-36 | Recovery start as V-33. Enter Settings, open Display, leave without an edit step, go back to the main menu. Re-init without P0.2 low. | No EEPROM erase or write. After re-init: contrast 0, backlight 0. |
| V-37 | Recovery start as V-33. Change Tone 600 -> 610, leave Settings. Re-init without P0.2 low. | One save. After re-init: Tone 610 Hz, contrast 32, backlight 8. |
| V-38 | Recovery start as V-33. In Display, change Contrast 32 -> 33 -> 32, leave Settings. | One save (an edit step happened and the values differ from the stored record). After re-init: contrast 32, backlight 8. |
| V-39 | P3.2 (enter) low at start-up for 60 ms, P0.2 high. | Normal start, stored contrast and backlight, no message screen. |
| V-32 | Render every screen in this file. | No string reaches past column 24. Every value-row label satisfies `len(label) <= 20 - fw`. Only ASCII 0x20-0x7E is used. |

### 9.1 Tone preview tests (section 6.5)

These tests use `fake_sidetone.c` (`sidetone_spec.md` section 12.2) and `fake_tick.c` with an advance of 0 per
read. They check the calls of the menu only. `sidetone_init()` from `main.c` is not part of these tests.

Probes and steps:

- `SLOG`: the ordered call log of `fake_sidetone.c` for `sidetone_set_tone_hz` and `sidetone_set_on`. An entry is
  `tone(hz)`, `on(true)` or `on(false)`.
- `ADD[...]`: since the previous check, exactly these entries came into `SLOG`, in this order. `ADD[]`: no entry.
- `AT t: X`: set the fake tick to `t` (`fake_tick_set_now`), send event `X` with `menu_handle()`, then call
  `menu_poll()` once. `AT t: poll` sends no event.
- `POLL t..u`: for each `t` to `u` in steps of 1 ms, set the fake tick and call `menu_poll()` once.
- `CWTONE(hz)`: power-on with Tone = `hz` (empty EEPROM for 600, else a valid record), open Settings > CW with the
  cursor on `Tone` in browse mode, then reset the fake sidetone log.
- Global rule for V-40 to V-53: each `on(true)` in `SLOG` comes directly after a `tone()` entry from the same event.

| ID | Setup / action | Expected result |
| --- | --- | --- |
| V-40 | `CWTONE(600)`. `AT 10000: enter`. | `ADD[tone(600), on(true)]`. Screen EDIT on Tone. |
| V-41 | Continue V-40. `POLL 10001..10299`. `AT 10300: poll`. `POLL 10301..11000`. | `ADD[]` up to 10299. `ADD[on(false)]` at 10300. `ADD[]` after it. From 10301 to 11000 `fake_tick_read_count()` does not change. Screen still EDIT, value `600 Hz`. |
| V-42 | `CWTONE(600)`. `AT 10000: enter`. `AT 10200: next`. `POLL 10201..10499`. `AT 10500: poll`. `AT 11000: prev`. `AT 11300: poll`. | `ADD[tone(610), on(true)]` at 10200. No stop at 10300. `ADD[]` up to 10499. `ADD[on(false)]` at 10500. `ADD[tone(600), on(true)]` at 11000. `ADD[on(false)]` at 11300. |
| V-43 | `CWTONE(1000)`. `AT 10000: enter`. `AT 10100: next`. `POLL 10101..10299`. `AT 10300: poll`. `AT 10500: next`. `POLL 10501..11000`. Repeat with `CWTONE(400)` and `prev`. | `ADD[tone(1000), on(true)]` at 10000. `ADD[]` at 10100 and up to 10299: the limit press does not restart the timer. `ADD[on(false)]` at 10300. `ADD[]` from 10500 to 11000: a limit press after the stop gives no sound. Same with 400 Hz and `prev`. |
| V-44 | `CWTONE(600)`. `AT 10000: enter`. `AT 10300: poll`. `AT 11000: next`. `POLL 11001..11399`. Repeat events `REPEAT(next)` at 11400 + 80k, k = 0..7, with `POLL` on every ms between them. `POLL 11961..12259`. `AT 12260: poll`. | `ADD[on(false)]` at 10300. `ADD[tone(610), on(true)]` at 11000. `ADD[on(false)]` at 11300 (the repeat delay is longer than the preview). Each repeat k: `ADD[tone(620 + 10k), on(true)]`. No `on(false)` from 11400 to 12259. `ADD[on(false)]` at 12260 (300 ms after the last repeat at 11960, value 690 Hz). |
| V-45 | `CWTONE(980)`. `AT 10000: enter`. `AT 10100: next`. Repeat events `REPEAT(next)` at 10500, 10580, 10660, 10740, 10820, 10900, with `POLL` on every ms between them. | `ADD[tone(990), on(true)]` at 10100. `ADD[tone(1000), on(true)]` at 10500. `ADD[]` for the repeats at 10580 to 10900. `ADD[on(false)]` at 10800 exactly, while the repeats still come. |
| V-46 | `CWTONE(600)`. `AT 10000: enter`. `AT 10100: enter`. `POLL 10101..10600`. | `ADD[on(false)]` at 10100. Screen LIST, `SEL(1)` on `Tone`. `ADD[]` from 10101 to 10600. |
| V-47 | `CWTONE(600)`. `AT 10000: enter`. `AT 10100: menu`. `POLL 10101..10600`. `AT 11000: enter`. `AT 11300: poll`. `AT 11500: menu`. | `ADD[on(false)]` at 10100. Still on `Settings > CW`. `ADD[]` up to 10600. `ADD[tone(600), on(true)]` at 11000. `ADD[on(false)]` at 11300. `ADD[on(false)]` at 11500: one call on leaving, also after the timeout. |
| V-48 | `CWTONE(600)`. `AT 10000: enter`. At 10100 call `menu_show_message(MENU_MESSAGE_NOT_SAVED)`, then `menu_poll()`. `POLL 10101..10600`. Close the message with `enter`. | `ADD[on(false)]` at 10100. `ADD[]` up to 10600 and after the close. |
| V-49 | `CWTONE(600)`. `AT 10000: enter`. `AT 10100: next`. `AT 10200: enter`. `AT 11000: enter`. | `ADD[tone(610), on(true)]` at 10100. `ADD[on(false)]` at 10200. `ADD[tone(610), on(true)]` at 11000: a new edit plays the stored current value. |
| V-50 | `CWTONE(600)`. `AT 65400: enter`. `POLL 65401..65535`, then `POLL 0..163`. `AT 164: poll`. | `ADD[tone(600), on(true)]` at 65400. `ADD[]` up to 163. `ADD[on(false)]` at 164. |
| V-51 | `CWTONE(600)`. `AT 10000: enter`. At 10100: `menu_handle(next)`, `menu_handle(REPEAT(next))`, then one `menu_poll()`. `POLL 10101..10399`. `AT 10400: poll`. | `ADD[tone(610), on(true), tone(620), on(true)]` at 10100. `ADD[]` up to 10399. `ADD[on(false)]` at 10400. |
| V-52 | Power-on, empty EEPROM, reset the fake sidetone log. Call `menu_poll()` after each event. Do a `POLL` of 500 ms after each step. Steps: 1. Move in the main menu with presses and repeats. 2. Open each placeholder and press all four buttons there. 3. Open Settings. Move the cursor onto `Tone` and past it in browse mode. 4. Edit each value and choice item except Tone up to both limits, with presses and repeats. 5. Open the Reset Statistics dialog and answer `No`. Open it again and answer `Yes`. Close the message. 6. Change a value and leave Settings with a save. | `SLOG` is empty at the end. `fake_tick_read_count()` does not change during the `POLL` steps. |
| V-53 | Recovery start as V-33, then reset the fake sidetone log. Release P0.2, close the message with `menu/back`. `POLL` for 1000 ms. | `SLOG` is empty. |

## 10. Durable decisions

| ID | Decision | Reason | Rejected alternatives |
| --- | --- | --- | --- |
| UX-1 | 8 x 25 grid, x origin 2, 2 px left margin and 2 px right margin (glyph column + x 127). | Symmetric margins. Probes at x 0, 1, 127 never hit text. | x origin 0 (text touches the glass edge in a highlight). |
| UX-2 | Inverted title bar 8 rows. Highlight band 7 rows (`8r+1`..`8r+7`) with an off gap line at `8r` across the width of the band. | Keeps the title bar and a selected row 1 apart while keeping 7 visible items and page-aligned rows. | Full 8-row highlight: merges with the title bar. 10-row title with 6 items: breaks page alignment, loses a row. Title in normal video: weak separation. Clipping cell row 0: cuts `l`, `t`, `h`. |
| UX-3 | Labels and value formats in section 5.3, fixed field width per item. `RX`/`TX` labels decided by the user. | Fits 25 columns with edit markers. Value does not jump while it changes. | Full words `Receive`/`Send` (do not fit with markers). `Recv`/`Send` short forms. |
| UX-4 | Edit mode = highlight shrinks to the value field, `<`/`>` markers show the possible directions. | Two cues (shape and markers) for the mode. Markers also show the limits. | Blinking value (needs a timer, weak on slow LCD). `*` marker only (too small). |
| UX-5 | Scroll window of 7, minimum movement, `i/n` in the title bar. | No extra pixels. Testable as text. | Scroll bar (needs a column). |
| UX-6 | Placeholder: title + `Not ready`, only menu/back works. | Plan. | - |
| UX-7 | Dialog `No`/`Yes`, focus `No` at every open, menu/back = `No`. | Plan. Also protects against a spurious `enter` from the paddle socket. | - |
| UX-8 | `Statistics reset` / `OK` message after `Yes` (accepted by the user). | Gives a completion signal for an action that cannot be undone. | Silent return (the user cannot tell `Yes` from `No`). |
| UX-9 | Control mapping in section 6.1. Menu/back at the root does nothing. | One rule for back everywhere. No hidden function at the root. | menu/back at the root toggles something (hidden mode). |
| UX-10 | Title Case for labels and titles, sentence case for sentences. | Matches the names the user gave to the items. | - |
| UX-11 | ASCII 0x20-0x7E only. | Avoids `char` signedness in the glyph search. | Latin-1 arrows such as `»`. |
| UX-12 | Debounce 20 ms stable. Repeat 400 ms / 80 ms on prev and next only. Held-at-start buttons ignored. | Plan values. One event per press for enter keeps a held key harmless. | Edge-triggered press with lockout (more sensitive to plug-insertion glitches). |
| UX-13 | The list cursor wraps on press only. Values never wrap. | Fast reach of the last item without spinning on a held button. No jump from 50 WPM to 5 WPM. | Wrap on repeat. Wrapping choices. |
| UX-14 | No extra message for the effective-speed clamp. | The base value is visible on the same screen. The marker already shows the limit. | A hint text (no space in the row). |
| UX-15 | Silent save on leaving Settings. Condition: at least one edit step in the visit, and the values differ from the stored record. | The stall is about 6 ms. A message only slows the user. The edit-step condition keeps a display recovery start temporary until the user edits. | `Saving...` / `Saved` text. Compare-only rule (saves the recovery defaults without an edit). |
| UX-16 | Response target 100 ms. No lost steps during repeat. | The user counts steps by the value, not by frames. | - |
| UX-17 | Silent defaults on an empty or corrupted record. | See section 8. | One-time message. |
| UX-18 | No driver change for the font y offset. `gfx` draws at the row top minus the font y offset. ASCII only. | User decision. Keeps `firmware/drivers/` unchanged. | Driver arithmetic fix. |
| UX-19 | Display recovery start: `menu/back` held for 50 ms from the first sample at start-up gives default contrast and backlight for the session, with a message screen. | User decision (Q-1). Without it, a bad contrast or backlight save locks the user out. | No recovery. Limiting the contrast range (the plan fixes 0-63). Recovery without a message (the user cannot tell the gesture worked). |
| UX-20 | Tone preview (section 6.5): the tone plays at the start of a Tone edit and after each change. A step at a limit plays nothing new. Leaving the edit by any path stops it at once. No other sound in the menu. | User decision (Q-3). The user hears the value that the display shows. The stop on leave ties the sound to the edit: `enter` means "done". Silence at a limit keeps UX-13. | A sound for the Tone row in browse mode (noise during a cursor move). Tone continues after leaving until the timeout (sound without a visible owner). A beep at a limit (a second limit signal next to the marker, and noise during a held repeat). A beep on save or on other items (no user request). |
| UX-21 | `MENU_TONE_PREVIEW_MS` = 300 ms, counted from the hook that starts or restarts the tone. | 300 ms is one dah at 12 WPM, a familiar length for the user. It is long enough to judge the pitch after the 8 ms rise. Single presses about 0.5 s apart give one separate beep each. It is shorter than the 400 ms repeat delay, so a hold gives one beep, a short silence and then the chain. | 150 ms (short for a low tone, and the rise and fall take 16 ms of it). 1000 ms or continuous while in edit (tiring in long sessions, and the user cannot stop it without leaving the edit). |
| UX-22 | A repeat event uses the same rule as a press. The retune dip (about 17-19 ms each 80 ms) during a held repeat is accepted. | The user hears the pitch move and can release by ear. The dip is shaped, not a click. No interface change. | Silence during repeats and one tone after the release. There is no release event. It needs a buttons change, or a new hook where `menu_poll()` starts the tone after a pause in the repeats. Sound on press events only (the user cannot hear the final value of a hold without one more step). Sound on every n-th repeat only (the sound lags the display). Glide without silence (needs an ARR change during a ramp, a sidetone interface change). Each alternative needs an `architector` decision. |

## 11. Device checks (unverified until the user tests on the device)

| ID | Check | Pass condition |
| --- | --- | --- |
| D-1 | Legibility of the font, the highlight, the gap line, `<` and `>`. Conditions: contrast 32, backlight 8 and 0, 30-60 cm, indoor light, straight and at an angle. | All labels and values readable. The gap line between the title bar and a selected row 1 is visible. |
| D-2 | Contrast range 0-63. | Record the lowest and highest readable values and whether 0 or 63 make the screen blank or black. |
| D-3 | Backlight levels 0-10. | No visible flicker. Levels are distinguishable. The screen is readable in room light with level 0. |
| D-4 | Bounce time of the four buttons and of the paddle tip. | All bounce trains are shorter than 20 ms. |
| D-5 | Plug insertion into the paddle socket while the menu shows. | Record whether a spurious `enter` occurs. |
| D-6 | Time from a button press to the change on the glass. Full-screen redraw time. Behaviour during auto-repeat. | Within 100 ms. Values step at 12.5 per second. |
| D-7 | Save on leaving Settings. | No visible glitch. A button pressed during the save is not lost. |
| D-8 | Physical order of prev and next on the case. | prev is up/left. Otherwise review sections 5.4, 5.7 and 6.1. |
| D-9 | Display recovery start: set contrast 0 and backlight 0, save, switch off. Hold `menu/back` and switch on. | The `Display Defaults` message is readable. The `on/off` button powers the MCU up from off. |
| D-10 | Tone preview at the edit start. Speaker and headphones. Tone 400, 600 and 1000 Hz. Volume at its middle. | The tone starts with no audible delay after `enter`. It lasts about 0.3 s. No click at the start or at the end. The user can judge the pitch. |
| D-11 | Single presses about 0.5 s apart from 600 Hz up and down. | Each press gives one clear beep at the new pitch. |
| D-12 | Hold `next` from 400 Hz to 1000 Hz, then hold `prev` back. | The chain sounds as a series of pitch steps, not as clicks or a buzz. The user can release near a target pitch. The tone ends about 0.3 s after the release, and at the limit also while the button is held. If the chain is unpleasant, record it for `architector` (UX-22 alternatives). |
| D-13 | Stop timing on a logic analyzer (P1.1 or P1.2) while the screen redraws. Leave with `enter` and with `menu/back`. Let a preview time out. | After a leave, the output is quiet within about 10 ms of the event. A timeout ends 300 ms after the hook, plus at most one main-loop pass, plus the 8 ms fall. |
| D-14 | Insert and remove the headphone plug during a single preview and during a held repeat. | The route changes at the next silence. No click on either output. A spurious `enter` from the socket (D-5) ends the edit and stops the tone. |

## 12. Open questions

Resolved by the user: Q-1 (UX-19), Q-2 (UX-3), Q-3 (a Tone edit plays the tone, see UX-20 to UX-22 and section 6.5),
Q-6 (UX-18), the `Statistics reset` screen (UX-8).
Resolved by `architector`: Q-4 (`eeprom_write` reads back each byte, and a failed save shows `Not saved` / `OK`, see
F-7 and `menu_subsystem_spec.md` sections 3.5 and 3.6).

| ID | Question | Impact | Owner |
| --- | --- | --- | --- |
| Q-5 | The datasheet advises against EEPROM writes at low supply voltage. The board has no documented battery-voltage input. | A save at a low battery can corrupt the record (then F-2 applies). | `architector` |
