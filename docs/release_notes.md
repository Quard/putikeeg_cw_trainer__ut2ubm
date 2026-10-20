# Release notes

These notes list the changes of the custom firmware for the Putikeeg CW Trainer (board `CW_MINI_V2.0D`) that a
user can see, hear or press. The [user manual](user_manual.md) tells you how to use the device.

## Version 0.1.2

Version 0.1.2 changes the answer state of Receiving, and the start screen and the session end screen of Receiving
and Sending. It adds the `Ramp` setting for the rise and fall time of the tone. The `Settings > CW` list now scrolls,
and the device saves the settings in a new format.

### Koch training: Receiving and Sending

- **Receiving waits after the third wrong attempt.** After the third wrong attempt, the answer stays on the screen
  until you tap `enter`. There is no time limit. The answer plays one time. To hear it again, hold `enter` for about
  0.8 s. In version 0.1.0, the next item came 2.5 s after the end of the playback.
- **Go on with `enter`.** A tap of `enter` shows the next item. On the last item, it shows the session end screen.
  You can tap while the answer plays. Then the playback stops.
- **`Next` and `Finish`.** In this state, the bottom row shows `Next` above `enter`, or `Finish` on the last item.
  The other three labels are blank. `Next` is the label of the `enter` button. A tap of the `next` button does
  nothing. The other buttons also do nothing. To leave the session, hold `menu/back` for about 0.8 s.
- **Key or paddle.** In this state, a short closure of the tip contact acts as a tap of `enter`. A closure of about
  0.8 s plays the answer again. A plug insertion can also go on to the next item. Insert the plug before you open
  Receiving.
- **Sending does not change.** After the third wrong attempt, the answer shows for 2.5 s. Then the next item comes.

### Koch training: start screen and session end screen

These changes apply to Receiving and Sending.

- **New start screen.** The level selector, for example `<  L20 of 20`, is now at the top, directly under the
  header. Below it, a label shows the kind of the level: `New chars` at L1, `New char` at a character level. At a
  word level, the label row is blank. In the middle of the screen, the level description shows in the large
  characters of the items. It is `K M` at L1, and the new character at a character level, for example `J`. At a
  word level, it is `Words of` and the word length, for example `Words of 4`. In version 0.1.0, one short line
  under the selector showed the description, for example `New char J` or `K and M`.
- **CW code on the Sending start screen.** In Sending, the dots and dashes of the new character show below the
  large character. At L1, the codes of K and M show. The code shows for every value of `Show CW Code`, also with
  `Off`. So you can see the code before you key the character. In a session, `Show CW Code` works as before. At a
  word level, no code shows. Receiving shows no code on the start screen.
- **Simpler session end screen.** The end screen shows only the score and the result, for example `Score 46/50`
  and `Passed, new level`, with `OK` at the bottom. It has no level selector and no level description.
- **Two presses to the next session.** Press `enter` on the end screen, or close the tip contact. The start screen
  shows the level of the next session: the next level after a pass, the same level after a fail. You can change
  the level with `prev` and `next`. Press `enter` again to start the session. One held press of `enter` opens only
  the start screen. In version 0.1.0, `enter` on the end screen started the next session at once.
- **End screen buttons.** `menu/back` opens the main menu. `prev` and `next` do nothing on the end screen. A plug
  insertion on the end screen can open the start screen. In version 0.1.0, a tip closure on the end screen started
  a session.

### Settings: Ramp

- **New setting `Ramp`.** `Settings > CW > Ramp` sets the rise time and the fall time of the tone: 4 to 12 ms, in
  steps of 1 ms. The default is 6 ms. One value sets both edges of every sound: the sidetone in Free Input and
  Sending, the playback in Receiving, and the previews. In version 0.1.0, the tone always rose in 8 ms and fell in
  8 ms.
- **Shorter default edges.** The default of 6 ms gives a little shorter edges than the fixed 8 ms of version 0.1.0.
  To get the earlier sound, set `Ramp` to 8 ms.
- **Ramp preview.** When you edit `Ramp`, the device plays the tone of the `Tone` setting for about 0.3 s with the
  new rise and fall, as in the `Tone` edit. To compare two values, press `prev` or `next` one time, wait about
  0.5 s, and press again. Each single press then gives one full beep. When you hold the button, the tone stays on,
  and you hear only the first rise and the last fall.
