# User manual

This manual tells you how to use the menu and the settings of the custom firmware for the Putikeeg CW Trainer
(board `CW_MINI_V2.0D`).

> **Not tested on the device.** This firmware version passed host tests only. Nobody tested it on the device
> yet. The behavior in this manual comes from the firmware design and the host tests. On the device, some
> details can be different, for example the button response time, the readable contrast range and the backlight
> levels.

## What this version does

This version has the menu, the settings and the display controls. The training modes are not available yet:

- `Receiving`, `Sending` and `Free Input` show the message `Not ready`.
- The only sound is the Tone preview. When you edit `Tone` in `Settings > CW`, the device plays the edited tone
  for about 0.3 s. Each change of the value plays the new tone again. At a limit, a press plays nothing new. When
  you leave the edit, the tone stops at once. The volume knob sets the loudness. With headphones plugged in, the
  headphones play instead of the speaker.
- The device collects no statistics.
- Only `Backlight` and `Contrast` have a visible effect now. The firmware saves the other settings for the
  training modes of a later version.

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
  `enter`. The other buttons continue to work.
- If you insert a plug into the paddle socket while the menu shows, the device can see one `enter`. Then it can
  open an item or start editing. Press `menu/back` to go back. Nothing changes. This effect is not checked on the
  device.
- **Hold for auto-repeat.** To repeat the action of `prev` or `next`, hold the button. The first repeat comes
  after 0.4 s. After that, the action repeats every 0.08 s (12.5 steps per second). `enter` and `menu/back` do
  not repeat.
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
├── Free Input          shows "Not ready"
└── Settings
    ├── CW
    │   ├── Tone
    │   ├── RX Speed
    │   ├── RX Eff. Speed
    │   ├── TX Speed
    │   ├── TX Eff. Speed
    │   └── Key Type
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

### Receiving, Sending and Free Input

In this version these items open a screen with the message `Not ready`. Only `menu/back` works on this screen. It
takes you back to the main menu.

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
| Settings | `Show CW Code` | `Off`, `On` | - | - | `On` |
| Settings | `Max Word Length` | 4 to 8 | 1 | - | 5 |
| Settings > Display | `Backlight` | `Off`, 1 to 10 | 1 | - | 8 |
| Settings > Display | `Contrast` | 0 to 63 | 1 | - | 32 |

`RX Speed` and `TX Speed` are the character speeds. `RX Eff. Speed` and `TX Eff. Speed` are the effective
(Farnsworth) speeds.

`Backlight` and `Contrast` change the display at each step while you edit them. The other settings have no effect
in this version.

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

A firmware download can keep or delete the saved settings. Nobody checked this on the device yet.

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
