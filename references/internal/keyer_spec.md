# Paddle keyer, Free Input and long presses: implementation specification

Owner: `architector`. Status: approved for implementation. Section 17 (prosign display setting): D2 review and final
review approved on 2026-10-06 (section 17.12). Readers: `c-embedded-developer`, `qa-embedded`,
`ui-ux-embedded-designer`, `technical-writer`.

## 1. Sources and precedence

| Source | Role |
| --- | --- |
| This document | Interfaces, memory placement, keyer behavior, timing, event protocol, task split and acceptance criteria. It refines the approved plan. It does not change a user decision. |
| `references/internal/ui-ux.md` (owner `ui-ux-embedded-designer`, final for this change) | All that the user sees, hears or presses: sections 5.6, 5.9, 6.1, 6.2, 6.4, 6.6, 6.7, decisions UX-23 to UX-37, tests V-54 to V-79, device checks D-15 to D-24. For section 17: sections 5.9 and 5.10, decision UX-38 (it refines UX-28), F-14, F-15, tests V-80 to V-89, device check D-25. |
| `references/internal/paddle_keyer_research.md` | Filter, entry mask, keyer phases, A/B rules, host-test list. This document wins where the two differ (section 3). |
| `references/internal/paddle_keyer_howtoimplement_research.md` | Background on iambic A and B. This document wins. |
| `references/internal/sidetone_spec.md` | Sidetone contract. Section 21 holds decision D-R2-1. Section 3.1 of this document revises it. |
| `references/internal/menu_subsystem_spec.md` | Buttons (4.5), settings record (5.1, 5.2), menu (6). This document adds to them. |
| `references/internal/components_and_wiring.md` | P3.2 = `enter` and paddle tip, P5.5 = paddle ring. |
| `references/context/STC8H_datasheet.md` | Register facts (section 4). |
| `AGENTS.md`, `llm.md` | Project rules: memory, software SPI, ISR rules, team workflow. |

Precedence rules:

- For a UI value, `ui-ux.md` wins. UI values are the long-press threshold, layout, glyphs, labels and short-press
  behavior. This document repeats the current values only to make the contract complete. When `ui-ux.md` changes a
  value, use the new value and tell `architector`.
- For keyer behavior, timing, events, memory and interfaces, this document wins.
- When `ui-ux.md` asks for a behavior that the interfaces of section 5 cannot give, stop and report it to
  `architector`. Do not change an interface yourself.
- After implementation starts, only `architector` changes this document.

## 2. Scope

User decisions (fixed):

- Straight-key thresholds come from `TX Speed` and `TX Eff. Speed`. There is no adaptive decoding.
- A pattern that is not in the Morse table shows one unknown glyph, `*` (UX-27). The 8-dit error sign is one of
  these patterns.
- Tip/ring swap is a setting: `Paddle` with the values `Normal` and `Reversed` (UX-32).
- SK shows as the four characters `<SK>` (UX-28). AR, BT, KN and AS show as the `Prosigns` setting selects
  (section 17). The default is `Both`, for example `<AR/+>`.
- No exit hint on the mode screens (UX-36). No stuck-key timeout (UX-37).
- "Done" means: host tests pass, the build checks pass, QA approves, `architector` approves. Device behavior stays
  unverified (section 14). The user runs device tests on demand.

In scope:

- Keyer for Straight, Iambic A and Iambic B in the Timer 0 4 kHz interrupt. Parts: contact filters, entry mask, swap,
  phases, sidetone requests, gap detection, event queue to the main loop.
- P5.5 GPIO setup. P3.2 ownership between `buttons` and the keyer.
- Morse decoder: table and pattern builder.
- Free Input mode.
- Long-press events on `menu/back`, `prev` and `next`. Mode screens act only on long presses.
- `Paddle` setting in bit 1 of the settings flags byte.
- `check_asm.sh` R5 extension for port 5.
- Host fakes, host tests, memory, stack and ISR-time report.

Out of scope:

- Receiving and Sending modes. They keep the placeholder.
- A stuck-key timeout (UX-37).
- An RF or key output pin.
- Any change in `firmware/drivers/`.
- Removal of the unused 8x16 font (follow-up F-1, section 16).

## 3. Decisions and plan refinements

### 3.1 D-K1: ISR layout (revises D-R2-1 of `sidetone_spec.md`)

Revision: decision D-T1 of `training_spec.md` (section 3.1) adds the CW player as a third service. The ISR body is
`keyer_service(); player_service(); sidetone_service();`. `firmware/inc/cw/player_service.h` follows every rule of
this section for `keyer_service.h`. `check_asm.sh` rules R6 and R7 check the call list and the absence of
`sdcc_atomic`. The keyer behavior and timing do not change.

Decision:

- The ISR-side keyer code is one function, `static void keyer_service(void)`. The header
  `firmware/inc/cw/keyer_service.h` defines it, together with the ISR-private keyer state as `static` variables.
- On the target, only `firmware/src/sidetone/sidetone.c` includes this header. The include is at the end of the file,
  inside `#if defined(__SDCC_mcs51)`, just before the ISR. So `keyer_service()` is a function of `sidetone.c`.
- The ISR body is `keyer_service(); player_service(); sidetone_service();` in this order (D-T1 of
  `training_spec.md`). The keyer request and the player request then reach the envelope in the same call.
- `sidetone.c` defines two macros before the include. They are the ISR-side sidetone entry that
  `sidetone_spec.md` section 3.10 reserved for the keyer:

```c
#define KEYER_SIDETONE_ON() (sidetone_is_on_requested = true)   /* one SETB */
#define KEYER_SIDETONE_OFF() (sidetone_is_on_requested = false) /* one CLR */
```

- An includer that does not define both macros gets an `#error` from the header.
- `firmware/src/cw/keyer.c` defines the state that the main loop shares with the ISR.
  `firmware/inc/cw/keyer_shared.h` declares it `extern`. The ISR reads and writes these variables directly. That is a
  memory access, not a call. So the rule "an ISR calls only functions of its own file" holds.
- `--norestartseqatomics` stays. AC-21 of `sidetone_spec.md` keeps its strict form, with the call list of AC-K60 and AC-K61.
- Host tests include `keyer_service.h` directly. They define the two macros themselves and call `keyer_service()`
  once per simulated tick.
- Stop and report to `architector` in these cases: the build shows an `sdcc_atomic` symbol, the link fails, or
  `sidetone_timer0_isr` has a call list other than `_keyer_service`, `_player_service`, `_sidetone_service`
  (`check_asm.sh` R6). Do not try another layout.

Rationale:

- By the C rules, a `static` function that `sidetone.c` includes belongs to the `sidetone.c` translation unit. So the
  D-R2-1 rule holds with no compiler-specific behavior.
- The plan proposed `static inline` with a macro fallback. SDCC 4.6.0 treats `inline` as a hint. This repository has
  no evidence of how SDCC inlines into an ISR. So the layout does not depend on it.
- The cost is one `LCALL`/`RET` pair per 4 kHz call, about 7 clocks. The stack does not grow. The ISR already calls
  `sidetone_service()` at the same depth and already saves the full register set (14 pushes).
- The host tests run the same source as the target.
- Rejected: a macro body (hard to read and review, no gain). Rejected: one ISR file with all services (it moves
  `sidetone_service()` out of its file or calls across files). Rejected: no `--norestartseqatomics` (the rollback
  epilogue costs clocks and stack on every call).

Rule for `keyer_service.h`: every translation unit that includes it calls `keyer_service()`. Otherwise GCC reports an
unused static function, and `-Werror` stops the host build.

### 3.2 Refinements of the plan

None of these changes a user decision.

| ID | Plan text | Refinement | Reason |
| --- | --- | --- | --- |
| R-1 | `static inline` service, macro fallback | `static` function in the header (D-K1). | No dependency on SDCC inlining. |
| R-2 | `keyer_start` reseeds the filters | `keyer_start` sets a start request. The ISR does the reseed on its next enabled call. | The ISR stays the only writer of the filter, phase and memory state (research "Firmware layout"). |
| R-3 | Overflow: drop the new event, set a flag | The ISR drops every new event while the flag is set. `keyer_read_event()` returns `KEYER_EVENT_OVERFLOW` once, after the queued events, then clears the flag. | The decoder sees the loss at the right place in the event order. |
| R-4 | (not stated) | The service writes at most one event per call and has one push site. | Section 6.8 proves that two events never fall on one call. The ISR code is smaller. |
| R-5 | `keyer_init()` before EA | `keyer_init()` runs after `sidetone_init()` with EA = 1. | `main.c` sets EA = 1 at step 2. The C start-up RAM clear sets `keyer_is_enabled` to 0. Until `keyer_start()`, the ISR does nothing for the keyer. |
| R-6 | Free Input draws its screen | `menu_render()` draws the Free Input title bar once at the mode start. It uses the code that draws the placeholder title. `free_input_poll()` runs only with no redraw pending. | Reuses the title code. A character never comes before the frame. |
| R-7 | `BUTTON_EVENT_LONG_FLAG 0x40U` | `BUTTON_EVENT_CODE()` masks with `0x3FU`. | The macro must remove both flags. |
| R-8 | One long timer | Each of `menu/back`, `prev` and `next` has its own long timer. | `ui-ux.md` 6.6: `prev` and `next` held together give two long events, each at its own 800 ms moment. |
| R-9 | Placeholders: long press for prev and next only | Placeholders also close only with a long `menu/back` (UX-31). | User decision. Existing tests that close a placeholder with a short `menu/back` change to a long press. |
| R-10 | `morse_decode` returns a char | It returns `MORSE_CHAR_SK` for `...-.-`, one of `MORSE_CHAR_AR`, `MORSE_CHAR_BT`, `MORSE_CHAR_KN` and `MORSE_CHAR_AS` for the four prosigns with a mark (section 17), and `MORSE_CHAR_UNKNOWN` (0) for an unknown pattern. `morse_format()` turns the decoded value into the display text. | UX-27, UX-28. The decoder table holds no UI glyph. |
| R-11 | (not stated) | `check_asm.sh` R5 covers `P5` 0xC8, `P5M1` 0xC9 and `P5M0` 0xCA. | Same rule as the other port mode registers. |

### 3.3 Decisions of the final review

These decisions come from the `architector` review of the implementation and the QA findings F-1 to F-3. The
sections that they change hold the binding text.

| ID | Decision | Changed sections | Reason |
| --- | --- | --- | --- |
| D-K2 | The start call of a generated element latches like an ELEMENT call. After "Start element E", the levels of the same call set the memory of the other lever and `keyer_was_squeezed`. | 6.5, AC-K11, AC-K25, 8 | QA F-1. By the countdown rule (6.4) the start call is the first call of the element. Without the latch, B = A for one release call. That call is the logical open of the squeeze on the second element call. This is against the UX rule "B adds one more element" (`ui-ux.md` 6.4). The other lever was closed while the element ran, so mode B must add it. |
| D-K3 | PREV and NEXT presses that debounce in the same poll: the repeat belongs to PREV. | 6.12, AC-K46 | QA F-3. Two presses in one 1 ms poll have no "pressed last". This is the behavior of the current code, so no code change. |
| D-K4 | The long-press hold counts on the debounced state. | 6.12, AC-K45 | QA note on V-73. Both edges of the debounced state lag the pin by `BUTTONS_DEBOUNCE_MS`. |
| D-K5 | Accepted include and parameter deviations: `keyer_service.h` includes `cw/keyer.h` for the event codes. `free_input.c` includes `cw/keyer_shared.h` only for `KEYER_QUEUE_SIZE`. Parameters 2-4 of `keyer_start` are `MEM_XDATA` (section 5 allows it). | 5.1 | The event codes and the queue size are defined once. `free_input.c` reads no shared keyer variable. |
| D-K6 | Existing tests can change where sections 6.12 to 6.14 change the expected behavior, not only for R-9. | AC-K52, AC-K66 | QA F-2. A held button now also gives a long event, the settings tables have 11 ids, and the CW list has 7 items. |

## 4. Hardware facts

Each fact has a datasheet section or a board source. When a fact is different, stop and report it to
`architector`.

| Fact | Value | Source |
| --- | --- | --- |
| Paddle tip | P3.2, shared with `enter`. Active low. `BUTTON_PINS_CONFIGURE()` already sets the pull-up and the high-impedance input. | `components_and_wiring.md`, `button_pins.h` |
| Paddle ring | P5.5. Active low (not checked on the device). | `components_and_wiring.md`, research "Hardware facts" |
| P5 | SFR 0xC8, bit-addressable. P5.5 is bit address 0xCD. Bits 7:6 do not exist. | 9.1.1, SFR list ("P5 (C8H)") |
| P5M1, P5M0 | SFR 0xC9, 0xCA. M1 = 1 and M0 = 0 give a high-impedance input. Reset: input. | 9.1.2 |
| P5PU | XFR 0xFE15. Bit 5 is the P5.5 pull-up, about 4.1 kOhm. Access needs EAXFR = 1. | 9.1.3 |
| P5NCS | XFR 0xFE1D. Reset 0: Schmitt trigger on. No write. | 9.1.4 |
| P5IE | XFR 0xFE35. Digital input enable, enabled at reset. No write. | 9.1.7 |
| Pull-up doubt | The research says that the repeated tables of the manual differ about the P5.5 pull-up. | research Step 1, device check DC-K1 |
| Timer 0 rate | 4000 calls per second, exact at 24 MHz. | `sidetone_spec.md` section 8 |

## 5. Interfaces

Rules for all new code (same as `sidetone_spec.md` section 5):

- C11 as SDCC accepts it, small model. No `--stack-auto`, no `__reentrant`, no `__using`, no function pointers on
  the target.
- Constants are `#define` values with a `U` suffix. Interfaces use `typedef uint8_t`, no `enum`.
- Boolean names start with `is`, `has`, `should`, `can`, `does` or `was`.
- No module clears `EA`. No code clears `EAXFR`. No ISR writes port 2, the LCD pins, the button pins, PWMB or IAP.
- Comments: short, the reason and not the action, simple English, no status narration.
- The C types and the parameter order below are binding. The developer can add `MEM_XDATA` to the parameters after
  the first of a main-loop function. The RAM report decides.

### 5.1 Files

| File | Content | Included by |
| --- | --- | --- |
| `firmware/inc/cw/keyer.h` | Public main-loop API, event codes, key types, WPM limits. | `keyer.c`, `keyer_service.h` (event codes), `free_input.c`, tests, `fake_keyer.c` |
| `firmware/inc/cw/keyer_shared.h` | Phase codes, filter and queue constants, `extern` declarations of the shared state. | `keyer.c`, `keyer_service.h`, `free_input.c` (only `KEYER_QUEUE_SIZE`, no shared variable, D-K5) |
| `firmware/inc/cw/keyer_service.h` | `keyer_service()` and the ISR-private state (D-K1). | `sidetone.c` (target only), keyer tests |
| `firmware/src/cw/keyer.c` | Definitions of the shared state. `keyer_init`, `keyer_start`, `keyer_stop`, `keyer_read_event`. | - |
| `firmware/inc/board/paddle_pins.h` | Target-only P5.5 setup and raw contact reads. | `keyer.c`, `keyer_service.h` |
| `firmware/inc/cw/morse.h`, `firmware/src/cw/morse.c` | Pattern builder and table. | `free_input.c`, tests |
| `firmware/inc/modes/free_input.h`, `firmware/src/modes/free_input.c` | Free Input mode. | `menu.c`, tests |
| `firmware/Tests/fakes/board/paddle_pins.h`, `fake_paddle_pins.c/.h` | Host replacement of the paddle pins. | keyer tests |
| `firmware/Tests/fakes/fake_keyer.c/.h` | Fake of `keyer.h` for the menu and Free Input groups. | menu, Free Input tests |