- **Clicks at a low tone.** A short `Ramp` at a low `Tone` can click more, for example 4 ms at 400 Hz. The device
  changes the level of the tone only once in each period of the tone (2.5 ms at 400 Hz). If you hear a click, set
  a longer `Ramp` or a higher `Tone`.
- **Fast speeds.** At 50 WPM with `Ramp` = 12 ms, a dit rises to one step below full level (0.2 % less) and then
  falls. The length of the elements and of the gaps does not change.

### Menu: the CW list scrolls

- **`Ramp` is item 2.** `Settings > CW` has 8 items: `Tone`, `Ramp`, `RX Speed`, `RX Eff. Speed`, `TX Speed`,
  `TX Eff. Speed`, `Key Type` and `Paddle`. `Paddle` moved from item 7 to item 8.
- **Scroll.** The screen shows 7 items. When the cursor moves past the first or the last row, the list moves by one
  row. A wrap shows the first 7 or the last 7 items. The other lists do not scroll.
- **Position mark.** The title bar of `Settings > CW` shows the cursor position at its right end, for example `1/8`.

### Saved settings

- **Your settings stay.** If the device keeps the settings of an earlier version, for example 0.1.0, they stay as
  they were, and `Ramp` is 6 ms. The device shows no message.
- **New settings format.** At the first save after you change a setting, the device writes a new settings format.
  An earlier firmware version cannot read it. If you download an earlier version after that save, it uses all
  default values and shows no message. Set the values again.

## Version 0.1.0

Version 0.1.0 is the first version of the custom firmware. All functions below are new.

### Menu and buttons

- **Main menu.** The main menu `CW Trainer` has five items: `Receiving`, `Sending`, `Free Input`, `Settings` and
  `About`. `Receiving` and `Sending` show your level at the right side of the row, for example `L12`. `Settings`
  has a `>` marker, because it opens a list. `About` has no marker, because it opens a screen.
- **Four buttons.** From left to right: `menu/back`, `prev`, `next` and `enter`. In a list, `prev` and `next` move
  the cursor, `enter` opens the item, and `menu/back` goes back one level.
- **Auto-repeat.** Hold `prev` or `next` to repeat the action. The first repeat comes after 0.4 s, then 12.5 steps
  per second.
- **Wrap in lists.** A press of `next` on the last item goes to the first item, and a press of `prev` on the first
  item goes to the last item. When you hold the button, the cursor stops at the end of the list.
- **Hold a button in a mode screen.** In Free Input and in a training session, hold a button for about 0.8 s to
  give a command. For example, hold `menu/back` to leave. The screen shows no hint for these holds.
- **The tip contact is `enter`.** The tip contact of the key/paddle socket is connected to `enter`. In the menu, a
  dit lever or a straight key acts as `enter`.
- The menu has no timeout.

### Startup screen and About

- **Startup screen.** Each power-on shows a startup screen for about 5 s: `Putikeeg`, `CW Trainer` in large
  characters, `mod by UT2UBM` and the version, for example `v0.1.0`. Then the main menu shows with the cursor on
  `Receiving`. The screen makes no sound.
- **Skip the startup screen.** A press of any of the four buttons shows the main menu at once. The press does
  nothing else: it does not move the cursor and does not open an item. A closure of the tip contact skips the
  screen too. A button that you hold while you switch on the device does not skip it.
- **`About` screen.** The last item of the main menu. Its screen shows `Putikeeg CW Trainer`, `mod by UT2UBM`,
  `Version 0.1.0` and the project link <https://github.com/Quard/putikeeg_cw_trainer__ut2ubm> in three parts.
  Press `enter` or `menu/back` to go back to the main menu. `prev` and `next` do nothing on this screen.
- **Firmware version.** The version has the format `MAJOR.MINOR.PATCH`. The startup screen and the `About` screen
  show it.

### Settings

| Menu | Setting | Values | Default |
| --- | --- | --- | --- |
| Settings > CW | `Tone` | 400 to 1000 Hz, step 10 Hz | 600 Hz |
| Settings > CW | `RX Speed` | 5 to 50 WPM | 32 WPM |
| Settings > CW | `RX Eff. Speed` | 5 WPM to the current `RX Speed` | Same as `RX Speed` |
| Settings > CW | `TX Speed` | 5 to 50 WPM | 20 WPM |
| Settings > CW | `TX Eff. Speed` | 5 WPM to the current `TX Speed` | Same as `TX Speed` |
| Settings > CW | `Key Type` | `Straight`, `Iambic A`, `Iambic B` | `Iambic B` |
| Settings > CW | `Paddle` | `Normal`, `Reversed` | `Normal` |
| Settings | `Show CW Code` | `Off`, `On` | `On` |
| Settings | `Prosigns` | `Punctuation`, `Prosign`, `Both` | `Both` |
| Settings | `Max Word Length` | 4 to 8 | 5 |
| Settings > Display | `Backlight` | `Off`, 1 to 10 | 8 |
| Settings > Display | `Contrast` | 0 to 63 | 32 |

