# CW sidetone with Tone preview: implementation specification

Owner: `architector`. Status: approved for implementation. Change request D-RM1 to D-RM15 (ramp duration setting,
plan `noble-bubbling-dahl.md`, user-approved 2026-10-16): specification published on 2026-10-16, section 24. R0-R6 and Q-R done, final review `approved` on 2026-10-16 (section 24.12). W-R ready to start. Where
section 24 and the older text differ, section 24 wins. Readers: `c-embedded-developer`, `qa-embedded`,
`ui-ux-embedded-designer`, `technical-writer`.

## 1. Sources and precedence

| Source | Role |
| --- | --- |
| This document | Interfaces, memory placement, behavior, register values, task split and acceptance criteria. It refines the approved plan and does not change a user decision. |
| Plan `curious-snacking-goblet.md` (user-approved) | Scope, user decisions, team steps. |
| Plan `noble-bubbling-dahl.md` (user-approved 2026-10-16) | Ramp duration setting: range, default, envelope method, menu scroll, Ramp preview (section 24). |
| `references/internal/sidetone_pwm_envelope_research.md` | Duty table, envelope idea, host-test list. Where this document differs, this document wins (section 3). |
| `references/internal/ui-ux.md` (owner `ui-ux-embedded-designer`) | Tone preview interaction (Q-3, O-3), the preview timing value and the new V-tests. |
| `references/internal/components_and_wiring.md` | Audio pins, quiet levels, plug detect. |
| `references/context/STC8H_datasheet.md` | Register facts. Section 4 lists the facts with their datasheet sections. |
| `AGENTS.md` | Project rules: memory, software SPI, interrupts, team workflow. |

Precedence rules:

- For the preview interaction (when the tone starts, restarts and stops in the menu) and its timing, `ui-ux.md`
  wins. Section 10 gives the hooks and a proposed default.
- For the sidetone module (API, envelope, mailbox, route, registers, memory), this document wins.
- When `ui-ux.md` asks for a sidetone behavior that the API in section 5 cannot give, stop and report it to
  `architector`. Do not change the API yourself.
- After implementation starts, only `architector` changes this document.

`AGENTS.md` names `llm.md` for agent-maintained instructions. `llm.md` at the repository root holds the internal RAM placement rule and the ISR rules of this change.

## 2. Scope

User decisions (fixed):

- The only trigger in this change is the Tone preview in the menu (`ui-ux.md` Q-3). The later keyer uses the
  same module.
- The route is automatic. P1.0 LOW selects the headphones (P1.2). Otherwise the speaker (P1.1) plays. A plug
  change takes effect only at silence.
- "Done" means: host tests pass, QA approves, `architector` approves. Device behavior stays listed as
  unverified (section 17). The user runs device tests on demand.

In scope:

- `sidetone` module: tone math, double-buffered tone mailbox, envelope state machine, plug debounce, route
  latch.
- PWMA setup on P1.1 (`PWM1N`) and P1.2 (`PWM2P`), Timer 0 at 4 kHz and its ISR.
- Menu hooks for the Tone preview, `menu_poll()`, `main.c` integration.
- `check_asm.sh` R5 extension for the new shared registers.
- Host fakes, host tests, memory and stack report.

Out of scope:

- Keyer, paddle input, CW playback, any sound outside the Tone preview.
- A volume control in firmware.
- Any change in `firmware/drivers/`.

## 3. Plan refinements found during specification

These changes make the approved plan safer or testable. None changes a user decision.

### 3.1 Double-buffered tone tables (replaces the ISR copy)

The plan has the ISR copy 33 compare values and the ARR from a pending table to the active table. That copy is
about 70 `MOVX` reads and writes, roughly 1000 CPU clocks in one ISR call. The later keyer shares this ISR, so a
long call adds jitter to the paddle sampling.

Decision: two tables, A and B, with their own ARR values. A bit selects the active table. The main loop always
writes the inactive table. The ISR only flips the selector bit. The xdata cost stays the same (2 x 66 B + 2 x 2 B).
Section 6.4 gives the protocol.

### 3.2 One envelope move on every service call, at the set ramp speed (revised by D-RM1)

The first plan said that an off request in RISING or STEADY enters FALLING with the index unchanged. That call then
writes nothing, so a full fall takes 33 calls and a full rise 32 calls.

Decision: each service call in RISING, STEADY and FALLING moves the envelope position one increment toward the
target that the current requests select (section 6.3). The position counts in 1/64 of a table step. The increment
comes from the ramp setting `N` (4 to 12 ms, section 6.8). A full rise and a full fall both take exactly
`4 * N` calls: 16 to 48 calls, 4 to 12 ms at 4 kHz. At 8 ms the increment is one table step per call, as in the
first design. A reversal never resets the position and never loses a call. The keyer and the CW player use the
same call count, `sidetone_ramp_calls`, as their lead time (section 5.7).

### 3.3 Write order at index 0

At index 0 the ISR writes the compare value 0 first and clears the PWMA update flag after it. The other order
is wrong. An update event can come between the flag clear and the compare write. That event sets the flag while
the shadow register still holds the old, non-zero compare. The ISR then disables the output during a pulse.

### 3.4 ZERO_WAIT limit

A fault can stop the PWMA update flag, for example a cleared EAXFR. Then ZERO_WAIT stays forever, and the tone
never plays again. Decision: ZERO_WAIT ends after `SIDETONE_ZERO_WAIT_LIMIT` (12) calls also without the flag.
The envelope position counts these calls, because it is 0 in ZERO_WAIT. No extra RAM.

### 3.5 Timer 0 setup moves into the board header

The plan puts the Timer 0 setup in a target-only `sidetone_timer0.c`. Decision: Timer 0 setup is the macro
`SIDETONE_TIMER_START()` in `board/sidetone_pwm.h`, so that a host test can prove that the timer starts last in
`sidetone_init()`. The ISR is at the end of `sidetone.c`, for the target only (section 5.6, D-R2-1).

### 3.6 Route flags are bits

The active route is a `__bit`, not a `__data` byte. The ISR tests it with one `JB`. Internal RAM drops by one byte.

### 3.7 Test seam for the mailbox tear test

`SIDETONE_PREEMPTION_POINT()` is an empty macro on the target. The host fake runs a test hook at each point. The
main-loop mailbox writer calls it after each store. This lets QA run `sidetone_service()` "inside" the writer, as
the Timer 0 interrupt can do.

### 3.8 Timers use `tick_now()` only

The plan says that the menu timer uses `tick_has_elapsed()`. The host fake `fake_tick.c` gives only `tick_now()`,
and the menu group links that fake. Decision: the preview timer and the plug debounce call `tick_now()` and use
`(uint16_t)(now - start)`, as `buttons.c` does.

### 3.9 A repeated tone request does nothing

`sidetone_set_tone_hz()` with the same clamped value as the last request returns at once. Without this rule, a
repeated request during a tone starts a needless fall and rise.

### 3.10 `sidetone_set_on()` is main-loop only in this change

A normal SDCC function that both the main loop and an ISR call is not safe: the ISR call can overwrite the
parameter of the interrupted main-loop call. The keyer change will define its own ISR-side entry. Until then all
public functions except `sidetone_service()` are main-loop only.

### 3.11 Busy includes the on request

`sidetone_is_busy()` is true while the on request is set or the state is not OFF. Without the first part, the
call right after `sidetone_set_on(true)` returns false before the ISR starts the rise.

### 3.12 New public functions

- `sidetone_state()` returns the envelope state. QA and the later keyer need it. Cost: a few bytes of flash.
- `menu_poll()` runs the preview timer from the main loop.

### 3.13 `check_asm.sh` R5 extension

`P1`, `P1M0`, `P1M1`, `TMOD` and `AUXR` get the R5 rule (only `ORL` or `ANL`). Reasons: `TMOD` holds Timer 1 in the
high nibble, and mode 3 of Timer 0 is a non-maskable interrupt (datasheet 13.2.2). `AUXR` bit 1 `EXTRAM` = 1
switches off the internal xdata RAM (7.2.4). Port 1 holds the audio pins and the plug input.

## 4. Hardware facts

Each fact has a datasheet section or a board source. The developer cites them again in the step report. When a
fact is different, stop and report it to `architector`.

| Fact | Value | Source |
| --- | --- | --- |
| PWMA pin select | `PWMA_PS` XFR 0xFEB2. C1PS[1:0] bits 1:0, C2PS[1:0] bits 3:2. 00: PWM1P P1.0, PWM1N P1.1, PWM2P P1.2 (or P5.4), PWM2N P1.3. 01 puts PWM1/PWM2 on P2.0-P2.3 (LCD pins). | 3.1.5 |
| P5.4 | Unconnected on the board (U1 pin 9). The "P1.2/P5.4" entry for PWM2P has no effect here. | netlist from `hardware/putikeeg_cw_trainer.kicad_sch` |
| Output enable | `PWMA_ENO` XFR 0xFEB1: bit 0 ENO1P, bit 1 ENO1N, bit 2 ENO2P, bit 3 ENO2N. A pin with its ENO bit 0 is a GPIO. | 20.7.1 |
| Extra output control | `PWMA_IOAUX` XFR 0xFEB3, reset 0: ENO alone controls each pin. | 20.7.2 |
| 16-bit writes | ARR and CCRi: high byte first, then low byte. The high-byte write locks the shadow update until the low byte. Each register has its own lock, so PWMB writes in the main loop do not affect PWMA writes in the ISR. | 20.3.2, 20.5.1 |
| Preload | ARPE = 1 buffers ARR. OCiPE = 1 buffers CCRi. Both move to the shadow registers at an update event (overflow or UG). | 20.3, 20.7.11 |
| PWM mode 1 | Up-counting: OCiREF high while CNT < CCRi. CCRi = 0 keeps OCiREF low. Period = ARR + 1 counts. | 20.5.7, 20.7.11 |
| Update flag | `PWMA_SR1` XFR 0xFEC5 bit 0 UIF. Hardware sets it at an update event. With URS = 0, UG also sets it. Software clears it. | 20.7.8 |
| Prescaler | f_CNT = f_SYS / (PSCR + 1). PSCR loads at the next update event. | 20.3.3, 20.7.19 |
| Brake register | `PWMA_BKR` XFR 0xFEDD: MOE bit 7, OSSR bit 3, OSSI bit 2, LOCK bits 1:0. Lockable bits must be correct at the first write. | 20.7.32 |
| CCMR channel select | CCiS is writable only while CCiE = 0. So CCMR is written while `PWMA_CCER1` = 0. | 20.7.11 |
| Port mode | `P1M1` 0x91, `P1M0` 0x92. M1 = 0, M0 = 1: push-pull. M1 = 1, M0 = 0: high-impedance input. Reset: P1M1 = 0xFF, P1M0 = 0x00 (all inputs). | 9.1.2, SFR table |
| Pull-up | `P1PU` XFR 0xFE11, bit 0 for P1.0, about 4.1 kOhm. | 9.1.3, 9.3.5 |
| Digital input | `P1IE` XFR 0xFE31, reset 0xFF (inputs enabled). No write needed. | 9.1.7 |
| Timer 0 | `TMOD` 0x89 low nibble: GATE, C/T, M1, M0. M = 00: 16-bit auto-reload. M = 11: non-maskable interrupt, never use it. TL0 0x8A, TH0 0x8C: a write while TR0 = 0 also sets the reload. `TCON` TR0 bit 4, TF0 bit 5. Hardware clears TF0 at the vector. | 13.2.1-13.2.3 |
| Timer 0 clock | `AUXR` 0x8E bit 7 T0x12: 0 = SYSclk / 12. Bit 1 `EXTRAM` must stay 0. Reset value 0x01. | 7.2.4, 13.2.12, SFR table |
| Timer 0 interrupt | Vector 0x000B, number 1, enable `ET0` = IE bit 1, priority bits PT0 / PT0H (reset 0). Timer 3 has the fixed level 0. Same level: no nesting. When both are pending, Timer 0 goes first (order 1 before 19). | 11.3, 11.4.1 |
| XFR access | `P_SW2` 0xBA bit 7 EAXFR = 1 for any XFR access. It stays set after init (`button_pins.h`, `backlight_pwm.h`). | 3.1.2 |
| Audio routes | P1.1 speaker, P1.2 headphones, both quiet HIGH (about 4 V). P1.0 LOW with headphones inserted. | `components_and_wiring.md` |
| MCU supply | The netlist shows U1 `Vcc` (pin 10) on net `VCC` with the emitter of the headphone PNP Q4. A push-pull HIGH therefore equals the PNP emitter rail, and the PNP is off. This explains the 4 V quiet level. | netlist (export reports annotation errors) |

### 4.1 Output polarity: what the datasheet says

The table "Control bits of complementary output channels OCi and OCiN with brake function" (section 20.7.15) is
partly garbled in the scan. Its rows match the STM8-style table. With MOE = 1 and OSSR = 0:

| CCiE | CCiNE | OCi (P pin) | OCiN (N pin) |
| --- | --- | --- | --- |
| 0 | 0 | output prohibited | output prohibited |
| 0 | 1 | output prohibited | OCiREF with polarity (not inverted) |
| 1 | 0 | OCiREF with polarity | output prohibited |
| 1 | 1 | OCiREF with polarity and dead zone | inverted OCiREF with polarity and dead zone |

Findings:

- Speaker, P1.1 (`PWM1N`): CC1E = 0, CC1NE = 1, so PWM1N = OC1REF XOR CC1NP. With CC1NP = 1, P1.1 is LOW while
  CNT < CCR1 and HIGH at CCR1 = 0. This is the required quiet-HIGH, LOW-active output.
