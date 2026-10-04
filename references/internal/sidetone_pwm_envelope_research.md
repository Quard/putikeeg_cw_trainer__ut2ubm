# PWM sidetone and envelope implementation guide

Date: 2026-09-27

## Purpose

This document defines how to add a click-reduced CW sidetone to the Putikeeg
CW Trainer. It covers the firmware structure, STC8 PWM setup, envelope state
machine, keyer integration, and validation required before the feature is
considered complete.

The initial design uses:

- an audible-frequency, edge-aligned PWM carrier;
- a 50% maximum duty cycle;
- a 4 kHz envelope service tick;
- a nominal 8 ms rise and fall, represented by 32 intervals; and
- separate logical-key and audio-envelope signals.

These values are starting points. Final tone range and envelope duration must
be selected from measurements on the actual speaker and headphone paths.

## Hardware facts and required checks

The following connections are recorded for board revision `CW_MINI_V2.0D`:

- P1.1 connects to the fifth pin of the volume potentiometer and can output
  `PWM1N`.
- P1.2 connects to the fourth pin of the volume potentiometer and can output
  `PWM2P`.
- Separate SS8550 stages drive the speaker and headphones.

Measurements recorded on 2026-10-02 established:

- P1.1 is the speaker route and has a HIGH quiet level of approximately 4 V.
- P1.2 is the headphone route and has a HIGH quiet level of approximately 4 V.
- P1.0 goes LOW when headphones are inserted.
- Diagnostic captures show only the selected PWM route active.

The initial speaker capture exposed an inverted PWM1N active duty: P1.1 was
LOW for approximately 95% of the cycle in the nominal 5% diagnostic profile.
The polarity selection was corrected, but a confirming speaker capture is
still required before enabling the validated profile. Detailed evidence and
remaining checks are in [sidetone_validation.md](sidetone_validation.md).

The hardware validation procedure is:

1. Trace P1.1 and P1.2 through the potentiometer and amplifier stages.
2. Identify the speaker and headphone routes.
3. Measure the quiet level required at each amplifier input.
4. Confirm the active polarity of `PWM1N` and `PWM2P` at the amplifier inputs.
5. Confirm whether headphone insertion selects the route in hardware or
   requires firmware action.
6. Record the results in
   [components_and_wiring.md](components_and_wiring.md).

Do not enable a PWM pin until a zero-duty or disabled channel has been verified
to leave its amplifier input quiet. In particular, do not assume that the
negative `PWM1N` output has the same idle polarity as a positive output.

## Firmware layout

Keep register access separate from envelope policy. A suitable layout is:

```text
firmware/inc/audio/sidetone.h
firmware/src/audio/sidetone.c
firmware/inc/platform/sidetone_io.h
firmware/src/platform/sidetone_io_stc8h.c
firmware/tests/fakes/sidetone_io_fake.c
firmware/tests/test_sidetone.c
```

Responsibilities:

- `sidetone_io_stc8h.c` owns PWMA registers, pin routing, ordered 16-bit
  register writes, output enable/disable, and update-status handling.
- `sidetone.c` owns the requested tone, envelope state, duty table, and
  transition rules. It must not access MCU registers directly.
- The keyer or player publishes logical mark and sidetone requests. It must not
  write PWM state.
- Host tests replace the platform layer with a fake that records period,
  compare, route, and enable operations.

Only the sidetone platform module may write the selected PWMA period, compare,
pin-selection, and channel-enable registers.

## Public behavior contract

The sidetone module shall provide these behaviors, regardless of the final API
names:

- Initialize in a silent state before either audio output is enabled.
- Accept an on/off request and a requested tone frequency.
- Ramp up from the current envelope level when keyed.
- Ramp down from the current envelope level when released.
- Reverse direction without resetting the envelope if the request changes
  during a ramp.
- Apply a frequency change only after zero output has physically latched.
- Report whether an audible transition is still in progress so playback can
  wait for silence before replacing a tone.
- Keep logical CW mark timing independent of the audio envelope.

Use a single owner for mutable envelope state. If the 4 kHz timer interrupt
services the envelope, foreground code may publish requests but must not also
advance or rewrite the state machine.

