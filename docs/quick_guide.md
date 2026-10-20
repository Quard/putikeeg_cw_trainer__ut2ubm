# Quick guide

This guide tells you how to use the Putikeeg CW Trainer with the UT2UBM firmware. For all details, read the
[user manual](user_manual.md).

## The device

The trainer teaches you Morse code (CW). It has a screen, a speaker, a headphone socket, a volume knob and a socket
for a paddle or a straight key. It has three modes:

- `Receiving`: the trainer plays Morse code, and you pick the characters with the buttons.
- `Sending`: the trainer shows characters and words, and you key them.
- `Free Input`: you key freely, and the screen shows what you sent.

`Receiving` and `Sending` use the Koch method. You start with two characters at full character speed. Each level
adds a new character, until you know all 43 characters. The trainer remembers your level for each mode and each
speed, also after you switch it off.

## Buttons

From left to right, the four buttons are `menu/back`, `prev`, `next` and `enter`. A long press means: hold the button
for about 0.8 s, until the screen changes.

The tip contact of the paddle socket works as `enter`. So in the menu, the dit lever of a paddle or a straight key
can open an item. In `Free Input` and in a `Sending` session, the `enter` button keys like the dit lever. With
`Paddle` = `Reversed`, it keys a dah.

Insert the plug of the paddle before you open a mode. A plug insertion can give an `enter` press.

## Menu

When you switch on the trainer, the startup screen shows the firmware version for 5 s. To skip it, press any
button. Then the main menu shows:

```text
CW Trainer
Receiving           L12
Sending              L1
Free Input
Settings                >
About
```

`L12` is your level in this mode. `>` shows an item that opens a submenu.

The screen has room for 7 items. `Settings > CW` has 8 items, so it scrolls. Its title bar shows the cursor
position at the right end, for example `1/8` for item 1 of 8. When the cursor moves past the first or the last row,
the list moves by one row.

| Screen | Button | Short press | Long press |
| --- | --- | --- | --- |
| Menu list | `menu/back` | Goes back one level. | Same as short press. |
| Menu list | `prev` | Moves the cursor up. | Moves the cursor up fast. |
| Menu list | `next` | Moves the cursor down. | Moves the cursor down fast. |
| Menu list | `enter` | Opens the selected item. | Same as short press. |
| Edit a value | `menu/back` | Ends editing and keeps the value. | Same as short press. |
| Edit a value | `prev` | Decreases the value. | Decreases the value fast. |
| Edit a value | `next` | Increases the value. | Increases the value fast. |
| Edit a value | `enter` | Ends editing and keeps the value. | Same as short press. |
| `About` | `menu/back`, `enter` | Closes the screen. | Same as short press. |

## Receiving

The trainer plays a character or a word. You pick each character with a four-button keyboard. Use it to learn to
copy Morse code by ear.

1. Open `Receiving`. The start screen shows your level at the top. Below it, large characters show what the level
   teaches: the new character, `K M` at L1, or the word length, for example `Words of 4`.
2. Select a level with `prev` and `next`, and press `enter`.
3. Listen to the item. Each character has a slot in the middle of the screen.
4. Pick the characters. The bottom row shows four labels, one above each button. A label shows a character or a
   group of characters, for example `A-K`. Press a button to pick its character, or to split its group into four
   smaller groups. One pick takes three presses at most.
5. When you fill the last slot, the trainer checks the answer. You have three attempts for each item.
6. After the third wrong attempt, the answer stays on the screen and plays one time. `Next` shows above `enter`,
   or `Finish` on the last item. Tap `enter` to go on. The `next` button does nothing here. Hold `enter` to hear
   the answer again.
7. After the last item, the end screen shows your score and the result. Press `enter` to see the start screen with
   the level of the next session. Press `enter` again to start it.

| Screen | Button | Short press | Long press |
| --- | --- | --- | --- |
| Start screen | `menu/back` | Goes to the main menu. | Same as short press. |
| Start screen | `prev` | Selects a lower level. | Selects lower levels fast. |
| Start screen | `next` | Selects a higher level. | Selects higher levels fast. |
| Start screen | `enter` | Starts the session. | Same as short press. |
| End screen | `menu/back` | Goes to the main menu. | Same as short press. |
| End screen | `prev`, `next` | Do nothing. | Do nothing. |
| End screen | `enter` | Opens the start screen with the level of the next session. | Same as short press. |
| Session | `menu/back` | Keyboard key 1. | Leaves the session. The session is lost. |
| Session | `prev` | Keyboard key 2. | Undoes one step: the last group or the last pick. |
| Session | `next` | Keyboard key 3. | Does nothing. |
| Session | `enter` | Keyboard key 4. | Plays the item again. This is free. |
| Answer after the third wrong attempt | `enter` | Goes to the next item, or to the end screen. | Plays the answer again. |
| Answer after the third wrong attempt | `prev`, `next` | Do nothing. | Do nothing. |
| Answer after the third wrong attempt | `menu/back` | Does nothing. | Leaves the session. The session is lost. |

## Sending

The trainer shows a character or a word. You key it with a paddle or a straight key, and the trainer decodes and
checks each character at once. Use it to learn to send clean Morse code.

1. Insert the plug of the paddle or the key.
2. Open `Sending`. The start screen shows your level at the top. Below it, large characters show what the level
   teaches: the new character, `K M` at L1, or the word length, for example `Words of 4`. Below the new character,
   its dots and dashes show, also with `Show CW Code` = `Off`.