- The plan statement "CC1NE only with CC1NP = 1" is correct. The inversion of the N output applies only when
  CC1E = 1 too. So CC1E must stay 0.
- The first speaker capture (LOW for about 95 % at a 5 % duty) matches CC1NP = 0 in this table: PWM1N then
  follows OC1REF directly. This supports the reading. A new capture must check it (device check DC-2).
- Headphones, P1.2 (`PWM2P`): CC2E = 1, CC2NE = 0, CC2P = 1. P1.2 is LOW while CNT < CCR2 and HIGH at CCR2 = 0.
- OSSR = 0 keeps the unused half of each pair "output prohibited". With OSSR = 1, OC1 on P1.0 becomes
  "enabled at the inactive level". ENO1P = 0 already keeps P1.0 a GPIO, so OSSR = 0 is a second barrier.
- CCPC = 0 (`PWMA_CR2`) so that the CCER bits take effect at once, without a COM event.

### 4.2 Prohibited outputs

- ENO1P (bit 0) never becomes 1. P1.0 is `PWM1P` and also the plug-detect input. An enabled PWM1P drives
  the plug line.
- ENO2N (bit 3) never becomes 1. P1.3 is `PWM2N` and is not an audio route.
- ENO3P-ENO4N stay 0. `PWMA_PS` never selects 01 for any channel, because 01 puts PWMA on the LCD pins of port 2.
- The design makes these rules structural: the ISR writes `PWMA_ENO` only through three macros with the fixed
  values 0x00, 0x02 (ENO1N) and 0x04 (ENO2P). `PWMA_PS` is written once, before any ENO bit, with 0x00.

## 5. Interfaces

Rules for all new code:

- C11 as SDCC accepts it (`--std=sdcc11`), small model. No `--stack-auto`, no `__reentrant`, no `__using`, no
  function pointers on the target.
- Constants are `#define` values with a `U` suffix. Interfaces use `typedef uint8_t` values, no `enum`.
- Boolean names start with `is`, `has`, `should`, `can`, `does` or `was`.
- Hardware access goes through `inc/board/sidetone_pwm.h`. Its host replacement is
  `firmware/Tests/fakes/board/sidetone_pwm.h`.
- No module clears `EA`. `main.c` stays the only file that sets `EA`. No code clears `EAXFR`.
- No ISR writes port 2, the LCD pins, the button pins, P0.3, PWMB or IAP.
- Comments follow the project rules: short, the reason and not the action, simple English, no status narration.

### 5.1 `inc/mcu/compiler.h` additions

```c
#if defined(__SDCC_mcs51)
#define MEM_DATA __data   /* direct internal RAM */
#define MEM_BIT __bit     /* one bit in the bit area 0x20-0x2F; used as a type: static volatile MEM_BIT is_x; */
#else
#define MEM_DATA
#define MEM_BIT bool      /* needs <stdbool.h> */
#endif
```

### 5.2 `inc/mcu/stc8h.h` additions

Follow the existing style: normal SFRs with `__sfr __at(...)`, XFRs as `volatile __xdata` macros, no `__at`
variables for XFRs.

| Name | Address | Kind |
| --- | --- | --- |
| `P1M1`, `P1M0` | 0x91, 0x92 | SFR |
| `AUXR` | 0x8E | SFR (not in SDCC `<8051.h>`) |
| `P1PU` | 0xFE11 | XFR |
| `PWMA_ENO`, `PWMA_PS`, `PWMA_IOAUX` | 0xFEB1, 0xFEB2, 0xFEB3 | XFR |
| `PWMA_CR1`, `PWMA_CR2`, `PWMA_IER`, `PWMA_SR1`, `PWMA_EGR` | 0xFEC0, 0xFEC1, 0xFEC4, 0xFEC5, 0xFEC7 | XFR |
| `PWMA_CCMR1`, `PWMA_CCMR2`, `PWMA_CCER1` | 0xFEC8, 0xFEC9, 0xFECC | XFR |
| `PWMA_PSCRH`, `PWMA_PSCRL`, `PWMA_ARRH`, `PWMA_ARRL` | 0xFED0, 0xFED1, 0xFED2, 0xFED3 | XFR |
| `PWMA_CCR1H`, `PWMA_CCR1L`, `PWMA_CCR2H`, `PWMA_CCR2L` | 0xFED5, 0xFED6, 0xFED7, 0xFED8 | XFR |
| `PWMA_BKR` | 0xFEDD | XFR |

Bit values (one `#define` each, names in the style of the PWMB block):

- `PWMA_ENO_ENO1N 0x02U`, `PWMA_ENO_ENO2P 0x04U`.
- `PWMA_PS_P1 0x00U` (C1PS = C2PS = C3PS = C4PS = 00).
- `PWMA_CR1_CEN 0x01U`, `PWMA_CR1_ARPE 0x80U`, `PWMA_EGR_UG 0x01U`, `PWMA_SR1_UIF 0x01U`.
- `PWMA_CCMR_PWM1_PRELOAD 0x68U` (OCiM = 110, OCiPE = 1, CCiS = 00).
- `PWMA_CCER1_CC1NE 0x04U`, `PWMA_CCER1_CC1NP 0x08U`, `PWMA_CCER1_CC2E 0x10U`, `PWMA_CCER1_CC2P 0x20U`.
- `PWMA_BKR_MOE 0x80U`.
- `AUXR_T0X12 0x80U`, `TMOD_T0_MASK 0x0FU`.
- `#define TIMER0_INTERRUPT 1` (vector 0x000B).

`TMOD`, `TL0`, `TH0`, `TR0`, `TF0`, `ET0`, `P1`, `P1_0`, `P1_1`, `P1_2` come from SDCC `<8051.h>`.

### 5.3 `inc/board/sidetone_config.h`

Values can be changed with `-D` where marked. Host tests use the default target values
(`MCU_CLOCK_HZ=24000000UL`).

```c
#define SIDETONE_SERVICE_HZ 4000U          /* envelope service rate */
#define SIDETONE_PRESCALER 1U              /* PWMA_PSCR; counter = MCU_CLOCK_HZ / 2 = 12 MHz */
#define SIDETONE_COUNTER_HZ (MCU_CLOCK_HZ / ((uint32_t)SIDETONE_PRESCALER + 1UL))
#define SIDETONE_TONE_MIN_HZ 400U          /* equal to settings_min(SETTINGS_ID_TONE_HZ), host test AC-11 */
#define SIDETONE_TONE_MAX_HZ 1000U         /* equal to settings_max(SETTINGS_ID_TONE_HZ), host test AC-11 */
#define SIDETONE_RAMP_STEPS 32U            /* the duty table has SIDETONE_RAMP_STEPS + 1 values */
#define SIDETONE_RAMP_MIN_MS 4U            /* equal to settings_min(SETTINGS_ID_RAMP_MS), host test AC-28 */
#define SIDETONE_RAMP_MAX_MS 12U           /* equal to settings_max(SETTINGS_ID_RAMP_MS), host test AC-28 */
#define SIDETONE_RAMP_CALLS_PER_MS (SIDETONE_SERVICE_HZ / 1000U)                          /* 4 */
#define SIDETONE_RAMP_MAX_CALLS (SIDETONE_RAMP_CALLS_PER_MS * SIDETONE_RAMP_MAX_MS)        /* 48 */
#ifndef SIDETONE_PLUG_DEBOUNCE_MS
#define SIDETONE_PLUG_DEBOUNCE_MS 50U      /* -D; not checked on the device */
#endif
/* One carrier period at the lowest tone, in service calls, plus a margin of 2. */
#define SIDETONE_ZERO_WAIT_LIMIT \
  ((uint8_t)(((SIDETONE_SERVICE_HZ + SIDETONE_TONE_MIN_HZ - 1U) / SIDETONE_TONE_MIN_HZ) + 2U))  /* 12 */
#define SIDETONE_TIMER0_COUNTS (MCU_CLOCK_HZ / 12UL / SIDETONE_SERVICE_HZ)                    /* 500 */
#define SIDETONE_TIMER0_RELOAD ((uint16_t)(65536UL - SIDETONE_TIMER0_COUNTS))                 /* 0xFE0C */
```

Compile-time checks (`#if ... #error`):

- `MCU_CLOCK_HZ % (12 * SIDETONE_SERVICE_HZ) == 0` (exact 4 kHz) and `1 <= SIDETONE_TIMER0_COUNTS <= 65536`.
- `1 <= SIDETONE_TONE_MIN_HZ <= SIDETONE_TONE_MAX_HZ <= 65535`.
- The rounded period at `SIDETONE_TONE_MIN_HZ` is at most 65536 counts (ARR fits 16 bits).
- `SIDETONE_COUNTER_HZ / SIDETONE_TONE_MAX_HZ >= 1000`. Then each step of the permille table changes the compare
  value by at least one count, so the compare table is strictly increasing.
- `SIDETONE_ZERO_WAIT_LIMIT <= 255`.
- `SIDETONE_SERVICE_HZ == 4000U`. The increment table of section 6.8 holds values for 4 calls per ms.
- `1 <= SIDETONE_RAMP_MIN_MS <= SIDETONE_RAMP_MAX_MS` and `SIDETONE_RAMP_MAX_CALLS <= 255` (the call count is one
  byte).

The macros of this header have no casts where an `#if` uses them. `timing.h` uses `SIDETONE_RAMP_MAX_CALLS` and
`SIDETONE_ZERO_WAIT_CALLS` in `#if` (`training_spec.md` section 5.3).

### 5.4 Public API `inc/sidetone/sidetone.h`

```c
#include <stdbool.h>
#include <stdint.h>

#include "mcu/compiler.h"

#if defined(__SDCC_mcs51)
#include "mcu/stc8h.h"
#endif

typedef uint8_t sidetone_state_t;

#define SIDETONE_STATE_OFF 0U        /* output disabled, confirmed at zero */
#define SIDETONE_STATE_RISING 1U
#define SIDETONE_STATE_STEADY 2U     /* position at full scale, 50 % duty */
#define SIDETONE_STATE_FALLING 3U
#define SIDETONE_STATE_ZERO_WAIT 4U  /* compare 0 written, waits for the PWMA update flag */

void sidetone_init(uint16_t tone_hz, MEM_XDATA uint8_t ramp_ms);
void sidetone_set_tone_hz(uint16_t tone_hz);
void sidetone_set_ramp_ms(uint8_t ramp_ms);
void sidetone_set_on(bool is_on);
bool sidetone_is_busy(void);
sidetone_state_t sidetone_state(void);
void sidetone_poll(void);
void sidetone_service(void);

#if defined(__SDCC_mcs51)
/* SDCC creates the interrupt vector only when main.c sees this declaration. */
void sidetone_timer0_isr(void) __interrupt(TIMER0_INTERRUPT);
#endif
```

| Function | Context | Behavior |
| --- | --- | --- |
| `sidetone_init(tone_hz, ramp_ms)` | Main loop, once, at start-up. `EA` can be 1. | Clamps `tone_hz` (section 6.1). Sets the ramp with the same clamp and the same two stores as `sidetone_set_ramp_ms(ramp_ms)`, but with no preemption point and no call (section 6.8, D-RM11). Fills table A and its ARR, selects A, clears the pending flag and the on request. Configures PWMA with ARR of A, compares 0, all ENO bits 0. Configures the pins. Samples P1.0 once and takes it as the debounced, requested and active route. State OFF, position 0. Starts Timer 0 last. Silent the whole time. Does not change `EA`. |
| `sidetone_set_tone_hz(tone_hz)` | Main loop only. | Clamps. When the clamped value equals the last requested value, returns at once. Else writes the inactive table and its ARR through the mailbox (section 6.4) and stores the value as the last requested. The new tone sounds after the next silence (OFF). Runs 33 32-bit multiply-divide operations. No hardware access. |
| `sidetone_set_ramp_ms(ramp_ms)` | Main loop only. Call it only while the keyer and the CW player are disabled (D-RM6). | Clamps `ramp_ms` to `SIDETONE_RAMP_MIN_MS..SIDETONE_RAMP_MAX_MS`. Writes the increment byte, then `sidetone_ramp_calls` = 4 x the clamped value (section 6.8). No early return for a repeated value. No fall, no restart: a ramp that runs goes on at the new speed. No hardware access, no multiply, no division. |
| `sidetone_set_on(is_on)` | Main loop only (section 3.10). | Writes the on-request bit with one instruction. The ISR starts or ends the envelope on its next call. |
| `sidetone_is_busy()` | Main loop. | Returns true for a set on request or for a state other than OFF. False means: the output is disabled and nothing is requested. |
| `sidetone_state()` | Main loop. | Returns the state byte (one read). |
| `sidetone_poll()` | Main loop, each pass. | Calls `tick_now()` exactly once. Debounces P1.0 (section 6.6) and publishes the requested route bit. |
| `sidetone_service()` | Timer 0 ISR only. Host tests call it directly. | One envelope step (section 6.3). Calls no function. |
| `sidetone_timer0_isr()` | Interrupt 1, 4 kHz. Target only. | Calls `sidetone_service()` and nothing else. |

### 5.5 Target header `inc/board/sidetone_pwm.h`

Target only. The first lines are `#if !defined(__SDCC_mcs51)` / `#error`, as in `backlight_pwm.h`. Every macro is a
`do { ... } while (0)` statement or a plain expression. No macro calls a function, so `sidetone_service()` stays a
leaf on the target.

