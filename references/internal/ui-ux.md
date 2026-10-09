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
- UI scope in this version:
  - The menu tree, the Settings editor, the `Paddle` setting and the `Prosigns` setting (section 5.10).
  - The Koch training modes Receiving and Sending (sections 5.11 to 5.15, 6.8 and 6.9): the header, the group
    keyboard, the result feedback, the transitions between items and the session end screen.
  - Level selection in Receiving and Sending (section 5.16): the start screen, the level selector on the start screen
    and on the session end screen, and the earned level in the main menu.
  - The Free Input mode (section 5.9) and the keyer for the three key types (section 6.7).
  - Long presses (section 6.6) and the other button input.
  - The Reset Statistics dialog. It sets the training level of every mode and speed back to level 1.
  - The live effect of contrast and backlight, and the Tone preview (section 6.5).
  - The startup screen at power-on (section 5.17) and the `About` screen of the main menu (section 5.18), both with
    the firmware version.
- Out of scope in this version: statistics other than the training levels, any sound other than the Tone preview,
  the keyer sidetone (Free Input and Sending) and the CW playback in Receiving. A level above the earned level
  cannot be selected (section 5.16).
- Users: amateur radio operators who learn or practise Morse code. They know the terms WPM, RX, TX, Iambic A/B and
  Farnsworth (effective) speed.
- Environment: a handheld device or a device on a desk, indoor light, viewing distance about 30-60 cm
  (unverified). Long practice sessions.
- Frequent tasks:
  - Most frequent, long practice: a Receiving or Sending session of 10 to 100 items, many sessions in a row.
  - Frequent: key text in Free Input and read the decoded result. Start a mode from the main menu.
  - Sometimes: change a speed, the key type or the tone.
  - Rare: change contrast or backlight. Reset statistics.

## 2. Sources

| Source | What it gives |
| --- | --- |
| `references/internal/components_and_wiring.md` | Buttons, pins, display, paddle socket, backlight pin. |
| `firmware/inc/u8g2_fonts.h` (`u8g2_font_spleen5x8_mf`) | Font header bytes and glyph bitmaps (decoded, see section 3.3). |
| `firmware/drivers/u8g2_font_render/u8g2_font_render.c` | How the driver places a glyph relative to the `y` argument. |
| `firmware/inc/lcd/lcd_fb.h`, `firmware/inc/board/lcd_config.h` | 128 x 64 framebuffer, 8 pages of 8 rows, contrast range 0-63, default 0x20. |
| `references/context/STC8H_datasheet.md` | EEPROM sector erase about 4-6 ms, byte program about 30-40 us. Avoid EEPROM writes at low voltage. |
| `references/internal/sidetone_spec.md` (owner `architector`) | Sidetone API, 8 ms rise and fall, retune through silence, automatic route, menu hooks H1-H6 (section 10.3). |
| `references/internal/keyer_spec.md` (owner `architector`) | Keyer contract: filter, entry mask, state machine, events, timing formulas, Morse table, long-press events, P3.2 ownership. Section 17: the prosign display setting (decoder codes, `morse_format()`, flags bits 2-3, record layout version 2). It refers to sections 5.9, 5.10, 6.6 and 6.7 of this file. |
| `references/internal/paddle_keyer_research.md`, `paddle_keyer_howtoimplement_research.md` | Iambic A/B behaviour, squeeze release, entry mask, straight-key mode. |
| Plan "Koch training modes Receiving and Sending" (approved by the user, 2026-10) | User decisions for the training modes: session sizes, pass rule, level model, Koch order, word length, attempts, Receiving input, progress key, button order on the case. |
| Plan "Firmware version, startup screen and About item" (approved by the user, 2026-10-08) | User decisions: the startup screen mockup and its time. The plan gave 2 s. A later user decision (2026-10-08) changed it to 5 s. Any button skips it. The `About` item after `Settings` and its text rows. The version `0.1.0` comes from the build macro `FIRMWARE_VERSION` (file `firmware/VERSION`). |
| `references/internal/training_spec.md` (owner `architector`) | Interface contract of the training modes: Koch tables, level model, item generation, player, progress record, session engine. This file gives the visible and audible behaviour. If the two differ, report it to `architector`. |
| `firmware/inc/buttons/buttons.h` | Button events: press, repeat (`prev`, `next`), long (`menu/back`, `prev`, `next`, and `enter` in tap mode only, `training_spec.md` D-T28). There is no release event. In tap mode, the press event comes at the release. |
| `firmware/inc/cw/keyer.h`, `firmware/inc/cw/morse.h` | Keyer events (DIT and DAH at the end of a mark, CHAR_GAP, WORD_GAP, OVERFLOW). Prosign codes and `morse_format()` texts. |

## 3. Inventories

### 3.1 Controls (fact, unless marked)

| ID | Control | Pin | Behaviour | Notes |
| --- | --- | --- | --- | --- |
| C-1 | `next` button | P0.0 | Active low, no external pull-up. | Third of the four buttons from the left, between `prev` and `enter` (user fact 2026-10-06, below). |
| C-2 | `prev` button | P0.1 | Active low, no external pull-up. | Second of the four buttons from the left (user fact, below). |
| C-3 | `menu/back` button | P0.2 | Active low, no external pull-up. | Left button of the four (user fact, below). |
| C-4 | `enter` button | P3.2 | Active low. Shared with the tip of the paddle/key socket. | In the menu and in Receiving, a paddle dit contact or a straight key also presses `enter`. In Free Input and in the input states of Sending, the keyer owns the pin. The `enter` button then keys like the tip contact (sections 6.7 and 5.13). Inserting a plug can short the tip for a short time (unverified). Right button of the four, right of `next` (user fact 2026-10-06, below). In Receiving, a tap of `enter` or a short tip closure selects the right run, and a hold of 800 ms replays (section 6.8). |
| C-5 | Paddle ring | P5.5 | Dah contact (with `Paddle` = `Normal`). Active low, internal pull-up (plan). | The menu ignores it. Only the keyer in Free Input reads it. A mono plug shorts the ring to the sleeve (research Step 3). |
| C-6 | `on/off` button | none | Not read by the MCU (no pin in the wiring table). | Unverified: the effect of the button on the MCU power is not known. |
| C-7 | Volume potentiometer | analog | Not read by the MCU. Sets the sidetone loudness in the analog path (`components_and_wiring.md`). | The firmware has no volume control. At the lowest setting the Tone preview can be silent. |

Button order (user facts 2026-10 and 2026-10-06, closes D-8): from left to right on the case, the buttons are
`menu/back`, `prev`, `next`, `enter`. So `prev` is left of `next`, and `enter` is right of `next`. The meaning
`prev` = up, left and decrease, and `next` = down, right and increase, agrees with the case. The four group labels of
the Receiving keyboard follow this order: one label above each button (section 5.12.1).

### 3.2 Outputs

| ID | Output | Budget | Notes |
| --- | --- | --- | --- |
| O-1 | LCD, 128 x 64, monochrome, probably ST7567 | 8 text rows x 25 columns with the 5 x 8 font | Contrast 0-63 (`lcd_set_contrast`), default 0x20 = 32. The full-screen transfer time is not measured yet. |
| O-2 | LCD backlight, Q6 switch driven from P0.3 (HIGH = on; HX4004A supply, Vout unverified) | Levels 0-10, duty = level x 10 % (plan) | Perceived brightness per level is unverified. |
| O-3 | Sidetone, PWM on P1.1 (speaker) or P1.2 (headphones) | One tone at a time, 400-1000 Hz. Rise 8 ms, fall 8 ms. A tone change while sounding falls to silence, waits for the carrier update (at most 3 ms) and rises again: a dip of about 17-19 ms. | Uses: the Tone preview (section 6.5), the keyer sidetone in Free Input and Sending (section 6.7), the CW playback in Receiving (section 5.12). The route is automatic. With the plug in (P1.0 LOW), the headphones play. Otherwise the speaker plays. A plug change takes effect at the next silence. Loudness comes from C-7. Facts from `sidetone_spec.md` sections 2, 6.3, 10.3. The sound at the ear is unverified (D-10 to D-14). |

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

#### 3.3.1 Large font `u8g2_font_spleen8x16_mf` (`FONTS_ID_8X16`)

Decoded with `firmware/Tests/tools/gen_font_golden.py`, which uses the arithmetic of the C decoder
(`firmware/src/gfx/font_u8g2.c`: the glyph top is the bbox height plus the bbox y offset, minus the glyph height,
minus the glyph y offset). Facts of
the bitmap. Their look on the glass is unverified until D-26.

| Field | Value |
| --- | --- |
| Glyph count | 192 |
| Bounding box mode | 2 (monospace), pitch 8 for every glyph 0x20-0x7E |
| Bounding box | 8 x 15 (not 16), x offset 0, y offset -3. Rows from the cell top to the baseline: 12 |
| Ascent of `A`, descent of `g` | 10, -3 |

A glyph drawn with `gfx_draw_text()` at the cell top `y` uses these cell rows (`y + row`):

| Cell rows | Glyphs (0x21-0x7E) |
| --- | --- |
| 2-11 | Capitals except `Q`, digits, `! # % & < > ?` |
| 2-13 | `Q` (the tail ends in row 13) |
| 1-12 | `( ) / [ \ ] { } $ \|` |
| 5-9 | `+` |
| 5-8 | `=` |
| 7 | `-` |
| 10-11 | `.` |
| 10-12 | `,` |
| 14 | `_` |

- Cell rows 0 and 15 are blank for every glyph. Rows 12-15 are blank for every glyph of the Koch set except `Q`
  (rows 12-13) and `( ) / ,` (row 12).
- Ink is in cell columns 0-6. Column 7 is blank, except for `*` and `T`.
- A line of 16 cells fills the 128 px width with no margin. The training screens use at most 15 cells per line, so
  the margin columns x 0, 1 and 127 stay off (section 5.11).

## 4. Global display conventions

### 4.1 Grid (decision UX-1)

- Text cell `(r, c)`, row `r` = 0..7, column `c` = 0..24: pixels `x = 2 + 5c` to `6 + 5c`, `y = 8r` to `8r + 7`.
- Margins: x 0-1 on the left, x 127 on the right (the blank glyph column at x 126 adds to it). Text never uses
  x 0, 1 or 127. Tests use these margin columns as font-independent probes.
- Row 0 is the title bar. Rows 1-7 are content. Every text row and its highlight stay inside one LCD page, so a
  cursor move dirties two pages and a value change dirties one page.
- Exception: the item line of the training screens uses the 8 x 16 font in pixel positions, not on this grid. It
  fills pages 3 and 4 (section 5.11.2).
- Exception: the startup screen has no title bar, and its 8 x 16 line `CW Trainer` is in pixel positions. It fills
  pages 2 and 3 (section 5.17).
- Text never wraps. A string that is too long is cut at column 24. All strings in this file fit. A cut is a defect.
  The only exception is the decoded text in Free Input, which wraps per character (section 5.9).

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
7  |Paddle           Normal  |
```

- Label in column 0.
- Value item: value right-aligned, last character in column 22.
- Submenu item: `>` in column 24, no value.
- Mode item: `Receiving` and `Sending` show the earned level `L<n>`, right-aligned, last character in column 22
  (section 5.16.6, UX-57). `Free Input`, `About` and the action item: label only, no marker. The `>` marker means
  "opens a list" (UX-61).
- Selected row: highlight band over x 0-127. The label, value and marker of that row are inverted.
- Rows below the last item are blank (all pixels off).

### 5.3 Labels, values and field widths (decision UX-3)

Each value item has a fixed field width `fw` = the length of its longest value text. The field does not move while
the value changes.

| Level | Label | Kind | Values shown | fw | Default (plan) |
| --- | --- | --- | --- | --- | --- |
| Main | `Receiving` | mode, read-only level | `L1` .. `L65535`: the earned level at `RX Speed` (section 5.16.6) | 6 | `L1` |
| Main | `Sending` | mode, read-only level | `L1` .. `L65535`: the earned level at `TX Speed` (section 5.16.6) | 6 | `L1` |
| Main | `Free Input` | mode | - | - | - |
| Main | `Settings` | submenu | - | - | - |
| Main | `About` | info screen (section 5.18) | - | - | - |
| Settings | `CW` | submenu | - | - | - |
| Settings | `Show CW Code` | choice | `Off`, `On` | 3 | `On` |
| Settings | `Prosigns` | choice | `Punctuation`, `Prosign`, `Both` | 11 | `Both` (user) |
| Settings | `Max Word Length` | value | `4` .. `8`, step 1 | 1 | `5` |
| Settings | `Display` | submenu | - | - | - |
| Settings | `Reset Statistics` | action | - | - | - |
| CW | `Tone` | value | `400 Hz` .. `1000 Hz`, step 10 | 7 | `600 Hz` |
| CW | `RX Speed` | value | `5 WPM` .. `50 WPM`, step 1 | 6 | `32 WPM` |
| CW | `RX Eff. Speed` | value | `5 WPM` .. RX Speed, step 1 | 6 | = RX Speed |
| CW | `TX Speed` | value | `5 WPM` .. `50 WPM`, step 1 | 6 | `20 WPM` |
| CW | `TX Eff. Speed` | value | `5 WPM` .. TX Speed, step 1 | 6 | = TX Speed |
| CW | `Key Type` | choice | `Straight`, `Iambic A`, `Iambic B` | 8 | `Iambic B` |
| CW | `Paddle` | choice | `Normal`, `Reversed` | 8 | `Normal` |
| Display | `Backlight` | value | `Off` (level 0), `1` .. `10` | 3 | `8` |
| Display | `Contrast` | value | `0` .. `63`, step 1 | 2 | `32` |

Order of the items in each list is the order in this table. Choice order (prev = towards the first entry, next =
towards the last entry) is the order in the "Values shown" column.

Fit rule: `len(label) <= 20 - fw`. This keeps one blank column between the label and the `<` edit marker. Every
row above passes. The main menu levels have no edit mode, but they pass too: `Receiving` uses 9 of 14 allowed. The tightest rows are `RX Eff. Speed` and `TX Eff. Speed` (13 of 14 allowed) and `Prosigns`
(8 of 9 allowed). `Paddle` uses 6 of 12 allowed.

`Punctuation` (11 characters) is the longest value or choice text of the tree. The menu value text buffer holds it
and the NUL (12 bytes, `keyer_spec.md` decision D-P7). A longer value text in a later change needs a larger buffer
first.

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
7  |Paddle           Normal  |
```

- The highlight moves from the whole row to the value field. The label goes back to normal video. This change of
  the highlight shape is the edit-mode marker.
- Field layout: value in columns `23 - fw` to 22 (right-aligned inside the field), `<` in column `21 - fw`,
  column `22 - fw` blank, column 23 blank, `>` in column 24.
- Highlight band: x from `106 - 5 * fw` to 127 (one pixel left of the `<` cell), y `8r + 1` to `8r + 7`.
  Examples (fw -> x): 1 -> 101, 2 -> 96, 3 -> 91, 6 -> 76, 7 -> 71, 8 -> 66, 11 -> 51.
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

No list in this version has more than 7 items (main 5, Settings 6, CW 7, Display 2), so no list scrolls now. CW
uses all 7 rows. Settings uses 6 rows, and row 7 stays blank. The main menu uses 5 rows, and rows 6 and 7 stay
blank. An eighth CW item or an eighth Settings item needs this scroll rule.
The rule applies to later lists:

- The window shows 7 items. Minimum movement: the window moves only by the rows that keep the cursor inside it.
- After a wrap (section 6.3) the window shows the first 7 or the last 7 items.
- The title bar shows `i/n` (section 5.1). There is no scroll bar.

### 5.6 Placeholder screen (decision plan + UX-6, UX-31)

No menu item opens a placeholder in this version: `Receiving` and `Sending` open the training modes (sections 5.11
to 5.15). The layout and rules below stay as the pattern for a later mode that is not ready.

```
0  |Receiving                | title bar, inverted
3  |        Not ready        | column 8
```

- Title: `Receiving` or `Sending`. Body: `Not ready` in row 3, column 8. All other rows blank.
- No highlight on the screen.
- Only a long `menu/back` works (section 6.1). A short press does nothing. The keyer is off: the tip acts as
  `enter`, and `enter` does nothing here.

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

If the reset is not stored (`stats_reset()` returns false, `training_spec.md` D-T11 and U-17), `Yes` in the
dialog shows the `Not saved` message of F-7:

```
0  |Settings                 | title bar, inverted
3  |        Not saved        | column 8
5  |           OK            | columns 11-12, highlight x 52-71 (cells 10-13), always focused
```

`enter` or `menu/back` closes it to the list that is open below it. After the reset, that is Settings with the
cursor on `Reset Statistics`. After a failed settings save (F-7), it is the main menu with the cursor on `Settings`.

### 5.9 Free Input screen (decision UX-25 to UX-30, UX-38)

Purpose: the user keys with the paddle or a straight key, hears the sidetone and reads the decoded text. The screen
shows only the text. It has no cursor, no status and no hint.

Keyed text: `CQ CQ DE UT2UBM UT2UBM K`, a delimiter, then `UR 5NN BT NAME VADIM BT QTH KY*V AR 73 SK`. `Prosigns` =
`Both` (the default):

```
    0         1         2
    0123456789012345678901234
0  |Free Input               | title bar, inverted (section 5.1)
1  |CQ CQ DE UT2UBM UT2UBM K | text, normal video
2  |-------------------------| delimiter: one 1 px line, y = 8r + 3, x 2-126 (not '-' glyphs)
3  |UR 5NN <BT/=> NAME VADIM | '<BT/=>' = prosign BT (Both). The word space after VADIM leaves column 24 blank
4  |<BT/=> QTH KY*V <AR/+> 73| '*' = unknown pattern, '<AR/+>' = AR. A full line: the next character wraps
5  |<SK>                     | '<SK>' = prosign SK (the same text for every value)
6  |                         |
7  |                         |
```

The same keyed text with the two other values (rows 3-5 only):

```
    0123456789012345678901234
3  |UR 5NN = NAME VADIM = QTH|   Punctuation
4  |KY*V + 73 <SK>           |

3  |UR 5NN <BT> NAME VADIM <B|   Prosign: the second <BT> splits at the line end
4  |T> QTH KY*V <AR> 73 <SK> |
```

The mockups are illustrative. Legibility on the glass is device check D-21. Legibility of the prosign texts is
device check D-25.

#### Layout

- Font: `FONTS_ID_5X8`, the UI font. The grid of section 4.1 applies: cell `(r, c)` is at `x = 2 + 5c`, `y = 8r`.
- Row 0: the title bar with `Free Input` (section 5.1). It does not change while the mode is open.
- Rows 1-7: the text area, 7 lines of 25 cells, normal video. No highlight, no cursor mark, no inverted cell.
- Letters are capitals. The decoder gives printable ASCII or the code of a prosign. The screen shows printable ASCII
  only (UX-11).
- The firmware draws each character straight into the framebuffer. There is no text buffer. After the mode ends,
  the text is gone.

#### Cursor

The cursor is the cell where the next character goes. It is not drawn. Its row is 1-7 and its column is 0-25.
Column 25 means "line full": the line has no free cell, but the next line does not exist yet.

Invariant: every cell at the cursor and after it, in the text area, is blank.

| Event | Rule |
| --- | --- |
| Mode start, clear | Cursor at row 1, column 0. |
| Character | If the column is 25, first go to a new line. Then draw the character at the cursor and add 1 to the column. |
| Word space | If the column is 0 or 25, do nothing. Else add 1 to the column. The cell stays blank. |
| New line | If the row is below 7, add 1 to the row. Else scroll. The column becomes 0. |
| Scroll | Move the content of rows 2-7 up to rows 1-6. Clear row 7. The cursor stays on row 7. The title bar does not move. |

Results of these rules:

- Wrap is per character. A word can break across two lines, for example `VAD` at the end of one line and `IM` at
  the start of the next. There is no word wrap.
- The wrap is lazy. A full line stays the last line until the next character comes. So the screen never scrolls to
  show an empty line, and a full row 7 stays on the screen while the user pauses.
- A line never starts with a space. A space at the end of a full line is dropped. The line break then acts as the
  word space.
- Two word spaces in a row cannot occur: the keyer gives one word-gap event per pause (`keyer_spec.md`).
- A text of more than one cell (`<SK>`, and the prosign texts of `Prosign` and `Both`, up to 6 cells) has no
  special rule. Each of its cells wraps like other text. Example: with the cursor in column 23, `<S` goes to
  columns 23-24 and `K>` starts the next line. A split in row 7 scrolls in the middle of the text. More examples:
  "Prosign texts" below.
- The text area keeps the last 7 lines, at most 175 cells.

#### Characters

When the character gap comes (section 6.7), the decoder turns the elements of one character into one text. Most
texts are one glyph. A prosign text has 1 to 6 cells (`keyer_spec.md` section 17.4.1).

| Pattern | Display | Decision |
| --- | --- | --- |
| A pattern in the Morse table | Its character: `A` to `Z`, `0` to `9` and the punctuation of the table. | plan |
| AR `.-.-.`, BT `-...-`, KN `-.--.`, AS `.-...` | The text that the `Prosigns` setting selects. See the next table. | UX-38, user |
| SK `...-.-` | `<SK>` for every value of `Prosigns`: four cells, the angle brackets included, no space inside. Each of the four characters follows the character rule, so `<SK>` can split across two lines like any word. | UX-28, user |
| A pattern that is not in the table, more than 6 elements, or a lost event | `*` (one cell) | UX-27, user |
| The 8-dit error sign | `*` | user |
| A character gap with no elements (for example after a clear) | Nothing. The cursor does not move. | UX-30 |

Prosign texts per value of `Prosigns` (decision UX-38, user). The four prosigns have the code of a punctuation mark:

| Prosign | Code | `Punctuation` | `Prosign` | `Both` (default) |
| --- | --- | --- | --- | --- |
| AR | `.-.-.` | `+` | `<AR>` | `<AR/+>` |
| BT | `-...-` | `=` | `<BT>` | `<BT/=>` |
| KN | `-.--.` | `(` | `<KN>` | `<KN/(>` |
| AS | `.-...` | `&` | `<AS>` | `<AS/&>` |
| SK | `...-.-` | `<SK>` | `<SK>` | `<SK>` |

- `Punctuation`: one cell, the mark only.
- `Prosign`: four cells, the name in angle brackets, as `<SK>`.
- `Both`: six cells: `<`, the name, `/`, the mark, `>`. No space inside.
- No other prosign has a text. A pattern that is not in the Morse table gives `*`.

Rules for the Morse table and the text (`keyer_spec.md` sections 6.10 and 17, owner `architector`):

- The decoder gives at least `A` to `Z`, `0` to `9` and `.` `,` `?` `/` `-`. It also gives the codes of AR and BT.
  With `Punctuation` and `Both`, AR and BT show the marks `+` and `=`. With `Prosign`, `+`, `=`, `(` and `&` never
  show, because no other code gives them. Other punctuation is the choice of `architector`.
- The decoder never gives a lowercase letter, a space, `*`, `<` or `>`. The glyph `*` means "unknown" and nothing
  else.
- In Free Input, `<` and `>` appear only as the brackets of a prosign text: `<SK>` in every value, and the texts of
  AR, BT, KN and AS in `Prosign` and `Both`. So a `<` always opens a prosign text and a `>` always closes it.
- `*` has no Morse code in the ITU table, so it never looks like a correct character. `*` uses cell columns 0-3
  and cell rows 1-6 only (section 3.3), so it does not touch its neighbours.

#### Prosign texts (decision UX-38)

Free Input row 1 for each value. Keyed: AR, BT, KN, AS, SK with a word gap after each of the first four:

```
    0123456789012345678901234
1  |+ = ( & <SK>             |   Punctuation
1  |<AR> <BT> <KN> <AS> <SK> |   Prosign
1  |<AR/+> <BT/=> <KN/(> <AS/|   Both: '<AS/&>' splits at the line end
2  |&> <SK>                  |
```

The same five prosigns with no word gap (the host test V-84):

```
    0123456789012345678901234
1  |+=(&<SK>                 |   Punctuation
1  |<AR><BT><KN><AS><SK>     |   Prosign
1  |<AR/+><BT/=><KN/(><AS/&><|   Both
2  |SK>                      |
```

A 6-cell text at a line end (`Both`, AR). Each cell wraps on its own (UX-26). Row 1 holds `E` letters up to the
cursor:

```
    0123456789012345678901234
1  |EEEEEEEEEEEEEEEEEEE<AR/+>|   cursor in column 19: the text fits, the column becomes 25 (line full)
1  |EEEEEEEEEEEEEEEEEEEEEE<AR|   cursor in column 22: '<AR' in columns 22-24
2  |/+>                      |                        '/+>' in row 2, columns 0-2
1  |EEEEEEEEEEEEEEEEEEEEEEE<A|   cursor in column 23: '<A' in columns 23-24
2  |R/+>                     |                        'R/+>' in row 2, columns 0-3
1  |EEEEEEEEEEEEEEEEEEEEEEEEE|   cursor in column 25 (line full): the whole text on row 2
2  |<AR/+>                   |
```

A split in row 7 scrolls in the middle of the text. Rows 1-6 hold `A` to `F` (25 each), row 7 holds 22 letters
`G`, then AR in `Both`:

```
    0123456789012345678901234
1  |BBBBBBBBBBBBBBBBBBBBBBBBB|
2  |CCCCCCCCCCCCCCCCCCCCCCCCC|
3  |DDDDDDDDDDDDDDDDDDDDDDDDD|
4  |EEEEEEEEEEEEEEEEEEEEEEEEE|
5  |FFFFFFFFFFFFFFFFFFFFFFFFF|
6  |GGGGGGGGGGGGGGGGGGGGGG<AR|   '<AR' is drawn in row 7, then the scroll moves it to row 6
7  |/+>                      |
```

How the user reads a prosign text:

- A `<` always opens a prosign text and the next `>` closes it. Everything between them is one prosign.
- In `Both`, the `/` inside the brackets separates the name and the mark. A keyed `/` (`-..-.`) is a one-cell
  character outside the brackets. Examples with no word gap: AR then a keyed `/` gives `<AR/+>/`. A keyed `/`
  then AR gives `/<AR/+>`.
- At a line end, a text can split. The part on the next line can then start with `/`, a letter, a mark or `>`.
  Examples: `/+>`, `R/+>`, `&>`. The closing `>` shows that the part belongs to the text above. This is the accepted cost of the per-character
  wrap (UX-26, UX-38).
- `(` (KN with `Punctuation` and `Both`) and `<` are both left brackets of 3 px width. Glyph facts below. D-25
  checks on the glass that the user tells them apart.

Glyph facts, decoded from `u8g2_font_spleen5x8_mf` with the driver arithmetic (section 3.3). They are facts of
the bitmap. Their look on the glass is unverified until D-25.

| Glyph | Cell columns | Cell rows | Shape |
| --- | --- | --- | --- |
| `<`, `>` | 1-3 | 1-6 (capital height) | A point at rows 3-4. |
| `(`, `)` | 1-3 | 0-7 (one row more above and below) | A straight vertical stroke at rows 2-5. |
| `/` | 0-3 | 0-7 | Taller than the capitals beside it. |
| `+` | 0-4 | 2-6 | The bar in row 4 uses column 4. |
| `&` | 0-4 | 0-6 | Ink in column 4 at rows 4 and 6. |
| `=` | 0-3 | 3 and 5 | Two bars. |

`>` has no ink in cell column 0, so `+>` and `&>` keep a 1 px blank column between the glyphs. No glyph pair inside a
prosign text touches. Pixel rendering of three `Both` texts (`#` = ink, 30 x 8 px, cell row 0 on top):

```
<AR/+>                           <KN/(>                           <AS/&>
..................#...........   ..................#....#......   ..................#...#.......
...#..##..###.....#.......#...   ...#.#..#.#..#....#...#...#...   ...#..##...###....#..#.#..#...
..#..#..#.#..#...#....#....#..   ..#..#..#.##.#...#...#.....#..   ..#..#..#.#......#...#.#...#..
.#...#..#.#..#...#....#.....#.   .#...###..##.#...#...#......#.   .#...#..#..##....#...##.....#.
.#...####.###...#...#####...#.   .#...#..#.#.##..#....#......#.   .#...####....#..#...#.#.#...#.
..#..#..#.#..#..#.....#....#..   ..#..#..#.#.##..#....#.....#..   ..#..#..#....#..#...#..#...#..
...#.#..#.#..#.#......#...#...   ...#.#..#.#..#.#......#...#...   ...#.#..#.###..#.....##.#.#...
...............#..............   ...............#.......#......   ...............#..............
```

#### Delimiter (long `next`, decision UX-29)

- If the column is not 0, go to a new line first (with a scroll on row 7). At the start position (row 1, column 0)
  the delimiter uses row 1.
- Draw one horizontal line in that row: pixel row `y = 8r + 3`, `x` = 2 to 126 (the 25 text cells). All other
  pixels of the row stay off. Margin pixels x 0, 1 and 127 stay off.
- Set the column to 25. The next character starts a new line under the delimiter. Until then the delimiter can be
  the last row, and no empty row shows under it.
- Two delimiters in a row give two lines on two rows.
- The delimiter does not change the decoder. A character in progress during the long press still shows, on the row
  after the delimiter.
- The line scrolls with the text, because it is part of the row.
- A continuous line is different from the `-` glyph: the glyph is short and has a gap at each cell edge.

#### Clear (long `prev`, decision UX-30)

- All pixels of rows 1-7 go off. The title bar stays and is not drawn again.
- The cursor goes to row 1, column 0.
- The elements of the character in progress are dropped. The next character starts from its first element.
- The keyer and the sidetone do not change. A tone that sounds while the user clears continues until the key opens.
- The device does not ask again before it clears. The 800 ms hold is the protection (UX-23).

### 5.10 `Prosigns` setting (decision UX-38)

Purpose: the user chooses how AR, BT, KN and AS show in Free Input (section 5.9). SK always shows as `<SK>`.

| Item | Value |
| --- | --- |
| Label | `Prosigns` (8 characters, fit rule 8 <= 9) |
| List and position | `Settings`, index 2: row 3, after `Show CW Code` and before `Max Word Length` |
| Kind | choice, field width 11 |
| Values in order | `Punctuation`, `Prosign`, `Both`. `prev` goes towards `Punctuation`, `next` towards `Both`. No wrap. |
| Default | `Both`. A record of the earlier firmware also gives `Both` (section 8, F-14). |
| Effect | At the next start of Free Input (section 6.7). No effect in the menu, no sound, no preview. |
| Save | The rule of section 7 (UX-15): on the exit from Settings, after an edit step. |

Settings list in browse mode, cursor on `Prosigns`, default values:

```
    0         1         2
    0123456789012345678901234
0  |Settings                 | title bar, inverted
1  |CW                      >|
2  |Show CW Code         On  |
3  |Prosigns           Both  | selected: highlight x 0-127, y 25-31
4  |Max Word Length       5  |
5  |Display                 >|
6  |Reset Statistics         |
7  |                         | blank: the list has 6 items
```

The value is right-aligned to column 22 in browse mode: `Both` in columns 19-22, `Prosign` in columns 16-22,
`Punctuation` in columns 12-22. Each value shows in full.

Edit mode (section 5.4) with fw 11: the value field is columns 12-22, `<` is in column 10, column 11 is blank, `>` is
in column 24. The highlight band is x 51-127, y 25-31. The label `Prosigns` (columns 0-7) stays in normal video.
Columns 8 and 9 stay blank.

