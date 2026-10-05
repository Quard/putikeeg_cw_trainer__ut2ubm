# Putikeeg CW Trainer — UT2UBM mod

Custom firmware and reverse-engineered hardware documentation for the
Putikeeg CW Trainer. The firmware targets the STC8H3K64S2 with SDCC's MCS-51
backend and drives the trainer's 128 × 64 LCD and four front-panel controls.

The [user manual](docs/user_manual.md) describes the menu and the settings.

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

## Sidetone hardware bring-up

The firmware plays a CW sidetone on board revision `CW_MINI_V2.0D`. In this
version, the only sound is the Tone preview in `Settings > CW > Tone` (see the
[user manual](docs/user_manual.md)). The normal build has no special sidetone
configuration.

The route is automatic. P1.0 detects the headphone plug. With the plug in
(P1.0 LOW), the headphone route P1.2 plays. Without the plug, the speaker route
P1.1 plays. A plug change takes effect at the next tone start. Both routes are
quiet at a HIGH level. The PWM pulses are LOW-active, with at most 50 % duty.

The sidetone is not tested on the device yet. Run the device checks in
[references/internal/sidetone_validation.md](references/internal/sidetone_validation.md)
and record the results there. For the first powered test, set the volume to
minimum. Check the PWM pin and the amplified output with a logic analyzer or an
oscilloscope before you increase the volume.

## Download firmware

Connect the trainer's programming interface, then run:

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