Changed files:

- `inc/mcu/stc8h.h`, `inc/buttons/buttons.h`, `inc/board/buttons_config.h`, `src/buttons/buttons.c`.
- `inc/settings/settings.h`, `src/settings/settings.c`.
- `inc/menu/menu.h`, `src/menu/menu.c`, `src/menu/menu_render.c`, `src/menu/menu_tree.c`.
- `src/sidetone/sidetone.c` (end of file only), `src/main.c`.
- `tools/check_asm.sh`, `Tests/CMakeLists.txt`.

### 5.2 `inc/mcu/stc8h.h` additions

```c
__sfr __at(0xC8) P5;
__sbit __at(0xCD) P5_5;
__sfr __at(0xC9) P5M1;
__sfr __at(0xCA) P5M0;
#define P5PU (*(volatile __xdata uint8_t*)0xFE15U)
```

### 5.3 `inc/board/paddle_pins.h` (target)

The first lines are `#if !defined(__SDCC_mcs51)` and `#error`, as in `button_pins.h`. Every macro is a
`do { } while (0)` statement or a plain expression. No macro calls a function.

| Macro | Context | Effect |
| --- | --- | --- |
| `PADDLE_RING_MASK` | - | `0x20U` (P5.5). |
| `PADDLE_PINS_CONFIGURE()` | `keyer_init()` | Four steps, below the table. |
| `PADDLE_PINS_IS_TIP_CLOSED()` | ISR | Expression `(!P3_2)`. Reads the pin bit only. |
| `PADDLE_PINS_IS_RING_CLOSED()` | ISR | Expression `(!P5_5)`. Reads the pin bit only. |

`PADDLE_PINS_CONFIGURE()` steps:

1. `P_SW2 |= P_SW2_EAXFR` (`ORL`).
2. `P5PU |= PADDLE_RING_MASK` (`ORL`). The pull-up comes before the mode write.
3. `P5M1 |= PADDLE_RING_MASK` (`ORL`).
4. `P5M0 &= (uint8_t)~PADDLE_RING_MASK` (`ANL`).

The mode is already input after reset (datasheet section 2.9.3, note 1). Steps 3 and 4 make it explicit. The macro
does not touch P3.2: `buttons_init()` owns its setup.

### 5.4 Public API `inc/cw/keyer.h`

```c
#include <stdbool.h>
#include <stdint.h>

typedef uint8_t keyer_event_t;

#define KEYER_EVENT_NONE 0U
#define KEYER_EVENT_DIT 1U       /* end of a dit mark */
#define KEYER_EVENT_DAH 2U       /* end of a dah mark */
#define KEYER_EVENT_CHAR_GAP 3U  /* space reached KEYER_CHAR_GAP_DITS dits */
#define KEYER_EVENT_WORD_GAP 4U  /* space reached KEYER_WORD_GAP_UNITS Farnsworth units */
#define KEYER_EVENT_OVERFLOW 5U  /* events were lost after the events before it */

#define KEYER_KEY_STRAIGHT 0U    /* equal to SETTINGS_KEY_STRAIGHT */
#define KEYER_KEY_IAMBIC_A 1U    /* equal to SETTINGS_KEY_IAMBIC_A */
#define KEYER_KEY_IAMBIC_B 2U    /* equal to SETTINGS_KEY_IAMBIC_B */

#define KEYER_WPM_MIN 5U         /* equal to settings_min(SETTINGS_ID_TX_WPM) */
#define KEYER_WPM_MAX 50U        /* equal to settings_max(SETTINGS_ID_TX_WPM) */

void keyer_init(void);
void keyer_start(uint8_t key_type, bool is_swapped, uint8_t wpm, uint8_t eff_wpm);
void keyer_stop(void);
keyer_event_t keyer_read_event(void);
```

| Function | Context | Behavior |
| --- | --- | --- |
| `keyer_init()` | Main loop, once, after `sidetone_init()`. EA can be 1. | Clears `keyer_is_enabled` and `keyer_is_start_requested`. Runs `PADDLE_PINS_CONFIGURE()`. Sets the queue tail to the head. Clears `keyer_is_overflow`. Does not touch the sidetone. |
| `keyer_start(key_type, is_swapped, wpm, eff_wpm)` | Main loop. | Section 6.1. Runs the 16-bit and 32-bit divisions of section 6.7. |
| `keyer_stop()` | Main loop. | Step 1: clear `keyer_is_enabled` (one `CLR`). Step 2: clear `keyer_is_start_requested`. Step 3: `sidetone_set_on(false)`. A second call has no other effect. No event comes after it. |
| `keyer_read_event()` | Main loop. | Section 6.9. Returns one event or `KEYER_EVENT_NONE`. |

`keyer.c` has compile-time checks that `KEYER_KEY_*` equal `SETTINGS_KEY_*`. A host test compares `KEYER_WPM_MIN` and
`KEYER_WPM_MAX` with the settings range (AC-K22).

### 5.5 `inc/cw/keyer_shared.h`

```c
#include "board/sidetone_config.h"
#include "mcu/compiler.h"

typedef uint8_t keyer_phase_t;
#define KEYER_PHASE_IDLE 0U
#define KEYER_PHASE_STRAIGHT_DOWN 1U
#define KEYER_PHASE_ELEMENT 2U
#define KEYER_PHASE_GAP 3U

#define KEYER_SERVICE_HZ SIDETONE_SERVICE_HZ       /* the keyer runs in the sidetone Timer 0 ISR */
#define KEYER_FILTER_CLOSED_MIN 5U                 /* closed samples of 8 that vote closed */
#define KEYER_FILTER_OPEN_MAX 3U                   /* closed samples of 8 at or below which it votes open */
#define KEYER_QUEUE_SIZE 8U                        /* power of 2, divides 256 */
#define KEYER_QUEUE_MASK (KEYER_QUEUE_SIZE - 1U)
#define KEYER_EARLY_RELEASE_TICKS SIDETONE_RAMP_STEPS  /* 32: the fall ends at the mark end */
#define KEYER_CHAR_GAP_DITS 2U
#define KEYER_WORD_GAP_UNITS 5U
#define KEYER_DIT_TICKS_NUMERATOR 4800UL           /* KEYER_SERVICE_HZ * 1.2 s */
#define KEYER_FARNSWORTH_C_FACTOR 240000UL         /* KEYER_SERVICE_HZ * 60 */
#define KEYER_FARNSWORTH_S_FACTOR 148800UL         /* KEYER_SERVICE_HZ * 37.2 */
#define KEYER_FARNSWORTH_DIVISOR 19UL

/* Shared state, defined in keyer.c. Writers in section 7. */
extern volatile MEM_BIT keyer_is_enabled;
extern volatile MEM_BIT keyer_is_start_requested;
extern volatile MEM_BIT keyer_is_overflow;
extern volatile MEM_BIT keyer_is_straight;
extern volatile MEM_BIT keyer_is_iambic_a;
extern volatile MEM_BIT keyer_is_swapped;
extern volatile MEM_DATA uint8_t keyer_queue_head;
extern volatile MEM_DATA uint8_t keyer_queue_tail;
extern volatile MEM_XDATA uint8_t keyer_queue[KEYER_QUEUE_SIZE];
extern volatile MEM_XDATA uint16_t keyer_dit_ticks;
extern volatile MEM_XDATA uint16_t keyer_dah_ticks;
extern volatile MEM_XDATA uint16_t keyer_two_dit_ticks;   /* straight dah threshold and char gap */
extern volatile MEM_XDATA uint16_t keyer_word_gap_ticks;
```

The four tick values and the three mode bits together are the keyer parameters. Compile-time checks (`#error` or
`_Static_assert`):

- `KEYER_SERVICE_HZ == 4000U`. The three timing factors assume it.
- `KEYER_QUEUE_SIZE` is a power of 2 and at most 128.
- The dit at `KEYER_WPM_MAX` is longer than `2 * SIDETONE_RAMP_STEPS` calls (96 > 64). Formula:
  `(KEYER_DIT_TICKS_NUMERATOR + KEYER_WPM_MAX / 2) / KEYER_WPM_MAX > 2 * SIDETONE_RAMP_STEPS`. Result: the early
  release comes after the rise, and `ZERO_WAIT` ends before the next element starts.

### 5.6 `inc/cw/keyer_service.h` (ISR side)

The header includes `board/paddle_pins.h`, `cw/keyer_shared.h` and `mcu/compiler.h`. It has the check
`#if !defined(KEYER_SIDETONE_ON) || !defined(KEYER_SIDETONE_OFF)` with `#error`.

ISR-private state: all `static`, no `volatile`. The names are binding, because tests read them.

| Variable | Type and qualifier | Meaning |
| --- | --- | --- |
| `keyer_phase` | `static MEM_DATA keyer_phase_t` | Current phase. |
| `keyer_ticks` | `static MEM_DATA uint16_t` | ELEMENT and GAP: calls left. STRAIGHT_DOWN: mark calls so far, with saturation at 0xFFFF. |
| `keyer_space_ticks` | `static MEM_DATA uint16_t` | Calls since the last mark end, while a gap event is pending. |
| `keyer_tip_history`, `keyer_ring_history` | `static MEM_DATA uint8_t` | Last 8 samples. Bit 0 is the newest. 1 = closed. |
| `keyer_tip_count`, `keyer_ring_count` | `static MEM_DATA uint8_t` | Count of 1 bits in the history, 0..8. |
| `keyer_is_tip_closed`, `keyer_is_ring_closed` | `static MEM_BIT` | Voted state of the filter. |
| `keyer_is_tip_masked`, `keyer_is_ring_masked` | `static MEM_BIT` | Entry mask (section 6.3). |
| `keyer_has_dit_memory`, `keyer_has_dah_memory` | `static MEM_BIT` | Element memories. |
| `keyer_was_squeezed` | `static MEM_BIT` | Both logical contacts were closed at one call since the last decision. |
| `keyer_is_element_dah` | `static MEM_BIT` | The current or last generated element is a dah. |
| `keyer_is_char_gap_pending`, `keyer_is_word_gap_pending` | `static MEM_BIT` | Gap events still to come in this space. |

The logical mark is this macro. Tests and any later key output use it, never the sidetone request:

```c
#define KEYER_IS_MARK_ACTIVE() ((keyer_phase == KEYER_PHASE_ELEMENT) || (keyer_phase == KEYER_PHASE_STRAIGHT_DOWN))
```

`static void keyer_service(void)` is inside `#pragma save`, `#pragma nooverlay` and `#pragma restore`, guarded by
`#if defined(__SDCC_mcs51)`. It calls no function and no library helper. Section 6 gives its behavior.

### 5.7 `inc/cw/morse.h`

```c
#include <stdbool.h>
#include <stdint.h>

#define MORSE_PATTERN_EMPTY 0x01U    /* no element yet */
#define MORSE_PATTERN_INVALID 0x00U  /* more than MORSE_MAX_ELEMENTS, or a lost event */
#define MORSE_MAX_ELEMENTS 6U
#define MORSE_CHAR_UNKNOWN '\0'      /* Free Input shows '*' */
#define MORSE_CHAR_SK '\x01'         /* prosign SK; Free Input shows "<SK>" */
/* MORSE_CHAR_AR to MORSE_CHAR_AS, MORSE_STYLE_*, MORSE_TEXT_*: section 17.3.1 */

uint8_t morse_add_element(uint8_t pattern, bool is_dah);
char morse_decode(uint8_t pattern);
uint8_t morse_format(char decoded, uint8_t style, char MEM_XDATA* text);  /* section 17.4.1 */
```

| Function | Behavior |
| --- | --- |
| `morse_add_element(pattern, is_dah)` | `MORSE_PATTERN_INVALID` stays invalid. A pattern of 64 or more gives `MORSE_PATTERN_INVALID`: it already has 6 elements, or it is not a pattern. Any other pattern gives `(pattern << 1) \| (is_dah ? 1 : 0)`. |
| `morse_decode(pattern)` | A pattern of 128 or more gives `MORSE_CHAR_UNKNOWN`. Any other pattern gives `morse_table[pattern]`. Entries 0 and 1 hold `MORSE_CHAR_UNKNOWN`, and so does each slot that section 6.10 does not list. |
| `morse_format(decoded, style, text)` | Section 17.4.1. |

The table is `static const MEM_CODE char morse_table[128]`. Main loop only. The later Sending and Receiving modes can
use it. The special values keep UI glyphs out of the decoder table. `morse_format()` gives the display text.

### 5.8 `inc/modes/free_input.h`

```c
void free_input_enter(void);
void free_input_poll(void);
void free_input_clear(void);
void free_input_delimiter(void);
void free_input_leave(void);
```

Main loop only. Section 6.11 gives the behavior. Only `menu.c` calls these functions.

### 5.9 `inc/buttons/buttons.h` and `inc/board/buttons_config.h` changes

```c
#define BUTTON_EVENT_LONG_FLAG 0x40U
#define BUTTON_EVENT_CODE(event) ((uint8_t)((event) & 0x3FU))

/** Gives P3.2 to the menu (true) or takes it away (false). Main loop only. */
void buttons_set_enter_enabled(bool is_enabled);
```

```c
#ifndef BUTTONS_LONG_PRESS_MS
#define BUTTONS_LONG_PRESS_MS 800U  /* ui-ux.md section 6.2, decision UX-23 */
#endif
```

A compile-time check keeps `BUTTONS_LONG_PRESS_MS` above 0. The other values do not change. Section 6.12 gives the
behavior.

### 5.10 `inc/settings/settings.h` changes

```c
#define SETTINGS_ID_PADDLE_SWAP 10U  /* 0 Normal, 1 Reversed */
#define SETTINGS_ID_COUNT 11U
```

Section 6.13 gives the behavior. Section 17.3.2 adds `SETTINGS_ID_PROSIGN_STYLE` and makes `SETTINGS_ID_COUNT` 12.

### 5.11 `inc/menu/menu.h` changes

```c
#define MENU_SCREEN_FREE_INPUT 5U
```

Section 6.14 gives the behavior. The other declarations do not change.

## 6. Behavior

### 6.1 `keyer_start(key_type, is_swapped, wpm, eff_wpm)`

Main loop, in this order:

1. `keyer_stop()`. From here the ISR returns at once for the keyer, so the keyer parameters can change safely.
2. Clamp `wpm` to `[KEYER_WPM_MIN, KEYER_WPM_MAX]`. Clamp `eff_wpm` to `[KEYER_WPM_MIN, wpm]`. A `key_type` above
   `KEYER_KEY_IAMBIC_B` becomes `KEYER_KEY_IAMBIC_B`.
3. Write the keyer parameters: the four tick values of section 6.7, `keyer_is_straight`, `keyer_is_iambic_a` and
   `keyer_is_swapped`.
4. `keyer_queue_tail = keyer_queue_head`. Clear `keyer_is_overflow`. The ISR does not write the head while the keyer
   is disabled.
5. Set `keyer_is_start_requested` (one `SETB`).
6. Set `keyer_is_enabled` (one `SETB`).

All shared variables are `volatile`. So C keeps the stores of steps 3 and 4 before step 6. D-R3-1 of
`sidetone_spec.md` uses the same reason. The ISR reads the keyer parameters only while `keyer_is_enabled` = 1. The
parameters never change while an element runs.

### 6.2 `keyer_service()`: order of one call

