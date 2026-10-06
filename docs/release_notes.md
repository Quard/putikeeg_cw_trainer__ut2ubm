# Release notes

These notes list the changes of the custom firmware for the Putikeeg CW Trainer (board `CW_MINI_V2.0D`) that a
user can see, hear or press. The [user manual](user_manual.md) tells you how to use the device.

## Unreleased: Prosigns setting

Changes since commit `ee4eedd` (paddle keyer and Free Input).

> **Not tested on the device.** This version passed the host tests, the build checks, the QA review and the
> architecture review. Nobody tested it on the device yet. See
> [Not checked on the device: Prosigns](#not-checked-on-the-device-prosigns).

### New

- **`Prosigns` setting.** `Settings > Prosigns` sets how Free Input shows the prosigns AR, BT, KN and AS. These four
  prosigns have the same code as a punctuation mark. The default is `Both`.

  | Prosign | `Punctuation` | `Prosign` | `Both` (default) |
  | --- | --- | --- | --- |
  | AR | `+` | `<AR>` | `<AR/+>` |
  | BT | `=` | `<BT>` | `<BT/=>` |
  | KN | `(` | `<KN>` | `<KN/(>` |
  | AS | `&` | `<AS>` | `<AS/&>` |

  SK has no punctuation mark. It always shows as `<SK>`. A change applies the next time you open Free Input.

### Changed

- **AR, BT, KN and AS show a new text by default.** With the default `Both`, AR shows as `<AR/+>`, not as `+`. To
  see only the marks, set `Prosigns` to `Punctuation`.
- **`<` and `>` mark every prosign text.** In Free Input, `<` opens a prosign text and `>` closes it. Before, they
  appeared only in `<SK>`.
- **A prosign text can split at a line end.** A text has up to 6 characters, and each character wraps on its own,
  as `<SK>` did before. For example, `<AR` can end one line and `/+>` can start the next line.
- **`Settings` has 6 items.** `Prosigns` is the new third item, after `Show CW Code`.

### Saved settings

- After the update, all earlier settings stay as they were. `Prosigns` is `Both`. The device shows no message.
  This is true if the firmware download keeps the saved settings.
- The settings have a new saved format. The device writes it at the first save, after you change a setting. The
  update alone writes nothing.
- **A return to an earlier firmware resets all settings.** After this version saved the settings, an earlier
  firmware cannot read them. It uses all default values and shows no message. Set the values again.

### Not checked on the device: Prosigns

The firmware design and the host tests give the behavior above. These items need a check on the device:

- The text of AR, BT, KN, AS and SK for each `Prosigns` value, and its legibility. Can the user read `<AR/+>`,
  `<KN/(>` and `<AS/&>` at a glance, tell `(` from `<`, and tell the `/` inside a text from a keyed `/`? Can the
  user join a text that splits at a line end?
- The update from the earlier firmware with saved settings that are not the defaults. The settings must stay, and
  `Prosigns` must show `Both`.
- A power cycle keeps each `Prosigns` value.

The device checks are in `references/internal/keyer_spec.md` section 17.10 (DC-P1 to DC-P3) and
`references/internal/ui-ux.md` section 11 (D-25).

## Unreleased: paddle keyer and Free Input

Changes since commit `4e9b9d2` (PWM sidetone and Tone preview).

> **Not tested on the device.** This version passed the host tests, the build checks, the QA review and the
> architecture review. Nobody tested it on the device yet. See
> [Not checked on the device](#not-checked-on-the-device).

### New

- **Free Input mode.** Open `Free Input` from the main menu. Key Morse code with a paddle or a straight key. The
  device plays the sidetone and shows each decoded character in a text area of 7 lines of 25 characters. A full
  screen moves up by one line.
- **Keyer with three key types.** `Settings > CW > Key Type` now has an effect: `Straight`, `Iambic A` or
  `Iambic B`. In the iambic modes, dits, dahs and spaces have exact lengths. A squeeze gives dits and dahs in turn.
  At the release of a squeeze, `Iambic B` adds one more element.
- **Speeds from the settings.** `TX Speed` sets the dit length. `TX Eff. Speed` sets the pause for a word space.
  For a straight key, a mark of 2 dits or longer is a dah. The decoder does not adapt to your speed.
- **Decoded text.** The decoder knows `A` to `Z`, `0` to `9` and the signs `.` `,` `?` `/` `=` `+` `-` `(` `)`
  `&` `_` `"` `@` `'` `;` `!` `:`. AR shows as `+`, BT as `=` and SK as `<SK>`. An unknown pattern shows as `*`.
- **`Paddle` setting.** `Settings > CW > Paddle` has the values `Normal` (tip = dit, ring = dah) and `Reversed`.
  The default is `Normal`.
- **Hold a button in a mode screen.** In Free Input, hold `menu/back` for about 0.8 s to leave, hold `prev` to
  clear the text, and hold `next` to draw a line under the text. The screen shows no hint for these holds.
- **`enter` in Free Input.** While Free Input is open, the `enter` button keys like the tip contact of the paddle
  socket. The `enter` press that opens Free Input gives no dit.

### Changed

- **`Receiving` and `Sending` close with a hold.** To leave the `Not ready` screen, hold `menu/back` for about
  0.8 s. A short press does nothing on this screen now.
- **`Settings > CW` has 7 items.** `Paddle` is the new last item.
- **More settings take effect.** `Tone`, `TX Speed`, `TX Eff. Speed`, `Key Type` and `Paddle` take effect the next
  time you open Free Input. `RX Speed`, `RX Eff. Speed`, `Show CW Code` and `Max Word Length` still have no effect.

### Saved settings

The `Paddle` setting uses a free bit of the saved settings. The layout of the saved settings does not change. If
the device keeps the settings of the earlier firmware, `Paddle` is `Normal`, and all other settings stay as they
were. Free Input does not save anything.

### Known limits

- **Insert the plug before you open Free Input.** A plug that you insert while Free Input is open can close a
  contact. Then the tone does not stop, or the keyer sends without end. Hold `menu/back` to leave, then open Free
  Input again. The device has no time limit for a closed contact.
- **The device does not store the text.** The end of Free Input or a power loss deletes it.
- `Receiving` and `Sending` are not available yet.
- `Show CW Code` has no effect on Free Input.

### Not checked on the device

The firmware design and the host tests give the behavior above. These items need a check on the device:

- The internal pull-up of the paddle ring input (P5.5). The reference manual of the processor is not clear about
  it. If the pull-up does not work, the dah lever does nothing, or the keyer sends with no lever pressed.
- The polarity of the tip and ring contacts.
- The element and space lengths, and the difference between `Iambic A` and `Iambic B` with a real paddle.
- The bounce of a real paddle, and the decoding of a straight key.
- The feel of the 0.8 s hold.
- No tone at the start of Free Input, and no menu item at its end, with a closed contact.
- The legibility and the speed of the Free Input text, and the sidetone while the text moves up.
- A mono plug inserted before and during Free Input.

The device checks are in `references/internal/keyer_spec.md` section 14 (DC-K1 to DC-K7) and
`references/internal/ui-ux.md` section 11 (D-15 to D-24).