- **Edit a value.** Press `enter` to start editing. `<` and `>` show which directions are possible. Values do not
  wrap. Press `enter` or `menu/back` to end editing.
- **Effective-speed rule.** An effective speed is never higher than its base speed. When you decrease a base speed
  below its effective speed, the effective speed decreases with it.
- **When a setting takes effect.** `Backlight` and `Contrast` change the display at each step while you edit them.
  The other settings take effect the next time you open Free Input, `Receiving` or `Sending`.
- **`Reset Statistics`.** In `Settings`, this item sets all training levels back to L1, after a `Yes` in the dialog
  `Reset statistics?`. It does not change the settings.

### Sidetone and Tone preview

- **Sidetone.** The device plays the sidetone in Free Input and in Sending, and the Morse playback in Receiving. The
  pitch is the `Tone` setting. The sidetone rises in 8 ms and falls in 8 ms, to prevent clicks.
- **Volume and headphones.** The volume knob sets the loudness. With headphones plugged in, the headphones play
  instead of the speaker.
- **Tone preview.** At the start of a `Tone` edit, the device plays the tone for about 0.3 s. Each change of the
  value plays the new tone again. At the end of the edit, the tone stops at once.

### Free Input and the keyer

- **Free Input mode.** Key Morse code with a paddle or a straight key. The device plays the sidetone and shows each
  decoded character in a text area of 7 lines of 25 characters. A full screen moves up by one line.
- **Key types.** `Settings > CW > Key Type` sets `Straight`, `Iambic A` or `Iambic B`. In the iambic modes, dits,
  dahs and spaces have exact lengths. A squeeze gives dits and dahs in turn. At the release of a squeeze,
  `Iambic B` adds one more element.
- **`Paddle` setting.** `Normal` gives dits on the tip and dahs on the ring. `Reversed` swaps them.
- **Speeds.** `TX Speed` sets the dit length. `TX Eff. Speed` sets the pause for a word space. For a straight key,
  a mark of 2 dits or longer is a dah.
- **Decoded text.** The decoder knows `A` to `Z`, `0` to `9`, the signs `.` `,` `?` `/` `=` `+` `-` `(` `)` `&`
  `_` `"` `@` `'` `;` `!` `:`, and the prosigns AR, BT, KN, AS and SK. An unknown pattern shows as `*`.
- **Holds in Free Input.** Hold `menu/back` for about 0.8 s to leave, hold `prev` to clear the text, and hold
  `next` to draw a line under the text.
- **`enter` in Free Input.** The `enter` button keys like the tip contact of the paddle socket. The `enter` press
  that opens Free Input gives no dit.

### Prosigns

`Settings > Prosigns` sets how the device shows the prosigns AR, BT, KN and AS. These four prosigns have the same
code as a punctuation mark.

| Prosign | `Punctuation` | `Prosign` | `Both` (default) |
| --- | --- | --- | --- |
| AR | `+` | `<AR>` | `<AR/+>` |
| BT | `=` | `<BT>` | `<BT/=>` |
| KN | `(` | `<KN>` | `<KN/(>` |
| AS | `&` | `<AS>` | `<AS/&>` |

- SK has no punctuation mark. It always shows as `<SK>`.
- `<` opens a prosign text and `>` closes it. A text has up to 6 characters, and it can split at a line end.
- Free Input uses the setting. `Receiving` and `Sending` use it for BT, SK and AR in the items, in the keyboard
  labels, in the `Wrong:` text and in the description line of the level selector.

### Koch training: Receiving and Sending

- **Koch method.** Both modes teach 43 characters in Koch order, from K and M up to the prosigns BT, SK and AR.
  Two character sessions of 50 characters add two new characters. Then a word session of 10 words uses all
  characters that you learned. From L64, the final word sessions add one word for each level, up to 100 words.
- **Pass rules.** The score counts only the items that you answer correctly on the first attempt. A character
  session needs 90 %. A word session needs 80 % of the words, rounded up. You have 3 attempts for each item.