| Macro | Context | Effect (target) |
| --- | --- | --- |
| `SIDETONE_PWM_CONFIGURE(arr)` | init | The full PWMA sequence of section 7.1 with ARR = `arr`. |
| `SIDETONE_PINS_CONFIGURE()` | init | Section 7.2. |
| `SIDETONE_PINS_IS_HEADPHONES()` | init, `sidetone_poll` | Expression: true for a LOW level on P1.0 (`!P1_0`). Reads the pin bit only. |
| `SIDETONE_TIMER_START()` | init, last step | Section 8. |
| `SIDETONE_PWM_SET_ARR(value)` | ISR | `PWMA_ARRH` then `PWMA_ARRL`. |
| `SIDETONE_PWM_SET_COMPARE_SPEAKER(value)` | ISR | `PWMA_CCR1H` then `PWMA_CCR1L`. |
| `SIDETONE_PWM_SET_COMPARE_HEADPHONES(value)` | ISR | `PWMA_CCR2H` then `PWMA_CCR2L`. |
| `SIDETONE_PWM_GENERATE_UPDATE()` | ISR | `PWMA_EGR = PWMA_EGR_UG`. |
| `SIDETONE_PWM_CLEAR_UPDATE_FLAG()` | ISR | `PWMA_SR1 = 0x00` (PWMA uses no other flag and no PWMA interrupt). |
| `SIDETONE_PWM_IS_UPDATE_FLAG_SET()` | ISR | Expression: `(PWMA_SR1 & PWMA_SR1_UIF) != 0`. |
| `SIDETONE_PWM_ENABLE_SPEAKER()` | ISR | `PWMA_ENO = PWMA_ENO_ENO1N`. |
| `SIDETONE_PWM_ENABLE_HEADPHONES()` | ISR | `PWMA_ENO = PWMA_ENO_ENO2P`. |
| `SIDETONE_PWM_DISABLE_OUTPUTS()` | ISR | `PWMA_ENO = 0x00`. |
| `SIDETONE_PREEMPTION_POINT()` | main loop | Empty on the target. Section 12.1 gives the host behavior. |

After `sidetone_init()`, only the Timer 0 ISR writes PWMA registers. So the byte writes to `PWMA_ENO` and
`PWMA_SR1` need no read-modify-write protection.

### 5.6 ISR at the end of `src/sidetone/sidetone.c`

```c
#if defined(__SDCC_mcs51)
void sidetone_timer0_isr(void) __interrupt(TIMER0_INTERRUPT) { sidetone_service(); }
#endif
```

There is no separate ISR file. The ISR must be in the same file as `sidetone_service()` (D-R2-1). The host build
does not see it. TF0 needs no software clear (hardware clears it at the vector, 13.2.1).

### 5.7 Shared ramp call count `inc/sidetone/sidetone_shared.h` (new, D-RM3)

```c
#include <stdint.h>

#include "mcu/compiler.h"

/*
 * Service calls of one full rise or fall: 4 x the ramp in ms, 16..48. sidetone_set_ramp_ms() writes it. The keyer
 * and the CW player read it in the Timer 0 interrupt. One byte, so each access is atomic.
 */
extern volatile MEM_DATA uint8_t sidetone_ramp_calls;
```

- `src/sidetone/sidetone.c` defines `volatile MEM_DATA uint8_t sidetone_ramp_calls;` without a static initializer.
  `sidetone_init()` writes it before Timer 0 starts.
- `cw/keyer_shared.h` and `cw/player_shared.h` include this header. They define `KEYER_EARLY_RELEASE_TICKS` and
  `PLAYER_EARLY_RELEASE_TICKS` as `sidetone_ramp_calls` (`keyer_spec.md` section 18, `training_spec.md` D-T51).
- Only `sidetone.c` writes it. Only `keyer_service.h`, `player_service.h` and host tests read it. A file that only
  includes `keyer_shared.h` or `player_shared.h` and does not read it needs no link to `sidetone.c`.

## 6. Behavior

### 6.1 Tone math (main loop only)

- `clamped = min(max(tone_hz, SIDETONE_TONE_MIN_HZ), SIDETONE_TONE_MAX_HZ)`. The sidetone has no step grid.
  The `settings` module owns the 10 Hz grid.
- `period = (SIDETONE_COUNTER_HZ + clamped / 2) / clamped` in `uint32_t`, stored as `uint16_t`. `ARR = period - 1`.
- `compare[k] = (uint16_t)(((uint32_t)period * duty_permille[k]) / 1000UL)`, k = 0..32.
- `duty_permille` is `static const MEM_CODE uint16_t[SIDETONE_RAMP_STEPS + 1U]` with the values of the research
  document (below). A compile-time check ties the array length to `SIDETONE_RAMP_STEPS + 1`.

```text
0, 1, 5, 11, 19, 30, 42, 57, 73, 91, 111, 132, 154, 177, 201, 225, 250,
275, 299, 323, 346, 368, 389, 409, 427, 443, 458, 470, 481, 489, 495, 499, 500
```

Reference values (checked with a script against the formulas above):

| Tone Hz | period | ARR | compare[1] | compare[16] | compare[32] |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 400 | 30000 | 29999 | 30 | 7500 | 15000 |
| 600 | 20000 | 19999 | 20 | 5000 | 10000 |
| 605 | 19835 | 19834 | 19 | 4958 | 9917 |
| 610 | 19672 | 19671 | 19 | 4918 | 9836 |
| 800 | 15000 | 14999 | 15 | 3750 | 7500 |
| 990 | 12121 | 12120 | 12 | 3030 | 6060 |
| 1000 | 12000 | 11999 | 12 | 3000 | 6000 |

At 1000 Hz, `compare[k] = 12 * duty_permille[k]` exactly. Tests use this to read the table back.

### 6.2 State and memory placement

| Variable | Type and qualifier | Writer | Readers |
| --- | --- | --- | --- |
| state | `static volatile MEM_DATA uint8_t` | ISR (and init) | ISR, main (`sidetone_is_busy`, `sidetone_state`) |
| position | `static MEM_DATA uint16_t`, 0..`SIDETONE_POSITION_FULL` (2048), in 1/64 of a table step. The table index is `position >> 6`. In ZERO_WAIT it counts the wait calls. | ISR (and init) | ISR |
| ramp increment | `static volatile MEM_DATA uint8_t`, 43..128 (section 6.8) | main (`sidetone_set_ramp_ms`, init) | ISR, once per call |
| `sidetone_ramp_calls` | `volatile MEM_DATA uint8_t`, 16..48, not `static` (section 5.7) | main (`sidetone_set_ramp_ms`, init) | keyer and player in the ISR |
| ramp increments | `static const MEM_CODE uint8_t[9]` (section 6.8) | - | main |
| is_on_requested | `static volatile MEM_BIT` | main | ISR |
| is_tone_pending | `static volatile MEM_BIT` | main sets and clears, ISR clears | ISR |
| is_table_b_active | `static volatile MEM_BIT` | ISR (and init) | ISR, main |
| is_headphones_requested | `static volatile MEM_BIT` | main (`sidetone_poll`, init) | ISR |
| is_headphones_active | `static MEM_BIT` | ISR (and init) | ISR |
| table_a, table_b | `static MEM_XDATA uint16_t[33]` each, two separate arrays | main (inactive one only), init | ISR (active one only) |
| arr_a, arr_b | `static MEM_XDATA uint16_t` each | main (inactive one only), init | ISR |
| requested_hz | `static MEM_XDATA uint16_t` | main | main |
| plug debounce: candidate, change time | `static MEM_XDATA` (`bool`, `uint16_t`). The debounced value is `is_headphones_requested` itself (section 20). | main | main |
| duty_permille | `static const MEM_CODE uint16_t[33]` | - | main |

Rules:

- The ISR selects the table and the ARR with `is_table_b_active` and two separate arrays. It never indexes a 2-D
  array with a variable first index, because that needs a multiplication.
- The ISR reads each flag bit directly. One `JB`, `JNB`, `SETB`, `CLR` or `MOV bit,C` is atomic.
- New static internal RAM is in the budget of section 14.

### 6.3 Envelope state table (`sidetone_service`, the ISR is the only writer)

Names: `P` = position, `I` = ramp increment, `FULL` = `SIDETONE_POSITION_FULL` = `SIDETONE_RAMP_STEPS * 64` = 2048.
`SIDETONE_POSITION_FULL` and `SIDETONE_POSITION_SHIFT` (6) are private macros of `sidetone.c`.

Each call first reads `I` once into a local and forms the target direction:

```text
is_up = is_on_requested AND NOT is_tone_pending
```

| State | Condition | Actions, in this order | Next state |
| --- | --- | --- | --- |
| OFF | `is_on_requested` = 0 | none | OFF |
| OFF | `is_on_requested` = 1 | 1. When `is_tone_pending`: flip `is_table_b_active`, clear `is_tone_pending`, `SET_ARR(arr of the new active table)`. 2. `is_headphones_active = is_headphones_requested`. 3. `SET_COMPARE_<route>(0)`. 4. `GENERATE_UPDATE()`. 5. `ENABLE_<route>()`. 6. P = 0. | RISING |
| RISING, STEADY, FALLING | `is_up` and P < FULL | P = P + I. When P > FULL: P = FULL. `SET_COMPARE_<route>(active[P >> 6])`. | P = FULL: STEADY. Else RISING. |
| RISING, STEADY, FALLING | `is_up` and P = FULL | none | STEADY |
| RISING, STEADY, FALLING | not `is_up` and P > I | P = P - I. `SET_COMPARE_<route>(active[P >> 6])`. | FALLING |
| RISING, STEADY, FALLING | not `is_up` and 0 < P <= I | P = 0. `SET_COMPARE_<route>(active[0])`. Then `CLEAR_UPDATE_FLAG()`. | ZERO_WAIT |
| RISING | not `is_up` and P = 0 (off right after the start call) | `CLEAR_UPDATE_FLAG()` (the compare is already 0 and UG loaded it) | ZERO_WAIT |
| ZERO_WAIT | `IS_UPDATE_FLAG_SET()` | `DISABLE_OUTPUTS()`. P = 0. | OFF |
| ZERO_WAIT | flag clear, P + 1 < `SIDETONE_ZERO_WAIT_LIMIT` | P = P + 1 | ZERO_WAIT |
| ZERO_WAIT | flag clear, P + 1 = `SIDETONE_ZERO_WAIT_LIMIT` | `DISABLE_OUTPUTS()`. P = 0. | OFF |

`<route>`: `is_headphones_active` = 1 selects `HEADPHONES`, 0 selects `SPEAKER`. `active[]`: `is_table_b_active` = 1
selects table B, 0 selects table A. P + I is at most 2048 + 128, so it fits 16 bits.

Properties that follow, and that the tests check. `N` is the ramp in ms, `R` = `4 * N` = `sidetone_ramp_calls`:

- From OFF with an on request: the start call, then exactly R calls to STEADY. Call `j` (1..R) writes
  `active[min(j * I, FULL) >> 6]`. The indexes never go down, and the last one is 32.
- From STEADY with the request cleared: exactly R calls to ZERO_WAIT. The indexes never go up, and the last write
  is `active[0]`.
- At 8 ms (I = 64) the rise writes `active[1]` to `active[32]` and the fall `active[31]` to `active[0]`, one step
  per call. This is the sequence of the first design.
- Two writes in a row differ by at most `ceil(I / 64)` table steps: 2 at 4 to 7 ms, 1 at 8 to 12 ms. At 9 to
  12 ms the same index can come on two calls in a row.
- At 9 to 12 ms (I < 64) the first rise call writes `active[0]` again: the start call already wrote compare 0.
- At 9 to 12 ms the fall can write `active[0]` while still in FALLING (0 < P < 64). The next call writes
  `active[0]` again and clears the flag. The flag clear always comes after a compare 0 write.
- A reversal at any position moves one increment in the new direction on the next call. The position never jumps.
- A new increment during a ramp changes only the speed of the rest of that ramp. The position does not jump.
- A pending tone acts like an off request: STEADY or RISING falls, FALLING keeps falling, and no rise happens until
  OFF applies the new table. In ZERO_WAIT and OFF it is held until the next start.
- ARR changes only in OFF, and UG happens only in OFF. No UG during a ramp.
- ZERO_WAIT ignores the on request. The new start happens on the call after OFF.
- The route changes only in OFF. A plug change while sounding takes effect at the next start.
- The ISR writes only the compare register of the active route. The other channel keeps compare 0.
- `PWMA_ENO` gets only the values 0x00, 0x02 and 0x04.
- In OFF without an on request, a call does only the request test (shortest ISR path).

### 6.4 Tone mailbox protocol (lock-free, no `EA` change)

Writer: `sidetone_set_tone_hz()` in the main loop. Reader: the OFF + on row of section 6.3.

Main loop, in this order:

1. Clamp. When equal to `requested_hz`, return.
2. `is_tone_pending = 0` (one `CLR`). From here, the ISR does not flip the selector.
3. Read `is_table_b_active`. The inactive table is the other one. The ISR changes the selector only while
   `is_tone_pending` = 1, so this choice stays valid.
4. Write the 33 compare values and the ARR of the inactive table. Call `SIDETONE_PREEMPTION_POINT()` after step 2,
   after each table store and after the ARR store.
5. `is_tone_pending = 1` (one `SETB`). Then `SIDETONE_PREEMPTION_POINT()`.
6. `requested_hz = clamped`. Only the main loop reads `requested_hz`, so this store can come at any point after
   step 1 (D-R3-2).