```
    0         1         2
    0123456789012345678901234
3  |Prosigns  <        Both  |   Both (default, last value): only '<'
3  |Prosigns  <     Prosign >|   Prosign: both markers
3  |Prosigns    Punctuation >|   Punctuation (first value): only '>'
```

From `Both`, two `prev` presses reach `Punctuation`. A third `prev` changes nothing. `next` goes back.

### 5.11 Training screens: common layout (decision UX-39 to UX-41)

Receiving and Sending use the same frame. The user decisions of the plan (session sizes, pass rule, level model,
attempts) are in section 5.15 and in `training_spec.md`.

```
    0         1         2
    0123456789012345678901234
0  |RX 20/15 L12         7/50| header, inverted (UX-39)
1  |       Try 2 of 3        | status line, normal video, centered
2  |                         | blank
3  |                         | item line: 8 x 16 font, cell top y = 24, pages 3 and 4 (UX-41)
4  |                         |
5  |                         | Sending: the code of the answer (section 5.13). Receiving: blank
6  |_____ _____ ____ ________| Receiving: four key bars at y = 55. Sending: blank
7  |A-K    L-V  W-6    7-<AR>| Receiving: group labels (section 5.12.1). Sending: keyed code (section 5.13)
```

| Area | Pixels | Font and video | Content |
| --- | --- | --- | --- |
| Header | y 0-7, x 0-127 | 5 x 8, inverted | Section 5.11.1. |
| Status line | row 1, y 8-15 | 5 x 8, normal | One text of section 5.14, centered: start column = floor((25 - length) / 2). With no text, the row is blank. |
| Item line | y 24-39 | 8 x 16, normal, plus the block and bar marks | Section 5.11.2. |
| Answer code | row 5, y 40-47 | marks (section 5.13) | Sending only. |
| Key bars | y 55, four segments: x 2-26, 30-54, 58-82, 87-126 | 1 px lines | Receiving only. One segment above each label field (section 5.12.1, UX-51). All other pixels of row 6 are off. |
| Bottom row | row 7, y 56-63 | 5 x 8, normal, or marks | Receiving: group labels. Sending: keyed code. |

- Rows 2 and 6 hold no text. Each area stays inside its own LCD pages, so a status change dirties page 1 only, and an
  item change dirties pages 3 and 4 only.
- The training screens have no highlight band. The block mark (section 5.11.2) is the only focus mark. The start
  screen and the session end screen have one highlight band each, on `Start` (sections 5.15 and 5.16).
- Pixels with x = 0, 1 and 127 are off in rows 1-7, as in Free Input. The key bars and the marks start at x 2.
- The mockups show the labels and the key bars on the 25-column grid. This is illustrative. The labels are placed in
  pixels (section 5.12.1).

#### 5.11.1 Header (decision UX-39)

| Field | Cells | Text |
| --- | --- | --- |
| Mode tag | 0-1 | `RX` in Receiving, `TX` in Sending (the abbreviations of UX-3). |
| Speed | from column 3, left-aligned, 3 to 5 cells | `<speed>/<effective speed>`: `RX Speed` and `RX Eff. Speed` in Receiving, `TX Speed` and `TX Eff. Speed` in Sending. Both numbers always show, for example `20/20`. |
| Level | from column 9, left-aligned, 2 to 6 cells | `L` and the level number, no leading zeros. The level number is the level index plus 1, at most 65535 (`training_spec.md` D-T5). |
| Item | right-aligned, last character in column 24, 3 to 7 cells | `<i>/<n>`. `i` is the 1-based number of the current item. `n` is the item count of the session: 50 for characters, 10 to 100 for words. No leading zeros. |

Examples:

```
    0123456789012345678901234
0  |RX 32/32 L1          1/50|   first session at the default RX speeds
0  |TX 5/5   L12         7/50|   the speed field is 3 cells wide, the level stays in column 9
0  |RX 50/50 L65535   100/100|   worst case: 25 cells, columns 15-17 stay blank
```

- The worst case `RX 50/50 L65535   100/100` fills all 25 columns. At least 3 blank cells (columns 15-17) stay
  between the level and the item. No short form is necessary.
- Video: inverted over x 0-127, y 0-7, the same as the title bar (UX-2). The header takes the place of the title.
- The tag, the speeds and the level do not change during a session. The item number changes at the start of the
  blank moment between two items (section 5.14). The rule: in a gap (the start pause or a blank moment), the header
  shows the number of the next item. At other times it shows the number of the item on the screen. So the start pause shows `1/<n>`.
  The session end screen keeps the header of the session that ended, with `i` = `n`, also while the user changes
  the selected level (section 5.15).
- Start screen (UX-53, section 5.16.1): the level field shows the selected level, and the item field shows `1/<n>`
  with `n` = the item count of the selected level. This is the header of the start pause of that level. So the
  header does not change when the session starts.

#### 5.11.2 Item line (decision UX-41)

Font: `FONTS_ID_8X16` (section 3.3.1), cell top `TRAINING_ITEM_Y` = 24. Glyph ink is in y 25-38, capitals in y 26-35.

Position of the text:

- Character item: one character text of `m` cells (1, 4 or 6, by the `Prosigns` setting). Its x is `64 - 4m`:
  60 for `K`, 48 for `<SK>` or `<AR>`, 40 for `<AR/+>`. The text is centered.
- Word item of `n` characters (2 to 8): every character is one cell (UX-48). Character `k` (0-based) is at
  `x = 68 - 8n + 16k`. One blank cell separates two characters, and the word is centered.

| n | x of the characters |
| --- | --- |
| 1 | 60 |
| 2 | 52, 68 |
| 3 | 44, 60, 76 |
| 4 | 36, 52, 68, 84 |
| 5 | 28, 44, 60, 76, 92 |
| 6 | 20, 36, 52, 68, 84, 100 |
| 7 | 12, 28, 44, 60, 76, 92, 108 |
| 8 | 4, 20, 36, 52, 68, 84, 100, 116 |

Marks (all on pixels, normal video):

| Mark | Pixels | Meaning |
| --- | --- | --- |
| Block | x to x + 6, y 26-39 (7 x 14 px) | Receiving: the slot that the next pick fills. |
| Bar | x to x + 6, y 38-39 (7 x 2 px) | Receiving: an empty slot that is not the next one. Sending: the character that the user keys next, in a word item only. |

- `x` is the slot x of the table above (60 for a character item).
- The bar is under the glyph rows. Only the tail of `Q` (y 37) is next to it. D-26 checks that `Q` stays readable.
- All other pixels of y 24-39 are off.

### 5.12 Receiving screen (decision UX-42 to UX-44, UX-48, UX-51)

Purpose: the device plays an item in CW. The user picks the characters with the four buttons. The device checks
the answer.

Character session, all 43 characters learned, `Prosigns` = `Both`, before the first pick. The item is AR:

```
    0         1         2
    0123456789012345678901234
0  |RX 20/15 L62        12/50| header, inverted
1  |                         | status: blank in the first attempt
2  |                         |
3  |            #            | item line: block at x 60 (the slot of the answer)
4  |            #            |
5  |                         |
6  |_____ _____ ____ ________| four key bars, y 55
7  |A-K    L-V  W-6    7-<AR>| labels: menu/back, prev, next, enter
```

The same item after `enter` (into `7-<AR>`), then after `enter` again (into `<SK><AR>`):

```
7  |789    .,? /<BT> <SK><AR>|   'enter' narrows to SK and AR
7  |<SK>  <AR>               |   two runs of one character: 'prev' picks AR, the slot shows <AR/+>
```

Exact label x (the mockups above are on the grid and are illustrative):

| Screen | `menu/back` | `prev` | `next` | `enter` |
| --- | --- | --- | --- | --- |
| Whole set | `A-K` at x 2 | `L-V` at x 35 | `W-6` at x 63 | `7-<AR>` at x 97 |
| After `enter` | `789` at x 2 | `.,?` at x 35 | `/<BT>` at x 58 | `<SK><AR>` at x 87 |
| After `enter`, `enter` | `<SK>` at x 2 | `<AR>` at x 32 | blank | blank |

Word session, 5 characters, two picks done (`K`, `M`):

```
    0         1         2
    0123456789012345678901234
3  |    K  M  #  _  _        |   K at x 28, M at x 44, block at x 60, bars at x 76 and 92 (illustrative)
4  |                         |
```

The mockups are illustrative. The item line is in pixels, not on the 25-column grid.

#### 5.12.1 Group keyboard (decision UX-42, UX-51)

Keyboard set: the characters that the session can ask for, in keyboard order: `A` to `Z`, `0` to `9`, `.` `,` `?`
`/`, then BT, SK, AR (plan). A character session uses all learned characters. A word session uses the learned
characters without BT, SK and AR (UX-48).

Current group: a run of the keyboard set. At the start of each pick it is the whole set. The four buttons split the
current group into four runs, in keyboard order. Each run belongs to the button under it, from left to right
(user decision 2026-10-06, `training_spec.md` D-T27 and section 8):

- `c` = size of the current group. Integer division.
- Run `k` has `s_k = (c + 3 - k) / 4` characters: `k` = 0 for `menu/back`, 1 for `prev`, 2 for `next`, 3 for
  `enter`.
- The runs differ by at most 1 character. The larger runs are on the left. Empty runs are on the right.

| `c` | 2 | 3 | 4 | 5 | 9 | 13 | 16 | 17 | 40 | 43 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| Runs | 1/1/0/0 | 1/1/1/0 | 1/1/1/1 | 2/1/1/1 | 3/2/2/2 | 4/3/3/3 | 4/4/4/4 | 5/4/4/4 | 10/10/10/10 | 11/11/11/10 |

A tap of a group button (tap mode: it acts at the release, section 6.8):

| Run size | Effect |
| --- | --- |
| 0 | Nothing: no change, no redraw. The label is blank. The key bar stays. |
| 1 | Pick: the character goes into the slot of the block. The current group becomes the whole set again. The pick that fills the last slot starts the check at once. |
| 2 or more | Narrow: the run becomes the current group. The labels change. |

Presses per pick (every character, every Koch prefix, script check below):

| Keyboard set | Presses per pick |
| --- | --- |
| 2 to 4 characters | 1. Every run holds at most one character. |
| 5 to 16 characters | 1 or 2, at most 2. |
| 17 to 43 characters | 2 or 3, at most 3. With 43 characters (11 -> 3 -> 1) and with the 40-character word keyboard (10 -> 3 -> 1), every pick takes exactly 3 presses. |

Empty runs: with 2 characters, `next` and `enter` select nothing. With 3 characters, `enter` selects nothing. From 4
characters on, every button has a run at the start of a pick.

Label fields in row 7 (y 56-63, 5 x 8 font, normal video). `n` = cells of the label. Integer division:

| Run | Button | Field (pixel x) | Width | Text x |
| --- | --- | --- | --- | --- |
| 0 | `menu/back` (left) | 2-26 | 5 cells | Left-aligned: x = 2. |
| 1 | `prev` | 30-54 | 5 cells | Centered: x = 30 + (5 * (5 - n)) / 2. |
| 2 | `next` | 58-82 | 5 cells | Centered: x = 58 + (5 * (5 - n)) / 2. |
| 3 | `enter` (right) | 87-126 | 8 cells | Right-aligned: x = 127 - 5n. The last cell ends at x 126. |

- Pixels x 27-29, 55-57 and 83-86 of row 7 stay off. With the blank glyph column, two labels have at least 4 blank
  pixels between their ink. Letters inside a label have 1.
- The key bar above each field has the x range of its field (section 5.11). The four bars show four keys, also when a
  label is blank or long.
- The positions are compile-time constants (`training_spec.md` LC-3). A label redraw writes row 7 only (LC-7). The
  key bars are drawn with the screen and do not change in a session.
- The `enter` field is wider: the prosigns are at the end of the keyboard order, so only the right run can hold two
  of them (`<BT><SK>`, `<SK><AR>`: 8 cells). The fields of the other three runs need at most 5 cells.

Label text of a character (UX-51): the text of `morse_format()` in `MORSE_STYLE_PUNCTUATION` when `Prosigns` =
`Punctuation`, else in `MORSE_STYLE_NAME`. A letter, a digit or a mark is itself. BT is `<BT>` or `=`, SK is
`<SK>`, AR is `<AR>` or `+`. In `Both`, a label shows the first text of `<AR/+>`: `<AR>`. No new style and no
label table (`training_spec.md` LC-4).

| Run | Label |
| --- | --- |
| Empty | Blank. |
| One character | Its label text: `K`, `.`, `<BT>`, `=`, `<SK>`, `<AR>`, `+`. |
| Two or more characters, and the label texts written one after the other fit the field | List: the texts with no separator: `KM`, `AEFIJ`, `/<BT>`, `<SK><AR>`, `<SK>+`, `Z059.,/`. |
| Two or more characters, and the list does not fit | Range: the text of the first character, `-`, the text of the last character: `A-K`, `4-/`, `7-<AR>`, `7-+`. |

- `-` is not in the keyboard set, so a `-` in a label always means a range.
- A range holds only the learned characters between its ends. For example, with 30 learned characters, `A-K` holds
  A, E, F, G, H, I, J and K: B, C and D are not learned yet. The user hears only learned characters, so the label
  is not misleading.
- The list limit is the field width. So the `enter` field can show a list of 6 to 8 characters while the other
  fields show ranges. Example, 28 learned characters: `A-J`, `K-Q`, `R-Y`, `Z059.,/`.
- A prosign label never splits and never loses its brackets: `<` and `>` always bound a prosign name, as in Free
  Input (UX-38).
- The item line, the answer and the status texts keep the full text of the `Prosigns` setting. So in `Both`, the
  label `<AR>` picks AR, and the slot shows `<AR/+>`.
- The `Prosigns` setting is read once at the mode start, as in Free Input (UX-38).

Fit check (task D1, script `label_check.py`, 2026-10-06): every Koch prefix of 2 to 43 learned characters, as a
character session and as a word session (word sets have at most the first 40 Koch characters), for all three
`Prosigns` values. Every range that the user can reach, at every depth. 12 096 labels checked. Result: pass.

| Field | Longest label | Cells |
| --- | --- | --- |
| `menu/back` | `AEFIJ` (17 learned) | 5 of 5 |
| `prev` | `KLMNO` (18 learned) | 5 of 5 |
| `next` | `PRSTU` (19 learned), `/<BT>` (41 or 43 learned) | 5 of 5 |
| `enter` | `<SK><AR>` (43), `<BT><SK>` (42), `3589.,?/` (32) | 8 of 8 |

Also checked: no label pixel at x 0, 1 or 127, at least 4 blank pixels between two labels, at most 3 selects per
pick (2 for 5 to 16 characters, 1 for 2 to 4), no prosign in a word set. The hard cases of `training_spec.md` LC-6:
`7-<AR>` is 6 of 8 cells in the `enter` field. `<SK><AR>` is a list of 8 of 8 cells, so the range `<SK>-<AR>` (9)
never shows. `<BT/=>` and `<AR/+>` (6) never show in a label. One row is enough: row 6 holds no label (LC-2).

Labels with all 43 characters (character session). `Prosign` and `Both` give the same labels. `Punctuation` differs
only where the table shows it in brackets:

| Group (taps from the whole set) | `menu/back` | `prev` | `next` | `enter` |
| --- | --- | --- | --- | --- |
| Whole set | `A-K` | `L-V` | `W-6` | `7-<AR>` (`7-+`) |
| `menu/back` | `ABC` | `DEF` | `GHI` | `JK` |
| `prev` | `LMN` | `OPQ` | `RST` | `UV` |
| `next` | `WXY` | `Z01` | `234` | `56` |
| `enter` | `789` | `.,?` | `/<BT>` (`/=`) | `<SK><AR>` (`<SK>+`) |
| `enter`, `next` | `/` | `<BT>` (`=`) | blank | blank |
| `enter`, `enter` | `<SK>` | `<AR>` (`+`) | blank | blank |

Labels of the word keyboard with 40 characters (word session, no prosign, the same for every `Prosigns` value):

| Group | `menu/back` | `prev` | `next` | `enter` |
| --- | --- | --- | --- | --- |
| Whole set | `A-J` | `K-T` | `U-3` | `4-/` |
| `menu/back` | `ABC` | `DEF` | `GH` | `IJ` |
| `prev` | `KLM` | `NOP` | `QR` | `ST` |
| `next` | `UVW` | `XYZ` | `01` | `23` |
| `enter` | `456` | `789` | `.,` | `?/` |

Prosign tail with 41 and 42 learned characters (character sessions):

| Learned, group | `menu/back` | `prev` | `next` | `enter` |
| --- | --- | --- | --- | --- |
| 41, whole set | `A-K` | `L-U` | `V-4` | `5-<BT>` (`5-=`) |
| 41, `enter` | `567` | `89.` | `,?` | `/<BT>` (`/=`) |
| 41, `enter`, `enter` | `/` | `<BT>` (`=`) | blank | blank |
| 42, whole set | `A-K` | `L-V` | `W-5` | `6-<SK>` |
| 42, `enter` | `678` | `9.,` | `?/` | `<BT><SK>` (`=<SK>`) |
| 42, `enter`, `enter` | `<BT>` (`=`) | `<SK>` | blank | blank |

#### 5.12.2 Item flow

States of a Receiving session. They follow the phases of the session engine (`training_spec.md` section 5.10).
Section 6.8 gives the button rules of each state.

| State (engine phase) | Screen | Sound | Ends when |
| --- | --- | --- | --- |
| Start screen (SELECT) | Section 5.16.1. Tap mode is off. | None | An `enter` press or a tip closure: the session starts at the selected level (start pause). A `menu/back` press: the main menu (section 5.16.4). |
| Start pause (GAP) | Item line blank, labels of the whole set, status blank, header `1/<n>` | None | `TRAINING_START_MS` (1000 ms) after the session start. Then Item. |
| Item, playing (ITEM) | Empty slots, block on the first slot, labels. Picks show at once. | The player plays the item at `RX Speed` and `RX Eff. Speed`, at the `Tone` pitch. | The player is idle: Item, waiting. The last slot filled: the check. A long `enter`: the playback starts again. |
| Item, waiting (ITEM) | Picks, block, bars, labels | None | The last slot filled: the check. A long `enter` (or a tip closure of 800 ms): replay, then Item, playing. |
| Result (RESULT) | Section 5.14 | Section 5.14 | Section 5.14 |
| Blank moment (GAP) | Section 5.14 | None | `TRAINING_RECEIVING_GAP_MS` (500 ms). Then Item with the next item. |
| End (END) | Section 5.15. Tap mode is off. | None | An `enter` press or a tip closure: the next session at the selected level. A `menu/back` press: the main menu (section 5.16.4). |

Rules:

- The check comes at the pick that fills the last slot, at the release of the button. It compares the whole answer
  with the item. One check is one attempt. There are 3 attempts per item (plan).
- At the check, a running playback stops (the tone falls in 8 ms). The player and the keyer are never on together.
- Retry (attempt 2 or 3): the slots become empty, the block goes to the first slot, the status shows `Try 2 of 3`
  or `Try 3 of 3`, and the item plays again. No start pause, no blank moment.
- Replay: a long `enter` (a hold of 800 ms) in Item plays the item again from its start. The replay starts at
  800 ms, while the button is still held. The release then does nothing. A running playback starts again from the
  start. The picks, the current group and the labels stay. Replays are free: they use no attempt and do not change
  the score. In the start pause, a result and the blank moment, a long `enter` does nothing.
- An `enter` tap (released before 800 ms) in Item selects the right run (section 5.12.1). It never replays.
- In Receiving, the paddle tip is `enter` (C-4). A short closure of the dit lever or a straight key (opened before
  800 ms) is an `enter` tap: it selects the right run. A closure of 800 ms or longer is a long `enter`: it replays.
  The ring does nothing. The keyer is off and the paddle gives no tone.
- The `enter` press that opens Receiving from the menu shows the start screen and does nothing else. The `enter`
  press (or tip closure) that starts a session on the start screen or the end screen gives nothing at its release
  and nothing after 800 ms: tap mode goes on after its press (section 6.2). So it never picks and never replays
  (`training_spec.md` 5.9).

### 5.13 Sending screen (decision UX-45, UX-50, UX-52)

Purpose: the device shows an item. The user keys it with the paddle or a straight key. The device checks each
character at once. In a word item, it also checks the pause between two characters (UX-52).

Character session, `Show CW Code` = `On`, the user keyed `-.` (N) for the item K:

```
    0         1         2
    0123456789012345678901234
0  |TX 20/20 L12         7/50| header, inverted
1  |        Wrong: N         | status (section 5.14)
2  |                         |
3  |            K            | item line: K at x 60, no bar (character item)
4  |            K            |
5  |                         | blank (the answer code shows here after the 3rd wrong attempt)
6  |                         |
7  |-- -                     | keyed code: dash, dot (marks, illustrative)
```

Word session, 4 characters, the first two keyed correctly:

```
3  |      K  M  R  S         |   K at x 36, M at x 52, R at x 68, S at x 84 (illustrative)
4  |            ==           |   bar under R (x 68-74, y 38-39): the next character to key
```

The same word, attempt 1. The user keyed K and M, then paused too long (UX-52):

```
    0         1         2
    0123456789012345678901234
1  |  Wrong: pause too long  | status, column 2, for TRAINING_WRONG_MS (1200 ms)
3  |      K  M  R  S         |
4  |            ==           | the bar stays under R: the pause came before R
5  |                         | blank (after the 3rd attempt: the code of R here)
7  |-- --                    | row 7 unchanged: the marks of M, the last keyed character
```

The same word at the retry (1200 ms after the check). The screen looks like a fresh item, but the status keeps the
attempt number:

```
    0         1         2
    0123456789012345678901234
1  |       Try 2 of 3        | status
3  |      K  M  R  S         |
4  |      ==                 | bar back under K (D-T6)
5  |                         | blank
7  |                         | row 7 off (UX-52), with Show CW Code On and Off
```

#### 5.13.1 Item line and checks

- Start pause: for `TRAINING_START_MS` (1000 ms) after the session start, the item line is blank. Keyer events in
  this time are dropped (section 5.13.2). Then the first item appears.
- The item line shows the whole item (section 5.11.2). Prosign texts follow the `Prosigns` setting,
  read once at the mode start. A character item has no bar.
- Word item: the bar is under the character that the user keys next. A correct character moves the bar to the next
  character. The keyed characters keep their normal look.
- The check: at each CHAR_GAP event, the decoded character (`morse_decode()`) is compared with the expected
  character. The compare uses the character code, not the text. So `=` and `<BT/=>` are the same answer.
- A correct character that is not the last one: no status text, no delay. The bar moves on.
- The last character correct: the result `Correct` (section 5.14).
- A wrong character, also `*` (a pattern that is not in the table, the 8-dit error sign): the attempt ends. The
  result `Wrong` (section 5.14).
- Pause too long (word item only, UX-52): a WORD_GAP in Item counts when it ends the space right after a correct
  character that is not the last one of the item. It ends the attempt. The result is `Wrong: pause too long`
  (section 5.14). It uses an attempt, as a wrong character does. The bar stays under the next character: the pause came before it.
- Rule: a WORD_GAP in Item counts when it ends the space right after a correct character that is not the last one
  of the item. A WORD_GAP does nothing in these cases: a character item, and a word item before the first correct
  character of the attempt. It also does nothing in every state that is not Item (section 5.13.2), and after
  OVERFLOW (a lost event is a device fault, D-T16). In the result delay, the blank moment, or the next attempt
  before its first correct character, a WORD_GAP does nothing. At a low `TX Eff. Speed` the WORD_GAP can come late:
  5 units take about 2669 ms at 20 / 5.
- Retry (`training_spec.md` D-T6): the user keys the whole item again from its first character. The bar goes back
  to the first character. This matches CW practice: after an error, the operator sends the word again. The retry
  screen is in sections 5.13.3 and 5.14.
- First-attempt correct: the item had no wrong character and no pause error.

Pause limit (UX-52). After the last element, the space grows. At 5 Farnsworth units of `TX Eff. Speed`, the keyer
gives WORD_GAP (`KEYER_WORD_GAP_UNITS`, `keyer_spec.md` section 6.7). Free Input inserts a word space at the same
point. The limit counts from the end of the last element of a character to the start of the first element of the
next character. Examples, from the keyer table (4 ticks per ms):

| `TX Speed` / `TX Eff. Speed` | Normal space between characters | Pause limit |
| --- | --- | --- |
| 32 / 32 | 113 ms (3 units) | 188 ms |
| 20 / 20 | 180 ms | 300 ms |
| 13 / 13 | 277 ms | 461 ms |
| 5 / 5 | 720 ms | 1200 ms |
| 20 / 10 | 654 ms | 1090 ms |
| 20 / 5 | 1601 ms | 2669 ms |

The normal space between characters is 3 units. So the user has 2 units of margin before the limit. A user who needs
more time lowers `TX Eff. Speed`.

#### 5.13.2 Keyer events

| Event | In an input state (ITEM) | During the start pause, a result delay or the blank moment (GAP, RESULT) |
| --- | --- | --- |
| DIT, DAH | Builds the character. With `Show CW Code` = `On`, a mark appears in row 7. | Dropped. The character that it starts is dropped (next row). |
| CHAR_GAP | The check (section 5.13.1). If an element of this character was dropped, nothing: no check, no attempt. | Dropped. |
| WORD_GAP | Word item: a WORD_GAP in Item counts when it ends the space right after a correct character that is not the last one of the item. Then the attempt ends, result `Wrong: pause too long` (UX-52). Nothing in all other cases: a character item, a word item before the first correct character of the attempt, and after OVERFLOW (a lost event is a device fault, D-T16). | Dropped. |
| OVERFLOW | The character in progress is dropped: no check, no attempt, no status. | Dropped. |

- Dropped-character rule: when a DIT or DAH is dropped, the mode also drops every event up to and including the
  next CHAR_GAP. So a character that starts during a delay never counts, even after the delay ends. The user
  then keys it again.
- The keyer and its sidetone run during the delays. The user hears what they key, but it does not count.
- A lost event is a fault of the device, not of the user. So OVERFLOW never uses an attempt (F-11 for Sending).
- WORD_GAP comes once per space, after the CHAR_GAP of that space (`keyer_spec.md` 6.6). After OVERFLOW, a
  WORD_GAP never counts (`training_spec.md` D-T31, D-T16):
  - If the lost events held the CHAR_GAP, the drop flag is still set when the WORD_GAP comes. So the WORD_GAP is
    dropped with the rest of the character.
  - If the CHAR_GAP came after the OVERFLOW, the WORD_GAP of that space does nothing.

#### 5.13.3 Code row (row 7, decision UX-50)

`Show CW Code` = `On`: row 7 shows the marks of the character in progress. `Off`: row 7 stays blank.

| Item | Value |
| --- | --- |
| Dot mark | 2 x 2 px |
| Dash mark | 6 x 2 px |
| Mark rows | `y = 8r + 3` and `8r + 4` (row 7: y 59-60, row 5: y 43-44) |
| Gap between marks | 3 px |
| Row 7 start | x = 2, left-aligned. Each new mark goes to the right of the last one. |
| Row 5 start (answer) | Centered: x = floor((128 - w) / 2), w = the width of all marks and gaps. |
| Limit | `TX_CODE_MAX_MARKS` = 12 marks. Later marks of the same character are not drawn. 12 dashes are 105 px. |

- A mark appears at the DIT or DAH event, at the end of the element. The sidetone already gives the element at
  once. The marks show what the keyer decoded.
- Row 7 keeps the marks of a character until the first element of the next character. Then row 7 clears and the
  new mark appears. So after a wrong character, the user sees what they keyed.
- During the result delay (`Wrong: ...`, 1200 ms), row 7 does not change. The user reads the wrong code there.
  After a pause error, row 7 keeps the marks of the last correct character.
- The retry clears row 7 (UX-52, user decision 2026-10-07). At the retry moment, all pixels of row 7 go off, in
  the same redraw as `Try 2 of 3` or `Try 3 of 3` and the bar move. The first element of the new attempt then
  draws its mark from x 2. With `Show CW Code` = `Off`, row 7 is already off, and the retry gives no visible change
  there.
- An element that the mode drops (a character that started in the delay, section 5.13.2) draws no mark, also after
  the retry.
- The blank moment clears row 7.
- The setting is read once at the mode start.

#### 5.13.4 P3.2 and the keyer (decision UX-33, extended)

- The keyer runs in all states of a Sending session: input, result delays and the blank moment. It does not run on
  the start screen. It stops at the session end screen and at the exit.
- Session start (from the start screen or the end screen): the `enter` press or tip closure that starts the session
  gives no element and no tone. A contact closed at the start stays ignored until it opens (the entry mask of
  UX-33).
- Session end screen: the keyer stops and P3.2 goes back to the menu. A contact held at that moment gives no
  `enter` until it opens and closes again.
- The keyer reads `Key Type`, `Paddle`, `TX Speed`, `TX Eff. Speed` and `Tone` at the session start (section 6.7).

### 5.14 Results and transitions (decision UX-46, UX-52)

Status texts (row 1, centered, section 5.11):

| Moment | Status text | Item line | Row 5 (Sending) | Row 7 (Sending) |
| --- | --- | --- | --- | --- |
| Attempt 1, input | Blank | Receiving: slots. Sending: the item. | Blank | Marks of the current or last character (5.13.3) |
| Attempt 2 or 3, input (the retry) | `Try 2 of 3`, `Try 3 of 3` | Receiving: empty slots, block on the first slot. Sending: the item, bar under the first character of a word. | Blank | Off at the retry moment, then the marks of the new attempt |
| Correct, any attempt | `Correct` | Unchanged: the picks or the item. | Blank | Unchanged |
| Wrong, attempt 1 or 2, Receiving | `Wrong` | Unchanged: the wrong picks stay visible. | - | - |
| Wrong character, attempt 1 or 2, Sending | `Wrong: ` and the full text of the keyed character (`Wrong: N`, `Wrong: *`, `Wrong: <AR/+>`) | Unchanged. In a word, the bar stays under the wrong position. | Blank | Unchanged: the marks of the wrong character |
| Pause too long, attempt 1 or 2, Sending word (UX-52) | `Wrong: pause too long` | Unchanged. The bar stays under the next character, where the pause came. | Blank | Unchanged: the marks of the last correct character |
| Wrong, attempt 3, Receiving | `Wrong. The answer is:` | The correct item, in the layout of section 5.11.2, no marks | - | - |
| Wrong character, attempt 3, Sending | `Wrong: ` and the keyed text, as above | Unchanged | The code of the expected character, centered marks (section 5.13.3). Shown also with `Show CW Code` = `Off`. | Unchanged |
| Pause too long, attempt 3, Sending word (UX-52) | `Wrong: pause too long` | Unchanged, bar under the next character | The code of the next character (the character under the bar), centered marks. Shown also with `Show CW Code` = `Off`. | Unchanged |

Longest status texts: `Wrong. The answer is:` (21 cells), `Wrong: pause too long` (21 cells, start column 2) and
`Wrong: <KN/(>` (13 cells). All fit the 25 cells.

Status text of a pause error (UX-52). The prefix `Wrong: ` is the same as for a wrong character. So the user reads
"the attempt failed" first, as in UX-46. A character text after `Wrong: ` is 1 to 6 cells and has no space. So
`pause too long` (three words) never reads as a keyed character. The bar shows where the pause came.