1. With `keyer_is_enabled` = 0: return. Nothing changes. This is the menu path: one bit test.
2. With `keyer_is_start_requested` = 1: reseed (below), clear `keyer_is_start_requested`, return. This call does not
   key.
3. Sample the tip, then the ring, with no other code between the two reads.
4. Update both filters and both entry masks (section 6.3).
5. Form the logical contacts and apply the swap (section 6.3).
6. Run the phase step (section 6.5). It can set the event of the call to DIT or DAH.
7. Run the gap step (section 6.6). It can set the event of the call to CHAR_GAP or WORD_GAP.
8. Push the event of the call (section 6.8). A call without an event pushes nothing.

Reseed:

- Read both raw contacts once.
- A closed contact gets history 0xFF and count 8. An open contact gets history 0x00 and count 0.
- The voted state of each contact = its raw state. The mask of each contact = its raw state.
- `keyer_phase` = IDLE, `keyer_ticks` = 0, `keyer_space_ticks` = 0.
- Clear both memories, `keyer_was_squeezed`, `keyer_is_element_dah` and both gap-pending bits.
- `KEYER_SIDETONE_OFF()`.

### 6.3 Filter, entry mask and swap

For each contact, on every call after the reseed:

1. Bit 7 of the history is 1: subtract 1 from the count.
2. Shift the history left by 1. Raw contact closed: set bit 0 and add 1 to the count.
3. Count >= `KEYER_FILTER_CLOSED_MIN` (5): voted closed. Count <= `KEYER_FILTER_OPEN_MAX` (3): voted open. Count 4:
   the voted state does not change.
4. Contact masked and voted open: clear the mask. This makes no edge and no event.
5. Logical contact = voted closed AND not masked.

Results that tests check:

- From a settled state, the 5th opposite sample changes the voted state.
- One, two or three opposite samples never change a settled state.
- An alternating input never changes a settled state.

Swap, below the keyer:

- `is_dit = keyer_is_swapped ? ring : tip`, with logical contacts.
- `is_dah = keyer_is_swapped ? tip : ring`, with logical contacts.
- In Straight mode the swap has no effect, because either contact keys.

Entry mask results:

- The `enter` press that opens Free Input gives no element. The tip stays masked until it votes open.
- A mono plug works as a straight key. The plug shorts the ring to the sleeve, so the ring stays masked.

### 6.4 Countdown rule

The call that enters ELEMENT or GAP loads N into `keyer_ticks`. The phase then lasts exactly N calls, the entry call
included. On each later call in that phase, the service first subtracts 1. The phase ends on the call where the
result is 0.

Example at 20 WPM (d = 240), Iambic, Paddle `Normal`, settled open filters. The raw tip closes at call 1 and opens at
call 100:

| Call | What happens |
| --- | --- |
| 1-4 | The tip count rises 1..4. Logical open. IDLE. |
| 5 | Voted closed. A dit starts: ELEMENT, `keyer_ticks` = 240, `KEYER_SIDETONE_ON()`. |
| 104 | The tip votes open (5th open sample). ELEMENT continues. |
| 213 | `keyer_ticks` becomes 32: `KEYER_SIDETONE_OFF()`. The 32-call fall writes compare 0 on call 244. |
| 245 | `keyer_ticks` becomes 0: event DIT. GAP, `keyer_ticks` = 240. The space starts: `keyer_space_ticks` = 1. |
| 485 | Gap end. No request: IDLE. |
| 724 | `keyer_space_ticks` = 480 = 2 d: event CHAR_GAP. |
| 1444 | `keyer_space_ticks` = 1200 = 5 d: event WORD_GAP. |

ELEMENT covers calls 5-244 (240 calls). GAP covers calls 245-484 (240 calls).

### 6.5 Phase step

Two actions recur in the table:

- "Start element E": `keyer_phase` = ELEMENT. `keyer_is_element_dah` = (E is a dah). `keyer_ticks` =
  `keyer_dah_ticks` or `keyer_dit_ticks`. `KEYER_SIDETONE_ON()`. Clear both gap-pending bits, because the space
  ended. Then apply the start latch (below) with `is_dit` and `is_dah` of this call (D-K2).
- "Mark end with event X": the event of the call = X. `keyer_space_ticks` = 0. Set both gap-pending bits.

| Phase at entry | Condition | Actions | Next phase |
| --- | --- | --- | --- |
| IDLE | Straight, `is_dit` or `is_dah` | `keyer_ticks` = 1. `KEYER_SIDETONE_ON()`. Clear both gap-pending bits. | STRAIGHT_DOWN |
| IDLE | Iambic, `is_dit` (also together with `is_dah`) | Start element dit. | ELEMENT |
| IDLE | Iambic, only `is_dah` | Start element dah. | ELEMENT |
| IDLE | No logical contact | none | IDLE |
| STRAIGHT_DOWN | `is_dit` or `is_dah` | `keyer_ticks` + 1, with saturation at 0xFFFF. | STRAIGHT_DOWN |
| STRAIGHT_DOWN | Both open | `KEYER_SIDETONE_OFF()`. Mark end. The event is DAH for `keyer_ticks >= keyer_two_dit_ticks`, else DIT. | IDLE |
| ELEMENT | Every call | Latch first (below). Then subtract 1 from `keyer_ticks`. | - |
| ELEMENT | Result 0 | Mark end with DIT or DAH (`keyer_is_element_dah`). `keyer_ticks` = `keyer_dit_ticks`. | GAP |
| ELEMENT | Result <= `KEYER_EARLY_RELEASE_TICKS` | `KEYER_SIDETONE_OFF()`. | ELEMENT |
| ELEMENT | Other result | none | ELEMENT |
| GAP | Every call | Latch first (below). Then subtract 1 from `keyer_ticks`. | - |
| GAP | Result not 0 | none | GAP |
| GAP | Result 0 | Gap-end decision (below): it gives element E or IDLE. Clear both memories and `keyer_was_squeezed`. Then, for E, "Start element E", with its start latch. | ELEMENT or IDLE |

Latch in ELEMENT:

- The element is a dah and `is_dit`: set the dit memory.
- The element is a dit and `is_dah`: set the dah memory.
- `is_dit` and `is_dah`: set `keyer_was_squeezed`.

Start latch (D-K2): the same three rules as the latch in ELEMENT, for the new element E, on its start call. The start
call is the first of the N calls of the element (section 6.4), so every call of an element latches. On a gap-end call
the start latch comes after the clear of the memories and `keyer_was_squeezed`, so it can set them again. Both start
sites apply it: the start from IDLE and the start at the gap end.

Latch in GAP:

- `is_dit`: set the dit memory. `is_dah`: set the dah memory.
- `is_dit` and `is_dah`: set `keyer_was_squeezed`.

Gap-end decision. Let `last` = `keyer_is_element_dah`. Let `wants_dit` = `is_dit` OR the dit memory. Let `wants_dah`
= `is_dah` OR the dah memory. Use the first rule that matches:

1. Iambic A, `keyer_was_squeezed` set, `is_dit` open and `is_dah` open: IDLE. No trailing element.
2. The opposite of `last` is wanted: start that element.
3. The same as `last` is wanted: start that element.
4. Else IDLE.

Iambic B never applies rule 1. Both modes use the same memories. So in A and in B, a single tap of the other lever
adds that element. The tap can come during an element or a gap, without a squeeze.

Defined results that the tests check:

- A held squeeze alternates. From IDLE it starts with a dit.
- For the same released squeeze, B gives the elements of A plus exactly one element of the other kind. This holds for
  every release call. Edge case: only the start call of an element saw the squeeze, and the levers open logically on
  the next call. B then also adds one element (D-K2).
- A squeeze that is held through the start call of an element, with the lever of the last element open from the next
  call: the next element is of the other kind, in A and in B. Example: after a dit, both levers are closed at the
  gap-end call, so a dah starts. The dit lever opens on the next call and the dah lever stays closed. Then a dit
  follows the dah.
- Iambic A, overlap tap. The other lever closes while the current lever is still closed, so `keyer_was_squeezed` = 1.
  Both levers then open before the gap end. Result: A adds no element, B adds it.
- A short tap of the same lever during a gap sets its memory, and the element repeats.
- A release counts at the gap end only after the filter votes open, on a call before the gap-end call or on it.
- Sidetone request for a generated element: on from the start call. Off from the call where `keyer_ticks` becomes 32.
  The 32-call fall (8 ms) then writes compare 0 on the last ELEMENT call. The logical mark keeps its full N calls.
- Sidetone request for a straight key: on during STRAIGHT_DOWN, off at the release call.

### 6.6 Gap step

The gap step runs on every call that reaches step 7, after the phase step:

1. With `keyer_is_word_gap_pending` = 0: nothing.
2. Else add 1 to `keyer_space_ticks`.
3. With `keyer_is_char_gap_pending` set and `keyer_space_ticks == keyer_two_dit_ticks`: the event of the call =
   CHAR_GAP. Clear `keyer_is_char_gap_pending`.
4. With `keyer_space_ticks == keyer_word_gap_ticks`: the event of the call = WORD_GAP. Clear
   `keyer_is_word_gap_pending`.

Results:

- On the mark-end call `keyer_space_ticks` becomes 1.
- CHAR_GAP comes once per space, on the call where the space reaches `2 * dit_ticks` calls.
- WORD_GAP comes once per space, after CHAR_GAP.
- A new mark clears both pending bits. So a short space gives no event.
- No gap event comes before the first mark after a start.
- The counter stops at `keyer_word_gap_ticks`, at most 11 850. It never wraps.
- In the Iambic modes, CHAR_GAP always comes in IDLE, because GAP lasts 1 dit.

### 6.7 Timing values (main loop, in `keyer_start`)

| Value | Formula | Range at 5..50 WPM |
| --- | --- | --- |
| `keyer_dit_ticks` | `(4800 + wpm / 2) / wpm`. This is round(4800 / WPM), or 1200 / WPM ms. | 960..96 |
| `keyer_dah_ticks` | `3 * dit` | 2880..288 |
| `keyer_two_dit_ticks` | `2 * dit` | 1920..192 |
| Farnsworth unit `fu` | `round((240000 * c - 148800 * s) / (19 * c * s))`. Here `c` = `wpm` and `s` = `eff_wpm`. Use `uint32_t` and `(num + den / 2) / den`. A result below `dit` becomes `dit`. | `fu = dit` for `s = c` |
| `keyer_word_gap_ticks` | `5 * fu` | 480..11 850 |

The Farnsworth unit is the PARIS spacing unit. A PARIS word has 50 units. 31 units are the character elements at
speed `c`. The other 19 units stretch so that the word takes `60 / s` seconds. In seconds,
`fu = (60 c - 37.2 s) / (19 c s)`. At 4000 calls per second this gives the formula above.

Reference values, checked with a script against the formulas:

| `wpm` | `eff_wpm` | dit | dah | 2 dit | fu | word gap |
| ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 5 | 5 | 960 | 2880 | 1920 | 960 | 4800 |
| 7 | 7 | 686 | 2058 | 1372 | 686 | 3430 |
| 13 | 13 | 369 | 1107 | 738 | 369 | 1845 |
| 20 | 20 | 240 | 720 | 480 | 240 | 1200 |
| 20 | 10 | 240 | 720 | 480 | 872 | 4360 |
| 20 | 5 | 240 | 720 | 480 | 2135 | 10 675 |
| 25 | 15 | 192 | 576 | 384 | 529 | 2645 |
| 32 | 32 | 150 | 450 | 300 | 150 | 750 |
| 50 | 50 | 96 | 288 | 192 | 96 | 480 |
| 50 | 49 | 96 | 288 | 192 | 101 | 505 |
| 50 | 5 | 96 | 288 | 192 | 2370 | 11 850 |

All values fit `uint16_t`. The ISR only compares, counts and copies them. It never multiplies or divides.

### 6.8 Event push (ISR)

At most one event comes per call. Proof:

- On a mark-end call `keyer_space_ticks` = 1, and `2 * dit >= 192`. So no gap event falls on a mark-end call.
- `keyer_word_gap_ticks >= 5 * dit`, which is more than `2 * dit`. So CHAR_GAP and WORD_GAP never fall on one call.

The service has one push site:

1. With `keyer_is_overflow` = 1: drop the event.
2. With `(uint8_t)(keyer_queue_head - keyer_queue_tail) >= KEYER_QUEUE_SIZE`: set `keyer_is_overflow`, drop the
   event.
3. Else write `keyer_queue[keyer_queue_head & KEYER_QUEUE_MASK] = event`, then add 1 to `keyer_queue_head`. The slot
   store comes before the head store. Both are `volatile`.

The head and the tail are free-running bytes. 256 is a multiple of the queue size, so the wrap needs no extra code.

The fastest event rate is one dit per 48 ms at 50 WPM (dit plus gap). So the main loop must fall about 300 ms behind
before the queue overflows.

### 6.9 `keyer_read_event()` (main loop)

1. With `keyer_queue_tail != keyer_queue_head`: read `keyer_queue[keyer_queue_tail & KEYER_QUEUE_MASK]`. Add 1 to the
   tail (one `INC`). Return the event.
2. With `keyer_is_overflow` = 1: clear it (one `CLR`). Return `KEYER_EVENT_OVERFLOW`.
3. Else return `KEYER_EVENT_NONE`.

Why the order is correct:

- The ISR sets the overflow bit only for a full queue.
- The ISR drops every event while the bit is set.
- So an empty queue with the bit set means that the main loop read every event from before the loss.
- Events after the loss come only after the clear.

### 6.10 Morse table

Index rule: start at 1. For each element, shift left by 1. Add 1 for a dah. Up to 6 elements give an index below 128.
The table holds exactly these 54 entries. All other entries are `MORSE_CHAR_UNKNOWN`.

| Group | Index:char |
| --- | --- |
| Letters 1 | 2:E 3:T 4:I 5:A 6:N 7:M 8:S 9:U 10:R 11:W 12:D 13:K 14:G |
| Letters 2 | 15:O 16:H 17:V 18:F 20:L 22:P 23:J 24:B 25:X 26:C 27:Y 28:Z 29:Q |
| Digits | 32:5 33:4 35:3 39:2 47:1 48:6 56:7 60:8 62:9 63:0 |
| Signs 1 | `40:AS` `42:AR` `49:BT` `50:/` `54:KN` `69:SK` `76:?` `77:_` `82:"` |
| Signs 2 | `85:.` `90:@` `94:'` `97:-` `106:;` `107:!` `109:)` `115:,` `120::` |

| Char | Code | Prosign |
| --- | --- | --- |
| AS (index 40) | `.-...` | AS, the entry holds `MORSE_CHAR_AS`. Mark `&`. |
| AR (index 42) | `.-.-.` | AR, the entry holds `MORSE_CHAR_AR`. Mark `+`. |
| BT (index 49) | `-...-` | BT, the entry holds `MORSE_CHAR_BT`. Mark `=`. |
| KN (index 54) | `-.--.` | KN, the entry holds `MORSE_CHAR_KN`. Mark `(`. |
| SK (index 69) | `...-.-` | SK, the entry holds `MORSE_CHAR_SK`. No mark. |
| `/` | `-..-.` | - |
| `?` | `..--..` | - |
| `_` | `..--.-` | - |
| `"` | `.-..-.` | - |
| `.` | `.-.-.-` | - |
| `@` | `.--.-.` | - |
| `'` | `.----.` | - |
| `-` | `-....-` | - |
| `;` | `-.-.-.` | - |
| `!` | `-.-.--` | - |
| `)` | `-.--.-` | - |
| `,` | `--..--` | - |
| `:` | `---...` | - |