ISR: it reads the inactive table only in the OFF + on row and only while `is_tone_pending` = 1. In that row it
flips the selector, clears the flag and loads the ARR.

Why it cannot tear:

- The ISR runs to its end before the main loop continues. The main loop never interrupts the ISR.
- The ISR never reads the inactive table while the main loop writes it, because the main loop cleared the flag
  first.
- The ISR never writes a table.
- A new request before the ISR used the previous one overwrites the same inactive table. The newest tone wins.
- If the ISR starts a rise between steps 2 and 5, it uses the old active table, which is complete. When the flag
  comes, the envelope falls and rises again at the new tone. This is the defined behavior, not an error.

### 6.5 On request and route mailbox

- `is_on_requested`: one bit. The main loop writes it, the ISR reads it. No protocol needed.
- `is_headphones_requested`: one bit. `sidetone_poll()` writes it after the debounce. The ISR copies it into
  `is_headphones_active` only in the OFF + on row.

### 6.6 Plug debounce (`sidetone_poll`)

1. Read the raw pin with `SIDETONE_PINS_IS_HEADPHONES()` and `now = tick_now()`, each once.
2. When raw differs from the candidate: candidate = raw, change time = now.
3. When raw equals the candidate, the candidate differs from `is_headphones_requested`, and
   `(uint16_t)(now - change time) >= SIDETONE_PLUG_DEBOUNCE_MS`: `is_headphones_requested = candidate`. The main
   loop is the only writer of this bit after init, so it is also the debounced value.
4. Correct across the 16-bit tick wrap.

`sidetone_init()` takes its single raw sample as candidate, requested and active value. The pull-up has
just turned on, so this sample can be wrong. The debounce corrects it within 50 ms, long before any user action
can start the preview.

### 6.7 `sidetone_init(tone_hz)` order

1. Clamp, fill table A and `arr_a`. `requested_hz` = clamped. `is_table_b_active` = 0, `is_tone_pending` = 0,
   `is_on_requested` = 0, state OFF, position 0. Set the ramp from `ramp_ms` with steps 1 to 3 of section 6.8, but
   with no preemption point (Timer 0 is not running yet).
2. `SIDETONE_PWM_CONFIGURE(arr_a)`.
3. `SIDETONE_PINS_CONFIGURE()`.
4. Route sample (section 6.6).
5. `SIDETONE_TIMER_START()`.

Timer 0 is not running before step 5, so the ISR cannot see half-set state. Call it once.

### 6.8 Ramp setting (`sidetone_set_ramp_ms`, main loop, D-RM1, D-RM2)

1. `N` = `ramp_ms` clamped to `SIDETONE_RAMP_MIN_MS..SIDETONE_RAMP_MAX_MS` (4..12).
2. Ramp increment = `sidetone_ramp_increments[N - SIDETONE_RAMP_MIN_MS]` (one `MOV`). Then
   `SIDETONE_PREEMPTION_POINT()`.
3. `sidetone_ramp_calls` = `(uint8_t)(N << 2)` (one `MOV`). Then `SIDETONE_PREEMPTION_POINT()`.

`sidetone_ramp_increments` is `static const MEM_CODE uint8_t[]` with 9 values, indexed by `N - 4`. A
`_Static_assert` ties its length to `SIDETONE_RAMP_MAX_MS - SIDETONE_RAMP_MIN_MS + 1`. Value = `ceil(512 / N)` =
`ceil(FULL / (4 * N))`:

| N (ms) | 4 | 5 | 6 | 7 | 8 | 9 | 10 | 11 | 12 |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| Increment | 128 | 103 | 86 | 74 | 64 | 57 | 52 | 47 | 43 |
| Calls of a full ramp, `ceil(2048 / increment)` | 16 | 20 | 24 | 28 | 32 | 36 | 40 | 44 | 48 |

`architector` checked each column with a script (2026-10-16): with the rules of section 6.3, a full rise and a
full fall each take exactly `4 * N` calls. The main loop needs no division and no multiply.

Rules:

- Each of the two stores is one byte, so each is atomic. The pair is not. An ISR call between steps 2 and 3 sees
  the new increment with the old call count. Effect: at most one keyer or player mark ends its fall up to
  32 calls (8 ms) away from the mark end. The fall still ends in silence, because the fall rule needs no call
  count. In this change only `sidetone_init()` and the menu Ramp edit call it. At both times the keyer and the
  player are disabled (D-RM6).
- No fall and no restart. A tone that sounds stays on. The next rise or fall uses the new speed.
- The sidetone never changes the ramp by itself.

Physics note (no defect): the PWM loads a new compare value only at an update event, once per carrier period
(compare preload OCiPE = 1, datasheet 20.5.6 and 20.7.11). One carrier period is 2.5 ms at 400 Hz and 1 ms at 1000 Hz. A
4 ms ramp at 400 Hz therefore reaches the output as only 1 or 2 compare values. A short ramp at a low tone can
click more. Device check DC-13.

## 7. PWMA and pin configuration

### 7.1 `SIDETONE_PWM_CONFIGURE(arr)` sequence

| Step | Write | Value | Reason | Section |
| --- | --- | --- | --- | --- |
| 1 | `P_SW2 \|= P_SW2_EAXFR` | `ORL` | XFR access. It stays set. | 3.1.2 |
| 2 | `PWMA_ENO` | 0x00 | All PWMA pins are GPIO during setup. | 20.7.1 |
| 3 | `PWMA_CCER1` | 0x00 | Channels off, so CCMR CCiS is writable. | 20.7.11, 20.7.15 |
| 4 | `PWMA_CR1` | 0x00 | Counter stopped during setup. | 20.7.3 |
| 5 | `PWMA_IER`, `PWMA_CR2`, `PWMA_IOAUX` | 0x00 each | No PWMA interrupt. CCPC = 0. ENO alone controls the pins. | 20.7.7, 20.7.4, 20.7.2 |
| 6 | `PWMA_PS` | 0x00 | PWM1N on P1.1, PWM2P on P1.2. Before any ENO bit. Never 01 (port 2). | 3.1.5 |
| 7 | `PWMA_PSCRH`, `PWMA_PSCRL` | 0x00, 0x01 | 12 MHz counter at 24 MHz. | 20.3.3, 20.7.19 |
| 8 | `PWMA_ARRH`, `PWMA_ARRL` | `arr` high, low | High byte first. | 20.3.2 |
| 9 | `PWMA_CCMR1`, `PWMA_CCMR2` | 0x68 each | PWM mode 1, compare preload, output. | 20.5.7, 20.7.11 |
| 10 | `PWMA_CCR1H/L`, `PWMA_CCR2H/L` | 0 | Quiet at the first update. | 20.5.1 |
| 11 | `PWMA_CCER1` | 0x3C | CC1NE + CC1NP, CC2E + CC2P. CC1E = CC1P = CC2NE = CC2NP = 0 (section 4.1). | 20.7.15 |
| 12 | `PWMA_BKR` | 0x80 | MOE = 1. AOE = BKE = BKP = OSSR = OSSI = 0, LOCK = 00. | 20.7.32 |
| 13 | `PWMA_EGR` | 0x01 | UG loads ARR, PSCR and compares. | 20.7.10 |
| 14 | `PWMA_SR1` | 0x00 | Clears the UIF that UG set. | 20.7.8 |
| 15 | `PWMA_CR1` | 0x81 | ARPE + CEN. Edge-aligned (CMS = 00), up (DIR = 0), URS = 0, UDIS = 0, OPM = 0. | 20.7.3 |

The counter then runs all the time, also in OFF. ZERO_WAIT needs its update events. Do not touch `PWMA_DTR`,
`PWMA_OISR`, `PWMA_SMCR` or `PWMA_ETR`. Their reset value 0 is correct.

### 7.2 `SIDETONE_PINS_CONFIGURE()` sequence

| Step | Write | Reason |
| --- | --- | --- |
| 1 | `P1_1 = 1`, `P1_2 = 1` (`SETB`) | Latch HIGH before the pins drive. Reset latch is 1 too. |
| 2 | `P1M0 \|= 0x06` (`ORL`) | P1.1 and P1.2 pass through open-drain with latch 1 (released, HIGH by the board). |
| 3 | `P1M1 &= ~0x06` (`ANL`) | P1.1 and P1.2 push-pull HIGH. M0 before M1 avoids a LOW glitch. |
| 4 | `P1PU \|= 0x01` (`ORL`, XFR) | Pull-up on P1.0. The need for it is not checked on the device. |
| 5 | `P1M1 \|= 0x01`, `P1M0 &= ~0x01` | P1.0 high-impedance input. This is the reset mode too. The write makes the state clear. |

Before this macro, all of port 1 is in high-impedance input (reset). The board pulls P1.1 and P1.2 HIGH, so they
are quiet from reset. No code writes the full `P1` byte.

## 8. Timer 0 setup `SIDETONE_TIMER_START()`

| Step | Write | Reason | Section |
| --- | --- | --- | --- |
| 1 | `TR0 = 0`, `ET0 = 0` (`CLR`) | Stopped while it is set up. | 13.2.1, 11.4.1 |
| 2 | `TMOD &= 0xF0` (`ANL`) | T0: GATE = 0, C/T = 0, mode 0 (16-bit auto-reload). High nibble is Timer 1. Never mode 3. | 13.2.2 |
| 3 | `AUXR &= 0x7F` (`ANL`) | T0x12 = 0: SYSclk / 12 = 2 MHz. `EXTRAM` keeps 0. | 13.2.12, 7.2.4 |
| 4 | `TL0 = 0x0C`, `TH0 = 0xFE` | 500 counts = 250 us. While TR0 = 0, these writes also set the reload. | 13.2.3 |
| 5 | `TF0 = 0` (`CLR`) | No stale request. | 13.2.1 |
| 6 | `ET0 = 1` (`SETB`, IE bit 1) | Enable. R2 allows single interrupt-enable bits. | 11.4.1 |
| 7 | `TR0 = 1` (`SETB`) | Start. | 13.2.1 |

No `IP` or `IPH` write. Timer 0 keeps priority level 0, the same level as the Timer 3 tick, so the two ISRs never
nest. The 4 kHz rate is exact at 24 MHz. The only error is the oscillator trim.

## 9. SDCC constraints

- `sidetone_service()` is called from the ISR. Wrap its definition in `#pragma save` / `#pragma nooverlay` /
  `#pragma restore`, inside `#if defined(__SDCC_mcs51)`, so that GCC sees no unknown pragma. Its locals then go
  to DSEG and never share the overlay with main-loop functions.
- `sidetone_service()` calls no function. All hardware access is macros. The ISR calls only
  `sidetone_service()`, and both are in `sidetone.c`. With `--norestartseqatomics`, SDCC 4.6.0 still gives an ISR
  that calls a function of another file the `sdcc_atomic_maybe_rollback` epilogue, and the link then fails. So an
  ISR calls only functions of its own file. Measured with SDCC 4.6.0: an ISR that calls a function pushes 14 bytes (`bits`, `acc`,
  `b`, `dpl`, `dph`, r0-r7, `psw`). That is accepted.
- The ISR path uses no library helper: no `__mulint`, `__divuint`, `__moduint`, `__mullong`, `__divulong`,
  `__modulong` or other `__` arithmetic routine. These routines are not reentrant and the main loop uses them.
  Use `uint8_t` index arithmetic and separate arrays (section 6.2).
- The position math of section 6.3 is a 16-bit add, subtract and compare and a shift by the constant 6. SDCC
  generates all of them inline. The increment is a byte, so no operation needs a multiply. Check the `.asm`.
- The 32-bit math of section 6.1 runs only in `sidetone_init()` and `sidetone_set_tone_hz()`.
- `sidetone_set_ramp_ms()` uses a table read and `N << 2`. It has no library helper.
- The ISR relies on `EAXFR` = 1 and never reads, saves or writes `P_SW2`. `SIDETONE_PWM_CONFIGURE()` sets it with
  `ORL`. No code clears it.
- No `__critical`, no write to `EA` or the `IE` byte. `ET0` changes only with `SETB` and `CLR`.
- The firmware build uses `--norestartseqatomics` (section 20). No source includes `<stdatomic.h>` or uses C11
  atomics. Every ISR ends with a plain `reti`.
- `bool` variables in SDCC are DSEG bytes. Use `MEM_BIT` for the ISR flags of section 6.2, and `MEM_XDATA bool` for
  main-loop-only flags.
- Parameters after the first of a non-leaf main-loop function can have `MEM_XDATA`, as in the menu spec section 4.
- Everything compiles on the host with `-Wall -Wextra -Wpedantic -Werror`, ASan and UBSan. No new suppression.

## 10. Menu and `main.c` integration

### 10.1 `main.c`

Start-up order (changes in bold):

1. `tick_init()`, 2. `EA = 1`, 3. `eeprom_init()`, 4. `buttons_init()`, 5. `settings_load()`,
6. recovery defaults (only after a recovery start),
**7. `sidetone_init(settings_get(SETTINGS_ID_TONE_HZ), (uint8_t)settings_get(SETTINGS_ID_RAMP_MS))`**, then
`keyer_init()`, `lcd_init()` and the rest as today.

Main loop, forever:

1. Up to 4 button events into `menu_handle()` (unchanged).
2. **`menu_poll()`**
3. **`sidetone_poll()`**
4. For a pending redraw: `menu_render()`.
5. `lcd_flush_step()`.

