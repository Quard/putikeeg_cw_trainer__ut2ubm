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

The normal build configures headphone detection but keeps both audio PWM
outputs disabled. Measurements on board revision `CW_MINI_V2.0D` found HIGH
quiet levels on P1.1 and P1.2, with P1.1 routed to the speaker and P1.2 routed
to headphones. The corrected P1.1 PWM1N polarity still requires a confirming
capture, so the validated profile remains gated.

After measuring the quiet level, configure one diagnostic route at a time.
The diagnostic profile limits the peak duty cycle to 5% and cannot enable the
other route. For example, if P1.1 was measured quiet at a high level:

```sh
cmake \
  -S firmware \
  -B firmware/build/diagnostic-speaker \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/firmware/cmake/sdcc-toolchain.cmake" \
  -DSIDETONE_BOARD_PROFILE=diagnostic-speaker \
  -DSIDETONE_SPEAKER_IDLE_LEVEL=HIGH
cmake --build firmware/build/diagnostic-speaker --parallel
```

Use `diagnostic-headphones` and `SIDETONE_HEADPHONES_IDLE_LEVEL` to test P1.2.
Accepted idle levels are `HIGH` and `LOW`. Keep the volume at minimum for the
first powered test and verify the PWM pin and amplified output with an
oscilloscope before increasing volume or duty cycle.

The `validated` profile restores the 50% maximum duty and automatic P1.0 jack
routing. It requires both measured idle levels and the explicit
`-DSIDETONE_BOARD_VALIDATED=ON` acknowledgement. Complete and record the
remaining checks in `references/internal/sidetone_validation.md` before using
that profile.

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