## Step 1: establish the clock and tone range

The build currently defines `PLATFORM_CPU_HZ=24000000UL`, but the firmware does
not program the system clock. Confirm that the STC-ISP configuration and the
measured clock match this value before calculating PWM registers.

Start target evaluation with a provisional 300-1200 Hz range. This is not yet
an approved product limit. At an assumed 24 MHz PWMA clock, a fixed divide-by-2
prescaler keeps this range within the 16-bit auto-reload register:

```text
pwm_clock_hz      = system_clock_hz / prescaler_divisor
period_ticks      = round(pwm_clock_hz / tone_hz)
ARR               = period_ticks - 1
actual_tone_hz    = pwm_clock_hz / (ARR + 1)
frequency_error   = actual_tone_hz - requested_tone_hz
```

For a 24 MHz input and divisor 2:

| Requested tone | Period ticks | ARR |
|---:|---:|---:|
| 300 Hz | 40000 | 39999 |
| 700 Hz | approximately 17143 | approximately 17142 |
| 1200 Hz | 10000 | 9999 |

Use 32-bit intermediates for these calculations. Reject or clamp settings that
cannot be represented by the selected prescaler and 16-bit period; do not rely
on integer wrap.

## Step 2: configure PWMA safely

Consult the STC8H family reference manual sections 3.1.2, 20.3, 20.5, and
20.7 while implementing the platform layer.

1. Preserve the existing `P_SW2` value, set `P_SW2.EAXFR`, and access extended
   PWM registers only while extended-register access is enabled.
2. Configure the selected channel for edge-aligned PWM output.
3. Enable ARR and CCR preload so period and duty changes take effect only at an
   update boundary.
4. Configure the verified P1.1/P1.2 pin route and polarity.
5. Write the prescaler, period, and zero compare. STC 16-bit ARR and CCR
   registers must be written high byte first, then low byte.
6. Generate one software update event to transfer the coherent initial period
   and compare values.
7. Clear pending status flags, start the counter, and enable only the verified
   output route.
8. Restore the previous `P_SW2` value.

Do not generate a software update for every envelope step. It resets the PWM
counter and can turn the 4 kHz control tick into unwanted modulation. Write the
preloaded compare and let the normal carrier update transfer it.

## Step 3: implement the envelope

Use 33 duty values for 32 intervals. The initial table, in permille of the PWM
period, is:

```text
0, 1, 5, 11, 19, 30, 42, 57, 73, 91, 111,
132, 154, 177, 201, 225, 250, 275, 299, 323, 346, 368,
389, 409, 427, 443, 458, 470, 481, 489, 495, 499, 500
```

The table is generated by:

```text
duty_permille(k) = round(500 * (1 - cos(pi * k / 32)) / 2)
                   for k = 0..32
```

This is a raised-cosine duty curve, not an exact raised-cosine acoustic
amplitude. With a tone-frequency pulse train, the speaker, amplifier, volume
control, enclosure, and duty-dependent harmonic content all affect the heard
result.

When a new frequency is accepted at silence, precompute the 33 compare values:

```text
compare[k] = floor(period_ticks * duty_permille[k] / 1000)
```

Use a 32-bit intermediate. A 33-entry `uint16_t` table costs 66 bytes but keeps
division out of the timing-critical ISR. Check the final SDCC memory map and
generated assembly before choosing CODE, IDATA, or XDATA placement.

Use these states:

```text
OFF      output is confirmed at zero
RISING   envelope index moves toward 32
STEADY   envelope index is 32
FALLING  envelope index moves toward 0
```

On each 4 kHz service tick:

```text
OFF + on request:
  apply a pending frequency
  load compare[0]
  enable the verified route
  enter RISING

RISING + on request:
  increment the index up to 32
  write compare[index]
  enter STEADY at 32

RISING + off request:
  enter FALLING without changing the index first

STEADY + off request:
  enter FALLING

FALLING + on request at the applied frequency:
  enter RISING without resetting the index

FALLING + off request:
  decrement the index toward 0
  write compare[index]
  after compare[0] has latched, enter OFF and optionally disable the route
```