The retry moment (both modes, attempt 2 or 3). It comes `TRAINING_WRONG_MS` (1200 ms) after the check. One redraw
does all of this:

| Area | Receiving | Sending |
| --- | --- | --- |
| Header | Unchanged | Unchanged |
| Status (row 1) | `Try 2 of 3` or `Try 3 of 3` | `Try 2 of 3` or `Try 3 of 3` |
| Item line | All slots empty, block on the first slot | The item, unchanged glyphs. Word: bar under the first character. Character: no bar. |
| Row 5 | - | Blank (it is already blank) |
| Row 6 and 7 | Key bars and labels of the whole set | Row 7 off, for every `Show CW Code` value |
| Sound | The item plays again | None. The keyer runs all the time. |

Before the retry moment, during the 1200 ms, nothing of this changes. So the user reads the wrong code in row 7
and the status text for the full delay. After the retry moment, the screen looks like a fresh item. Only
`Try N of 3` tells the user that this is a retry. Receiving and Sending show the same status at a retry.

Delays (UX-46). The names are the constants of `training_spec.md` section 5.10:

| Constant | Value | From | To |
| --- | --- | --- | --- |
| `TRAINING_START_MS` | 1000 ms | The session start (`Start` on the start screen or the end screen). | The first item. |
| `TRAINING_CORRECT_MS` | 600 ms | The check: the pick that fills the last slot (Receiving), or the CHAR_GAP of the last character (Sending). | The blank moment. |
| `TRAINING_WRONG_MS` | 1200 ms | The check, attempt 1 or 2. In Sending also the WORD_GAP of a pause error (UX-52). | The retry moment (table above). |
| `TRAINING_FAILED_MS` | 2500 ms | Receiving: the end of the answer playback. Sending: the CHAR_GAP of the wrong character, or the WORD_GAP of a pause error. | The blank moment. |
| `TRAINING_RECEIVING_GAP_MS` | 500 ms | The end of a result that ends the item. | The next item appears and plays. |
| `TRAINING_SENDING_GAP_MS` | 300 ms | The end of a result that ends the item. | The next item appears. |

The Receiving gap is longer: the next sound starts with the next item, and the ear needs a short rest after the
result. In Sending, the eye only needs to see the blank.

Answer playback (Receiving, attempt 3 wrong): the item plays once more while the answer shows. The answer delay
counts from the end of that playback. So the user hears and sees the answer together. A word at a low speed can
play for many seconds, so the result must wait for the player.

Blank moment (both modes):

1. At its start: rows 1-5 of the body go off (status, item line, row 5). Sending also clears row 7. Receiving keeps
   the key bars and the labels of the whole set. The header shows the next item number.
2. For the gap time (500 ms in Receiving, 300 ms in Sending) the item line stays blank.
3. At its end: the next item appears. Receiving: empty slots with the block, and the playback starts at once.
   Sending: the next item text.

The blank moment comes before every next item, also before the same character again. So the user always sees a
new item start.
After the last item of a session, no blank moment comes: the session end screen follows the result at once.

Example timelines (Receiving, character item):

```text
first attempt correct:  last pick 0 ... 'Correct' 0-600 ... blank 600-1100 ... next item and playback 1100
first attempt wrong:    last pick 0 ... 'Wrong' 0-1200 ... retry 1200: 'Try 2 of 3', empty slots, playback
third attempt wrong:    last pick 0 ... 'Wrong. The answer is:' + playback ... end of playback + 2500 ... blank 500
```

Example timelines (Sending, word item `KMRS` at 20/20, `Show CW Code` = `On`):

```text
pause error:  CHAR_GAP of M ... WORD_GAP 0 (180 ms later): 'Wrong: pause too long', bar under R, row 7 '-- --'
              ... retry 1200: 'Try 2 of 3', bar under K, row 7 off
wrong char:   CHAR_GAP of N 0: 'Wrong: N', bar under M, row 7 '-.' ... retry 1200: 'Try 2 of 3', bar under K, row 7 off
```

### 5.15 Session end screen (decision UX-47, UX-56)

The plan gives the rules (user decisions): a character session has 50 items and passes at 45 or more. A word
session has 10 to 100 words and passes at 80 % of the count, rounded up (user decision 2026-10-07): 8 of 10, 80 of
100. "Correct" for the score means correct on the first attempt. A pass at the earned level advances the earned level
by one and saves the progress. A pass below the earned level (a replay, section 5.16) changes nothing in the store.
A fail changes nothing in the store.

The end screen has the level selector of section 5.16 in rows 4 and 5 (UX-56, user decision 2026-10-07). After a
pass, the selector preselects the level after the session level. After a fail, it preselects the session level.

Pass at the earned level (L13), progress saved:

```
    0         1         2
    0123456789012345678901234
0  |RX 20/15 L13        50/50| header of the session that ended
1  |                         | 'Not saved' here when the save failed (column 8)
2  |       Score 46/50       | column 7
3  |    Passed, new level    | column 4
4  |     <  L14 of 14        | selector (section 5.16.2): the new level, no '>'
5  |       New char W        | description (section 5.16.3), column 7
6  |                         |
7  |          Start          | 'Start' in columns 10-14, highlight x 47-81, y 57-63, always focused
```

Fail at the earned level (L13):

```
2  |       Score 43/50       |
3  |   Not passed, need 45   | column 3
4  |     <  L13 of 13        | the same level
5  |       New char O        |
```

Replay: the earned level is L20, the session level is L5:

```
0  |RX 20/15 L5         50/50|
2  |       Score 47/50       |
3  |         Passed          | column 9: no 'new level', the earned level stays L20
4  |     <  L6 of 20  >      | the next level, and the selector still reaches L20
5  |       Words of 2        |

2  |       Score 44/50       |   replay, fail
3  |   Not passed, need 45   |
4  |     <  L5 of 20  >      |
5  |       New char U        |
```

Pass at the earned level, save failed (F-16):

```
1  |        Not saved        | column 8
2  |       Score 46/50       |
3  |    Passed, new level    |
4  |     <  L14 of 14        | the new level from RAM
```

| Row | Text | Rule |
| --- | --- | --- |
| 0 | Header | The header of the session that ended, `i` = `n` (section 5.11.1). It does not change while the selection changes. |
| 1 | `Not saved`, or blank | Only after a failed progress save of a pass at the earned level (F-16). Row 1 is the status line of the training frame (section 5.11), so the message is in the place where the session showed its messages. |
| 2 | `Score <c>/<n>` | `c` = items correct on the first attempt, `n` = item count. |
| 3 | `Passed, new level`, `Passed`, or `Not passed, need <t>` | `Passed, new level`: a pass that advanced the earned level (saved or not saved). `Passed`: a replay pass, and a pass at the highest level index 65535 (no level change, `training_spec.md` D-T17). `t` = the pass threshold of the session (45 for 50 characters, 8 for 10 words, 80 for 100 words). |
| 4 | Selector line | Section 5.16.2. Preselect: pass = session level + 1 (saturated at index 65535), fail = session level. |
| 5 | Description | Section 5.16.3, for the selected level. |
| 6 | Blank | |
| 7 | `Start` | Highlight band cells 9-15 (x 47-81, y 57-63). Gap line y 56 off across the band (UX-2). The same band as the start screen (section 5.16.1). |

- All texts are centered (start column = floor((25 - length) / 2)). The longest are `Not passed, need 80` and
  `Not passed, need 45` (19 cells, column 3). A threshold has at most 2 digits: the largest is 80 (100 words).
  `Passed, new level` has 17 cells, column 4.
- Row 3 is the only place that tells a pass at the earned level from a replay pass. The selector also shows it: after
  a pass at the earned level, the selected level and the earned level are equal (`L14 of 14`). After a replay pass,
  `of <earned>` keeps the earned level (`L6 of 20`).
- Body texts are normal video. `Start` is the only highlight: `enter` activates it, as `OK` on a message screen.
- After `Not saved`, the new level stays in RAM until the power goes off (plan). The selector, the next session and
  the main menu use it.
- Controls: section 5.16.4, the same as the start screen. `prev` and `next` change the selected level. `enter` or
  the paddle tip starts the next session at the selected level. A `menu/back` press opens the main menu with the
  cursor on the mode item. Tap mode is off in both modes, so every button acts at the press (UX-55).
- The screen has no timeout.

### 5.16 Level selection (decision UX-53 to UX-57)

Purpose: the user chooses the level of the next session. The user can repeat any level from L1 up to the earned
level. Source: the plan "Level selection in Receiving and Sending", approved by the user on 2026-10-07. This
section is task D4 of `training_spec.md` (D-T35 to D-T41, SL-1 to SL-7).

Terms:

- **Earned level**: the stored progress of the mode at the current speed: `progress_level()` of Receiving at
  `RX Speed`, or of Sending at `TX Speed`. The effective speed has no effect (progress key of the plan). The shown
  number is the index plus 1, saturated at 65535 (`training_spec.md` D-T5).
- **Selected level**: the level of the next session. From L1 up to the earned level.
- **Replay**: a session at a selected level below the earned level. A replay never changes the earned level and never
  accesses the EEPROM.

Example (plan): the earned level is L20. The user selects L5 and passes. The end screen preselects L6, the earned
level stays L20, and the selector still reaches L20.

#### 5.16.1 Start screen (UX-53)

The start screen shows at each entry of Receiving or Sending from the main menu, also when the earned level is L1.
The selected level is the earned level. No session runs: no player, no keyer, no sound. Tap mode is off.

Earned level L20 at `RX Speed` 20 and `RX Eff. Speed` 15:

```
    0         1         2
    0123456789012345678901234
0  |RX 20/15 L20         1/50| header, inverted: the level and the item count follow the selection
1  |                         |
2  |                         |
3  |                         |
4  |     <  L20 of 20        | selector line: '<' column 5, text from column 8, no '>' at the earned level
5  |       New char J        | description, column 7
6  |                         |
7  |          Start          | 'Start' in columns 10-14, highlight x 47-81, y 57-63, always focused
```

After one `prev` (L19), one more `prev` (L18, a word level of 10 items, `Max Word Length` 5):

```
0  |RX 20/15 L19         1/50|
4  |     <  L19 of 20  >     |
5  |       New char N        |

0  |RX 20/15 L18         1/10|
4  |     <  L18 of 20  >     |
5  |       Words of 4        |
```

First use (empty EEPROM, default `RX Speed` and `RX Eff. Speed` 32):

```
0  |RX 32/32 L1          1/50|
4  |         L1 of 1         | no marker: no step is possible
5  |         K and M         | column 9
7  |          Start          |
```

Final word phase. Sending, earned level L65 at 20/20, `Max Word Length` 5, after one `prev`:

```
0  |TX 20/20 L64         1/10| L64 has 10 words, L65 has 11
4  |     <  L64 of 65  >     |
5  |       Words of 5        |
```

| Row | Content | Rule |
| --- | --- | --- |
| 0 | Header | Section 5.11.1. The level field shows the selected level. The item field shows `1/<n>`, `n` = the item count of the selected level (`koch_item_count()`): 50 at a character level, 10 at a word level, 10 to 100 in the final word phase. This is the header that the start pause of that level shows (the "next item" rule of section 5.11.1). So the session start does not redraw row 0. |
| 1-3 | Blank | The selector is in the same rows as on the end screen. The eye finds it in the same place on both screens. |
| 4 | Selector line | Section 5.16.2. |
| 5 | Description | Section 5.16.3. |
| 6 | Blank | |
| 7 | `Start` | `Start` in columns 10-14, inverted inside the band. Highlight band cells 9-15 (x 47-81, y 57-63). Gap line y 56 off across the band (UX-2). Always focused. The only highlight on the screen. |

- Pixels with x = 0, 1 and 127 are off in rows 1-7, as on the other training screens. The start screen has no key
  bars and no labels, also in Receiving.
- The settings of the mode are read once at the mode entry (section 6.1): the speeds, `Tone`, `Prosigns`,
  `Max Word Length`, and in Sending also `Key Type`, `Paddle` and `Show CW Code`. The start screen uses `Prosigns`
  and `Max Word Length` for the description.

#### 5.16.2 Selector line (UX-54)

Row 4, 5 x 8 font, normal video. The text is `L<s> of <e>`: `s` = the shown selected level, `e` = the shown earned
level. Both are the index plus 1, saturated at 65535 (D-T5), with no leading zeros. The text has 7 to 15 cells.

| Item | Rule |
| --- | --- |
| Line | `<`, two blanks, the text, two blanks, `>`: `len + 6` cells, 13 to 21. |
| Position | Centered: `c = floor((25 - (len + 6)) / 2)` = `floor((19 - len) / 2)`. `<` in column `c`, the text from column `c + 3`, `>` in column `c + len + 5`. All other cells of row 4 are blank. |
| `<` | Shows when `prev` can step: the selected index is above 0. |
| `>` | Shows when `next` can step: the selected index is below the earned index. |
| At a limit | The marker cell is blank. The text does not move. The same rule as the edit markers of section 5.4 (UX-4). |
| Video | Normal. No highlight band: the band of the screen is on `Start` (UX-2: one band per screen). The markers are the cue for `prev` and `next`. |

| Selected / earned | Row 4 (columns 0-24) |
| --- | --- |
| L1 / L1 | `         L1 of 1         ` |
| L1 / L2 | `         L1 of 2  >      ` |
| L2 / L2 | `      <  L2 of 2         ` |
| L5 / L20 | `     <  L5 of 20  >      ` |
| L20 / L20 | `     <  L20 of 20        ` |
| L100 / L120 | `    <  L100 of 120  >    ` |
| L65534 / L65535 (widest) | `  <  L65534 of 65535  >  ` |

- The text moves left by one column for each second extra digit: `L9 of 20` and `L10 of 20` start in column 8,
  `L99 of 120` starts in column 7. The user reads the number, not its column. A fixed text start would push the
  widest line past column 22 or leave the short lines off center.
- Saturation: the indexes 65534 and 65535 both show L65535 (D-T5). With the earned index 65535 and the selected
  index 65534, the line is `<  L65535 of 65535  >`. One `next` then only removes `>`. Accepted: this case needs
  65535 passed sessions.

#### 5.16.3 Description line (UX-54)

Row 5, 5 x 8 font, normal video, centered: start column = floor((25 - length) / 2). It describes the selected
level in short words (user decision 2026-10-07). The level model is `training_spec.md` section 6.

| Selected level | Text | Examples |
| --- | --- | --- |
| L1 (index 0, K and M mixed) | `K and M` | `K and M` (column 9) |
| Character level, index 1 and up | `New char ` and the text of the new character, `koch_char(koch_learned_count(L) - 1)`, through `morse_format()` in the style of the `Prosigns` setting. This is the full text of the item line (section 5.11.2), not the short label text of the keyboard. | `New char R` (L2), `New char J` (L20), `New char .`, `New char <BT/=>` (`Both`), `New char <BT>` (`Prosign`), `New char =` (`Punctuation`), `New char <SK>` |
| Word level, and the final word phase (L64 and up) | `Words of <len>`, `len` = `koch_word_length(L, Max Word Length)` | `Words of 2` (L3), `Words of 4` (L18 with `Max Word Length` 5), `Words of 8` |

- Longest text: `New char <AR/+>` and `New char <BT/=>` (15 cells, column 5).
- The final word phase uses the same text as the word levels (user request: decide the text). Reasons: the user
  hears or keys the same kind of item. The start screen header shows the item count of the level (`1/11`). There is
  no new string and no extra branch: the kind of the level selects the text. On the end screen, the header shows
  the session that ended, so the item count of the selected final level shows only when the session starts. This is
  accepted.
- `Words of 4` means words of 4 characters. The user manual explains it (`technical-writer`).

#### 5.16.4 Selector controls (UX-55)

The same table applies to the start screen and the end screen, in both modes. Tap mode is off on both screens, so
every press acts at the press edge (section 6.2).

| Event | Effect |
| --- | --- |
| `prev` press or repeat | The selected level goes down by one. At L1: nothing. |
| `next` press or repeat | The selected level goes up by one. At the earned level: nothing. |
| `enter` press, or a paddle tip closure | The session starts at the selected level. Then the start pause (sections 5.12.2 and 5.13.1). |
| `menu/back` press | The main menu, with the cursor on the mode item (`Receiving` or `Sending`). No EEPROM access. The selection is lost. |
| Long `menu/back` | Nothing. The press of the same hold already opened the main menu. |
| Long `prev`, long `next` | Nothing. The repeats of the same hold act. |
| Long `enter` | No event (tap mode is off). |
| Ring (P5.5) | Nothing. |

"Nothing" means: no state change, no redraw, no dirty page, no sound (section 6.1).

- Repeat: the first repeat comes 400 ms after the press, then one every 80 ms (section 6.2): 12.5 levels per
  second. From L1, a hold of `next` reaches L20 with the press and 18 repeats, at 1760 ms. The repeats stop at a
  limit. They never wrap (UX-13). A held `prev` or `next` also gives one long event at 800 ms. It does nothing, and
  the repeats continue.
- No sound for a step. The new number is the feedback. At a limit, the missing marker is the feedback.
- Receiving: tap mode goes off when the start screen shows and when the end screen shows. It goes on at the session
  start. `buttons_set_tap_mode()` drops a press in progress, so a button that is held across the switch gives no
  event until it is released and pressed again. The `enter` press or tip closure that starts the session gives no
  tap at its release and no long `enter` after 800 ms (section 6.2).
- Sending: the keyer starts at the session start and stops when the end screen shows (section 5.13.4). On the
  start screen and the end screen, P3.2 is `enter`.
- The `enter` press that opens the mode from the main menu gives one event only, in the menu. So it shows the start
  screen and does not start a session.
- The mode entry always preselects the earned level. A selection on the end screen is lost when the user leaves.
- A long `menu/back` in a session still goes to the main menu (UX-49), not to the start screen.

Redraw budget (LCD pages, section 5.11, user requirement: a selector change redraws as few pages as possible):

| Event | Start screen | End screen |
| --- | --- | --- |
| A step (`prev` or `next`) | Pages 0 (header), 4 (selector) and 5 (description) | Pages 4 and 5 |
| A press at a limit | None | None |
| The screen shows | All pages | All pages |
| The session starts from the start screen | Pages 1-7 (the session screen). Page 0 is already right. | All pages (the header changes to the new session) |

- Each step redraws all of rows 4 and 5, blank cells included, because the text can move by one column.
- The start screen header changes at each step: the level always, the item count at a change of the level kind.
  The tag and the speeds do not change, but they share page 0.
- During auto-repeat, a redraw can be slower than 80 ms. Then the screen skips an intermediate level, but no step is
  lost, and the last level shows within 100 ms after the release (UX-16, D-35).

#### 5.16.5 Engine facts that this section assumes

`training_spec.md` (owner `architector`) gives the engine interface. This section assumes the facts below. A-L1 to
A-L3 match D-T9, D-T36 and D-T38. If the interface differs, report it to `architector`.

| ID | Assumption |
| --- | --- |
| A-L1 | `progress_level()` reads the record image in xdata (fact, `training_spec.md` D-T9). So the main menu reads it at every row draw with no EEPROM access, and after a failed save (F-16) the earned level is the new level in RAM. |
| A-L2 | The engine tells the end screen which pass it was: a pass that advanced the earned level (saved, or `Not saved`), a replay pass (`TRAINING_OUTCOME_PASS_REPLAY`), a pass at index 65535 (`TRAINING_OUTCOME_PASS_TOP`), or a fail. Row 3 maps them as in section 5.15. |
| A-L3 | `training_select_step()` returns false when nothing changed (a limit). The mode then redraws nothing. |
| A-L4 | The start screen can get the item count of the selected level (`koch_item_count()`) for the header. |
| A-L5 | A pass at index 65535 preselects index 65535 (`level + 1` saturated). A pass at index 65534 below an earned index 65535 is a replay and preselects 65535. |

#### 5.16.6 Main menu level (UX-57)

`Receiving` and `Sending` show the earned level at the current speed setting: Receiving at `RX Speed`, Sending at
`TX Speed`. `Free Input`, `Settings` and `About` show nothing new.

```
    0         1         2
    0123456789012345678901234
0  |CW Trainer               | title bar, inverted
1  |Receiving           L12  | selected: highlight x 0-127, 'L12' inverted with the label
2  |Sending              L1  | 'L1' in columns 21-22, normal video
3  |Free Input               |
4  |Settings                >|
5  |About                    | no marker: it opens a screen, not a list (UX-61)
6  |                         |
7  |                         |
```

- Text: `L<n>`, `n` = the earned index plus 1, saturated at 65535 (D-T5). 2 to 6 cells.
- Position: right-aligned, last character in column 22: the value column of the lists (section 4.3). `L65535` uses
  columns 17-22. `Receiving` ends in column 8, so at least 8 blank cells separate the label and the level.
- Video: the same as a value of a value item (section 5.2). On the selected row, the band covers the label and the
  level, so both are inverted. On another row, both are normal video.
- Read-only: `enter` opens the mode, as before. There are no edit markers and no edit mode.
- Update: every draw of the row reads the level (A-L1). The main menu draws its rows when it shows, and the cursor
  move redraws the two rows of the move. So the menu shows the new level after a passed session, after
  `Reset Statistics`, and after a change of `RX Speed` or `TX Speed`. No extra redraw is necessary.
- Page budget: the level is part of its row. A cursor move still dirties two pages.
- Each character speed has its own level (progress key of the plan). After a change of `RX Speed`, the menu can show
  `L1` although the user trained at another speed. The level of the other speed stays stored and shows again when
  the user sets that speed back. The user manual explains it (`technical-writer`).

### 5.17 Startup screen (decision UX-58 to UX-60, UX-63)

Purpose: each power-on shows the product name, the mod and the firmware version for 5 s. Then the main menu shows.
The user can skip the screen with any button. Source: the plan "Firmware version, startup screen and About item"
and the mockup of the user (2026-10-08).

```
    0         1         2
    0123456789012345678901234
0  |                         | blank
1  |                Putikeeg | columns 16-23, right-aligned to column 23
2  | CW Trainer              | 8 x 16 font, cell top-left (7, 16), pages 2 and 3
3  |                         |   (the 8 x 16 line is 80 px wide: this grid drawing is illustrative)
4  | mod by UT2UBM           | columns 1-13
5  |                         | blank
6  |                         | blank
7  |                  v0.1.0 | right-aligned to column 23: columns 18-23 for "v0.1.0"
```

#### 5.17.1 Layout (UX-58)

All text is in normal video. The screen has no title bar and no highlight band.

| String | Font | Cell top-left (x, y) | Cells and pixels | LCD page | Alignment |
| --- | --- | --- | --- | --- | --- |
| `Putikeeg` | 5 x 8 | (82, 8) | row 1, columns 16-23, x 82-121 | 1 | Right: the last character in column 23. |
| `CW Trainer` | 8 x 16 (`FONTS_ID_8X16`) | (7, 16) | 10 cells of 8 px, x 7-86, y 16-31 | 2 and 3 | Left: x 7. |
| `mod by UT2UBM` | 5 x 8 | (7, 32) | row 4, columns 1-13, x 7-71 | 4 | Left: column 1. |
| `v` + `FIRMWARE_VERSION` | 5 x 8 | (122 - 5n, 56), `n` = length of the text. `v0.1.0`: (92, 56) | row 7, columns `24 - n` to 23 | 7 | Right: the last character in column 23 (UX-63). |

Pixel facts (from a decode with `firmware/Tests/tools/gen_font_golden.py`, section 3.3.1):

- `Putikeeg`: ink in y 8-15 (`k` uses cell row 0, `g` uses cell row 7), x 82-120.
- `CW Trainer`: ink in y 18-27 (cell rows 2-11), x 7-85. The text has no descender. So y 16-17 and y 28-31 are off.
- `mod by UT2UBM`: ink in y 32-39 (`d` and `b` use cell row 0, `y` uses cell row 7), x 7-71.
- `v0.1.0`: ink in y 57-62, x 92-120.
- The left ink edge is x 7 on both left lines. The right ink edge is x 120 on both right lines. So the blank margin
  is 7 px on both sides (x 0-6 and x 121-127).
- Pages 0, 5 and 6 have all pixels off. Pixels with x = 0, 1 and 127 are off in all rows.
- The 8 x 16 line fills whole pages 2 and 3, as the item line of the training screens fills pages 3 and 4
  (section 5.11.2).

The screen does not change while it shows: no countdown, no animation, no sound. It is drawn once.

#### 5.17.2 Time and skip (UX-59)

| Name | Value | Use |
| --- | --- | --- |
| `MENU_SPLASH_MS` | 5000 ms | The time from `menu_init()` to the end of the startup screen. Tests can set another value. User decision 2026-10-08 (UX-59). |

- The time counts on the 1 ms tick from `menu_init()`. The screen ends at the first `menu_poll()` that sees an
  elapsed time of 5000 ms or more. Tolerance: +/- 2 ms against the tick, plus one main-loop pass.
- Skip: the first button event of any kind ends the screen at once. This is a press, a repeat or a long event of
  `menu/back`, `prev`, `next` or `enter`. The paddle tip is `enter` here.
- The skip event does nothing else. It does not move the cursor, open an item or close the message that comes next.
- The events after the skip event act on the next screen as usual. If the user keeps `next` held, its repeats from
  400 ms move the cursor in the main menu, the same as in every list. A held `prev` stops on `Receiving` (no wrap on
  a repeat, section 6.3). The long event of the hold does nothing in a list.
- A button that is held at power-on gives no event until the user releases it and presses it again (section 6.2).
  So it does not skip the screen. This includes the held `menu/back` of the display recovery gesture (section 8.1).
- The keyer is off. Tap mode is off. Nothing sounds. A plug insertion can give one `enter` (D-5). It only skips the
  screen.

End of the screen:

- Normal start: the main menu with the cursor on `Receiving`. The main menu is drawn in full.
- Display recovery start: the `Display Defaults` message (section 5.8, UX-60). When the user closes the message, the
  main menu shows.

#### 5.17.3 Display recovery start order (UX-60)

The order is: the startup screen, then the `Display Defaults` message, then the main menu. Reasons:

- The default contrast and backlight apply before the first frame (section 8.1). So the startup screen is the first
  readable frame, and it shows the user that the recovery worked.
- The message must stay until the user closes it. If the message came first, the startup screen comes after the
  `OK` and delays the main menu by 5 s.
- Every start has the same first screen. One sequence is simpler to test and to explain.
- The skip event does nothing else (UX-59). So a press that skips the startup screen never closes the message by
  accident. The user always sees the message.

Rejected: the message first and then the startup screen (5 s of delay after the user closed the message). No startup
screen on a recovery start (two start sequences for a rare path, and the user does not see the version).

#### 5.17.4 Version text (UX-63)

- `FIRMWARE_VERSION` is `MAJOR.MINOR.PATCH`: digits and dots only, ASCII (UX-11). It is `0.1.0` in this version.
- Maximum length: 17 characters. The `About` row `Version ` + version then ends in column 24 at most (section 5.18).
  The startup text `v` + version is 2 to 18 cells, from column 6 at the earliest.
- Start column of the startup text: `24 - n`. Examples: `v0.1.0` (n = 6) column 18, x 92. `v10.12.3` (n = 8)
  column 16, x 82. 18 cells: column 6, x 32.
- A longer version must stop the build. A cut text on the glass is a defect (section 4.1). `architector` decides
  where the build checks the length (the CMake check of `firmware/VERSION` or a compile-time check in C).
- Both screens build the text at compile time. The width is a constant, so the right alignment needs no run-time
  measure.

Reason for column 23 on the startup screen, not column 24 or 22: the screen has no title bar and no value fields.
Column 1 on the left and column 23 on the right give the same blank margin on both sides. The mockup of the user
also has blank space on both sides: a blank cell on the left of the left lines, and blank cells on the right of the
right lines.

### 5.18 About screen (decision UX-61, UX-62)

Purpose: the user can read the name, the mod, the firmware version and the project link at any time. The text
comes from the layout that the user chose (2026-10-08).

#### 5.18.1 `About` item in the main menu (UX-61)

- The fifth item of the main menu, after `Settings`, in row 5 (section 5.16.6). Label `About`. Kind: info screen.
- No marker and no value. The `>` marker means "opens a list" (section 5.2). `Free Input`, `Receiving`, `Sending`
  and `Reset Statistics` open a screen and have no marker. `About` also opens a screen.
- Cursor: `prev` on `Receiving` wraps to `About`. `next` on `About` wraps to `Receiving`. A held `next` stops on
  `About` (section 6.3).

Rejected: `>` on the `About` row (it promises a list). A `...` suffix (no other item uses it).

#### 5.18.2 Layout (UX-62)

```
    0         1         2
    0123456789012345678901234
0  |About                    | title bar, inverted
1  |Putikeeg CW Trainer      | columns 0-18
2  |mod by UT2UBM            | columns 0-12
3  |Version 0.1.0            | columns 0-12: "Version " + FIRMWARE_VERSION
4  |                         | blank
5  |https://github.com/Quard/| columns 0-24
6  |putikeeg_cw_trainer__    | columns 0-20
7  |ut2ubm                   | columns 0-5
```

| Row | Text | Cell top-left (x, y) | Columns | Video |
| --- | --- | --- | --- | --- |
| 0 | `About` | (2, 0) | 0-4 | Inverted title bar, x 0-127 (section 5.1). |
| 1 | `Putikeeg CW Trainer` | (2, 8) | 0-18 | Normal. |
| 2 | `mod by UT2UBM` | (2, 16) | 0-12 | Normal. |
| 3 | `Version ` + `FIRMWARE_VERSION` | (2, 24) | 0-12 for `0.1.0`, at most 0-24 (UX-63) | Normal. |
| 4 | none | - | - | All pixels off. |
| 5 | `https://github.com/Quard/` | (2, 40) | 0-24 | Normal. |
| 6 | `putikeeg_cw_trainer__` | (2, 48) | 0-20 | Normal. |
| 7 | `ut2ubm` | (2, 56) | 0-5 | Normal. |

- The text is left-aligned in column 0, not centered as on the message screen. Reason: the link uses three rows.
  One left edge makes the three rows read as one text, and rows 1-3 keep the same edge.
- The link is `https://github.com/Quard/putikeeg_cw_trainer__ut2ubm`. It breaks after column 24 and after `__`.
  No hyphen is added, because a hyphen changes the link. The user manual gives the full link
  (`technical-writer`).
- The screen has no highlight band and no `OK` button. It has no choice, and the link uses the rows that a button
  needs. The title tells the user where they are. `enter` and `menu/back` close the screen, the same as on the
  message screen.
- The text is static. When the screen opens, it is drawn in full. After that, it does not change.
- Pixels with x = 0, 1 and 127 are off in rows 1-7.
- `Putikeeg CW Trainer` and `mod by UT2UBM` are the same names as on the startup screen.

#### 5.18.3 Controls (UX-62)

| Event | Effect |
| --- | --- |
| `enter` press (also the paddle tip) | The main menu, cursor on `About`. |
| `menu/back` press | Same as `enter`. |
| `prev`, `next`: press and repeat | Nothing. |
| Long `menu/back`, `prev`, `next` | Nothing. |
| Long `enter` | No event (tap mode off). |

- "Nothing" means the same as in section 6.1: no state change, no redraw, no dirty page, no sound.
- The keyer is off. Nothing sounds on this screen.
- The `enter` press that opens `About` acts at the press. Its release gives no event. So the screen stays open until
  the next press.

## 6. Control mapping

