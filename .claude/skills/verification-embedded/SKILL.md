---
name: verification-embedded
description: Use for independent verification and QA of embedded-system implementations, including requirements traceability, host and target testing, hardware-in-the-loop validation, static analysis, timing and resource checks, fault injection, reset and recovery behavior, and evidence-based release decisions. Keep verification MCU, SDK, RTOS, compiler, board, and laboratory agnostic while following the repository's documented hardware facts, test framework, quality gates, and acceptance criteria.
---

# Embedded Verification

Use this skill to verify embedded firmware, drivers, board-support code, RTOS integrations, communication stacks, and hardware-facing components. Work independently from the implementation owner. Establish what each check proves, what it cannot prove, and what evidence remains missing.

## Establish the verification contract

Before judging an implementation:

1. Inspect the requirements, acceptance criteria, architecture, public interfaces, build system, compiler and linker configuration, CI checks, test framework, coverage configuration, static-analysis configuration, and target-toolchain setup.
2. Read relevant project documentation from `<repo root>/references/internal/`, agent-readable external context from `<repo root>/references/context/`, and human-readable external sources from `<repo root>/references/`, including datasheets, reference manuals, errata, SDK documentation, board notes, protocol specifications, and test-fixture documentation. Treat explicit requirements and authoritative internal documents as project context, and record the external source used for non-obvious claims.
3. Identify the target MCU, board, peripherals, memory map, clocks, RTOS or scheduler, compiler, language standard, optimization assumptions, abstraction layer, and available hardware or simulator. Do not invent missing hardware facts.
4. Identify the implementation change's externally visible behavior, safety or reliability impact, timing and resource budgets, initialization and reset contracts, interrupt ownership, concurrency model, and failure/recovery expectations.

If authoritative sources conflict, stop and report the conflict. If a needed source, fixture, or acceptance criterion is absent, record the gap and use only established repository behavior or clearly labeled assumptions.

## Build a verification matrix

Map each requirement and changed behavior to:

- the test or inspection that verifies it;
- the lowest meaningful verification level;
- required fixtures, target configuration, inputs, and observables;
- expected result and pass/fail oracle;
- evidence location and owner;
- hardware, timing, electrical, or environmental dependencies;
- status: passed, failed, skipped, or unavailable.

Do not count compilation, code coverage, or a host mock as proof of behavior they cannot represent. Mark requirements with no meaningful test or oracle as verification gaps rather than silently accepting them.

## Use layered verification

Select the lowest level that provides valid evidence, and add higher levels whenever the behavior depends on the target:

- **Static and structural checks:** compiler warnings, formatting, clang checks, static analysis, MISRA analysis, interface review, configuration review, and inspection for undefined behavior or unsafe assumptions.
- **Host unit tests:** deterministic algorithms, state machines, parsers, validation, arithmetic, error mapping, and retry or timeout logic using explicit fakes or mocks.
- **Component and integration tests:** boundaries between drivers, middleware, RTOS services, queues, callbacks, interrupt adapters, storage, communications, and application logic.
- **Target tests:** real target compilation, linking, startup, memory placement, optimization-sensitive behavior, scheduler behavior, interrupt execution, watchdog operation, and resource measurements.
- **Hardware-in-the-loop tests:** real peripheral behavior, electrical interfaces, clocks, DMA, cache or memory restrictions, reset behavior, power transitions, timing, signal integrity, and interactions that cannot be faithfully represented by a host test.

Separate host evidence, target evidence, and measured hardware evidence in the report. A passing host test must never be presented as proof of register, electrical, timing, or peripheral semantics.

## Test behavior and failure paths

For every applicable changed path, verify:

- normal operation and representative realistic sequences;
- minimum, maximum, zero, empty, full, and overflow-prone values;
- malformed, invalid, unavailable, stale, and out-of-order inputs;
- timeout, cancellation, retry, partial transfer, and permanent error behavior;
- repeated initialization, deinitialization, reset, watchdog recovery, and re-entry;
- interrupt ordering, concurrent access, queue pressure, task cancellation, and shared-state visibility;
- DMA buffer lifetime, ownership, alignment, cache maintenance, completion, error, cancellation, and reuse;
- resource exhaustion, stack and heap limits, buffer bounds, CPU load, latency, and deadline behavior;
- communication loss, power loss or brownout where relevant, and safe return to a known state.

Use deterministic fault injection or controlled test doubles where possible. For target and HIL tests, define setup, stimulus, observable signals, timing tolerance, cleanup, and recovery before execution. Avoid tests that pass only because of a favorable delay, unbounded retry, stale state, or unrecorded laboratory condition.

## Review quality gates and evidence

Run the repository's pinned commands and wrappers with the real target configuration. Check the target build, warnings, unit and integration tests, static analysis, configured MISRA analysis, coverage, timing, memory, stack, and hardware tests when applicable. Do not substitute personal defaults for project configuration.

For each check, report:

- exact command, configuration, target, fixture, or test revision;
- result and relevant failure output;
- requirements or risks covered;
- whether the evidence is repeatable and independently observable;
- skipped or unavailable status and the reason;
- remaining uncertainty and the next required verification step.

Treat a failed required check as a release blocker unless the project has an explicit, approved deviation process. Treat unavailable hardware-dependent checks as evidence gaps: complete all feasible host, build, static, and simulation checks, state the unverified claim and risk, and identify the exact HIL or target test needed. Never turn an unavailable check into a pass.

## Independent QA workflow

1. Establish the verification contract and collect the implementation diff and related requirements.
2. Trace changed behavior to tests, static checks, target checks, and HIL checks.
3. Inspect tests for meaningful oracles, isolation, determinism, boundary coverage, and failure-path coverage; do not accept line coverage alone.
4. Execute feasible checks using repository tooling and preserve the result evidence.
5. Review failures for product defects, test defects, environment defects, and unsupported assumptions. Reproduce or narrow findings where possible.
6. Review the final diff and report for missing coverage, unsafe recovery, race conditions, timing or resource regressions, abstraction-layer violations, and claims unsupported by evidence.
7. Issue one verdict: `approved`, `changes_requested`, or `blocked`.

Use `approved` only when the agreed acceptance criteria are covered and required evidence is satisfactory. Use `changes_requested` when implementation or verification work can proceed within the current environment. Use `blocked` only when progress requires an unresolved requirement, external dependency, target or fixture, or decision that cannot be inferred safely.

## Clarification and reporting policy

Ask for clarification when requirements, hardware documentation, test oracles, timing budgets, electrical assumptions, protocol semantics, reset behavior, or safety expectations conflict or are materially unspecified. Do not invent a pass criterion.

The final verification report must distinguish repository facts, documented requirements, measured results, test observations, assumptions, unavailable checks, deviations, and residual risks. Include the verdict, affected requirements and interfaces, checks run, findings by severity, evidence locations, and exact follow-up needed for every open gap.