The table has no lowercase letter, no space, no `*`, no `<` and no `>` (`ui-ux.md` 5.9). The only values below 0x20
in the table are `MORSE_CHAR_UNKNOWN` and the five prosign codes 0x01 to 0x05. The marks `&`, `+`, `=` and `(` come
only from `morse_format()` (section 17.4.1). The 8-dit error sign has more than 6 elements, so it is unknown.

### 6.11 Free Input mode

UI rules: `ui-ux.md` sections 5.9 and 6.7. Current values:

- Font `FONTS_ID_5X8`, the UI font, already selected.
- Title bar `Free Input` in row 0. Text rows 1-7, 25 columns.
- Cell `(r, c)` at `x = 2 + 5c`, `y = 8r`.
- Unknown glyph `*`. SK as `<SK>`, four cells. AR, BT, KN and AS as the `Prosigns` setting selects, one to six
  cells (section 17.4.4).
- Delimiter at `y = 8r + 3`, `x` 2-126.

State in xdata: cursor row (1..7), cursor column (0..25), current pattern, a 2-byte text buffer for one glyph, the
prosign style, and a `MORSE_TEXT_SIZE` buffer for the text of one character. Column 25 means "line full". There is no
text store: the framebuffer is the only store.

`free_input_enter()`:

1. `buttons_set_enter_enabled(false)`.
2. `sidetone_set_tone_hz(settings_get(SETTINGS_ID_TONE_HZ))`.
3. `keyer_start()` with the key type, `paddle swap != 0`, TX WPM and TX Eff. WPM from `settings_get()`.
4. The prosign style = `settings_get(SETTINGS_ID_PROSIGN_STYLE)`, kept in xdata until the mode ends (section 17.4.4).
5. Cursor row 1, column 0. Pattern `MORSE_PATTERN_EMPTY`.
6. It draws nothing. `menu_render()` draws the frame (R-6).

`free_input_poll()`: read events with `keyer_read_event()` up to `KEYER_EVENT_NONE`. At most `KEYER_QUEUE_SIZE + 1`
events per call. Handle each event as below.

| Event | Action |
| --- | --- |
| DIT, DAH | `pattern = morse_add_element(pattern, event == KEYER_EVENT_DAH)`. |
| CHAR_GAP | Emit the pattern. |
| WORD_GAP | Emit the pattern, then the word space. |
| OVERFLOW | `pattern = MORSE_PATTERN_INVALID`. |

Emit the pattern:

- Pattern `MORSE_PATTERN_EMPTY`: nothing.
- Else decode it, and format the result with `morse_format(decoded, prosign style, text buffer)`. Draw each
  character of the text, one glyph at a time. So `MORSE_CHAR_UNKNOWN` draws `*`, `MORSE_CHAR_SK` draws `<`, `S`, `K`
  and `>`, a prosign with a mark draws the text of section 17.4.1, and any other value draws that character.
- Then the pattern is `MORSE_PATTERN_EMPTY`.
- `MORSE_PATTERN_INVALID` decodes as unknown, so it draws `*`.

Word space: column 0 or column 25 gives nothing. Any other column gets + 1.

Draw one glyph:

1. Column 25: new line first.
2. Draw the glyph with `gfx_draw_text()` at the cell, in `GFX_TEXT_NORMAL`.
3. Column + 1.

Each glyph of a text of more than one character (`<SK>`, `<AR>`, `<AR/+>`) follows this rule, so the text can split
across two lines (`ui-ux.md` 5.9).

New line: a row below 7 gets + 1. Row 7 scrolls instead. Then the column is 0.

Scroll:

1. Copy the 128 bytes of page `p + 1` to page `p` for `p` = 1..6, through `lcd_fb_page()`.
2. Set the 128 bytes of page 7 to 0.
3. Mark pages 1-7 dirty with `lcd_fb_mark_dirty()`.
4. Page 0 is never written or marked.

`free_input_clear()`:

- All pixels of rows 1-7 go off, x 0-127. Page 0 stays as it is.
- Cursor row 1, column 0. Pattern `MORSE_PATTERN_EMPTY`.
- The keyer and the sidetone do not change.

`free_input_delimiter()`:

1. Column not 0: new line first.
2. Draw one line of 1 px with `gfx_fill_rect(2, 8 * row + 3, 125, 1, LCD_PIXEL_ON)`.
3. Column = 25. The pattern does not change.

`free_input_leave()`: `keyer_stop()`, then `buttons_set_enter_enabled(true)`. It draws nothing.

Invariant from `ui-ux.md` 5.9: every cell at the cursor and after it is blank. So a word space needs no drawing.

### 6.12 Buttons

The poll algorithm of `menu_subsystem_spec.md` section 4.5 stays. This section adds steps. They are binding.

New state in xdata:

- `buttons_is_enter_enabled` (`bool`). It is true after `buttons_init()`.
- A long-pending mask of the `menu/back`, `prev` and `next` bits.
- One press time (`uint16_t`) per button.

Poll steps:

1. Sample. With `buttons_is_enter_enabled` false, clear the ENTER bit of the raw sample before the debounce. ENTER
   then never gives a press edge.
2. Debounce. A released-to-pressed change of MENU, PREV or NEXT sets its long-pending bit and its press time = now.
   A pressed-to-released change clears its long-pending bit. PREV and NEXT presses that debounce in the same poll
   give the repeat to PREV (D-K3).
3. Pending press events: no change. The order stays MENU, ENTER, PREV, NEXT.
4. Long events, after the press events and before the repeat. Check MENU, PREV and NEXT in this order. Take the first
   button with its long-pending bit set and `(uint16_t)(now - press time) >= BUTTONS_LONG_PRESS_MS`. Clear its bit.
   Return its event code with `BUTTON_EVENT_LONG_FLAG`.
5. Repeat: no change. A repeat that falls due in the same poll as a long event comes on a later poll.

Hold length (D-K4): the hold counts on the debounced state. Step 2 runs before step 4. Take the poll at press time +
`BUTTONS_LONG_PRESS_MS`. If the debounced state is still pressed after the debounce of that poll, the long event
comes. Else no long event comes.
Both edges of the debounced state lag the pin by `BUTTONS_DEBOUNCE_MS` (20 ms). With clean edges, the pin hold and
the debounced hold have the same length. Measured from the press event, the pin must still be closed at
`BUTTONS_LONG_PRESS_MS - BUTTONS_DEBOUNCE_MS` = 780 ms. A pin that opens at 780 ms gives no long event. A pin that
opens at 781 ms gives one long event at 800 ms.

`buttons_init()` clears the long-pending mask and sets `buttons_is_enter_enabled` to true. A button held at start-up
gives no long event, because it has no press edge.

`buttons_set_enter_enabled(false)`: `buttons_is_enter_enabled` = false. Clear the ENTER pending bit.

`buttons_set_enter_enabled(true)`:

1. Read the raw pins once and `tick_now()` once.
2. The ENTER candidate bit and the ENTER debounced bit take the raw ENTER bit.
3. The ENTER change time = now. Clear the ENTER pending bit.
4. `buttons_is_enter_enabled` = true. The other buttons do not change.

Result: a contact held at the mode end gives no ENTER until it opens and closes again.

ENTER has no long event and no repeat. One P3.2 closure is never both ENTER and a key element. The keyer runs only
while ENTER is disabled, and the keyer masks the contact that is closed at its start.

### 6.13 `Paddle` setting

- Parameters: min 0, max 1, step 1, default 0 (`Normal`). Choice texts `Normal` and `Reversed` (UX-32).
- Storage: bit 1 of the flags byte (record byte 10). Show CW Code stays in bit 0. Add
  `RECORD_FLAG_PADDLE_SWAP 0x02U`. The offset table entry of the new id is `RECORD_FLAGS`.
- `settings_get(SETTINGS_ID_SHOW_CW_CODE)` returns flags bit 0. `settings_get(SETTINGS_ID_PADDLE_SWAP)` returns flags
  bit 1 as 0 or 1.
- `settings_store()` for these two ids changes only the bit of the id.
- Load: the flags mask becomes `RECORD_FLAG_SHOW_CW_CODE | RECORD_FLAG_PADDLE_SWAP`. Bits 2-7 are written as 0 and
  ignored on load.
- No layout change, no version change. A record of the earlier firmware has bit 1 = 0. So it reads as `Normal`, and
  its CRC stays valid.
- Test vectors (layout version 1). Default record: `43 57 01 58 02 20 20 14 14 02 01 05 08 20 54` (no change).
  Default record with `Reversed`: `43 57 01 58 02 20 20 14 14 02 03 05 08 20 78`.
- Section 17 adds bits 2-3 and layout version 2. Both vectors above stay valid as version 1 input records
  (section 17.4.2). Section 17.4.2 gives the version 2 vectors.

### 6.14 Menu

`menu_enter_item()`, mode item `MENU_MODE_FREE_INPUT`: screen `MENU_SCREEN_FREE_INPUT`, redraw pending, then
`free_input_enter()`. Receiving and Sending keep `MENU_SCREEN_PLACEHOLDER`.

`menu_handle()`. Let `is_long` = the event has `BUTTON_EVENT_LONG_FLAG`.

- Screens `MENU_SCREEN_PLACEHOLDER` and `MENU_SCREEN_FREE_INPUT`:
  - An event without `is_long` does nothing.
  - A long MENU: in Free Input, `free_input_leave()` first. Then screen `MENU_SCREEN_LIST` with a redraw. The cursor
    stays on the mode item.
  - A long PREV in Free Input: `free_input_clear()`. A long NEXT in Free Input: `free_input_delimiter()`.
  - A long PREV or NEXT on a placeholder does nothing.
- All other screens: an event with `is_long` does nothing. Other events behave as today.
- "Does nothing" means no state change and no redraw request (`ui-ux.md` 6.1).

`menu_poll()`: on screen `MENU_SCREEN_FREE_INPUT` with no redraw pending, call `free_input_poll()`. The Tone preview
part does not change.

`menu_render()` on `MENU_SCREEN_FREE_INPUT`:

- It draws only the title bar with the label of the item (`Free Input`).
- It uses the code that draws the placeholder title, after its usual full clear.
- No other path sets a redraw request while Free Input is open.

`menu_show_message()`: on screen `MENU_SCREEN_FREE_INPUT`, call `free_input_leave()` first.

`menu_tree.c`: new 7th item of the CW node (UX-32):

- Label `Paddle`, kind `MENU_KIND_CHOICE`, target `SETTINGS_ID_PADDLE_SWAP`.
- Texts `Normal` and `Reversed`, field width 8.
- The existing `_Static_assert` on 7 visible rows holds.

### 6.15 `main.c`

Start-up: call `keyer_init()` after `sidetone_init(...)`. Then `lcd_init()` and the rest as today. The main loop does
not change: Free Input runs from `menu_poll()`.

### 6.16 `sidetone.c`

Only the end of the file changes (D-K1):

```c
#if defined(__SDCC_mcs51)
#define KEYER_SIDETONE_ON() (sidetone_is_on_requested = true)
#define KEYER_SIDETONE_OFF() (sidetone_is_on_requested = false)
#include "cw/keyer_service.h"
#define PLAYER_SIDETONE_ON() (sidetone_is_on_requested = true)
#define PLAYER_SIDETONE_OFF() (sidetone_is_on_requested = false)
#include "cw/player_service.h"

void sidetone_timer0_isr(void) __interrupt(TIMER0_INTERRUPT) {
  keyer_service();
  player_service();
  sidetone_service();
}
#endif
```

The player lines come from D-T1 of `training_spec.md`.

The menu Tone preview and the keyer never write `sidetone_is_on_requested` at the same time. The preview runs only in
the Tone edit, and the keyer only in Free Input. `keyer_stop()` clears `keyer_is_enabled` before it calls
`sidetone_set_on(false)`.

## 7. Memory placement

| Variable | Qualifier | Writer | Readers |
| --- | --- | --- | --- |
| `keyer_is_enabled` | `volatile MEM_BIT`, `keyer.c` | main | ISR |
| `keyer_is_start_requested` | `volatile MEM_BIT`, `keyer.c` | main sets, ISR clears | ISR |
| `keyer_is_overflow` | `volatile MEM_BIT`, `keyer.c` | ISR sets, main clears | both |
| `keyer_is_straight`, `keyer_is_iambic_a`, `keyer_is_swapped` | `volatile MEM_BIT`, `keyer.c` | main, only while disabled | ISR |
| `keyer_queue_head` | `volatile MEM_DATA uint8_t`, `keyer.c` | ISR | both |
| `keyer_queue_tail` | `volatile MEM_DATA uint8_t`, `keyer.c` | main | both |
| `keyer_queue[8]` | `volatile MEM_XDATA uint8_t`, `keyer.c` | ISR, free slots only | main |
| The four tick values | `volatile MEM_XDATA uint16_t`, `keyer.c` | main, only while disabled | ISR |
| ISR-private state of section 5.6 | `static MEM_DATA` and `static MEM_BIT`, `keyer_service.h` | ISR only | ISR (tests read them) |
| Work values of the timing math | `static MEM_XDATA`, `keyer.c` | main | main |
| Free Input cursor, pattern, glyph buffer, prosign style, text buffer (section 17.5) | `static MEM_XDATA` | main | main |
| Buttons: enter flag, long-pending mask, press times | `static MEM_XDATA` | main | main |
| `morse_table` | `static const MEM_CODE char[128]` | - | main |
| Prosign names and marks (section 17.4.1) | `static const MEM_CODE` | - | main |

Rules:

- Each shared access is one byte or one bit, so every main-loop read and write is atomic.
- No multi-byte value is shared while the ISR can change it. The 16-bit tick values change only while the keyer is
  disabled.
- The ISR never reads the keyer parameters of a stopped keyer.

## 8. ISR constraints and budget

- `keyer_service()` and `sidetone_service()` are leaves. They have no `lcall` and use no library helper (`__mulint`,
  `__divuint`, `__moduint`, `__mullong`, `__divulong`, `__modulong` or any other `__` routine). A 16-bit decrement,
  increment or compare compiles to plain instructions.
- `keyer_service()` uses `#pragma nooverlay`. Its locals are in registers or DSEG, never in OSEG.
- The ISR has no `P_SW2` access, no `EA` or `IE` byte write and no port 2 write. The keyer reads pin bits of P3 and P5
  only and never writes them.
- The ISR frame does not grow. The service calls are at the same depth (`sidetone_spec.md` section 9: 14 pushes plus
  the return addresses). D-T1 of `training_spec.md` adds `player_service` at the same depth.
- ISR time covers the whole Timer 0 ISR: entry, `keyer_service`, `sidetone_service`, exit. Count it from the `.asm`
  with the STC8H instruction timing. Target 400 clocks. Hard limit 800 clocks, of 6000 per 250 us.
- Also report the keyer part alone. Also report the disabled path (menu). The disabled path must not grow by more than
  10 clocks.
- Worst-path candidates: a gap-end call that starts an element while the sidetone starts from OFF with a pending tone.
  A mark-end call that pushes an event. Name the path that you count.
- D-K2 adds the start latch to the start path. With it, a worst case up to 420 clocks is accepted with no new decision.
  Report the new count and the path. Above 420 clocks, stop and report to `architector`.

## 9. Build and static checks

- The `src/` glob of `firmware/CMakeLists.txt` takes `src/cw/` and `src/modes/`. No firmware CMake change.
- `tools/check_asm.sh` R5: add `P5` 0xC8, `P5M1` 0xC9 and `P5M0` 0xCA (only `ORL` or `ANL` byte writes). Add the
  predefined names `p5`, `p5m1` and `p5m0`. Update the header comment.
