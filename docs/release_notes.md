# Release notes

These notes list the changes of the custom firmware for the Putikeeg CW Trainer (board `CW_MINI_V2.0D`) that a
user can see, hear or press. The [user manual](user_manual.md) tells you how to use the device.

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