### 6.1 Per state (decision plan + UX-9, UX-23, UX-24, UX-59, UX-62)

`press` = a debounced press event. `repeat` = an auto-repeat event (prev and next only, section 6.2). `long` = a
long-press event (`menu/back`, `prev` and `next`, and `enter` in tap mode only, section 6.6). Tap mode is on only in
a Receiving session, from the session start to the session end (section 6.8). The start screen and the end screen
keep it off (UX-55).

Short presses and repeats:

| State | `prev` press | `next` press | `prev`/`next` repeat | `enter` | `menu/back` |
| --- | --- | --- | --- | --- | --- |
| List, browse | Cursor up. Wraps from first to last | Cursor down. Wraps from last to first | Same as press, but stops at the first/last item (no wrap) | Mode: open the mode screen (Receiving, Sending or Free Input). Submenu: open the list, cursor on its first item. Value/choice: enter edit mode. Action: open the dialog. Info screen: open the `About` screen (section 5.18). | Main menu: nothing (no redraw). Settings list: go to the main menu with the cursor on `Settings`. Save under the rule in section 7 (UX-15). CW or Display list: go to Settings with the cursor on `CW` or `Display`. No save. |
| List, edit | Decrease the value by one step, or the previous choice. Stops at the limit. Applies at once. | Increase, or the next choice. Stops at the limit. Applies at once. | Same as press, stops at the limit | Leave edit mode, keep the value. Cursor stays on the row. | Leave edit mode, keep the value (same as `enter`). Does not leave the list. |
| Placeholder (no screen uses it in this version) | nothing | nothing | nothing | nothing | nothing |
| Free Input | nothing | nothing | nothing | No event. The keyer owns P3.2: the `enter` button keys like the tip contact (section 6.7). | nothing |
| Receiving: Item (playing or waiting) | Run 1 (second from the left), at the release (tap mode, section 6.8). | Run 2, at the release | none (tap mode) | Run 3 (the right run), at the release. Also a short tip closure. | Run 0 (the left run), at the release |
| Receiving: start pause, result, blank moment | nothing | nothing | none (tap mode) | nothing | nothing |
| Sending: all states but the end screen | nothing | nothing | nothing | No event. The keyer owns P3.2 (section 5.13.4). | nothing |
| Training start screen and session end screen (both modes, section 5.16.4) | Selected level down by one. Stops at L1. | Selected level up by one. Stops at the earned level. | Same as press, stops at the limit (no wrap) | Start a session at the selected level, at the press. The paddle tip does the same. | Main menu, cursor on the mode item, at the press. |
| Dialog | Focus `No` (no wrap) | Focus `Yes` (no wrap) | Same as press | Activate the focused button. `No`: back to Settings. `Yes`: reset statistics, show the message screen. | Same as `No`. |
| Message | nothing | nothing | nothing | `Statistics reset`: back to Settings, cursor on `Reset Statistics`. `Display Defaults`: the main menu, cursor on `Receiving`. | Same as `enter`. |
| Startup screen (section 5.17) | End the startup screen. Nothing else. | Same as `prev`. | Same as a press: end the startup screen, nothing else. | End the startup screen, at the press. Nothing else. The paddle tip does the same. | Same as `enter`. |
| About (section 5.18) | nothing | nothing | nothing | The main menu, cursor on `About`. The paddle tip does the same. | Same as `enter`. |

Long presses:

| State | Long `menu/back` | Long `enter` | Long `prev` | Long `next` |
| --- | --- | --- | --- | --- |
| List (browse or edit), dialog, message | nothing | no event (tap mode off) | nothing | nothing |
| Placeholder (no screen uses it in this version) | Back to the main menu, cursor on the mode item that opened it. | no event | nothing | nothing |
| Free Input | Stop the keyer (a sounding tone falls in 8 ms). Back to the main menu, cursor on `Free Input`. The text is lost. | no event (the keyer owns P3.2) | Clear the text area (section 5.9). | Draw the delimiter (section 5.9). |
| Receiving: Item | Stop the player. Drop the session. Main menu, cursor on `Receiving` (section 6.8). | Replay from the start (section 6.8). Also a tip closure of 800 ms. | Undo one step (section 6.8). | nothing |
| Receiving: start pause, result, blank moment | Same as above. | nothing | nothing | nothing |
| Sending: all states but the end screen | Stop the keyer. Drop the session. Main menu, cursor on `Sending` (section 6.9). | no event (the keyer owns P3.2) | nothing | nothing |
| Training start screen and session end screen (both modes) | Nothing: the press of the same hold already opened the main menu. | no event (tap mode off) | nothing (the repeats of the hold act) | nothing (the repeats of the hold act) |
| Startup screen | End the startup screen. Nothing else. In practice, the press of the same hold already ended it. | no event (tap mode off) | Same as long `menu/back`. | Same as long `menu/back`. |
| About | nothing | no event (tap mode off) | nothing | nothing |

"nothing" means: no state change, no redraw, no dirty page, no sound. In a list, the short press already acted on the
press edge. The long event of the same press must not act again (UX-24).

Other rules:

- "Applies at once": contrast goes to the LCD and the backlight level goes to the PWM at each step. The other
  values change only in memory until the save. In the menu, Tone is the only value with an audible effect: the Tone
  preview (section 6.5). The mode settings take effect at the next mode start:
  - Free Input: `Key Type`, `Paddle`, `TX Speed`, `TX Eff. Speed`, `Tone`, `Prosigns` (section 6.7).
  - Receiving: `RX Speed`, `RX Eff. Speed`, `Tone`, `Prosigns`, `Max Word Length` (section 5.12).
  - Sending: `Key Type`, `Paddle`, `TX Speed`, `TX Eff. Speed`, `Tone`, `Prosigns`, `Show CW Code`,
    `Max Word Length` (section 5.13).
  - A new session from the end screen keeps the settings of the mode start. The settings cannot change in a mode.
- Effective-speed rule (plan): an effective speed can never be higher than its base speed. Lowering a base speed
  below its effective speed lowers the effective speed in the same step. Raising a base speed never changes the
  effective speed.
- A list opened from its parent always starts with the cursor on its first item. Going back restores the cursor of
  the parent on the item that was opened.
- After power-on the startup screen shows (section 5.17). Then the main menu shows with the cursor on `Receiving`.
  On a display recovery start, the `Display Defaults` message comes between the two (section 8.1, UX-60).
- After the startup screen ends, the events of the same hold act on the next screen. Example: the repeats of a held
  `next` move the main menu cursor (UX-59).
- There is no inactivity timeout anywhere in this version. Free Input, the training start screen, a training session
  and the session end screen stay open until the user leaves. The Tone preview timeout (section 6.5) is not a menu timeout: it ends the
  sound and changes no screen. The training delays (section 6.10) are steps of a session, not timeouts.
  The 5 s of the startup screen (section 5.17) are a step of the start-up, not a timeout.
- Long presses act only in the mode screens (section 6.6). Lists, edit mode, the dialog and the message screen keep
  their short-press behaviour without a change.
- Short presses act in the mode screens only in Receiving (the keyboard, UX-43), on the training start screen and on
  the session end screen (UX-55). Free Input and the session states of Sending ignore them (UX-24).

### 6.2 Button timing (decision UX-12)

| Parameter | Value | Notes |
| --- | --- | --- |
| Debounce, press | Pin low without interruption for 20 ms. The press event comes at that moment. | Bounce time of the buttons and the paddle is unverified (D-4). |
| Debounce, release | Pin high without interruption for 20 ms. | A bounce on release never gives a second press. |
| Auto-repeat buttons | `prev` and `next` only | A held `enter` or `menu/back` gives one event per press. |
| First repeat | 400 ms after the press event | |
| Repeat period | 80 ms (12.5 steps per second) | Full Tone range: 60 steps, about 5.1 s. Full Contrast range: 63 steps, about 5.4 s. No acceleration. |
| Two of prev/next held | The repeat belongs to the one pressed last. Releasing it stops the repeat. The other one does not start repeating again until it is pressed again. When both presses debounce in the same poll, the repeat belongs to prev. | |
| Button held at start-up | Gives no event until it is released and pressed again. | Protects against a closed key/paddle at power-on. After the display recovery gesture (section 8.1), the held `menu/back` does not close the message screen. |
| Two press events due in the same poll | Order: `menu/back`, `enter`, `prev`, `next`. | Deterministic for tests. |
| Long press (UX-23) | `BUTTONS_LONG_PRESS_MS` = **800 ms** after the press event, while the debounced state stays pressed. One long event per press. Buttons: `menu/back`, `prev`, `next`, and `enter` in tap mode only (UX-51). The hold counts on the debounced state. Both of its edges lag the pin by 20 ms, so a pin hold and a debounced hold have the same length. Measured from the press event, the pin must still be closed at 780 ms. | With tap mode off, `enter` has no long press: it is the paddle tip, and its event comes at the press. In tap mode (Receiving), a tip closure of 800 ms is a long `enter`. A release bounce shorter than 20 ms does not end the hold (debounced state). |
| Long press and repeat | A held `prev` or `next` gives its repeats and also one long event. Each screen uses only one of them: lists and the training start and end screens use repeats, the other mode screens use the long event. | The repeat at 800 ms (400 + 5 x 80) and the long event fall due together. |
| Events due in the same poll | Order: press events (`menu/back`, `enter`, `prev`, `next`), then long events (`menu/back`, `enter`, `prev`, `next`), then a repeat. A later event is not lost: it comes on a later poll. | Deterministic for tests. Long `enter` comes only in tap mode (`training_spec.md` D-T28). |
| Long press and start-up | A button held at start-up gives no long event until it is released and pressed again. | Same rule as the press event. |
| Tap mode (Receiving session only, UX-43, UX-51, UX-55) | `buttons_set_tap_mode(true)`: all four buttons, `menu/back`, `enter` (button or paddle tip), `prev` and `next`, give their press event at the debounced release, and only for a release before `BUTTONS_LONG_PRESS_MS`. A hold gives only the long event (long `enter` = `BUTTON_EVENT_ENTER` with `BUTTON_EVENT_LONG_FLAG`). Nothing at the release of a hold. No repeat. A press that started before tap mode went on gives no event, also no long event (the `enter` that opens Receiving). | `training_spec.md` D-T7, D-T28. All other screens keep tap mode off, also the Receiving start screen and end screen (UX-55). |
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
  edit mode. `menu/back` undoes that without a change. If it opens Free Input, a long `menu/back` closes it.
- In Free Input, P3.2 belongs to the keyer, not to the menu (UX-33, section 6.7). One closure of P3.2 is never both
  `enter` and a key element.
- In Receiving, the tip is `enter` in tap mode (section 6.8). A closure that opens before 800 ms is an `enter` tap:
  in Item it selects the right run. A closure of 800 ms or longer is a long `enter`: in Item it replays the item.
  The action comes at the release for a tap, and at 800 ms for a hold.
- A plug insertion in Receiving can give one `enter` tap (F-20, D-5). In Item it selects the right run. If the run
  has 2 or more characters, it narrows: a long `prev` steps out again. If the run has one character, it picks: a
  long `prev` takes the pick back. If that pick fills the last slot, the check comes at once and can use an
  attempt. No undo exists for a check. The user accepts this (`training_spec.md` D-T27, R-T7). The manual says:
  insert the plug before you open Receiving.
- On the start screen and the end screen of both modes, tap mode is off and P3.2 is `enter` at the press. A plug
  insertion there can start a session at once (F-20, F-21). A long `menu/back` then drops the session.
- In a Sending session, P3.2 belongs to the keyer. On the start screen and the session end screen, it is `enter`
  (section 5.13.4).

### 6.5 Tone preview (decision UX-20, user Q-3)

User decision (Q-3): a Tone edit plays the tone. This is the only sound in the menu. The keyer sounds only in Free
Input (section 6.7). Outside Free Input the keyer is off, and the paddle gives no tone (UX-34).

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

### 6.6 Long press (decision UX-23, UX-24, UX-36, user)

User decision: in every mode screen, a long `menu/back` exits, a long `prev` clears the screen and a long `next`
draws a delimiter. The placeholders use only the long `menu/back`.

The training modes follow the same rule (plan, user): a long `menu/back` exits from every training screen. In
Receiving, a long `prev` undoes one step of the answer (section 6.8). This is the clear of Receiving. Sending gives
long `prev` and long `next` no action. Receiving gives long `next` no action. On the start screen and the session end
screen, a `menu/back` press already leaves, and long events do nothing (section 5.16.4, UX-55).

Long `enter` (user decision 2026-10-06, UX-51): in Receiving, a long `enter` replays the item (section 6.8). It
exists only in Receiving, because only Receiving turns tap mode on. A tip closure of 800 ms is a long `enter` too.
On the other screens, `enter` keeps its event at the press and has no long event.

| Item | Value |
| --- | --- |
| Threshold | 800 ms after the press event (section 6.2). The user holds the button about 0.8 s: the press event and the release both lag the pin by 20 ms. |
| Moment of the action | At the threshold, while the button is still held. Not at the release. |
| Count | One action per press. A longer hold does nothing more. |
| Feedback | The result of the action is the only feedback: the main menu appears, the text area goes blank, the delimiter line appears, the answer goes back one step, or the item plays again (Receiving, long `enter`). There is no beep, no progress mark and no flash. |
| Short press, repeat and `enter` in a mode screen | Ignored: no state change, no redraw, no sound (section 6.1). Exceptions: the Receiving keyboard (section 6.8), the training start screen and the session end screen (section 5.16.4). |

How the user learns it: hold the button until the screen changes, then release. The screen changes during the hold.
This tells the user the moment to let go.

Special cases:

- Clear on an empty text area changes nothing on the screen. That is correct: there is nothing to clear.
- After the exit, the button is still held. The menu gets no event from it: the long event comes once per press, and
  `menu/back` has no repeat.
- `prev` and `next` held together: each gives its own long event at its own 800 ms moment, in that order. In Free
  Input this gives a clear and a delimiter.
- A long press while the user keys: the keyer runs on. Only the clear drops the character in progress
  (section 5.9).
- A button held at start-up gives no long event (section 6.2).

### 6.7 Keyer from the user's view (decision plan, UX-33 to UX-35, UX-37)

The keyer runs only in Free Input and in a Sending session (section 5.13.4). The mode start starts the keyer, and
the mode end stops it. In Sending, the session start starts it, and the session end screen stops it. The start
screen does not run it. At the mode start the keyer reads `Key Type`, `Paddle`, `TX Speed`, `TX Eff. Speed` and `Tone` once. Free Input also reads `Prosigns`
once at the mode start (UX-38). The settings cannot change while the mode is open. `RX Speed`, `RX Eff. Speed`, `Show CW Code` and `Max Word Length` have no effect on Free Input.

#### Key Type

| Value | What the user does | What the device does |
| --- | --- | --- |
| `Straight` | Keys with a straight key, a bug, or one paddle lever as a key. | The tip contact and the ring contact both key. The tone sounds while a contact is closed. A mark shorter than 2 dits is a dit. A mark of 2 dits or longer is a dah. `Paddle` has no effect. |
| `Iambic A` | Keys with a dual-lever paddle. | The dit lever gives dits and the dah lever gives dahs, for as long as the lever is held. Lengths are exact: dah = 3 dits, gap = 1 dit, at `TX Speed`. A squeeze gives alternate elements. A squeeze from idle starts with a dit. When the user opens both levers during a squeeze, the keyer ends the current element and stops. A single tap on the other lever during an element adds that element. |
| `Iambic B` | Same as `Iambic A`. | Same as `Iambic A`, with one difference: when the user opens both levers during a squeeze, the keyer adds one more element of the other kind. |

The exact state machine is in `keyer_spec.md` (owner `architector`).

#### Speed and gaps

- `TX Speed` sets the dit length: 1200 / WPM ms (5 WPM = 240 ms, 20 WPM = 60 ms, 50 WPM = 24 ms).
- Character end: when the keyer is idle for 2 dits after the last element, the character shows on the screen.
- Word space: when the pause reaches 5 units of the `TX Eff. Speed` spacing, a word space follows. If
  `TX Eff. Speed` equals `TX Speed`, this is 5 dits. A lower `TX Eff. Speed` needs a longer pause. The exact formula
  is in `keyer_spec.md`.
- In a Sending word item, this word space is an error: the pause between two characters of a word must stay
  shorter (section 5.13.1, UX-52). Free Input and Sending use the same threshold.
- User decision: the thresholds come from the settings. The decoder does not adapt to the speed of the user.

#### Sidetone

- Pitch: the `Tone` setting. Route: automatic (O-3).
- `Straight`: the tone follows the contact. It rises in 8 ms after the close and falls in 8 ms after the open.
- `Iambic A` and `Iambic B`: the tone starts with the element. Its 8 ms fall ends at the end of the element, so the
  element and the gap keep their exact lengths.
- The display never delays the tone: the keyer runs in the timer interrupt.

#### Paddle (decision UX-32)

| `Paddle` | Tip (P3.2, also the `enter` button) | Ring (P5.5) |
| --- | --- | --- |
| `Normal` | dit | dah |
| `Reversed` | dah | dit |

With `Reversed`, the `enter` button keys dahs in an iambic mode. With `Straight`, both contacts key, so the setting
has no effect.

#### P3.2 ownership (decision UX-33)

The same rules apply to a Sending session (section 5.13.4). There, "mode start" means the start of a session
from the start screen or the end screen, and "mode end" also means the session end screen. In Receiving, P3.2 stays with the
menu: the tip is `enter` (a tap selects the right run, a hold of 800 ms replays).

- Mode start: the `enter` press that opens Free Input gives no element and no tone. This is true for the `enter`
  button and for a dit lever that acts as `enter`. At the mode start, a closed contact stays ignored until it
  opens. After that it keys as normal. The ring contact follows the same rule, so a mono plug (ring shorted to
  the sleeve) works as a straight key on the tip.
- Inside the mode: the menu gets no `enter` event. The `enter` button keys like the tip contact.
- Mode end: after the long `menu/back`, the menu gets P3.2 back. A contact that is held at that moment gives no
  `enter` until it opens and closes again. So a held lever never opens a menu item.
- One closure of P3.2 is never both `enter` and a key element.

### 6.8 Receiving buttons (decision UX-43, UX-51)

Receiving uses short presses as keyboard keys. This is the one mode screen where short presses act. UX-43 refines
UX-24 for Receiving. UX-51 gives the keyboard all four buttons (user decision 2026-10-06).

Tap mode (`training_spec.md` D-T7, D-T28, `buttons_set_tap_mode()`): Receiving turns it on at the session start
(the `enter` press on the start screen or the end screen) and off when the end screen shows. The start screen and the
end screen have tap mode off. Their buttons act at the press (section 5.16.4, UX-55). In tap mode, all four buttons
work as follows: `menu/back`, `prev`, `next` and `enter`. `enter` includes the paddle tip (P3.2):

- A tap: a release before `BUTTONS_LONG_PRESS_MS` (800 ms after the press event). The press event comes at the
  debounced release, 20 ms after the pin opens.
- A hold: only the long event, at 800 ms. Nothing at the release.
- No repeat events.
- A press that started before tap mode went on gives no event. So the `enter` press (or tip closure) that starts a
  session gives no tap at its release and no long `enter` after 800 ms.
- Long events due in the same poll come in the order `menu/back`, `enter`, `prev`, `next` (section 6.2).

| Event | Start pause, blank moment, result | Item (playing or waiting) |
| --- | --- | --- |
| `menu/back` tap | Nothing | Run 0, the left run (section 5.12.1) |
| `prev` tap | Nothing | Run 1 |
| `next` tap | Nothing | Run 2 |
| `enter` tap (or a short tip closure) | Nothing | Run 3, the right run |
| Long `menu/back` | Exit | Exit |
| Long `enter` (or a tip closure of 800 ms) | Nothing | Replay: the item plays again from its start |
| Long `prev` | Nothing | Undo one step |
| Long `next` | Nothing | Nothing |

The start screen and the end screen are not in this table: tap mode is off there (section 5.16.4).

"Nothing" means: no state change, no redraw, no sound. An ignored event is not kept for later. A tap of an empty
run (section 5.12.1) is also nothing.

Why tap mode:

- A long `prev` and a long `menu/back` never act as a group press first. Without it, the press of a hold narrows a
  group or picks a character. A pick in the last slot then starts a check before the long event comes.
- A long `enter` never selects the right run first. Without tap mode, the press of the hold selects the run, and
  the replay comes only after it.
- The check can come at once at the last pick. No wait for a possible long event is necessary.
- The cost: a tap acts at the release, about 0.1 s later than at the press. Device check D-30 checks that a tap
  still feels prompt.

Replay (long `enter`):

- At 800 ms of the hold, the item plays again from its start. A running playback starts again. The release does
  nothing.
- The picks, the current group, the labels and the block stay. Replays are free (section 5.12.2).
- The replay is the only feedback: the screen does not change. The user lets go when the playback starts again.
- A replay is possible in Item only: playing or waiting, in every attempt.

Undo (long `prev`, `training_spec.md` section 12.2):

1. The current group is a run inside the set: step out one level. The current group becomes the run that it came
   from, and the labels of that level show.
2. Else, at least one slot is filled: the last filled slot becomes empty. The block moves to it.
3. Else: nothing.

The undo does not change the playback. One long `prev` is one step. To step out of two levels, the user holds `prev`
two times.

Exit (long `menu/back`):

- The player stops (a sounding tone falls in 8 ms). The session is dropped: the level does not change and the
  EEPROM is not accessed.
- The main menu shows with the cursor on `Receiving`. Tap mode goes off.

### 6.9 Sending buttons (decision UX-24, UX-45)

- Tap mode stays off in Sending.
- In all states of a session except the end screen, short presses and repeats do nothing (UX-24). `enter` gives no
  event: the keyer owns P3.2 and the `enter` button keys like the tip (section 5.13.4).
- Long `menu/back`: the keyer stops (a sounding tone falls in 8 ms), the session is dropped, the main menu shows with
  the cursor on `Sending`. The level does not change and the EEPROM is not accessed.
- Long `prev` and long `next`: nothing. Sending has no clear and no delimiter.
- Start screen and end screen (section 5.16.4): the keyer is off, and P3.2 is `enter`. `prev` and `next` (press
  and repeat) change the selected level. `enter` or the tip starts a session at the selected level. A `menu/back`
  press opens the main menu, cursor on `Sending`. Long events do nothing.

### 6.10 Training timing and layout constants (decision UX-39 to UX-57)

Names for the interface contract (`training_spec.md` section 5.10, U-1 to U-6). All times count on the 1 ms tick
with the `+/- 2 ms` tolerance of section 6.2. A visible change can lag by one main-loop pass and the LCD transfer
(unverified, D-29).

| Name | Value | Use |
| --- | --- | --- |
| `BUTTONS_LONG_PRESS_MS` | 800 ms | Existing (UX-23). Long `menu/back`, long `prev` and long `enter` (Receiving) in the training screens. The tap limit in tap mode, also for a tip closure. |
| `TRAINING_START_MS` | 1000 ms | From the session start (`Start` on the start screen or the end screen) to the first item. Both modes. |
| `TRAINING_CORRECT_MS` | 600 ms | `Correct` on the screen. |
| `TRAINING_WRONG_MS` | 1200 ms | `Wrong` after attempt 1 or 2, then the retry. |
| `TRAINING_FAILED_MS` | 2500 ms | The answer after attempt 3. Receiving: counts from the end of the answer playback. |
| `TRAINING_RECEIVING_GAP_MS` | 500 ms | The blank moment before the next Receiving item. |
| `TRAINING_SENDING_GAP_MS` | 300 ms | The blank moment before the next Sending item. |
| `TX_CODE_MAX_MARKS` | 12 | Marks drawn per character in row 7. |
| Pause limit in a Sending word | 5 Farnsworth units at `TX Eff. Speed` (`KEYER_WORD_GAP_UNITS`, existing) | The WORD_GAP event. No new constant (UX-52). |

| Name | Value | Use |
| --- | --- | --- |
| Header columns | tag 0, speed 3, level 9, item ends at 24 | Section 5.11.1. |
| Status row | 1 | 5 x 8, centered. |
| `TRAINING_ITEM_Y` | 24 | Cell top of the item line, `FONTS_ID_8X16`. |
| Character item x | `64 - 4m` | `m` = cells of the text, 1 to 6. |
| Word item x | `68 - 8n + 16k` | `n` = 2 to 8 characters, `k` = 0 to `n - 1`. |
| Block | x to x + 6, y 26-39 | Receiving, next slot. |
| Bar | x to x + 6, y 38-39 | Receiving empty slot, Sending next character of a word. |
| Answer code row | 5 | Sending, centered marks. |
| Key bars | y 55, x 2-26, 30-54, 58-82, 87-126 | Receiving, one above each label field. |
| Label fields | x 2-26 (5 cells, left), 30-54 (5, centered), 58-82 (5, centered), 87-126 (8, right) | Receiving, row 7, runs 0-3 = `menu/back`, `prev`, `next`, `enter` (section 5.12.1). |
| Marks | dot 2 x 2, dash 6 x 2, rows `8r + 3` and `8r + 4`, gap 3 | Row 7 from x 2, row 5 centered. |
| End screen | `Not saved` row 1, score row 2, result row 3, selector row 4, description row 5, `Start` row 7 (band x 47-81, y 57-63) | Section 5.15. |
| Start screen | header row 0 (selected level, `1/<n>`), rows 1-3 blank, selector row 4, description row 5, `Start` row 7 (band x 47-81, y 57-63) | Section 5.16.1. |
| Selector line | row 4, `<` column `c`, text `L<s> of <e>` from column `c + 3`, `>` column `c + len + 5`, `c = floor((19 - len) / 2)` | Section 5.16.2. |
| Description line | row 5, centered | Section 5.16.3. |
| Main menu level | `L<n>` right-aligned to column 22 on the `Receiving` and `Sending` rows | Section 5.16.6. |

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
| Long press in a mode screen | The result of the action at 800 ms, during the hold. Nothing else. | UX-23 |
| Short press or repeat in a mode screen | Nothing. Exceptions: the Receiving keyboard, the training start screen and the session end screen. | UX-24, UX-43, UX-55 |
| Key element in Free Input | The sidetone, at once. The screen does not show elements. | plan, UX-26 |
| Character keyed in Free Input | The glyph, or the prosign text of section 5.9, appears after the 2-dit pause. Target: visible within 100 ms after the character gap (unverified, D-21). | UX-26 |
| Pattern not in the table | `*` in its cell. | UX-27 |
| `Prosigns` edit | The value changes in the field. Nothing else changes in the menu, and nothing sounds. The new text shows at the next Free Input start. | UX-38 |
| Word space in Free Input | No visible change until the next character, which starts one cell later. | UX-26 |
| Save on leaving Settings | Nothing extra. The main menu appears with the cursor on `Settings`. The erase and write take about 5-7 ms (datasheet: erase 4-6 ms, 15 bytes x 40 us), which the user does not notice. | UX-15 |
| Save not needed | Same screen, no EEPROM access. A save needs two conditions: at least one edit step changed a value during this Settings visit, and the current values differ from the stored record. A value changed and changed back does not cause a write. A visit without an edit step does not cause a write, also after a display recovery start. | UX-15 |
| Reset Statistics `Yes` | The message screen `Statistics reset` / `OK`. | UX-8 |
| Receiving group tap | At the release of any of the four buttons (also a short tip closure for `enter`): the labels change (narrow), or the character appears in its slot and the block moves (pick). An empty run: nothing. | UX-42, UX-43, UX-51 |
| Receiving last pick | The answer is complete, the block goes away, and the result shows at once. | UX-43 |
| Receiving long `prev` | Nothing during the hold. At 800 ms the answer goes back one step. | UX-43 |
| Receiving replay | At 800 ms of a long `enter` (or of a tip closure), the item plays again. The screen does not change. | UX-44, UX-51 |
| Sending element | The sidetone at once. With `Show CW Code` = `On`, a dot or dash mark in row 7 at the end of the element. | UX-45, UX-50 |
| Sending correct character in a word | The bar moves to the next character. No text, no delay. | UX-45 |
| Result | `Correct` (600 ms), `Wrong`, `Wrong: N` or `Wrong: pause too long` (1200 ms), or the answer (2500 ms) in the status line. No sound, except the answer playback in Receiving. | UX-46, UX-52 |
| Sending pause too long in a word | `Wrong: pause too long` in the status line. The bar stays under the next character. It uses an attempt. | UX-52 |
| Retry | `Try 2 of 3` or `Try 3 of 3`. The item looks fresh: empty slots (Receiving), or the bar on the first character and row 7 off (Sending). | UX-46, UX-52 |
| Next item | The item line goes blank (500 ms in Receiving, 300 ms in Sending), and the header item number changes. Then the next item appears. | UX-46 |
| Session end | The end screen with the score, the result and the level selector. Row 3 says `Passed, new level` when the earned level went up, `Passed` after a replay pass. `Not saved` in row 1 after a failed save. | UX-47, UX-56 |
| Start screen | At each mode entry: the selector at the earned level, the description, `Start`. No sound. | UX-53 |
| Level step (`prev`, `next`, press or repeat) | The selector number and the description change. On the start screen, the header level and item count change too. No sound. | UX-55 |
| Level at a limit (L1, or the earned level) | The `<` or `>` marker disappears. More presses change nothing. | UX-54 |
| Main menu | `Receiving` and `Sending` show their earned level `L<n>` at the current speed. | UX-57 |
| Power-on | The startup screen: `Putikeeg`, `CW Trainer` in the large font, `mod by UT2UBM` and the version. After 5 s, the main menu. No sound. | UX-58, UX-59 |
| Button during the startup screen | The main menu at once (or the `Display Defaults` message on a recovery start). The button has no other effect. | UX-59, UX-60 |
| `About` | The `About` screen with the name, the mod, the version and the link. `enter` or `menu/back` goes back to the main menu with the cursor on `About`. | UX-61, UX-62 |
| Response time | Target: the change is visible within 100 ms after the press event (unverified, D-6). A redraw can be slower than 80 ms during auto-repeat. Then the screen skips an intermediate value, but no step is lost. The value advances once per event, and the last value shows within 100 ms after release. | UX-16 |

## 8. Start-up, faults and recovery

Every power-on starts with the startup screen (section 5.17, F-24). In this table, "the main menu" after a power-on
means: the main menu after the startup screen.