- New self-tests: one accept case (`orl` or `anl`) per new register. One `r5_` reject case (`mov`) per new register.
  One accept case with bit operations on `P5_5` (0xCD).
- R1-R4 apply to the new files with no change.

## 10. Host test infrastructure

### 10.1 `Tests/fakes/board/paddle_pins.h`, `fake_paddle_pins.c/.h`

The fake header maps `PADDLE_PINS_CONFIGURE()`, `PADDLE_PINS_IS_TIP_CLOSED()` and `PADDLE_PINS_IS_RING_CLOSED()`
to fake functions. Minimum API (the names are binding, more is allowed):

```c
void fake_paddle_pins_reset(void);
void fake_paddle_pins_set_tip_closed(bool is_closed);
void fake_paddle_pins_set_ring_closed(bool is_closed);
size_t fake_paddle_pins_configure_count(void);
size_t fake_paddle_pins_read_count(void);   /* tip and ring reads together */
```

### 10.2 `Tests/fakes/fake_keyer.c/.h`

It implements `keyer.h` for the menu and Free Input groups. Minimum API:

```c
void fake_keyer_reset(void);
void fake_keyer_push_event(keyer_event_t event);    /* FIFO of at least 64 events; overflow fails the test */
size_t fake_keyer_init_count(void);
size_t fake_keyer_start_count(void);
size_t fake_keyer_stop_count(void);
bool fake_keyer_is_running(void);                   /* after start, before stop */
uint8_t fake_keyer_last_key_type(void);
bool fake_keyer_last_is_swapped(void);
uint8_t fake_keyer_last_wpm(void);
uint8_t fake_keyer_last_eff_wpm(void);
```

`keyer_read_event()` of the fake returns the FIFO events in order, then `KEYER_EVENT_NONE`.

### 10.3 Keyer test harness

A keyer test defines `KEYER_SIDETONE_ON()` and `KEYER_SIDETONE_OFF()`, includes `cw/keyer_service.h` and calls
`keyer_service()` once per tick. Two styles are allowed:

- A test variable that records the request and its call number.
- The real `sidetone_set_on()` of `sidetone.c`, with `sidetone_service()` after each `keyer_service()`. This is an
  integration test with `fake_sidetone_pwm`.

### 10.4 `Tests/CMakeLists.txt`

| Pattern | Group sources |
| --- | --- |
| `KEYER_TEST_PATTERN "^test_(qa_)?keyer"` | `src/cw/keyer.c`, `fakes/fake_paddle_pins.c`, `src/sidetone/sidetone.c`, `fakes/fake_sidetone_pwm.c`, `fakes/fake_tick.c`, `src/settings/settings.c`, EEPROM sources. `TARGET_DEFINITIONS`. |
| `MORSE_TEST_PATTERN "^test_(qa_)?morse"` | `src/cw/morse.c`. |
| `FREE_INPUT_TEST_PATTERN "^test_(qa_)?free_input"` | Section 10.5. `TARGET_DEFINITIONS`. |
| menu group (existing) | Adds `src/modes/free_input.c`, `src/cw/morse.c` and `fakes/fake_keyer.c`. |

### 10.5 Free Input group sources

- `src/modes/free_input.c`, `src/cw/morse.c`.
- GFX sources, with `lcd_fb.c`.
- `fakes/fake_keyer.c`, `fakes/fake_sidetone.c`.
- `src/settings/settings.c` and the EEPROM sources.
- `src/buttons/buttons.c`, `fakes/fake_tick.c`, `fakes/fake_delay.c`, `fakes/fake_lcd_pins.c`,
  `fakes/fake_button_pins.c`.

Add the new patterns to `MODULE_TEST_PATTERNS`. All host flags stay. The sidetone group does not change: the include
of `keyer_service.h` in `sidetone.c` is target only.

## 11. Acceptance criteria

Each criterion maps to a host test, a script or a review. Developer test files: `test_keyer*.c`, `test_morse*.c`,
`test_free_input*.c`, `test_buttons*.c`, `test_settings*.c`, `test_menu*.c`. QA test files: `test_qa_*.c`.

Keyer (`test_keyer*.c`):

| ID | Criterion |
| --- | --- |
| AC-K1 | Filter edges. From settled open, a clean close votes closed on the 5th sample. From settled closed, a clean open votes open on the 5th sample. Tip and ring filter independently. |
| AC-K2 | Glitches. 1, 2 and 3 opposite samples never change a settled state, in both directions. An alternating input never changes a settled state. |
| AC-K3 | Entry mask. A contact closed at `keyer_start` gives no mark, no sidetone request and no event while it stays closed. Test 2 s (8000 calls). After it votes open, the mask clears with no event. The next closure keys normally. Both contacts, all three key types. |
| AC-K4 | Mono plug. Straight mode, ring closed through the whole test: the tip keys normally. |
| AC-K5 | Swap. Iambic, `Normal`: the tip gives dits and the ring gives dahs. `Reversed`: the opposite. Straight: no difference. |
| AC-K6 | Straight. Tip only, ring only and both key. The STRAIGHT_DOWN length equals the count of calls with a logical contact closed. At 20 WPM a mark of 479 calls gives DIT, and 480 calls give DAH. |
| AC-K7 | Straight sidetone and saturation. The sidetone request is on exactly during STRAIGHT_DOWN. A mark longer than 65 535 calls gives DAH. |
| AC-K8 | Exact timing at 5, 20 and 50 WPM. A dit lasts exactly `dit` ELEMENT calls and a dah `3 dit`. Every gap lasts exactly `dit` GAP calls. Cases: a single element, a held repeat of 5 or more elements, a held squeeze. |
| AC-K9 | Dit formula. `keyer_dit_ticks` equals the formula of section 6.7 for every WPM 5..50. |
| AC-K10 | Iambic decisions: one dit, one dah, a held repeat, and a held squeeze that alternates and starts with a dit. An opposite tap during ELEMENT and during GAP adds that element in A and B. A same-lever tap during GAP repeats the element. |
| AC-K11 | A versus B. The squeeze opens logically at one of these calls: early in an element, the second call of an element, the last ELEMENT call, in the gap, the gap-end call. In each case B gives the sequence of A plus exactly one element of the other kind. A sweep over every release call at 50 WPM, for at least 10 dits after the start, finds no exception (D-K2). |
| AC-K12 | Overlap tap. The rule of section 6.5 holds for A and for B. |
| AC-K13 | Early release. For every generated element, the sidetone request goes off on the call where 32 ELEMENT calls remain. `KEYER_IS_MARK_ACTIVE()` still lasts the full N calls. |
| AC-K14 | Envelope integration. The test runs the real `sidetone_service()` after each `keyer_service()`. The compare 0 write of the fall comes on the last ELEMENT call. Cases: 5, 20 and 50 WPM, speaker and headphones. |
| AC-K15 | Gap events. CHAR_GAP comes once, on the call where the space reaches `2 dit` calls. WORD_GAP comes once, on the call where it reaches `5 fu`. A new mark before the threshold cancels the event. No gap event comes before the first mark after a start. Straight and Iambic. |
| AC-K16 | Farnsworth. Every row of the table in section 6.7 holds. |
| AC-K17 | Queue. 8 events are stored and read in order. The 9th sets the overflow bit and is dropped. Events while the bit is set are dropped. A read gives the 8 events, then `KEYER_EVENT_OVERFLOW`, then `KEYER_EVENT_NONE`. After that, new events flow. |
| AC-K18 | Queue wrap. The order holds across the wrap of the head and tail bytes. Test at least 300 events. |
| AC-K19 | One event per call. In a long mixed run, the head never moves by more than 1 per call. QA adds random contacts, all key types and 5..50 WPM. |
| AC-K20 | Clamps in `keyer_start`. `wpm` 0 and 4 give 5. `wpm` 51 and 255 give 50. `eff_wpm` above `wpm` gives `wpm`. `eff_wpm` 0 gives 5. Key type 3 and 255 give Iambic B. |
| AC-K21 | Start during an element. The old element ends at once with no event. The next call reseeds. The queue is empty. No stale memory acts. |
| AC-K22 | WPM range. `KEYER_WPM_MIN == settings_min(SETTINGS_ID_TX_WPM)` and `KEYER_WPM_MAX == settings_max(SETTINGS_ID_TX_WPM)`. |
| AC-K23 | Stop. After `keyer_stop`, the next calls change no keyer state and push no event. `sidetone_set_on(false)` comes after `keyer_is_enabled` is 0. A disabled `keyer_service()` reads no pin: `fake_paddle_pins_read_count()` stays the same. |
| AC-K24 | Init. `keyer_init` configures the paddle pins once and leaves the keyer disabled. |
| AC-K25 | Start latch (D-K2). On the start call of a gap-end element with both levers closed, the memory of the other lever and `keyer_was_squeezed` are set after the call. With a single lever closed, the start call sets no memory. The held-squeeze case of section 6.5 gives the other element next, in A and in B. |

Decoder, Free Input, buttons, settings, menu:

| ID | Criterion |
| --- | --- |
| AC-K40 | Morse table. The 54 entries of section 6.10 decode. Every other index 0..127 gives `MORSE_CHAR_UNKNOWN`. Index 128..255 gives unknown. The table has no lowercase letter, no space, no `*`, no `<` and no `>`. Section 17 changes entries 40, 42, 49 and 54 (AC-P1). |
| AC-K41 | Pattern builder. `morse_add_element` builds the index. A 7th element gives `MORSE_PATTERN_INVALID`, and invalid stays invalid. The 8-dit pattern is unknown. |
| AC-K42 | Free Input text. Unit tests in `test_free_input*.c` cover the cursor rules, the lazy per-character wrap and the word-space rules. `*` shows for unknown, invalid and overflow. `<SK>` takes four cells and can split across a line end. An empty CHAR_GAP draws nothing. Prosigns with a mark: AC-P17 to AC-P20. |
| AC-K43 | Free Input scroll, clear and delimiter. A scroll copies pages 1-6 and clears page 7. Page 0 is never written or dirty. Clear and delimiter follow section 6.11. |
| AC-K44 | Free Input calls. `free_input_enter` call order and arguments. `free_input_leave` call order. At most `KEYER_QUEUE_SIZE + 1` events per poll. |
| AC-K45 | Long press. One long event per press, at exactly `BUTTONS_LONG_PRESS_MS` after the press event. The hold counts on the debounced state (D-K4, section 6.12): a pin that opens `BUTTONS_LONG_PRESS_MS - BUTTONS_DEBOUNCE_MS` ms after the press event gives none, and a pin that opens 1 ms later gives one. ENTER gives none. A button held at start-up gives none. |
| AC-K46 | Long press order. With a long event and a repeat due together, the long event comes first (V-75). `prev` and `next` held together each give their own long event at their own moment. PREV and NEXT presses that debounce in the same poll give the repeat to PREV (D-K3). |
| AC-K47 | Long press bounce. A release bounce shorter than the debounce does not cancel or restart the hold. `BUTTON_EVENT_CODE()` removes both flags. |
| AC-K48 | Enter ownership. Disabled: no ENTER event, and a pending ENTER is dropped. Enabled with P3.2 low: no ENTER until a release and a new press. Enabled with P3.2 high: the next press gives ENTER. The other buttons do not change in any case. |
| AC-K49 | Paddle setting values. Paddle get, set, min, max, step and default. The flags byte holds Show CW Code in bit 0 and Paddle in bit 1. Each id changes only its own bit. |
| AC-K50 | Paddle setting record. Both record vectors of section 6.13 load and save. An earlier record reads as `Normal`. Flags bits 2-7 are ignored on load. A change and a change back cause no save. |
| AC-K51 | Menu V-tests. V-54 to V-79 of `ui-ux.md` section 9.2 pass with `fake_keyer.c`. |
| AC-K52 | Changed existing tests. Placeholder tests change from a short to a long `menu/back` (R-9). Other existing tests change only where sections 6.12 to 6.14 change the expected result (D-K6). Allowed causes: the long event of a held button, 11 settings ids, the 7th CW item, Free Input as a mode screen. No change removes a check or loosens an expected value. |
| AC-K53 | Main. `keyer_init()` comes after `sidetone_init()` and before `lcd_init()`. |

Build and review:

| ID | Criterion |
| --- | --- |
| AC-K60 | ISR calls. `sidetone_timer0_isr` has exactly the calls `lcall _keyer_service`, `lcall _player_service` and `lcall _sidetone_service`, in this order (D-T1 of `training_spec.md`, `check_asm.sh` R6). No service has an `lcall` or a library helper. |
| AC-K61 | ISR assembly rules. Locals of both services are not in OSEG. Neither service has a `P_SW2` access, an `EA` or `IE` byte write, or a write to port 2, P3 or P5. The ISR ends with `reti`. No `.asm` file has an `sdcc_atomic` symbol. |
| AC-K62 | ISR time within section 8, with the counted path named. |
| AC-K63 | Memory within section 12, with the full report. |
| AC-K64 | `firmware/test.sh` passes. All host tests pass under ASan and UBSan. The `check_asm.sh` self-test passes with the new R5 cases. |
| AC-K65 | `firmware/build.sh` passes, and `check_asm.sh` reports no violation. |
| AC-K66 | No regression. All existing host tests pass. Existing tests change only as AC-K52 allows. |


## 12. Memory budget and report

Baseline (HEAD `4e9b9d2`, `build/putikeeg_cw_trainer.mem`): flash 15 913 / 61 440 B, xdata 1346 / 3072 B, stack
start 0x30 with 208 B free. T0 measures it again.

| Resource | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- |
| Flash added by the whole change | at most 3072 B | 4096 B |
| xdata added | at most 64 B | 128 B |
| New static internal RAM: DSEG + overlay + bit bytes | at most 16 B | 24 B |
| Stack margin: free stack minus (deepest main-loop use + largest ISR frame) | | at least 64 B |
| Timer 0 ISR, worst case | 400 clocks | 800 clocks |

Report table from `build.sh`, `.map` and `.mem`, before and after each step:

| Step | Flash B | DSEG B | Overlay B | Bit bytes | xdata B | Stack start | Stack free B | Deepest main-loop stack B | Largest ISR frame B | Margin B |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |

Also report:

- Flash of `keyer.c`, `morse.c` and `free_input.c`, and of the keyer part of `sidetone.c`.
- Newly linked library helpers.
- The stack method: each `lcall` adds 2, each `push` adds 1, an ISR entry adds 2 plus its pushes.
- The deepest main-loop chain, by name. Candidate 1: `menu_poll` -> `free_input_poll` -> `gfx_draw_text` ->
  `font_u8g2_draw_glyph` -> `lcd_fb_set_pixel`. Candidate 2: `menu_handle` -> `free_input_enter` -> `keyer_start`
  -> `__divulong`.
- The main-loop time of one scroll, estimated from the `.asm`.

## 13. Tasks

`c-embedded-developer` owns T0-T6. Each step ends with these items:

- `firmware/build.sh` passes with `check_asm.sh`.
- `firmware/test.sh` passes.
- The memory table, the list of assumptions and the open device checks.

### D1 UI specification (`ui-ux-embedded-designer`)

- Done: `ui-ux.md` is final for this change. It holds sections 5.6, 5.9, 6.1, 6.2, 6.6, 6.7, UX-23 to UX-37, V-54 to
  V-79 and D-15 to D-24.
- `architector` reviews it against sections 6.10 to 6.14 before T5 starts.

