# Sidetone device validation

Status: procedure ready. No check of this procedure has run on the device yet. Every result below is empty.

Audience: the person who tests the trainer on the bench, and the reviewers of the results (`qa-embedded`,
`ui-ux-embedded-designer`, `architector`).

Scope:

- Board revision `CW_MINI_V2.0D`.
- The CW sidetone with the Tone preview, as approved on 2026-10-05 (`sidetone_spec.md` section 22). Host tests
  check the logic. The checks here cover what host tests cannot prove: pins, timing, sound and current.
- Sidetone checks DC-1 to DC-12 (`sidetone_spec.md` section 17) and Tone preview checks D-10 to D-14
  (`ui-ux.md` section 11).

The user runs these checks on demand, for example after a problem appears. Until a check has a recorded result, its
behavior stays unverified.

## Sources

| Source | Used for |
| --- | --- |
| [sidetone_spec.md](sidetone_spec.md) | Check list (section 17), register values, envelope timing, expected numbers. |
| [ui-ux.md](ui-ux.md) | Tone preview behavior (section 6.5) and checks D-10 to D-14 (section 11). |
| [components_and_wiring.md](components_and_wiring.md) | Pins, routes, quiet levels, earlier measurements. |
| [sidetone_pwm_envelope_research.md](sidetone_pwm_envelope_research.md) | Earlier measurements, list of target measurements. |
| `firmware/src/sidetone/sidetone.c`, `firmware/inc/board/sidetone_pwm.h`, `firmware/inc/board/sidetone_config.h`, `firmware/src/menu/menu.c` | Implemented behavior. |

## Measurement results 2026-10-02

These results come from [components_and_wiring.md](components_and_wiring.md) (Audio output) and
[sidetone_pwm_envelope_research.md](sidetone_pwm_envelope_research.md) (Hardware facts and required checks). The
measuring instrument, the firmware revision and the capture files were not recorded. No capture file of
2026-10-02 is in the repository.

Board measurements:

| Item | Result |
| --- | --- |
| P1.1, power on, quiet | About 4 V to ground (HIGH). |
| P1.1, power removed | About 98 kΩ to VCC, about 57 MΩ to ground. |
| P1.2, power on, quiet | About 4 V to ground (HIGH). |
| P1.2, power removed | About 93 kΩ to VCC, about 75 MΩ to ground. |
| P1.0 | With headphones inserted, P1.0 goes LOW (active-low plug detect). |

Diagnostic measurements and logic captures:

| Item | Result |
| --- | --- |
| Speaker route | P1.1. |
| Headphone route | P1.2. |
| Route selection | Only the selected route was active. |
| Speaker polarity | The first speaker capture showed inverted `PWM1N` duty. With a nominal 5 % diagnostic duty, P1.1 was LOW for about 95 % of each carrier cycle, although its quiet level is HIGH. |
| Follow-up | The firmware polarity selection was corrected after this capture. A new speaker capture must check it (DC-2). |

The diagnostic firmware that made these captures is not in the current firmware tree.

The untracked files `cw_sidetone_headphones_high.sr`, `cw_sidetone_speaker_high.sr` (with `.pvs` views) and
`lcd_dump.sr` in the repository root are dated 2026-10-04. Their firmware revision is not recorded. This document
does not analyze them.

## Before you start

### Safety

- Set the volume potentiometer to its minimum before the first powered test. Increase it slowly.
- Do not put the headphones on your ears for the first headphone test. Listen with the headphones away from your
  ears until you know the level.
- The trainer runs from a Li-ion battery. Do not short the battery, the probe points or neighboring MCU pins.
- Connect the ground of each instrument to the board ground before you connect a signal probe.
- In the HIGH state, P1.1 and P1.2 are about 4 V. Make sure that the logic analyzer inputs accept 5 V.

### Equipment

| Item | Use |
| --- | --- |
| Logic analyzer, at least 3 channels, 8 MHz sample rate or more | P1.1, P1.2, P1.0. More channels for DC-3 and DC-8. The shortest ramp pulse is about 1 µs (compare value 12 counts of the 12 MHz PWM counter at 1000 Hz). |
| Oscilloscope, or an analog input of the analyzer | The amplified audio of the selected route, to see clicks. |
| Multimeter | Pin voltages (DC-1, DC-7, DC-10). |
| Ammeter in series with the supply | Quiet current (DC-10). |
| Headphones with a plug that fits the headphone socket | Headphone route, plug detect. |
| Programmer and `firmware/flash.sh` | Firmware download. |

The board documents do not name probe points for the amplified audio. Use the speaker terminals for the speaker
route and the signal contact of the headphone socket for the headphone route. Record the point that you use.

### Logic analyzer channels

