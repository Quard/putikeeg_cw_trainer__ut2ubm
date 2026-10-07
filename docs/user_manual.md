# User manual

This manual is for the custom firmware of the Putikeeg CW Trainer (board `CW_MINI_V2.0D`). It tells you how to use
the menu, the settings, the Free Input mode and the training modes `Receiving` and `Sending`. The
[release notes](release_notes.md) list the changes of each version.

> **Not tested on the device.** This firmware version passed host tests only. Nobody tested it on the device
> yet. The behavior in this manual comes from the firmware design and the host tests. On the device, some
> details can be different, for example the button response time, the readable contrast range and the backlight
> levels.

## What this version does

This version has the menu, the settings, the display controls, the [Free Input](#free-input) mode and the
[Koch training](#koch-training) modes:

- In `Free Input`, you key Morse code with a paddle or a straight key. You hear the sidetone, and the screen shows
  the decoded text. The keyer has three key types: `Straight`, `Iambic A` and `Iambic B`.
- In `Receiving`, the device plays Morse code and you pick the characters with the buttons. In `Sending`, the
  device shows characters and words and you key them. Both modes teach the characters in Koch order, one level at
  a time. Before each session, you choose the level. You can repeat each level up to the level that you reached.
- The device makes three sounds: the sidetone in Free Input and Sending, the Morse playback in Receiving, and the
  Tone preview. When you edit `Tone` in
  `Settings > CW`, the device plays the edited tone for about 0.3 s. Each change of the value plays the new tone
  again. At a limit, a press plays nothing new. When you leave the edit, the tone stops at once. The volume knob
  sets the loudness. With headphones plugged in, the headphones play instead of the speaker.
- The device stores your training level for each mode and each speed. It stores no other statistics.
- `Backlight` and `Contrast` change the display. The other settings set Free Input and the training modes (see
  [Settings for training](#settings-for-training)).

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
  In the menu and in Receiving, a paddle dit contact or a straight key acts as `enter`. This is also true on the
  [start screen](#choose-a-level) and the [session end screen](#session-end-screen) of Sending. In the menu, a key
  that stays closed gives only one `enter`. The other buttons continue to work. In Free Input and in a Sending
  session, the keyer uses this contact. Then `enter` keys like the tip contact (see
  [The enter button in Free Input](#the-enter-button-in-free-input)).
- If you insert a plug into the paddle socket while the menu shows, the device can see one `enter`. Then it can
  open an item or start editing. Press `menu/back` to go back. Nothing changes. If it opens `Receiving` or
  `Sending`, the start screen shows. Press `menu/back` to go back. If it opens Free Input, hold `menu/back` to go
  back (see [Hold a button in a mode screen](#hold-a-button-in-a-mode-screen)). On the start screen and the session
  end screen, a plug insertion can start a session (see [Choose a level](#choose-a-level)). In Receiving, a plug
  insertion can select the `enter` group of the keyboard (see
  [Key or paddle in Receiving](#key-or-paddle-in-receiving)). These effects are not checked on the device.
- **Hold for auto-repeat.** To repeat the action of `prev` or `next`, hold the button. The first repeat comes
  after 0.4 s. After that, the action repeats every 0.08 s (12.5 steps per second). `enter` and `menu/back` do
  not repeat. In the menu lists, a longer hold does nothing more. The level selector of the training modes also
  repeats (see [Choose a level](#choose-a-level)).
- **Wrap in lists.** A press of `next` on the last item moves the cursor to the first item. A press of `prev` on
  the first item moves it to the last item. When you hold the button, the cursor stops at the end of the list.
- A button that is held while you switch on the device does nothing until you release it and press it again.
  The only exception is the [display recovery start](#display-recovery-start).
- From left to right on the case, the four buttons are `menu/back`, `prev`, `next` and `enter`. `prev` means up or
  decrease, and `next` means down or increase.

## Screens

- The top row is the title bar. It shows the name of the current menu, for example `Settings > CW`.
- A highlight band shows the selected item.
- An item with `>` at the right edge opens a submenu.
- A value item shows its value at the right side of the row.
- In the main menu, `Receiving` and `Sending` show your level at the right side of the row, for example `L12` (see
  [Level in the main menu](#level-in-the-main-menu)).

## Menu structure

After you switch on the device, the main menu `CW Trainer` shows. The cursor is on `Receiving`.

```text
CW Trainer
├── Receiving           Koch training: listen and pick the characters
├── Sending             Koch training: read and key the characters
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
    ├── Prosigns
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

`Receiving`, `Sending` and `Free Input` are mode screens. In Free Input and in a Sending session, a short press of
a button does nothing. This protects your work from a press by accident. Receiving uses short presses as keyboard
keys (see [The group keyboard](#the-group-keyboard)). To give a command, hold the button for about 0.8 s.

| Hold this button | In `Free Input` | In `Receiving` | In `Sending` |
| --- | --- | --- | --- |
| `menu/back` | Leaves Free Input. The main menu shows with the cursor on `Free Input`. The text is lost. | Leaves the session. The main menu shows with the cursor on `Receiving`. | Leaves the session. The main menu shows with the cursor on `Sending`. |
| `prev` | Clears the text. | Undoes one step of the answer. | Does nothing. |
| `next` | Draws a line under the text. | Does nothing. | Does nothing. |
| `enter` | No hold action. `enter` keys like the tip contact. | Plays the item again from its start, while the item shows. | No hold action. `enter` keys like the tip contact. |

How to hold a button:

1. Push and hold the button.
2. Wait until the screen changes. This takes about 0.8 s. The action comes while you hold the button.
3. Release the button.

Notes:

- The screen shows no hint about these holds. The device gives no beep and no progress mark. The change of the
  screen is the only signal.
- One hold gives one action. A longer hold does nothing more.
- Only Receiving has a hold of `enter`. In Free Input and in a Sending session, `enter` keys like the tip contact.
- If you hold `prev` and `next` together in Free Input, the device clears the text and then draws a line.

### Receiving and Sending

A session that you leave with a hold of `menu/back` is lost. Your level does not change.

The table above is for a session. The start screen and the session end screen are different. There, each button
acts at the push, and a hold gives no command (see [Choose a level](#choose-a-level)):

- A press of `menu/back` opens the main menu.
- `prev` and `next` change the level. A hold repeats the step.
- `enter` starts a session.

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

   Free Input also reads `Settings > Prosigns` each time it opens. It sets how the screen shows AR, BT, KN and AS
   (see [Prosigns setting](#prosigns-setting)).
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
- When a line is full, the next character starts a new line. A word can break across two lines. A prosign text,
  for example `<AR/+>`, can also break across two lines.
- When all 7 lines are full, the text moves up by one line. The top line of text goes away. The title stays.
- A line never starts with a space.

The decoder knows these characters:

- The letters `A` to `Z` and the digits `0` to `9`.
- The signs `.` `,` `?` `/` `=` `+` `-` `(` `)` `&` `_` `"` `@` `'` `;` `!` `:`.
- The prosigns AR, BT, KN, AS and SK.

The signs `+`, `=`, `(` and `&` have the same code as the prosigns AR, BT, KN and AS. The
[Prosigns setting](#prosigns-setting) sets how these four codes show:

| Prosign | Code | `Punctuation` | `Prosign` | `Both` (default) |
| --- | --- | --- | --- | --- |
| AR | `.-.-.` | `+` | `<AR>` | `<AR/+>` |
| BT | `-...-` | `=` | `<BT>` | `<BT/=>` |
| KN | `-.--.` | `(` | `<KN>` | `<KN/(>` |
| AS | `.-...` | `&` | `<AS>` | `<AS/&>` |
| SK | `...-.-` | `<SK>` | `<SK>` | `<SK>` |

SK has no punctuation mark. It always shows as `<SK>`.

How to read a prosign text:

- `<` opens a prosign text and `>` closes it. In Free Input, `<` and `>` appear only in prosign texts. All
  characters between them are one prosign.
- With `Both`, the `/` inside the brackets separates the name and the mark. A keyed `/` (`-..-.`) shows outside the
  brackets. For example, AR and then a keyed `/` with no pause give `<AR/+>/`.
- A prosign text has up to 6 characters. Each character of the text goes to the next free place, as all other
  text does. So at the end of a line, the text can split: for example `<AR` at the end of one line and `/+>` at the
  start of the next line. The `>` shows where the text ends.

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

### Prosigns setting

`Settings > Prosigns` sets how Free Input shows the prosigns AR, BT, KN and AS. It is the third item of
`Settings`, after `Show CW Code`.

| `Prosigns` | AR shows as | What shows |
| --- | --- | --- |
| `Punctuation` | `+` | The punctuation mark only. |
| `Prosign` | `<AR>` | The name of the prosign in angle brackets. |
| `Both` (default) | `<AR/+>` | The name and the mark in angle brackets, with `/` between them. |

- SK always shows as `<SK>`.
- The table in [What the screen shows](#what-the-screen-shows) gives the texts of all five prosigns.
- A change applies the next time you open Free Input. The menu shows no preview of the text, and the device makes
  no sound.
- `Receiving` and `Sending` also use this setting for BT, SK and AR (see
  [Settings for training](#settings-for-training)).

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
| You cannot leave Free Input. | A short press does nothing in Free Input. | Hold `menu/back` for about 0.8 s, until the main menu shows. |
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
- The prosign texts of each `Prosigns` value. The user must read `<AR/+>`, `<KN/(>` and `<AS/&>` at a glance, tell
  `(` from `<`, tell the `/` inside a text from a keyed `/`, and join a text that splits at a line end.

## Koch training

`Receiving` and `Sending` are the two training modes. They use the Koch method: you start with two characters at
the character speed that you set, and each passed session adds a new character.

- In `Receiving`, the device plays a character or a word in Morse code. You pick the characters with the buttons.
- In `Sending`, the device shows a character or a word. You key it with a paddle or a straight key.

Each mode has its own level. The level sets the characters and the length of the session.

When you open `Receiving` or `Sending`, no session starts. The [start screen](#choose-a-level) shows first. On it,
you choose the level and start the session.

### Levels and sessions

A session is a set of items. An item is one character or one word. The device teaches 43 characters in this order:

```text
K M R S U A P T L O W I . N J E F 0 Y , V G 5 / Q 9 Z H 3 8 B ? 4 2 7 C 1 D 6 X BT SK AR
```

BT, SK and AR are prosigns. The sessions come in a fixed order. Two character sessions add two new characters.
Then a word session uses all characters that you learned:

| Level | Session | Characters | Items |
| --- | --- | --- | --- |
| L1 | Characters | K and M | 50 characters |
| L2 | Characters | K, M and the new R | 50 characters |
| L3 | Words | K, M, R | 10 words |
| L4 | Characters | Adds S | 50 characters |
| L5 | Characters | Adds U | 50 characters |
| L6 | Words | K, M, R, S, U | 10 words |
| ... | The same pattern: two character sessions, then one word session | ... | ... |
| L61 | Characters | Adds SK | 50 characters |
| L62 | Characters | Adds AR, the last character | 50 characters |
| L63 | Words | All characters | 10 words |
| L64 and higher | Final word sessions | All characters | 10 words at L64. Each level adds one word, up to 100 words from L154. |

The final word sessions have no end. After L154, each level has 100 words.

Rules for the items:

- In a character session, about half of the items are the new character. The earlier characters share the other
  half equally. At L1, K and M come equally often.
- A word is a random group of the characters that you learned. A word never contains BT, SK or AR. These three
  prosigns come only in character sessions.
- The first two word sessions have words of 2 characters. The next two have words of 3 characters. The length grows
  by one character every two word sessions.
- `Max Word Length` sets the longest word: 4 to 8 characters, default 5. The final word sessions always use words of
  `Max Word Length` characters.

Rules for a pass:

- The score counts only the items that you answer correctly on the first attempt.
- To pass a character session, you need 90 % of the items: 45 of 50 characters.
- To pass a word session, you need 80 % of the words, rounded up: 8 of 10 words, 80 of 100 words. This rule
  applies to every word session, also to the final word sessions.
- A pass at your [earned level](#choose-a-level) moves you to the next level. A fail does not change your level.
- You can choose each level from L1 up to your earned level (see [Choose a level](#choose-a-level)).
- `Reset Statistics` sets all levels back to L1 (see [Reset Statistics](#reset-statistics)).

### Choose a level

Your **earned level** is the level that the device stores for the mode at the current speed (see
[Progress](#progress)). It is the highest level that you can choose. To get a higher level, pass a session at your
earned level.

#### The start screen

When you open `Receiving` or `Sending` from the main menu, the start screen shows. No session runs, and the device
makes no sound. The start screen always shows your earned level first.

Example: Receiving, earned level L20, `RX Speed` 20 WPM and `RX Eff. Speed` 15 WPM:

```text
RX 20/15 L20         1/50



     <  L20 of 20
       New char J

          Start
```

| Row | Example | Meaning |
| --- | --- | --- |
| Header | `RX 20/15 L20         1/50` | The header of the session that you can start. The level is the level that you chose. The item field shows `1/` and the number of items of that level. |
| Selector | `<  L20 of 20` | The level that you chose, and your earned level. `<` and `>` show which steps are possible. |
| Description | `New char J` | What the level that you chose teaches. |
| `Start` | `Start` | The highlight is always on `Start`. `enter` starts the session. |

The selector line:

- `L5 of 20` means: you chose L5, and your earned level is L20.
- `<` shows that `prev` can go one level down. At L1, `<` does not show.
- `>` shows that `next` can go one level up. At your earned level, `>` does not show.
- With an earned level of L1, the line is `L1 of 1`, with no `<` and no `>`.

The description line:

| Level that you chose | Description | Meaning |
| --- | --- | --- |
| L1 | `K and M` | The first two characters, K and M. |
| A character level from L2 | `New char` and the character, for example `New char J` | The new character of this level. |
| A word level, also the final word levels from L64 | `Words of` and a number, for example `Words of 4` | Each word of the session has this number of characters. |

The new character shows in the style of the [Prosigns setting](#prosigns-setting). For example, BT shows as
`New char <BT/=>` with `Both`, `New char <BT>` with `Prosign` and `New char =` with `Punctuation`. The word length
comes from the level and from `Max Word Length` (see [Levels and sessions](#levels-and-sessions)).

#### Buttons on the start screen

| Button | Action |
| --- | --- |
| `prev` | One level down. At L1, it does nothing. |
| `next` | One level up. At your earned level, it does nothing. |
| `enter` | Starts the session at the level that you chose. A closure of the tip contact does the same. |
| `menu/back` | Opens the main menu, with the cursor on the mode. |

- Each button acts at the push.
- Hold `prev` or `next` to step quickly. The first repeat comes after 0.4 s. Then a step comes every 0.08 s (12.5
  levels per second). From L1, a hold of `next` reaches L20 in about 1.8 s. From L1 to L150 takes about 12 s.
- The level stops at L1 and at your earned level. It does not wrap.
- A step gives no sound. The new number on the screen is the only signal.
- A hold of `menu/back` or `enter` does nothing more than the push.
- The ring contact does nothing.
- The start screen has no timeout.

The level that you choose is not stored. When you leave the mode and open it again, the start screen shows your
earned level again.

> **Insert the plug before you open a training mode.** On the start and end screens, the tip is `enter`. A
> closure of the tip contact acts at once. A plug insertion can close the tip contact for a short time. Then a
> session can start without a press. If this happens, hold `menu/back` for about 0.8 s to leave the session. A
> session that you leave does not change your level.

#### Replay a lower level

A session below your earned level is a replay. A replay never changes your earned level and never saves anything.
The rules:

- A pass below your earned level keeps the stored level.
- A pass at your earned level moves your earned level up by one and saves it.
- A fail does not change your level.

Example: your earned level is L20. You choose L5 and pass. The session end screen shows `Passed` and
`<  L6 of 20  >`. If you press `enter`, the next session is L6. Your earned level stays L20, and the selector still
goes up to L20.

#### The level after a session

After a session, the session end screen shows the same selector (see [Session end screen](#session-end-screen)).
It shows the next level first:

- After a pass, the next level: the level of the session + 1.
- After a fail, the same level again.

After a pass at your earned level, the next level is your new earned level, for example `L14 of 14`.

### Level in the main menu

In the main menu, `Receiving` and `Sending` show your earned level at the right side of the row:

```text
CW Trainer
Receiving           L12
Sending              L1
Free Input
Settings                >
```

- `Receiving` shows the level at the current `RX Speed`. `Sending` shows the level at the current `TX Speed`. The
  effective speeds have no effect.
- The level changes after a pass at your earned level and after `Reset Statistics`. It can also change after a
  change of `RX Speed` or `TX Speed`. A replay does not change it.
- Each speed has its own level. So after a change of `RX Speed`, `Receiving` can show `L1`, although you trained at
  another speed. Set the speed back, and your level shows again.

### Settings for training

When you open a mode, the device reads its settings once. The start screen and each session use these settings. A
new session from the session end screen keeps the same settings. To change a setting, leave the mode: press
`menu/back` on the start screen or the session end screen, or hold it during a session. Change the setting, and leave
`Settings` to save it. Then open the mode again.

| Setting | In `Receiving` | In `Sending` |
| --- | --- | --- |
| `Tone` | The pitch of the playback. | The pitch of the sidetone. |
| `RX Speed` | The character speed of the playback. It also selects the stored level (see [Progress](#progress)). | No effect. |
| `RX Eff. Speed` | The effective speed of the playback. | No effect. |
| `TX Speed` | No effect. | The speed of the keyer and the decoder. It also selects the stored level. |
| `TX Eff. Speed` | No effect. | Shows in the header. It sets the pause limit between the characters of a word (see [Pause between characters of a word](#pause-between-characters-of-a-word)). |
| `Key Type`, `Paddle` | No effect. | The keyer, as in Free Input (see [Key types](#key-types) and [Paddle setting](#paddle-setting)). |
| `Show CW Code` | No effect. | Shows the dots and dashes that you key (see [Sending](#sending)). |
| `Prosigns` | How BT, SK and AR show in the items, in the keyboard labels and in the description line of the selector. | How BT, SK and AR show in the items, in the `Wrong:` text and in the description line of the selector. |
| `Max Word Length` | The longest word. | The longest word. |

The `Prosigns` setting gives these texts. For example, BT shows as `=`, `<BT>` or `<BT/=>`. SK always shows as
`<SK>`. The table in [What the screen shows](#what-the-screen-shows) gives all texts.

### The training screen

Both modes use the same screen layout. The top row is the header. It shows the mode, the speeds, the level and the
item number:

```text
RX 20/15 L12         7/50
```

| Field | Example | Meaning |
| --- | --- | --- |
| Mode | `RX` | `RX` in Receiving, `TX` in Sending. |
| Speeds | `20/15` | The character speed and the effective speed, in WPM. Receiving shows `RX Speed` and `RX Eff. Speed`. Sending shows `TX Speed` and `TX Eff. Speed`. |
| Level | `L12` | The level of this session. On the start screen, the level that you chose. |
| Item | `7/50` | The number of the current item and the number of items in the session. |

Below the header:

- The second row is the status line. It shows the result of a check, for example `Correct` or `Try 2 of 3`.
- The middle of the screen shows the item, or the slots of your answer, in large characters.
- In Receiving, the bottom row shows the four keyboard labels. A short bar above each label marks its button. In
  Sending, the bottom row shows the dots and dashes that you key.

Each session starts with a pause of 1 s. Then the first item comes. Before each next item, the item area goes blank
for a short moment. So you can see the start of each new item, also for the same character again. During a
pause, the header already shows the number of the next item.

The screen shows no hint about how to leave. To leave a session at any time, hold `menu/back` for about 0.8 s. The
main menu shows with the cursor on the mode. The session is lost, and your level does not change.

### Receiving

In Receiving, you listen to an item and pick its characters with the four buttons.

#### Start, answer and leave

1. If you use a key or paddle, insert its plug now (see
   [Key or paddle in Receiving](#key-or-paddle-in-receiving)).
2. In the main menu, select `Receiving` and press `enter`. The start screen shows. No session starts yet.
3. Choose the level with `prev` and `next` (see [Choose a level](#choose-a-level)).
4. Press `enter`, or close the tip contact. The session starts. This press picks nothing and replays nothing. A
   hold of this press also picks nothing and replays nothing.
5. Wait 1 s. The device plays the first item at `RX Speed` and `RX Eff. Speed`.
6. Look at the slots in the middle of the screen. Each character of the item has one slot. A block marks the slot
   that your next pick fills. A short bar marks each other empty slot.
7. Pick the characters with the keyboard (see [The group keyboard](#the-group-keyboard)). You can pick while the
   item plays. Each pick shows at once.
8. Fill the last slot. The device checks the whole answer at once. If the item still plays, the playback stops.
9. Read the result in the status line (see [Results and attempts](#results-and-attempts)). The next item comes
   after the result.
10. To leave, hold `menu/back` for about 0.8 s.

In a Receiving session, a short press is a tap. You must release the button before about 0.8 s. When you release the
button, the tap acts. The push alone does nothing. A tap of `enter` selects the right group of the keyboard. It
does not replay the item.

To hear the item again, hold `enter` for about 0.8 s while the item shows:

- The item plays again from its start. This also works while the item plays.
- The replay starts while you still hold the button. The screen does not change. When you hear the playback start
  again, release the button.
- Your picks and the labels stay. A replay is free: it uses no attempt and does not change the score.
- A hold of `enter` does nothing during the start pause, a result or the blank moment.

#### Key or paddle in Receiving

The tip contact of the key/paddle socket is connected to `enter`. So in Receiving, the dit lever (with
`Paddle` = `Normal`) or a straight key acts as `enter`:

- On the start screen and the session end screen, a closure starts a session at once, like a press of `enter`.
- In a session, a short closure is a tap of `enter`. It selects the right group.
- In a session, a closure of about 0.8 s is a hold of `enter`. It plays the item again.

The ring contact does nothing. The paddle gives no sidetone in Receiving.

> **Insert the plug before you open Receiving.** A plug insertion can close the tip contact for a short time.
> During Receiving, this acts as a tap of `enter`. So it can select the right group. If that group has two or more
> characters, the labels change. Hold `prev` to go back. If that group has one character, the device picks it.
> Hold `prev` to undo the pick. But if the pick fills the last slot, the device checks the answer at once. This
> uses an attempt, and you cannot undo it. On the start screen and the session end screen, a plug insertion can
> start a session.

#### The group keyboard

The bottom row shows four labels, one above each button. From left to right, the buttons are `menu/back`, `prev`,
`next` and `enter`. Each label shows a group of characters. A short bar above each label marks its button. Also
for a blank label, the bar shows.

The keyboard holds only the characters that you learned. It shows them in this order: `A` to `Z`, `0` to `9`, `.`
`,` `?` `/`, then BT, SK and AR. In a word session, the keyboard has no BT, SK and AR.

The keyboard splits its characters into four groups, one for each button. The groups keep the keyboard order. Their
sizes differ by one character at most, and the larger groups are on the left. For example, 43 characters give
groups of 11, 11, 11 and 10 characters.

Example: all 43 characters learned, `Prosigns` = `Both`, before the first pick. `#` is the block in the slot:

```text
RX 20/15 L62        12/50


            #
            #

_____ _____ ____ ________
A-K    L-V  W-6    7-<AR>
```

The labels `A-K`, `L-V`, `W-6` and `7-<AR>` belong to `menu/back`, `prev`, `next` and `enter`.

| Label | Example | A tap of the button below it |
| --- | --- | --- |
| One character | `K`, `<BT>` | Picks the character. It goes into the slot with the block. |
| A list of characters | `KM`, `ABC` | Splits these characters into four smaller groups. The labels change. |
| A range | `A-K`, `7-<AR>` | Splits the learned characters from the first to the last into four smaller groups. The labels change. |
| Blank | | Does nothing. |

How to read a label:

- A list shows the characters with no space between them, for example `KM` or `<SK><AR>`.
- A range shows the first character, `-` and the last character. A `-` in a label always means a range. The
  keyboard has no `-` character.
- A range holds only the characters that you learned. For example, with 30 characters learned, `A-K` holds A, E,
  F, G, H, I, J and K. You did not learn B, C and D yet.
- The `enter` label has more room than the other labels. So it can show a list while the other labels show ranges.
- A prosign shows as `<BT>`, `<SK>` or `<AR>`. With `Prosigns` = `Punctuation`, BT shows as `=` and AR as `+`. SK
  always shows as `<SK>`.
- The slots show the full text of the `Prosigns` setting. So with `Both`, the label `<AR>` picks AR, and the slot
  shows `<AR/+>`.
- After each pick, the labels show all learned characters again.

Example: pick AR from the screen above.

1. The labels show `A-K`, `L-V`, `W-6` and `7-<AR>`. Tap `enter`.
2. The labels show `789`, `.,?`, `/<BT>` and `<SK><AR>`. Tap `enter`.
3. The labels show `<SK>` and `<AR>`. The `next` and `enter` labels are blank. Tap `prev`. AR goes into the slot,
   and the slot shows `<AR/+>`.

One pick needs 3 taps at most:

| Characters on the keyboard | Taps for one pick |
| --- | --- |
| 2 to 4 | 1 |
| 5 to 16 | 1 or 2 |
| 17 to 43 | 2 or 3. With 43 characters, and with the 40 characters of a word session, each pick needs 3 taps. |

With 2 characters, the `next` and `enter` labels are blank. With 3 characters, the `enter` label is blank. A tap of a
button with a blank label does nothing.

If you hold a button for about 0.8 s, the device sees a hold, not a tap. A hold picks nothing. A held button gives no
repeat. The holds give commands (see [Hold a button in a mode screen](#hold-a-button-in-a-mode-screen)).

#### Undo

To undo one step, hold `prev` for about 0.8 s:

- If the labels show a smaller group, they go back one group level.
- If the labels show all characters, the last pick goes away. The block moves to that slot.
- If no slot holds a pick, nothing happens.

One hold undoes one step. To undo two steps, hold `prev` two times. The undo does not change the playback. A hold
of `next` does nothing.

A pick that fills the last slot starts the check at once. You cannot undo a check.

### Sending

In Sending, you read an item and key it. The device decodes each character and checks it at once.

#### Before you start

1. Set the keyer in `Settings > CW`: `Key Type`, `Paddle`, `TX Speed`, `TX Eff. Speed` and `Tone`. Sending uses
   the same keyer as Free Input (see [Key types](#key-types) and [Speeds and pauses](#speeds-and-pauses)).
2. Set `Show CW Code` in `Settings`. With `On`, the screen shows the dots and dashes that you key.
3. Press `menu/back` until the main menu shows. The device saves the changed settings.
4. Insert the plug of the key or paddle into the key/paddle socket.

> **Insert the plug before you open Sending.** A plug that you insert during a session can close a contact. Then
> the keyer can send a tone, extra elements and a wrong character. A wrong character uses an attempt. On the start
> screen and the session end screen, a plug insertion can start a session.

#### Start, key and leave

1. In the main menu, select `Sending` and press `enter`. The start screen shows. No session starts yet, and the
   keyer does not run. On this screen, the tip contact and the `enter` button act as `enter`.
2. Choose the level with `prev` and `next` (see [Choose a level](#choose-a-level)).
3. Press `enter`, or close the tip contact. The session starts, and the keyer starts. This press gives no element
   and no tone. If a contact stays closed, the keyer ignores it until it opens.
4. Wait 1 s. The first item shows in large characters.
5. Key the item. You hear the sidetone. After a pause of 2 dits, the device decodes the character and checks it.
6. In a word, a short bar shows under the character that you key next. After each correct character, the bar moves
   to the next character. The status line shows nothing.
7. After the last correct character, the status line shows `Correct`. Then the next item comes.
8. To leave, hold `menu/back` for about 0.8 s.

Notes:

- One wrong character ends the attempt. The status line shows `Wrong:` and the character that you keyed, for
  example `Wrong: N`. `*` means a pattern that the decoder does not know. It is always wrong.
- In a word, a long pause between two characters also ends the attempt (see
  [Pause between characters of a word](#pause-between-characters-of-a-word)).
- In the next attempt, key the whole item again from its first character (see
  [Wrong attempts and the retry](#wrong-attempts-and-the-retry)).
- After the last character of a word, a pause does nothing. You do not have to key a word space at the end of a
  word.
- In a session, short presses do nothing. `enter` keys like the tip contact, as in Free Input. A hold of `prev` or
  `next` does nothing.

#### Code row

With `Show CW Code` = `On`, the bottom row shows one mark for each element that you key. A short mark is a dot, and
a long mark is a dash. The marks show what the keyer decoded. They stay until you start the next character, or
until the retry starts. So after a wrong character, you can see what you keyed. The row shows at most 12 marks for
one character. With `Off`, the bottom row stays blank.

#### Pause between characters of a word

In a word session, key the word as one word. Start the next character before the pause reaches 5 units of the
`TX Eff. Speed` spacing. With `TX Eff. Speed` equal to `TX Speed`, one unit is one dit. Free Input inserts a word
space at the same point (see [Speeds and pauses](#speeds-and-pauses)). The pause starts at the end of the last
element of a character. It ends at the start of the first element of the next character.

A longer pause is a wrong attempt, the same as a wrong character:

- The status line shows `Wrong: pause too long`.
- The bar stays under the next character. It shows where the pause came.
- The pause uses an attempt. In the next attempt, key the whole word again from its first character.
- After the third wrong attempt, the code of the character under the bar shows below the item.

A pause does nothing in these cases:

- Before the first character of an attempt.
- After the last character of the word.
- In a character session.
- After a character that the device lost (see [Keying during a pause](#keying-during-a-pause)).
- During the start pause, a result or the blank moment.

Examples (values are rounded):

| `TX Speed` | `TX Eff. Speed` | Normal pause between characters (3 units) | Pause limit (5 units) |
| ---: | ---: | ---: | ---: |
| 32 WPM | 32 WPM | 113 ms | 188 ms |
| 20 WPM | 20 WPM | 180 ms | 300 ms |
| 13 WPM | 13 WPM | 277 ms | 461 ms |
| 5 WPM | 5 WPM | 720 ms | 1200 ms |
| 20 WPM | 10 WPM | 654 ms | 1090 ms |
| 20 WPM | 5 WPM | 1601 ms | 2669 ms |

The normal pause between characters is 3 units. So you have 2 units of margin before the limit. If you need more
time, set a lower `TX Eff. Speed`. A lower value gives a longer pause limit.

#### Wrong attempts and the retry

After a wrong attempt 1 or 2:

1. The status line shows the result for 1.2 s: `Wrong:` and the keyed character, or `Wrong: pause too long`.
   During this time, the bottom row keeps the code that you keyed, so you can read it. After a pause error, the
   bottom row shows the code of the last correct character.
2. Then the retry starts. The status line shows `Try 2 of 3` or `Try 3 of 3`. The rows below the item are blank,
   with `Show CW Code` = `On` and with `Off`. In a word, the bar goes back to the first character.
3. Key the whole item again from its first character.

Only the status line tells you that this is a retry. The rest of the screen looks like a new item.

#### Keying during a pause

The keyer and the sidetone also work during the start pause, a result and the blank moment. You hear what you key,
but the device ignores it.

- If you start a character during a pause, the device ignores the whole character. This is also true for the part
  that you key after the pause. Key the character again.
- If the display falls far behind the keyer, the device ignores the character in progress. It uses no attempt and
  shows no message. Key the character again.

### Results and attempts

You have 3 attempts for each item. The status line shows the result of each check. The device makes no sound for a
result.

| Result | Status line | Time on the screen | Then |
| --- | --- | --- | --- |
| Correct | `Correct` | 0.6 s | The blank moment, then the next item. |
| Wrong, attempt 1 or 2 | Receiving: `Wrong`. Your picks stay on the screen. Sending: `Wrong:` and the keyed character, or `Wrong: pause too long`. The code that you keyed stays in the bottom row. | 1.2 s | The next attempt. The status line shows `Try 2 of 3` or `Try 3 of 3`. In Receiving, the slots are empty and the item plays again at once. In Sending, the bottom row is blank, and in a word the bar goes back to the first character. |
| Wrong, attempt 3 | Receiving: `Wrong. The answer is:`. The answer shows in the middle of the screen and plays once more. Sending: `Wrong:` and the keyed character, or `Wrong: pause too long`. The dots and dashes of the expected character show below the item. After a pause error, this is the character under the bar. | Receiving: 2.5 s after the end of the playback. Sending: 2.5 s. | The blank moment, then the next item. |

- In Sending, the dots and dashes of the expected character show after the third wrong attempt, also with
  `Show CW Code` = `Off`.
- An item that you answer correctly on attempt 2 or 3 does not add to the score.
- The blank moment takes 0.5 s in Receiving and 0.3 s in Sending.
- After the last item, the session end screen shows at once.

### Session end screen

After the last item, the session end screen shows. The header of the session stays. The screen shows your result
and the level selector. A pass at your earned level looks like this:

```text
RX 20/15 L13        50/50

       Score 46/50
    Passed, new level
     <  L14 of 14
       New char W

          Start
```

A fail looks like this:

```text
RX 20/15 L13        50/50

       Score 43/50
   Not passed, need 45
     <  L13 of 13
       New char O

          Start
```

A pass below your earned level (a replay) looks like this. Your earned level is L20:

```text
RX 20/15 L5         50/50

       Score 47/50
         Passed
     <  L6 of 20  >
       Words of 2

          Start
```

| Row | Text | Meaning |
| --- | --- | --- |
| 1 | `Not saved` | After a pass at your earned level: the device did not store the new level (see [Progress](#progress)). Else the row is blank. |
| 2 | `Score 46/50` | Items correct on the first attempt, and the number of items. |
| 3 | `Passed, new level` | You passed at your earned level. Your earned level went up by one. |
| 3 | `Passed` | You passed below your earned level, or at the top level L65535. Your earned level does not change. |
| 3 | `Not passed, need 45` | You did not pass. The number is the score that you need. Your level does not change. |
| 4 | `<  L14 of 14` | The level selector. It shows the level of the next session first: the next level after a pass, the same level after a fail (see [The level after a session](#the-level-after-a-session)). |
| 5 | `New char W` | The description of the level that the selector shows (see [Choose a level](#choose-a-level)). |
| 7 | `Start` | The highlight is always on `Start`. |

- Row 3 tells a pass at your earned level from a replay. After a pass at your earned level, both numbers of the
  selector are equal, for example `L14 of 14`. After a replay, the second number stays your earned level, for
  example `L6 of 20`.
- At the top level L65535, a pass shows `Passed`, and the selector shows `L65535 of 65535`.
- The header shows the session that ended. A change of the level does not change the header.

Controls: the same as on the start screen (see [Buttons on the start screen](#buttons-on-the-start-screen)).

- Press `prev` or `next` to choose the level. A hold repeats the step.
- Press `enter` to start the next session at the level that the selector shows. A closure of the tip contact also
  starts it. The session uses the settings from the mode start.
- Press `menu/back` to go to the main menu. The cursor is on the mode. The level that you chose is lost.
- Each button acts at the push, in both modes. A hold of `menu/back` or `enter` does nothing more than the push.
- The end screen ignores a button or a contact that you hold from the session. Release it, and then press it
  again. So a press from the last item does not change the level or start a session.
- In Sending, the keyer stops at the session end screen. The ring contact does nothing on this screen. The press
  that starts the next session gives no element and no tone.
- A plug insertion can start the next session.
- The screen has no timeout.

### Progress

The device stores one level for each mode and each character speed from 5 to 50 WPM:

- Receiving uses the level of the current `RX Speed`. Sending uses the level of the current `TX Speed`.
- The effective speeds do not select a level.
- A speed that you never trained starts at L1.

Example: you are at L12 in Receiving at `RX Speed` 20 WPM. You set `RX Speed` to 25 WPM. The main menu and the
start screen now show the level that you have at 25 WPM, for example L1. When you set `RX Speed` back to 20 WPM,
they show L12 again.

When the device saves the level:

- The device saves only after a pass at your earned level. It saves before the session end screen shows.
- A pass below your earned level (a replay) does not change the stored level. The device does not save.
- A failed session, a session that you leave, and a power loss during a session do not change the stored level.
- If the stored levels are damaged, every level is L1. This is also true at the first start. The device shows no
  message.

If the save fails, the session end screen shows `Not saved`. A low battery voltage is one possible cause:

1. Start the next session. The new level stays in use until you switch off the device.
2. Pass a session. The device then tries the save again.
3. If you switch off before a save works, the next start can use the earlier levels, or L1 for all speeds in both
   modes.

> **Caution:** Wait until the session end screen shows before you switch off. To save, the device first erases all
> stored levels and then writes them again. This takes a few milliseconds. If the power stops at that moment, the
> next start can use L1 for all speeds in both modes. The device shows no message.

The levels and the settings are in the same memory. A firmware download can keep or delete them. Nobody checked
this on the device yet.

### Problems in training

| Problem | Possible cause | What to do |
| --- | --- | --- |
| You cannot leave a session. | In Receiving, a short press of `menu/back` is a keyboard key. In Sending, it does nothing. | Hold `menu/back` for about 0.8 s, until the main menu shows. |
| A press picks nothing in Receiving. | The device saw a hold. Or a start pause, a result or the blank moment shows. Or the label above the button is blank. | Press and release the button quickly while the item shows. |
| You hear no playback in Receiving. | The volume knob is at its minimum. | Turn up the volume. Hold `enter` for about 0.8 s to play the item again. |
| The item plays again without a press. | You inserted or removed a plug. A tip closure of about 0.8 s acts as a hold of `enter`. | Nothing. A replay is free. |
| In Receiving, a character was picked without a press, or an attempt was used after you inserted the plug. | You inserted the plug while Receiving was open. A short tip closure acts as a tap of `enter`. | Insert the plug before you open Receiving. To undo a pick that the device did not check yet, hold `prev`. |
| In Sending, a tone sounds or a wrong character shows without a keyed element. | You inserted or removed a plug during the session. A mono plug closes the ring contact. | Hold `menu/back` to leave. Insert the plug, then open `Sending` again. |
| In Sending, a character that you keyed does not count. | You started it during a pause. | Key the character again. |
| In Sending, many characters are wrong or show as `*`. | Your speed is different from `TX Speed`. The decoder does not adapt. | Set `TX Speed` near your keying speed. |
| In Sending, `Wrong: pause too long` shows while you key at a normal rhythm. | Your pause between two characters of a word is too long. The limit is 5 units, only 2 units more than the normal pause. | Set a lower `TX Eff. Speed`. A lower value gives a longer pause limit (see [Pause between characters of a word](#pause-between-characters-of-a-word)). |
| The level is L1, but you trained before. | You changed `RX Speed` or `TX Speed`. Each speed has its own level. | Set the speed back. |
| A session starts without a press on the start screen or the session end screen. | You inserted the plug while the screen showed. On these screens, a closure of the tip contact starts a session at once. | Hold `menu/back` for about 0.8 s to leave the session. Your level does not change. Insert the plug before you open the mode. |
| `next` does not go above a level. | The selector stops at your earned level. | Pass a session at your earned level. Then the next level is available. |
| You passed, but your level in the main menu did not change. | You passed below your earned level. A replay does not change the stored level. The end screen showed `Passed`, not `Passed, new level`. | Nothing. To move up, choose your earned level and pass. |
| All levels are L1. | A reset, a failed save, or a power loss during a save. | Train the levels again. |

### Not checked on the device

These parts of the training modes passed host tests only. Nobody checked them on the device yet:

- The timing of the playback, and no clicks at the start and the end of each element.
- The legibility of the large characters, the block, the bars and the dot and dash marks.
- The four keyboard labels sit above `menu/back`, `prev`, `next` and `enter`, in this order.
- The feel of a tap that acts at the release.
- A hold of `enter` plays the item again with no group change and no pick first.
- Short tip closures and tip closures of 0.8 s, with the dit lever of a paddle and with a straight key.
- A plug insertion in Receiving during an item.
- The blank moment of 0.3 s in Sending. The display can be too slow to show it.
- The pause limit in Sending words, with a paddle and with a straight key, at 20/20, 13/13 and 20/10 WPM. Words
  with the normal 3-unit pause must pass. A pause of about 6 units must give `Wrong: pause too long`. A pause of
  about 4 units must not.
- The retry screen in Sending: the rows below the item must be blank at the retry. The wrong code in the bottom
  row must be readable during the 1.2 s result.
- The time on the screen of each result.
- A held lever at the session end and at the start of the next session.
- A stored level after a power cycle, and L1 after `Reset Statistics` and a power cycle.
- The feel of the level selector in both modes, on the start screen and on the session end screen. This includes
  single presses of `prev` and `next`, and holds over 20 levels or more, up to an earned level above L50. The number
  must follow a hold at about 12.5 levels per second, and the last level must show within 0.1 s after the release.
  You must be able to stop on a level that you want.
- The start of a session from the start screen and from the session end screen. Use the `enter` button, the dit
  lever of a paddle and a straight key, in both modes. One closure must start one session. A contact that you hold
  after the start must give no pick and no replay in Receiving, and no element and no tone in Sending.
- A plug insertion while the start screen or the session end screen shows. It can start a session.
- The legibility of the selector, of `<` and `>`, of the description line (for example `New char .` and
  `New char <AR/+>`), and of the level in the main menu on the selected row.
- The level next to `Receiving` and `Sending` in the main menu.

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
| Settings | `Prosigns` | `Punctuation`, `Prosign`, `Both` | - | - | `Both` |
| Settings | `Max Word Length` | 4 to 8 | 1 | - | 5 |
| Settings > Display | `Backlight` | `Off`, 1 to 10 | 1 | - | 8 |
| Settings > Display | `Contrast` | 0 to 63 | 1 | - | 32 |

`RX Speed` and `TX Speed` are the character speeds. `RX Eff. Speed` and `TX Eff. Speed` are the effective
(Farnsworth) speeds.

`Backlight` and `Contrast` change the display at each step while you edit them. The other settings take effect the
next time you open Free Input, `Receiving` or `Sending`. [Settings for training](#settings-for-training) tells which
setting affects which training mode.

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
keeps the settings of an earlier firmware version, `Prosigns` is `Both`, and the other settings stay as they were.
The device shows no message. If the earlier version had no `Paddle` setting, `Paddle` is `Normal`.

This version saves the settings in a new format. It writes the new format at the first save, after you change a
setting. An earlier firmware version cannot read the new format. If you download an earlier firmware version after
this version saved the settings, the earlier version uses all default values. It shows no message. Set the values
again.

## Reset Statistics

`Reset Statistics` sets all training levels back to L1: both modes, all speeds. It does not change the settings.

1. In `Settings`, select `Reset Statistics` and press `enter`.
2. The dialog `Reset statistics?` shows. The focus is on `No` each time the dialog opens.
3. Press `next` to select `Yes`, or `prev` to select `No`.
4. Press `enter`:
   - With `No`, the dialog closes and `Settings` shows.
   - With `Yes`, the device resets the levels and shows `Statistics reset` and `OK`. Press `enter` or
     `menu/back` to go back to `Settings`.

`menu/back` in the dialog is the same as `No`.

If the device cannot store the reset, it shows `Not saved` and `OK` in place of `Statistics reset`. Press `enter` or
`menu/back` to go back to `Settings`. Then do the reset again. After a failed reset, the next start can use the
earlier levels, or L1 for all speeds.

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
