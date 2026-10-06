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
(decision D-K1).

- An interrupt calls only functions of its own file. The firmware compiles with `--norestartseqatomics`. With
  SDCC 4.6.0, an interrupt that calls a function of another file still gets the `sdcc_atomic_maybe_rollback`
  epilogue, and the link fails. Only `architector` changes this layout.
- A service of another module comes into the ISR file through a header. The header defines the service as a
  `static` function. Only the ISR file includes it on the target, so the service is a function of that file.
  Example: `firmware/inc/cw/keyer_service.h` defines `static void keyer_service(void)`, and only
  `firmware/src/sidetone/sidetone.c` includes it, at the end of the file, before the Timer 0 ISR. The ISR body is
  `keyer_service(); sidetone_service();` in this order.
- Every file that includes such a header calls its `static` function. Otherwise GCC reports an unused function,
  and `-Werror` stops the host build. Host tests include the header directly.
- The ISR reads and writes the state that it shares with the main loop directly. That is a memory access, not a
  call. Example: `firmware/src/cw/keyer.c` defines the shared keyer state, and `firmware/inc/cw/keyer_shared.h`
  declares it `extern`.
- `sidetone_timer0_isr` has exactly two calls: `lcall _keyer_service` and `lcall _sidetone_service`. If the build
  shows an `sdcc_atomic` symbol or another call in this ISR, stop and report it to `architector`.
- A function that an interrupt calls uses `#pragma nooverlay`. It calls no other function and no library
  arithmetic helper (`__mulint`, `__divulong` and others), because the main loop uses them too.
- No source uses `<stdatomic.h>` or C11 atomics.
- `P_SW2.EAXFR` stays set after init. No code clears it. An interrupt never reads, saves or restores `P_SW2`.

## Documents

Project documents in `references/internal/`:

| Document | Content |
| --- | --- |
| `components_and_wiring.md` | Board revision, components, pins, audio routes, measurements. |
| `menu_subsystem_spec.md` | Menu, settings, EEPROM and button contract. |
| `ui-ux.md` | UI/UX decision record: display, buttons, long presses, Tone preview, Free Input screen, keyer from the user's view, `Paddle` setting, `Prosigns` setting (section 5.10, UX-38), device checks. |
| `sidetone_spec.md` | Sidetone contract: API, envelope, registers, memory, decisions. |
| `keyer_spec.md` | Keyer, Free Input and long-press contract: ISR layout (D-K1), keyer behavior and timing, Morse table, P3.2 and P5.5 use, `Paddle` setting, memory, device checks DC-K1 to DC-K7. Prosign display setting (section 17): prosign codes, `morse_format()`, settings layout version 2, device checks DC-P1 to DC-P3. |
| `sidetone_pwm_envelope_research.md` | Sidetone research and the implemented design. |
| `sidetone_validation.md` | Sidetone device checks and measurement results. |
| `paddle_keyer_research.md` | Paddle input and iambic keyer guide. |
| `paddle_keyer_howtoimplement_research.md` | Iambic mode B notes. |

External context: `references/context/STC8H_datasheet.md` (STC8H family reference manual).
