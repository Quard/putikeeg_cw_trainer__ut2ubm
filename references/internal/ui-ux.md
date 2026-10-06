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
  - Placeholder screens for Receiving and Sending.
  - The Free Input mode (section 5.9) and the keyer for the three key types (section 6.7).
  - Long presses (section 6.6) and the other button input.
  - The Reset Statistics dialog.
  - The live effect of contrast and backlight, and the Tone preview (section 6.5).
- Out of scope in this version: the Receiving and Sending modes, statistics, any sound other than the Tone preview
  and the keyer sidetone in Free Input.
- Users: amateur radio operators who learn or practise Morse code. They know the terms WPM, RX, TX, Iambic A/B and
  Farnsworth (effective) speed.
- Environment: a handheld device or a device on a desk, indoor light, viewing distance about 30-60 cm
  (unverified). Long practice sessions.
- Frequent tasks: key text in Free Input and read the decoded result (most frequent, long sessions), start a mode
  from the main menu (frequent), change a speed, the key type or the tone (sometimes), change contrast or backlight
  (rare), reset statistics (rare).

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

## 3. Inventories

### 3.1 Controls (fact, unless marked)

| ID | Control | Pin | Behaviour | Notes |
| --- | --- | --- | --- | --- |
| C-1 | `next` button | P0.0 | Active low, no external pull-up. | Physical position on the case is not documented (unverified). |
| C-2 | `prev` button | P0.1 | Active low, no external pull-up. | Same as C-1. |
| C-3 | `menu/back` button | P0.2 | Active low, no external pull-up. | |
| C-4 | `enter` button | P3.2 | Active low. Shared with the tip of the paddle/key socket. | In the menu, a paddle dit contact or a straight key also presses `enter`. In Free Input, the keyer owns the pin and the `enter` button keys like the tip contact (section 6.7). Inserting a plug can short the tip for a short time (unverified). |
| C-5 | Paddle ring | P5.5 | Dah contact (with `Paddle` = `Normal`). Active low, internal pull-up (plan). | The menu ignores it. Only the keyer in Free Input reads it. A mono plug shorts the ring to the sleeve (research Step 3). |
| C-6 | `on/off` button | none | Not read by the MCU (no pin in the wiring table). | Unverified: the effect of the button on the MCU power is not known. |
| C-7 | Volume potentiometer | analog | Not read by the MCU. Sets the sidetone loudness in the analog path (`components_and_wiring.md`). | The firmware has no volume control. At the lowest setting the Tone preview can be silent. |

Semantic assumption (unverified, D-8 in section 11): `prev` means up, left and decrease. `next` means down, right
and increase. If the buttons on the case are in a different order, this file needs a review.

### 3.2 Outputs

| ID | Output | Budget | Notes |
| --- | --- | --- | --- |
| O-1 | LCD, 128 x 64, monochrome, probably ST7567 | 8 text rows x 25 columns with the 5 x 8 font | Contrast 0-63 (`lcd_set_contrast`), default 0x20 = 32. The full-screen transfer time is not measured yet. |
| O-2 | LCD backlight, HX4004A on P0.3 | Levels 0-10, duty = level x 10 % (plan) | Perceived brightness per level is unverified. |
| O-3 | Sidetone, PWM on P1.1 (speaker) or P1.2 (headphones) | One tone at a time, 400-1000 Hz. Rise 8 ms, fall 8 ms. A tone change while sounding falls to silence, waits for the carrier update (at most 3 ms) and rises again: a dip of about 17-19 ms. | Used for the Tone preview (section 6.5) and for the keyer sidetone in Free Input (section 6.7). The route is automatic. With the plug in (P1.0 LOW), the headphones play. Otherwise the speaker plays. A plug change takes effect at the next silence. Loudness comes from C-7. Facts from `sidetone_spec.md` sections 2, 6.3, 10.3. The sound at the ear is unverified (D-10 to D-14). |

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
row above passes. The tightest rows are `RX Eff. Speed` and `TX Eff. Speed` (13 of 14 allowed) and `Prosigns`
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

