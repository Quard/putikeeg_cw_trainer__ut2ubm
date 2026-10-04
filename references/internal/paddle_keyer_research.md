# Paddle input and iambic keyer implementation guide

Date: 2026-09-27

## Purpose

This document defines how to build the Putikeeg paddle input, straight-key
handling, and Iambic A/B keyer. It specifies the electrical checks, module
boundaries, debounce algorithm, timer service, keyer states, audio integration,
and validation needed for reliable operation with mechanically noisy contacts.

The initial design uses:

- active-low tip and ring contacts, subject to board verification;
- independent 5-of-8 majority filters with hysteresis;
- a 4 kHz interrupt-driven control tick;
- tick-counted Morse timing; and
- separate logical mark and sidetone-envelope requests.

## Hardware facts and required checks

The board record for `CW_MINI_V2.0D` establishes:

- P3.2 connects to the paddle jack tip and the Enter button.
- P5.5 connects to the paddle jack ring.
- The front-panel buttons are active-low and have no fitted external pull-ups.

The paddle sleeve connection, P5.5 polarity, and user's dit/dah preference are
not independently established. Before enabling the keyer:

1. Confirm continuity from the jack sleeve to ground.
2. Confirm that shorting tip to sleeve pulls P3.2 low.
3. Confirm that shorting ring to sleeve pulls P5.5 low.
4. Measure that both open contacts idle high after GPIO initialization.
5. Measure the available internal pull-up on both pins.
6. Record the verified polarity, resistance/current, and default dit/dah mapping
   in [components_and_wiring.md](components_and_wiring.md).

P3.2 is shared with Enter. The application must assign that physical input to
exactly one function at a time:

- in menus, publish the debounced contact as Enter;
- in a keying screen, publish it as the tip paddle contact; and
- when ownership changes, clear keyer memory and reseed both debounce filters
  from the current electrical levels.

Never publish one P3.2 closure simultaneously as Enter and a paddle event.

## Firmware layout

Separate electrical input, filtering, and Morse behavior:

```text
firmware/inc/input/paddle.h
firmware/src/input/paddle.c
firmware/inc/cw/keyer.h
firmware/src/cw/keyer.c
firmware/inc/platform/paddle_io.h
firmware/src/platform/paddle_io_stc8h.c
firmware/tests/fakes/paddle_io_fake.c
firmware/tests/test_paddle.c
firmware/tests/test_keyer.c
```

Responsibilities:

- `paddle_io_stc8h.c` configures P3.2/P5.5 and returns raw electrical levels.
- `paddle.c` owns polarity, optional tip/ring swap, contact histories,
  hysteresis, boot masking, and input ownership.
- `keyer.c` consumes only stable logical `dit_closed` and `dah_closed` values.
  It must not know MCU pins or raw polarity.
- The keyer publishes a full-duration logical mark and a separate sidetone
  request.
- Host tests advance both modules with explicit ticks and fake inputs.

The timer ISR is the only writer of filter histories, keyer phase, countdowns,
element memory, and output requests. Foreground code changes settings through a
bounded handoff and reads events through an atomic snapshot or queue.

## Step 1: configure the paddle GPIO

Consult the STC8H family reference manual sections 3.1.2 and 9.1.2-9.1.4.

For the provisional active-low configuration:

```text
P3.2: P3M1.2 = 1, P3M0.2 = 0, P3PU.2 = 1
P5.5: P5M1.5 = 1, P5M0.5 = 0, P5PU.5 = 1
closed = (pin_level == 0)
```

Preserve unrelated port bits with read-modify-write operations. `P3PU` and
`P5PU` are extended registers, so preserve `P_SW2`, set `P_SW2.EAXFR` while
accessing them, and restore the previous value afterward. Leave the Schmitt
trigger enabled for each input.

The family manual is inconsistent about the P5.5 strong-pull-up bit in repeated
tables. Treat the P5.5 pull-up as a target-verification requirement. If the
fitted silicon cannot enable it, use a verified quasi-bidirectional input with
the port latch set high or add an external 4.7-10 kOhm pull-up in a hardware
revision. Do not leave P5.5 floating.

Keep the two raw reads adjacent in the ISR. They are on different ports and
cannot form a truly atomic snapshot, so later verify that the read skew and
complete ISR time are small relative to the 250 us tick.

## Step 2: implement independent contact filters

Use one state object per physical contact:

```c
typedef struct {
  uint8_t history;      /* Latest eight samples; 1 means electrically closed. */
  uint8_t closed_count; /* Number of set bits in history. */
  bool stable_closed;
  bool boot_masked;
} paddle_contact_t;
```

On every 4 kHz tick, update each contact independently:

1. Read the oldest bit before shifting it out.
2. Subtract that bit from `closed_count`.
3. Shift `history` and insert the newest normalized `closed` sample.
4. Add the new bit to `closed_count`.
5. Apply the hysteresis rule:

```text
closed_count >= 5  -> stable closed
closed_count <= 3  -> stable open
closed_count == 4  -> retain the previous stable state
```

This filters contact make and break symmetrically. From a fully settled
opposite state, five clean samples confirm an edge in 1.25 ms. One to three
opposite samples cannot reverse a settled state.

When the application changes between menu and keying ownership, reseed the
history from the current raw level instead of replaying an old partial window.
Clear keyer element memory during the same transition.

## Step 3: prevent keying from a held contact at boot

Initialize each eight-sample history to all zeros when its raw contact is open
or all ones when closed. If closed at initialization:

- set `boot_masked`;
- expose the logical contact as open;
- keep it masked while its voted state remains closed; and
- clear the mask only after the filter votes open.

Clearing the mask must not emit a synthetic edge. The next voted closure is a
normal input. Expose boot-masked status to diagnostics so a stuck paddle can be
distinguished from a silent keyer.

Initialize GPIO and both filters before enabling the timer interrupt.

## Step 4: configure the 4 kHz control tick

Use a dedicated hardware timer and derive its reload from the measured timer
clock and selected timer mode:

```text
tick_hz             = 4000
timer_period_counts = timer_clock_hz / (prescaler_divisor * tick_hz)
```

Check that the division is exact or calculate and record the resulting rate
error. Do not assume 1T timer operation or a 24 MHz clock merely because
`PLATFORM_CPU_HZ` is defined that way.

Initialization order:

1. Configure and verify P3.2/P5.5.
2. Load key type, WPM, and tip/ring swap settings.
3. Initialize both contact filters and boot masks.
4. Initialize the keyer to idle with empty memories and inactive outputs.
5. Initialize the sidetone module.
6. Configure the 4 kHz timer and clear its pending flag.
7. Enable the interrupt only after every tick consumer is ready.

Service order inside the ISR:

```text
paddle_tick()
keyer_tick()
sidetone_tick()
```

Give this timer higher priority than display or communication work. Keep all
three service functions bounded; no allocation, blocking, display transfer,
flash write, or unbounded loop is permitted in this ISR.

## Step 5: calculate Morse timing in ticks

At 4 kHz, calculate:

```text
dit_ticks               = round(4800 / WPM)
dah_ticks               = 3 * dit_ticks
intra_element_gap_ticks = dit_ticks
```

Use a checked, unsigned type wide enough for all supported WPM values. Reject
zero and out-of-range WPM before division. Latch WPM and key type when leaving
idle and retain them until the current character finishes so a settings change
cannot resize an active element.

Define countdown semantics once and cover them with tests. One useful rule is:
loading `N` means that the output remains in the current phase for exactly the
next `N` ISR executions; transition only after those `N` ticks have been
accounted for. This prevents boundary-dependent `N-1` behavior.

## Step 6: implement the keyer state machine

Use these phases:

```text
IDLE          no generated mark or gap is active
STRAIGHT_DOWN operator controls the full mark duration
ELEMENT       an automatically timed dit or dah is active
GAP           the one-dit inter-element gap is active
```

Track at least:

```text
phase
current_element       DIT or DAH
remaining_ticks
dit_memory
dah_memory
was_squeezed
latched_key_type
latched_dit_ticks
logical_mark_active
sidetone_target_on
```

### IDLE

- No contact: remain idle.
- Straight-key mode with either contact closed: enter `STRAIGHT_DOWN` and set
  `logical_mark_active`.
- Iambic mode with dit closed, including a simultaneous squeeze: start a dit.
- Iambic mode with only dah closed: start a dah.

Choosing dit first for a simultaneous idle squeeze is the project default.
Apply tip/ring swap below this logical interface.

### STRAIGHT_DOWN

- While either logical contact remains closed, keep the logical mark active.
- When both contacts open, clear the logical mark, request the sidetone fall,
  publish the recorded closure if needed, and return to `IDLE`.

The operator owns straight-key duration. Do not round it to a dit or dah and do
not predict its release.

### ELEMENT

- Assert `logical_mark_active` for exactly one dit or three dits.
- While the element runs, latch the contact opposite the current element into
  memory. OR `was_squeezed` with the current both-closed condition.
- For generated audio, request the sidetone fall early enough to reach silence
  at the logical element boundary. Do not clear `logical_mark_active` early.
- At the logical boundary, clear the mark and enter `GAP` for exactly one dit.

### GAP

- Keep `logical_mark_active` false.
- Latch either closed contact into its corresponding memory.
- Continue OR-ing `was_squeezed` with the both-closed condition.
- At the end of the gap, take one snapshot of current contacts plus memories,
  then clear both memory bits so each decision consumes one request snapshot.

Choose the next action at the end of the gap:

1. In Iambic A, if a squeeze was seen and both current contacts are open, go
   idle without a trailing element.