| ID | Condition | What the user sees | Device behaviour | Recovery |
| --- | --- | --- | --- | --- |
| F-1 | First power-on, empty EEPROM | The startup screen, then the main menu, no message. Settings show the defaults. | Defaults load. Nothing is written until the user changes a value and leaves Settings. | None needed. |
| F-2 | Record with a wrong magic, a layout version other than 1 or 2, or a wrong CRC (for example a power loss during a save) | Same as F-1, no message. | Defaults load. | The user sets the values again. |
| F-3 | Valid record with a value out of range | The clamped value. | Each field is clamped. An effective speed above its base speed becomes the base speed. | None needed. |
| F-4 | Power loss while inside Settings | After power-on, the values from before the visit. | Unsaved changes are lost (plan). | The user sets the values again. |
| F-5 | Contrast set to a value that makes the screen unreadable | Blank or black screen. | The value applies at once. If the user leaves Settings, it is saved. | During the edit: press the opposite button. After a save: the display recovery start (section 8.1). |
| F-6 | Backlight `Off` in the dark | No light. | Level 0 is a valid setting. | Same as F-5. |
| F-8 | `menu/back` held at start-up | The startup screen, the message screen `Display Defaults`, then the main menu. | Section 8.1, UX-60. | - |
| F-7 | EEPROM write fails (read-back mismatch, CMD_FAIL or low supply) | The message screen: title `Settings`, text `Not saved`, `OK`. `enter` or `menu/back` closes it to the main menu with the cursor on `Settings`. | `eeprom_write` reads back each byte. The save stops with `SETTINGS_SAVE_FAILED`. The values stay active. The magic is written last, so a stopped save leaves no valid record (`menu_subsystem_spec.md` sections 3.5 and 3.6). | The next exit from Settings tries the save again. If the user switches off first, the next power-on uses the defaults (F-2). |
| F-9 | A mono plug is inserted while Free Input is open (the ring closes after the mode start). | `Straight`: a tone without end and no new character. Iambic: a stream of dahs (`Normal`) or dits (`Reversed`) and no new character until the plug is pulled. Then `*` shows. | The keyer sees a closed contact. The entry mask works only at the mode start. | Long `menu/back`, then open Free Input again: the mask now ignores the ring. The manual says: insert the plug before you open Free Input. No stuck-key timeout (UX-37). |
| F-10 | A plug is inserted or removed while Free Input is open. | A short tone and maybe one extra character (`E`, `T` or `*`). | A short contact closure is a real element for the keyer. | Long `prev` clears the screen. |
| F-11 | The main loop falls behind and the keyer event queue overflows (more than 8 events). | `*` in place of the character in progress. The text continues after it. | The keyer drops the new event and sets an overflow flag. Sound and keying stay correct. | None needed. If it happens often, record it for `architector` (D-21). |
| F-12 | Power loss while Free Input is open. | After power-on, the main menu. The text is gone. | No mode state survives. The settings stay as stored. | Open Free Input again. |
| F-13 | No plug, or the key is not connected. | `Free Input` and a blank text area. No sound. | The keyer idles. The `enter` button still keys the tip. | Connect the key. |
| F-14 | First power-on after the update from the earlier firmware: a valid record of layout version 1. | The main menu, no message. All earlier settings stay. `Prosigns` shows `Both`. Free Input shows AR, BT, KN and AS in the `Both` text. | The record loads. `Prosigns` takes its default. Nothing is written at start-up. The first save after an edit step writes the new layout (version 2) with all current values (`keyer_spec.md` section 17.4.2). | None needed. The user can choose another value in Settings. |
| F-15 | Return to the earlier firmware after a save with this firmware. | The earlier firmware shows all its defaults, no message. | The earlier firmware does not accept layout version 2. It uses F-2. | The user sets the values again. The release notes say this (`technical-writer`). |
| F-16 | Progress save fails after a passed session (read-back mismatch, CMD_FAIL or low supply). | The end screen shows `Not saved` in row 1 and `Passed, new level` in row 3. The selector preselects the new level, and the main menu shows it. | The new level stays in RAM. The selector, the next session and the main menu use it (plan, A-L1). | The next passed session tries the save again. If the user switches off first, the next start depends on the failed step. After a failed erase, it uses the earlier stored levels. After a failed write that followed the erase, it uses level 1 everywhere (F-17). |
| F-17 | Empty, erased or bad progress record (wrong magic, version or CRC). | Level 1 in both modes at every speed. No message. | Each level starts at index 0 (`training_spec.md`). | None needed. Same reason as UX-17. |
| F-18 | Power loss during a training session. | After power-on, the main menu. The session is gone. | The progress changes only at a passed session end. | Start the mode again. The level is the stored one. |
| F-19 | Power loss during the progress save. | After power-on: level 1 everywhere (F-17), or the old levels. | The save erases page 1, then writes the record. | The user trains the levels again. The risk window is about 6 ms per passed session. |
| F-20 | A plug is inserted or removed in Receiving. | Maybe one `enter` tap: in Item, the right run is selected (the labels narrow, or a character shows in its slot). In the last slot, the check can come and use an attempt. On the start screen and the end screen, a session can start at the selected level. | In a session, a short tip closure is an `enter` tap (tap mode, section 6.4). On the start screen and the end screen, tap mode is off, and the closure is an `enter` press (UX-55). A closure of 800 ms or longer is a long `enter` (a free replay). Unverified: D-5. | A long `prev` steps out of a narrowed group or takes a pick back. A check has no undo. Accepted by the user (`training_spec.md` D-T27, R-T7). A session that started by accident: a long `menu/back` drops it, and the level does not change. The manual says: insert the plug before you open Receiving. |
| F-21 | A plug is inserted or removed in Sending, or a mono plug closes the ring after the start. | As F-9 and F-10: a tone, extra elements, maybe a wrong character that uses an attempt. On the start screen and the end screen, a tip closure can start a session at the selected level. | The keyer sees a real contact. | Long `menu/back` and a new start. The manual says: insert the plug first. |
| F-22 | Volume at its minimum in Receiving. | The screen works, but the playback is silent. | The player plays as usual. | Turn up the volume (C-7), then a long `enter` replays the item. |
| F-23 | `Reset Statistics` `Yes`, and the progress save fails. | The message `Not saved` (section 5.8) in place of `Statistics reset`. | If the erase failed, the stored record keeps the old levels. If a write after the erase failed, no valid record remains, and after a power cycle every level is 1 (F-17). | `OK` goes back to Settings. The user can try the reset again. After a power cycle, the stored levels apply, or level 1 everywhere. |
| F-24 | Every power-on (UX-58, UX-59) | The startup screen for 5 s, then the main menu with the cursor on `Receiving`. On a recovery start, the `Display Defaults` message comes between the two (F-8, UX-60). | Settings and progress load before the first frame. The screen is drawn once. Nothing is written to the EEPROM. The keyer is off, nothing sounds. | Any button press skips the screen. The press does nothing else. |
| F-25 | `prev`, `next`, `enter` or the paddle tip is held closed at power-on (for example a key that is closed in the socket). | The startup screen for the full 5 s, then the main menu. The held contact does not skip the screen. | The held button gives no event until it is released and pressed again (section 6.2). | Release the button or open the key. The next press acts as usual. |

Decision UX-17: F-1 and F-2 are silent. Reasons: the user can see and fix every value in Settings. A message on
first power-on is noise. A corruption needs a power loss inside a 6 ms window. Rejected: a one-time
`Settings reset to defaults` message (costs a state and a string for a very rare event).

F-14 is silent for the same reasons (UX-17, UX-38). The user keeps every earlier setting. The new item is visible in
Settings with its default. A message after the update tells the user nothing that the Settings list does not show. Each start-up loads the version 1 record again with the same result, until the first save. So an update
alone never erases the EEPROM.

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

1. The startup screen (section 5.17), already with the default contrast and backlight. It ends after 5 s, or at the
   first new button press. The held `menu/back` and its release give no event (section 6.2), so they do not skip it.
2. The message screen `Display Defaults` / `Default contrast and` / `backlight are in use.` / `OK` (section 5.8).
   The press that skipped the startup screen does not close it (UX-59, UX-60). The next `enter` or `menu/back`
   closes the message.
3. The main menu with the cursor on `Receiving`.

Order: decision UX-60 (section 5.17.3).

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

Outside section 9.5, "power-on" and "re-init" include the end of the startup screen (section 5.17). The test lets
`MENU_SPLASH_MS` (5000 ms) pass with no button event. Then the scenario starts. A test helper can end the screen with one
button event instead, because the result is the same (UX-59). Section 9.5 tests the startup screen itself.

| ID | Setup / action | Expected result |
| --- | --- | --- |
| V-01 | Power-on, empty EEPROM. | `TITLE`, `TEXT(0,0,"CW Trainer",inv)`. `SEL(1)`, `TEXT(1,0,"Receiving",inv)`, `TEXT(1,21,"L1",inv)`. `TEXT(2,0,"Sending")`, `TEXT(2,21,"L1")` (UX-57), `TEXT(3,0,"Free Input")`, `TEXT(4,0,"Settings")` and `TEXT(4,24,">")`. `TEXT(5,0,"About")`, and cell (5,24) is blank (UX-61). Rows 6-7 have all pixels off. No EEPROM erase or write. |
| V-02 | Power-on with a record that has a bad CRC. Open Settings > CW and Settings > Display. | No message screen. CW shows `600 Hz`, `32 WPM`, `32 WPM`, `20 WPM`, `20 WPM`, `Iambic B`, `Normal`. Settings shows `On`, `Both`, `5`. Display shows `8`, `32`. |
| V-03 | Main menu: press prev, then press next. | After prev: `SEL(5)` on `About`, `PLAIN(1)`. After next: `SEL(1)`. |
| V-04 | Main menu: hold next for 1000 ms. | Cursor stops on `About` (`SEL(5)`). It does not wrap to `Receiving`. |
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
| V-19 | Main menu: enter on Receiving, enter on the start screen. Long menu/back during the start pause. Then enter on Sending, enter on the start screen. Long menu/back. | Receiving opens as V-90. After the long press: main menu with `SEL(1)` on `Receiving`. The fake player was never started. Sending opens as V-110. After the long press: main menu with `SEL(2)` on `Sending`, the keyer stopped once. No EEPROM erase or write. |
| V-20 | Settings: enter on Reset Statistics. Enter. | Dialog: title `Reset Statistics`, `TEXT(3,4,"Reset statistics?")`, `BUTTON(37,56)` with `No` inverted, `Yes` normal. After enter: Settings, `SEL(6)` on `Reset Statistics`. `stats_reset()` not called. |
| V-21 | Open the dialog. Next. Next. Enter. Enter. | `BUTTON(67,91)` after the first next. The second next changes nothing. First enter: `stats_reset()` called once. `TEXT(3,4,"Statistics reset")`, `BUTTON(52,71)` with `OK`. Second enter: Settings, `SEL(6)`. |
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
| V-33 | Stored contrast 0, backlight 0. P0.2 low from `buttons_init()` for 60 ms and longer. | The fake LCD gets contrast 32 and the fake backlight gets level 8 before the first flush. The stored contrast 0 is never applied. The first frame is the startup screen (V-152). After `MENU_SPLASH_MS`, the screen shows: title `Display Defaults`, `TEXT(2,2,"Default contrast and")`, `TEXT(3,2,"backlight are in use.")`, `BUTTON(52,71)` with `OK`. No EEPROM erase or write. |
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
| V-52 | Power-on, empty EEPROM, reset the fake sidetone log. Call `menu_poll()` after each event. Do a `POLL` of 500 ms after each step. Steps: 1. Move in the main menu with presses and repeats. 2. Open Settings. Move the cursor onto `Tone` and past it in browse mode. 3. Edit each value and choice item except Tone up to both limits, with presses and repeats. 4. Open the Reset Statistics dialog and answer `No`. Open it again and answer `Yes`. Close the message. 5. Change a value and leave Settings with a save. | `SLOG` is empty at the end. `fake_tick_read_count()` does not change during the `POLL` steps. The training modes have their own sound tests (section 9.4). |
| V-53 | Recovery start as V-33, then reset the fake sidetone log. Release P0.2, close the message with `menu/back`. `POLL` for 1000 ms. | `SLOG` is empty. |

### 9.2 Free Input and long-press tests (sections 5.9, 6.6, 6.7)

These tests use a fake keyer event source and the probes of section 9. Notation:

- `FI`: power-on with an empty fake EEPROM, move the cursor to `Free Input`, press `enter`.
- `KEY(s)`: for each character of `s`, the fake keyer gives the DIT and DAH events of its Morse code and then one
  CHAR_GAP. A space in `s` is one WORD_GAP. `KEYP(p)` does the same for one pattern `p` of `.` and `-`.
- `En`: the text of `n` letters `E`. `Ln(X)`: a line of 25 letters `X`.
- `LONG(X)`: hold button `X` until its long event comes, then release it.
- `BLANK(r)`: all pixels of row `r` are off.
- `LINE(r)`: pixels `(x, 8r + 3)` are on for x = 2..126. All other pixels of rows `8r` to `8r + 7` are off.
- Global rule for the Free Input tests (V-54 to V-73, V-84 to V-89): in rows 1-7, the pixels with x = 0, 1 and 127
  are off.

| ID | Setup / action | Expected result |
| --- | --- | --- |
| V-54 | `FI`. | `TITLE`, `TEXT(0,0,"Free Input",inv)`. `BLANK(1)`..`BLANK(7)`. The keyer starts once with `Iambic B`, `Normal`, 20 WPM, 20 WPM. The menu `enter` is off. |
| V-55 | `FI` with P3.2 low from the `enter` press on. Keep P3.2 low for 2 s, then release. | No ENTER event. No DIT event and no sidetone request from the closed contact. The screen does not change. |
| V-56 | `FI`. `KEY("CQ DE")`. | `TEXT(1,0,"CQ DE")`. Every other cell of rows 1-7 is blank. |
| V-57 | `FI`. One WORD_GAP. `KEY("E")`. | `TEXT(1,0,"E")`: no space at the line start. |
| V-58 | `FI`. `KEY(E25)`. One WORD_GAP. `KEY("T")`. | Row 1 is `L25(E)`. `TEXT(2,0,"T")`: the space at the end of a full line is dropped. |
| V-59 | `FI`. `KEY(E24)`. One WORD_GAP. `KEY("T")`. | Row 1: 24 letters `E`, cell (1,24) blank. `TEXT(2,0,"T")`. |
| V-60 | `FI`. Key 7 lines `L25(A)` to `L25(G)`. Check. Then `KEY("H")`. | After 175 characters: rows 1-7 show `A` to `G`, no scroll yet. After `H`: rows 1-6 show `B` to `G`, `TEXT(7,0,"H")`, the rest of row 7 blank. `TITLE` holds. Page 0 is not dirty. |
| V-61 | `FI`. `KEYP("........")`. `KEYP(".......")`. A pattern with an empty table slot. Set the overflow flag, then `KEYP(".-")`. | Four cells `*`: `TEXT(1,0,"****")`. |
| V-62 | Valid record with `Prosigns` = `Punctuation`. `FI`. `KEYP(".-.-.")`, `KEYP("-...-")`, `KEYP("...-.-")`. | `TEXT(1,0,"+=<SK>")`. With the default `Both`, the same input gives V-84. |
| V-63 | `FI`. `KEY(E23)`. `KEYP("...-.-")`. Then a new `FI`: `KEY(E25)`, one WORD_GAP, `KEYP("...-.-")`. | First: `<S` in cells (1,23) and (1,24), `TEXT(2,0,"K>")`. Second: row 1 is `L25(E)`, `TEXT(2,0,"<SK>")`, no space at the line start. |
| V-64 | `FI`. `KEY("ABC")`. DIT, DIT. `LONG(prev)`. DAH, CHAR_GAP. | After the clear: `BLANK(1)`..`BLANK(7)`, `TITLE` holds, page 0 is not dirty. Then `TEXT(1,0,"T")`: the two dits are dropped. |
| V-65 | `FI`. `LONG(prev)`. One CHAR_GAP with no elements. One WORD_GAP. `KEY("A")`. | No change before `A`. `TEXT(1,0,"A")`. |
| V-66 | `FI`. `KEY("AB")`. `LONG(next)`. `KEY("C")`. | `TEXT(1,0,"AB")`, `LINE(2)`. Then `TEXT(3,0,"C")`, `BLANK(4)`. |
| V-67 | `FI`. `LONG(next)`. `KEY("C")`. Then `LONG(next)`, `LONG(next)`. | `LINE(1)`, `TEXT(2,0,"C")`, `LINE(3)`, `LINE(4)`. |
| V-68 | `FI`. Key 7 lines `L25(A)` to `L25(G)`. `LONG(next)`. `KEY("H")`. | After the delimiter: rows 1-6 show `B` to `G`, `LINE(7)`. After `H`: rows 1-5 show `C` to `G`, `LINE(6)`, `TEXT(7,0,"H")`. |
| V-69 | `FI`. DIT. `LONG(next)`. DAH, CHAR_GAP. | `LINE(1)`, then `TEXT(2,0,"A")`: the delimiter keeps the character in progress. |
| V-70 | `FI`. `KEY("AB")`. Press prev, next, menu/back. P3.2 low 100 ms, then high. Hold prev for 600 ms, release. Hold next for 600 ms, release. | No redraw, no dirty page from the buttons, still Free Input. The keyer is not stopped. P3.2 gives no ENTER event. |
| V-71 | `FI`. `KEY("AB")`. `LONG(menu)`. Press enter. | After the long press: main menu, `SEL(3)` on `Free Input`. The keyer stops once. The menu `enter` is on. After enter: Free Input with `BLANK(1)`..`BLANK(7)`: the text is gone. |
| V-72 | `FI`. P3.2 low. `LONG(menu)` while P3.2 stays low. Keep P3.2 low 2 s more, release, then press enter. | Main menu, `SEL(3)`. No ENTER event while P3.2 stays low. One ENTER after the new press. |
| V-73 | `FI`. Hold menu/back with its debounced state pressed for 790 ms after its press event (the pin opens 770 ms after the press event), release. Then a debounced hold of 810 ms (the pin opens 790 ms after the press event). Then `FI` and hold it for 5 s. | 790 ms: still Free Input. 810 ms: the main menu shows at 800 ms (+/- 2 ms). 5 s: one exit only. The menu gets no event during the rest of the hold and after the release. |
| V-74 | Lists with long holds: main menu, hold menu/back 2 s. Settings > CW, hold menu/back 2 s. Main menu, hold next 1000 ms. Tone edit at 600 Hz, hold next 1000 ms. Dialog, hold next 1000 ms. | Main menu: no change (as V-18). CW: Settings with `SEL(1)` on `CW`, no second step. Main menu: as V-04. Tone: 690 Hz, as V-08. Dialog: focus `Yes`, nothing else. |
| V-75 | `buttons` only: hold prev. At 800 ms after the press event, the long event and a repeat are due in the same poll. | The first poll gives the long event of prev. The next poll gives the repeat. |
| V-76 | Settings > CW. | 7 rows. `TEXT(7,0,"Paddle")`, `TEXT(7,17,"Normal")`. |
| V-77 | CW: enter on Paddle. Next. Next. Enter. Leave Settings. Re-init from the fake EEPROM. | `EDIT(7,8)`. `Normal`: only `>`. `Reversed` (`TEXT(7,15,"Reversed",inv)`): only `<`. The second next changes nothing. One save. After re-init: `Reversed`. |
| V-78 | Valid record from the earlier firmware (layout version 1, flags bit 1 = 0). | `Paddle` shows `Normal`. `Prosigns` shows `Both` (V-83). |
| V-79 | Settings: `Key Type` = `Straight`, `Paddle` = `Reversed`, TX Speed 25, TX Eff. Speed 15. Leave Settings. `FI`-style entry from the main menu. | The keyer starts once with `Straight`, swapped, 25 WPM, 15 WPM. |

### 9.3 `Prosigns` tests (sections 5.9, 5.10, F-14)

These tests use the probes and the notation of sections 9 and 9.2. Prosign patterns: AR `.-.-.`, BT `-...-`,
KN `-.--.`, AS `.-...`, SK `...-.-`. "AR" in an action means `KEYP(".-.-.")`, and the same for the others. "A record
with `Prosigns` = X" means a valid layout version 2 record from `keyer_spec.md` section 17.4.2 with that value. The
tests agree with AC-P14 to AC-P20 of `keyer_spec.md` section 17.7.

