---
name: ui-ux-embedded
description: Use when designing, specifying, or reviewing user interfaces for embedded devices with constrained physical controls or display hardware, including buttons, switches, knobs, encoders, touch zones, LEDs, seven/eight-segment indicators, monochrome displays, and small color displays. Produce decision-ready interaction specifications grounded in device capabilities, control placement, task flows, legibility, feedback, faults, and recovery; maintain the scoped references/internal/ui-ux.md decision record for consistency across sessions. Use alongside c-embedded-dev when low-level C/C++ or firmware implementation is required.
---

# Embedded UI/UX

Design and review embedded interfaces as complete physical interactions: what the user can see, touch, hear, feel, and do, under the device's real control and display limits. Prefer explicit, testable decisions over generic advice, and preserve durable decisions in the project-local UI/UX record.

## Establish the project UI/UX contract

Before proposing a design or reviewing an existing one:

1. Identify the scope of the device or product being designed.
2. Find the canonical scoped UI/UX decision record:
   - In a single-project repository, look for `<project-root>/references/internal/ui-ux.md`.
   - In a monorepo, look first for `references/internal/ui-ux.md` in the nearest relevant component, then product, then repository root for a shared baseline.
   - Reuse an existing equivalent file or documented convention instead of creating a duplicate.
   - Keep the record under `references/internal/`; use `references/` for human-readable external references and `references/context/` for agent-readable external context.
3. Read the record before making recommendations. Treat confirmed project facts and resolved decisions as constraints.
4. If no record exists, proceed with clearly labeled assumptions and recommend a scoped `references/internal/ui-ux.md` location. Create or update the file only when the user requests persistent documentation or the implementation task includes maintaining project documentation.
5. Keep the record useful over time. Separate facts, decisions, assumptions, open questions, and rejected alternatives. Never silently overwrite an established decision; surface conflicts and their consequences.

Use the nearest record as the primary contract and inherit only missing decisions from broader records. A nearer record may override a broader decision only within its scope. Keep the repository-level record limited to genuinely shared decisions; escalate same-scope contradictions or conflicts with shared safety and accessibility constraints to `architector`.

The record should cover, as applicable:

- Product purpose, users, operating environment, and critical tasks
- Control inventory, physical placement, grouping, reach, force, gloves, handedness, and accidental activation risks
- Display modules, orientation, resolution, segments, colors, brightness, contrast, viewing angle, refresh, persistence, and power constraints
- Navigation, terminology, units, formatting, timing, confirmation, cancellation, and feedback conventions
- Normal, startup, unavailable, warning, fault, locked, service, and degraded states
- Legibility, accessibility, localization, alarm, safety, and regulatory assumptions
- Durable decisions with rationale, owner/date if the project uses them, and unresolved questions

## Gather the interface constraints

Do not invent hardware capabilities. Inspect the repository's requirements, hardware notes, schematics, existing UI code, fonts/assets, display drivers, test fixtures, and simulator if available. Record the source or mark the value as an assumption.

Build two inventories before designing:

1. **Control inventory:** identifier, type, location/orientation, size/affordance, actuation, available gestures, feedback, repeat behavior, debounce implications, and actions in each mode.
2. **Output inventory:** display/indicator identifier, type, usable dimensions or segments, typography/icon limits, color and brightness behavior, update/persistence limits, and available audio/haptic feedback.

Also establish:

- User goals, frequency, urgency, expertise, and environmental attention
- Normal operating modes and mode-entry/exit rules
- Timing budgets for response, progress, transient messages, alarms, and acknowledgment
- Power-on, reset, communication-loss, sensor-invalid, low-power, and recovery behavior
- What must be glanceable, what may require navigation, and what must never be ambiguous

For display-specific constraints, load [references/display-capabilities.md](references/display-capabilities.md) only for the relevant display families.

## Design the interaction model

Start from user tasks, not screens. For each task, define the intent, starting state, shortest successful path, required feedback, interruption behavior, and completion signal.

Use a state model that makes these explicit:

- Current mode or screen and the information hierarchy
- Focus/selection and the control that moves it
- Action semantics for short press, long press, repeat, rotation, direction, and simultaneous input where applicable
- Entry, exit, cancel, back, timeout, and reset behavior
- Confirmation requirements for destructive, safety-relevant, or hard-to-reverse actions
- Disabled, unavailable, stale, invalid, or partially known values
- Error announcement, user recovery action, fallback behavior, and return to normal operation

Design for the available controls rather than mapping a desktop pattern onto a small device. Keep interaction grammar consistent across screens. Avoid hidden modes, overloaded controls without feedback, and color-only meaning. If a control must have different behavior by mode or press duration, specify how the device communicates that fact.

For every screen or indicator state, specify the minimum information needed to complete the task. Spend scarce pixels, segments, LEDs, color channels, and update time on priority and status before decoration. Use stable placement for values and actions so users can build recognition and muscle memory.

When layout, hierarchy, or transitions cannot be reliably evaluated in text, create representative wireframes or mockups for the actual resolution/aspect ratio and label them as illustrative until verified against hardware. Do not imply that a mockup proves physical legibility.

## Validate the design

Review the result against concrete scenarios, not only a screen inventory. At minimum cover:

- First use and routine completion of the highest-frequency task
- Fast operation by an experienced user
- Recovery after a wrong input, timeout, reset, or interrupted task
- Every warning/fault state and the user's available recovery path
- Invalid, stale, unavailable, boundary, and rapidly changing values
- Low light, glare, distance, viewing angle, noise, gloves, reduced dexterity, and divided attention when relevant
- Power-up, low-power, communication-loss, and reboot behavior
- Localization, units, numeric overflow, truncation, and long labels when relevant

Use a constraint checklist for each output:

- Does every required value fit the real display/indicator budget?
- Can the user distinguish normal, warning, fault, unavailable, and stale states without relying on one channel such as color?
- Is each physical control discoverable, reachable, and unambiguous in its context?
- Is feedback timely and persistent enough for the environment?
- Are irreversible or hazardous actions protected without making routine work unnecessarily slow?
- Can the user recover without guessing, and does the device fail to a safe understandable state?
- Are assumptions, unresolved questions, and hardware validation needs explicit?

Report findings by severity and cite the affected task, control, state, or display constraint. Separate repository facts, design decisions, measured results, and assumptions.

## Produce the decision-ready specification

Use [references/spec-template.md](references/spec-template.md) when creating a new specification or reviewing a substantial interface. The output should include, as applicable:

- Scope, users, operating conditions, and assumptions
- Control and output inventories
- Task flows and state transitions
- Screen/state table with content and priority
- Input/action mapping and feedback timing
- Fault, degraded, and recovery behavior
- Legibility, ergonomic, accessibility, and localization considerations
- Validation scenarios and acceptance criteria
- Decisions to add or update in the applicable scoped `references/internal/ui-ux.md`

Keep the specification implementation-neutral unless the user asks for code architecture. When firmware or rendering code is requested, preserve this interaction contract and invoke the `c-embedded-dev` skill for the hardware-facing implementation workflow.