2. Otherwise, if the opposite of the previous element is requested, start it.
3. Otherwise, if the same element is requested, repeat it.
4. Otherwise, go idle.

For Iambic B, do not apply step 1. The remembered opposite request produces one
trailing opposite element after a released squeeze. Clear `was_squeezed` after
the gap-end decision has consumed it; a continuing squeeze will set it again
during the next element.

This decision point is deliberately at the end of the one-dit gap. A release
confirmed anywhere before the next-element decision is therefore interpreted
consistently.

## Step 7: keep logical keying separate from audio shaping

The keyer shall publish two different outputs:

```text
logical_mark_active  exact generated/straight mark timing
sidetone_target_on   request consumed by the audio envelope
```

For a generated dit or dah, `sidetone_target_on` may become false before
`logical_mark_active` so the shaped audio reaches zero at the mark boundary.
Any future RF or external key output must follow `logical_mark_active`, never
the early audio request.

For a straight key, both outputs become inactive only after the debounced
physical release; the sidetone then completes its fall. Event timestamps and
future RF output still follow `logical_mark_active`, not the end of the audio
tail.

See
[sidetone_pwm_envelope_research.md](sidetone_pwm_envelope_research.md) for the
audio state machine and target-measurement requirements.

## Step 8: hand events to foreground code safely

If the foreground needs decoded element or closure events, use one documented
handoff method:

- a short interrupt-protected snapshot;
- a sequence-numbered mailbox; or
- a fixed-size single-producer/single-consumer queue.

Define what happens on overflow. Do not silently overwrite an unread event.
Multi-byte timestamps and records are not atomic on the 8051 and must not be
read while the ISR can partially update them.

## Host tests

Drive raw inputs and time explicitly. Test:

1. clean make and break both confirm on sample five;
2. one, two, and three opposite samples do not reverse a settled state;
3. alternating input creates no stable edge;
4. tip and ring filter independently and swap mapping works;
5. contacts closed at boot cannot key, remain masked while held, recover after
   a voted release, and key normally on the next closure;
6. switching P3.2 ownership never publishes both Enter and dit and clears stale
   keyer memory;
7. one dit, one dah, held-paddle repeat, held-squeeze alternation, and an
   opposite-paddle tap during both `ELEMENT` and `GAP`;
8. Iambic A and B squeeze release early, at element boundaries, throughout the
   gap, and at the next-decision boundary;
9. for the same released squeeze, B produces A's sequence plus exactly one
   final opposite element;
10. exact 1:3 mark lengths and one-dit gaps across the supported WPM range;
11. settings changes take effect only after the current character;
12. tick/countdown behavior remains correct across unsigned counter wrap;
13. straight-key operation from either contact and both contacts; and
14. event handoff either preserves every record or reports defined overflow.

Also assert that early sidetone release never shortens
`logical_mark_active`.

## Target validation

1. With no plug and with an open paddle, verify P3.2 and P5.5 are high after
   GPIO initialization. Record pull-up current or resistance.
2. Boot with tip, ring, and both shorted to sleeve. Confirm that no tone or
   logical mark is produced, then release and confirm recovery without reset.
3. Export raw and filtered contact states to diagnostic pins or a bounded RAM
   trace. Exercise the actual paddle lightly, firmly, slowly, and at maximum
   intended WPM.
4. Confirm that short open glitches do not split a held contact and that real
   release latency remains acceptable.
5. Measure the 4 kHz ISR period, worst-case execution time, and jitter while
   display and other foreground work runs.
6. Capture filtered contacts, keyer phase, logical mark, sidetone request, and
   audio output together. Verify exact 1:3 timing, one-dit gaps, and Iambic A/B
   release behavior around every boundary.
7. If an RF/key output is added, verify that its mark follows the full logical
   duration and is never shortened by audio pre-release.
8. Repeat input checks over the supported supply and temperature range because
   internal pull-up value and mechanical contact behavior can vary.

Use the recorded contact trace and decoded element stream for acceptance.
Listening is an important usability check, but it cannot identify whether an
extra or missing element came from the contact, filter, state machine, or audio
path.

## Completion criteria

The paddle/keyer feature is ready to integrate when:

- jack polarity, sleeve connection, pull-ups, and logical mapping are recorded;
- the measured timer rate matches the timing calculation;
- host tests cover debounce, boot masking, every keyer transition, A/B release,
  timing boundaries, and event handoff;
- P3.2 ownership prevents simultaneous Enter and paddle events;
- target traces show bounded ISR latency during display activity; and
- logical mark timing remains independent of sidetone shaping.

## Project references

- [Components, wiring, and chips](components_and_wiring.md)
- [PWM sidetone and envelope implementation guide](sidetone_pwm_envelope_research.md)
- [Non-blocking display transfer research](soft_spi_nonblocking_research.md)
- [STC8H family reference manual](../context/STC8H_datasheet.md), especially
  sections 3.1.2 and 9.1.2-9.1.4