- **`Max Word Length`.** It sets the longest word, 4 to 8 characters.
- **Receiving.** The device plays an item at `RX Speed` and `RX Eff. Speed`. You pick its characters with a group
  keyboard of four labels, one above each button. A tap acts at the release. One pick needs 3 taps at most. Hold
  `enter` for about 0.8 s to hear the item again. Hold `prev` to undo one step.
- **Sending.** The device shows an item, and you key it with the keyer of Free Input. The device decodes and checks
  each character at once. One wrong character ends the attempt, and the status line shows `Wrong:` and the keyed
  character. In a word, a pause of more than 5 units of the `TX Eff. Speed` spacing between two characters shows
  `Wrong: pause too long`. A retry starts the whole item again from its first character.
- **Code row.** With `Show CW Code` = `On`, the bottom row of Sending shows the dots and dashes that you key. After
  the third wrong attempt, the code of the expected character shows, also with `Off`.
- **Session end screen.** It shows the score and one result text: `Passed, new level` after a pass at your earned
  level, `Passed` after a replay pass or a pass at L65535, or `Not passed, need <t>`. Below the result, it shows the
  level selector and `Start`. After a failed save of the level, row 1 shows `Not saved`.
- **Leave a session.** Hold `menu/back` for about 0.8 s. The session is lost, and your level does not change.

### Levels and level selection

- **Earned level.** The device stores one level for each mode and each character speed from 5 to 50 WPM.
  `Receiving` uses the level of the current `RX Speed`, and `Sending` uses the level of the current `TX Speed`. The
  main menu shows the earned level next to `Receiving` and `Sending`.
- **Start screen.** `Receiving` and `Sending` open on a start screen. No session starts until you press `enter` or
  close the tip contact. The screen shows the level selector `<  L20 of 20` and a description line. The first
  number is the level that you choose. The second number is your earned level. The start screen always shows your
  earned level first. Press `menu/back` to go back to the main menu.
- **Choose a level.** `prev` goes one level down and `next` goes one level up, from L1 up to your earned level. A
  hold repeats the step: the first repeat after 0.4 s, then 12.5 levels per second. The level stops at L1 and at
  your earned level. It does not wrap. `<` and `>` show which steps are possible.
- **Description line.** Under the selector, a short text tells what the level teaches: `K and M` for L1,
  `New char J` for a character level, and `Words of 4` for a word level. The new character shows in the style of
  the `Prosigns` setting, for example `New char <BT/=>`.
- **Replay a lower level.** A pass below your earned level keeps the stored level. The device saves nothing.
  Example: your earned level is L20. You pass L5. The next session preselects L6, and the store keeps L20.
- **Level after a session.** After a pass, the selector on the session end screen shows the next level. After a
  fail, it shows the same level. You can change it before you start the next session.
- **Buttons on the start screen and the session end screen.** Every button acts at the push, in both modes. The
  end screen ignores a button that you hold from the session. Release the button, and then press it again.

### Saved settings and levels

- **Settings.** To save the settings, leave `Settings` for the main menu. A save needs a change of at least one
  value. If the save fails, the message `Not saved` shows.
- **Levels.** The device saves a level only after a pass at your earned level. A replay pass, a failed session and
  a session that you leave save nothing.
- **Empty or damaged memory.** With no saved settings, or with damaged settings, the device uses the default
  values. With no stored levels, or with damaged levels, every level is L1. The device shows no message.
- The startup screen and `About` write nothing.

### Display recovery start

Hold `menu/back` while you switch on the device. The startup screen shows first, with the default contrast 32 and
the default backlight 8. Then the `Display Defaults` message shows. The press that skips the startup screen does
not close the message. Press `enter` or `menu/back` to close it. The recovery start does not change the saved
settings.

### Known limits

- **Insert the plug before you open Free Input, `Receiving` or `Sending`.** A plug that you insert while Free Input
  is open can close a contact. Then the tone does not stop, or the keyer sends without end. On the start screen and
  the session end screen, a closure of the tip contact starts a session at once. A plug insertion can do the
  same. Hold `menu/back` for about 0.8 s to leave. Your level does not change. The device has no time limit for a
  closed contact.
- **The device does not store the Free Input text.** The end of Free Input or a power loss deletes it.
- `Show CW Code` has no effect on Free Input.
- The decoder does not adapt to your speed. Key at the speed of the settings.
- A far level takes time to reach: from L1 to L150 takes about 12 s.
- The level that you choose is not stored. When you open the mode again, the selector shows your earned level.
- A firmware download can keep or delete the saved settings and levels.