No list in this version has more than 7 items (main 4, Settings 6, CW 7, Display 2), so no list scrolls now. CW
uses all 7 rows. Settings uses 6 rows, and row 7 stays blank. An eighth CW item or an eighth Settings item needs
this scroll rule.
The rule applies to later lists:

- The window shows 7 items. Minimum movement: the window moves only by the rows that keep the cursor inside it.
- After a wrap (section 6.3) the window shows the first 7 or the last 7 items.
- The title bar shows `i/n` (section 5.1). There is no scroll bar.

### 5.6 Placeholder screen (decision plan + UX-6, UX-31)

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

## 6. Control mapping

### 6.1 Per state (decision plan + UX-9, UX-23, UX-24)

`press` = a debounced press event. `repeat` = an auto-repeat event (prev and next only, section 6.2). `long` = a
long-press event (`menu/back`, `prev` and `next` only, section 6.6).

Short presses and repeats:

| State | `prev` press | `next` press | `prev`/`next` repeat | `enter` | `menu/back` |
| --- | --- | --- | --- | --- | --- |
| List, browse | Cursor up. Wraps from first to last | Cursor down. Wraps from last to first | Same as press, but stops at the first/last item (no wrap) | Mode: open the mode screen (Free Input) or the placeholder (Receiving, Sending). Submenu: open the list, cursor on its first item. Value/choice: enter edit mode. Action: open the dialog. | Main menu: nothing (no redraw). Settings list: go to the main menu with the cursor on `Settings`. Save under the rule in section 7 (UX-15). CW or Display list: go to Settings with the cursor on `CW` or `Display`. No save. |
| List, edit | Decrease the value by one step, or the previous choice. Stops at the limit. Applies at once. | Increase, or the next choice. Stops at the limit. Applies at once. | Same as press, stops at the limit | Leave edit mode, keep the value. Cursor stays on the row. | Leave edit mode, keep the value (same as `enter`). Does not leave the list. |
| Placeholder | nothing | nothing | nothing | nothing | nothing |
| Free Input | nothing | nothing | nothing | No event. The keyer owns P3.2: the `enter` button keys like the tip contact (section 6.7). | nothing |
| Dialog | Focus `No` (no wrap) | Focus `Yes` (no wrap) | Same as press | Activate the focused button. `No`: back to Settings. `Yes`: reset statistics, show the message screen. | Same as `No`. |
| Message | nothing | nothing | nothing | `Statistics reset`: back to Settings, cursor on `Reset Statistics`. `Display Defaults`: the main menu, cursor on `Receiving`. | Same as `enter`. |

Long presses:

| State | Long `menu/back` | Long `prev` | Long `next` |
| --- | --- | --- | --- |
| List (browse or edit), dialog, message | nothing | nothing | nothing |
| Placeholder | Back to the main menu, cursor on the mode item that opened it. | nothing | nothing |
| Free Input | Stop the keyer (a sounding tone falls in 8 ms). Back to the main menu, cursor on `Free Input`. The text is lost. | Clear the text area (section 5.9). | Draw the delimiter (section 5.9). |

"nothing" means: no state change, no redraw, no dirty page, no sound. In a list, the short press already acted on the
press edge. The long event of the same press must not act again (UX-24).

Other rules:

- "Applies at once": contrast goes to the LCD and the backlight level goes to the PWM at each step. The other
  values change only in memory until the save. In the menu, Tone is the only value with an audible effect: the Tone
  preview (section 6.5). `Key Type`, `Paddle`, `TX Speed`, `TX Eff. Speed`, `Tone` and `Prosigns` take effect at
  the next start of Free Input (section 6.7). The other values have no effect in this version.
- Effective-speed rule (plan): an effective speed can never be higher than its base speed. Lowering a base speed
  below its effective speed lowers the effective speed in the same step. Raising a base speed never changes the
  effective speed.
- A list opened from its parent always starts with the cursor on its first item. Going back restores the cursor of
  the parent on the item that was opened.