### T0 Baseline

- Rebuild and run the tests. Record the memory table, the stack estimate and the ISR clock count of HEAD.

### T1 Contract, stubs and fakes

- Outputs: all headers of section 5 with stub bodies, the `stc8h.h` additions, `board/paddle_pins.h`, the fakes of
  section 10, the CMake groups, and the `check_asm.sh` R5 extension with self-tests.
- Stub bodies are silent and build. The stub `keyer_service()` returns at once. `sidetone.c` does not change yet.
- Acceptance: both scripts pass. Every value of section 4 has a datasheet citation. QA can write tests from here.

### T2 Keyer core and ISR hook

- Inputs: sections 3.1, 6.1 to 6.9, 7 and 8.
- Outputs: `keyer_service.h`, `keyer.c`, `test_keyer*.c`, the end of `sidetone.c` (section 6.16), `main.c`
  (section 6.15).
- Acceptance: AC-K1 to AC-K24, AC-K53, AC-K60 to AC-K62.

### T3 Morse decoder

- Outputs: `morse.c`, `test_morse.c`. Acceptance: AC-K40, AC-K41.

### T4 Buttons and settings

- Outputs: the changes of sections 6.12 and 6.13, with tests. Acceptance: AC-K45 to AC-K50.

### T5 Free Input and menu

- Inputs: the D1 review, sections 6.11 and 6.14.
- Outputs: `free_input.c`, the menu changes, the `menu_tree.c` item, `test_free_input*.c`, menu tests for V-54 to
  V-79, and the R-9 test changes.
- Acceptance: AC-K42 to AC-K44, AC-K51, AC-K52, AC-K66.

T2, T3 and T4 can run in any order after T1. T5 needs T3, T4 and the D1 review.

### T6 Developer close-out

- Final report: all memory tables, the stack analysis, the ISR clock count, the asm review of AC-K60 and AC-K61, the
  test lists, the assumptions and the open device checks.

### Q1 QA (`qa-embedded`)

- QA starts its tests after T1 and runs its verification after T6.
- `test_qa_keyer*.c`: fault injection for AC-K2, AC-K3, AC-K11, AC-K17, AC-K19, AC-K21 and AC-K23.
- Fault cases: random bounce, a release on every call around the gap end, a start and a stop in every phase, an
  overflow under a stalled reader.
- Long random runs against an independent reference model of section 6.
- `test_qa_free_input*.c`, `test_qa_buttons*.c` and `test_qa_menu*.c` as QA decides.
- Independent review of every criterion, the `.asm` of the ISR path and the stack count.
- Verdict: `approved`, `changes_requested` or `blocked`.

### T7 Review changes (`c-embedded-developer`, after the final review)

- Inputs: section 3.3, the start latch of section 6.5, section 8.
- Outputs: the start latch in `keyer_service.h`, developer tests for AC-K11 and AC-K25, the comment fix of the review.
- Acceptance: AC-K11, AC-K25, AC-K60 to AC-K65, and the memory and ISR report of sections 8 and 12.

### Q2 QA of the review changes (`qa-embedded`, after T7)

- The reference model of `test_qa_keyer*.c` follows the start latch. The squeeze sweep expects B = A + 1 for every
  release that gives an element.
- `test_qa_buttons*.c` checks D-K3.
- Verdict on T7 and Q2.

### W1 Documentation (`technical-writer`, after the final `approved`)

- `llm.md`: add this document to the documents table. Update the ISR rule: an ISR calls only functions of its own
  file. A service of another module comes into the ISR file through a header (D-K1).
- `components_and_wiring.md`: firmware use of P5.5 and P3.2.
- `menu_subsystem_spec.md`: long-press events, `buttons_set_enter_enabled`, the Paddle setting, flags bit 1.
- User manual and release notes: Free Input, key types, Paddle, long presses. Tell the user to insert the plug before
  Free Input opens (UX-37), and how to leave a mode screen (UX-36).

## 14. Device checks (unverified until the user tests on the device)

The UI device checks are D-15 to D-24 of `ui-ux.md`. These keyer checks come in addition:

| ID | Check |
| --- | --- |
| DC-K1 | P5.5 idles high with the internal pull-up, with no plug and with an open paddle. Ring to sleeve pulls it low. Record the level. |
| DC-K2 | Jack sleeve to ground, tip to P3.2: the polarity is as assumed (research "Hardware facts", checks 1-4). |
| DC-K3 | Element and gap lengths on the logic analyzer at 5, 20 and 50 WPM. The expected error is the oscillator trim only. The 8 ms fall ends at the mark end. |
| DC-K4 | Iambic A and B squeeze release with a logic capture of P1.1 or P1.2 and the paddle pins. B adds one element, A does not (with D-18). |
| DC-K5 | Timer 0 ISR time and jitter while the text scrolls. This needs a measurement build. Ask `architector` first. |
| DC-K6 | Contact bounce of a real paddle: no split elements at light and firm pressure. |
| DC-K7 | No EEPROM write happens on the way into Free Input, during it, or on the way out. So DC-12 of `sidetone_spec.md` cannot happen in Free Input. |

## 15. Risks

| Risk | Handling |
| --- | --- |
| The P5.5 pull-up does not work on the fitted silicon. | DC-K1. Fallback, an `architector` decision: quasi-bidirectional mode with latch 1, or an external pull-up. No code for it now. |
| ISR time with the keyer goes above the target. | Section 8 count. The disabled path stays one bit test. |
| A slow main loop overflows the queue. | About 300 ms of margin. Defined overflow: the next character shows `*`. |
| A contact that closes after the start gives an endless tone. | User decision UX-37: no timeout. Recovery: long `menu/back`. The manual tells the user to insert the plug first. |
| SDCC places keyer locals in OSEG or adds a rollback epilogue. | AC-K60 and AC-K61. Stop and report. |
| Deeper Free Input call chains use more stack. | Section 12 margin, DC-9 of `sidetone_spec.md`. |

## 16. Open questions and follow-ups

The user resolved Q-7 (UX-36), Q-8 (UX-37) and Q-9 (UX-28).

| ID | Item | Owner | Effect on this contract |
| --- | --- | --- | --- |
| Q-K1 | Iambic A overlap tap (section 6.5). The other lever closes while the current lever is held, and both open before the gap end. Then A adds no element. This follows the research rule. Some commercial keyers act differently. | user (for information) | None. A different rule needs a new decision. |
| F-1 | `FONTS_ID_8X16` is linked but no screen uses it. It costs about 3.3 KB of flash. | `architector`, separate change | None. Not part of this change. |

## 17. Prosign display setting

This section is the binding contract of the change (2026-10-06). Where it differs from sections 1 to 16, this section wins. The precedence rules of section 1
apply: `ui-ux.md` wins for labels, strings, layout and mockups. This section wins for codes, interfaces, record
format, behavior and memory.

### 17.1 Scope and user decisions

User decisions (fixed):

- Four prosigns have the same code as a punctuation mark. All four follow the new setting.

| Prosign | Code | Mark |
| --- | --- | --- |
| AR | `.-.-.` | `+` |
| BT | `-...-` | `=` |
| KN | `-.--.` | `(` |
| AS | `.-...` | `&` |

- SK has no mark. It shows as `<SK>` for every value of the setting.
- The values, with AR as the example:

| Value | AR shows as |
| --- | --- |
| `Punctuation` | `+` |
| `Prosign` | `<AR>` |
| `Both` | `<AR/+>` |

- The default is `Both`. A device with an old settings record (layout version 1) also gets `Both`. The other
  settings of the old record stay as they are.
- The menu item goes in the `Settings` list (5 items now, 7 visible rows). The CW list is full.

In scope:

- Decoder codes for AR, BT, KN and AS, and a formatter that turns a decoded value into display text.
- The setting, flags bits 2-3, layout version 2 and the load of a version 1 record.
- The menu item and the size of the menu value text buffer.
- Free Input text.
- Host tests, the memory report.

Out of scope:

- Receiving and Sending modes. They can use `morse_format()` later.
- Any change to an ISR, to the keyer, to `sidetone.c` or to `firmware/drivers/`.
- `Show CW Code` behavior. Menu scrolling.

### 17.2 Decisions

| ID | Decision | Reason | Rejected alternatives |
| --- | --- | --- | --- |
| D-P1 | Table entries 40, 42, 49 and 54 hold the codes `MORSE_CHAR_AS`, `MORSE_CHAR_AR`, `MORSE_CHAR_BT` and `MORSE_CHAR_KN` (0x02 to 0x05). | The decoder tells a prosign from a mark it never gives. The table still holds no UI glyph (R-10). | Keep the marks in the table and map them back in the UI (then a later table entry for a real `+` is not possible). |
| D-P2 | The formatter `morse_format()` is in `morse.h` and `morse.c`. The style codes `MORSE_STYLE_*` are in `morse.h`. `cw/` does not include `settings.h`. `free_input.c` checks at compile time that `MORSE_STYLE_*` equal `SETTINGS_PROSIGN_*`, with the `#if` / `#error` pattern of `keyer.c`. | The later Receiving and Sending modes use the same text. The morse test group links only `morse.c`. The `KEYER_KEY_*` constants use the same pattern. | A new `cw/morse_text.c` module (one more file for about 100 bytes). Pass the settings id into `cw/` (a `cw` to `settings` dependency). |
| D-P3 | `morse_format()` gives the text for every decoded value: `*` for `MORSE_CHAR_UNKNOWN` and for any value that is not printable, `<SK>` for SK. | Free Input gets one draw path. Later modes show the same text with no copy of the rules. | The caller handles unknown and SK (two more branches in each mode). |
| D-P4 | A style value above `MORSE_STYLE_BOTH` acts as `MORSE_STYLE_BOTH`. | Defined result for every input. `Both` is the default. | Treat it as `Punctuation` (not the default). Undefined output. |
| D-P5 | The setting is in bits 2-3 of the flags byte (record byte 10). The record size stays 15 bytes. `settings.c` gets a `MEM_CODE` shift table next to the mask table. `settings_store()` stays a leaf. | No new record byte. A table keeps the flag code generic and leaf. | A new record byte (changes the size and every offset after it). An `if` on the id in `settings_store()` (less generic). |
| D-P6 | `SETTINGS_LAYOUT_VERSION` becomes 2. `settings_load()` also accepts a version 1 record with a valid CRC. It loads its values, ignores flags bits 2-7 and sets `Prosigns` to its default. The status is `SETTINGS_LOAD_OK`. The baseline is the result of this load. No EEPROM write happens at load. | User decision: an old record gives `Both` and keeps the other settings. A version 1 record has bits 2-3 = 0, which would read as `Punctuation`. | Keep version 1 and store `value XOR 2` in bits 2-3, so that 0 reads as `Both` (a hidden encoding: the record value differs from the setting value, and every later field needs the same trick). A new status `SETTINGS_LOAD_MIGRATED` (`main.c` ignores the status, and a caller that checks for `SETTINGS_LOAD_OK` would treat a good record as bad). A write at load (an EEPROM erase at each start-up of an updated device, against UX-15). |
| D-P7 | `VALUE_TEXT_SIZE` in `menu_render.c` becomes 12. | `Punctuation` has 11 characters. With 10 bytes, `append_text()` cuts it to `Punctuat` with no error. | A shorter value text (needs a user decision, D2 item 2). |
| D-P8 | The item is a `MENU_KIND_CHOICE` item at index 2 of the Settings list, after `Show CW Code`. Field width 11. Label `Prosigns` (`ui-ux.md` UX-38). | Approved plan. `Show CW Code` and this item both change how characters show. The Settings list gets 6 items, below `MENU_VISIBLE_ROWS`. | The CW list (full). The Display list (it holds screen hardware settings). |
| D-P9 | `free_input_enter()` reads the style once and keeps it in xdata. A change applies at the next Free Input start. | Same rule as `Paddle` (UX-32). The settings cannot change while Free Input is open. One `settings_get()` call per mode start. | A `settings_get()` call for each character (more calls, no gain). |
| D-P10 | Existing tests can change for these causes only: the Morse table holds the prosign codes in entries 40, 42, 49 and 54 in place of the marks (D-P1); `SETTINGS_ID_COUNT` is 12; the layout version is 2, so a saved record has version 2 and a new CRC; a version 1 record now loads; the Settings list has 6 items, and the items from index 2 on move down by one row; the default style `Both` changes the text of AR, BT, KN and AS in Free Input. A test of the old text either sets `Prosigns` = `Punctuation` in its setup or expects the new text. | The change moves these values. | - |

| D-P11 | Accepted after P5 (2026-10-06). (a) clang-tidy `bugprone-easily-swappable-parameters` on `morse_format(char decoded, uint8_t style, ...)` stays, with no suppression and no signature change. (b) The prosign data is one flat 15-byte `MEM_CODE` table (3 bytes per code). (c) Table slots 40, 42, 49 and 54 use the literals `'\x05'`, `'\x02'`, `'\x03'` and `'\x04'` with a comment, and the default of id 11 is the literal `2U`, because clang-format breaks the table layout with the names. Host tests check each literal against `MORSE_CHAR_*` and `SETTINGS_PROSIGN_BOTH`. | (a) The same finding is accepted for the coordinate and color parameters (`menu_subsystem_spec.md` section 15). The two types differ, and AC-P2 to AC-P4 test every style and value, so a swap fails the tests. (b) It meets 17.4.1 and saves 10 B. (c) The tests catch a wrong literal. | (a) A struct parameter or a separate function per style (more flash, no gain). A `NOLINT` (adds a suppression). (b) An array of structs (10 B more). |

No change in this section changes a user decision.

### 17.3 Interface contract

Section 5 rules apply. The C types, names, values and the parameter order below are binding. Private names marked
"recommended" can change. Their values and behavior are binding.

#### 17.3.1 `inc/cw/morse.h`

The existing declarations do not change. Add the include and these items:

```c
#include "mcu/compiler.h"

#define MORSE_CHAR_AR '\x02'  /* .-.-.  mark '+' */
#define MORSE_CHAR_BT '\x03'  /* -...-  mark '=' */
#define MORSE_CHAR_KN '\x04'  /* -.--.  mark '(' */
#define MORSE_CHAR_AS '\x05'  /* .-...  mark '&' */

#define MORSE_STYLE_PUNCTUATION 0U  /* equal to SETTINGS_PROSIGN_PUNCTUATION */
#define MORSE_STYLE_NAME 1U         /* equal to SETTINGS_PROSIGN_NAME */
#define MORSE_STYLE_BOTH 2U         /* equal to SETTINGS_PROSIGN_BOTH */

#define MORSE_TEXT_UNKNOWN '*'
#define MORSE_TEXT_MAX_LENGTH 6U    /* "<AR/+>" */
#define MORSE_TEXT_SIZE 7U          /* MORSE_TEXT_MAX_LENGTH and NUL */

/**
 * Writes the display text of @p decoded, a morse_decode() result, in @p style to @p text and returns its length,
 * 1 to MORSE_TEXT_MAX_LENGTH. @p text holds MORSE_TEXT_SIZE bytes.
 */
uint8_t morse_format(char decoded, uint8_t style, char MEM_XDATA* text);
```

- `MORSE_CHAR_UNKNOWN` (0x00) and `MORSE_CHAR_SK` (0x01) do not change.
- `char MEM_XDATA* text` is a pointer to xdata. On the host, `MEM_XDATA` is empty.
- Section 17.4.1 gives the behavior.

#### 17.3.2 `inc/settings/settings.h`