### 10.2 Menu interface

`inc/menu/menu.h` adds:

```c
#ifndef MENU_TONE_PREVIEW_MS
#define MENU_TONE_PREVIEW_MS 300U  /* ui-ux.md owns the value; proposed default */
#endif

/** Runs the time-based menu work: the Tone preview timeout. Main loop, each pass. */
void menu_poll(void);
```

`menu_poll()` reads `tick_now()` only while a preview runs, once per call.

`menu.c` includes `sidetone/sidetone.h` and calls only `sidetone_set_tone_hz()`, `sidetone_set_ramp_ms()` and
`sidetone_set_on()`. It does not call `sidetone_init`, `sidetone_poll` or `sidetone_service`. The Ramp preview hooks
are in `menu_subsystem_spec.md` section 18.5.

### 10.3 Preview hooks (filled in by `ui-ux-embedded-designer`)

`ui-ux-embedded-designer` resolves Q-3, updates O-3 and specifies what each hook does in `ui-ux.md`, with V-tests.
The hooks are the only places where the menu touches the sidetone.

| Hook | When | Proposed default |
| --- | --- | --- |
| H1 | The EDIT screen opens on the Tone item. | `sidetone_set_tone_hz(current)`, then `sidetone_set_on(true)`, preview time = now. |
| H2 | An edit step changes the Tone value (`settings_set` returns true). Press or repeat event. | `sidetone_set_tone_hz(new)`, `sidetone_set_on(true)`, preview time = now (restart). |
| H3 | An edit step at a limit changes nothing. | No call. The timer keeps running. |
| H4 | `menu_poll()` sees `MENU_TONE_PREVIEW_MS` passed since the preview time. | `sidetone_set_on(false)`. The preview ends. |
| H5 | The EDIT screen of the Tone item ends by any path: ENTER, MENU, or `menu_show_message()` over the edit. | `sidetone_set_on(false)`, exactly once per exit. A preview that already timed out gets this call too. The preview ends. |
| H6 | Any other screen, item or edit. | No sidetone call. |

Ramp item (`ui-ux.md` section 6.5, UX-73, D-RM8): the hooks H1-H6 apply to the Tone item and to the Ramp item. One
preview state, one start time and `MENU_TONE_PREVIEW_MS` serve both. A Ramp hook that starts the tone (H1, H2) calls
`sidetone_set_ramp_ms(value)`, then `sidetone_set_tone_hz(settings_get(SETTINGS_ID_TONE_HZ))`, then
`sidetone_set_on(true)`. The Tone hooks stay as above and call no `sidetone_set_ramp_ms()`. H3 to H5 are the same
for both items. `menu_subsystem_spec.md` section 18.5 has the menu code contract.

Constraints for the designer:

- The buttons give press and repeat events only. There is no release event. A rule like "play after release"
  needs a buttons change. Report it to `architector` first.
- Every tone change while sounding costs one fall, the wait for the carrier update and one rise. Each ramp is
  the set ramp, 4 to 12 ms: about 4 + 2.5 + 4 ms to 12 + 2.5 + 12 ms at 400 Hz. With the 80 ms repeat, a held
  button gives a chain of short tones.
- A ramp change while sounding causes no fall and no rise (section 6.8).
- The sidetone itself adds no delay rule. The tone sounds at most `MENU_TONE_PREVIEW_MS` plus one fall at the set
  ramp.
- Call order within one hook: `sidetone_set_tone_hz()` before `sidetone_set_on(true)`, so the first rise already
  uses the new tone.

## 11. Build and static checks

- The `src/` glob of `firmware/CMakeLists.txt` already takes the new files. Add `--norestartseqatomics` to
  `target_compile_options` of the firmware (section 20). No other firmware CMake change.
- `tools/check_asm.sh` R5: add `P1` 0x90, `P1M1` 0x91, `P1M0` 0x92, `TMOD` 0x89 and `AUXR` 0x8E (only `ORL` or `ANL`
  byte writes). Add predefined names `p1`, `p1m1`, `p1m0`, `tmod`, `auxr`. Update the header comment.
- Self-tests: for each new register one accept case (an `orl` or `anl`) and one `r5_` reject case (a `mov`).
- Existing rules R1-R4 apply to the new files unchanged. `sidetone.c` must pass them.

## 12. Host test infrastructure

### 12.1 Fake platform header `Tests/fakes/board/sidetone_pwm.h` and `fake_sidetone_pwm.c/.h`

The fake header includes `board/sidetone_config.h` and `fake_sidetone_pwm.h`, and maps each macro of section 5.5 to
a fake function. The fake models these parts of the hardware:

- Preload and shadow registers for ARR, CCR1 and CCR2. A set writes the preload value.
- `SIDETONE_PWM_CONFIGURE(arr)` sets the preload and shadow ARR to `arr`, both compares to 0, ENO to 0, and clears
  the flag. It is one event in the log, not a `GENERATE_UPDATE()` call.
- `GENERATE_UPDATE()` copies the preloads to the shadows and sets the update flag (URS = 0 in hardware).
- `fake_sidetone_pwm_update_event()` (test call) models a counter overflow: preload to shadow, flag set.
- `CLEAR_UPDATE_FLAG()` clears the flag. `IS_UPDATE_FLAG_SET()` returns it.
- ENO value. Each ENO write that is not 0x00, 0x02 or 0x04 counts as a violation. A write of a non-zero ENO while
  the shadow compare of that channel is not 0 also counts as a violation (a click at enable).
- The plug pin: the test sets "headphones inserted" or not. `IS_HEADPHONES()` returns it.
- An ordered event log of every macro call with its value (configure, pins, timer start, ARR, CCR speaker,
  CCR headphones, UG, flag clear, flag read, ENO, preemption point). Log size at least 1024 events. An overflow
  counts as a test failure, not a silent drop.
- A count of register writes before `SIDETONE_PWM_CONFIGURE()`.
- `SIDETONE_PREEMPTION_POINT()` counts the points. With a test hook set, it also calls the hook.

Minimum fake API. The names are binding. The developer can add more. `fake_sidetone_pwm_event_t` is a struct with
`uint8_t kind` and `uint16_t value`. The kinds are `FAKE_SIDETONE_PWM_EVENT_*` constants.

```c
void fake_sidetone_pwm_reset(void);
void fake_sidetone_pwm_set_headphones(bool is_inserted);
void fake_sidetone_pwm_update_event(void);
void fake_sidetone_pwm_set_preemption_hook(void (*hook)(void));
size_t fake_sidetone_pwm_preemption_count(void);
size_t fake_sidetone_pwm_configure_count(void);
size_t fake_sidetone_pwm_pins_configure_count(void);
size_t fake_sidetone_pwm_timer_start_count(void);
size_t fake_sidetone_pwm_writes_before_configure(void);
uint16_t fake_sidetone_pwm_configured_arr(void);
uint16_t fake_sidetone_pwm_arr(void);              /* preload */
uint16_t fake_sidetone_pwm_shadow_arr(void);
uint16_t fake_sidetone_pwm_compare_speaker(void);  /* preload */
uint16_t fake_sidetone_pwm_compare_headphones(void);
uint16_t fake_sidetone_pwm_shadow_compare_speaker(void);
uint16_t fake_sidetone_pwm_shadow_compare_headphones(void);
uint8_t fake_sidetone_pwm_eno(void);
bool fake_sidetone_pwm_is_update_flag_set(void);
size_t fake_sidetone_pwm_update_count(void);       /* UG calls */
size_t fake_sidetone_pwm_violation_count(void);
size_t fake_sidetone_pwm_event_count(void);
fake_sidetone_pwm_event_t fake_sidetone_pwm_event_at(size_t index);  /* kind and value */
```

### 12.2 `Tests/fakes/fake_sidetone.c/.h` (menu tests)

Implements the functions of `sidetone.h` for the menu group. It keeps an ordered call log (kind and value). The log
has the calls of `sidetone_init`, `sidetone_set_tone_hz`, `sidetone_set_ramp_ms` and `sidetone_set_on`. The fake
counts the `sidetone_poll` calls. `sidetone_is_busy()` returns a value that the test sets. Accessors: reset, call
count, call at index, last tone, last ramp, last on value.

Additions of D-RM8: `FAKE_SIDETONE_CALL_RAMP 4U` (value: ramp in ms, as passed, not clamped) and
`uint8_t fake_sidetone_last_ramp(void)` (0 without a call). `sidetone_init(tone, ramp)` logs one `INIT` entry with
the tone, as before, and stores the ramp as the last ramp. The fake defines no `sidetone_ramp_calls`.

### 12.3 `Tests/CMakeLists.txt`

- New pattern `SIDETONE_TEST_PATTERN "^test_(qa_)?sidetone"` with `sidetone_group`: `src/sidetone/sidetone.c`,
  `fakes/fake_sidetone_pwm.c`, `fakes/fake_tick.c`, `src/settings/settings.c` and the EEPROM sources (for the
  range cross-check AC-11). `TARGET_DEFINITIONS`. Add it to `MODULE_TEST_PATTERNS`.
- `menu_group` adds `fakes/fake_sidetone.c`.
- The ISR in `sidetone.c` is inside `#if defined(__SDCC_mcs51)`, so the host group compiles without it.
- All host flags stay as they are.

## 13. Acceptance criteria

Each criterion maps to a host test or a review. Tests: `test_sidetone.c` (developer), `test_qa_sidetone*.c` (QA),
menu tests (developer, QA). Every sidetone test ends with `fake_sidetone_pwm_violation_count() == 0`.

