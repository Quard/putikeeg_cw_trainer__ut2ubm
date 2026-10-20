# Agent notes

Agent-maintained project instructions. `AGENTS.md` holds the user rules. If the two differ, `AGENTS.md` wins.

## Firmware notes

### Internal RAM placement

Source: `references/internal/sidetone_spec.md` sections 6.2, 9 and 14.

- The linker gives all internal RAM that data does not use to the stack. So the message "No spare internal RAM
  space left" does not mean that data fills the RAM. Read `firmware/build/putikeeg_cw_trainer.mem`: it shows the
  stack start and the free stack bytes.
- Stack depth is the real limit on internal RAM. Each change reports a stack estimate from the generated `.asm`:
  the deepest main-loop call chain plus the largest interrupt frame. Interrupts at the same priority level do not
  nest, so only the largest frame counts. Keep a margin of at least 64 bytes.
- `MEM_DATA` (`__data`): small state that an interrupt reads on every call.
- `MEM_BIT` (`__bit`): flags shared with an interrupt. One bit instruction is atomic. An SDCC `bool` is a full
  `__data` byte, so do not use `bool` for these flags.
- `MEM_XDATA`: tables, and state that only the main loop uses. This includes main-loop `bool` flags and work
  variables of long calculations.
- `MEM_CODE`: constant tables.

### Interrupt service routines

Source: `references/internal/sidetone_spec.md` sections 9 and 21, `references/internal/keyer_spec.md` section 3.1
(decision D-K1), `references/internal/training_spec.md` section 3.1 (decision D-T1).

- An interrupt calls only functions of its own file. The firmware compiles with `--norestartseqatomics`. With
  SDCC 4.6.0, an interrupt that calls a function of another file still gets the `sdcc_atomic_maybe_rollback`
  epilogue, and the link fails. Only `architector` changes this layout.
- A service of another module comes into the ISR file through a header. The header defines the service as a
  `static` function. Only the ISR file includes it on the target, so the service is a function of that file.
  Example: `firmware/inc/cw/keyer_service.h` defines `static void keyer_service(void)`, and only
  `firmware/src/sidetone/sidetone.c` includes it, at the end of the file, before the Timer 0 ISR.
  `firmware/inc/cw/player_service.h` defines `static void player_service(void)` in the same way. The ISR body is
  `keyer_service(); player_service(); sidetone_service();` in this order.
- Every file that includes such a header calls its `static` function. Otherwise GCC reports an unused function,
  and `-Werror` stops the host build. Host tests include the header directly.
- The ISR reads and writes the state that it shares with the main loop directly. That is a memory access, not a
  call. Example: `firmware/src/cw/keyer.c` defines the shared keyer state, and `firmware/inc/cw/keyer_shared.h`
  declares it `extern`.
  `firmware/src/sidetone/sidetone.c` defines `sidetone_ramp_calls`, and `firmware/inc/sidetone/sidetone_shared.h`
  declares it. The keyer and the player read it as their early release (`sidetone_spec.md` D-RM3).
- `sidetone_timer0_isr` has exactly three calls: `lcall _keyer_service`, `lcall _player_service` and
  `lcall _sidetone_service`, in this order. `firmware/tools/check_asm.sh` rule R6 checks the call list and the leaf
  services, and rule R7 rejects every `sdcc_atomic` symbol. If the build shows an `sdcc_atomic` symbol or another
  call in this ISR, stop and report it to `architector`.
- At most one of the keyer and the CW player is enabled at a time. Both write the sidetone request bit.
- A function that an interrupt calls uses `#pragma nooverlay`. It calls no other function and no library
  arithmetic helper (`__mulint`, `__divulong` and others), because the main loop uses them too.
- No source uses `<stdatomic.h>` or C11 atomics.
- `P_SW2.EAXFR` stays set after init. No code clears it. An interrupt never reads, saves or restores `P_SW2`.

