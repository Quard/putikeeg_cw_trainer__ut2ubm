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
  Reset Statistics dialog, button input, and the live effect of contrast and backlight.
- Out of scope in this version: the Receiving, Sending and Free Input modes, sidetone and paddle keying, statistics.
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
| C-7 | Volume potentiometer | analog | Not used by the menu. | |

Semantic assumption (unverified, D-8 in section 11): `prev` means up, left and decrease. `next` means down, right
and increase. If the buttons on the case are in a different order, this file needs a review.

### 3.2 Outputs

| ID | Output | Budget | Notes |
| --- | --- | --- | --- |
| O-1 | LCD, 128 x 64, monochrome, probably ST7567 | 8 text rows x 25 columns with the 5 x 8 font | Contrast 0-63 (`lcd_set_contrast`), default 0x20 = 32. The full-screen transfer time is not measured yet. |
| O-2 | LCD backlight, HX4004A on P0.3 | Levels 0-10, duty = level x 10 % (plan) | Perceived brightness per level is unverified. |
| O-3 | Sidetone | not available in this version | No audible feedback in the menu. |

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
  values change only in memory until the save. They have no audible or visible effect in this version.
- Effective-speed rule (plan): an effective speed can never be higher than its base speed. Lowering a base speed
  below its effective speed lowers the effective speed in the same step. Raising a base speed never changes the
  effective speed.
- A list opened from its parent always starts with the cursor on its first item. Going back restores the cursor of
  the parent on the item that was opened.
- After power-on the main menu shows with the cursor on `Receiving`. A display recovery start shows the
  `Display Defaults` message first (section 8.1).
- There is no inactivity timeout anywhere in this version. There is no long-press function.

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
  shows the limit (section 5.4). No message, no sound.

### 6.4 Shared enter and paddle

- In the menu, a paddle dit contact or a straight key acts as `enter`. This is a hardware fact and the menu accepts
  it.
- A key held closed gives one `enter` event only. The other three buttons keep working.
- If inserting a plug causes a short tip contact longer than 20 ms, it gives one `enter` (unverified, D-5). The
  dialog focus starts on `No`, so such an event never resets statistics. In a list it can open an item or enter
  edit mode. `menu/back` undoes that without a change.

## 7. Feedback

| Situation | What the user sees | Decision |
| --- | --- | --- |
| Cursor moves | The highlight band moves to the new row. | UX-2 |
| Enter edit mode | The highlight shrinks to the value field. `<`/`>` appear. | UX-4 |
| Value at its min/max | The `<` or `>` marker disappears. Further presses change nothing. | UX-13 |
| Effective speed at its base speed | `>` disappears on the effective-speed row, the same as a hard maximum. The base value is in the row above on the same screen. | UX-14 |
| Base speed lowered below the effective speed | The effective-speed row changes in the same redraw as the base row. Both rows are on the CW screen. | UX-14 |
| Contrast or backlight edit | The LCD contrast or backlight changes at each step. | plan |
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
| F-7 | EEPROM write fails | Not detectable with the planned API. | - | See question Q-4. |

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

## 12. Open questions

Resolved by the user: Q-1 (UX-19), Q-2 (UX-3), Q-6 (UX-18), the `Statistics reset` screen (UX-8).

| ID | Question | Impact | Owner |
| --- | --- | --- | --- |
| Q-3 | Tone preview: when the sidetone exists, does a Tone edit play the tone? | Later feature. | user |
| Q-4 | Save failure: the planned EEPROM API does not read back a write. If `architector` adds a read-back test, the failure uses the message screen (title `Settings`, text `Not saved`, `OK`). | Without a check, a failed write is silent until the next power-on. | `architector` |
| Q-5 | The datasheet advises against EEPROM writes at low supply voltage. The board has no documented battery-voltage input. | A save at a low battery can corrupt the record (then F-2 applies). | `architector` |