- After power-on the main menu shows with the cursor on `Receiving`. A display recovery start shows the
  `Display Defaults` message first (section 8.1).
- There is no inactivity timeout anywhere in this version. Free Input stays open until a long `menu/back`. The
  Tone preview timeout (section 6.5) is not a menu timeout: it ends the sound and changes no screen.
- Long presses act only in the mode screens (section 6.6). Lists, edit mode, the dialog and the message screen keep
  their short-press behaviour without a change.

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
| Long press (UX-23) | `BUTTONS_LONG_PRESS_MS` = **800 ms** after the press event, while the debounced state stays pressed. One long event per press. Buttons: `menu/back`, `prev`, `next`. The hold counts on the debounced state. Both of its edges lag the pin by 20 ms, so a pin hold and a debounced hold have the same length. Measured from the press event, the pin must still be closed at 780 ms. | `enter` has no long press: it is the paddle tip. A release bounce shorter than 20 ms does not end the hold (debounced state). |
| Long press and repeat | A held `prev` or `next` gives its repeats and also one long event. Each screen uses only one of them: lists use repeats, mode screens use the long event. | The repeat at 800 ms (400 + 5 x 80) and the long event fall due together. |
| Events due in the same poll | Order: press events, then long events (`menu/back`, `prev`, `next`), then a repeat. A later event is not lost: it comes on a later poll. | Deterministic for tests. |
| Long press and start-up | A button held at start-up gives no long event until it is released and pressed again. | Same rule as the press event. |
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

| Item | Value |
| --- | --- |
| Threshold | 800 ms after the press event (section 6.2). The user holds the button about 0.8 s: the press event and the release both lag the pin by 20 ms. |
| Moment of the action | At the threshold, while the button is still held. Not at the release. |
| Count | One action per press. A longer hold does nothing more. |
| Feedback | The result of the action is the only feedback: the main menu appears, the text area goes blank, or the delimiter line appears. There is no beep, no progress mark and no flash. |
| Short press, repeat and `enter` in a mode screen | Ignored: no state change, no redraw, no sound (section 6.1). |

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

The keyer runs only in Free Input. The mode start starts the keyer, and the mode end stops it. At the mode start the
keyer reads `Key Type`, `Paddle`, `TX Speed`, `TX Eff. Speed` and `Tone` once. Free Input also reads `Prosigns`
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

- Mode start: the `enter` press that opens Free Input gives no element and no tone. This is true for the `enter`
  button and for a dit lever that acts as `enter`. At the mode start, a closed contact stays ignored until it
  opens. After that it keys as normal. The ring contact follows the same rule, so a mono plug (ring shorted to
  the sleeve) works as a straight key on the tip.
- Inside the mode: the menu gets no `enter` event. The `enter` button keys like the tip contact.
- Mode end: after the long `menu/back`, the menu gets P3.2 back. A contact that is held at that moment gives no
  `enter` until it opens and closes again. So a held lever never opens a menu item.
- One closure of P3.2 is never both `enter` and a key element.

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
| Short press or repeat in a mode screen | Nothing. | UX-24 |
| Key element in Free Input | The sidetone, at once. The screen does not show elements. | plan, UX-26 |
| Character keyed in Free Input | The glyph, or the prosign text of section 5.9, appears after the 2-dit pause. Target: visible within 100 ms after the character gap (unverified, D-21). | UX-26 |
| Pattern not in the table | `*` in its cell. | UX-27 |
| `Prosigns` edit | The value changes in the field. Nothing else changes in the menu, and nothing sounds. The new text shows at the next Free Input start. | UX-38 |
| Word space in Free Input | No visible change until the next character, which starts one cell later. | UX-26 |
| Save on leaving Settings | Nothing extra. The main menu appears with the cursor on `Settings`. The erase and write take about 5-7 ms (datasheet: erase 4-6 ms, 15 bytes x 40 us), which the user does not notice. | UX-15 |
| Save not needed | Same screen, no EEPROM access. A save needs two conditions: at least one edit step changed a value during this Settings visit, and the current values differ from the stored record. A value changed and changed back does not cause a write. A visit without an edit step does not cause a write, also after a display recovery start. | UX-15 |
| Reset Statistics `Yes` | The message screen `Statistics reset` / `OK`. | UX-8 |
| Response time | Target: the change is visible within 100 ms after the press event (unverified, D-6). A redraw can be slower than 80 ms during auto-repeat. Then the screen skips an intermediate value, but no step is lost. The value advances once per event, and the last value shows within 100 ms after release. | UX-16 |