### Pre-commit hooks

`.pre-commit-config.yaml` runs `reuse lint-file` and `clang-format` on each commit. Before you report a change as
done, run `pre-commit run --files <changed files>`. Each new source file starts with the SPDX lines.

## Documents

Project documents in `references/internal/`:

| Document | Content |
| --- | --- |
| `components_and_wiring.md` | Board revision, components, pins, audio routes, backlight circuit, measurements. |
| `menu_subsystem_spec.md` | Menu, settings, EEPROM and button contract. `Ramp` setting, settings layout version 3, CW scrolling and Ramp preview hooks (section 18). |
| `ui-ux.md` | UI/UX decision record: display, buttons, long presses, Tone preview, Free Input screen, keyer from the user's view. `Paddle` setting, `Prosigns` setting (section 5.10, UX-38). Training modes: Receiving, Sending and session end screens, group keyboard, tap mode, delays and layout constants (sections 5.11-5.15, 6.8-6.10, 9.4, UX-39 to UX-50). Four-button Receiving keyboard, tap `enter` and long `enter` (UX-51). Sending retry screen and pause limit in a word (UX-52). Level selection: start screen, level selector, earned level in the main menu (section 5.16, UX-53 to UX-57). Start screen with a big level description and the Sending code, end screen with score and result only, redraw rule (UX-65 to UX-70). Receiving answer state after the third wrong attempt: it waits for an `enter` tap, a long `enter` replays the answer (UX-64). `Ramp` setting, CW list scroll and Ramp preview (UX-71 to UX-73, sections 5.3, 5.5, 6.5, 9.6). Device checks. |
| `sidetone_spec.md` | Sidetone contract: API, envelope, registers, memory, decisions. Ramp duration setting 4-12 ms: envelope position, `sidetone_set_ramp_ms()`, `sidetone_ramp_calls`, task split R0-R6, QA checks (section 24, D-RM1 to D-RM15; D-RM15 is in `menu_subsystem_spec.md` 18.2). |
| `training_spec.md` | Koch training modes Receiving and Sending. Three-call ISR (D-T1), CW player and timing extraction. RNG, Koch level model and the four-button group keyboard with tap `enter` and long `enter` (D-T27, D-T28). Progress record in EEPROM page 1. Session engine, menu wiring, memory budget, tasks T0-T11, D1, Q2. Sending retry screen and word-space error (D-T30, D-T31), tasks D2, T12, Q3, W3. Word pass rate 80 % (D-T33), tasks D3, T13, Q4, W4. Level selection: start screen, selector, replay rule, main menu level (D-T35 to D-T43), tasks D4, T14, Q5, W5. Receiving waits for `enter` after the third wrong attempt, long `enter` replays the answer (D-T44), tasks D5, T15, Q6, W6. Start screen with a big level description and the Sending code (D-T45 to D-T47). End screen with score and result only, where `enter` opens the start screen (`training_reopen_select()`). Shared code-mark helpers. Tasks D6, T16, Q7, W7. Player early release at the `Ramp` setting and the `timing.h` speed check (D-T51, section 17.7). |
| `keyer_spec.md` | Keyer, Free Input and long-press contract: ISR layout (D-K1), keyer behavior and timing, Morse table, P3.2 and P5.5 use, `Paddle` setting, memory, device checks DC-K1 to DC-K7. Prosign display setting (section 17): prosign codes, `morse_format()`, settings layout version 2, device checks DC-P1 to DC-P3. Early release at the `Ramp` setting (section 18). |
| `sidetone_pwm_envelope_research.md` | Sidetone research and the implemented design. |
| `sidetone_validation.md` | Sidetone device checks and measurement results. |
| `paddle_keyer_research.md` | Paddle input and iambic keyer guide. |
| `paddle_keyer_howtoimplement_research.md` | Iambic mode B notes. |

External context: `references/context/STC8H_datasheet.md` (STC8H family reference manual).