| Channel | Signal | Used in |
| --- | --- | --- |
| 0 | P1.1, speaker PWM (`PWM1N`) | all checks |
| 1 | P1.2, headphone PWM (`PWM2P`) | all checks |
| 2 | P1.0, plug detect | DC-3, DC-7, D-14 |
| 3 | P1.3 | DC-3 |
| 4 | P5.4 (MCU pin 9, not connected on the board) | DC-3 |
| 5-9 | P2.0 CS1, P2.1 RST, P2.2 A0, P2.3 SCLK, P2.4 SDA (LCD) | DC-3, DC-8 |
| analog | Amplified audio | DC-6, D-10 to D-14 |

With fewer channels, do DC-3 in two passes.

### Session record

Fill in this table once per test session.

| Field | Value |
| --- | --- |
| Date | |
| Tester | |
| Firmware commit (`git rev-parse --short HEAD`) | |
| Uncommitted changes in `firmware/` (`git status --short firmware`) | |
| Flash size from `firmware/build/putikeeg_cw_trainer.mem` (15913 B for the approved tree) | |
| `MCU_CLOCK_HZ` from `firmware/build/CMakeCache.txt` (24000000 by default) | |
| Board revision | `CW_MINI_V2.0D` |
| Logic analyzer model and sample rate | |
| Oscilloscope model | |
| Audio probe points | |
| Battery voltage at the start | |

Each check also records its tone, volume setting and route. Write the volume as a position of the potentiometer,
for example "minimum", "middle" or "maximum".

### How to play a tone

The Tone preview is the only sound of this firmware (`ui-ux.md` section 6.5):

1. Open `Settings > CW`. Move the cursor to `Tone`.
2. Press `enter`. The tone plays for 300 ms at the shown value, then stops.
3. In the Tone edit, `next` and `prev` change the value in 10 Hz steps (400 to 1000 Hz, 600 Hz on an empty EEPROM).
   Each change plays the new tone for 300 ms. A hold repeats every 80 ms.
4. `enter` or `menu/back` ends the edit and stops the tone at once.

To reach 400 Hz or 1000 Hz, hold `prev` or `next` until the value stops at the limit.

The route is automatic. With the headphone plug in (P1.0 LOW), P1.2 plays. Without the plug, P1.1 plays. A plug
change takes effect at the next start from silence.

## Sidetone checks

Expected numbers come from `sidetone_spec.md` sections 6.1, 6.3, 7 and 8:

| Tone | Carrier period | LOW time at full level (50 %) |
| ---: | ---: | ---: |
| 400 Hz | 2.500 ms | 1.250 ms |
| 600 Hz | 1.667 ms | 0.833 ms |
| 1000 Hz | 1.000 ms | 0.500 ms |

The envelope service runs at 4 kHz (250 µs per call). A full rise and a full fall each take 32 calls, 8.0 ms.

### DC-1 Quiet at power-on, after init and in silence

Setup: channels 0 and 1. Trigger on a falling edge of either channel. Volume at the middle. Do the check without
the plug, then with the plug.

Steps:

1. Arm the analyzer. Switch the trainer on.
2. Wait 5 s in the main menu. Do not open the Tone edit.
3. Measure P1.1 and P1.2 with the multimeter.
4. Listen to the selected output during power-on.

Expected: no LOW pulse on P1.1 or P1.2 after the supply is up. Both pins stay HIGH (about 4 V). No click at
power-on.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-2 Output polarity

Setup: channels 0 and 1. Tone 600 Hz. Volume at the middle. Speaker first (no plug), then headphones.

Steps:

1. Open the Tone edit to play a 300 ms tone.
2. In the middle of the tone, measure the LOW time and the period of the pulses on the active pin.
3. Measure the level of the pin before and after the tone.

Expected: LOW-active pulses. In the steady part of the tone, the pin is LOW for at most 50 % of the period.
At 600 Hz, that is 0.833 ms of 1.667 ms. The pin is HIGH before and after the tone. Only the pin of the selected route
has pulses. This check tests the polarity reading of `sidetone_spec.md` section 4.1. A pin that is LOW for most
of the period means inverted polarity: stop the tests and report it to `architector`.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-3 No signal on other pins

Setup: channels 2 to 9 (P1.0, P1.3, P5.4, P2.0 to P2.4). Tone 600 Hz.

Steps:

1. In the Tone edit, hold `next` for about 2 s, so that tones play while the screen redraws.
2. Repeat with the plug in.

Expected: no carrier-rate pulses on P1.0, P1.3 or P5.4. P1.0 changes only with the plug. Port 2 shows only LCD
transfers. The display shows the correct values while the tone plays.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-4 Tone frequency

Setup: channel 0 or 1 for the active route. Speaker route is enough.

Steps:

