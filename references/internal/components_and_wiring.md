# Components, wiring, and chips

Board revision: **CW_MINI_V2.0D**.

## Components

| Type            | Name                     | Description                   |
|-----------------|--------------------------|-------------------------------|
| MCU             | STC8H3K64S2              | 8051-based MCU with a 1T core |
| LCD             | 128 × 64 COG LCD, 10-pin |                               |
| Battery charger | TP4056                   |                               |
| LCD backlight   | HX4004A                  | Charge pump for the backlight supply. Always enabled: En connects to VCC through R13 1 kΩ. Output about 5 V (unverified). |
| Power           | Li-ion battery           |                               |

## Controls

The device has one `on/off` button and four control buttons: `menu/back`, `prev`, `next`, and `enter`. The buttons pull their signals low when pressed. No hardware pull-up resistors are fitted.

The `enter` button is also connected to the tip of the key/paddle TRS socket.

A 100 kΩ potentiometer controls the volume.

### Firmware use of the paddle pins

The keyer firmware uses the paddle socket pins as follows. Sources:
[keyer_spec.md](keyer_spec.md) sections 4, 5.3, 6.12 and 14, [ui-ux.md](ui-ux.md) sections 3.1 and 6.7,
`firmware/inc/board/paddle_pins.h`, `firmware/inc/board/button_pins.h`. The device behavior is unverified until
the device checks DC-K1 and DC-K2 of `keyer_spec.md` section 14 pass.

| Pin | Firmware setup | Owner | Device status |
| --- | --- | --- | --- |
| P3.2 | High-impedance input with the internal pull-up (`P3PU` bit 2). `buttons_init()` sets it up. Active low: a low pin is a closed contact. | Shared. In the menu, `buttons` reads it as `enter`. While Free Input is open, the keyer reads it as the paddle tip, and `buttons_set_enter_enabled(false)` keeps `enter` events off. At the mode end, `buttons_set_enter_enabled(true)` gives it back. A contact held at that moment gives no `enter` until it opens and closes again. | Tip polarity unverified (DC-K2). |
| P5.5 | High-impedance input (`P5M1` bit 5 = 1, `P5M0` bit 5 = 0) with the internal pull-up (`P5PU` bit 5, about 4.1 kΩ). `keyer_init()` sets it up with `PADDLE_PINS_CONFIGURE()`: the pull-up first, then the mode. Active low. | The keyer only. The menu does not read it. | Pull-up unverified (DC-K1). The repeated tables of the reference manual differ about the P5.5 pull-up. |

Rules:

- The keyer runs in the Timer 0 interrupt. It reads the P3.2 and P5.5 pin bits only. It never writes P3 or P5,
  their mode registers or `P_SW2`.
- `P5M1` and `P5M0` change only with `ORL` or `ANL` (`check_asm.sh` rule R5).
- With `Paddle` = `Normal`, the tip is the dit contact and the ring is the dah contact. `Reversed` swaps them. With
  `Key Type` = `Straight`, both contacts key.
- A mono plug shorts the ring to the sleeve. At the start of Free Input, the keyer ignores each closed contact
  until it opens. So a mono plug that is in the socket before the start works as a straight key on the tip.
- If the P5.5 pull-up does not work on the fitted chip, the fallback is an `architector` decision: the
  quasi-bidirectional mode with the latch at 1, or an external pull-up. The firmware has no code for it.

## Wiring

| MCU Pin | Description |
|---------|-------------|
| P0.0    | Button `next` |
| P0.1    | Button `prev` |
| P0.2    | Button `menu/back` |
| P0.3    | LCD backlight. Drives the Q6 (SS8050) base through R2 120 Ω. R10 47 kΩ goes to GND. HIGH = on. LOW or high-impedance = off. |
| P1.0    | Ring 2 of the headphone socket (TRRS); measured LOW when headphones are inserted |
| P1.1    | Fifth pin of the volume potentiometer; speaker sidetone route; measured quiet level HIGH (approximately 4 V) |
| P1.2    | Fourth pin of the volume potentiometer; headphone sidetone route; measured quiet level HIGH (approximately 4 V) |
| P2.0    | LCD |
| P2.1    | LCD |
| P2.2    | LCD |
| P2.3    | LCD |
| P2.4    | LCD |
| P3.0    | Programmer |
| P3.1    | Programmer |
| P3.2    | Button `enter` and tip of the paddle socket |
| P5.5    | Ring of the paddle socket |

## Display

**The exact model is unknown.** The display has a 10-pin connector. The current assumption is that it uses a software implementation of SPI, probably with an ST7567 controller.

Tentative pin mapping:

| MCU Pin | Display Pin | Display Pin Name |
|---------|-------------|------------------|
| P2.0    | 1           | CS1              |
| P2.1    | 2           | RST              |
| P2.2    | 3           | A0               |
| P2.3    | 4           | SCLK             |
| P2.4    | 5           | SDA              |