| ID | Criterion | Evidence |
| --- | --- | --- |
| AC-1 | Silent init. After `sidetone_init(600, 6)`: one PWM configure with ARR 19999, then pins configure, then one timer start, in this order. No register write before the configure. ENO 0, both compares 0. State OFF, `sidetone_is_busy()` false. | test |
| AC-2 | Duty table. A full rise at 1000 Hz and ramp 8 ms writes `12 * p` for each of the 33 values of section 6.1. The values are strictly increasing: 0 at the start, 6000 (50 %) at STEADY. | test |
| AC-3 | Rise timing, for each ramp N = 4..12. With the on request: the first call writes compare 0, UG, then the ENO bit of the route, state RISING. The next 4N calls write `active[min(j * I, 2048) >> 6]`, j = 1..4N, one per call. After call 4N + 1 the state is STEADY, not earlier. STEADY with the request held writes nothing. | test |
| AC-4 | Fall timing, for each ramp N = 4..12. From STEADY with the request cleared: exactly 4N calls to ZERO_WAIT. The indexes never go up, and the last write is `active[0]`. The flag clear comes after a compare 0 write in the log. Then ZERO_WAIT, ENO unchanged. | test |
| AC-5 | ZERO_WAIT. Without the flag, the state stays and busy is true. After `update_event()`, the next call writes ENO 0 and the state is OFF. At that ENO write, the shadow compare of the active channel is 0. | test |
| AC-6 | ZERO_WAIT limit. With no update event, the state becomes OFF with ENO 0 after exactly `SIDETONE_ZERO_WAIT_LIMIT` (12) calls in ZERO_WAIT. | test |
| AC-7 | Reversal at every position, for ramps 4, 6, 8 and 12 ms. The test reverses after each rise call and after each fall call, and in STEADY. After a request change, the next write is one increment in the new direction, never a jump. Two compare writes in a row differ by at most `ceil(I / 64)` table indexes (exactly one at 8 ms). Off right after the start call goes to ZERO_WAIT. | test, QA |
| AC-8 | Stop, restart and replace in RISING, STEADY, FALLING and ZERO_WAIT give the behavior of section 6.3. | test, QA |
| AC-9 | Retune. Sounding at 600 Hz, `sidetone_set_tone_hz(800)`: no ARR write until OFF. The envelope falls, waits for the flag and goes OFF. The next call writes ARR 14999, compare 0, UG and ENO. The rise uses the 800 Hz table (compare[32] = 7500). Retune requested in each state behaves as section 6.3. | test, QA |
| AC-10 | Tone math. ARR and compare[1], [16], [32] equal the table of section 6.1 for 400, 600, 605, 610, 800, 990 and 1000 Hz. | test |
| AC-11 | Clamping. 0, 1 and 399 give the 400 Hz values. 1001, 5000 and 65535 give the 1000 Hz values, also through `sidetone_init`. `SIDETONE_TONE_MIN_HZ == settings_min(SETTINGS_ID_TONE_HZ)` and `SIDETONE_TONE_MAX_HZ == settings_max(SETTINGS_ID_TONE_HZ)`. | test |
| AC-12 | Same tone. `sidetone_set_tone_hz()` with the last requested (clamped) value in STEADY gives no fall, no pending flag and no preemption point. | test |
| AC-13 | Route. Headphones inserted (debounced) gives ENO 0x04 and writes to CCR2 only. Removed gives ENO 0x02 and CCR1 only. A plug change during a tone keeps the route until OFF. The next start uses the new route. A change shorter than `SIDETONE_PLUG_DEBOUNCE_MS` changes nothing. The debounce takes exactly `SIDETONE_PLUG_DEBOUNCE_MS` and works across the tick wrap. One `tick_now()` per `sidetone_poll()`. | test, QA |
| AC-14 | ENO safety. In every test, every ENO write is 0x00, 0x02 or 0x04, and no output is enabled with a non-zero shadow compare. The other channel's compare stays 0. | fake check in all tests |
| AC-15 | Mailbox tear. A hook runs `sidetone_service()` (on request set, in each state) at every preemption point of `sidetone_set_tone_hz()`. Then every rise uses an ARR and 33 values of one tone only, the old one or the new one. After the writer returns and the next OFF start comes, the new tone is used. Also with two writer calls in a row before the ISR uses the first. | QA |
| AC-16 | Busy. `sidetone_is_busy()` is true from `sidetone_set_on(true)` until OFF with the request cleared, and false only then. | test |
| AC-17 | No UG in a ramp. The count of `GENERATE_UPDATE()` calls equals the number of starts from OFF. ARR writes happen only in the OFF start call. | test |
| AC-18 | Menu preview. The designer's V-tests for H1-H6 pass with `fake_sidetone.c`, for the Tone item (V-40 to V-53) and the Ramp item (V-185 to V-191). They cover: start at the edit, restart on a change, nothing at a limit, stop on leave, stop on timeout (fake tick). No other screen calls the sidetone. `sidetone_set_tone_hz` comes directly before `sidetone_set_on(true)` in each hook. On Ramp, `sidetone_set_ramp_ms` comes first. | menu tests |
| AC-19 | No regression. All existing host tests pass. Allowed test changes are only those of section 24.7. `firmware/test.sh` passes, with the `check_asm.sh` self-test. | `test.sh` |
| AC-20 | Build. `firmware/build.sh` passes with `check_asm.sh`, including the new R5 registers and their self-tests. | `build.sh` |
| AC-21 | ISR assembly review. `sidetone_timer0_isr` calls only `_sidetone_service`. `sidetone_service` has no `lcall` and no library helper. It has no `P_SW2` access, no `EA` or `IE` byte write and no port 2 write. Its locals are in DSEG, not OSEG. The ISR ends with `reti`. No `.asm` file of the build has an `sdcc_atomic` symbol. | `.asm` review, QA |
| AC-22 | ISR time. The report gives the worst-case cycle count of the ISR: entry, OFF + on with a pending tone, exit. The count uses the `.asm` and the STC8H instruction timing. Target at most 400 clocks, hard limit 800 clocks (of 6000 per 250 us). | report |
| AC-23 | Memory. The report of section 14 is complete and within the hard limits. | report |
| AC-24 | Start-up and loop order in `main.c` are as in section 10.1. | review |
| AC-25 | Ramp 8 ms gives the old envelope. With `sidetone_init(tone, 8)`, every compare write, ENO write, UG and flag clear of a full rise, a full fall, every reversal of AC-7 and AC-9 is the same as in the first design (one table step per call). | test |
| AC-26 | Ramp clamp. `sidetone_set_ramp_ms()` and `sidetone_init()` with 0, 1, 3 give the 4 ms behavior. 13, 100 and 255 give the 12 ms behavior. `sidetone_ramp_calls` is 4 x the clamped value for 0..255. | test |
| AC-27 | Ramp change during a ramp. A change of the ramp at each call of a rise and of a fall (4 -> 12 ms and 12 -> 4 ms): the position never jumps, the indexes stay monotonic in the current direction, STEADY and ZERO_WAIT come at the end, and no ENO violation occurs. A change in OFF, STEADY or ZERO_WAIT writes no register and changes no state. | test, QA |
| AC-28 | Range cross-check. `SIDETONE_RAMP_MIN_MS == settings_min(SETTINGS_ID_RAMP_MS)` and `SIDETONE_RAMP_MAX_MS == settings_max(SETTINGS_ID_RAMP_MS)`. | test |
| AC-29 | Preemption of the ramp writer. A hook runs `sidetone_service()` at each preemption point of `sidetone_set_ramp_ms()` in RISING and FALLING. The envelope ends at STEADY or ZERO_WAIT with no ENO violation. | QA |
| AC-30 | ISR assembly for the ramp. `sidetone_service` keeps AC-21: no `lcall`, no library helper (the shift by 6 and the 16-bit add and subtract are inline). `sidetone_set_ramp_ms` has no `lcall` and no multiply. `keyer_service` and `player_service` read `_sidetone_ramp_calls` directly. `check_asm.sh` R6 and R7 pass. | `.asm` review, QA |

## 14. Memory budget and report

Last known baseline (`build/putikeeg_cw_trainer.mem`): flash 14 441 / 61 440 B, xdata 1193 / 3072 B, stack start
0x2C with 212 B free. T0 measures it again.

| Resource | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- |
| Flash added by the whole change | at most 2048 B | 3072 B |
| xdata added | about 150 B (final review measured 153 B, accepted) | 192 B |
| New static internal RAM: DSEG + overlay + bit bytes (the stack start moves up by this) | at most 8 B | 16 B |
| Stack margin: free stack minus (deepest main-loop use + largest ISR frame) | | at least 64 B |
| Timer 0 ISR, worst case | 400 clocks | 800 clocks |

Report table from `firmware/build.sh`, `.map` and `.mem`, before and after each step:

| Step | Flash B | DSEG B | Overlay B | Bit bytes | xdata B | Stack start | Stack free B | Deepest main-loop stack B | Largest ISR frame B | Margin B |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |

Also report:

- Flash of `sidetone.c` and the newly linked library helpers (for example
  `_mullong`, `_divulong`).
- The stack estimate method. Count from the generated `.asm`: each `lcall` adds 2, each `push` adds 1, an ISR entry
  adds 2 plus its pushes. Name the deepest main-loop call chain and the largest ISR frame.
  Candidates: `menu_handle` -> `sidetone_set_tone_hz` -> `__divulong`, and `menu_render` -> `gfx_draw_text` ->
  `font_u8g2_draw_glyph` -> `lcd_fb_set_pixel`. Timer 0 and Timer 3 have the same priority, so only the larger
  frame counts.
- The main-loop time of one `sidetone_set_tone_hz()` call, estimated from the asm (33 multiply-divide operations).

## 15. Tasks

`c-embedded-developer` owns T0-T5. Each step ends with: `firmware/build.sh` passes with `check_asm.sh`,
`firmware/test.sh` passes, the memory table, the list of assumptions and the open device checks.

### D1 Preview interaction (`ui-ux-embedded-designer`)

- Inputs: section 10, `ui-ux.md` Q-3 and O-3, the user decision (preview is the only trigger).
- Outputs in `ui-ux.md`: Q-3 resolved, O-3 updated, the behavior of H1-H6, the value of `MENU_TONE_PREVIEW_MS`,
  new V-tests (from V-40) with probes that `fake_sidetone.c` can observe, new device checks for the preview feel.
- Runs in parallel with T0-T3. T4 waits for D1.
- Acceptance: `architector` review. No behavior that the constraints of section 10.3 forbid.

### T0 Baseline

- Rebuild with `firmware/build.sh`, run `firmware/test.sh`. Record the memory table and the baseline stack
  estimate.

### T1 Platform, contract and fakes

- Outputs:
  - `compiler.h` and `stc8h.h` additions, `board/sidetone_config.h`, `board/sidetone_pwm.h`.
  - `inc/sidetone/sidetone.h` and `src/sidetone/sidetone.c` with signature stubs. The stub bodies are silent and
    build.
  - The fake header, `fake_sidetone_pwm.c/.h`, `fake_sidetone.c/.h`, the CMake groups.
  - The `check_asm.sh` R5 extension with self-tests.
- Acceptance: both scripts pass. Datasheet citations for every value of sections 7 and 8. QA can write tests
  against the headers and fakes from this point.

### T2 Sidetone core

- Inputs: sections 6, 9.
- Outputs: `sidetone.c` complete, `Tests/test_sidetone.c`.
- Acceptance: AC-1 to AC-14, AC-16, AC-17 pass. Assembly of `sidetone_service` reviewed for AC-21.

### T3 ISR and `main.c`

- Outputs: the ISR at the end of `sidetone.c` (section 5.6), `main.c` changes of section 10.1.
- Acceptance: AC-20 to AC-22, AC-24. The ISR time in the report.

### T4 Menu hooks

- Inputs: D1 output, section 10.
- Outputs: `menu.h` (`menu_poll`, `MENU_TONE_PREVIEW_MS`), `menu.c` hooks, menu tests for the new V-tests.
- Acceptance: AC-18, AC-19.

### T5 Developer close-out

- The final report: all step tables, the stack analysis, the ISR cycle count, datasheet citations, test lists,
  assumptions, open device checks of section 17.

### Q1 QA (`qa-embedded`)

- Starts its tests after T1, verifies after T5.
- `Tests/test_qa_sidetone*.c`: fault injection for AC-7, AC-8, AC-9, AC-13 and AC-15:
  - Reversal at every index, retune in every state, plug change during a tone.
  - Mailbox writes preempted at every point, two writes before one use.
  - ZERO_WAIT without update events, tick wrap in the debounce.
- Independent review of every acceptance criterion, the `.asm` of the ISR path (AC-21) and the stack count.
- Verdict: `approved`, `changes_requested` or `blocked`.

### W1 Documentation (`technical-writer`, after the final `approved`)

- Create `references/internal/sidetone_validation.md` (already linked, missing) with the device-check procedure
  of section 17.
- Update `components_and_wiring.md` and `sidetone_pwm_envelope_research.md` where this design differs (EAXFR stays
  set and is not saved, Timer 0, ZERO_WAIT, double buffer, one step per call, file layout).
- Record the internal RAM placement rule (section 6.2) in the firmware notes.

## 16. Interaction with other rules

- The Timer 0 ISR can stop the LCD software SPI between any two pin changes. The SPI already tolerates this
  (`AGENTS.md`). The ISR does not touch port 2.
- `P1M0`, `P1M1`, `P1`, `TMOD` and `AUXR` change only with `ORL` or `ANL` (R5). Port-1 pins change with `SETB` or
  `CLR`.
- `P_SW2` keeps `EAXFR` = 1. The xdata RAM (0x0000-0x0BFF) does not overlap the XFR area.
- The ISR runs also in OFF (about one short test per call). Timer 0 stays on for the later keyer.

## 17. Device checks (unverified until the user tests on the device)

| ID | Check |
| --- | --- |
| DC-1 | Both routes stay quiet (HIGH) at power-on, after init, and in OFF. No click at power-on. |
| DC-2 | Polarity: P1.1 (PWM1N) and P1.2 (PWM2P) show LOW-active pulses, at most 50 % LOW in STEADY, HIGH at idle. This checks the reading of section 4.1. |
| DC-3 | No signal on P1.0, P1.3, P5.4 and the port 2 pins. The LCD works while the tone plays. |
| DC-4 | Tone frequency at 400, 600 and 1000 Hz (expected error: the oscillator trim only). |
| DC-5 | Ramp on the logic analyzer at Ramp 4, 6, 8 and 12 ms: rise and fall time equal the setting within one carrier period, first and last pulses, compare coalescing at 400 Hz. |
| DC-6 | No click at start, stop, retune and route change, on the speaker and on the headphones. |
| DC-7 | Plug insert and remove: the route follows at the next silence. P1.0 levels with the internal pull-up. Is the pull-up needed? |
| DC-8 | Timer 0 ISR time and jitter while the LCD redraws. The full LCD redraw time with the 4 kHz ISR active. |
| DC-9 | Stack high-water mark after a long run (fill the stack with a pattern at start-up, read it later). |
| DC-10 | Quiet current: no extra battery current from P1.1 and P1.2 push-pull HIGH (PNP stages fully off). |
| DC-11 | Preview feel: the designer's device checks from D1. |
| DC-12 | An EEPROM erase (4-6 ms, the CPU stops) during a fall or a rise only stretches the ramp. The PWM keeps the last compare, and the ramp goes on after the erase without a click. |
| DC-13 | Click by ear at Ramp 4 ms and 12 ms, at 400 Hz and 1000 Hz, speaker and headphones (with `ui-ux.md` D-46). More click at 4 ms and 400 Hz is expected (section 6.8, physics note). Record the shortest ramp with no click at each tone. |
| DC-14 | Keyer and player at 50 WPM with Ramp 12 ms (with `ui-ux.md` D-47): dits sound, element and gap times hold, the fall ends at or one call before the mark end (D-RM5). |

## 18. Risks

| Risk | Handling |
| --- | --- |
| The scanned output-control table is garbled. The PWM1N polarity reading can be wrong. | Section 4.1 reading, DC-2. In OFF the pin is a GPIO and stays HIGH. A wrong polarity drives the pin LOW at compare 0 while the output is on, so the start clicks and the tone is inverted. DC-2 and DC-6 show it at once. |
| The MCU supply is possibly not the PNP emitter rail. Then a push-pull HIGH half-opens a PNP stage. | Netlist evidence (section 4). DC-10. Fallback: open-drain mode (P1M1 = P1M0 = 1), an `architector` decision. |
| ISR load and jitter for the later keyer. | Double buffer (section 3.1), AC-22, DC-8. |
| Stack depth with a second ISR frame. | Section 14 margin rule, DC-9. |
| EAXFR cleared by a future module silences PWMA writes. | Rule in section 9, ZERO_WAIT limit (section 3.4). |
| Plug debounce value without measurement. | `-D` value, DC-7. |
| Retune chain during auto-repeat sounds choppy. | Designer decision D1, DC-11. |

## 19. Decisions for the user

No blocking decision is open. Items to tell the user:

- The plan refinements of section 3. None changes a user decision.
- Netlist observation, outside this change: the exported netlist shows the MCU `Vcc` pin on net `VCC` (the
  battery side, also the HT7833 input), and the `+3.3V` net only on two capacitors. The menu spec (Q-5) assumes the
  MCU runs from the HT7833 3.3 V output, and sets the low-voltage detector to 3.0 V for that. If the netlist is
  right, that reasoning needs a review. The schematic export reports annotation errors, so the netlist can be
  wrong too.