3. Select a level with `prev` and `next`, and press `enter`.
4. Key the item at the `TX Speed` setting. In a word, a bar shows the next character to key.
5. A wrong character ends the attempt. Then key the whole item again. You have three attempts for each item.
6. After the last item, the end screen shows your score and the result. Press `enter` to see the start screen with
   the level of the next session. Press `enter` again to start it.

In a word, do not pause too long between two characters. A long pause is an error. If you need more time, set a
lower `TX Eff. Speed`.

| Screen | Button | Short press | Long press |
| --- | --- | --- | --- |
| Start screen | `menu/back` | Goes to the main menu. | Same as short press. |
| Start screen | `prev` | Selects a lower level. | Selects lower levels fast. |
| Start screen | `next` | Selects a higher level. | Selects higher levels fast. |
| Start screen | `enter` | Starts the session. | Same as short press. |
| End screen | `menu/back` | Goes to the main menu. | Same as short press. |
| End screen | `prev`, `next` | Do nothing. | Do nothing. |
| End screen | `enter` | Opens the start screen with the level of the next session. | Same as short press. |
| Session | `menu/back` | Does nothing. | Leaves the session. The session is lost. |
| Session | `prev`, `next` | Do nothing. | Do nothing. |
| Session | `enter` | Keys like the dit lever. | Keys like the dit lever. |

## Levels

- Each level is a session of 50 characters or of 10 or more words. Each character level adds one new character.
  After two character levels, a word level uses all the characters that you learned.
- To pass a character level, answer 90 % of the items right on the first attempt. For a word level, you need 80 %.
- A pass at your level opens the next level. You can repeat any level up to your level. A repeat does not change
  your level.
- Each speed has its own level. If you change `RX Speed` or `TX Speed`, the level can change. Set the speed back to
  see your level again.

## Free Input

You key Morse code with a paddle or a straight key. You hear the sidetone, and the screen shows each character that
the trainer decodes. Use it to practice sending without a task. The trainer does not store the text.

Key at the `TX Speed` setting, because the decoder does not adapt to your speed. `*` shows a character that the
decoder does not know.

| Button | Short press | Long press |
| --- | --- | --- |
| `menu/back` | Does nothing. | Leaves Free Input. The text is lost. |
| `prev` | Does nothing. | Clears the text. |
| `next` | Does nothing. | Draws a line under the text. |
| `enter` | Keys like the dit lever. | Keys like the dit lever. |

## Settings

To change a setting, select it and press `enter`. Then press `prev` or `next`, and press `enter` again. When you
leave `Settings` and go back to the main menu, the trainer saves the settings. Before you switch off, leave
`Settings`. `Backlight` and `Contrast` change at once. The other settings apply the next time that you open a mode.

| Menu | Setting | What it does | Values | Default |
| --- | --- | --- | --- | --- |
| `Settings > CW` | `Tone` | The pitch of the sound. | 400 to 1000 Hz | 600 Hz |
| `Settings > CW` | `Ramp` | The rise time and the fall time of each tone. A short ramp gives hard edges. A long ramp gives soft edges. | 4 to 12 ms | 6 ms |
| `Settings > CW` | `RX Speed` | The character speed in `Receiving`. | 5 to 50 WPM | 32 WPM |
| `Settings > CW` | `RX Eff. Speed` | The overall speed in `Receiving` (Farnsworth). A lower value adds longer pauses between characters and words. | 5 WPM to `RX Speed` | 32 WPM |
| `Settings > CW` | `TX Speed` | The keyer speed in `Sending` and `Free Input`. | 5 to 50 WPM | 20 WPM |
| `Settings > CW` | `TX Eff. Speed` | The pause between characters and words in `Sending` and `Free Input`. A lower value gives you more time. | 5 WPM to `TX Speed` | 20 WPM |
| `Settings > CW` | `Key Type` | The type of your key. | `Straight`, `Iambic A`, `Iambic B` | `Iambic B` |
| `Settings > CW` | `Paddle` | Which lever gives the dits. `Reversed` swaps the levers. | `Normal`, `Reversed` | `Normal` |
| `Settings` | `Show CW Code` | Shows the dots and dashes that you key in `Sending`. | `Off`, `On` | `On` |
| `Settings` | `Prosigns` | How AR, BT, KN and AS show, for example `+`, `<AR>` or `<AR/+>`. | `Punctuation`, `Prosign`, `Both` | `Both` |
| `Settings` | `Max Word Length` | The longest word in the training modes. | 4 to 8 characters | 5 |
| `Settings > Display` | `Backlight` | The brightness of the screen light. | `Off`, 1 to 10 | 8 |
| `Settings > Display` | `Contrast` | The contrast of the screen. | 0 to 63 | 32 |
| `Settings` | `Reset Statistics` | Sets all levels back to L1. Select `Yes` and press `enter` to reset. | - | - |

`RX` means receive and `TX` means send.

When you edit `Tone` or `Ramp`, the trainer plays the tone for about 0.3 s at the start of the edit and after each
change. To compare `Ramp` values, press `prev` or `next` one time, wait about 0.5 s, and press again. If you hold
the button in the `Ramp` edit, the tone stays on, and you hear only the first rise and the last fall.

A short `Ramp` at a low `Tone` can click, for example 4 ms at 400 Hz. If you hear a click, set a longer `Ramp` or a
higher `Tone`. Earlier firmware versions had no `Ramp` setting and used a fixed ramp of 8 ms. To get that sound, set
`Ramp` to 8 ms.

## If you cannot read the screen

If you set a bad contrast or switched off the backlight, hold `menu/back` while you switch on the trainer. The
trainer starts with the default contrast and backlight. Press `enter` on the `Display Defaults` message. To keep
these values, change any setting and change it back, then leave `Settings`.
