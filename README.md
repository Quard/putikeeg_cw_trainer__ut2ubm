# Putikeeg CW Trainer — UT2UBM mod

Custom firmware and reverse-engineered hardware documentation for the
Putikeeg CW Trainer. The firmware targets the STC8H3K64S2 with SDCC's MCS-51
backend and drives the trainer's 128 × 64 LCD and four front-panel controls.

* The [quick guide](docs/quick_guide.md) is a short introduction for users.
* The [user manual](docs/user_manual.md) describes the menu, the settings and the
Free Input mode.
* The [firmware download guide](docs/firmware_download.md) tells
you how to connect the trainer to a computer and download the firmware.
* The [release notes](docs/release_notes.md) list the changes.

## Project overview

Learn Morse code on a trainer that keeps up with your hand.

The Putikeeg CW Trainer is a small, battery-powered Morse code trainer with a
screen, a speaker, a headphone socket and a socket for your key. The stock
firmware has few functions, and it trips over an iambic paddle. This project
gives the same device new firmware, written from scratch, that takes you from
your first two letters to copying and sending whole words.

### What you get

- **Practice with your own paddle.** Plug in a paddle or a straight key and
  key freely. You hear a clean sidetone, and the screen shows what you sent.
  Instant feedback, no computer needed.
- **A keyer that does iambic right.** Choose `Straight`, `Iambic A` or
  `Iambic B`. Squeeze the paddle and get exact dits, dahs and spaces.
  Left-handed, or a paddle wired the other way? One setting swaps the contacts.
- **Learn with the Koch method.** Two training modes cover both sides of a
  QSO. In `Receiving`, you copy what the trainer plays. In `Sending`, you key
  what it shows. 43 characters, two new ones at a time, then whole words.
- **Progress that sticks.** The trainer remembers your level for each mode and
  each speed, even after power-off. Go back to any level you passed to brush up.
- **Your speed, your spacing.** Set character speed and Farnsworth spacing
  separately for receiving and sending, from 5 to 50 WPM.
- **A tone you can listen to for hours.** Pick a pitch from 400 to 1000 Hz.
  Soft edges remove key clicks. Plug in headphones for quiet practice.
- **Speaks ham.** The decoder reads letters, digits, punctuation and the
  prosigns AR, BT, KN, AS and SK, shown the way you prefer.
- **Free and open.** The firmware is open source.

To start using the device with this firmware, read the
[**Quick Guide**](docs/quick_guide.md).

The [release notes](docs/release_notes.md) list what is new.

## Prerequisites

- CMake 3.20 or newer
- SDCC with MCS-51 support
- A host C11 compiler for unit tests
- `stcgal` 1.10 or newer for normal firmware downloads
- `stc8prog` only for destructive recovery or factory erasure

The normal programmer configuration uses a 24 MHz internal clock and divides
the MCU's 64 KiB flash into 60 KiB of program space and 4 KiB of internal
EEPROM.

## Build

From the repository root:

```sh
./firmware/build.sh
```

The firmware image is written to
`firmware/build/putikeeg_cw_trainer.ihx`. The build also prints the SDCC memory
report and fails if the program exceeds its 60 KiB partition.

Run the host tests with:

```sh
./firmware/test.sh
```

## Pre-commit hooks

The repository uses [pre-commit](https://pre-commit.com/) to check each commit.
The hooks check the files that you commit. The `reuse lint-file` hook makes
sure that each source file has the `SPDX-FileCopyrightText` and
`SPDX-License-Identifier` lines. The license texts are in `LICENSES/`. The
`clang-format` hook formats the C files with `.clang-format`.

To install the hooks, run this command from the repository root:

```sh
pre-commit install
```

To check all files, run:

```sh
pre-commit run --all-files
```

## Sidetone hardware bring-up

The firmware plays a CW sidetone on board revision `CW_MINI_V2.0D`. In this
version, the device plays the keyer sidetone in Free Input and the Tone preview
in `Settings > CW > Tone` (see the [user manual](docs/user_manual.md)). The
normal build has no special sidetone configuration.

The route is automatic. P1.0 detects the headphone plug. With the plug in
(P1.0 LOW), the headphone route P1.2 plays. Without the plug, the speaker route
P1.1 plays. A plug change takes effect at the next tone start. Both routes are
quiet at a HIGH level. The PWM pulses are LOW-active, with at most 50 % duty.

## Download firmware

To connect a USB-to-serial adapter to the trainer, read the
[firmware download guide](docs/firmware_download.md). Then run:

```sh
./firmware/flash.sh /dev/ttyUSB0
```

The serial-port argument is optional. It defaults to `STCGAL_PORT`, then to
`/dev/ttyUSB0`:

```sh
STCGAL_PORT=/dev/ttyACM0 ./firmware/flash.sh
```

The script builds the firmware, starts `stcgal`, configures the 60 KiB/4 KiB
split, and waits for the STC ISP bootloader. Power-cycle the trainer when
prompted. A normal download erases only program flash and preserves settings
and learning progress in EEPROM.

The EEPROM layout is:

| IAP address | Purpose |
|---|---|
| `0x0000–0x01FF` | Persistent settings |
| `0x0200–0x0FFF` | Learning progress reserved for future firmware |

If the serial port cannot be opened, verify the device name and that the user
has access to the serial-port group. If synchronization times out, leave the
script running and power-cycle the trainer again.

## Destructive recovery

`stc8prog -e` erases the whole chip. It destroys all settings and learning
progress and does not establish the required 60 KiB/4 KiB partition, so it is
not used by `flash.sh`.

For a deliberate factory recovery, erase and program with `stc8prog`, then
immediately run the normal script to restore the partition configuration:

```sh
stc8prog -p /dev/ttyUSB0 -e \
  -f firmware/build/putikeeg_cw_trainer.ihx
./firmware/flash.sh /dev/ttyUSB0
```

Power-cycle the trainer when each programmer asks for the ISP bootloader.

## License

Copyright (c) 2026 Vadym Zakovinko.

The firmware, the scripts and the documentation use the
[BSD 3-Clause License](LICENSE). The hardware design files in `hardware/` use
the [CERN Open Hardware Licence Version 2 - Permissive](hardware/LICENSE)
(CERN-OHL-P-2.0). Each source file names its license with an
`SPDX-License-Identifier` line.

Both licenses permit use, change and distribution, also in closed products. If
you distribute this work or a work based on it, you must keep the copyright
notice and the license text.

Third-party components keep their own licenses. The firmware contains the
Spleen font under the BSD-2-Clause license. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md). The documents in
`references/context/`, for example the MCU datasheet, belong to their authors
and are not part of the licensed work.

The schematic and the PCB in `hardware/` are a reverse-engineered description
of the original Putikeeg CW Trainer. The CERN-OHL-P covers this description
only. The rights to the original product design stay with its manufacturer.