## 20. Decisions after developer round 1 (2026-10-05)

Round 1 result: `test.sh` 36/36, `build.sh` passes. Flash 14 441 -> 15 845 B, DSEG 30 -> 32 B, xdata 1193 -> 1344 B,
stack margin 178 -> 160 B. All are within the limits of section 14.

- D-R1-1, ISR epilogue. SDCC 4.6.0 ends an ISR that calls a function with `ljmp sdcc_atomic_maybe_rollback`. This
  library routine rolls back an interrupted C11 atomic sequence and then does `reti`. The leaf `tick_isr` already
  ends with a plain `reti`. No source uses C11 atomics, so the rollback never acts. It only adds clocks and stack
  bytes to each 4 kHz call. Decision: add `--norestartseqatomics` to the firmware compile options. AC-21 keeps its
  strict form and gets the `reti` check. `<stdatomic.h>` is not allowed in the firmware. The developer reports the
  flash, stack and ISR clock change of this flag.
- D-R1-2, xdata. 151 B is accepted. The extra 6 B are working variables of `sidetone_set_tone_hz()` in xdata, which
  keeps DSEG small. The separate debounced byte goes away: `is_headphones_requested` is the debounced value
  (sections 6.2 and 6.6). No behavior changes.
- D-R1-3, H5. `on(false)` comes exactly once on every exit from the Tone EDIT screen. The exits are ENTER, MENU and
  `menu_show_message()` over the edit. A preview that already timed out gets this call too. A second
  `on(false)` is one bit write and has no effect on the sidetone. This matches `ui-ux.md` section 6.5, V-47 and
  V-48.
- D-R1-4, Q-4. The menu spec (sections 3.5 and 3.6) resolved it: `eeprom_write` reads back each byte, and a failed save
  shows `Not saved` / `OK`. `ui-ux.md` still lists Q-4 as open and F-7 as "not detectable". The owner
  `ui-ux-embedded-designer` updates both entries. This does not block round 2.

## 21. Decision after developer round 2 (2026-10-05)

- D-R2-1, ISR layout. This replaces the layout part of D-R1-1. An ISR can call a function of another file. With
  `--norestartseqatomics`, SDCC 4.6.0 still ends that ISR with `ljmp sdcc_atomic_maybe_rollback`. The flag also drops
  the support routines, so the link fails. Decision: the ISR moves to the end of `sidetone.c` inside
  `#if defined(__SDCC_mcs51)`. `sidetone_timer0.c` is deleted. The flag stays. Measured: plain `reti`, no
  `sdcc_atomic` symbol, `tick_timer3.c.asm` unchanged, flash 16 034 -> 15 923 B, RAM and stack unchanged, ISR about
  12 clocks faster.
- Rule for later work: an ISR calls only functions of its own file. If an ISR must call a function of another
  file, the build fails at the link. The later keyer change then decides the layout again (one ISR file with all
  services, or the flag removed and AC-21 changed). Only `architector` makes that decision.
- D-R2-1 revision for the keyer change, decision D-K1 of `keyer_spec.md` section 3.1. The keyer service is a
  `static` function in `inc/cw/keyer_service.h`. Only `sidetone.c` includes that header, at the end of the file, so
  the function belongs to `sidetone.c`. The ISR body becomes `keyer_service(); sidetone_service();`. This replaces
  "calls `sidetone_service()` and nothing else" in sections 5.4 and 5.6. The macros `KEYER_SIDETONE_ON()` and
  `KEYER_SIDETONE_OFF()` in `sidetone.c` are the ISR-side entry that section 3.10 reserved. The flag stays, and the
  rule "an ISR calls only functions of its own file" stays.

## 22. Final review decisions (2026-10-05)

QA verdict `approved`. `architector` checked again: `test.sh` 39/39 and 145/145 `check_asm` self-test cases,
`build.sh` passes, flash 15 923 B, xdata 1346 B, stack start 0x30 with 208 B free. `sidetone_service` has no
`lcall` and no library helper. The ISR ends with `reti`. No `.asm` file has an `sdcc_atomic` symbol.

- D-R3-1, mailbox store order. The tables and the ARR values are not `volatile`. C does not keep the order of a
  non-volatile store against the volatile `is_tone_pending = true`. The order holds only in the SDCC 4.6.0 output.
  Decision: make both tables and both ARR values `volatile` (also the pointee of the fill pointer). The developer
  measures the result. When flash grows by at most 32 B and the ISR worst case does not grow, keep it. Else revert
  it, and the `.asm` check of the store order becomes a fixed step of each review after a compiler or flag change.
- D-R3-2, `requested_hz` is stored before the fill. This has no visible effect (section 6.4, step 6).
- D-R3-3, xdata 153 B is accepted. It is under the hard limit of 192 B.
- D-R3-4, the new device check DC-12 (IAP erase during a ramp) comes from QA.
- Result of D-R3-1: `volatile` is kept. It costs 0 B of flash and changes no ISR instruction. The `.asm` shows
  the table fill and both ARR stores before `setb _sidetone_is_tone_pending`.
- Final verdict: `approved`. Flash 14 441 -> 15 913 B, xdata 1193 -> 1346 B, internal RAM +4 B, stack start 0x30
  with 208 B free. Device behavior DC-1 to DC-12 stays unverified until the user runs the device checks.

## 23. Device-check notes from `technical-writer` (2026-10-05)

These notes do not change the scope. `references/internal/sidetone_validation.md` has the procedure.

- DC-8 (ISR time) and DC-9 (stack high-water mark) need a measurement build with an ISR marker pin and a stack
  fill. This change has no such build, so both are recorded as "not run".
- DC-12 (EEPROM erase during a ramp) cannot happen in this change. A save needs three debounced button events, and
  a fall lasts only 8 ms. The check waits for the keyer change or a measurement build.
- No measurement firmware is part of this change.

## 24. Change request: ramp duration setting (2026-10-16)

This section is the binding contract of the change. Where it differs from sections 1 to 23, this section wins.
Plan: `noble-bubbling-dahl.md` (user-approved). Related contracts: `keyer_spec.md` section 18 (keyer early release),
`training_spec.md` D-T51 (player early release, `timing.h` check), `menu_subsystem_spec.md` section 18 (setting,
record layout version 3, scrolling, Ramp preview). UI decisions: `ui-ux.md` UX-71 to UX-73, sections 5.3, 5.5,
6.5, 9.6.

### 24.1 Scope and user decisions

User decisions (fixed):

- A `Settings > CW` item sets the rise and fall time of the sidetone: 4 to 12 ms, step 1 ms, default 6 ms. One value
  sets both edges of every tone: the previews, Free Input, Receiving and Sending.
- The service rate (4 kHz) and the 32-step duty table stay. `sidetone_config.h` keeps its timer and PWM values.
- The CW list gets 8 items, so the menu scrolls as `ui-ux.md` section 5.5 says.
- The Ramp edit plays a preview, as the Tone edit does.
- "Done" means: host tests pass, QA approves, `architector` approves. Device behavior stays unverified until the
  user tests on the device.

Out of scope: a change of the service rate, of the duty table, of the timer or PWM setup; separate rise and fall
values; a restart of the tone on a ramp change; any change in `firmware/drivers/`.

### 24.2 Decisions

| ID | Decision | Reason |
| --- | --- | --- |
| D-RM1 | Envelope position (sections 3.2, 6.2, 6.3, 6.8). The 8-bit index becomes `static MEM_DATA uint16_t` position in 1/64 of a table step, 0 to 2048. The table index is `position >> 6`. A `static volatile MEM_DATA uint8_t` increment `ceil(512 / N)` comes from a 9-entry `MEM_CODE` table. A full rise and a full fall take exactly `4 * N` calls. | Plan design. One byte of increment, no division or multiply in the main loop or the ISR. 8 ms keeps the old envelope bit for bit (AC-25). |
| D-RM2 | API. `void sidetone_init(uint16_t tone_hz, MEM_XDATA uint8_t ramp_ms)` and `void sidetone_set_ramp_ms(uint8_t ramp_ms)`. The sidetone clamps to `SIDETONE_RAMP_MIN_MS..SIDETONE_RAMP_MAX_MS` (4, 12), defined in `sidetone_config.h` next to the tone range. AC-28 ties them to the settings range. | Same pattern as the tone: the sidetone owns its limits, the settings module owns the grid and the default. The second parameter of a non-leaf main-loop function is `MEM_XDATA` (section 9), so it costs no internal RAM. `sidetone.h` includes `mcu/compiler.h` for it. |
| D-RM3 | Shared call count. `volatile MEM_DATA uint8_t sidetone_ramp_calls`, defined in `sidetone.c`, declared in the new header `inc/sidetone/sidetone_shared.h` (section 5.7). `cw/keyer_shared.h` and `cw/player_shared.h` include it and define `KEYER_EARLY_RELEASE_TICKS` and `PLAYER_EARLY_RELEASE_TICKS` as `sidetone_ramp_calls`. No static initializer: `sidetone_init()` writes it. | The ISR reads shared state as memory, not through a call (`llm.md`). The keyer and player host groups link `src/sidetone/sidetone.c` already (`Tests/CMakeLists.txt`), so their tests link with no new fake. The macro names stay, so `keyer_service.h` and `player_service.h` change only their comments. Files that only include the shared headers (`free_input.c`, `sending.c`, their tests) do not read the byte, so the menu group needs no definition. |
| D-RM4 | A ramp change only changes the speed. No fall, no restart, no pending flag. | No ISR code for a restart. The user decision UX-73 accepts a steady tone during a held Ramp repeat. |
| D-RM5 | The speed check. `timing.h` stops the build when the dit at `CW_TIMING_WPM_MAX` is shorter than `2 * SIDETONE_RAMP_MAX_CALLS` (96 < 96 is false, so 50 WPM and 12 ms pass), and when it is not longer than `SIDETONE_ZERO_WAIT_CALLS`. Result at exactly 50 WPM and 12 ms: a dit has 47 rise calls before the release, not 48. The rise reaches position 2021 (index 31, duty 49.9 % instead of 50 %). The fall then takes 47 calls and enters ZERO_WAIT one call before the last mark call. Every other speed and ramp reaches index 32, and its fall enters ZERO_WAIT on the last mark call. | `architector` checked all 9 ramps at 50 WPM with a script. The duty is 499 instead of 500 permille. That level change is not audible, and it gives no click. A strict check would forbid the user range 4 to 12 ms at the user maximum 50 WPM. The plan statement "the dit still reaches full level" is one step too high. |
| D-RM6 | `sidetone_set_ramp_ms()` is main loop only. Only `sidetone_init()` and the menu Ramp hooks (H1, H2) call it. The keyer and the player are disabled at both times. No mode entry calls it. | The two byte stores are not atomic as a pair (section 6.8). With this rule no mark ever sees a mixed pair. The sidetone always has the Ramp setting value: `sidetone_init()` gets it, and each Ramp edit step sends it. |
| D-RM7 | ZERO_WAIT counts its calls in the position variable, as it counted in the index byte. | No extra RAM. |
| D-RM8 | Menu and fakes. `fake_sidetone.c` logs `sidetone_set_ramp_ms()` as kind `FAKE_SIDETONE_CALL_RAMP` (4) with the value as passed. The Ramp hooks call `ramp(value)`, `tone(Tone)`, `on(true)` (UX-73). The Tone hooks are unchanged. | The designer's order keeps the rule "each `on(true)` comes directly after a `tone()`", so the existing Tone tests and their check helpers stay. The extra `tone()` call costs a few bytes and does nothing audible. |
| D-RM9 | The row "RISING, not `is_up`, P = 0" stays without a compare write. | It keeps the old behavior and the old tests of "off right after the start call". |
| D-RM10 | `MENU_TONE_PREVIEW_MS` keeps its name. It serves Tone and Ramp. | A rename changes code, tests and `ui-ux.md` for no behavior gain. |

### 24.3 Files

| File | Change |
| --- | --- |
| `inc/board/sidetone_config.h` | `SIDETONE_RAMP_MIN_MS`, `SIDETONE_RAMP_MAX_MS`, `SIDETONE_RAMP_CALLS_PER_MS`, `SIDETONE_RAMP_MAX_CALLS` and the checks of section 5.3. Nothing else changes. |
| `inc/sidetone/sidetone.h` | Section 5.4: `sidetone_init()` signature, `sidetone_set_ramp_ms()`, the STEADY comment, `#include "mcu/compiler.h"`. |
| `inc/sidetone/sidetone_shared.h` (new) | Section 5.7. SPDX lines first. |
| `src/sidetone/sidetone.c` | Sections 6.2, 6.3, 6.7, 6.8. The duty table stays. |
| `inc/cw/keyer_shared.h`, `inc/cw/player_shared.h` | The early release macros (D-RM3). |
| `inc/cw/keyer_service.h`, `inc/cw/player_service.h` | Only the comments that say "32" (now "the ramp calls"). No code change. |
| `inc/cw/timing.h` | The checks of D-RM5 (`training_spec.md` D-T51). |
| `src/main.c` | Section 10.1. |
| `inc/settings/settings.h`, `src/settings/settings.c`, `src/menu/*`, `inc/menu/menu.h` | `menu_subsystem_spec.md` section 18. |
| `Tests/fakes/fake_sidetone.c/.h` | Section 12.2 (D-RM8). |
| Tests | Section 24.7. |