```c
#define SETTINGS_ID_PADDLE_SWAP 10U    /* 0 Normal, 1 Reversed */
#define SETTINGS_ID_PROSIGN_STYLE 11U  /* SETTINGS_PROSIGN_*: 0 Punctuation, 1 Prosign, 2 Both */
#define SETTINGS_ID_COUNT 12U

#define SETTINGS_PROSIGN_PUNCTUATION 0U
#define SETTINGS_PROSIGN_NAME 1U
#define SETTINGS_PROSIGN_BOTH 2U

#define SETTINGS_LAYOUT_VERSION 2U
```

All other declarations, the statuses and the function signatures do not change. `SETTINGS_RECORD_SIZE` stays 15.
The doc comment of `settings_load()` changes: it accepts layout version 1 and version 2 (section 17.4.2).

#### 17.3.3 `src/settings/settings.c` (private)

| Item | Value | Name |
| --- | --- | --- |
| Flags mask of `SETTINGS_ID_PROSIGN_STYLE` | `0x0CU` | `RECORD_FLAG_PROSIGN_STYLE` (recommended) |
| Shift table | `static const MEM_CODE uint8_t[SETTINGS_ID_COUNT]`: the bit number of the lowest bit of the mask of the id. 0 for `SETTINGS_ID_SHOW_CW_CODE` (mask `0x01`), 1 for `SETTINGS_ID_PADDLE_SWAP` (mask `0x02`), 2 for `SETTINGS_ID_PROSIGN_STYLE` (mask `0x0C`), 0 for every id with a byte of its own (mask 0) | `settings_flag_shifts` (recommended) |
| Offset table entry of id 11 | `RECORD_FLAGS` | - |
| Min, max, step, default of id 11 | 0, 2, 1, `SETTINGS_PROSIGN_BOTH` | - |
| Flags bits kept on a version 2 load | `0x0FU` (bits 0-3) | recommended: `RECORD_FLAGS_USED` |
| Flags bits kept on a version 1 load | `0x03U` (bits 0-1) | recommended: `RECORD_FLAGS_USED_V1` |
| Oldest accepted layout version | 1 | recommended: `RECORD_LAYOUT_VERSION_1` |

#### 17.3.4 `src/menu/menu_render.c`

```c
#define VALUE_TEXT_SIZE 12U /* "Punctuation" and NUL */
```

If D2 changes a value text, the size is the longest value or choice text of the tree plus 1.

#### 17.3.5 `src/menu/menu_tree.c`

```c
static const menu_text_t MEM_CODE menu_prosign_texts[3] = {"Punctuation", "Prosign", "Both"};
```

New item at index 2 of `menu_settings_items`, after `Show CW Code`:
`{"Prosigns", NULL, menu_prosign_texts, MENU_KIND_CHOICE, SETTINGS_ID_PROSIGN_STYLE, 11U}`. The text order is
the value order: index = `SETTINGS_PROSIGN_*`. The `_Static_assert` on `MENU_VISIBLE_ROWS` stays and holds (6 <= 7).

#### 17.3.6 `src/modes/free_input.c`

- No change of `inc/modes/free_input.h`.
- New `static MEM_XDATA` state: the prosign style (`uint8_t`) and a text buffer of `MORSE_TEXT_SIZE` chars.
- A compile-time check: `MORSE_STYLE_PUNCTUATION`, `MORSE_STYLE_NAME` and `MORSE_STYLE_BOTH` equal
  `SETTINGS_PROSIGN_PUNCTUATION`, `SETTINGS_PROSIGN_NAME` and `SETTINGS_PROSIGN_BOTH`. A difference stops the build
  with `#error`.

### 17.4 Behavior

#### 17.4.1 `morse_format(decoded, style, text)`

Let `c` = `(uint8_t)decoded`. Compare as `uint8_t`, never as `char`: `char` is signed on the host and unsigned on
SDCC.

| `c` | `MORSE_STYLE_PUNCTUATION` (0) | `MORSE_STYLE_NAME` (1) | `MORSE_STYLE_BOTH` (2), and any style above 2 |
| --- | --- | --- | --- |
| 0x01 `MORSE_CHAR_SK` | `<SK>` | `<SK>` | `<SK>` |
| 0x02 `MORSE_CHAR_AR` | `+` | `<AR>` | `<AR/+>` |
| 0x03 `MORSE_CHAR_BT` | `=` | `<BT>` | `<BT/=>` |
| 0x04 `MORSE_CHAR_KN` | `(` | `<KN>` | `<KN/(>` |
| 0x05 `MORSE_CHAR_AS` | `&` | `<AS>` | `<AS/&>` |
| 0x20 to 0x7E | that character | that character | that character |
| 0x00 `MORSE_CHAR_UNKNOWN`, 0x06 to 0x1F, 0x7F to 0xFF | `*` | `*` | `*` |

Rules:

- It writes the text to `text[0]` .. `text[length - 1]` and a NUL to `text[length]`. It writes no byte after
  `text[length]`. It returns `length`, 1 to 6.
- The prosign data is one small `static const MEM_CODE` table, indexed by `c - 1`: the two letters of the name and
  the mark (none for SK). Write the names as character pairs, not as string literals in `char[2]`: GCC 15 and later
  report an unterminated string initializer under `-Wextra`, and `-Werror` stops the host build.
- It is a leaf: no call and no library helper. It reads no setting. Main loop only.

#### 17.4.2 Setting and record

Parameters: min 0, max 2, step 1, default 2 (`Both`). Choice texts in value order: `Punctuation`, `Prosign`, `Both`
(final strings from `ui-ux.md`).

Flag fields (all ids with a non-zero mask):

- `settings_get(id)` returns `(flags & mask) >> shift`. For bits 0 and 1 this gives 0 or 1, as before.
- `settings_store(id, value)` sets `flags = (flags & ~mask) | ((value << shift) & mask)`. The value is already
  clamped. Only the bits of the id change.
- `settings_store()` stays a leaf: its `.asm` has no `lcall` and no library helper.
- `settings_set_defaults()` keeps its rule (clear the flags byte, then store each default). The default flags byte is
  0x09: Show CW Code 1, Paddle 0, Prosigns 2.

`settings_load()` checks in this order: read status, magic, version, CRC.

1. Version 1 and version 2 pass the version check. Every other version gives `SETTINGS_LOAD_BAD_VERSION` and the
   defaults.
2. The CRC rule is the same for both versions: CRC-8 over bytes 0..13.
3. Version 2: keep flags bits 0-3, clear bits 4-7.
4. Version 1: keep flags bits 0-1, clear bits 2-7, then store the default of `SETTINGS_ID_PROSIGN_STYLE`.
5. Both versions: clamp every id as today. Bits 2-3 = 3 give 2.
6. The record in RAM gets version 2. The baseline takes the resulting values. The edited flag is clear. The status
   is `SETTINGS_LOAD_OK`. No EEPROM access other than the read.

Results:

- After a version 1 load, `settings_has_unsaved_changes()` is false. A Settings exit with no edit step saves nothing
  (UX-15). Each start-up loads the version 1 record again, with the same result.
- The first save after an edit step writes a version 2 record with all current values.
- Every save writes version 2. The layout, the size and the order of operations of the save do not change.
- Downgrade: earlier firmware reads a version 2 record as `SETTINGS_LOAD_BAD_VERSION` and uses its defaults. This is
  accepted (plan). No code handles it.

Test vectors (CRC-8 of section 5.1 of `menu_subsystem_spec.md`, computed 2026-10-06):

| Record | Bytes 0..14 |
| --- | --- |
| Version 2, defaults (`Both`) | `43 57 02 58 02 20 20 14 14 02 09 05 08 20 03` |
| Version 2, `Punctuation` | `43 57 02 58 02 20 20 14 14 02 01 05 08 20 B3` |
| Version 2, `Prosign` | `43 57 02 58 02 20 20 14 14 02 05 05 08 20 EB` |
| Version 2, `Both`, `Paddle` = `Reversed` | `43 57 02 58 02 20 20 14 14 02 0B 05 08 20 2F` |
| Version 2, bits 2-3 = 3 (loads as `Both`) | `43 57 02 58 02 20 20 14 14 02 0D 05 08 20 5B` |
| Version 1, defaults (loads as `Both`) | `43 57 01 58 02 20 20 14 14 02 01 05 08 20 54` |
| Version 1, `Reversed` (loads as `Reversed`, `Both`) | `43 57 01 58 02 20 20 14 14 02 03 05 08 20 78` |
| Version 1, bits 2-3 = 3 (loads as `Both`) | `43 57 01 58 02 20 20 14 14 02 0D 05 08 20 BC` |
| Version 3, valid CRC (`SETTINGS_LOAD_BAD_VERSION`) | `43 57 03 58 02 20 20 14 14 02 09 05 08 20 5E` |

#### 17.4.3 Menu

Settings list (title `Settings`, save on exit):

| Index | Item | Kind | Target |
| --- | --- | --- | --- |
| 0 | `CW` | submenu | `MENU_NODE_CW` |
| 1 | `Show CW Code` | choice | `SETTINGS_ID_SHOW_CW_CODE` |
| 2 | `Prosigns` | choice | `SETTINGS_ID_PROSIGN_STYLE` (`Punctuation`, `Prosign`, `Both`), field width 11 |
| 3 | `Max Word Length` | value | `SETTINGS_ID_MAX_WORD_LEN` |
| 4 | `Display` | submenu | `MENU_NODE_DISPLAY` |
| 5 | `Reset Statistics` | action | `MENU_ACTION_RESET_STATS` |

- The rules of `menu_subsystem_spec.md` section 6.3 apply with no change: edit with `prev` and `next`, no wrap, the
  `<` and `>` markers, the save on the Settings exit.
- Field width 11: the value is in columns 12-22, `<` in column 10, the edit band from x 51. Fit rule of `ui-ux.md`
  5.3: the label has at most 9 characters (`20 - 11`).
- The change applies at the next Free Input start (D-P9).

#### 17.4.4 Free Input

`free_input_enter()`: section 6.11 step 4 reads `settings_get(SETTINGS_ID_PROSIGN_STYLE)` into the xdata style byte.
The other steps and their order do not change.

Emit the pattern (replaces the SK branch of section 6.11):

1. Pattern `MORSE_PATTERN_EMPTY`: nothing.
2. Else `length = morse_format(morse_decode(pattern), style, text buffer)`. The pattern becomes `MORSE_PATTERN_EMPTY`.
3. Draw `text[0]` .. `text[length - 1]`, each with the "draw one glyph" rule of section 6.11.

The per-character wrap applies to each glyph, so a text of up to 6 cells can split across two lines. A split at row 7
scrolls in the middle of the text. Examples for `Both` (`ui-ux.md` gives the binding mockups):

| Cursor before AR | Result |
| --- | --- |
| Row 1, column 19 | `<AR/+>` in columns 19-24. The column becomes 25. |
| Row 1, column 22 | `<AR` in columns 22-24. `/+>` in row 2, columns 0-2. |
| Row 1, column 23 | `<A` in columns 23-24. `R/+>` in row 2, columns 0-3. |
| Row 1, column 25 (full line) | `<AR/+>` in row 2, columns 0-5. |

The word-space rule does not change: after a text, a word gap adds one blank cell.

### 17.5 Memory placement and budget

| Item | Qualifier | Writer | Readers |
| --- | --- | --- | --- |
| Free Input prosign style (`uint8_t`) | `static MEM_XDATA` | main | main |
| Free Input text buffer (`char[MORSE_TEXT_SIZE]`) | `static MEM_XDATA` | main | main |
| Prosign names and marks | `static const MEM_CODE`, `morse.c` | - | main |
| `settings_flag_shifts` | `static const MEM_CODE`, `settings.c` | - | main |
| `menu_prosign_texts`, the new item, its strings | `MEM_CODE`, `menu_tree.c` | - | main |
| `value_text` (2 bytes more) | `static MEM_XDATA`, `menu_render.c` | main | main |

Reference before the change (`build/putikeeg_cw_trainer.mem` of 2026-10-05, older than HEAD `ee4eedd`): flash
18 737 / 61 440 B, xdata 1390 / 3072 B, stack start 0x3D with 195 B free. P0 measures HEAD again.

| Resource | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- |
| Flash added by the whole change | at most 400 B | 640 B |
| xdata added | at most 16 B | 24 B |
| New static internal RAM: DSEG + overlay + bit bytes | 0 B | 4 B |
| Stack margin: free stack minus (deepest main-loop use + largest ISR frame) | | at least 64 B |
| Timer 0 ISR | no change (AC-P21) | - |

Report the table of section 12 before and after each step. Also report:

- Flash of `morse.c`, `settings.c`, `free_input.c`, `menu_tree.c` and `menu_render.c`, before and after.
- Newly linked library helpers (expected: none).
- The deepest main-loop chain. Show that `free_input_poll` -> `free_input_emit` -> `morse_format` is not deeper than
  the draw chain of section 12, candidate 1.

### 17.6 Host tests

- Developer files: `test_morse_prosign.c`, `test_settings_prosign.c`, `test_menu_prosign.c`,
  `test_free_input_prosign.c`, and the changed existing tests (D-P10). QA files: `test_qa_<group>*.c`.
- The names match the existing CMake patterns, so no `Tests/CMakeLists.txt` change is expected. If a group needs one
  more source, report it in the step report.
- All host flags stay. No warning suppression.

### 17.7 Acceptance criteria

Decoder and formatter (`test_morse*.c`):

| ID | Criterion |
| --- | --- |
| AC-P1 | Morse table. Index 40 gives `MORSE_CHAR_AS`, 42 `MORSE_CHAR_AR`, 49 `MORSE_CHAR_BT`, 54 `MORSE_CHAR_KN`, 69 `MORSE_CHAR_SK`. The other 49 entries of section 6.10 give the same characters as before. Every other index 0..255 gives `MORSE_CHAR_UNKNOWN`. The five codes are 0x01 to 0x05 and all differ. No table value is in 0x06..0x1F. The table has no `+`, `=`, `(`, `&`, `<`, `>`, `*`, space or lowercase letter. |
| AC-P2 | Formatter, prosigns. Each of the 15 cells of the table in section 17.4.1 for styles 0, 1 and 2 gives the exact text, the exact length and a NUL at `text[length]`. Styles 3 and 255 give the `Both` text for AR, BT, KN and AS, and `<SK>` for SK. |
| AC-P3 | Formatter, other values. Each value 0x20..0x7E gives that character with length 1, for styles 0, 1, 2, 3 and 255. 0x00, 0x06..0x1F and 0x7F..0xFF give `*` with length 1. |
| AC-P4 | Formatter bounds. For all 256 values and the styles 0, 1, 2, 3 and 255, in a buffer with a guard pattern after `text[length]`: no guard byte changes, and the length is at most `MORSE_TEXT_MAX_LENGTH`. A buffer of exactly `MORSE_TEXT_SIZE` bytes passes under ASan. `MORSE_TEXT_MAX_LENGTH` is 6 and `MORSE_TEXT_SIZE` is 7. |
| AC-P5 | Style codes. `MORSE_STYLE_*` equal `SETTINGS_PROSIGN_*` (0, 1, 2). The `#error` check is in `free_input.c` (review). |

Settings (`test_settings*.c`):

