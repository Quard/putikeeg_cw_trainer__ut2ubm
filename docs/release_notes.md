# Release notes

These notes list the changes of the custom firmware for the Putikeeg CW Trainer (board `CW_MINI_V2.0D`) that a
user can see, hear or press. The [user manual](user_manual.md) tells you how to use the device.

## Unreleased: level selection in Receiving and Sending

Changes since commit `5916c0f` (Koch training modes Receiving and Sending).

> **Not tested on the device.** This version passed the host tests, the build checks, the QA review and the
> architecture review. Nobody tested it on the device yet. See
> [Not checked on the device: level selection](#not-checked-on-the-device-level-selection).

### New

- **Start screen.** `Receiving` and `Sending` open on a start screen. No session starts until you press `enter`.
  The screen shows the level selector `<  L20 of 20` and a description line. The first number is the level that
  you choose. The second number is your earned level, the level that the device stores for the mode at the current
  speed. The start screen always shows your earned level first.
- **Choose a level.** `prev` goes one level down and `next` goes one level up, from L1 up to your earned level. A
  hold repeats the step: the first repeat after 0.4 s, then 12.5 levels per second. The level stops at L1 and at
  your earned level. It does not wrap. `<` and `>` show which steps are possible.
- **Description line.** Under the selector, a short text tells what the level teaches: `K and M` for L1,
  `New char J` for a character level, and `Words of 4` for a word level. The new character shows in the style of
  the `Prosigns` setting, for example `New char <BT/=>`.
- **Replay a lower level.** A pass below your earned level keeps the stored level. The device saves nothing.
  Example: your earned level is L20. You pass L5. The next session preselects L6, and the store keeps L20.
- **Level in the main menu.** `Receiving` shows your earned level at the current `RX Speed`, for example `L12`.
  `Sending` shows it at the current `TX Speed`.

### Changed

- **Opening a mode does not start a session.** Press `enter`, or close the tip contact, on the start screen to
  start the session. To leave the start screen, press `menu/back`.
- **New session end screen.** After a failed save, row 1 shows `Not saved`. Then come the score and one result
  text: `Passed, new level` after a pass at your earned level, `Passed` after a replay pass or a pass at L65535, or
  `Not passed, need <t>`. Below the result, the end screen shows the same selector as the start screen, and `Start`.
  The texts `New level`, `Repeat level`, `Top level` and `Next` are gone.
- **Preselection after a session.** After a pass, the selector shows the next level. After a fail, it shows the
  same level. You can change it before you start the next session.
- **Buttons on the session end screen.** `prev` and `next` change the level. In both modes, every button acts at
  the push. In Receiving, the end screen buttons are no longer taps. A short press of `menu/back` opens the main
  menu.
- **A held button does not change the selection.** The end screen ignores a button that you hold from the session.
  Release the button, and then press it again.

### Saved levels

The stored levels and their format do not change. A replay pass writes nothing. Only a pass at your earned level
saves, as before.

### Known limits

- **Insert the plug before you open `Receiving` or `Sending`.** On the start screen and the session end screen, a
  closure of the tip contact starts a session at once. A plug insertion can do the same. Hold `menu/back` for about
  0.8 s to leave such a session. Your level does not change.
- A far level takes time to reach: from L1 to L150 takes about 12 s.
- The level that you choose is not stored. When you open the mode again, the selector shows your earned level.

### Not checked on the device: level selection

The firmware design and the host tests give the behavior above. These items need a check on the device:

- The feel of the selector with auto-repeat, on the start screen and the end screen, in both modes, up to an earned
  level above L50. The last level must show within 0.1 s after the release.
- The start of a session with the `enter` button and with the tip contact (dit lever and straight key). A held
  contact must give no pick in Receiving, and no element and no tone in Sending.
- A plug insertion while the start screen or the end screen shows. It can start a session.
- The legibility of the selector, the description line and the level in the main menu.

The device checks are in `references/internal/training_spec.md` section 18 (DC-T9) and
`references/internal/ui-ux.md` section 11 (D-35 to D-37).

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
