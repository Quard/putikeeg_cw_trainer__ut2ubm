# User manual

This manual is for the custom firmware of the Putikeeg CW Trainer (board `CW_MINI_V2.0D`). It tells you how to use
the menu, the settings and the Free Input mode. The [release notes](release_notes.md) list the changes of each
version.

> **Not tested on the device.** This firmware version passed host tests only. Nobody tested it on the device
> yet. The behavior in this manual comes from the firmware design and the host tests. On the device, some
> details can be different, for example the button response time, the readable contrast range and the backlight
> levels.

## What this version does

This version has the menu, the settings, the display controls and the [Free Input](#free-input) mode:

- In `Free Input`, you key Morse code with a paddle or a straight key. You hear the sidetone, and the screen shows
  the decoded text. The keyer has three key types: `Straight`, `Iambic A` and `Iambic B`.
- `Receiving` and `Sending` are not available yet. They show the message `Not ready`.
- The device makes two sounds: the sidetone in Free Input and the Tone preview. When you edit `Tone` in
  `Settings > CW`, the device plays the edited tone for about 0.3 s. Each change of the value plays the new tone
  again. At a limit, a press plays nothing new. When you leave the edit, the tone stops at once. The volume knob
  sets the loudness. With headphones plugged in, the headphones play instead of the speaker.
- The device collects no statistics.
- `Backlight` and `Contrast` change the display. `Tone`, `TX Speed`, `TX Eff. Speed`, `Key Type` and `Paddle` set
  the keyer in Free Input. The firmware saves the other settings for the training modes of a later version.

## Buttons

The device has four control buttons and an `on/off` button.

| Button | In a list | While you edit a value |
| --- | --- | --- |
| `menu/back` | Goes back one level. In the main menu it does nothing. | Ends editing and keeps the value. |
| `prev` | Moves the cursor up. | Decreases the value, or selects the previous choice. |
| `next` | Moves the cursor down. | Increases the value, or selects the next choice. |
| `enter` | Opens the selected item. | Ends editing and keeps the value. |

Notes:

- **`enter` is shared with the paddle socket.** The tip contact of the key/paddle socket is connected to `enter`.
  In the menu, a paddle dit contact or a straight key acts as `enter`. A key that stays closed gives only one
  `enter`. The other buttons continue to work. In Free Input, the keyer uses this contact. Then `enter` keys like
  the tip contact (see [The enter button in Free Input](#the-enter-button-in-free-input)).
- If you insert a plug into the paddle socket while the menu shows, the device can see one `enter`. Then it can
  open an item or start editing. Press `menu/back` to go back. Nothing changes. If it opens a mode screen, hold
  `menu/back` to go back (see [Hold a button in a mode screen](#hold-a-button-in-a-mode-screen)). This effect is
  not checked on the device.
- **Hold for auto-repeat.** To repeat the action of `prev` or `next`, hold the button. The first repeat comes
  after 0.4 s. After that, the action repeats every 0.08 s (12.5 steps per second). `enter` and `menu/back` do
  not repeat. In the menu lists, a longer hold does nothing more.
- **Wrap in lists.** A press of `next` on the last item moves the cursor to the first item. A press of `prev` on
  the first item moves it to the last item. When you hold the button, the cursor stops at the end of the list.
- A button that is held while you switch on the device does nothing until you release it and press it again.
  The only exception is the [display recovery start](#display-recovery-start).
- The position of `prev` and `next` on the case is not checked yet. This manual assumes that `prev` means up or
  decrease and `next` means down or increase.

## Screens

- The top row is the title bar. It shows the name of the current menu, for example `Settings > CW`.
- A highlight band shows the selected item.
- An item with `>` at the right edge opens a submenu.
- A value item shows its value at the right side of the row.

## Menu structure

After you switch on the device, the main menu `CW Trainer` shows. The cursor is on `Receiving`.

```text
CW Trainer
├── Receiving           shows "Not ready"
├── Sending             shows "Not ready"
├── Free Input          key and decode Morse code
└── Settings
    ├── CW
    │   ├── Tone
    │   ├── RX Speed
    │   ├── RX Eff. Speed
    │   ├── TX Speed
    │   ├── TX Eff. Speed
    │   ├── Key Type
    │   └── Paddle
    ├── Show CW Code
    ├── Max Word Length
    ├── Display
    │   ├── Backlight
    │   └── Contrast
    └── Reset Statistics
```

`RX` means receive. `TX` means send.

- When you open a submenu, the cursor is on its first item.
- When you go back with `menu/back`, the cursor is on the item that you opened.
- The menu has no timeout. It stays on the current screen until you press a button.

## Hold a button in a mode screen

`Receiving`, `Sending` and `Free Input` are mode screens. In a mode screen, a short press of a button does nothing.
This protects your text from a press by accident. To give a command, hold the button for about 0.8 s.

| Hold this button | In `Free Input` | In `Receiving` or `Sending` |
| --- | --- | --- |
| `menu/back` | Leaves Free Input. The main menu shows with the cursor on `Free Input`. The text is lost. | Goes back to the main menu. |
| `prev` | Clears the text. | Does nothing. |
| `next` | Draws a line under the text. | Does nothing. |

How to hold a button:

1. Push and hold the button.
2. Wait until the screen changes. This takes about 0.8 s. The action comes while you hold the button.
3. Release the button.

Notes:

- The screen shows no hint about these holds. The device gives no beep and no progress mark. The change of the
  screen is the only signal.
- One hold gives one action. A longer hold does nothing more.
- `enter` has no hold action. In Free Input, `enter` keys like the tip contact.
- If you hold `prev` and `next` together in Free Input, the device clears the text and then draws a line.

### Receiving and Sending

In this version these items open a screen with the message `Not ready`. To go back to the main menu, hold
`menu/back` for about 0.8 s. A short press does nothing on this screen.

## Free Input

In Free Input you key Morse code. The device plays the sidetone and shows each decoded character on the screen.
Use it to practise your sending.

### Before you start

1. Set the keyer in `Settings > CW`. Free Input reads these settings each time it opens:
   - `Key Type`: `Straight`, `Iambic A` or `Iambic B` (see [Key types](#key-types)).
   - `Paddle`: which lever gives the dits (see [Paddle setting](#paddle-setting)).
   - `TX Speed` and `TX Eff. Speed`: the speeds that the keyer and the decoder use (see
     [Speeds and pauses](#speeds-and-pauses)).
   - `Tone`: the pitch of the sidetone.
2. Press `menu/back` until the main menu shows. When you leave `Settings`, the device saves the changed settings.
3. Insert the plug of the key or paddle into the key/paddle socket.

> **Insert the plug before you open Free Input.** At the start of Free Input, the keyer ignores each closed contact
> until it opens. A plug that you insert while Free Input is open can close a contact for a long time. Then the
> tone does not stop (see [Problems in Free Input](#problems-in-free-input)). The device has no time limit for a
> closed contact.

### Start, key and leave

1. In the main menu, select `Free Input` and press `enter`. The screen shows the title `Free Input` and an empty
   text area.
2. Key your text. You hear each element at once. After each character, the character shows on the screen.
3. To leave, hold `menu/back` for about 0.8 s. The main menu shows with the cursor on `Free Input`.

Notes:

- The `enter` press that opens Free Input gives no dit and no tone. This is also true for a dit lever that you use
  as `enter`. If the contact stays closed, Free Input ignores it until it opens. After that, it keys as normal.
- **When you leave Free Input, the text is lost.** The device does not store it. A power loss also deletes the text.
- If you hold a lever closed while you leave, no menu item opens. After you release the lever, `enter` works again.
- If a tone sounds when you leave, it stops at once.
- Free Input has no timeout. It stays open until you hold `menu/back`.
- The settings cannot change while Free Input is open. To change a setting, leave Free Input, change the setting,
  and open Free Input again.
- `RX Speed`, `RX Eff. Speed`, `Show CW Code` and `Max Word Length` have no effect on Free Input.

### What the screen shows

The top row shows the title `Free Input`. Below it, the text area has 7 lines of 25 characters.

- Letters show as capitals.
- The screen does not show a cursor, and it does not show the dits and dahs of the character in progress. You hear
  them.
- When a line is full, the next character starts a new line. A word can break across two lines.
- When all 7 lines are full, the text moves up by one line. The top line of text goes away. The title stays.
- A line never starts with a space.

The decoder knows these characters:

- The letters `A` to `Z` and the digits `0` to `9`.
- The signs `.` `,` `?` `/` `=` `+` `-` `(` `)` `&` `_` `"` `@` `'` `;` `!` `:`.

Prosigns:

| Prosign | Code | Shows as |
| --- | --- | --- |
| AR | `.-.-.` | `+` |
| BT | `-...-` | `=` |
| SK | `...-.-` | `<SK>` (four characters, can break across two lines) |
| KN | `-.--.` | `(` |
| AS | `.-...` | `&` |

**`*` means "unknown".** The screen shows `*` for these cases:

- A pattern that is not in the list above.
- A pattern with more than 6 elements, for example the 8-dit error sign.
- Lost keyer events. The cause is a display that falls far behind the keyer (about 0.3 s at 50 WPM). This is
  rare. The text continues after the `*`.

`*` has no Morse code, so it never stands for a correct character.

### Clear the text and draw a line

- **Clear:** hold `prev` for about 0.8 s. The text area goes blank. The title stays. The elements of the character
  in progress are dropped, so the next character starts with its first element. The device does not ask before it
  clears, and you cannot undo a clear. A tone that sounds continues until the contact opens.
- **Line:** hold `next` for about 0.8 s. The device draws one horizontal line on a new row. The next character
  starts on the row below the line. Use it to separate practice blocks. A character in progress shows after the
  line. Two holds give two lines.

### Key types

Set the key type with `Settings > CW > Key Type`.

| `Key Type` | Use it with | What the keyer does |
| --- | --- | --- |
| `Straight` | A straight key, a bug, or one paddle lever as a key. | Both contacts key. The tone sounds while a contact is closed. A mark shorter than 2 dits is a dit. A mark of 2 dits or longer is a dah. `Paddle` has no effect. |
| `Iambic A` | A paddle with two levers. | The dit lever gives dits and the dah lever gives dahs, for as long as you hold the lever. A dah is 3 dits long, and the space between two elements is 1 dit. If you squeeze both levers, the keyer gives dits and dahs in turn. A squeeze from rest starts with a dit. When you release both levers during a squeeze, the keyer completes the current element and stops. |
| `Iambic B` | A paddle with two levers. | The same as `Iambic A`, with one difference: when you release both levers during a squeeze, the keyer adds one more element of the other kind. |

More rules for `Iambic A` and `Iambic B`:

- A short tap on the other lever during an element or during the space after it adds that element.
- A short tap on the same lever during the space after an element repeats the element.
- In `Iambic A`, the keyer can see a tap as a squeeze: you close the other lever while the first lever is still
  closed. You then release both levers before the space ends. In this case, `Iambic A` adds no element, and
  `Iambic B` adds it. Some other keyers act differently in this case.

The default key type is `Iambic B`.

### Paddle setting

`Settings > CW > Paddle` sets which contact gives the dits:

| `Paddle` | Tip contact (and the `enter` button) | Ring contact |
| --- | --- | --- |
| `Normal` (default) | dit | dah |
| `Reversed` | dah | dit |

With `Key Type` = `Straight`, both contacts key, so `Paddle` has no effect.

### The enter button in Free Input

In Free Input, the `enter` button is connected to the same contact as the tip of the paddle socket. So `enter`
keys like the tip contact: dits with `Paddle` = `Normal`, dahs with `Paddle` = `Reversed`. With
`Key Type` = `Straight`, `enter` works as a straight key. You can use it to try Free Input without a key.

### Mono plug and straight key

A mono plug connects the ring contact to the sleeve, so the ring contact is always closed. At the start of Free
Input, the keyer ignores each closed contact until it opens. So a straight key with a mono plug works on the tip
contact. Insert the plug before you open Free Input, and set `Key Type` to `Straight`.

### Speeds and pauses

The keyer and the decoder use `TX Speed` and `TX Eff. Speed`. The decoder does not adapt to your speed. Key at the
speed of the settings.

- **Dit length:** `TX Speed` sets it. One dit is 1200 / WPM milliseconds. Example: 60 ms at 20 WPM.
- **Character end:** after a pause of 2 dits, the character shows on the screen. The pause starts at the end of the
  last element.
- **Word space:** a pause of 5 units of the `TX Eff. Speed` spacing gives a word space. With `TX Eff. Speed` equal
  to `TX Speed`, this is 5 dits. A lower `TX Eff. Speed` needs a longer pause.
- **Straight key:** a mark shorter than 2 dits is a dit. A mark of 2 dits or longer is a dah.

Examples (values are rounded):

| `TX Speed` | `TX Eff. Speed` | Dit | Character end and straight-key dah limit | Word space |
| ---: | ---: | ---: | ---: | ---: |
| 5 WPM | 5 WPM | 240 ms | 480 ms | 1200 ms |
| 20 WPM | 20 WPM | 60 ms | 120 ms | 300 ms |
| 20 WPM | 10 WPM | 60 ms | 120 ms | 1090 ms |
| 20 WPM | 5 WPM | 60 ms | 120 ms | 2669 ms |
| 32 WPM | 32 WPM | 38 ms | 75 ms | 188 ms |
| 50 WPM | 50 WPM | 24 ms | 48 ms | 120 ms |

### Sidetone

- The pitch is the `Tone` setting. The volume knob sets the loudness. With headphones plugged in, the headphones
  play instead of the speaker. A headphone plug change takes effect at the next silence.
- With `Straight`, the tone follows the contact.
- With `Iambic A` and `Iambic B`, the tone starts with the element and ends with it.
- The tone rises in 8 ms and falls in 8 ms, to prevent clicks.
- The keyer runs in a timer interrupt, so the display never delays the tone.
- In the menu, the paddle gives no tone.

### Problems in Free Input

| Problem | Possible cause | What to do |
| --- | --- | --- |
| You cannot leave Free Input, `Receiving` or `Sending`. | A short press does nothing in a mode screen. | Hold `menu/back` for about 0.8 s, until the main menu shows. |
| The tone does not stop, or the keyer sends dahs or dits without end, and no new character shows. | You inserted the plug while Free Input was open. A mono plug closes the ring contact. | Hold `menu/back` to leave. Then open Free Input again. Insert the plug before you open Free Input. |
| A short tone sounds, and an extra character (`E`, `T` or `*`) can show. | You inserted or removed a plug while Free Input was open. | To remove the extra character, hold `prev` to clear the text. |
| Dits and dahs are on the wrong levers. | `Paddle` does not match your paddle. | Change `Settings > CW > Paddle`. |
| Many characters are wrong or show as `*`. | Your speed is different from `TX Speed`. The decoder does not adapt. | Set `TX Speed` near your keying speed. |
| Word spaces come too often or too rarely. | The pause for a word space comes from `TX Eff. Speed`. | Change `TX Eff. Speed`. A lower value needs a longer pause. |
| The dah lever does nothing, or the keyer sends with no lever pressed. | The ring input needs the internal pull-up of the processor. Nobody checked yet that it works on this board. | Report the problem with your key type and plug type. |
| The text is gone. | You left Free Input, or the power went off. | The device does not store the text. |
| The screen shows `Free Input` and no text, and no tone sounds. | No key is connected. | Connect the key. The `enter` button keys the tip contact. |

### Not checked on the device

These parts of Free Input passed host tests only. Nobody checked them on the device yet:

- The feel of the 0.8 s hold. A hold must never come by accident in the menu.
- The ring contact with the internal pull-up of the processor, and the polarity of both contacts.
- The exact element and space lengths, and the difference between `Iambic A` and `Iambic B` with a real paddle.
- The bounce of a real paddle. It must never split an element.
- The decoding of a straight key at normal speed.
- The start and the end of Free Input with a closed contact. The `enter` press that opens Free Input must give no
  tone. A lever that you hold while you leave must open no menu item.
- The legibility of the text, the time until a character shows, and the sidetone while the text moves up.
- A mono plug inserted before and during Free Input.
- The line from a long `next`. It must look different from the `-` and `_` characters.

## Change a setting

1. Select the setting with `prev` and `next`.
2. Press `enter` to start editing. The highlight becomes smaller and covers only the value. The markers `<` and
   `>` show next to the value.
3. Press `prev` or `next`. Each press changes the value by one step. The new value applies at once. Hold the
   button to change the value quickly.
4. Press `enter` or `menu/back` to end editing. The value stays. The cursor stays on the same row.

The markers show which directions are possible:

- `<` shows that `prev` can decrease the value.
- `>` shows that `next` can increase the value.
- At the minimum, `<` is not shown. At the maximum, `>` is not shown. More presses change nothing.

Values do not wrap. For example, at 50 WPM a press of `next` does not go to 5 WPM.

## Settings

| Menu | Setting | Values | Step | Unit | Default |
| --- | --- | --- | --- | --- | --- |
| Settings > CW | `Tone` | 400 to 1000 | 10 | Hz | 600 Hz |
| Settings > CW | `RX Speed` | 5 to 50 | 1 | WPM | 32 WPM |
| Settings > CW | `RX Eff. Speed` | 5 to the current `RX Speed` | 1 | WPM | Same as `RX Speed` (32 WPM) |
| Settings > CW | `TX Speed` | 5 to 50 | 1 | WPM | 20 WPM |
| Settings > CW | `TX Eff. Speed` | 5 to the current `TX Speed` | 1 | WPM | Same as `TX Speed` (20 WPM) |
| Settings > CW | `Key Type` | `Straight`, `Iambic A`, `Iambic B` | - | - | `Iambic B` |
| Settings > CW | `Paddle` | `Normal`, `Reversed` | - | - | `Normal` |
| Settings | `Show CW Code` | `Off`, `On` | - | - | `On` |
| Settings | `Max Word Length` | 4 to 8 | 1 | - | 5 |
| Settings > Display | `Backlight` | `Off`, 1 to 10 | 1 | - | 8 |
| Settings > Display | `Contrast` | 0 to 63 | 1 | - | 32 |

`RX Speed` and `TX Speed` are the character speeds. `RX Eff. Speed` and `TX Eff. Speed` are the effective
(Farnsworth) speeds.

`Backlight` and `Contrast` change the display at each step while you edit them. `Tone`, `TX Speed`,
`TX Eff. Speed`, `Key Type` and `Paddle` take effect the next time you open Free Input. The other settings have no
effect in this version.

### Effective-speed rule

An effective speed can never be higher than its base speed:

- `RX Eff. Speed` is at most `RX Speed`. `TX Eff. Speed` is at most `TX Speed`.
- When the effective speed is equal to its base speed, the `>` marker is not shown, and `next` changes nothing.
- When you decrease a base speed below its effective speed, the effective speed decreases with it in the same
  step.
- When you increase a base speed, the effective speed does not change.

Example: `RX Speed` is 32 WPM and `RX Eff. Speed` is 25 WPM. You decrease `RX Speed` to 24 WPM. Now
`RX Eff. Speed` is also 24 WPM. Then you increase `RX Speed` to 30 WPM. `RX Eff. Speed` stays at 24 WPM.

## When the device saves the settings

To save the settings, leave `Settings` with `menu/back` and go back to the main menu. The device saves only
with these two conditions:

- During this visit to `Settings`, you changed at least one value by at least one step.
- At least one value is different from the saved values.

If you change a value and then change it back to the saved value, the device does not save. Nothing on the
screen shows a save. The main menu shows with the cursor on `Settings`.

These actions do not save: end editing, or go back from `CW` or `Display` to `Settings`.

> **Caution:** Leave `Settings` before you switch off the device. If the power stops while you are in
> `Settings`, the device loses the changes of this visit. At the next start, it uses the values from before the
> visit.

### Not saved

If the save fails, a screen with the title `Settings` shows the message `Not saved` and `OK`. A low battery
voltage is one possible cause.

1. Press `enter` or `menu/back`. The main menu shows with the cursor on `Settings`.
2. The changed values stay in use until you switch off the device.
3. Open `Settings` and leave it again to try the save again. Before you try again, make sure that the battery is
   not low.

After a failed save, the next start can use the values from before the save. It can also use the default values.

### Default values

The device uses the default values in these cases. It shows no message.

- No settings are saved yet, for example at the first start.
- The saved settings are damaged, for example after a power failure during a save.

A firmware download can keep or delete the saved settings. Nobody checked this on the device yet. If the device
keeps the settings of the earlier firmware version, `Paddle` is `Normal`, and the other settings stay as they were.

## Reset Statistics

This version collects no statistics. The dialog works, but there is nothing to reset yet.

1. In `Settings`, select `Reset Statistics` and press `enter`.
2. The dialog `Reset statistics?` shows. The focus is on `No` each time the dialog opens.
3. Press `next` to select `Yes`, or `prev` to select `No`.
4. Press `enter`:
   - With `No`, the dialog closes and `Settings` shows.
   - With `Yes`, the device resets the statistics and shows `Statistics reset` and `OK`. Press `enter` or
     `menu/back` to go back to `Settings`.

`menu/back` in the dialog is the same as `No`.

## Display recovery start

The recovery start helps you to read the screen again. Use it after you saved a bad contrast, or after you set
the backlight to `Off` in a dark room.

While you edit `Contrast` or `Backlight`, you can undo a bad value at once: press the opposite button.

To start with the default contrast and backlight:

1. Switch off the device.
2. Push and hold `menu/back`.
3. Switch on the device. Hold `menu/back` until the screen shows. About 0.1 s is enough. The screen
   `Display Defaults` shows the text `Default contrast and backlight are in use.` and `OK`.
4. Release `menu/back`. The held button does not close the screen.
5. Press `enter` or `menu/back`. The main menu shows with the cursor on `Receiving`.

The device now uses `Contrast` 32 and `Backlight` 8. `Settings > Display` shows these values. All other settings
come from the saved settings.

The recovery start does not change the saved settings. If you do nothing more, the next normal start uses the saved
contrast and backlight again.

To keep the default contrast and backlight:

1. Open `Settings`.
2. Change any setting by at least one step. You can change it back after that. For example, in
   `Settings > Display`, increase `Contrast` by one step and then decrease it again.
3. Press `menu/back` until the main menu shows.

The device saves all current values, also the default contrast and backlight.

> **Not checked on the device:** The recovery start needs a full restart of the processor. Nobody checked yet
> that the `on/off` button gives a full restart.