1. Play a tone at 400 Hz, 600 Hz and 1000 Hz.
2. Measure the period in the steady part of each tone. Use the average over at least 10 periods.

Expected: the periods of the table at the start of [Sidetone checks](#sidetone-checks). The only error source
is the trim of the internal oscillator. Record the measured frequency and the error in percent for each tone.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-5 Ramp shape

Setup: channel 0 or 1 for the active route, 8 MHz sample rate or more. Tones 400 Hz and 1000 Hz.

Steps:

1. Play one tone. Capture the whole 300 ms tone.
2. Measure the time from the first LOW pulse to the first pulse at full width (rise).
3. Measure the time from the last full-width pulse to the last LOW pulse (fall).
4. Count the different pulse widths in each ramp.
5. Measure the time from the last LOW pulse to the end of the carrier activity.

Expected:

- Rise and fall each about 8 ms. The pulse width grows and shrinks step by step. No pulse is wider than the
  full-level LOW time.
- The PWM loads a new compare value only once per carrier period, so several envelope steps merge. At 400 Hz an
  8 ms ramp spans about 3.2 periods: expect only 3 or 4 different widths. At 1000 Hz expect about 8.
- After the fall reaches zero, the firmware waits for one carrier update and then disables the output. The wait
  is at most 12 service calls (3 ms). The pin stays HIGH from the last LOW pulse on.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-6 No clicks

Setup: oscilloscope on the amplified audio of the active route, channels 0 and 1. Volume at the middle. Speaker,
then headphones.

Steps:

1. Start: open the Tone edit.
2. Stop: let the tone time out. Then open it again and leave at once with `enter`.
3. Retune: in the Tone edit, press `next` while a tone plays (within 300 ms of the last change).
4. Route change: with no tone playing, insert the plug, wait 1 s, play a tone. Remove the plug, wait 1 s, play a
   tone.
5. Listen at each step.

Expected: no audible click and no spike on the audio trace at the start, the stop, the retune dip or after a route
change. A retune is a fall, a silence of at most 3 ms and a rise (about 17-19 ms in total).

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-7 Plug detect and route change

Setup: channels 0, 1 and 2. Multimeter on P1.0.

Steps:

1. With the plug out, measure P1.0. Insert the plug and measure P1.0 again.
2. Capture P1.0 while you insert and remove the plug five times. Measure the longest bounce train.
3. Insert the plug, wait at least 100 ms, play a tone. Note the active pin.
4. Remove the plug, wait at least 100 ms, play a tone. Note the active pin.
5. Optional, for the pull-up question: flash commit `2403854`, which does not write `P1PU` (reset value 0, so the
   internal pull-up is off, datasheet section 9.1). Measure P1.0 with the plug out. Then flash the sidetone
   firmware again.

Expected:

- P1.0 is HIGH with the plug out and LOW with the plug in. The firmware turns on the internal pull-up on P1.0
  (about 4.1 kΩ).
- Each bounce train is shorter than the 50 ms debounce (`SIDETONE_PLUG_DEBOUNCE_MS`, a value not yet checked on
  the device).
- With the plug in, P1.2 plays. With the plug out, P1.1 plays.
- Step 5: a stable HIGH without the internal pull-up means the board pulls P1.0 up by itself. A floating level
  means the pull-up is needed. Record the result for `architector`.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-8 Timer 0 ISR time and LCD redraw time

Setup: channels 5 to 9 (LCD), channel 0.

Steps:

1. Change from one menu screen to another. Measure the time from the first to the last SCLK (P2.3) edge of the
   redraw.
2. Repeat during a held `next` in the Tone edit, so that the 4 kHz interrupt and the tone run at the same time.
3. Optional: repeat step 1 with commit `2403854` (no Timer 0 interrupt) to compare.

Expected: the redraw completes, and the display is correct. Record the redraw times.

The ISR time and jitter need a measurement build that marks the ISR on a free pin. The firmware has no such build.
The host estimate (`sidetone_spec.md` AC-22) has a target of 400 clocks and a hard limit of 800 clocks. Ask
`architector` before you change the firmware for this measurement. Until then, record this part as "not run".

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-9 Stack high-water mark

This check needs a measurement build. It fills the free stack with a pattern at start-up and reads it after a long
run. The firmware has no such build. Ask `architector` before you make one.

The build report of the approved tree gives the stack start 0x30 with 208 bytes free
(`firmware/build/putikeeg_cw_trainer.mem`). The required margin is at least 64 bytes
(`sidetone_spec.md` section 14).

Expected: the used stack after a long run, with the menu, held repeats and tone previews, leaves at least 64
bytes free.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-10 Quiet current

Setup: ammeter in series with the supply. Same backlight level and the same screen for both measurements.

Steps:

1. Flash commit `2403854`, which leaves P1.1 and P1.2 as inputs. Measure the current in the main menu.
2. Flash the sidetone firmware. Measure the current in the main menu, with no tone playing.
3. Measure P1.1 and P1.2 with the multimeter.

Expected: no extra current from P1.1 and P1.2 in push-pull HIGH. The difference between steps 1 and 2 is within
the resolution of the ammeter. P1.1 and P1.2 are HIGH. A higher current can mean that the MCU supply is not the
emitter rail of the PNP stages (`sidetone_spec.md` section 18). Report it to `architector`.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-11 Preview feel

Run D-10 to D-14 in [Tone preview checks](#tone-preview-checks). If all of them pass, DC-11 passes.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### DC-12 EEPROM erase during a ramp

An EEPROM erase stops the CPU for 4-6 ms. The PWM hardware keeps running with the last compare value.

This firmware gives no user path to this case. The EEPROM erase comes only from a save. A save needs two
conditions: you leave the `Settings` screen, and a value differs from the saved value
(`firmware/src/menu/menu_tree.c`, `menu_go_back()`, `settings_save_if_changed()`). From the Tone edit, that takes
three button events: `enter` (the fall starts), `menu/back` to `Settings`, and `menu/back` again. The fall lasts
8 ms. Each press and each release needs 20 ms of debounce (`BUTTONS_DEBOUNCE_MS`). So the erase always comes after
the fall.

The check needs the later keyer, or a measurement build that starts a ramp and an erase together. Ask
`architector` before you make one. Until then, record this check as "not run".

Setup for a measurement build: channels 0 and 1, oscilloscope on the amplified audio. Speaker route.

Expected: the erase only stretches the ramp. The pulse width stays constant for 4-6 ms during the erase. Then the
ramp continues without a click.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

## Tone preview checks

These checks come from `ui-ux.md` section 11. The preview lasts 300 ms (`MENU_TONE_PREVIEW_MS`, decision UX-21).

### D-10 Preview at the edit start

Setup: volume at the middle. Speaker, then headphones. Tones 400, 600 and 1000 Hz.

Steps:

1. Set the tone. Leave the edit.
2. Press `enter` on `Tone`. Listen.

Expected: the tone starts with no audible delay after `enter`. It lasts about 0.3 s. No click at the start or at
the end. You can judge the pitch.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### D-11 Single presses

Setup: tone 600 Hz, volume at the middle.

Steps:

1. In the Tone edit, press `next` about every 0.5 s, five times.
2. Press `prev` about every 0.5 s, five times.

Expected: each press gives one clear beep at the new pitch.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### D-12 Held repeat

Setup: tone 400 Hz, volume at the middle.

Steps:

1. In the Tone edit, hold `next` until the value stops at 1000 Hz. Keep holding for 1 s.
2. Hold `prev` until the value stops at 400 Hz.
3. Hold `next` again and release near a target pitch, for example 700 Hz.

Expected: the chain sounds as a series of pitch steps, not as clicks or a buzz. You can release near a target
pitch. The tone ends about 0.3 s after the release. At a limit it also ends about 0.3 s after the last change while
you still hold the button. If the chain is unpleasant, record it for `architector` (`ui-ux.md` UX-22
alternatives).

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### D-13 Stop timing

Setup: channels 0 and 1. If the analyzer has a free channel, connect it to the button that you press. Hold a repeat or
change screens so that the screen redraws.

Steps:

1. Open the Tone edit and leave it with `enter` while the tone plays.
2. Open the Tone edit and leave it with `menu/back` while the tone plays.
3. Open the Tone edit and let the preview time out.
4. For each step, measure the time from the event to the last LOW pulse.

Expected: after a leave, the output is quiet within about 10 ms of the event. A timeout ends 300 ms after the
start, plus at most one main-loop pass, plus the 8 ms fall.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

### D-14 Plug change during a preview

Setup: channels 0, 1 and 2, oscilloscope on the amplified audio.

Steps:

1. Start a single preview. Insert the plug while the tone plays.
2. Start a single preview. Remove the plug while the tone plays.
3. Hold `next` in the Tone edit. Insert and remove the plug during the hold.

Expected: the route changes at the next silence. The current tone continues on the old route. No click on either
output. A spurious `enter` from the socket (`ui-ux.md` D-5) ends the edit and stops the tone.

| Result (pass / fail / not run) | Tone, volume, route | Capture file | Notes |
| --- | --- | --- | --- |
| | | | |

## Reporting

- Save each capture with a name that contains the check ID, for example `DC-2_speaker_600Hz.sr`. Write the file
  name in the result table.
- A failed check is a defect report. Send the check ID, the session record and the capture to `architector`.
- A passed check changes the related "unverified" note in `sidetone_spec.md`, `ui-ux.md` or
  `components_and_wiring.md`. Only the owner of that document changes it.