## 8. Start-up, faults and recovery

| ID | Condition | What the user sees | Device behaviour | Recovery |
| --- | --- | --- | --- | --- |
| F-1 | First power-on, empty EEPROM | The main menu, no message. Settings show the defaults. | Defaults load. Nothing is written until the user changes a value and leaves Settings. | None needed. |
| F-2 | Record with a wrong magic, a layout version other than 1 or 2, or a wrong CRC (for example a power loss during a save) | Same as F-1, no message. | Defaults load. | The user sets the values again. |
| F-3 | Valid record with a value out of range | The clamped value. | Each field is clamped. An effective speed above its base speed becomes the base speed. | None needed. |
| F-4 | Power loss while inside Settings | After power-on, the values from before the visit. | Unsaved changes are lost (plan). | The user sets the values again. |
| F-5 | Contrast set to a value that makes the screen unreadable | Blank or black screen. | The value applies at once. If the user leaves Settings, it is saved. | During the edit: press the opposite button. After a save: the display recovery start (section 8.1). |
| F-6 | Backlight `Off` in the dark | No light. | Level 0 is a valid setting. | Same as F-5. |
| F-8 | `menu/back` held at start-up | The message screen `Display Defaults`, then the main menu. | Section 8.1. | - |
| F-7 | EEPROM write fails (read-back mismatch, CMD_FAIL or low supply) | The message screen: title `Settings`, text `Not saved`, `OK`. `enter` or `menu/back` closes it to the main menu with the cursor on `Settings`. | `eeprom_write` reads back each byte. The save stops with `SETTINGS_SAVE_FAILED`. The values stay active. The magic is written last, so a stopped save leaves no valid record (`menu_subsystem_spec.md` sections 3.5 and 3.6). | The next exit from Settings tries the save again. If the user switches off first, the next power-on uses the defaults (F-2). |
| F-9 | A mono plug is inserted while Free Input is open (the ring closes after the mode start). | `Straight`: a tone without end and no new character. Iambic: a stream of dahs (`Normal`) or dits (`Reversed`) and no new character until the plug is pulled. Then `*` shows. | The keyer sees a closed contact. The entry mask works only at the mode start. | Long `menu/back`, then open Free Input again: the mask now ignores the ring. The manual says: insert the plug before you open Free Input. No stuck-key timeout (UX-37). |
| F-10 | A plug is inserted or removed while Free Input is open. | A short tone and maybe one extra character (`E`, `T` or `*`). | A short contact closure is a real element for the keyer. | Long `prev` clears the screen. |
| F-11 | The main loop falls behind and the keyer event queue overflows (more than 8 events). | `*` in place of the character in progress. The text continues after it. | The keyer drops the new event and sets an overflow flag. Sound and keying stay correct. | None needed. If it happens often, record it for `architector` (D-21). |
| F-12 | Power loss while Free Input is open. | After power-on, the main menu. The text is gone. | No mode state survives. The settings stay as stored. | Open Free Input again. |
| F-13 | No plug, or the key is not connected. | `Free Input` and a blank text area. No sound. | The keyer idles. The `enter` button still keys the tip. | Connect the key. |
| F-14 | First power-on after the update from the earlier firmware: a valid record of layout version 1. | The main menu, no message. All earlier settings stay. `Prosigns` shows `Both`. Free Input shows AR, BT, KN and AS in the `Both` text. | The record loads. `Prosigns` takes its default. Nothing is written at start-up. The first save after an edit step writes the new layout (version 2) with all current values (`keyer_spec.md` section 17.4.2). | None needed. The user can choose another value in Settings. |
| F-15 | Return to the earlier firmware after a save with this firmware. | The earlier firmware shows all its defaults, no message. | The earlier firmware does not accept layout version 2. It uses F-2. | The user sets the values again. The release notes say this (`technical-writer`). |

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
| V-02 | Power-on with a record that has a bad CRC. Open Settings > CW and Settings > Display. | No message screen. CW shows `600 Hz`, `32 WPM`, `32 WPM`, `20 WPM`, `20 WPM`, `Iambic B`, `Normal`. Settings shows `On`, `Both`, `5`. Display shows `8`, `32`. |
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
| V-19 | Main menu: enter on Receiving. Press prev, next, enter, menu/back. Long prev, long next. Then long menu/back. | Title `Receiving`, `TEXT(3,8,"Not ready")`, `PLAIN(1)`..`PLAIN(7)`. The short presses and the long prev and next change nothing: framebuffer unchanged, no dirty page. Long menu/back: main menu with `SEL(1)` on `Receiving`. Same for `Sending` (`SEL(2)` after back). |
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
| V-52 | Power-on, empty EEPROM, reset the fake sidetone log. Call `menu_poll()` after each event. Do a `POLL` of 500 ms after each step. Steps: 1. Move in the main menu with presses and repeats. 2. Open the Receiving and Sending placeholders and give all four buttons short presses, and `menu/back`, `prev`
and `next` long presses there. 3. Open Settings. Move the cursor onto `Tone` and past it in browse mode. 4. Edit each value and choice item except Tone up to both limits, with presses and repeats. 5. Open the Reset Statistics dialog and answer `No`. Open it again and answer `Yes`. Close the message. 6. Change a value and leave Settings with a save. | `SLOG` is empty at the end. `fake_tick_read_count()` does not change during the `POLL` steps. |
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