### 24.4 Memory and time budget

Baseline (`build/putikeeg_cw_trainer.mem`, 2026-10-15): flash 28 561 / 61 440 B, xdata 1818 / 3072 B, stack start
0x41 with 191 B free. R0 measures it again.

| Resource | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- |
| Flash added by the whole change | at most 450 B | 700 B |
| xdata added | 3 B (window tops, `menu_subsystem_spec.md` 18.4) | 8 B |
| New static internal RAM (DSEG + overlay + bit bytes) | 3 B (position +1, increment +1, call count +1) | 4 B |
| Stack margin: free stack minus (deepest main-loop use + largest ISR frame) | | at least 64 B |
| Timer 0 ISR worst case | no growth of the worst path of section 13 AC-22 | 800 clocks |
| RISING and FALLING path of `sidetone_service` | at most +30 clocks against R0 | +60 clocks |

The report uses the table of section 14 and also gives the clock count of the RISING, FALLING and keyer ELEMENT
paths before and after.

### 24.5 Tasks

| Task | Owner | Content | Depends on |
| --- | --- | --- | --- |
| D-R | `ui-ux-embedded-designer` | Done (UX-71 to UX-73, `ui-ux.md` 5.3, 5.5, 6.5, 9.6, D-45 to D-47). `architector` accepted it in section 24.9. | - |
| R0 | `c-embedded-developer` | Baseline: `firmware/build.sh`, `firmware/test.sh`, memory table, stack estimate, clock counts of the paths of 24.4. | - |
| R1 | `c-embedded-developer` | Contract and stubs: every header change of 24.3 and `menu_subsystem_spec.md` 18.3, `sidetone_set_ramp_ms()` with a body that only clamps and writes, `sidetone_init()` with the new parameter, all call sites (`main.c`, tests) with the mechanical changes of 24.7, `fake_sidetone` additions, `SETTINGS_ID_RAMP_MS` in the tables. Both scripts pass. QA can write tests from here. | R0 |
| R2 | `c-embedded-developer` | Sidetone core: sections 6.2, 6.3, 6.8. Tests AC-2 to AC-4, AC-7, AC-25 to AC-28 in `test_sidetone.c`. `.asm` check for AC-30. | R1 |
| R3 | `c-embedded-developer` | Keyer and player: comments, `timing.h` checks. Tests AC-K13, AC-K14 (revised, `keyer_spec.md` 18) and AC-T12 (revised, `training_spec.md` D-T51). | R2 |
| R4 | `c-embedded-developer` | Settings layout version 3: `menu_subsystem_spec.md` 5.1, 5.2, 18.2. Tests AC-M1 to AC-M6 of 18.7. | R1 |
| R5 | `c-embedded-developer` | Menu: tree item, scrolling, `i/n`, Ramp preview: `menu_subsystem_spec.md` 6.1 to 6.3, 18.4 to 18.6. Tests V-173 to V-191 and AC-M7 to AC-M12. | R1, R4 |
| R6 | `c-embedded-developer` | Close-out: memory table of 24.4 before and after each task, stack estimate, ISR clock counts, `.asm` review notes, `pre-commit run --files <changed files>`, list of assumptions and open device checks. | R2 to R5 |
| Q-R | `qa-embedded` | Starts its tests after R1, verifies after R6 (24.6). Verdict `approved`, `changes_requested` or `blocked`. | R1, R6 |
| A-R | `architector` | Final review on top of Q-R. | Q-R |
| W-R | `technical-writer` | After A-R `approved`: user manual and quick guide (Ramp item, CW scroll), `docs/release_notes.md` after the user picks the version, `sidetone_validation.md` (DC-5, DC-13, DC-14), the implemented-design note of `sidetone_pwm_envelope_research.md`. | A-R |

Each developer task ends with: `firmware/build.sh` passes with `check_asm.sh`, `firmware/test.sh` passes, the memory
table, the assumptions, the open device checks.

### 24.6 QA checks (`qa-embedded`, `Tests/test_qa_*.c`)

1. Envelope for every ramp 4..12 ms: exact `4 * N` rise and fall calls from OFF and from STEADY, monotonic indexes,
   the step limit of AC-7, `active[32]` and `active[0]` at the ends. The 8 ms log equals the first design (AC-25),
   for example with the golden capture of `test_qa_golden_capture.c`.
2. Reversal at every position and a ramp change at every call (AC-7, AC-27), for 4 -> 12, 12 -> 4 and 6 -> 7 ms.
3. Preemption: AC-15 (tone mailbox) at ramps 4 and 12 ms, and AC-29 (ramp writer).
4. Keyer and player: the off request on the call where `4 * N` mark calls remain, for N = 4, 6, 8, 12 at 5, 20 and
   50 WPM, and Farnsworth 50/5. The fall enters ZERO_WAIT on the last mark call, or one call earlier only at 50 WPM
   and 12 ms (D-RM5). ZERO_WAIT ends before the next element. `KEYER_IS_MARK_ACTIVE()` and the player busy flag keep
   the full mark. No lost or late element.
5. The `timing.h` check: a scratch compile (not committed) with a changed copy shows that 13 ms, or a dit of 95
   calls, stops the build.
6. Settings: every vector of `menu_subsystem_spec.md` 18.2, the clamp of bits 4-7, flag independence, power loss
   during a version 3 save, version 4 as `SETTINGS_LOAD_BAD_VERSION`, no save at start-up after a version 1 or 2
   load.
7. Menu: the scroll rule for every cursor position in both directions, press wrap and repeat stop, `i/n` text and
   cells, push and pop of the window top, edit in the last window, Ramp preview hooks H1 to H6 with the call
   order of D-RM8, a message over the Ramp edit, no `ramp()` call outside the Ramp edit, the Tone tests unchanged.
8. `.asm` review: AC-21 and AC-30, `check_asm.sh` R6 and R7, no `sdcc_atomic`, the keyer and player read
   `_sidetone_ramp_calls` directly.
9. Review of the memory, stack and clock report against 24.4.
10. The device checks of 24.8 stay listed as unverified.

### 24.7 Allowed changes of existing tests

- Each `sidetone_init(tone)` call gets a second argument. Tests that expect the first design use 8.
- Uses of `SIDETONE_RAMP_STEPS` as a call count, and `EARLY_RELEASE 32U` in `test_keyer.c` and `test_player.c`,
  become the call count of the ramp under test (`sidetone_ramp_calls` or `4 * N`). The expected values at 8 ms do
  not change.
- Keyer and player tests call `sidetone_init()` or `sidetone_set_ramp_ms()` in their setup, because the call count
  has no static initializer.
- Settings tests: the layout version, the saved record bytes (version 3), and the "version 3 gives
  `SETTINGS_LOAD_BAD_VERSION`" vector, which becomes version 4 (`menu_subsystem_spec.md` 18.2).
- Menu tests: the CW item indexes after `Tone` move by one, the CW list screens get `i/n` and the window.
- Accepted on 2026-10-16 (D-RM12), because the new id or layout version 3 forces them: the per-id tables of
  `test_settings.c`; the `SETTINGS_ID_COUNT == 13` checks; the `_v2` arrays of `test_settings_paddle.c` renamed
  `_v3`; expected flags `| 0x60` in `test_settings_prosign.c`; the V-32 check `item_count <= 7` in `test_menu.c`
  replaced by a title-fit check; the V-52 loop of `test_menu_preview.c` over rows 2 to 6 (Ramp is a preview item);
  the compare 0 classification of the fall end in `test_keyer_envelope.c` (`keyer_spec.md` 18.2); comments that
  name 32 or version 2.
- No other change of an existing test. A test that needs another change is reported to `architector`.

### 24.8 Device checks (unverified until the user tests on the device)

DC-5 (revised), DC-13 and DC-14 of section 17, with `ui-ux.md` D-45 to D-47. D-45 also measures the full-screen
transfer of a window move. When it takes longer than 80 ms during a held button, `architector` decides on a
split of the transfer (`AGENTS.md`).

### 24.9 Review of the UI decisions (2026-10-16)

`architector` accepts UX-71 to UX-73 and `ui-ux.md` 5.3, 5.5, 6.5 and 9.6 for this contract:

- Item order in CW: `Tone`, `Ramp`, `RX Speed`, `RX Eff. Speed`, `TX Speed`, `TX Eff. Speed`, `Key Type`, `Paddle`.
- Window rule, `i/n` and the redraw pages: `menu_subsystem_spec.md` 6.2.
- Ramp hooks: D-RM8. The rename of `MENU_TONE_PREVIEW_MS` is rejected (D-RM10).
- Correction for the designer: D-47 says that each dit reaches full level at 50 WPM and 12 ms. It reaches index 31
  of 32 (D-RM5). The check itself stays.

### 24.10 Decisions on the R0-R6 results (2026-10-16)

| ID | Decision | Reason |
| --- | --- | --- |
| D-RM11 | Accepted: `sidetone_init()` does not call `sidetone_set_ramp_ms()`. Both use the private macro `SIDETONE_WRITE_INCREMENT` (clamp in place, increment store). `sidetone_init()` then stores `sidetone_ramp_calls` with no preemption point. | Same values as the function. `sidetone_set_ramp_ms()` stays a leaf (AC-30), and the preemption count of `sidetone_init()` does not change. Timer 0 is not running in init, so a preemption point there tests nothing. |
| D-RM12 | Accepted: the test changes listed in 24.7 under D-RM12. | Each one follows from the new id, layout version 3, the scroll rule or the fall-end definition. None weakens a check. |
| D-RM13 | Accepted: the table index is built from the two bytes of the position, `((P >> 8) << 2) \| ((uint8_t)P >> 6)`, which equals `P >> 6` for 0 to 2048. ZERO_WAIT counts in the low byte and stores the count back as the full position, so the high byte stays 0. | Shorter code than a 16-bit shift. The values of section 6.3 do not change. |
| D-RM14 | Budget results: flash +544 B (target 450 B, hard limit 700 B), xdata +5 B (target 3 B, hard limit 8 B), internal RAM +3 B, stack margin 131 B, RISING +24 and FALLING +29 clocks (limit +30), worst ISR path about +28 clocks (target "no growth", hard limit 800 clocks). Accepted when QA confirms the numbers. The final review checks where the flash and the 2 B of xdata above the targets go. | All values are inside their hard limits. The ISR growth is the price of the 16-bit position on the RISING and FALLING paths. The worst path stays far below 800 clocks. |

### 24.11 Risks

| Risk | Handling |
| --- | --- |
| A short ramp at a low tone clicks. | Physics note of 6.8, DC-13, D-46. The user picks the value. |
| The 16-bit position math makes the ISR slower. | 24.4 limits, AC-30. Constant shift, byte increment. |
| A full-screen redraw at each window move is slow over the software SPI. | D-45. A split of the transfer is an `architector` decision. |
| Earlier firmware reads a version 3 record as `SETTINGS_LOAD_BAD_VERSION` and uses its defaults (downgrade). | Accepted, as for version 2 (`keyer_spec.md` 17.4.2). The release notes say it. |

### 24.12 Final review (2026-10-16)

Verdict: `approved`, host evidence only. QA verdict Q-R: `approved`.

| Item | Result (checked again by `architector`) |
| --- | --- |
| `test.sh` | 88 of 88 tests pass. `check_asm.sh` self-test: 168 of 168. |
| `build.sh` | Passes. `check_asm.sh` reports no violation (R6, R7). No `sdcc_atomic` text in the build `.asm` files. |
| Memory | Flash 28 561 -> 29 105 B (+544, target 450, hard limit 700). xdata 1818 -> 1823 B (+5: window tops 3 B and two 1-byte xdata parameters; target 3, hard limit 8). Stack start 0x41 -> 0x44 (+3 B internal RAM), 188 B free. Stack margin at least 131 B. |
| ISR | `sidetone_timer0_isr` has exactly the three calls and ends with `reti`. `sidetone_service` and `sidetone_set_ramp_ms` have no `lcall` and no library helper. RISING 72 -> 94 clocks, FALLING 69 -> 98 clocks (QA count), worst ISR path about +26 clocks (D-RM14). |
| Contract | D-RM1 to D-RM15 hold in the code: envelope position, increment table, `sidetone_shared.h`, early release macros, `timing.h` checks, settings layout version 3 with the D-RM15 check, CW order `Tone`, `Ramp`, ..., `Paddle`, window rule, `i/n`, Ramp hooks. |
| Format | `pre-commit run --files` on the changed firmware files: `reuse lint-file` and `clang-format` pass. |

Findings, no change required:

- QA F1: in `settings.c` the version 1 and version 2 masks change no result, because the default stores after them
  overwrite the cleared bits. They stay: sections 5.1 and 18.2 state the rule for each version, as for
  `RECORD_FLAGS_USED_V1` in `keyer_spec.md` 17.12.
- QA F2: the extra loop-bound case in `draw_list()` cannot be observed. Accepted.
- `test_menu_prosign.c` V-83 no longer checks `Reversed` on the first CW screen. The check after the save and
  after the re-init covers it. Accepted.
- `ui-ux.md` section 4.1 says that a cursor move "dirties" two pages. The owner `ui-ux-embedded-designer` changes it
  to "changes the pixels of" with the other page texts (menu spec 6.2). This does not block the change.

Open: the device checks DC-5, DC-8, DC-13, DC-14, DC-K3, DC-T1 at 4 and 12 ms, and `ui-ux.md` D-45 to D-47. They stay
unverified until the user tests on the device.