If a different frequency is requested while sounding, retain it as pending,
finish the fall, confirm zero, reprogram ARR and the compare table coherently,
and then rise at the new frequency. Never change ARR during an audible mark.

An 8-bit request flag is atomic on this MCU. A multi-byte frequency value can
tear if foreground and ISR access overlap; protect the request with a short
interrupt exclusion or a sequence-numbered mailbox.

## Step 4: integrate with CW timing

Maintain separate signals:

```text
logical_mark_active  full Morse mark; future RF/key output follows this
sidetone_target_on   audio-envelope request
```

For a predictable generated element, request the audio fall early enough for
the envelope to reach zero at the logical mark boundary. With a 4 kHz tick and
32 transitions, the nominal lead time is 8 ms. Define the producer/envelope
call order and write a boundary test that proves exactly how many service calls
occur before the boundary; avoid an unexplained `31` or `32` literal in the
keyer.

A straight-key release cannot be predicted. Start its audio fall after the
debounced release and accept the shaped tail. Logical event timestamps still
follow the contact, not the end of the audio tail.

The 4 kHz control rate does not create 32 physical duty changes in an 8 ms
ramp. Compare preload transfers only at carrier updates. An 8 ms ramp contains
about 2.4 periods at 300 Hz, 5.6 at 700 Hz, and 9.6 at 1200 Hz, so multiple
compare writes will coalesce. Keep preload enabled and tune the curve from the
physical output rather than assuming every software step appears at the pin.

Call the services in this order from the timer ISR:

```text
paddle_tick()
keyer_tick()
sidetone_tick()
```

This allows a newly confirmed contact to affect the keyer and audio request in
the same control tick.

## Host tests

Use a fake platform layer and deterministic tick advancement. Test:

1. initialization leaves the output disabled or at the verified quiet level;
2. all 33 duty values, monotonic rise, mirrored fall, and 0/50% endpoints;
3. exact 8 ms nominal rise and fall at a 4 kHz service rate;
4. reversal at every index without a jump to zero or full level;
5. stop, restart, and replacement while rising, steady, and falling;
6. frequency changes wait for confirmed zero;
7. generated dit/dah release scheduling preserves the logical mark and the
   complete one-dit inter-element gap;
8. straight-key release begins a fall only after the input release;
9. period and compare calculations at the minimum, default, and maximum tone;
10. invalid or unrepresentable frequency requests have defined behavior; and
11. request publication cannot expose a torn multi-byte value.

The fake verifies requested register operations. It does not prove when a
preloaded value reaches the physical pin.

## Target validation

Capture both the PWM pin and amplified audio with a logic analyzer or
oscilloscope. Test the minimum, default, and maximum tone and WPM settings on
both speaker and headphones.

Record:

- actual system and PWMA clocks;
- requested and measured tone frequency and error;
- output polarity, idle level, maximum duty, and DC behavior;
- the first and last physical pulses and distinct levels in each ramp;
- latency from request to first pulse and from release request to confirmed
  zero over different carrier phases;
- re-keying during every part of the fall;
- generated dit/dah duration and inter-element silence;
- behavior during tone changes, reset, and headphone insertion/removal; and
- worst-case 4 kHz ISR execution time and jitter while display work runs.

Compare the 8 ms curve with shorter and longer candidates, level matched.
Select the shortest curve that is click-free on both output paths without
materially weakening fast dits. Preserve the firmware revision, tone, WPM,
volume setting, output route, equipment, and measurements with the decision.

## Completion criteria

The sidetone is ready to integrate when:

- P1.1/P1.2 routing and quiet polarity are recorded from the board;
- the measured clock matches the build configuration;
- host tests cover every envelope transition and frequency-change path;
- the logical key signal remains independent of early audio release;
- target captures show bounded ISR timing and click-free operation; and
- reset, stop, replacement, and route changes always return to verified
  silence.

## Project references

- [Components, wiring, and chips](components_and_wiring.md)
- [Paddle input and keyer implementation guide](paddle_keyer_research.md)
- [Non-blocking display transfer research](soft_spi_nonblocking_research.md)
- [STC8H family reference manual](../context/STC8H_datasheet.md), especially
  sections 2.3.3, 3.1.2, 20.3, 20.5, and 20.7