| ID | Setup / action | Expected result |
| --- | --- | --- |
| V-80 | Power-on, empty EEPROM. Open Settings. Then prev. Then next. Then next, next. | Title `Settings`. `SEL(1)` with `TEXT(1,0,"CW",inv)` and `TEXT(1,24,">",inv)`. `TEXT(2,0,"Show CW Code")`, `TEXT(2,21,"On")`. `TEXT(3,0,"Prosigns")`, `TEXT(3,19,"Both")`. `TEXT(4,0,"Max Word Length")`, `TEXT(4,22,"5")`. `TEXT(5,0,"Display")`, `TEXT(5,24,">")`. `TEXT(6,0,"Reset Statistics")`. `BLANK(7)`. After prev: `SEL(6)` (wrap from the first to the last item). After next: `SEL(1)`. After next, next: `SEL(3)` with `TEXT(3,0,"Prosigns",inv)` and `TEXT(3,19,"Both",inv)`. (AC-P14) |
| V-81 | Settings, cursor on `Prosigns`. Enter. Prev. Prev. Prev. Next. Next. Next. Enter. | After enter: `EDIT(3,11)`, `TEXT(3,0,"Prosigns")` in normal video, `TEXT(3,19,"Both",inv)`, `TEXT(3,10,"<",inv)`, cell (3,24) blank inside the band. First prev: `TEXT(3,16,"Prosign",inv)`, `<` in (3,10), `>` in (3,24). Second prev: `TEXT(3,12,"Punctuation",inv)` (all 11 characters), cell (3,10) blank inside the band, `>` in (3,24). Third prev changes nothing. Next, next: `Prosign`, then `Both` with the markers above. The third next changes nothing. After enter: `SEL(3)`, `TEXT(3,19,"Both",inv)`. (AC-P15) |
| V-82 | Settings: edit `Prosigns` to `Punctuation` (enter, prev, prev, enter). Menu/back. Re-init from the fake EEPROM. Open Settings. Repeat with `Prosign` (enter, next, enter) and with `Both` (enter, next, enter). Then change `Both` -> `Prosign` -> `Both` and leave Settings. | Each of the first three exits: exactly one erase and one record write, at the menu/back from Settings. Main menu, `SEL(4)`. After each re-init: `TEXT(3,12,"Punctuation")`, then `TEXT(3,16,"Prosign")`, then `TEXT(3,19,"Both")`. The stored record has layout version 2 and flags bits 2-3 = 0, 1, then 2. The last exit: no EEPROM erase or write (UX-15). (AC-P15) |
| V-83 | The fake EEPROM holds the version 1 record `43 57 01 58 02 20 20 14 14 02 03 05 08 20 78` (`Paddle` = `Reversed`). Power-on. Open Settings, then CW. Go back to the main menu without an edit step. Then open Settings > CW, change Tone 600 -> 610 and leave Settings. Re-init from the fake EEPROM. | No message screen (F-14). Settings shows `On`, `Both`, `5`. CW shows `Reversed` and the other default values. First exit: no EEPROM erase or write. Second exit: one save. The stored record has layout version 2, flags byte `0x0B` and Tone 610 Hz. After re-init: `Reversed`, `Both`, `610 Hz`. (AC-P9, AC-P10 from the user's view) |
| V-84 | For each value: a record with `Prosigns` = that value (empty EEPROM for `Both`). `FI`. AR, BT, KN, AS, SK with no word gap. | `Punctuation`: `TEXT(1,0,"+=(&<SK>")`. `Prosign`: `TEXT(1,0,"<AR><BT><KN><AS><SK>")`. `Both`: `TEXT(1,0,"<AR/+><BT/=><KN/(><AS/&><")` and `TEXT(2,0,"SK>")`. Every other cell of rows 1-7 is blank. (AC-P18) |
| V-85 | For each value as V-84. `FI`. AR, one WORD_GAP, BT, one WORD_GAP, KN, one WORD_GAP, AS, one WORD_GAP, SK. Then a new `FI` with `Both`: `KEY("A")`, AR, one WORD_GAP, `KEY("K")`. | `Punctuation`: `TEXT(1,0,"+ = ( & <SK>")`. `Prosign`: `TEXT(1,0,"<AR> <BT> <KN> <AS> <SK>")`, cell (1,24) blank. `Both`: `TEXT(1,0,"<AR/+> <BT/=> <KN/(> <AS/")` and `TEXT(2,0,"&> <SK>")`. Last case: `TEXT(1,0,"A<AR/+> K")`: a word gap after a prosign text adds one blank cell. (AC-P20) |
| V-86 | `Both`, a new `FI` for each case. `KEY(E19)`, AR, `KEY("T")`. `KEY(E22)`, AR. `KEY(E23)`, AR. `KEY(E25)`, AR. `KEY(E23)`, KN. Then `Prosign`: `KEY(E23)`, BT. | E19: `<AR/+>` in cells (1,19) to (1,24), then `TEXT(2,0,"T")` (the column was 25). E22: `<AR` in (1,22) to (1,24), `TEXT(2,0,"/+>")`. E23: `<A` in (1,23) and (1,24), `TEXT(2,0,"R/+>")`. E25: row 1 is `L25(E)`, `TEXT(2,0,"<AR/+>")`. KN: `<K` in (1,23) and (1,24), `TEXT(2,0,"N/(>")`. `Prosign` BT: `<B` in (1,23) and (1,24), `TEXT(2,0,"T>")`. (AC-P19) |
| V-87 | `Both`. `FI`. Key rows 1-6 `L25(A)` to `L25(F)`, then 22 letters `G`. Then AR. | Rows 1-5 show `B` to `F`. Row 6 shows 22 letters `G` and `<AR` in (6,22) to (6,24). `TEXT(7,0,"/+>")`. `TITLE` holds. Page 0 is never written and never dirty. (AC-P19) |
| V-88 | Power-on, empty EEPROM. `FI`. AR. Test only: `settings_set(SETTINGS_ID_PROSIGN_STYLE, 0)`. AR. `LONG(menu)`. Then `FI` again. AR. Then `LONG(menu)`, set `Prosigns` = `Prosign` in Settings, leave Settings, `FI`, AR. | First mode: `TEXT(1,0,"<AR/+><AR/+>")`: the change during the mode has no effect. Second mode: `TEXT(1,0,"+")`. Third mode: `TEXT(1,0,"<AR>")`. (AC-P17) |
| V-89 | Run V-54 to V-61 and V-63 to V-73 once for each value of `Prosigns` (a record with that value). | The same results as with the default for every value. These tests key no AR, BT, KN or AS. (AC-P20) |

### 9.4 Training mode tests (sections 5.11 to 5.15, 6.8 to 6.10)

These tests use the probes of sections 9 and 9.2, a fake player, the fake keyer event source and a fake training
source. `architector` defines the fakes in `training_spec.md`. The tests check what the screen shows and which calls
the modes make.

Notation:

- `RX(set, kind, items)`: power-on with an empty fake EEPROM. The fake training source gives the keyboard set `set`
  (in keyboard order), the session kind (`chars` or `words`) and the items `items` in order. Move the cursor to
  `Receiving` and press `enter`: the start screen shows at the earned level (section 5.16.1). Press `enter` again
  at t = 0: the session starts at that level. `set` = `ALL` means the 43 characters of the Koch set. `TX(...)` does the same
  for `Sending`. The fake training source is `fake_koch_item`, and the keyboard set and the kind come from the level
  that `progress_fixture_write()` sets (`training_spec.md` section 15.2).
- `EARN(m, i)`: `progress_fixture_write()` gives the level index `i` to mode `m` (`RX` or `TX`) at the current speed
  of that mode. `OPEN(m)`: power-on with that fixture, move the cursor to the mode item and press `enter`. The start
  screen shows. On the start screen and the end screen, tap mode is off, so `L`, `M`, `R` and `E` act at the press.
- `L`, `M`, `R`, `E`: a tap (press and release before 800 ms) of `menu/back`, `prev`, `next`, `enter`. In Receiving
  they select the runs 0, 1, 2 and 3 from the left (section 5.12.1). The letters `L`, `M` and `R` keep their earlier
  names: `M` is `prev` and `R` is `next`, so `R` is not the right run. `E` is the right run. In Receiving, tap mode
  gives the event at the debounced release. `AT t: L` means that the event comes at t.
- `LONG(X)`: as in section 9.2. `LONG(E)` holds `enter`. "Hold `X` from t" means: the press event of `X` comes at t,
  and the long event at t + 800.
- `TIP(t, d)`: P3.2 (the paddle tip and the `enter` button) low from t for d ms, then high. The press event comes at
  t + 20. For d < 800, the tap comes at the release, t + d + 20. For d >= 800, the long `enter` comes at t + 820, and
  nothing comes at the release (`training_spec.md` D-T22).
- `AT t:` as in section 9.1. `t` is in ms after the event that starts the session.
- `PLOG`: the ordered call log of the fake player: `play("K")`, `stop()`. `PIDLE(t)`: the fake player becomes idle at
  `t`.
- `HDR(s)`: `TITLE` holds and `TEXT(0,0,s,inv)`.
- `STATUS(s)`: `TEXT(1, floor((25 - len(s)) / 2), s)`, and every other cell of row 1 is blank. `STATUS()`: all pixels
  of row 1 are off.
- `BIG(x, s)`: the cells from x, cell top y = 24, match a reference rendering of `s` in `FONTS_ID_8X16`, normal video.
  For a word, use one `BIG` per character.
- `BLOCK(x)`: pixels x to x + 6, y 26-39 are on. Pixels (x - 1, 30) and (x + 7, 30) are off.
- `BAR(x)`: pixels x to x + 6, y 38-39 are on. Pixels (x - 1, 38) and (x + 7, 38) are off.
- `ITEMLINE()`: only the listed `BIG`, `BLOCK` and `BAR` pixels are on in y 24-39.
- `KEYS(a, b, c, d)`: the four labels of section 5.12.1 in row 7, cell top y 56, 5 x 8 font, normal video. Each
  string matches a reference rendering from its pixel x: `a` at x 2, `b` at x 30 + floor(5 * (5 - len(b)) / 2), `c`
  at x 58 + floor(5 * (5 - len(c)) / 2), `d` at x 127 - 5 * len(d). An empty string means no ink in that field. All
  other pixels of row 7 are off. `KEYS` includes `KEYBARS`.
- `KEYBARS`: pixels (x, 55) are on for x 2-26, 30-54, 58-82 and 87-126. Pixels (x, 55) are off for x 0, 1, 27-29,
  55-57, 83-86 and 127. All other pixels of row 6 are off.
- "Top labels" means `KEYS` of the whole set.
- `MARKS(r, x0, p)`: for each element of the pattern `p`, from x0: a dot is 2 px wide, a dash 6 px, with a gap of
  3 px. The marks are on in y `8r + 3` and `8r + 4`. All other pixels of row r are off.
- `BODYOFF(a, b)`: all pixels of rows a to b are off.
- `STARTBTN`: `TEXT(7,10,"Start",inv)`. Pixels (47, y) and (81, y) are on for y 57-63. Pixels (46, 60) and (82, 60)
  are off. Pixels with y = 56 are off for x 47-81. All other pixels of row 7 outside x 47-81 are off.
- `LVL(s, e)`: row 4 shows the selector line of section 5.16.2 for the shown levels `s` and `e`. With `t` =
  `L<s> of <e>`, `len` = its length and `c` = floor((19 - len) / 2): `TEXT(4, c + 3, t)`. Cell (4, c) is `<` when
  s > 1, else blank. Cell (4, c + len + 5) is `>` when s < e, else blank. All other cells of row 4 are blank. Normal
  video. At the saturation of D-T5 the markers follow the indexes, not the shown numbers (V-148).
- `DESC(s)`: `TEXT(5, floor((25 - len(s)) / 2), s)`, and every other cell of row 5 is blank.
- `DIRTY(p, ...)`: exactly the listed LCD pages are dirty after the event. `DIRTY()`: no page is dirty.
- Global rule for V-90 to V-151: in rows 1-7, the pixels with x = 0, 1 and 127 are off.

| ID | Setup / action | Expected result |
| --- | --- | --- |
| V-90 | `RX("KM", chars, "K")`. `POLL 1..999`. `AT 1000: poll`. | `HDR("RX 32/32 L1          1/50")` from t = 0. Up to 999: `STATUS()`, `BODYOFF(2,5)`, `KEYS("K","M","","")`, `PLOG` empty. At 1000: `BLOCK(60)`, `ITEMLINE()`, `play("K")` at 32 WPM and 32 WPM. Tap mode is on. |
| V-91 | Continue V-90. `PIDLE(1500)`. `AT 2000: L`. `POLL 2001..2599`. `AT 2600: poll`. `POLL 2601..3099`. `AT 3100: poll`. | At 2000: `BIG(60,"K")`, no block, `STATUS("Correct")`, `KEYS("K","M","","")`. Unchanged up to 2599. At 2600: `BODYOFF(1,5)`, `HDR("RX 32/32 L1          2/50")`, `KEYS` unchanged. At 3100: `BLOCK(60)` and `play()` of item 2. |
| V-92 | `RX(ALL, chars, "M")`, `Prosigns` = `Both`. After the item starts: `M`, `L`, `M`. | Start: `KEYS("A-K","L-V","W-6","7-<AR>")`. After `M`: `KEYS("LMN","OPQ","RST","UV")`. After `L`: `KEYS("L","M","N","")`. After `M`: `BIG(60,"M")`, `STATUS("Correct")` at that event, top labels. Three taps for one pick. |
| V-93 | For each `Prosigns` value, `RX(ALL, chars, ...)`, three test runs: `E`, `E`, `M`. `E`, `E`, `L`. `E`, `R`, `M`. | Start: `KEYS("A-K","L-V","W-6","7-<AR>")` for `Both` and `Prosign`, `KEYS("A-K","L-V","W-6","7-+")` for `Punctuation`. After the first `E`: `KEYS("789",".,?","/<BT>","<SK><AR>")` for `Both` and `Prosign` (`<SK><AR>` at x 87), `KEYS("789",".,?","/=","<SK>+")` for `Punctuation`. After the second `E`: `KEYS("<SK>","<AR>","","")`, `KEYS("<SK>","<AR>","","")`, `KEYS("<SK>","+","","")`. First run, `M`: `BIG(40,"<AR/+>")`, `BIG(48,"<AR>")`, `BIG(60,"+")`. Second run, `L`: `BIG(48,"<SK>")` for every value. Third run, after `R`: `KEYS("/","<BT>","","")` for `Both` and `Prosign`, `KEYS("/","=","","")` for `Punctuation`. Then `M`: `BIG(40,"<BT/=>")`, `BIG(48,"<BT>")`, `BIG(60,"=")`. |
| V-94 | Run 1: `RX("KMRSUAPTLOWI.", words, "KMRS")`. After the item starts: `L`, `R`. `LONG(M)`. `L`. `LONG(M)`. `LONG(M)`. Run 2: `RX(ALL, chars, "M")`. After the item starts: `M`, `L`. `LONG(M)`. `LONG(M)`. `LONG(M)`. | Run 1. Start: `KEYS("AIKL","MOP","RST","UW.")`, `BLOCK(36)`, `BAR(52)`, `BAR(68)`, `BAR(84)`. After `L`: `KEYS("A","I","K","L")`. After `R`: `BIG(36,"K")`, `BLOCK(52)`, `BAR(68)`, `BAR(84)`, top labels. First `LONG(M)`: no change up to 800 ms, then `BLOCK(36)`, three bars, top labels. After `L`: `KEYS("A","I","K","L")`. Second `LONG(M)`: top labels, the slots unchanged. Third: nothing changes. Run 2. After `M`, `L`: `KEYS("L","M","N","")`. First `LONG(M)`: `KEYS("LMN","OPQ","RST","UV")`. Second: `KEYS("A-K","L-V","W-6","7-<AR>")`. Third: nothing changes. `BLOCK(60)` all the time. No check in both runs. |
| V-95 | `RX("KMR", chars, "M")`. After the item starts, hold `prev` for 1000 ms and release. `POLL` up to 3000 ms after the press. Then `M`. | During and after the hold: `BLOCK(60)`, `STATUS()`, `KEYS("K","M","R","")`. No pick, no check, no attempt. After `M`: `BIG(60,"M")`, `STATUS("Correct")` at that event. The item counts as correct on the first attempt. |
| V-96 | `RX(ALL, chars, ...)`. After the item starts, hold `next` for 2000 ms and release. Then hold `menu/back` with a debounced hold of 790 ms. | `next`: nothing changes, also at the release: no repeat, no tap, and the long `next` does nothing. `menu/back`: at its release, `KEYS("ABC","DEF","GHI","JK")`. Still Receiving. |
| V-97 | `RX("KM", chars, "K")`. `AT 500: E` (start pause). Hold `enter` from 1200 (playing) for 1000 ms. `PIDLE(2600)`. Hold `enter` from 3000 (waiting) for 900 ms. `PIDLE(4400)`. P5.5 low at 4500 for 100 ms, then high. `AT 5000: L`. | `PLOG`: `play("K")` at 1000, at 2000 (a restart during the playback: the long `enter` of the hold from 1200) and at 3800. Nothing from the tap at 500, nothing at the releases (2200, 3900), nothing from P5.5. The `enter` run is empty (2 characters), and the long `enter` never selects it. The replays do not change the screen: `BLOCK(60)`, `KEYS("K","M","","")`, `STATUS()` up to 5000. At 5000: `BIG(60,"K")`, `STATUS("Correct")`. The mode makes no sidetone call itself. |
| V-98 | `RX("KM", chars, "K")`. After the item starts and plays: `AT t: M`. | At t: `BIG(60,"M")`, `STATUS("Wrong")`. At t + 1200: `STATUS("Try 2 of 3")`, `BLOCK(60)`, `play("K")`. Header still `1/50`. |
| V-99 | Continue V-98. `M` (attempt 2). Then `AT u: M` (attempt 3). `PIDLE(P)` for the answer playback. | Attempt 2: `STATUS("Wrong")`, after 1200 ms `STATUS("Try 3 of 3")`. At u: `STATUS("Wrong. The answer is:")`, `BIG(60,"K")`, no block, `play("K")`. At P + 2499: unchanged. At P + 2500: `BODYOFF(1,5)`, header `2/50`. At P + 3000: `BLOCK(60)`, `play()` of item 2. |
| V-100 | `RX("KM", chars, "K", "K")`. Item 1: `M`, then after the retry `L`. | `STATUS("Wrong")`, then `STATUS("Try 2 of 3")`, then `STATUS("Correct")` at the `L` event. Then the blank moment and item 2 (V-91). The score does not count item 1. |
| V-101 | `RX("KMRSUAPTLOWI.", words, "KMRS")`. Pick K, M, R, S. | After each pick: the glyph at 36, 52, 68, 84 in turn, the block on the next slot, bars on the others. At the event of the 4th pick: four glyphs, no block, no bar, `STATUS("Correct")`. |
| V-102 | `RX(ALL, words, ...)`. | `KEYS("A-J","K-T","U-3","4-/")`: the word keyboard has 40 characters and no prosign (UX-48). After `E`: `KEYS("456","789",".,","?/")`. |
| V-103 | `RX("KM", chars, "K", "K")`. Pick K (`L`) at t. During `Correct` and the blank moment, each before t + 1100: `L`, `M`, `R`, `E`, `LONG(M)`, `LONG(E)`. | No change of the screen from these events, and no player call. At t + 1100: item 2 with `BLOCK(60)` and one `play("K")`. No pick shows in item 2. |
| V-104 | `RX("KM", chars, "K")`. Hold `menu/back` from 1200 (playing). New run: hold `menu/back` from 100 (start pause). | First run: no change from 1200 to 1999. At 2000: `stop()` in `PLOG`, main menu, `SEL(1)` on `Receiving`, tap mode off. Second run: main menu at 900, `PLOG` empty. No EEPROM erase or write in both runs. The stored level stays. |
| V-105 | `RX("KM", chars, 50 items)` (earned index 0). Answer 45 items right on the first attempt. Answer 5 items wrong once, then right. Then `E`. | End screen right after the last `Correct` (no blank moment): `HDR("RX 32/32 L1         50/50")`, `STATUS()`, `TEXT(2,7,"Score 45/50")`, `TEXT(3,4,"Passed, new level")`, `LVL(2,2)`, `DESC("New char R")`, `BLANK(6)`, `STARTBTN`. One erase of page 1 and one record write. Tap mode is off. At the press of `E` (not at its release): `HDR("RX 32/32 L2          1/50")` and the start pause of V-90. Tap mode is on. (UX-56) |
| V-106 | As V-105 with 44 first-attempt correct items. | `STATUS()`, `TEXT(2,7,"Score 44/50")`, `TEXT(3,3,"Not passed, need 45")`, `LVL(1,1)`, `DESC("K and M")`, `STARTBTN`. No EEPROM erase or write. At the press of `E`: header `L1`. |
| V-107 | As V-105 with a fake IAP that fails the write. Then `menu/back`. Then `OPEN(RX)`, then `E`. Then re-init from the fake EEPROM and `OPEN(RX)`. | `TEXT(1,8,"Not saved")`, `TEXT(3,4,"Passed, new level")`, `LVL(2,2)`. Main menu: `TEXT(1,21,"L2",inv)` (A-L1). Start screen: `LVL(2,2)`, header `L2`. After `E`: header `L2`. After re-init: `LVL(1,1)`, header `L1`. (F-16) |
| V-108 | End screen of V-105 (`LVL(2,2)`). Run 1: `M`, `M`, `R`, `R`, then `next` held for 1000 ms. Run 2: `menu/back` held for 1000 ms. Run 3: `TIP(t, 1000)`. Run 4: a `TX(...)` end screen with the same result, then `M`, then a `menu/back` press. Run 5: a `TX(...)` end screen, P3.2 low for 100 ms, then high. | Run 1: first `M`: `LVL(1,2)`, `DESC("K and M")`, `DIRTY(4,5)`, the header still `L1 50/50`. Second `M`: `DIRTY()`. First `R`: `LVL(2,2)`, `DESC("New char R")`. Second `R` and the held `next` (repeats and its long event): `DIRTY()`. Run 2: at the press, main menu with `SEL(1)` on `Receiving`. The long event at 800 ms changes nothing. Run 3: at t + 20 a session at L2: `HDR("RX 32/32 L2          1/50")`, tap mode on. `PLOG` has only the `play()` of item 1 at t + 1020: no replay, no pick from the hold. Run 4: `LVL(1,2)`, then main menu with `SEL(2)` on `Sending` at the press. Run 5: a new Sending session at the press (tap mode is off). The keyer starts once and gives no DIT from that closure. (UX-55) |
| V-109 | Fake progress: level index 65534 for Receiving at 50 WPM. RX Speed and RX Eff. Speed 50. A word session of 100 words. Answer 6 items. Repeat with index 65535. | `HDR("RX 50/50 L65535     1/100")`, then `HDR("RX 50/50 L65535     7/100")`. Columns 15-19 are blank. Index 65535 shows the same text (D-T5). |
| V-110 | `TX("KM", chars, "K")` with P3.2 held low from the `enter` press for 2 s. | `HDR("TX 20/20 L1          1/50")`. Up to 999: `BODYOFF(1,7)`. At 1000: `BIG(60,"K")`, `ITEMLINE()` (no bar), `STATUS()`, `BODYOFF(5,7)`. The keyer starts once with `Iambic B`, `Normal`, 20 WPM, 20 WPM. No DIT event and no tone from the held contact. |
| V-111 | `TX("KM", chars, "K", "K")`. After the item shows: `KEY("K")`, the CHAR_GAP at t. | At t: `STATUS("Correct")`, `BIG(60,"K")`. At t + 600: `BODYOFF(1,7)`, `HDR("TX 20/20 L1          2/50")`. At t + 899: still `BODYOFF(1,7)`. At t + 900: `BIG(60,"K")` (item 2, the same character). |
| V-112 | `TX("KM", chars, "K")`, `Show CW Code` = `On`. After the item shows: `KEYP("-.")` with the CHAR_GAP at t. At t + 1300: `KEY("K")`. | After the first DAH: `MARKS(7,2,"-")`. At t: `STATUS("Wrong: N")`, `MARKS(7,2,"-.")`, `BIG(60,"K")`. At t + 1199: unchanged, also `MARKS(7,2,"-.")`. At t + 1200: `STATUS("Try 2 of 3")`, `BODYOFF(7,7)` (UX-52), `BIG(60,"K")`, `BODYOFF(5,5)`. At the first DAH of K: `MARKS(7,2,"-")`. At its CHAR_GAP: `STATUS("Correct")`, `MARKS(7,2,"-.-")`. |
| V-113 | `TX("KM", chars, "K", "M")`. Three times `KEYP("-.")`, each after the retry starts. | At each of the two retries: `BODYOFF(7,7)`, then the marks of the new attempt from x 2. Third CHAR_GAP at t: `STATUS("Wrong: N")`, `MARKS(5,54,"-.-")`, `BIG(60,"K")`. At t + 2500: `BODYOFF(1,7)`, header `2/50`. At t + 2800: `BIG(60,"M")`. |
| V-114 | As V-112 and V-113 with `Show CW Code` = `Off`. | Row 7 is off in every step. After the third wrong: `MARKS(5,54,"-.-")` all the same. |
| V-115 | `TX("KM", chars, "K", "M")`. `KEY("K")` with the CHAR_GAP at t. DIT at t + 100, DIT at t + 400. DAH at t + 1000, CHAR_GAP at t + 1100. Then WORD_GAP. Then DIT, OVERFLOW, CHAR_GAP. Then `KEY("M")`. | Item 2 shows at t + 900. The DIT events in the delay, and the DAH and CHAR_GAP after it, give no check: `STATUS()`, no attempt used. WORD_GAP: nothing (a character item, UX-52). OVERFLOW: no check, no attempt, `STATUS()`. `KEY("M")`: `STATUS("Correct")`, and the score counts item 2 as first-attempt correct. |
| V-116 | `TX("KMRSUAPTLOWI.", words, "KMRS")`, `Show CW Code` = `On` (the default). `KEY("K")`, `KEY("N")` with the CHAR_GAP at t. After the retry: `KEY("K")`, `KEY("M")`, `KEY("R")`, `KEY("S")`. | Start: the glyphs at 36, 52, 68, 84 and `BAR(36)`. After K: `BAR(52)`, `STATUS()`. After N: `STATUS("Wrong: N")`, `BAR(52)`, `MARKS(7,2,"-.")` up to t + 1199. Retry at t + 1200: `STATUS("Try 2 of 3")`, `BAR(36)` (D-T6: the whole word again), `BODYOFF(7,7)` (UX-52). After S: `STATUS("Correct")`, no bar. The score does not count the item. |
| V-117 | `TX(ALL, chars, AR)` for each `Prosigns` value. `KEYP("-...-")` (BT). After the retry: `KEYP(".-.-.")`. | `Both`: `BIG(40,"<AR/+>")`, `STATUS("Wrong: <BT/=>")`. `Prosign`: `BIG(48,"<AR>")`, `STATUS("Wrong: <BT>")`. `Punctuation`: `BIG(60,"+")`, `STATUS("Wrong: =")`. Then `STATUS("Correct")` for every value. |
| V-118 | `TX("KM", chars, 50 items)`, all correct. P3.2 low from 100 ms before the last CHAR_GAP, held 2 s after the end screen shows. Release. Press `enter`. | At the end screen: the keyer stops once, the menu `enter` is on. No ENTER event while P3.2 stays low. After the new press: a new session, the keyer starts once, no DIT event from that press. |
| V-119 | `TX("KM", chars, "K")`. `L`, `M`, `R`, a held `next` (repeats), `LONG(M)`, `LONG(R)`. Then `LONG(L)`. | The first events change nothing: no redraw, no dirty page. `LONG(L)`: the keyer stops once, main menu, `SEL(2)` on `Sending`. No EEPROM erase or write. Tap mode is never on in Sending. |
| V-120 | `RX(ALL, chars, ...)` with `Prosigns` = `Both`. Test only: `settings_set(SETTINGS_ID_PROSIGN_STYLE, 0)`. Then `LONG(L)` and a new `RX(...)`. | First session: `KEYS("A-K","L-V","W-6","7-<AR>")` stays. New session: `KEYS("A-K","L-V","W-6","7-+")`. |
| V-121 | Render every training screen of this section, with the longest texts: `Wrong. The answer is:`, `Not passed, need 80`, `Wrong: <KN/(>`, an 8-character word, `<AR/+>`, 12 dash marks, and the labels `<SK><AR>`, `<BT><SK>`, `7-<AR>`, `AEFIJ`. Also render `Wrong: pause too long` (UX-52). | No 5 x 8 text reaches past column 24. Item line pixels are inside x 4-123. Marks are inside x 2-126. Every label is inside its field (section 5.12.1): row 7 pixels in x 27-29, 55-57 and 83-86 are off. `KEYBARS` on every Receiving screen except the start screen and the end screen. Also render the start screen and the end screen with `LVL(65534,65535)` (`<  L65534 of 65535  >`, columns 2-22), `DESC("New char <AR/+>")`, `Passed, new level`, `Not saved` and `STARTBTN`, and the main menu with `L65535` (columns 17-22): no text past column 24. Only ASCII 0x20-0x7E. No sidetone call comes from the training screens other than the player and the keyer. |
| V-122 | `RX("KMRSUAPTLOWI.", words, "KMRS")`. After the item starts: `L`, `R`, `L`. Hold `enter` for 1000 ms while the item plays. `PIDLE(P)`. Then hold `enter` for 1000 ms (waiting). | Each hold: one `play("KMRS")` at 800 ms of the hold, a restart for the first hold. Nothing at each release. Through both holds: `BIG(36,"K")`, `BLOCK(52)`, `BAR(68)`, `BAR(84)`, `KEYS("A","I","K","L")`, `STATUS()`. The current group and the pick stay. No check, no attempt. |
| V-123 | `RX("KMRS", chars, "S")`. `TIP(1500, 100)`. New run: `AT 1500: E`. | Start: `KEYS("K","M","R","S")`. Run 1: nothing at 1520 (the press). At 1620: `BIG(60,"S")`, `STATUS("Correct")`, and `PLOG` is `play("S")` at 1000, then `stop()` at 1620. No replay. Run 2: the same result at 1500. |
| V-124 | `RX("KMRS", chars, "S")`. `TIP(1500, 800)`. `PIDLE(3000)`. `TIP(3500, 700)`. | At 2320: `play("S")` (a restart). Nothing else at 2320, and no pick. Up to 4219: `BLOCK(60)`, `KEYS("K","M","R","S")`, `STATUS()`. At 4220: `BIG(60,"S")`, `STATUS("Correct")`. |
| V-125 | `RX("KMRS", chars, "S")` with `enter` (P3.2) low from the menu press at t = 0 up to 300. New run: P3.2 low from t = 0 up to 1500. Then `E` at 2000 in both runs. | Run 1: no event at the release. Run 2: no long `enter` at 800 and no event at the release. In both runs: `PLOG` has one `play("S")` at 1000 and no restart. `BLOCK(60)`, `KEYS("K","M","R","S")`, `STATUS()` up to 1999. At 2000: `BIG(60,"S")`, `STATUS("Correct")`. |
| V-126 | `RX("KM", chars, "K", "K")`. Hold `enter` from 100 for 1000 ms (start pause). `AT t: L` (item 1, correct). Hold `enter` from t + 10 for 1000 ms (long event at t + 810, blank moment). Item 2: `AT u: M` (wrong). Hold `enter` from u + 100 for 1000 ms (long event at u + 900, result). | `PLOG`: `play("K")` at 1000, `stop()` at t, `play("K")` at t + 1100, `stop()` at u, `play("K")` at u + 1200 (the retry). No other call. No screen change from the holds: at t + 810 `BODYOFF(1,5)`, at u + 900 `STATUS("Wrong")`. The end screen case is in V-108. |
| V-127 | `RX("KM", chars, "K")`. After the item starts: `E`, `R`. Then `L`. New run: `RX("KMR", chars, "K")`. After the item starts: `E`. Then `L`. | Run 1: `KEYS("K","M","","")`. `E` and `R` (empty runs) change nothing: no redraw, no dirty page, no player call. Run 2: `KEYS("K","M","R","")`. `E` changes nothing. In both runs, `L` then gives `BIG(60,"K")`, `STATUS("Correct")`. |
| V-128 | `TX("KMRSUAPTLOWI.", words, "KMRS")`, `Show CW Code` = `On`. `KEY("K")`, `KEY("M")`. One WORD_GAP at t. `POLL` up to t + 1200. Then `KEY("KMRS")`. | After M: `BAR(68)`, `STATUS()`, `MARKS(7,2,"--")`. At t: `STATUS("Wrong: pause too long")` (start column 2), the glyphs at 36, 52, 68, 84 unchanged, `BAR(68)`, `BODYOFF(5,5)`, `MARKS(7,2,"--")`. At t + 1199: unchanged. At t + 1200: `STATUS("Try 2 of 3")`, `BAR(36)`, `ITEMLINE()`, `BODYOFF(5,5)`, `BODYOFF(7,7)`, header unchanged. After `KEY("KMRS")`: `STATUS("Correct")`. Attempt 1 is used: the score does not count the item. (UX-52) |
| V-129 | Run 1: `TX("KMRSUAPTLOWI.", words, "KMRS")`. After the item shows: one WORD_GAP. Then `KEY("KMRS")`. Run 2: the same setup. `KEY("K")`, `KEY("N")` with the CHAR_GAP at t. One WORD_GAP at t + 1300 (after the retry, before the first character). Then `KEY("KMRS")`. | Run 1: the WORD_GAP changes nothing: no redraw, no dirty page, `STATUS()`, `BAR(36)`, no attempt. `KEY("KMRS")` gives `STATUS("Correct")`, and the score counts the item as first-attempt correct. Run 2: at t + 1200 `STATUS("Try 2 of 3")`, `BAR(36)`. The WORD_GAP at t + 1300 changes nothing. `KEY("KMRS")` gives `STATUS("Correct")`. One attempt is used, not two. (UX-52) |
| V-130 | `TX("KMRSUAPTLOWI.", words, "KMRS", "KMRS")`. `KEY("KMRS")` with the last CHAR_GAP at t. One WORD_GAP at t + 180. | At t: `STATUS("Correct")`, no bar. At t + 180: unchanged, no attempt. At t + 600: `BODYOFF(1,7)`, header `2/<n>`. At t + 900: item 2 with `BAR(36)`, `STATUS()`. The score counts item 1 as first-attempt correct. (UX-52) |
| V-131 | `TX("KMRSUAPTLOWI.", words, "KMRS", "UAPT")`, `Show CW Code` = `On`. Attempt 1: `KEY("K")`, one WORD_GAP. Attempt 2, after the retry: `KEY("K")`, one WORD_GAP. Attempt 3, after the retry: `KEY("KM")`, one WORD_GAP at t. One more WORD_GAP at t + 1000. Repeat with `Show CW Code` = `Off`. | Attempts 1 and 2: `STATUS("Wrong: pause too long")`, `BAR(52)`, `BODYOFF(5,5)`. Then `STATUS("Try 2 of 3")` and `STATUS("Try 3 of 3")`, each with `BAR(36)` and `BODYOFF(7,7)`. At t: `STATUS("Wrong: pause too long")`, `BAR(68)`, `MARKS(5,56,".-.")` (the code of R, the character under the bar), `MARKS(7,2,"--")`. The WORD_GAP at t + 1000 changes nothing. At t + 2499: unchanged. At t + 2500: `BODYOFF(1,7)`, header `2/<n>`. At t + 2800: item 2 `UAPT` with `BAR(36)`. The score does not count item 1. With `Off`: row 7 is off in every step, and `MARKS(5,56,".-.")` shows all the same. (UX-52) |
| V-132 | `TX("KMRSUAPTLOWI.", words, "KMRS", "KMRS")`. One WORD_GAP at 500 (start pause). Item 1: `KEY("K")`, `KEY("N")` with the CHAR_GAP at t. One WORD_GAP at t + 180 (wrong delay). After the retry: `KEY("KMRS")` with the last CHAR_GAP at u. One WORD_GAP at u + 700 (blank moment). | At 500: no change. At 1000: item 1, `BAR(36)`, `STATUS()`. At t + 180: `STATUS("Wrong: N")` stays, no attempt. At t + 1200: `STATUS("Try 2 of 3")`, not `Try 3 of 3`. At u: `STATUS("Correct")`. At u + 700: `BODYOFF(1,7)` stays. At u + 900: item 2 with `BAR(36)` and `STATUS()`: attempt 1 of item 2. (UX-52) |
| V-133 | Part A (mode, fake keyer): `TX("KMRSUAPTLOWI.", words, "KMRS")`. `KEY("K")`, then `KEY("M")` with no WORD_GAP between them (the space stayed below 5 units). Part B (keyer, the AC-K15 test setup of `keyer_spec.md`, real `keyer_service()`, fake pins): `TX Speed` 20, `TX Eff. Speed` 20. A mark, then a space that ends one call before `keyer_word_gap_ticks` (1200 calls), then the next mark. Again with a space that reaches 1200 calls. Repeat at 20 / 10 (4360 calls). | Part A: after K `BAR(52)`, after M `BAR(68)`, `STATUS()` all the time, no attempt. Part B: the short space gives one CHAR_GAP (at 480 calls at 20 / 20) and no WORD_GAP. The long space gives one CHAR_GAP and one WORD_GAP on the call where the space reaches `keyer_word_gap_ticks`. The same at 20 / 10. (UX-52) |
| V-134 | Run 1: `TX("KM", chars, "K")`, `Show CW Code` = `On`. `KEYP("--")` (M) with the CHAR_GAP at t. DIT at t + 1100 (in the delay). DAH at t + 1300 (after the retry, the same dropped character). CHAR_GAP at t + 1500. Then `KEY("K")`. Run 2: the same with `Show CW Code` = `Off`. | Run 1: at t `STATUS("Wrong: M")`, `MARKS(7,2,"--")`. Up to t + 1199: `MARKS(7,2,"--")`, also after the dropped DIT. At t + 1200: `STATUS("Try 2 of 3")`, `BIG(60,"K")`, `BODYOFF(5,5)`, `BODYOFF(7,7)`. At t + 1300 and t + 1500: row 7 stays off, no check, `STATUS("Try 2 of 3")`. First DAH of K: `MARKS(7,2,"-")`. Its CHAR_GAP: `STATUS("Correct")`, `MARKS(7,2,"-.-")`. Run 2: the same status texts. Rows 5 and 7 are off in every step. (UX-52) |
| V-135 | `TX("KMRSUAPTLOWI.", words, "KMRS")`. Run 1: `KEY("K")`, DIT, OVERFLOW, CHAR_GAP, WORD_GAP, then `KEY("MRS")`. Run 2: `KEY("K")`, DIT, OVERFLOW, WORD_GAP, then `KEY("M")` (dropped up to its CHAR_GAP), then `KEY("MRS")`. | Both runs: `STATUS()` and `BAR(52)` up to the M that counts. No attempt is used. Run 1: the WORD_GAP after the OVERFLOW does nothing. Run 2: the drop flag is still set, so the WORD_GAP is dropped with the character. Then `STATUS("Correct")`, and the score counts the item as first-attempt correct. (UX-52, D-T16, D-T31) |
| V-136 | Fixture: a Receiving word session of 10 items. Answer 7 items right on the first attempt, the rest wrong three times. New run: 8 items right. Then a word session of 100 items with 79 right, and a new run with 80 right. Repeat the first two runs with `TX(...)`. | 7 of 10: `TEXT(2,7,"Score 7/10")`, `TEXT(3,3,"Not passed, need 8")`, no EEPROM erase or write. 8 of 10: `TEXT(3,4,"Passed, new level")` (each session at the earned level), one erase of page 1 and one record write. 79 of 100: `TEXT(2,6,"Score 79/100")`, `TEXT(3,3,"Not passed, need 80")`. 80 of 100: `TEXT(3,4,"Passed, new level")`. Sending gives the same texts. A character session still needs 45 (V-105, V-106). |
| V-137 | Power-on, empty EEPROM. Cursor on `Receiving`, press `enter`. Then `prev`, `next`, `next` held for 1000 ms, `prev` held for 1000 ms. Repeat with `Sending`. | `HDR("RX 32/32 L1          1/50")`, `BODYOFF(1,3)`, `LVL(1,1)` (no marker), `DESC("K and M")`, `BLANK(6)`, `STARTBTN`. `PLOG` empty, tap mode off, no EEPROM erase or write. Every later event: `DIRTY()`. Sending: `HDR("TX 20/20 L1          1/50")`, the same rows, and the keyer is not started. (UX-53, UX-54) |
| V-138 | `EARN(RX, 19)`, `Max Word Length` 5. `OPEN(RX)`. Then `R`, `M`, `M`, `R`, `R`. | Start: `HDR("RX 32/32 L20         1/50")`, `LVL(20,20)`, `DESC("New char J")`. `R`: `DIRTY()`. First `M`: `HDR("RX 32/32 L19         1/50")`, `LVL(19,20)`, `DESC("New char N")`, `DIRTY(0,4,5)`. Second `M`: `HDR("RX 32/32 L18         1/10")`, `LVL(18,20)`, `DESC("Words of 4")`. The two `R`: back to the start rows. (UX-54, UX-55) |
| V-139 | `EARN(RX, 19)`. `OPEN(RX)`. Hold `prev` from 0, release at 1000. Hold `prev` from 2000 for 3000 ms. Hold `next` from 6000 for 3000 ms. | First hold: steps at 0 and at 400, 480, ..., 960: `LVL(11,20)` after the release. Second hold: steps at 2000 and at 2400 + 80k up to `LVL(1,20)` at 3040. Then `DIRTY()` up to the release, also at the long event (2800). No wrap. Third hold: `LVL(20,20)` at 7760 (the press and 18 repeats). Then `DIRTY()`. (UX-55) |
| V-140 | For each row: `EARN(RX, i)`, `OPEN(RX)`. i = 0, 1, 2, 17, 58, 60, 61 for each `Prosigns` value. i = 62, 63, 64 with `Max Word Length` 5 and 8. | 0: `DESC("K and M")`. 1: `DESC("New char R")`. 2: `DESC("Words of 2")`. 17: `DESC("Words of 4")` (max 5). 58: `DESC("New char <BT/=>")` (`Both`), `DESC("New char <BT>")` (`Prosign`), `DESC("New char =")` (`Punctuation`). 60: `DESC("New char <SK>")` for every value. 61: `DESC("New char <AR/+>")`, `DESC("New char <AR>")`, `DESC("New char +")`. 62: `DESC("Words of 5")` or `DESC("Words of 8")`, `HDR("RX 32/32 L63         1/10")`. 63: the same text, `HDR("RX 32/32 L64         1/10")`. 64: `HDR("RX 32/32 L65         1/11")`. (UX-54) |
| V-141 | `EARN(RX, 19)`. `OPEN(RX)`. `M` 15 times. P3.2 low from t = 0 for 1500 ms. | Before t: `LVL(5,20)`, `HDR("RX 32/32 L5          1/50")`. At 20 (the press): the session starts at L5. Page 0 is not dirty, and the header stays. Tap mode on. `STATUS()`, `BODYOFF(2,5)`, `KEYS("KM","R","S","U")`. At 1020: `play()` of item 1. Nothing at 820 and nothing at the release (1520). (UX-55) |
| V-142 | `EARN(TX, 19)` at `TX Speed` 20. `OPEN(TX)`. `M`. P3.2 low from t = 0 for 2000 ms. | Start screen: `HDR("TX 20/20 L20         1/50")`, the keyer is not started. After `M`: `HDR("TX 20/20 L19         1/50")`. At 20: the keyer starts once with the settings of the mode entry. No DIT event and no tone from the held contact. At 1020: item 1. (UX-55, section 5.13.4) |
| V-143 | `EARN(RX, 19)`. `OPEN(RX)`. `M` 3 times. Hold `menu/back` for 1000 ms. Then `OPEN(RX)` again. Repeat with `EARN(TX, 19)` and `OPEN(TX)`. | At the press: main menu, `SEL(1)` on `Receiving`, `TEXT(1,20,"L20",inv)`. The long event at 800 ms changes nothing. No player call, no EEPROM erase or write. After the second `OPEN(RX)`: `LVL(20,20)`. Sending: `SEL(2)`, the keyer was never started. (UX-55) |
| V-144 | V-105 setup. Press `next` 100 ms after the release of the last pick (during `Correct`) and hold it for 2000 ms. Then `R`. | At the end screen: tap mode off. The held `next` gives no step, no repeat and no long event: `LVL(2,2)` stays, `DIRTY()`. After the release, `R`: `DIRTY()` (the earned level). Repeat with `M` held: no step until a new press. (UX-55) |
| V-145 | `EARN(RX, 19)`. `OPEN(RX)`. `M` 15 times (L5). `E`. Answer 47 of 50 right on the first attempt. Then `R` 15 times. Then `menu/back`. | End screen: `HDR("RX 32/32 L5         50/50")`, `STATUS()`, `TEXT(2,7,"Score 47/50")`, `TEXT(3,9,"Passed")`, `LVL(6,20)`, `DESC("Words of 2")`, `STARTBTN`. No EEPROM erase or write. The fake progress keeps index 19. After 14 `R`: `LVL(20,20)`, each step `DIRTY(4,5)`. The 15th `R`: `DIRTY()`. Main menu: `TEXT(1,20,"L20",inv)`. (UX-56, plan example) |
| V-146 | As V-145 with 44 first-attempt correct items. Then `E`. | `TEXT(3,3,"Not passed, need 45")`, `LVL(5,20)`, `DESC("New char U")`. No EEPROM erase or write. After `E`: `HDR("RX 32/32 L5          1/50")`. |
| V-147 | `EARN(TX, 19)`. `OPEN(TX)`. Hold `prev` until `LVL(1,20)`. `E`. Pass the session. New run: `OPEN(TX)`, `E` at L20, pass, `Max Word Length` 5. | Run 1: `TEXT(3,9,"Passed")`, `LVL(2,20)`, `DESC("New char R")`, no EEPROM erase or write. Run 2: `HDR("TX 20/20 L20        50/50")`, `TEXT(3,4,"Passed, new level")`, `LVL(21,21)`, `DESC("Words of 5")`, one erase of page 1 and one record write. (UX-56) |
| V-148 | `EARN(RX, 65535)`, RX Speed 50. `OPEN(RX)`. Pass a session at L65535 (80 of 100). New run: `OPEN(RX)`, `M` (index 65534), `E`, pass. | Start screen: `LVL(65535,65535)` with `<` in column 2 and no `>`. Run 1 end screen: `TEXT(3,9,"Passed")`, `LVL(65535,65535)`, no EEPROM erase or write (D-T17). Run 2 start screen after `M`: `<  L65535 of 65535  >` (both markers, D-T5). End screen: `TEXT(3,9,"Passed")` (a replay), `LVL(65535,65535)`, no `>`. (A-L5) |
| V-149 | Fixture: Receiving index 11 at 32 WPM and index 4 at 20 WPM. Sending index 0 at 20 WPM and index 65534 at 25 WPM. Power-on. `next`. Then in Settings > CW: `RX Speed` 32 -> 20, `TX Speed` 20 -> 25. Back to the main menu. | Power-on: `TEXT(1,0,"Receiving",inv)`, `TEXT(1,20,"L12",inv)`, `TEXT(2,0,"Sending")`, `TEXT(2,21,"L1")`. Rows 3 and 4 as V-01: no level. After `next`: `SEL(2)`, `TEXT(1,20,"L12")`, `TEXT(2,21,"L1",inv)`, `DIRTY(1,2)`. Back from Settings (cursor on `Settings`): `TEXT(1,21,"L5")`, `TEXT(2,17,"L65535")`. No EEPROM access other than the Settings save. (UX-57) |
| V-150 | V-105, then `menu/back`. New run: Reset Statistics `Yes`, `OK`, back to the main menu. | First run: `TEXT(1,21,"L2",inv)`. After the reset: `TEXT(1,21,"L1")` and `TEXT(2,21,"L1")`. (UX-57) |
| V-151 | Every screen of section 5.16 at every selected level from L1 to L65 (`EARN(RX, 64)`), each `Prosigns` value, `Max Word Length` 4 and 8. | Every selector line fits columns 2-22, every description fits columns 5-19. Only ASCII 0x20-0x7E. Rows 1-3 and 6 are blank on the start screen. Row 6 is blank on the end screen. |

### 9.5 Startup screen and About tests (sections 5.17, 5.18)

In this section, "power-on" does not skip the startup screen. The tests use `fake_tick.c` and send button events to
the menu. `t` is in ms after `menu_init()`. A fake tick is exact, so the `+/- 2 ms` tolerance does not apply.

Probes:

- `TEXT5(x, y, s)`: the 5 x 8 cells from the pixel (x, y) match a reference rendering of `s`, normal video.
- `BIG16(x, y, s)`: the 8 x 16 cells from the pixel (x, y) match a reference rendering of `s` in `FONTS_ID_8X16`,
  normal video. It uses the golden pixels of the 8 x 16 font, as the training screen tests do.
- `SPLASH(v)`: `TEXT5(82, 8, "Putikeeg")`, `BIG16(7, 16, "CW Trainer")`, `TEXT5(7, 32, "mod by UT2UBM")` and
  `TEXT5(122 - 5 * len(v), 56, v)`. All other pixels of the screen are off. This includes pages 0, 5 and 6, and
  x = 0, 1 and 127 in every row.
- `ABOUT(v)`: `TITLE`, `TEXT(0,0,"About",inv)`, `TEXT(1,0,"Putikeeg CW Trainer")`, `TEXT(2,0,"mod by UT2UBM")`,
  `TEXT(3,0,"Version " + v)`, all pixels of row 4 off, `TEXT(5,0,"https://github.com/Quard/")`,
  `TEXT(6,0,"putikeeg_cw_trainer__")`, `TEXT(7,0,"ut2ubm")`. `PLAIN(r)` for r = 1..7: no highlight band.
- `MAIN(r)`: the main menu of V-01 with the cursor on row `r`: `SEL(r)`, and `PLAIN` on the other item rows.

| ID | Setup / action | Expected result |
| --- | --- | --- |
| V-152 | Power-on, empty EEPROM. No button event. Poll up to t = 4999. | `SPLASH("v0.1.0")`: `v0.1.0` from (92, 56). The menu reports the startup screen. No sidetone call, no keyer start, no EEPROM erase or write. After the first flush, no page is dirty up to t = 4999. |
| V-153 | Continue V-152. Poll at t = 5000. Then poll up to t = 8000. | At t = 5000: `MAIN(1)`. Pages 0-5 and 7 change and are dirty. Page 6 is blank on both screens, so it is not dirty. After that, no change and no dirty page. |
| V-154 | Power-on. At t = 500, one press event of `menu/back`. Repeat the test with `prev`, `next` and `enter`. | Each run: `MAIN(1)` at t = 500. No item opens, no edit mode, no cursor move, no EEPROM access, no sidetone call. At t = 5000 nothing changes and no page is dirty. |
| V-155 | Power-on. At t = 500, send one repeat event of `next` as the first event. Repeat the test with a repeat of `prev`, and with the long event of `menu/back`, `prev` and `next`. | Each run: the same as V-154. The event ends the startup screen and does nothing else. |
| V-156 | Power-on. P3.2 (paddle tip) low from t = 500 for 50 ms. | One `enter` press event. `MAIN(1)`. Receiving does not open. |
| V-157 | Power-on. Hold `next` from t = 500 (press event) for 1000 ms. | At t = 500: `MAIN(1)`. The repeats of the hold move the cursor after the skip (UX-59): `SEL(2)` at t = 900, and the cursor stops on `About` (`MAIN(5)`), as V-04. The long event at t = 1300 changes nothing. |
| V-158 | Power-on. Hold `prev` from t = 500 (press event) for 1000 ms. | `MAIN(1)` from t = 500 to the end. The repeats do not wrap (section 6.3). No page is dirty after t = 500. |
| V-159 | P3.2 low from `buttons_init()`, P0.2 high. Keep P3.2 low for 6000 ms, then release. Then press `enter`. | `SPLASH("v0.1.0")` up to t = 4999. `MAIN(1)` at t = 5000. Nothing opens while P3.2 stays low or at the release. The new press opens the Receiving start screen. |
| V-160 | Recovery start as V-33. Release P0.2 at t = 300. Press `enter` at t = 1000. Press `enter` again. Repeat the test with `next` and with `menu/back` as the first press. | Up to t = 999: `SPLASH("v0.1.0")`. The release gives no event. First press: the `Display Defaults` message of V-33. It stays open. Second press: `MAIN(1)`. |
| V-161 | Recovery start as V-33. Release P0.2 at t = 300. No other button event. | `SPLASH("v0.1.0")` up to t = 4999. At t = 5000 the `Display Defaults` message of V-33. It stays open at t = 15000. |
| V-162 | Main menu (`MAIN(1)`). Press prev. | `MAIN(5)`: `TEXT(5,0,"About",inv)`. Cell (5,24) is inside the band and has no `>` (UX-61). |
| V-163 | Main menu, cursor on `About`. Press `enter`. | `ABOUT("0.1.0")`. No sidetone call, no keyer start, no EEPROM access. |
| V-164 | Open `About` as V-163. Press `enter`. Open it again and press `menu/back`. Open it again and close P3.2 for 50 ms. | Each time: `MAIN(5)` with the cursor on `About`. No EEPROM access. |
| V-165 | Open `About`. Press prev. Press next. Hold next for 1000 ms. Long `menu/back`, long `prev`, long `next`. | After each event: the framebuffer is unchanged, no page is dirty, the `About` screen stays open, no sidetone call. |
| V-166 | Check the version macro. | `FIRMWARE_VERSION` is `"0.1.0"`. `SPLASH` uses `v0.1.0` from column 18 (x 92). `ABOUT` row 3 is `Version 0.1.0`, columns 0-12. |
| V-167 | Test build with `FIRMWARE_VERSION` = `10.12.3`, then with the 17-character `12345.12345.12345`. | `10.12.3`: `TEXT5(82, 56, "v10.12.3")`, `TEXT(3,0,"Version 10.12.3")`. 17 characters: `TEXT5(32, 56, "v12345.12345.12345")`, and `About` row 3 ends in column 24. No ink at x 0, 1 or 127. |
| V-168 | Build with an 18-character version, for example `123456.12345.12345`. | The build stops (UX-63). |
| V-169 | Render the startup screen and the `About` screen. | No 5 x 8 text reaches past column 24. The 8 x 16 line is inside x 7-86 and y 16-31. Only ASCII 0x20-0x7E. |

## 10. Durable decisions

| ID | Decision | Reason | Rejected alternatives |
| --- | --- | --- | --- |
| UX-1 | 8 x 25 grid, x origin 2, 2 px left margin and 2 px right margin (glyph column + x 127). | Symmetric margins. Probes at x 0, 1, 127 never hit text. | x origin 0 (text touches the glass edge in a highlight). |
| UX-2 | Inverted title bar 8 rows. Highlight band 7 rows (`8r+1`..`8r+7`) with an off gap line at `8r` across the width of the band. | Keeps the title bar and a selected row 1 apart while keeping 7 visible items and page-aligned rows. | Full 8-row highlight: merges with the title bar. 10-row title with 6 items: breaks page alignment, loses a row. Title in normal video: weak separation. Clipping cell row 0: cuts `l`, `t`, `h`. |
| UX-3 | Labels and value formats in section 5.3, fixed field width per item. `RX`/`TX` labels decided by the user. | Fits 25 columns with edit markers. Value does not jump while it changes. | Full words `Receive`/`Send` (do not fit with markers). `Recv`/`Send` short forms. |
| UX-4 | Edit mode = highlight shrinks to the value field, `<`/`>` markers show the possible directions. | Two cues (shape and markers) for the mode. Markers also show the limits. | Blinking value (needs a timer, weak on slow LCD). `*` marker only (too small). |
| UX-5 | Scroll window of 7, minimum movement, `i/n` in the title bar. | No extra pixels. Testable as text. | Scroll bar (needs a column). |
| UX-6 | Placeholder: title + `Not ready`. Only a long menu/back works (UX-31). No menu item opens it in this version: Free Input, Receiving and Sending are real modes. | Plan, user. | - |
| UX-7 | Dialog `No`/`Yes`, focus `No` at every open, menu/back = `No`. | Plan. Also protects against a spurious `enter` from the paddle socket. | - |
| UX-8 | `Statistics reset` / `OK` message after `Yes` (accepted by the user). | Gives a completion signal for an action that cannot be undone. | Silent return (the user cannot tell `Yes` from `No`). |
| UX-9 | Control mapping in section 6.1. Menu/back at the root does nothing. In lists, short menu/back goes back. In mode screens, long menu/back exits. | One rule for back in each screen family. No hidden function at the root. | menu/back at the root toggles something (hidden mode). |
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
| UX-23 | UX-51 adds a long `enter` in tap mode (Receiving only). Long press: 800 ms after the press event, on `menu/back`, `prev` and `next`. One action per press, at the threshold while the button is held. No feedback other than the result of the action. | The user decided on long presses. 800 ms is twice the repeat delay, so a normal press or a short hold never triggers it. Exit and clear destroy the text, so the hold must be deliberate. The screen changes during the hold. This is the signal to release. | 500 ms (close to a slow press, risk of lost text). 1500 ms or more (slow for a frequent clear). Action at the release (the user cannot see that the hold is long enough). A beep (mixes with the sidetone during practice). A progress bar (pixels and code for a 0.8 s wait). A separate value per button (more to learn, no gain). |
| UX-24 | UX-43, UX-47 and UX-55 refine this decision: the Receiving keyboard and the session end screen act on short presses. In mode screens, short presses, repeats and `enter` do nothing. In lists, dialogs and message screens, long events do nothing. | The other hand rests near the buttons during keying: an accidental short press must not lose the text. Each screen family uses one gesture set, so one press never acts twice. | Short menu/back exits Free Input (the user asked for long). Short prev/next scroll the text (needs a text buffer). |
| UX-25 | Free Input screen: `FONTS_ID_5X8`, the grid of section 4.1, title bar `Free Input` in row 0, text in rows 1-7 (7 x 25 cells). No status, no cursor mark, no hint. | Same font, grid and title rule as every other screen. 175 visible characters keep about 30 words of context. One text row is one LCD page, so a scroll is a page copy. The settings are one menu level away, and the sound shows the speed and the key type at once. | `FONTS_ID_8X16` (16 x 3 with a title: 48 characters, a scroll every 16 characters, a second grid, a scroll moves 2 pages per line). No title (one more line, but the screen does not say which mode is open). Key type or speed in the title (code and strings for a value the user just set). A blinking or static cursor (code and a timer, small gain). |
| UX-26 | Text rules of section 5.9. A character comes at the character gap. A word space is a blank cell. The wrap is per character and lazy. A scroll moves the text up by one line. A line never starts with a space. No text buffer. | Cheapest in RAM and flash: the framebuffer is the only store. The lazy wrap never shows an empty last line. Dropped edge spaces keep the left edge aligned. | Word wrap (needs a buffer of the current word). Clear a full screen (loses all context at once). Show the elements of the character in progress (code, and the sidetone already gives this feedback). |
| UX-27 | Unknown glyph: `*`. One cell for any pattern that is not in the table, more than 6 elements, the 8-dit error sign, or a lost event. | User decision: one unknown glyph. `*` has no Morse code, so it never looks like a correct character. ASCII, no ink in cell column 4. | `?` (a valid Morse character). An inverted cell (heavy in running text, more code). `#` (touches its neighbours). Nothing (hides the error from the student). A separate glyph for the error sign (the user decided against it). |
| UX-28 | UX-38 refines this decision: AR, BT, KN and AS follow the `Prosigns` setting, and the rule below is its value `Punctuation`. Prosigns: AR = `+`, BT = `=`, SK = `<SK>` (four cells, per-character wrap). Prosigns with the code of a punctuation mark show that mark (KN = `(`, AS = `&`). Others are `*`. | User decision (Q-9). SK has no punctuation mark. The brackets show a prosign, not the letters `S` and `K`. `<` and `>` are edit markers only in lists, and Free Input has no edit mode, so there is no conflict. Per-character wrap needs no extra rule. | `SK` (looks the same as `S` then `K`). Keep `<SK>` on one line (needs a look-ahead rule for one token). A rare ASCII sign such as `%` (the student must learn a code for it). `*` for SK (a correct prosign must not look like an error). |
| UX-29 | Delimiter: a 1 px line at `y = 8r + 3`, x 2-126, in its own text row. The next character starts a new row. | Clear separation of practice blocks for about 30 bytes of code. A continuous line cannot look like keyed text. | A blank row (looks like unused space). A row of `-` glyphs (looks like keyed `-` characters). A dashed line (more code, no gain). |
| UX-30 | Clear: rows 1-7 off, cursor home, the character in progress dropped. Title not redrawn. No question to the user. | Practice text has a low value, and the 800 ms hold protects it. Dropping the partial character gives a clean restart. | A Yes/No dialog (slow for a frequent action). Keep the partial character (its first elements are from before the clear). |
| UX-31 | Receiving and Sending placeholders: `Not ready`, only a long menu/back exits. The training modes replace them and keep the long menu/back exit (UX-43, UX-45, UX-49). | User decision: long menu/back exits any mode. The placeholders follow the same rule as the real modes. The real modes later keep the same gesture. | Short menu/back in placeholders only (two exit rules for the same screen family). |
| UX-32 | `Paddle` setting in CW, 7th item: choice `Normal`, `Reversed`, fw 8, default `Normal`. `Normal`: tip = dit, ring = dah. It applies at the next Free Input start. | User decision: a setting. `Normal`/`Reversed` say what changes better than `Off`/`On`. Fit rule: 6 <= 12. The CW list fills all 7 rows without a scroll. Old records read as `Normal`. | `Paddle Swap` with `Off`/`On` (less clear which side is the dit). `Swap Paddle` (a verb as a label). A swap gesture at the mode start (hidden function). |
| UX-33 | UX-45 extends this decision to Sending (section 5.13.4). P3.2 belongs to the keyer while Free Input is open, and to the menu at all other times. A contact closed at the mode start is ignored until it opens. A contact held at the mode end gives no `enter` until it opens and closes again. | The `enter` press that opens Free Input must not key a dit. A held lever at the exit must not open a menu item. A mono plug then works as a straight key. | Ignore the paddle for a fixed time after the start (a held contact still keys after the delay). Use a different button to open Free Input (the hardware has none free). |
| UX-34 | UX-45 refines this decision. The keyer and its sidetone run only in Free Input and in a Sending session (UX-45). In the menu the paddle gives no tone. | The tip acts as `enter` in the menu: a tone on each menu `enter` is noise. One clear place for practice. | A practice tone in the menu (noise, and conflicts with the Tone preview). |
| UX-35 | Key Type semantics of section 6.7. The thresholds come from `TX Speed` and `TX Eff. Speed`. A straight-key mark of 2 dits or longer is a dah. | User decision: no adaptive decoding. The student practises at the speed of the settings. | Adaptive speed tracking (more code, and the student gets no feedback on the speed). |
| UX-36 | No hint about the long `menu/back` on the placeholders or in Free Input. The user manual explains it. | User decision (Q-7). No string and no code. Free Input has no free row. | A `Hold menu to exit` line in row 5 of the placeholders. |
| UX-37 | No stuck-key timeout. A contact that closes after the mode start keys until it opens or the user exits (F-9). The manual tells the user to insert the plug before opening Free Input. | User decision (Q-8). The state is harmless, and the recovery is one long `menu/back`. No keyer code. | Mute or mask a contact that stays closed for some seconds (keyer code, and a new rule for a real long mark). |
| UX-38 | `Prosigns` setting (sections 5.9 and 5.10). Label `Prosigns`, `Settings` index 2 after `Show CW Code`, choice `Punctuation`, `Prosign`, `Both` in this order, fw 11, default `Both`. AR, BT, KN and AS show `+` `=` `(` `&`, or `<AR>` `<BT>` `<KN>` `<AS>`, or `<AR/+>` `<BT/=>` `<KN/(>` `<AS/&>`. SK is `<SK>` for every value. A record of the earlier firmware gives `Both` and keeps the other settings, with no message (F-14). A text of up to 6 cells wraps per cell (UX-26). The value applies at the next Free Input start. | User decisions: the values, their order, the default, SK unchanged, the item in Settings. Label: `Prosigns` names what the setting changes and fits (8 of 9). Position: next to `Show CW Code`, the other item about how characters show. The CW list is full, and Display holds screen hardware settings. A setting is read once per mode start, as `Paddle` (UX-32), because the settings cannot change while Free Input is open. Per-cell wrap keeps one wrap rule for all text. The brackets bound every prosign text, so the inner `/` of `Both` stays readable next to a keyed `/`. | Label `Prosign` (the same word as a value: the row reads `Prosign  Prosign`). `Prosign Style`, `Prosign As` (do not fit: 13 and 10 of 9). `AR Style` (names one prosign). Index 1 before `Show CW Code`, or after `Max Word Length` (splits the two character-display items). Keep a prosign text on one line (a look-ahead rule, and an empty cell run at a line end). A space inside the `Both` text (`<AR +>`: looks like a word space). A message after the update (UX-17). A live preview of the text in the menu (no free row, and it needs a second draw path). |
| UX-39 | UX-53 adds the start screen header: the selected level and `1/<n>` of that level. Training header in row 0, inverted: `RX`/`TX` in columns 0-1, `<speed>/<eff>` from column 3, `L<level>` from column 9, `<i>/<n>` right-aligned to column 24. The worst case `RX 50/50 L65535   100/100` fills 25 columns, so there is no short form. | The header takes the place of the title bar and keeps its look (UX-2). `RX`/`TX` name the mode and the speed settings with the user's terms (UX-3). Fixed start columns keep the fields still while the numbers change. `i/n` at the right edge matches the list position of section 5.1. | The mode name `Receiving` in the title (no room for the numbers). Leading zeros `07/50` (section 4.3 forbids them). One speed for two equal speeds (the format then changes with the settings). A normal-video header (weak separation from the status line). |
| UX-40 | UX-51 splits the key bar line into four segments, one per label field. Training frame: the status line is row 1 and the item line is pages 3-4. Row 5 holds the Sending answer code. The key bar line is at y 55. Row 7 holds the labels or the keyed code. Rows 2 and 6 hold no text. No highlight band in a session. | Each area has its own LCD pages, so a change dirties few pages. The status text is above the item, so `Wrong. The answer is:` reads before the answer. The labels are at the bottom edge, next to the buttons. | Status below the item (the answer text reads after the answer). Labels as inverted key caps (four highlight bands break UX-2). |
| UX-41 | Item line in `FONTS_ID_8X16` at cell top y 24. A character item is centered (`64 - 4m`). A word uses a 16 px pitch (`68 - 8n + 16k`). Receiving marks the next slot with a 7 x 14 block and empty slots with a 7 x 2 bar. Sending marks the next character of a word with the bar. | The item is the thing the user looks at, so it gets the large font. One blank cell between letters keeps the slots countable. The block is the input focus, the only focus mark on the screen (UX-2). The bar sits under the glyph rows of the Koch set. | `FONTS_ID_5X8` for the item (small for a glance at 30-60 cm). `_` glyphs for empty slots (1 px, weak). An inverted glyph for the Sending cursor (the large plain glyph reads best, and a character item needs no cursor). |
| UX-42 | UX-51 replaces the three runs, their split rule, the label fields, the key bar line and the full text for one character. The narrowing presses, the list and range labels and the `-` range mark stay. Group keyboard: the set in keyboard order splits into three runs by the rule of `training_spec.md` section 8 (left `(c + 2) / 3`, middle `(c - left + 1) / 2`, right the rest). A run of 1 picks, of 2 or more narrows, of 0 does nothing. Labels in fields of 8, 7 and 8 cells. One character: its full text. More characters: a list of short texts, or a range `first-last` for a list that does not fit. Short texts of prosigns: `<BT>` `<SK>` `<AR>`, or the marks in `Punctuation`. A 1 px key bar line at y 55. | User decision: three groups in the case order, and narrowing presses. Equal runs give 3 or 4 presses for 43 characters. A list shows exactly what a run holds. A range uses the alphabet order the user knows. `-` is not in the Koch set. The field sizes fit every label (script check over all Koch prefixes and all three `Prosigns` values). The line marks row 7 as key labels, not content. | A fixed split by class (letters, digits, other: 4 presses for a letter, and empty groups in early levels). Labels in the full `Both` text in lists and ranges (up to 13 cells). A list with separators (does not fit). |
| UX-43 | UX-55 limits tap mode to the session: the start screen and the end screen keep it off. UX-51 extends this decision to `enter`: tap mode covers all four buttons, an `enter` tap selects the right run, and a long `enter` replays. Receiving buttons (section 6.8): tap mode for the whole Receiving screen (`training_spec.md` D-T7). A tap of `menu/back`, `prev` or `next` acts at its release. A hold gives only the long event. Repeats and long `next` do nothing. `enter` replays in Item, also during a playback. The check comes at once at the last pick. A long `prev` undoes one step: out of one group level, else the last pick. Refines UX-24 for Receiving. | The user decision gives long `prev` = undo. With an action at the press, the press of that hold first narrows or picks, and a pick in the last slot starts a check. Tap mode removes that, so a hold never acts as a tap. The check needs no wait. One step per hold is easy to predict. | Picks at the press with a restore at the long event. The screen then changes twice in one hold, and every check must wait 850 ms. Delay only `prev` picks (the middle button is slow). Step out to the whole set at once (two undo meanings). |
| UX-44 | UX-51 refines this decision: the replay is a long `enter` (or a tip closure of 800 ms). Receiving flow: a 1000 ms start pause before the first item (`TRAINING_START_MS`). Picks work during the playback. The check stops a running playback. A retry clears the slots and plays the item again at once. After the third wrong attempt, the answer shows and plays once more. Replays are free, and a replay during a playback starts it again. | The start pause gives the user time to look at the screen after `enter`. Fast users copy while the item plays. The replay on a retry saves one press. Hearing and seeing the answer together supports the learning. A free replay matches the plan. A restart is the simplest rule for `enter`. | A pause before every item (slower sessions, and the blank moment already marks a new item). `enter` ignored during a playback (one more state rule, little gain). A replay costs an attempt (no user decision for it). |
| UX-45 | Sending: each character is checked at its CHAR_GAP by code. A wrong character ends the attempt. The retry starts again at the first character of the item (`training_spec.md` D-T6). Events in the start pause, a delay or a gap are dropped, also the whole character that they start. WORD_GAP in Item: UX-52 (a pause error in a word item, else nothing). OVERFLOW drops the character without an attempt. Long `prev` and long `next` do nothing. P3.2 ownership as UX-33. | Plan: check at once. The whole-item retry matches CW practice: after an error, the operator sends the word again. One attempt rule for both modes. The drop rule never judges a character that the user started during a delay. A lost event is a device fault. A clear has no use in Sending. | Continue at the failed character (a second retry rule, different from Receiving). Judge a character that started in a delay (unfair, it misses its first elements). OVERFLOW as a wrong character (the user pays for a device fault). |
| UX-46 | Result feedback in the status line: `Correct` (`TRAINING_CORRECT_MS` 600 ms), `Wrong` (Receiving), or `Wrong: <text>` or `Wrong: pause too long` (Sending, UX-52) (`TRAINING_WRONG_MS` 1200 ms), then `Try 2 of 3` or `Try 3 of 3` during the retry. At the retry moment the item looks fresh in both modes: Receiving empties the slots, Sending moves the bar to the first character and clears row 7 (UX-52). After the third wrong attempt (`TRAINING_FAILED_MS` 2500 ms): Receiving `Wrong. The answer is:` with the answer in the item line, Sending the code of the character as marks in row 5. A blank item line before every next item, also for the same character: `TRAINING_RECEIVING_GAP_MS` 500 ms, `TRAINING_SENDING_GAP_MS` 300 ms. `TRAINING_START_MS` 1000 ms before the first item. No result sound. | Short delays keep a 50-item session moving. A wrong result needs time to read. The answer needs time to learn. The blank moment shows every new item, also a repeat of the same item. The Receiving gap gives the ear a rest before the next sound. In Sending, the user needs to know what they keyed (`Wrong: N`), and the answer of Sending is the code. A result beep mixes with the CW. | A result tone (mixes with the sidetone and the playback). An inverted `Wrong` band (breaks UX-2, and the text is clear). A blank moment only for the same character (two transition looks). Delays as settings (more menu items, no user request). |
| UX-47 | UX-56 replaces the level row, `Not saved` in row 5, `Next` and the Receiving tap rule of this decision: rows 4-5 hold the level selector, `Not saved` is in row 1, the button is `Start`, and tap mode is off on the end screen. UX-51 refines this decision for Receiving: the `enter` tap acts at the release, and a long `enter` does nothing. Session end screen: `Score c/n`, `Passed` or `Not passed, need t`, `New level`, `Repeat level` or `Top level`, `Not saved` after a failed save, a focused `Next` in row 7. `enter` or the tip starts the next session. A short or long `menu/back` opens the main menu. No timeout. | The score, the threshold and the new level answer the questions after a session. `Next` follows the message-screen pattern (one focused button, `menu/back` leaves). The tip as `enter` lets the user start from the paddle. | A `Yes`/`No` dialog (two presses for a frequent action). Auto start of the next session (no rest, and a missed score). Only a long `menu/back` on the end screen (the session is over, and nothing is lost). |
| UX-48 | User decision (Q-11). Word items and the word keyboard hold no prosign (BT, SK, AR). Prosigns appear only in character sessions. | A prosign text is 4 or 6 cells. A word of 8 slots with prosign texts needs up to 48 cells, which no line of the screen holds. In CW a prosign is sent as a word of its own. A smaller word keyboard needs fewer presses. | Prosigns in words with a 5 x 8 fallback line (two fonts, a reflow after a wrong prosign pick). Prosigns as one-cell marks in words (SK has no mark). |
| UX-49 | A session left with a long `menu/back` is dropped with no question: the level and the EEPROM do not change. The settings are read once at the mode start, and a next session keeps them. | The 800 ms hold is the protection (UX-23). Progress changes only at a passed session end, so an exit cannot damage it. The settings cannot change inside a mode. | A `Leave session?` dialog (slow, and the loss is one session). A partial score. |
| UX-50 | `Show CW Code` in Sending: row 7 shows a mark per element (dot 2 x 2 px, dash 6 x 2 px, gap 3 px) from x 2. The marks stay until the next character starts, the retry or the blank moment (UX-52). At most 12 marks. The answer code in row 5 shows for every value of the setting. No effect in Receiving. | The plan gives this use of the setting. Graphic marks sit on one line. The `.` and `-` glyphs sit at different heights. Keeping the marks after the character shows what a wrong character was. The answer shows for both values of the hint. | `.` and `-` glyphs (different heights, small dot). The code of the expected character as a hint before keying (not the plan). |
| UX-51 | User decision 2026-10-06: the Receiving keyboard uses all four buttons (sections 5.12.1, 6.8). Four runs in the case order `menu/back`, `prev`, `next`, `enter`. Run `k` of a group of `c` characters has `(c + 3 - k) / 4` characters (`training_spec.md` D-T27). Tap mode covers all four buttons. An `enter` tap (or a tip closure shorter than 800 ms) selects the right run. A long `enter` (or a tip closure of 800 ms) replays in Item and does nothing in the other states and on the end screen. Long `menu/back` exits, long `prev` undoes one step, long `next` does nothing. Labels in row 7 only, in fields x 2-26 (5 cells, left-aligned), 30-54 (5 cells, centered), 58-82 (5 cells, centered) and 87-126 (8 cells, right-aligned), with a key bar segment at y 55 above each field. Label text of a character: `morse_format()` in `MORSE_STYLE_NAME` (`Prosign`, `Both`) or `MORSE_STYLE_PUNCTUATION` (`Punctuation`). One character: its text. More characters: a list if it fits the field, else a range `first-last`. Refines UX-42, UX-43, UX-44 and UX-47. | User: three groups for four buttons were confusing. One run per button puts each label above its own button. The balanced split keeps the keyboard order and gives 3 presses for every pick with 43 or 40 characters, at most 2 up to 16 characters and 1 up to 4. The pick is the frequent action, so it gets the tap. The replay is rare, so it gets the hold of the same button. Tap mode keeps a hold from selecting first. On the end screen, a hold never acts as a tap (UX-43), and the user taps again. The field widths fit every label of every Koch prefix for all three `Prosigns` values (script check, section 5.12.1) and keep at least 4 blank pixels between two labels. Only the right run can hold two prosigns, so only its field is wider. One text per character keeps the labels short. The user hears the name, so `<AR>` is enough to find AR. The brackets stay, so a prosign never reads as two letters. The key bars show four keys and the field limits, also for a blank label. | Keep three runs and `enter` for replay only (the user found it confusing). `enter` tap = replay and long `enter` = right run (every pick on the right run then costs 800 ms). Four equal fields of 6 cells (1 blank pixel between fields, so four adjacent lists read as one string). Labels in two rows, rows 6 and 7 (LC-2 allows it only when no one-row rule fits, and the key bar must move). Prosign names without brackets in lists and ranges (`SKAR`, `SK-AR` read as letters). The marks `+` and `=` in the labels for `Prosign` (the user chose names). The full `Both` text for one character (`<BT/=>` and `<AR/+>` need 6 cells). One unbroken key bar line (no field limits). Long `enter` starts the next session on the end screen (a second gesture for one action). |
| UX-52 | User decision 2026-10-07 (sections 5.13, 5.14). (1) The retry clears row 7. The retry moment comes `TRAINING_WRONG_MS` after the check. Its redraw shows `Try 2 of 3` or `Try 3 of 3`, moves the bar to the first character and turns row 7 off. This applies to both `Show CW Code` values. Before that moment, row 7 keeps the wrong code. (2) Pause limit in a Sending word item. Rule: a WORD_GAP in Item counts when it ends the space right after a correct character that is not the last one of the item. It ends the attempt like a wrong character. It uses an attempt. The retry is the whole item (D-T6). The third one is the failed path. The status is `Wrong: pause too long` (21 cells, 1200 ms, or 2500 ms after attempt 3). The bar stays under the next character. After attempt 3, row 5 shows the code of that next character. A WORD_GAP does nothing before the first correct character of the attempt, in a character item and in a delay. After OVERFLOW it also does nothing: a lost event is a device fault (D-T16). The limit is the keyer word space, 5 Farnsworth units at `TX Eff. Speed`. Refines UX-45, UX-46 and UX-50. | The user wants the retry to look like a fresh item, with the same `Try N of 3` status in both modes. The wrong code must stay readable for the whole delay, so the clear waits for the retry. In CW the space inside a word must be shorter than a word space. Otherwise the receiver hears two words. The keyer already measures this space for Free Input, so both modes use one rule and one threshold. The `Wrong: ` prefix keeps the UX-46 grammar: the user first reads that the attempt failed, then why. A keyed character text has no space, so the three words never read as a character. The bar marks where the pause came. A long pause often means the user did not recall the next code, so the failed path shows that code: the same rule as a wrong character (the expected character at the failed position). | Clear the status at the retry (the user chose to keep `Try N of 3`, as in Receiving). Clear row 7 at the check (the user cannot read the wrong code). `Wrong: KM` plus a space mark (the font has no space glyph, `_` and `-` are keyable characters, and the text does not say why). `Gap too long` (`gap` has other meanings in this file: the start pause and the blank moment) or `Pause too long` without the prefix (breaks the UX-46 grammar). A blank row 5 after a third pause error (no help for a user who did not recall the code). A limit of 3 or 7 units, or an own setting (the user chose 5 units, the Free Input point). A pause error before the first character (nothing to measure from: the space after the item start is not inside the word). |
| UX-53 | User decision 2026-10-07 (section 5.16.1). Receiving and Sending open with a start screen at each entry, also at L1. The selected level is the earned level. Header: the selected level and `1/<n>` of that level. Rows 1-3 blank, selector in row 4, description in row 5, a focused `Start` in row 7 (band x 47-81). No session, no player, no keyer, no sound. | The user can repeat a done level before a session starts. The rows match the end screen, so the selector is in one place. `1/<n>` is the start pause header (rule of 5.11.1), so the session start does not redraw page 0, and the item count shows the size of a final word level. | Skip the start screen at L1 (the screen then changes with the level, and the user loses the moment to get ready). Item field `0/<n>` (a new format) or blank (hides the size of a final level). Selector in rows 3-4 on the start screen (other rows than the end screen). |
| UX-54 | Selector line `<  L<s> of <e>  >` in row 4, centered with the markers, `c = floor((19 - len) / 2)`. `<` only when `prev` can step, `>` only when `next` can step, as the edit markers (UX-4). Normal video, no band. Description in row 5: `K and M` (L1), `New char <text>` (character levels, full `Prosigns` text), `Words of <len>` (word levels and the final word phase). | User decisions 2026-10-07: the selector style and short words. The markers show the possible directions and the limits with no extra pixels. `of <e>` shows the earned level, so a replay never looks like a lost level. The final phase needs no own text: the header shows the item count. One text per level kind keeps the code and the strings small. | A highlight band on the selector (two bands on one screen, UX-2). Markers at fixed columns 2 and 22 (far from a short number). `Final words of 8` or `11 words of 8` for the final phase (new strings, the count is in the header). The short label text of the keyboard for a prosign (`<AR>` in `Both`): the item line shows the full text, and the description names the same character. |
| UX-55 | Selector controls on the start screen and the end screen, both modes: `prev` and `next` press and repeat step by one, stop at L1 and at the earned level, no wrap. `enter` or the tip starts a session at the selected level. A `menu/back` press opens the main menu, cursor on the mode item. Long events do nothing. Tap mode is off on both screens (Receiving turns it on at the session start, off when the end screen shows). A step redraws pages 0, 4 and 5 on the start screen and pages 4 and 5 on the end screen. A press at a limit redraws nothing. | User decision 2026-10-07: press and auto-repeat both act. Repeats need tap mode off. All buttons then act at the press, as in the lists. The limits match the value edit (UX-13). The page budget keeps a step fast during the 80 ms repeat. | Long `prev`/`next` = step of 10 (the fallback of the plan, only if the repeat costs too much flash). Wrap from L1 to the earned level (a surprise at a limit, UX-13). Tap mode on the end screen (no repeats). A long `menu/back` to leave (the screen holds no work to lose). |
| UX-56 | Session end screen with the selector (section 5.15). Row 1 `Not saved` after a failed save. Row 2 score. Row 3 `Passed, new level` (the earned level went up), `Passed` (a replay pass, or a pass at index 65535), or `Not passed, need t`. Rows 4-5 the selector, preselected: pass = session level + 1, fail = session level. Row 7 `Start` (replaces `Next`). The header keeps the session that ended. A replay pass never changes the store. | User decision 2026-10-07: the same selector as the start screen, the preselect rule and the replay rule. The selector shows the next level, so the rows `New level L13` and `Repeat level L12` only repeated its number. Row 3 keeps the reward of a pass that advanced. `Not saved` goes to the status line of the frame. One button text for both screens saves a string, and `Start` is right for every selected level. The header still names the level that the score belongs to. | Keep `New level L<n>` and add the selector in rows 5-6 (a full screen and the same number twice). `Not saved` in row 6, next to the button. `Passed (replay)` or `Passed, level kept` (a new string, and `of <e>` already shows that the earned level stays). The header follows the selection (the score then has no level). `Next` on the end screen. |
| UX-57 | Main menu: `Receiving` and `Sending` show the earned level `L<n>` at the current `RX Speed` or `TX Speed`, right-aligned to column 22, inverted with the selected row like any value. `Free Input` and `Settings` show nothing new. Read at each row draw from the progress image in RAM. | User decision 2026-10-07. Column 22 is the value column of every list (section 4.3), so the level reads as the value of the row. The band shows the selected row as a whole. The draw needs no EEPROM access (D-T9). | A level after the label in column 10 (a new column rule). The level of the effective speed (the progress key uses the character speed). A level in normal video inside the band (a hole in the highlight). |
| UX-58 | Startup screen layout (section 5.17.1). No title bar, normal video. `Putikeeg` 5 x 8 at (82, 8), row 1, right-aligned to column 23. `CW Trainer` 8 x 16 at (7, 16), pages 2 and 3. `mod by UT2UBM` 5 x 8 at (7, 32), row 4, column 1. `v` + version 5 x 8 at (122 - 5n, 56), row 7, right-aligned to column 23. Pages 0, 5 and 6 blank. Drawn once. | Follows the mockup of the user. The 8 x 16 line fills whole pages, as the item line does. Columns 1 and 23 give the same 7 px margin on both sides. The version sits at the bottom, apart from the name. | Text at columns 0 and 24 (margins of 2 px: the text sits at the edge of the glass, and the mockup shows blank cells at both edges). `Putikeeg` in row 0 (no top margin). 8 x 16 line at y 15 (crosses a page edge). A title bar (the screen is not a menu). |
| UX-59 | Startup screen time and skip (section 5.17.2). `MENU_SPLASH_MS` = 5000 ms from `menu_init()`. The first button event of any kind (press, repeat or long, any of the four buttons, also the tip) ends it and does nothing else. The later events of the same hold act on the next screen. A button held at power-on does not skip it. | User decisions: any button skips (plan), and 5 s (2026-10-08, it replaces the 2 s of the plan). The skip never changes a setting or opens an item. A held `next` that scrolls the main menu after 400 ms is a normal list hold. | Swallow the whole press up to the release (needs a new buttons function and flash for a rare case). Skip on `enter` only (the user asked for any button). A countdown or progress bar (redraws and noise for 5 s). 2 s (the value of the plan, replaced by the user on 2026-10-08). |
| UX-60 | Display recovery start order (section 5.17.3): the startup screen, then the `Display Defaults` message, then the main menu. The press that skips the startup screen does not close the message. | The defaults apply before the first frame, so the startup screen shows that the screen works. The message stays until the user closes it. One start sequence for every start. | Message first, then the startup screen (5 s of delay after `OK`). No startup screen on a recovery start (two sequences, no version). |
| UX-61 | `About` is the fifth main menu item, after `Settings`, in row 5. No marker, no value. Kind: info screen. | `>` means "opens a list". Items that open a screen have no marker. Five items fit with no scroll. | `>` on `About` (promises a list). A `...` suffix (not used anywhere else). |
| UX-62 | `About` screen (section 5.18). Title `About`. Rows 1-3: the name, the mod and `Version ` + version. Row 4 blank. Rows 5-7: the link in three parts. All text is left-aligned in column 0, in normal video. No band and no `OK`. `enter` or `menu/back` goes to the main menu with the cursor on `About`. `prev`, `next`, repeats and long events do nothing. | User layout. The link needs three rows, and one left edge keeps it readable as one text. Closing with `enter` or `menu/back` matches the message screen. | Centered text (the link parts lose their common edge). An `OK` button (no free row). A hyphen at the link breaks (changes the link). |
| UX-63 | Version text (section 5.17.4): `FIRMWARE_VERSION` is digits and dots, at most 17 characters. The startup screen shows `v` + version right-aligned to column 23. `About` shows `Version ` + version from column 0. A longer version stops the build. | 17 characters keep `Version ` + version inside 25 columns. A cut text is a defect. A build check finds it before the device does. | No limit (a cut on the glass). A run-time cut or a short form (hides the real version). |

## 11. Device checks (unverified until the user tests on the device)

| ID | Check | Pass condition |
| --- | --- | --- |
| D-1 | Legibility of the font, the highlight, the gap line, `<` and `>`. Conditions: contrast 32, backlight 8 and 0, 30-60 cm, indoor light, straight and at an angle. | All labels and values readable. The gap line between the title bar and a selected row 1 is visible. |
| D-2 | Contrast range 0-63. | Record the lowest and highest readable values and whether 0 or 63 make the screen blank or black. |
| D-3 | Backlight levels 0-10. | No visible flicker. Levels are distinguishable. The screen is readable in room light with level 0. |
| D-4 | Bounce time of the four buttons and of the paddle tip. | All bounce trains are shorter than 20 ms. |
| D-5 | Plug insertion into the paddle socket while the menu shows, and in Receiving: in Item, on the start screen and on the end screen (UX-51, UX-55, `training_spec.md` DC-T7). | Record whether a spurious `enter` occurs and how long the tip stays closed. In Item, a closure shorter than 800 ms is an `enter` tap: record whether the right run is selected. A closure of 800 ms or longer replays. On the start screen and the end screen, record whether a session starts (an `enter` press). |
| D-6 | Time from a button press to the change on the glass. Full-screen redraw time. Behaviour during auto-repeat. | Within 100 ms. Values step at 12.5 per second. |
| D-7 | Save on leaving Settings. | No visible glitch. A button pressed during the save is not lost. |
| D-8 | Physical order of the buttons on the case. | Closed by user facts (section 3.1): `menu/back`, `prev`, `next`, `enter` from left to right (2026-10-06). |
| D-9 | Display recovery start: set contrast 0 and backlight 0, save, switch off. Hold `menu/back` and switch on. | The `Display Defaults` message is readable. The `on/off` button powers the MCU up from off. |
| D-10 | Tone preview at the edit start. Speaker and headphones. Tone 400, 600 and 1000 Hz. Volume at its middle. | The tone starts with no audible delay after `enter`. It lasts about 0.3 s. No click at the start or at the end. The user can judge the pitch. |
| D-11 | Single presses about 0.5 s apart from 600 Hz up and down. | Each press gives one clear beep at the new pitch. |
| D-12 | Hold `next` from 400 Hz to 1000 Hz, then hold `prev` back. | The chain sounds as a series of pitch steps, not as clicks or a buzz. The user can release near a target pitch. The tone ends about 0.3 s after the release, and at the limit also while the button is held. If the chain is unpleasant, record it for `architector` (UX-22 alternatives). |
| D-13 | Stop timing on a logic analyzer (P1.1 or P1.2) while the screen redraws. Leave with `enter` and with `menu/back`. Let a preview time out. | After a leave, the output is quiet within about 10 ms of the event. A timeout ends 300 ms after the hook, plus at most one main-loop pass, plus the 8 ms fall. |
| D-14 | Insert and remove the headphone plug during a single preview and during a held repeat. | The route changes at the next silence. No click on either output. A spurious `enter` from the socket (D-5) ends the edit and stops the tone. |
| D-15 | Long press feel: hold `menu/back`, `prev` and `next` in Free Input. Also use the menu with normal presses and holds of `prev`/`next` for a few minutes. | The action comes while the button is held, about 0.8 s after the press. It never comes by accident during normal menu use. The user finds 800 ms neither slow nor too easy. |
| D-16 | Open Free Input with the `enter` button, and with the dit lever as `enter` (keep it closed for 2 s). Repeat with a straight key on the tip. | No tone and no character until the contact opens and closes again. Then it keys as normal. |
| D-17 | Hold the dit lever (or the straight key) and leave Free Input with a long `menu/back`. Keep the lever closed for 2 s, then release. | The main menu shows. No item opens. After the release, a new `enter` works. |
| D-18 | Iambic A and Iambic B at 20 and 30 WPM: single elements, held levers, squeeze, and squeeze release at different moments. Listen, and capture P1.1 or P1.2 with a logic analyzer. | Dah = 3 dits and gap = 1 dit within 1 ms. On a squeeze release, B gives one more element than A. A squeeze from idle starts with a dit. |
| D-19 | Straight key at TX Speed 15 and 20 WPM, by an operator who keys well. Repeat with TX Eff. Speed 10. | Characters decode as keyed. Word spaces appear at normal word pauses. With TX Eff. Speed 10, a normal character pause does not give a word space. Record the error rate for `architector`. |
| D-20 | `Paddle` = `Reversed` in Iambic B. Key both levers and the `enter` button. | The ring lever gives dits. The tip lever and the `enter` button give dahs. |
| D-21 | Legibility and speed of the Free Input text: 5 x 8 font at 30-60 cm during keying. Key a full screen at 50 WPM with long words, so that the screen scrolls. Measure the 7-page flush time. | The text is readable at a glance. A character appears within about 100 ms after the character gap. No character is lost or turns into `*` during a scroll (F-11). The sidetone has no gap or click during the flush. |
| D-22 | Mono plug with a straight key: insert it before Free Input opens, then key. Then insert it while Free Input is open. | Before: the tip keys, the ring short has no effect. During: record the behaviour (F-9). A long `menu/back` stops the tone. |
| D-23 | Delimiter and clear on the glass. Key text with `-` and `_` characters around a delimiter. | The delimiter line is clearly different from the `-` and `_` glyphs. Clear leaves the title bar intact and gives no flicker of the title. |
| D-24 | Receiving and Sending: a user who has not read the manual tries to leave a session. | Record whether the user finds the long `menu/back`. If not, record it for the user. Decision UX-36: no hint on the screen. The start screen and the session end screen also leave with a short `menu/back`. |
| D-25 | Prosign texts on the glass (UX-38, `keyer_spec.md` DC-P1). In Free Input with `Both`, key AR, KN and AS. Also key KN and then a keyed `)` (`-.--.-`). Key a `/` (`-..-.`) next to AR. Key AR with the cursor in column 22, so that `<AR` and `/+>` split. Repeat with `Prosign` and `Punctuation`. Contrast 32, backlight 8 and 0, 30-60 cm, indoor light, straight and at an angle. | The user reads `<AR/+>`, `<KN/(>` and `<AS/&>` correctly at a glance. The user tells `(` from `<` in `<KN/(>`. The user tells the inner `/` of `<AR/+>` from a keyed `/` beside it. The user joins a split text across the line end. No glyph pair looks joined (`+>`, `&>`, `/(`). If a text fails, record it for the user: the options are the `Prosign` or `Punctuation` value, or a new text (a user decision, then `architector` changes `keyer_spec.md` section 17). |
| D-26 | Item line on the glass (UX-41): 8 x 16 letters, digits, `. , ? /`, `<SK>`, `<AR/+>`, an 8-letter word, the block and the bars. Also `Q` with the Sending bar under it. Contrast 32, backlight 8 and 0, 30-60 cm, straight and at an angle. | The user reads the item at a glance. The block and the bars are clearly different. The slots of an 8-letter word are countable. `Q` stays readable above the bar. |
| D-27 | Blank moment (300 ms) in Sending with the same character twice in a row, and in Receiving. If possible, repeat it at a low room temperature. | The user sees each new item start. If the LCD is too slow to show 300 ms, record the shortest visible time for `architector`. |
| D-28 | Group keyboard with four labels (UX-51, `training_spec.md` DC-T3): labels and key bars against the four button positions. A user picks 20 random characters with all 43 learned (also AR, SK and BT), 10 with 40 (a word session) and 10 with a few learned. Contrast 32, backlight 8 and 0, 30-60 cm. | Each label and key bar sits above its own button: `menu/back`, `prev`, `next`, `enter` from left to right. The user presses the button under each label without looking twice. Two labels never read as one, also four lists of 5 characters (`AEFIJ`, `KLMNO`, `PRSTU`, `WY0.,`). Ranges such as `A-K` and `7-<AR>` and the list `<SK><AR>` are clear. Record the time per pick and the wrong picks. |
| D-29 | Training timing feel and lag: the 1000 ms start pause, the result delays 600, 1200 and 2500 ms, the gaps 500 ms (Receiving) and 300 ms (Sending). Measure the time from a tap release to the change on the glass. | The rhythm feels steady and not slow. The user can read `Wrong. The answer is:` and the answer. Changes show within 100 ms (UX-16). Record values the user wants changed. |
| D-30 | Tap mode in Receiving (`training_spec.md` DC-T4, DC-T7): taps of normal speed on all four buttons, a long `prev` inside a group, after a pick and with no pick, a long `menu/back`, and a long `enter` while the item plays and after it. Repeat the `enter` tap and the long `enter` with the dit lever of a paddle and with a straight key on the tip. The start screen and the end screen have tap mode off (UX-55): D-35 and D-36 cover them. | A tap feels prompt, although it acts at the release. A long `prev` undoes one step with no group change first. A long `menu/back` exits with no pick first. A long `enter` replays with no group change or pick first. A short tip closure selects the right run, a tip closure of 800 ms replays. |
| D-31 | Code marks in rows 5 and 7 at 30-60 cm: dots and dashes at 20 and 40 WPM. | The user tells dots from dashes at a glance and reads the pattern of a wrong character. |
| D-32 | Sending: hold the dit lever across the last character and the session end. Start the next session with the dit lever and with `enter`. | The end screen shows. No next session until the lever opens and closes. The start gives no element and no tone. |
| D-33 | Receiving playback (speaker and headphones, Tone 600 Hz, 20 WPM and 50 WPM). Results with the playback running. | No click at element edges and at a stop by the check or the exit. The playback timing checks of `training_spec.md` pass. |
| D-34 | Sending word sessions (UX-52) at 20 / 20, 13 / 13 and 20 / 10, with `Iambic B` and with a straight key. Key words at a normal rhythm, then with one pause of about 4 units and one of about 6 units inside a word. Also read a wrong code in row 7 during the 1200 ms delay, and watch the retry. | A normal rhythm never gives `Wrong: pause too long`. The 6-unit pause gives it, the 4-unit pause does not. The user understands the text and finds the bar under the next character. The user reads the wrong code before row 7 clears, and sees the retry as a fresh start. Report false pause errors at 20 / 20 with a straight key to `architector`. The 5-unit limit is a user decision. |
| D-35 | Level selector feel (UX-55), Receiving and Sending: on the start screen and on the end screen, single `prev` and `next` presses, and holds over 20 or more levels. Try to stop on a given level. Measure the time from the release to the last change on the glass. | The number follows the hold at about 12.5 levels per second. The last level shows within 100 ms after the release (UX-16). The user stops on the target, or one level off, in 2 or 3 tries. At L1 and at the earned level, the marker disappears and nothing moves. |
| D-36 | Session start from the start screen and the end screen (UX-55): the `enter` button, the dit lever and a straight key on the tip, in Receiving and Sending. Hold the contact for 2 s after the start. Insert a plug while the start screen shows. | One session per closure. Receiving: no pick, no narrowed group and no replay from the held contact. Sending: no element and no tone from the held contact. Record whether the plug insertion starts a session (F-20, F-21). |
| D-37 | Legibility of the selector and the main menu level (UX-54, UX-57) at 30-60 cm, contrast 32, backlight 8 and 0: `<` and `>` next to `L5 of 20`, `New char .`, `New char ,`, `New char <AR/+>`, and `L12` inverted on the selected main menu row. | The user reads the level, the markers and the new character at a glance. `.` and `,` are visible as characters. The inverted `L12` is as readable as the label. |
| D-38 | Startup screen on the glass (UX-58, UX-59). Contrast 32, backlight 8 and 0, 30-60 cm, straight and at an angle. Switch on 5 times. Measure the time from power-on to the first frame and from the first frame to the main menu. Press each of the four buttons once during the screen. | The user reads `Putikeeg`, `CW Trainer`, `mod by UT2UBM` and `v0.1.0` at a glance. The two right lines end at the same x, and the two left lines start at the same x. The screen shows for about 5 s. Each button press shows the main menu at once and changes nothing else. Record the time to the first frame for `architector`. |
| D-39 | Display recovery start with the startup screen (UX-60). Set contrast 0 and backlight 0, save, switch off. Hold `menu/back` and switch on. Release it during the startup screen. Repeat, and press `enter` during the startup screen. | The startup screen is readable at once. The `Display Defaults` message follows and stays open. A press during the startup screen does not close the message. The next press closes it. |
| D-40 | `About` screen on the glass (UX-62). Contrast 32, backlight 8 and 0, 30-60 cm. | The user reads all rows. The two `_` characters at the end of `putikeeg_cw_trainer__` show as two marks. The user can type the link from the three rows with no error. The `About` row in the main menu reads well inverted and normal. |

## 12. Open questions

Resolved by the user: Q-1 (UX-19), Q-2 (UX-3), Q-3 (a Tone edit plays the tone, see UX-20 to UX-22 and section 6.5),
Q-6 (UX-18), the `Statistics reset` screen (UX-8). From the keyer plan: long presses in modes (UX-23), straight-key
thresholds from the TX settings (UX-35), one unknown glyph (UX-27), the `Paddle` setting (UX-32).
Resolved by the user during the keyer specification:

- Q-7: no hint on the mode screens. The manual explains the long `menu/back` (UX-36).
- Q-8: no stuck-key timeout. The manual tells the user to insert the plug before opening Free Input (UX-37).
- Q-9: SK shows as `<SK>` (UX-28).

Resolved by the user for the prosign display setting (`keyer_spec.md` section 17.1): AR, BT, KN and AS follow a
setting with the values `Punctuation`, `Prosign` and `Both`, default `Both`, also for an old record. SK stays `<SK>`.
The item is in the Settings list (UX-38).

Resolved by `architector`: Q-4 (`eeprom_write` reads back each byte, and a failed save shows `Not saved` / `OK`, see
F-7 and `menu_subsystem_spec.md` sections 3.5 and 3.6).

Resolved by the user for the training modes (approved plan, 2026-10):

- Session sizes and pass rules. "Correct" for the score means correct on the first attempt. The word pass rate
  changed on 2026-10-07 (below).
- 3 attempts per item, and the answer shows after the third wrong attempt.
- The Koch order, the word length, the progress key. A manual level choice came later (2026-10-07, below).
- Receiving input: narrowing presses, long `prev` undo, `enter` replay. Changed on 2026-10-06 (below).
- The button order on the case (closes D-8).

Resolved by the user on 2026-10-06 (`training_spec.md` D-T27, D-T28, UX-51):

- The order of the four buttons on the case, from left to right: `menu/back`, `prev`, `next`, `enter` (`enter` is
  right of `next`). This closes the open note on the position of `enter` (C-4).
- The Receiving keyboard uses all four buttons: four groups, one per button, in the case order. Three groups were
  confusing.
- In Receiving: long `menu/back` exits, long `enter` replays, long `prev` undoes, long `next` does nothing.
- The paddle tip shares P3.2 with `enter`: a short closure selects the right run, a closure of 800 ms replays, and a
  plug insertion can select a run (accepted risk, F-20).

Resolved by the user on 2026-10-07 (Sending, UX-52):

- A Sending retry clears row 7, so the screen looks like a fresh item. The bar goes back to the first character.
  The status keeps `Try 2 of 3` or `Try 3 of 3`, the same as Receiving. Receiving does not change.
- Row 7 keeps the wrong code for the whole `Wrong: ...` delay (1200 ms). The clear comes at the retry moment.
- In a Sending word item, a pause of 5 Farnsworth units at `TX Eff. Speed` between two characters is an error,
  like a wrong character. It counts only after the first correct character of the attempt and before the item is
  complete. Character sessions do not change.
- Word sessions pass at 80 % of the count, rounded up: 8 of 10, 80 of 100. This applies to every word session,
  also the final word levels with 10 to 100 words. Character sessions stay at 45 of 50 (90 %).

Resolved by the user on 2026-10-07 (level selection, section 5.16, UX-53 to UX-57):

- Receiving and Sending open with a start screen that selects the level. The session end screen has the same
  selector. The user can select L1 up to the earned level.
- `prev` and `next` step by one, by press and by auto-repeat, and stop at the limits. Receiving keeps tap mode off on
  these two screens. Long events do nothing there.
- Selector line `<  L5 of 20  >` and a description in short words: `New char F`, `K and M`, `Words of 4`.
- After a pass, the end screen preselects the next level. After a fail, the same level. A pass below the earned level
  does not change the stored level.
- The main menu shows the earned level next to `Receiving` and `Sending`, at the current `RX Speed` or `TX Speed`.

Resolved by the user on 2026-10-08 (startup screen and `About`, sections 5.17 and 5.18, UX-58 to UX-63):

- A startup screen shows for about 5 s after power-on, close to the mockup of the user, with `CW Trainer` in the
  8 x 16 font. Any button skips it to the main menu and does nothing else.
- Later on 2026-10-08, the user changed the time of the startup screen from 2 s to 5 s (`MENU_SPLASH_MS` 2000 ms
  to 5000 ms, UX-59). The skip rules, the held button at power-on and the recovery order (UX-60) do not change.
- The main menu has the item `About` after `Settings`. Its screen shows the name, the mod, the version and the link
  in the layout that the user chose.
- The version comes from the build macro `FIRMWARE_VERSION`. It is `0.1.0` in this version.

Resolved by the user after the training specification:

- Q-11: word items and the word keyboard hold no prosign (UX-48, `training_spec.md` D-T4).
- Q-12: the Koch set has 43 characters. The 44 of the plan was a counting error (`training_spec.md` D-T3).

Resolved by `architector` for the training modes (`training_spec.md`): the shown level is the index plus 1,
saturated at 65535 (D-T5, was Q-13). Tap mode on the Receiving screen (D-T7, U-11). A Sending retry starts the whole
item again (D-T6, U-15). A failed reset shows `Not saved` (D-T11, U-17). Also:

- Q-14: `training_hold_result()` holds RESULT while the answer plays. `TRAINING_FAILED_MS` starts at the first poll
  that sees the player idle (`training_spec.md` D-T14).
- Q-15: the Sending event rules of section 5.13.2 are adopted as proposed (`training_spec.md` D-T15 and D-T16).

Resolved by `architector` on 2026-10-08: Q-16. `firmware/cmake/firmware_version.cmake` stops the configure step
when the version has more than 17 characters, and a compile-time check in `menu_tree.c` stops the build too
(`menu_subsystem_spec.md` D-A15).

| ID | Question | Impact | Owner |
| --- | --- | --- | --- |
| Q-5 | The datasheet advises against EEPROM writes at low supply voltage. The board has no documented battery-voltage input. | A save at a low battery can corrupt the record (then F-2 applies). | `architector` |
| Q-10 | `Show CW Code` has no effect on Free Input. Does the user want the dots and dashes of each character there later? | A second line per character halves the text area. Not in this plan. | user |
