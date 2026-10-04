---
name: c-embedded-dev
description: Use for embedded C or C++ development and review involving MCU firmware, peripheral drivers, board support, RTOS integrations, SDK code, startup code, interrupt handlers, DMA, clocks, timers, UARTs, ADCs, or other hardware-facing code. Keep work MCU/SDK/OS agnostic while following the repository's documented hardware facts, selected abstraction layer, language standard, MISRA requirements, and clang-based quality workflow.
---

# C Embedded Development

Use this skill for changes to embedded C/C++ projects. Adapt to the repository and target; do not assume a particular MCU vendor, SDK, RTOS, compiler, board, or peripheral model.

## Establish the project contract

Before editing hardware-facing code:

1. Inspect the build system, toolchain files, compiler flags, `compile_commands.json`, CI checks, formatter and analyzer configuration, linker scripts, startup code, board support, and existing driver patterns.
2. Identify the selected language and exact standard (`C11`, `C17`, `C++17`, etc.), compiler extensions, warning policy, ABI, optimization assumptions, and applicable MISRA C or MISRA C++ edition/profile.
3. Identify the project’s abstraction layer and stay within it. Examples include CMSIS helpers, a vendor HAL/LL, an SDK such as ESP-IDF, or Arduino APIs. Do not mix register access, HAL, SDK, and Arduino layers casually. If a lower layer is necessary, document the reason and follow the project’s established boundary.
4. Inspect `<repo root>/references/internal/` before relying on project-specific hardware or SDK behavior. Read the relevant requirements, hardware notes, design decisions, and internal technical documents there. Use `<repo root>/references/` for human-readable third-party sources and `<repo root>/references/context/` for agent-readable external context. Treat authoritative internal documents and cited external sources as project context. Record the document and section/page used for non-obvious hardware decisions when the project’s documentation style supports it.
5. If `references/internal/`, `references/`, `references/context/`, or the required document is absent, say so, search existing source/configuration for established behavior, and do not invent register layouts, clock limits, electrical assumptions, timing requirements, or peripheral semantics.

Resolve uncertainty in this order: explicit project requirements and `references/internal/`, agent-readable external context in `references/context/`, human-readable external references in `references/`, selected SDK/abstraction-layer documentation, existing project conventions, then general embedded best practices. When sources conflict, stop and surface the conflict rather than silently choosing a value.

## Non-negotiable engineering rules

### Abstraction and language standard

- Follow the chosen abstraction layer consistently across implementation, tests, and examples.
- Follow the exact project-selected C/C++ standard. Do not introduce newer syntax, library facilities, compiler builtins, or extensions unless the project already permits them and the toolchain configuration confirms support.
- Preserve the project’s ownership, initialization, naming, error, concurrency, and portability conventions.
- Keep hardware-specific details behind the project’s existing interfaces where such interfaces exist. Do not create a second driver model for convenience.

### MISRA compliance

- Apply the project-selected MISRA C or MISRA C++ edition and compliance profile strictly. If the project has not selected one, identify that gap and use its existing coding standard as an interim constraint; do not claim MISRA compliance without a defined baseline.
- Prefer code that is statically verifiable: explicit types and conversions, bounded loops and buffers, clear ownership, controlled side effects, and no reliance on undefined or implementation-defined behavior.
- Treat every deviation as deliberate. Explain the rule, rationale, scope, risk, and review/approval status in the project’s established deviation format. Never hide a finding with an unexplained suppression or broad exclusion.
- Use compiler warnings, the project’s MISRA-capable checker when configured, and clang tooling together. Clang tools support the quality workflow but do not by themselves prove full MISRA compliance.

### Clang quality gates

Use the project’s pinned tool versions and configuration. Prefer a compilation database so target defines, include paths, language mode, and cross-compiler options are accurate.

- `clang-format`: format changed C/C++ files using the repository configuration; inspect the resulting diff.
- `clang-tidy`: analyze changed translation units with the repository checks and the correct `-p <build-dir>` compilation database. Fix findings or document an approved, narrowly scoped deviation.
- `clang-check`: run syntax/AST checks with the same compilation database and target flags; use it to catch parse, include, type, and source-level issues before relying on a hardware build.

Use adaptable commands such as `clang-format`, `clang-tidy -p <build-dir> <file>`, and `clang-check -p <build-dir> <file>`, replacing placeholders with the project’s actual build directory, files, wrappers, and cross-toolchain settings. Do not replace project configuration with personal defaults.

## Test strategy and hardware isolation

Treat production code and its tests as one deliverable. Unless the user explicitly disables testing, every behavior, interface, timing, or failure-handling change must add or update automated tests.

- Select the lowest test level that gives meaningful coverage: host-side unit tests for deterministic logic, integration tests for component boundaries, and hardware-in-the-loop tests for MCU, peripheral, timing, electrical, or toolchain behavior that cannot be represented faithfully on the host.
- Prefer small, deterministic units with explicit dependencies. Keep hardware access behind the project’s established interfaces so host tests can use mocks, fakes, or recorded peripheral responses without touching real registers or requiring a board.
- Do not make a host mock claim to verify hardware semantics. Mark hardware-dependent behavior and hardware validation separately in the test and final report.
- Cover normal operation, boundary values, invalid inputs, timeout and error paths, repeated initialization, reset/recovery, interrupt ordering, resource exhaustion, and DMA ownership whenever those cases apply.
- Preserve the project’s test framework and build conventions. Do not introduce a new framework or bypass target defines merely to make a test pass.