## 10. Durable decisions

| ID | Decision | Reason | Rejected alternatives |
| --- | --- | --- | --- |
| UX-1 | 8 x 25 grid, x origin 2, 2 px left margin and 2 px right margin (glyph column + x 127). | Symmetric margins. Probes at x 0, 1, 127 never hit text. | x origin 0 (text touches the glass edge in a highlight). |
| UX-2 | Inverted title bar 8 rows. Highlight band 7 rows (`8r+1`..`8r+7`) with an off gap line at `8r` across the width of the band. | Keeps the title bar and a selected row 1 apart while keeping 7 visible items and page-aligned rows. | Full 8-row highlight: merges with the title bar. 10-row title with 6 items: breaks page alignment, loses a row. Title in normal video: weak separation. Clipping cell row 0: cuts `l`, `t`, `h`. |
| UX-3 | Labels and value formats in section 5.3, fixed field width per item. `RX`/`TX` labels decided by the user. | Fits 25 columns with edit markers. Value does not jump while it changes. | Full words `Receive`/`Send` (do not fit with markers). `Recv`/`Send` short forms. |
| UX-4 | Edit mode = highlight shrinks to the value field, `<`/`>` markers show the possible directions. | Two cues (shape and markers) for the mode. Markers also show the limits. | Blinking value (needs a timer, weak on slow LCD). `*` marker only (too small). |
| UX-5 | Scroll window of 7, minimum movement, `i/n` in the title bar. | No extra pixels. Testable as text. | Scroll bar (needs a column). |
| UX-6 | Placeholder: title + `Not ready`. Only a long menu/back works (UX-31). Free Input is no longer a placeholder. | Plan, user. | - |
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
| UX-23 | Long press: 800 ms after the press event, on `menu/back`, `prev` and `next`. One action per press, at the threshold while the button is held. No feedback other than the result of the action. | The user decided on long presses. 800 ms is twice the repeat delay, so a normal press or a short hold never triggers it. Exit and clear destroy the text, so the hold must be deliberate. The screen changes during the hold. This is the signal to release. | 500 ms (close to a slow press, risk of lost text). 1500 ms or more (slow for a frequent clear). Action at the release (the user cannot see that the hold is long enough). A beep (mixes with the sidetone during practice). A progress bar (pixels and code for a 0.8 s wait). A separate value per button (more to learn, no gain). |
| UX-24 | In mode screens, short presses, repeats and `enter` do nothing. In lists, dialogs and message screens, long events do nothing. | The other hand rests near the buttons during keying: an accidental short press must not lose the text. Each screen family uses one gesture set, so one press never acts twice. | Short menu/back exits Free Input (the user asked for long). Short prev/next scroll the text (needs a text buffer). |
| UX-25 | Free Input screen: `FONTS_ID_5X8`, the grid of section 4.1, title bar `Free Input` in row 0, text in rows 1-7 (7 x 25 cells). No status, no cursor mark, no hint. | Same font, grid and title rule as every other screen. 175 visible characters keep about 30 words of context. One text row is one LCD page, so a scroll is a page copy. The settings are one menu level away, and the sound shows the speed and the key type at once. | `FONTS_ID_8X16` (16 x 3 with a title: 48 characters, a scroll every 16 characters, a second grid, a scroll moves 2 pages per line). No title (one more line, but the screen does not say which mode is open). Key type or speed in the title (code and strings for a value the user just set). A blinking or static cursor (code and a timer, small gain). |
| UX-26 | Text rules of section 5.9. A character comes at the character gap. A word space is a blank cell. The wrap is per character and lazy. A scroll moves the text up by one line. A line never starts with a space. No text buffer. | Cheapest in RAM and flash: the framebuffer is the only store. The lazy wrap never shows an empty last line. Dropped edge spaces keep the left edge aligned. | Word wrap (needs a buffer of the current word). Clear a full screen (loses all context at once). Show the elements of the character in progress (code, and the sidetone already gives this feedback). |
| UX-27 | Unknown glyph: `*`. One cell for any pattern that is not in the table, more than 6 elements, the 8-dit error sign, or a lost event. | User decision: one unknown glyph. `*` has no Morse code, so it never looks like a correct character. ASCII, no ink in cell column 4. | `?` (a valid Morse character). An inverted cell (heavy in running text, more code). `#` (touches its neighbours). Nothing (hides the error from the student). A separate glyph for the error sign (the user decided against it). |
| UX-28 | UX-38 refines this decision: AR, BT, KN and AS follow the `Prosigns` setting, and the rule below is its value `Punctuation`. Prosigns: AR = `+`, BT = `=`, SK = `<SK>` (four cells, per-character wrap). Prosigns with the code of a punctuation mark show that mark (KN = `(`, AS = `&`). Others are `*`. | User decision (Q-9). SK has no punctuation mark. The brackets show a prosign, not the letters `S` and `K`. `<` and `>` are edit markers only in lists, and Free Input has no edit mode, so there is no conflict. Per-character wrap needs no extra rule. | `SK` (looks the same as `S` then `K`). Keep `<SK>` on one line (needs a look-ahead rule for one token). A rare ASCII sign such as `%` (the student must learn a code for it). `*` for SK (a correct prosign must not look like an error). |
| UX-29 | Delimiter: a 1 px line at `y = 8r + 3`, x 2-126, in its own text row. The next character starts a new row. | Clear separation of practice blocks for about 30 bytes of code. A continuous line cannot look like keyed text. | A blank row (looks like unused space). A row of `-` glyphs (looks like keyed `-` characters). A dashed line (more code, no gain). |
| UX-30 | Clear: rows 1-7 off, cursor home, the character in progress dropped. Title not redrawn. No question to the user. | Practice text has a low value, and the 800 ms hold protects it. Dropping the partial character gives a clean restart. | A Yes/No dialog (slow for a frequent action). Keep the partial character (its first elements are from before the clear). |
| UX-31 | Receiving and Sending placeholders: `Not ready`, only a long menu/back exits. | User decision: long menu/back exits any mode. The placeholders follow the same rule as the real modes. The real modes later keep the same gesture. | Short menu/back in placeholders only (two exit rules for the same screen family). |
| UX-32 | `Paddle` setting in CW, 7th item: choice `Normal`, `Reversed`, fw 8, default `Normal`. `Normal`: tip = dit, ring = dah. It applies at the next Free Input start. | User decision: a setting. `Normal`/`Reversed` say what changes better than `Off`/`On`. Fit rule: 6 <= 12. The CW list fills all 7 rows without a scroll. Old records read as `Normal`. | `Paddle Swap` with `Off`/`On` (less clear which side is the dit). `Swap Paddle` (a verb as a label). A swap gesture at the mode start (hidden function). |
| UX-33 | P3.2 belongs to the keyer while Free Input is open, and to the menu at all other times. A contact closed at the mode start is ignored until it opens. A contact held at the mode end gives no `enter` until it opens and closes again. | The `enter` press that opens Free Input must not key a dit. A held lever at the exit must not open a menu item. A mono plug then works as a straight key. | Ignore the paddle for a fixed time after the start (a held contact still keys after the delay). Use a different button to open Free Input (the hardware has none free). |
| UX-34 | The keyer and its sidetone run only in Free Input. In the menu the paddle gives no tone. | The tip acts as `enter` in the menu: a tone on each menu `enter` is noise. One clear place for practice. | A practice tone in the menu (noise, and conflicts with the Tone preview). |
| UX-35 | Key Type semantics of section 6.7. The thresholds come from `TX Speed` and `TX Eff. Speed`. A straight-key mark of 2 dits or longer is a dah. | User decision: no adaptive decoding. The student practises at the speed of the settings. | Adaptive speed tracking (more code, and the student gets no feedback on the speed). |
| UX-36 | No hint about the long `menu/back` on the placeholders or in Free Input. The user manual explains it. | User decision (Q-7). No string and no code. Free Input has no free row. | A `Hold menu to exit` line in row 5 of the placeholders. |
| UX-37 | No stuck-key timeout. A contact that closes after the mode start keys until it opens or the user exits (F-9). The manual tells the user to insert the plug before opening Free Input. | User decision (Q-8). The state is harmless, and the recovery is one long `menu/back`. No keyer code. | Mute or mask a contact that stays closed for some seconds (keyer code, and a new rule for a real long mark). |
| UX-38 | `Prosigns` setting (sections 5.9 and 5.10). Label `Prosigns`, `Settings` index 2 after `Show CW Code`, choice `Punctuation`, `Prosign`, `Both` in this order, fw 11, default `Both`. AR, BT, KN and AS show `+` `=` `(` `&`, or `<AR>` `<BT>` `<KN>` `<AS>`, or `<AR/+>` `<BT/=>` `<KN/(>` `<AS/&>`. SK is `<SK>` for every value. A record of the earlier firmware gives `Both` and keeps the other settings, with no message (F-14). A text of up to 6 cells wraps per cell (UX-26). The value applies at the next Free Input start. | User decisions: the values, their order, the default, SK unchanged, the item in Settings. Label: `Prosigns` names what the setting changes and fits (8 of 9). Position: next to `Show CW Code`, the other item about how characters show. The CW list is full, and Display holds screen hardware settings. A setting is read once per mode start, as `Paddle` (UX-32), because the settings cannot change while Free Input is open. Per-cell wrap keeps one wrap rule for all text. The brackets bound every prosign text, so the inner `/` of `Both` stays readable next to a keyed `/`. | Label `Prosign` (the same word as a value: the row reads `Prosign  Prosign`). `Prosign Style`, `Prosign As` (do not fit: 13 and 10 of 9). `AR Style` (names one prosign). Index 1 before `Show CW Code`, or after `Max Word Length` (splits the two character-display items). Keep a prosign text on one line (a look-ahead rule, and an empty cell run at a line end). A space inside the `Both` text (`<AR +>`: looks like a word space). A message after the update (UX-17). A live preview of the text in the menu (no free row, and it needs a second draw path). |

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
| D-15 | Long press feel: hold `menu/back`, `prev` and `next` in Free Input. Also use the menu with normal presses and holds of `prev`/`next` for a few minutes. | The action comes while the button is held, about 0.8 s after the press. It never comes by accident during normal menu use. The user finds 800 ms neither slow nor too easy. |
| D-16 | Open Free Input with the `enter` button, and with the dit lever as `enter` (keep it closed for 2 s). Repeat with a straight key on the tip. | No tone and no character until the contact opens and closes again. Then it keys as normal. |
| D-17 | Hold the dit lever (or the straight key) and leave Free Input with a long `menu/back`. Keep the lever closed for 2 s, then release. | The main menu shows. No item opens. After the release, a new `enter` works. |
| D-18 | Iambic A and Iambic B at 20 and 30 WPM: single elements, held levers, squeeze, and squeeze release at different moments. Listen, and capture P1.1 or P1.2 with a logic analyzer. | Dah = 3 dits and gap = 1 dit within 1 ms. On a squeeze release, B gives one more element than A. A squeeze from idle starts with a dit. |
| D-19 | Straight key at TX Speed 15 and 20 WPM, by an operator who keys well. Repeat with TX Eff. Speed 10. | Characters decode as keyed. Word spaces appear at normal word pauses. With TX Eff. Speed 10, a normal character pause does not give a word space. Record the error rate for `architector`. |
| D-20 | `Paddle` = `Reversed` in Iambic B. Key both levers and the `enter` button. | The ring lever gives dits. The tip lever and the `enter` button give dahs. |
| D-21 | Legibility and speed of the Free Input text: 5 x 8 font at 30-60 cm during keying. Key a full screen at 50 WPM with long words, so that the screen scrolls. Measure the 7-page flush time. | The text is readable at a glance. A character appears within about 100 ms after the character gap. No character is lost or turns into `*` during a scroll (F-11). The sidetone has no gap or click during the flush. |
| D-22 | Mono plug with a straight key: insert it before Free Input opens, then key. Then insert it while Free Input is open. | Before: the tip keys, the ring short has no effect. During: record the behaviour (F-9). A long `menu/back` stops the tone. |
| D-23 | Delimiter and clear on the glass. Key text with `-` and `_` characters around a delimiter. | The delimiter line is clearly different from the `-` and `_` glyphs. Clear leaves the title bar intact and gives no flicker of the title. |
| D-24 | Receiving and Sending placeholders: a user who has not read the manual tries to leave. | Record whether the user finds the long `menu/back`. If not, record it for the user. Decision UX-36: no hint on the screen. |
| D-25 | Prosign texts on the glass (UX-38, `keyer_spec.md` DC-P1). In Free Input with `Both`, key AR, KN and AS. Also key KN and then a keyed `)` (`-.--.-`). Key a `/` (`-..-.`) next to AR. Key AR with the cursor in column 22, so that `<AR` and `/+>` split. Repeat with `Prosign` and `Punctuation`. Contrast 32, backlight 8 and 0, 30-60 cm, indoor light, straight and at an angle. | The user reads `<AR/+>`, `<KN/(>` and `<AS/&>` correctly at a glance. The user tells `(` from `<` in `<KN/(>`. The user tells the inner `/` of `<AR/+>` from a keyed `/` beside it. The user joins a split text across the line end. No glyph pair looks joined (`+>`, `&>`, `/(`). If a text fails, record it for the user: the options are the `Prosign` or `Punctuation` value, or a new text (a user decision, then `architector` changes `keyer_spec.md` section 17). |

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

| ID | Question | Impact | Owner |
| --- | --- | --- | --- |
| Q-5 | The datasheet advises against EEPROM writes at low supply voltage. The board has no documented battery-voltage input. | A save at a low battery can corrupt the record (then F-2 applies). | `architector` |
| Q-10 | `Show CW Code` has no effect on Free Input. Does the user want the dots and dashes of each character there later? | A second line per character halves the text area. Not in this plan. | user |