Based on the [referenced display module datasheet](https://www.display-lcd.com/data/upload/admin/202401/6593a0c5d936f.pdf), the power connections on pins 6 through 10 appear to match.

## Backlight

Source: the `kicad-cli` netlist of the corrected `hardware/putikeeg_cw_trainer.kicad_sch`. The schematic is reverse
engineered, and the export reports annotation errors. The user read the resistor markings on the board.

The circuit:

- P0.3 (U1 pin 32) connects through R2 (120 Ω, marking "121") to the base of Q6 (SS8050, NPN).
- R10 (47 kΩ, marking "473") connects P0.3 to GND.
- The emitter of Q6 connects to GND. The collector connects to J6 pin 2, the backlight cathode.
- U4 (HX4004A) is a charge pump. Pin 5 (Vin) connects to VCC. Pin 3 (En) connects to VCC through R13 (1 kΩ).
- U4 pin 1 (Vout) connects through R19 (50 Ω) to J6 pin 1, the backlight anode.

So Q6 is a low-side switch, and P0.3 drives its base. A HIGH pin turns the backlight on. A LOW or high-impedance
pin turns it off, because R10 pulls the base down.

The HX4004A is always enabled. On 2026-10-08, the backlight was lit on the device with En tied to VCC. The Vout
voltage, about 5 V, is unverified. The repository has no HX4004A datasheet.

Firmware use: PWMB channel 8 drives P0.3 with `CC8P` = 0, so the channel is active high. The pin is open drain
with the internal pull-up (`P0PU` bit 3), decision D-BL1. Level n gives n × 10 % on-time. Sources:
[menu_subsystem_spec.md](menu_subsystem_spec.md) section 4.6, `firmware/inc/board/backlight_config.h`,
`firmware/inc/board/backlight_pwm.h`.

Pin current: a HIGH pin is the internal pull-up (about 4.1 kΩ, 3 kΩ to 5 kΩ). It gives a base current of about
0.6 mA to 0.8 mA. A push-pull HIGH would source about 27 mA through R2. That is above the 20 mA limit for each pin
([STC8H family reference manual](../context/STC8H_datasheet.md) section 9.2). While the pin is LOW, the pull-up
draws about 1 mA. The reference manual does not state clearly that `PxPU` works in open-drain mode (sections 9.2
and 9.3.5). DC-BL1 tests it.

The behavior on the device is unverified. These device checks test it:

| Check | Procedure | Pass |
| --- | --- | --- |
| DC-BL1 | Set `Backlight` to Off, 1, 5 and 10. | Off is dark. 1 is dim, 5 is medium and 10 is the brightest. No level flickers. |
| DC-BL2 | Switch the device on with the default level 8. Then set another level, and switch the device on while you hold `menu/back` (display recovery start). | The backlight does not flash at full brightness at power-on. After the recovery start, the level is 8. |
| DC-BL3 (optional, multimeter) | Measure the P0.3 voltage at Off and at level 10. | P0.3 is about 0 V at Off. P0.3 is about 0.7 V to 0.9 V at level 10, because the Q6 base clamps the open-drain pin. |
| DC-BL4 | Change the level several times. Use the buttons and the menu during and after the changes. | The buttons work. The LCD shows the menu correctly while the level changes. |

If the brightness is inverted on the device, report the result to `architector`.

### Device record 2026-10-09: diagnostic build

The user ran a diagnostic build on the device. The build came from a temporary worktree and was never merged. It
has 12 phases. Each phase sets PWMB channel 8 and the P0.3 mode itself, and shows the registers on the LCD.

The register readback matched the written values in every phase: `PWMB_PS` 80, `PWMB_ENO` 40 (PWM phases) or 00
(GPIO phases), `PWMB_CR1` 81, `PWMB_BKR` 80, `PWMB_CCER2` 10, `PWMB_CCMR4` 68, `PWMB_IOAUX` 00, ARR 03E7, PSCR
0017, `P_SW2` 80, and CCR8 as set. The counter value changed between two reads, so the counter runs. Bit 3 of
`P0M0`, `P0M1`, `P0PU` and the P0.3 latch matched each phase. Example, phase 12 (push-pull, latch 0): `P0M0` 08,
`P0M1` F7, `P0PU` 07, `P0` 07.

The backlight was on in all 12 phases, also with a push-pull LOW and an open-drain LOW. The user measured these
P0.3 voltages to GND. The measurement point is not recorded.

| P0.3 state | Phases | Voltage |
| --- | --- | --- |
| Driven LOW, or PWM 0 % | 1, 2, 5, 8, 12 | 0.75 V to 0.77 V |
| PWM 50 %, or released | 3, 7, 9, 10, 11 | 0.82 V to 0.83 V |
| PWM 100 % | 4, 6 | 0.87 V |

The latch had no effect: phase 5 equals phase 1, and phase 6 equals phase 4.

Conclusion: the PWM reaches P0.3, and the register setup of the firmware works on the device. A source other than
P0.3 holds the Q6 base at about 0.8 V, so Q6 conducts at every level. The suspected cause on this unit is a
collector-base short in Q6, or a part with the wrong pinout. This is not confirmed. The user will replace Q6. Until
then, DC-BL1 to DC-BL4 cannot pass on this unit. It is still unknown if `P0PU` works in open drain. Phases 9 and
10 cannot show it while Q6 conducts.

## Audio output

The device has an internal speaker and a headphone jack. Inserting a headphone plug disables the internal speaker.

Two MCU pins connect to the fourth and fifth pins of the potentiometer. The
first and second potentiometer pins connect to the speaker, and the third
connects to the headphone jack. Diagnostic captures confirm that the MCU
generates PWM on P1.1 for the speaker route and P1.2 for the headphone route.

Audio amplification is provided by separate SS8550 transistor stages for the speaker and headphones.

Board measurements reported on 2026-10-02 establish the following electrical
facts. The measuring instrument was not recorded:

- P1.1 is approximately 4 V relative to ground when quiet. With power removed,
  it measures approximately 98 kΩ to VCC and 57 MΩ to ground.
- P1.2 is approximately 4 V relative to ground when quiet. With power removed,
  it measures approximately 93 kΩ to VCC and 75 MΩ to ground.
- P1.0 goes LOW when headphones are inserted, confirming active-low plug
  detection.

Diagnostic measurements and logic captures establish P1.1 as the speaker
sidetone route and P1.2 as the headphone sidetone route. The captures also
show that only the selected route was active. The initial speaker capture
found the PWM1N polarity inverted: despite a HIGH quiet state, P1.1 was LOW
for approximately 95% of each carrier cycle. The firmware polarity selection
was corrected after this capture. The normal firmware build uses both
routes, one at a time, with the corrected polarity, and a new speaker capture must check it
(DC-2). See
[sidetone_validation.md](sidetone_validation.md#measurement-results-2026-10-02).

### Firmware use of the audio pins

The sidetone firmware sets up the audio pins as follows. Sources:
[sidetone_spec.md](sidetone_spec.md) sections 4 and 7, `firmware/inc/board/sidetone_pwm.h`. The device
behavior is unverified until the checks in [sidetone_validation.md](sidetone_validation.md) pass.

| Pin | Firmware setup | Device status |
| --- | --- | --- |
| P1.1 | Push-pull output, latch HIGH, so the quiet level stays HIGH. PWMA channel 1 drives it as `PWM1N` with `CC1NE` = 1 and `CC1NP` = 1. `CC1E` stays 0: with `CC1E` = 1, the N output is the inverted signal (datasheet section 20.7.15). The pin is LOW while the counter is below the compare value, at most 50 % of the period. | Polarity unverified (DC-2). |
| P1.2 | Push-pull output, latch HIGH. PWMA channel 2 drives it as `PWM2P` with `CC2E` = 1 and `CC2P` = 1. LOW-active, at most 50 % of the period. | Polarity unverified (DC-2). |
| P1.0 | High-impedance input with the internal pull-up (`P1PU` bit 0, about 4.1 kΩ). A LOW level selects the headphones. `sidetone_poll()` debounces it for 50 ms (`SIDETONE_PLUG_DEBOUNCE_MS`). | The need for the pull-up and the 50 ms value are unverified (DC-7). |

Rules that keep the other pins safe:

- Only one route plays at a time. `PWMA_ENO` takes only the values 0x00 (off), 0x02 (`ENO1N`, speaker) and 0x04
  (`ENO2P`, headphones).
- `ENO1P` stays 0. P1.0 is also `PWM1P`. An enabled `PWM1P` drives the plug-detect line.
- `PWMA_PS` stays 0x00. The value 01 moves PWMA to P2.0 to P2.3, the LCD pins.
- P5.4, the other pin choice for `PWM2P`, is not connected on the board (U1 pin 9, from the netlist of
  `hardware/putikeeg_cw_trainer.kicad_sch`).
- The route changes only in silence. A plug change while a tone plays takes effect at the next tone start.

Netlist observation (from `sidetone_spec.md` section 4): the MCU `Vcc` pin (pin 10) is on net `VCC` with the
emitter of the headphone PNP transistor Q4. A push-pull HIGH is then equal to the emitter rail, and the PNP stage
is off. This explains the 4 V quiet level. The schematic export reports annotation errors, so this is not proven.
The quiet current check DC-10 tests it.

## Memory

There is no dedicated memory chip on the board. The MCU has a configurable EEPROM section.

## Related references

- [STC8H family reference manual](../context/STC8H_datasheet.md)
- [Putikeeg display module](../context/putikeeg_display_module.md)