| ID | Criterion |
| --- | --- |
| AC-P6 | Values. `SETTINGS_ID_COUNT` is 12. Id 11: min 0, max 2, step 1, default 2. A set of 0, 1 and 2 stores the value. A set of 3 and 255 gives 2. A set to the current value returns false and does not set the edited flag. Id 12 gives 0 from get, min, max and step, and false from set. |
| AC-P7 | Flag bits. For all 12 combinations of Show CW Code (0, 1), Paddle (0, 1) and Prosigns (0, 1, 2), set in two different orders: each get gives its own value, and a set of one id never changes the other two. The saved flags byte is `show + 2 * paddle + 4 * prosigns`. Bits 4-7 are 0. |
| AC-P8 | Version 2 record. The version 2 vectors of section 17.4.2 load with `SETTINGS_LOAD_OK` and give their values. Each of the 12 combinations of AC-P7 survives a save and a load. A saved record has version 2 and a correct CRC. From the defaults, a save after a Tone edit writes the version 2 default vector, except the tone bytes and the CRC. |
| AC-P9 | Version 1 record. The three version 1 vectors of section 17.4.2 load with `SETTINGS_LOAD_OK`, with their stored values and `Prosigns` = `Both`. A version 1 record with each flags value 0..255 and a valid CRC loads Show CW Code from bit 0, Paddle from bit 1 and `Both`. After the load: no EEPROM erase or program, `settings_has_unsaved_changes()` is false, and `settings_save_if_changed()` gives `SETTINGS_SAVE_NOT_NEEDED` with no EEPROM access. A version 1 record with out-of-range fields loads clamped, as a version 2 record does. A version 1 record with a bad CRC gives `SETTINGS_LOAD_BAD_CRC` and the defaults. |
| AC-P10 | Save after a version 1 load. After a Tone edit and a save, the record has version 2, flags bits 2-3 = 2, the other stored values and the new tone. After a load of the version 1 default vector, `Prosigns` = `Punctuation` and a save give flags bits 2-3 = 0. The next load gives `Punctuation`. |
| AC-P11 | Other versions. Versions 0 and 3..255 with a valid CRC give `SETTINGS_LOAD_BAD_VERSION`, the defaults (`Both`) and no EEPROM write. |
| AC-P12 | Power loss. The power-loss sweep of `menu_subsystem_spec.md` T4, for a save that starts from a loaded version 1 record: every next load gives the old values (the version 1 values with `Both`), the new values or the defaults, never a mix. |
| AC-P13 | Leaf. In the generated `settings.asm`, `_settings_store` has no `lcall` and no library helper (review). |

Menu (`test_menu*.c`):

| ID | Criterion |
| --- | --- |
| AC-P14 | Settings list. With an empty fake EEPROM: 6 rows in the order of section 17.4.3. Row 3 shows `Prosigns` in column 0 and `Both` in columns 19-22. `Display` is on row 5 and `Reset Statistics` on row 6. A press of `next` on the last item goes to item 0, and a press of `prev` on item 0 goes to item 5. |
| AC-P15 | Edit. `enter` on the item gives `EDIT(3,11)`. At `Both`: only `<`. `prev`: `Prosign` in columns 16-22, inverted, both markers. `prev`: all 11 characters of `Punctuation` in columns 12-22, inverted, only `>`. One more `prev` changes nothing. `next` goes back. Leave Settings: one save. A re-init from the fake EEPROM gives the edited value. In browse mode each of the three values shows in full. |
| AC-P16 | Value text size. `VALUE_TEXT_SIZE` is at least the longest value or choice text of the tree plus 1 (review). |

Free Input (`test_free_input*.c`, `test_menu_free_input*.c`):

| ID | Criterion |
| --- | --- |
| AC-P17 | Style read. `free_input_enter()` uses the style that `settings_get(SETTINGS_ID_PROSIGN_STYLE)` gives at the call. A `settings_set()` of the style after the call (test only) does not change the text of the open mode. The next `free_input_enter()` uses the new value. |
| AC-P18 | Text per style. `KEYP` of AR, BT, KN, AS and SK, with no word gap, gives: `Punctuation`: `TEXT(1,0,"+=(&<SK>")`. `Prosign`: `TEXT(1,0,"<AR><BT><KN><AS><SK>")`. `Both`: `TEXT(1,0,"<AR/+><BT/=><KN/(><AS/&><")` and `TEXT(2,0,"SK>")`. Every other cell of rows 1-7 is blank. |
| AC-P19 | Wrap and scroll. `Both`: the four rows of the table in section 17.4.4 give the shown cells. `Both`, `KEY(E23)`, KN: `<K` in (1,23) and (1,24), `TEXT(2,0,"N/(>")`. `Prosign`, `KEY(E23)`, BT: `<B` in (1,23) and (1,24), `TEXT(2,0,"T>")`. `Both`, rows 1-6 `L25(A)` to `L25(F)`, 22 letters `G` in row 7, then AR: rows 1-5 show `B` to `F`, row 6 shows 22 letters `G` and `<AR`, `TEXT(7,0,"/+>")`, page 0 is never written or dirty. |
| AC-P20 | Unchanged paths. In all three styles: letters, digits, the other punctuation, `*` for unknown, invalid and overflow, `<SK>`, an empty CHAR_GAP and the word-space rules give the results of AC-K42 and of V-54 to V-61 and V-63 to V-73 (V-89). V-62 sets `Punctuation` in its setup. A word gap after a prosign text adds one blank cell (V-85). |

Build and review:

| ID | Criterion |
| --- | --- |
| AC-P21 | No ISR change. The change does not touch `sidetone.c`, `keyer_service.h`, `keyer.c`, `keyer.h` or `keyer_shared.h`. AC-K60 and AC-K61 hold on the new build. |
| AC-P22 | Memory within section 17.5, with the full report. |
| AC-P23 | `firmware/test.sh` passes, with ASan, UBSan and the `check_asm.sh` self-test. `firmware/build.sh` passes, and `check_asm.sh` reports no violation. |
| AC-P24 | Existing tests change only for the causes of D-P10. No change removes a check or loosens an expected value. The step report lists each changed test with its cause. |

### 17.8 Tasks

`c-embedded-developer` owns P0-P5. Each step ends with: `firmware/build.sh` passes with `check_asm.sh`,
`firmware/test.sh` passes (see P1 for the one allowed exception), the memory table, the assumptions and the open
device checks.

#### D2 UI specification (`ui-ux-embedded-designer`)

- Inputs: sections 17.1, 17.4 and 17.9.
- Output: `ui-ux.md` updated for every item of section 17.9.
- `architector` reviews it against section 17.4 before P4 starts. A label, a value text or a position that differs
  from section 17 goes to `architector` first.
- D2 can run at the same time as P0 to P3.

#### P0 Baseline

- Rebuild HEAD and run the tests. Record the table of section 12 and the stack estimate.

#### P1 Contract and stubs

- Outputs: the header changes of sections 17.3.1 and 17.3.2. A stub `morse_format()` that writes `*` and a NUL and
  returns 1. The settings tables with 12 entries and the values of section 17.3.3. The developer-owned existing tests
  updated for `SETTINGS_ID_COUNT` = 12 (D-P10).
- Acceptance: `build.sh` passes. `test.sh` passes, except for these checks. List them in the report.
  - `test_qa_*` checks that depend on the id count or the version. QA updates them (Q3).
  - Tests that load a version 1 record. P1 raises the version to 2, and the version 1 load comes only in P3. These
    tests pass again after P3.

#### P2 Decoder and formatter

- Outputs: the table and `morse_format()` in `morse.c`, `test_morse_prosign.c`, changed morse tests.
- Acceptance: AC-P1 to AC-P4. Free Input tests that key AR, BT, KN or AS can fail until P4, because Free Input
  draws the codes 0x02 to 0x05 as raw glyphs until then. Do not flash an image built between P2 and P4.

#### P3 Settings

- Outputs: the flag shift, the load rules and the version 2 save in `settings.c`, `test_settings_prosign.c`, changed
  settings tests.
- Acceptance: AC-P6 to AC-P13.

#### P4 Menu and Free Input

- Inputs: the D2 review, sections 17.3.4 to 17.3.6, 17.4.3 and 17.4.4.
- Outputs: `menu_tree.c`, `menu_render.c`, `free_input.c`, `test_menu_prosign.c`, `test_free_input_prosign.c`,
  changed menu and Free Input tests.
- Acceptance: AC-P5, AC-P14 to AC-P20, V-80 to V-89, and the changed V-02, V-20, V-21, V-62 and V-78 of
  `ui-ux.md` section 9.

P2 and P3 can run in any order after P1. P4 needs P2, P3 and the D2 review.

#### P5 Developer close-out

- Final report: memory tables, stack analysis, the asm review of AC-P13 and AC-P21, the test lists, the changed
  existing tests with their causes, the assumptions and the open device checks.
- Acceptance: AC-P21 to AC-P24.

#### Q3 QA (`qa-embedded`)

- After P1: update the QA-owned existing tests for D-P10 (for example the loop over other layout versions, which
  now skips versions 1 and 2), and start the new QA tests.
- An independent reference model of the table in section 17.4.1, checked against `morse_format()` for all 256
  values and the styles 0..255.
- Fault injection: version 1 records with every flags value and out-of-range fields; power loss at each operation of
  the first save after a version 1 load; records of every other version.
- Review of every AC-P criterion, the `.asm` of `settings_store` and `morse_format`, the stack count and the memory
  report.
- Verdict: `approved`, `changes_requested` or `blocked`.

#### A2 Final review (`architector`)

- After the QA verdict. The work is done only after `approved`.

#### W2 Documentation (`technical-writer`, after the final `approved`)

- User manual and release notes: the `Prosigns` setting (final label from D2), its three values, the default `Both`,
  SK as `<SK>`. After the update, the earlier settings stay. A return to older firmware resets all settings to their
  defaults.
- `llm.md`: in the documents table, the `keyer_spec.md` row also names the prosign display setting (section 17),
  and the `ui-ux.md` row names the `Prosigns` setting (section 5.10, UX-38).

### 17.9 Open items for `ui-ux-embedded-designer` (D2)

Status: done. `architector` approved D2 on 2026-10-06. Label `Prosigns`. The values, their order, fw 11 and index 2
are confirmed with no change. The record is in `ui-ux.md` sections 5.3, 5.4, 5.5, 5.9, 5.10, 6.7, 7, 8 (F-2, F-14,
F-15), 9 (V-80 to V-89), 10 (UX-38) and 11 (D-25).

`ui-ux.md` had to decide and record these items:

1. Label of the item. Fit rule `len(label) <= 20 - fw`: with fw 11, at most 9 characters. Title Case (UX-10).
   Working label `Prosigns` (8 characters).
2. Value texts and order. The user named `Punctuation`, `Prosign` and `Both`. Confirm them and fw 11. A different
   text needs a user decision and changes `VALUE_TEXT_SIZE` (D-P7) and AC-P15.
3. Position in the Settings list: index 2, after `Show CW Code` (D-P8). Confirm it, or report another position to
   `architector` before P4.
4. Section 5.3: the new row, the fit-check line, and section 5.5: the Settings list has 6 items.
5. Section 5.9 character table: the AR, BT, KN and AS rows for each value. The SK row does not change. The rule
   "`<` and `>` appear only in `<SK>`" changes to cover the prosign texts. In `Both`, `/` appears inside the
   brackets, and `/` is also a keyed character (`-..-.`). The 5.9 mockup (row 4 shows `+` = AR) follows the default.
6. A new UX decision (next free id, UX-38) that refines UX-28: three values, default `Both`, an old record gives
   `Both`, SK unchanged, per-character wrap of texts up to 6 cells, the change applies at the next Free Input start.
   UX-28 gets only a reference to it.
7. Mockups: the Settings list in browse mode (6 rows), edit mode at each value with its markers (`Punctuation`: only
   `>`, `Prosign`: both, `Both`: only `<`), a Free Input row for each value, and a 6-cell text split at a line end.
8. V-tests: state the style of V-62 (keep its intent with `Punctuation`, or change its expected text). New V-tests
   (V-80 on) for the list, the edit, the save, an old record, Free Input text per value and the split. They must
   agree with AC-P14 to AC-P20, or report the difference to `architector`.
9. A device check (D-25) for the legibility of `<AR/+>`, `<KN/(>` and `<AS/&>` on the glass, and for confusion of
   `(` with `<` and of the inner `/` with a keyed `/`.
10. Section 8: an old record after the update gives `Both` and keeps the other settings, with no message (UX-17).

### 17.10 Device checks (unverified until the user tests on the device)

| ID | Check |
| --- | --- |
| DC-P1 | In Free Input, key AR, BT, KN, AS and SK for each value of the setting. The text is as section 17.4.1 shows and is readable (with D-25 of `ui-ux.md`). |
| DC-P2 | Update from the earlier firmware with saved non-default settings (for example Tone 700 Hz, `Paddle` = `Reversed`). After the update, the settings stay and the new item shows `Both`. An edit and a Settings exit save. A power cycle keeps all values. |
| DC-P3 | A power cycle keeps each of the three values. |

### 17.11 Risks

| Risk | Handling |
| --- | --- |
| A return to older firmware resets all settings. | Accepted in the plan. W2 tells the user. |
| `VALUE_TEXT_SIZE` too small cuts a value text with no error. | D-P7, AC-P15, AC-P16. |
| SDCC calls a library helper for the variable shift in `settings_store()`. | AC-P13. If it does, stop and report to `architector`. |
| The formatter parameters grow the overlay segment. | AC-P22, the internal RAM limit of section 17.5. |
| A 6-cell text wraps more often, and one character can take two rows. | Per-character wrap is the accepted rule (UX-28). D2 mockups. |
| `<KN/(>` and the inner `/` are hard to read. | D2 item 9, DC-P1. |

### 17.12 Final review (2026-10-06)

Verdict: `approved`. QA verdict Q3: `approved`.

| Item | Result |
| --- | --- |
| `test.sh` | 55 of 55 tests pass with ASan and UBSan. `check_asm.sh` self-test: 153 of 153. |
| `build.sh` | Passes. `check_asm.sh` reports no violation. |
| Memory | Flash 18 737 -> 19 055 B (+318, target 400). xdata 1390 -> 1400 B (+10, target 16). Stack start 0x3D with 195 B free, no change. New static internal RAM 0 B. |
| Stack | The Free Input chain `menu_poll` -> `free_input_poll` -> `free_input_emit` -> `free_input_draw` -> `free_input_draw_glyph` -> `gfx_draw_text` -> ... is 2 B deeper (SDCC saves two registers around each draw in `free_input_emit`). Upper bound from the `.asm` with all pushes summed: 34 B from `menu_poll`. The deepest main-loop chain stays the `menu_render` text path (upper bound 65 B), which this change does not touch. The margin of 112 B (at least 64 B) holds. |
| ISR | `sidetone_timer0_isr` has exactly `lcall _keyer_service` and `lcall _sidetone_service`. No `sdcc_atomic` symbol. No ISR or keyer file changed (AC-P21). |
| Leaves | `_settings_store` and `_morse_format` have no `lcall` and no push (AC-P13). |
| Format | `clang-format --dry-run --Werror` gives no finding on the changed production files and the new test files. |

Accepted findings, no change required:

- `firmware/Tests/test_settings_prosign.c` lines 383-386: the power-loss check compares only the tone and the flag
  values. Lines 189-226: AC-P7 starts only from the defaults. QA tests `test_qa_settings_prosign.c` and
  `test_qa_settings.c` cover both gaps.
- `firmware/src/settings/settings.c` line 22 (`RECORD_FLAGS_USED_V1`): the mutant `0x0F` survives, because the
  default store overwrites bits 2-3 next. The constant stays, because section 17.4.2 states the version 1 rule
  explicitly.

Open: device checks DC-P1 to DC-P3 and D-25 of `ui-ux.md`. They stay unverified until the user tests on the device.
