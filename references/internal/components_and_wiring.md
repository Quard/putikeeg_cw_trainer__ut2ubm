# Components, wiring, and chips

Board revision: **CW_MINI_V2.0D**.

## Components

| Type            | Name                     | Description                   |
|-----------------|--------------------------|-------------------------------|
| MCU             | STC8H3K64S2              | 8051-based MCU with a 1T core |
| LCD             | 128 × 64 COG LCD, 10-pin |                               |
| Battery charger | TP4056                   |                               |
| LCD backlight   | HX4004A                  |                               |
| Power           | Li-ion battery           |                               |

## Controls

The device has one `on/off` button and four control buttons: `menu/back`, `prev`, `next`, and `enter`. The buttons pull their signals low when pressed. No hardware pull-up resistors are fitted.

The `enter` button is also connected to the tip of the key/paddle TRS socket.

A 100 kΩ potentiometer controls the volume.

## Wiring

| MCU Pin | Description |
|---------|-------------|
| P0.0    | Button `next` |
| P0.1    | Button `prev` |
| P0.2    | Button `menu/back` |
| P0.3    | LCD backlight |
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
was corrected after this capture, so the speaker route requires a new capture
before the validated profile is approved. See
[sidetone_validation.md](sidetone_validation.md#measurement-results-2026-10-02).

## Memory

There is no dedicated memory chip on the board. The MCU has a configurable EEPROM section.

## Related references

- [STC8H family reference manual](../context/STC8H_datasheet.md)
- [Putikeeg display module](../context/putikeeg_display_module.md)