## Failure behavior and change discipline

Define failure behavior before implementation when the change can affect hardware, timing, concurrency, or externally visible state.

- Make timeout, cancellation, retry, reset, partial-transfer, unavailable-peripheral, and resource-exhaustion behavior explicit and bounded.
- Preserve existing APIs, ownership, initialization order, interrupt contracts, and abstraction boundaries unless a deliberate interface change is required. Update all callers and tests when an interface changes.
- Keep the change as small as the requirement permits. Do not weaken or delete tests, broaden static-analysis suppressions, or add unrelated cleanup to hide a failure.
- Review the diff for regressions, undefined behavior, race conditions, stale state after recovery, missing cleanup, and behavior that differs between host and target builds.

## Evidence and validation ownership

Base hardware and SDK decisions on evidence in this order: explicit requirements and authoritative project documents under `references/internal/`, agent-readable external context under `references/context/`, human-readable external references under `references/`, selected SDK or abstraction-layer documentation, existing project conventions, and finally general embedded practice. Record the source and section/page for non-obvious decisions when the repository supports that convention.

The implementer owns validation, not only compilation. Run the relevant unit tests, integration or hardware tests, target build, formatter, compiler warnings, clang checks, and configured MISRA analysis. Report each check as passed, failed, skipped, or unavailable, with the reason and any remaining risk. A host build alone is never sufficient evidence for hardware-facing work.

## Clarification policy

Ask for clarification instead of inventing hardware behavior when requirements, repository documentation, SDK behavior, or existing conventions conflict or leave a safety-critical detail unresolved. This includes register layouts, clock limits, electrical assumptions, timing budgets, protocol semantics, interrupt ownership, memory placement, and recovery behavior.

If the ambiguity is low risk and a documented project convention resolves it, follow that convention and state the assumption. Never silently choose between conflicting authoritative sources.

## Prefer safe data movement

Prefer DMA for transfers that are sufficiently large, periodic, latency-sensitive, or concurrent with CPU work, when the MCU and selected SDK support it. Make the decision from measured or documented constraints rather than adding DMA mechanically.

For every DMA path, establish and review:

- source/destination ownership and when a buffer may be reused;
- buffer lifetime, alignment, size, valid regions, and overrun protection;
- cache maintenance and memory-region restrictions on cache-enabled targets;
- transfer completion, error, timeout, cancellation, and reinitialization behavior;
- interrupt versus task/thread context and synchronization primitives;
- visibility of shared state, including appropriate `volatile`, atomics, barriers, or SDK synchronization APIs;
- whether circular, double-buffered, linked-list, or one-shot mode matches the data flow;
- a bounded, tested non-DMA fallback when DMA is unavailable or less reliable for the transfer size.

Do not read or modify a buffer concurrently with an active DMA transfer unless the hardware and synchronization contract explicitly permits it. Clear completion flags and handle stale events according to the selected abstraction layer.

## Document calculations

Add a clean comment for every non-obvious calculation that affects hardware behavior, timing, memory, or protocol correctness. State the formula, units, source values, rounding/truncation behavior, and relevant datasheet/reference-manual constraint. Keep the code as the executable source of truth; comments explain why the calculation is correct and what must remain invariant.

Examples include:

- clock tree and peripheral clock calculations;
- timer reload, prescaler, PWM period, and interrupt frequency calculations;
- UART baud-rate divider calculations and tolerated error;
- ADC resolution, reference-voltage, gain, and fixed-point scaling calculations;
- DMA transfer counts, ring-buffer sizes, sample windows, and timeout budgets.

For example:

```c
/* UART divider = peripheral_clock_hz / (oversampling * baud_hz).
 * Round to the nearest integer; the resulting baud error must remain within
 * the limit documented in <source document and section>.
 */
const uint32_t divider =
    (peripheral_clock_hz + ((oversampling * baud_hz) / 2U)) /
    (oversampling * baud_hz);
```

Use named constants with explicit units and checked ranges. Avoid unexplained magic numbers, implicit narrowing, overflow-prone intermediate expressions, and comments that merely restate an assignment.

## Implementation workflow

1. Establish the project contract and read the relevant `references/internal/`, `references/context/`, and `references/` material.
2. Trace the existing initialization, ownership, interrupt, and error paths before changing them.
3. Design within the selected abstraction layer and language/MISRA baseline; choose DMA where the data-flow and hardware constraints justify it.
4. Implement the smallest change, preserving deterministic timing, bounded resource use, and recoverable failure behavior.
5. Add or update host-side tests, mocks, static checks, and hardware-in-the-loop tests as supported by the repository. Cover boundary values, timeout/error paths, repeated initialization, reset/recovery, interrupt ordering, and DMA buffer ownership.
6. Run the project build with its real target flags, then run `clang-format`, `clang-tidy`, and `clang-check` against the affected code. Run the configured MISRA checker and review all warnings and deviations.
7. Review the diff for abstraction-layer mixing, standard violations, undocumented calculations, unbounded behavior, unsafe shared state, missing cleanup, and claims unsupported by the applicable internal or external references. Report any validation that could not run and why.

Never mark hardware-facing work complete solely because it compiles on the host. Separate verified facts, repository documentation, measured hardware behavior, and assumptions in the final report.
