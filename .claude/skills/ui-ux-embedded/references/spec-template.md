# Embedded UI/UX specification template

Copy and adapt this structure for a project specification. Omit sections that do not apply, but preserve explicit assumptions and open questions.

## 1. Scope and contract

- Product/device:
- UI scope:
- Primary users and expertise:
- Operating environments:
- Critical and high-frequency tasks:
- Source decision record: `references/internal/ui-ux.md` at:
- Related requirements, hardware notes, or measurements:

### Assumptions and open questions

| ID | Type | Statement | Impact | Owner/status |
|---|---|---|---|---|
| A-01 | Assumption/question |  |  |  |

## 2. Control inventory and placement

| ID | Type/location | Physical affordance | Available actions | Feedback | Modes/context | Risks/notes |
|---|---|---|---|---|---|---|
| C-01 |  |  |  |  |  |  |

Document grouping, handedness, reach, force, gloves, accidental activation, tactile differentiation, and visibility of each control where relevant.

## 3. Output and display budget

| ID | Type | Usable dimensions/vocabulary | Content limits | Feedback channels | Environmental limits |
|---|---|---|---|---|---|
| O-01 |  |  |  |  |  |

Record actual resolution/segments, font and icon limits, contrast/brightness, update/persistence behavior, viewing distance/angle, and power constraints.

## 4. Task flows

| Task | Starting state | User intent | Normal path | Completion signal | Interruptions/recovery |
|---|---|---|---|---|---|
| T-01 |  |  |  |  |  |

## 5. State and screen specification

| State/screen | Purpose/priority | Content and units | Default focus | Entry/exit | Timeout/update | Fault/degraded behavior |
|---|---|---|---|---|---|---|
| S-01 |  |  |  |  |  |  |

For each state, specify what is always visible, what is selectable, what is editable, and how warning, stale, unavailable, disabled, and invalid values appear.

## 6. Input and feedback rules

| Control/action | Short press | Long press | Repeat/rotation | Confirmation | Success feedback | Error/rejection feedback |
|---|---|---|---|---|---|---|
| C-01 |  |  |  |  |  |  |

Define navigation, back/cancel, mode changes, reset, timeout, simultaneous inputs, and destructive or hard-to-reverse actions.

## 7. Fault and recovery behavior

| Condition | User-visible indication | Latched/transient | Immediate device behavior | User recovery | Safe fallback |
|---|---|---|---|---|---|
| F-01 |  |  |  |  |  |

## 8. Validation scenarios and acceptance criteria

| Scenario | Setup | User action | Expected result | Evidence |
|---|---|---|---|---|
| V-01 Routine task |  |  |  |  |
| V-02 Wrong input/recovery |  |  |  |  |
| V-03 Fault/degraded state |  |  |  |  |
| V-04 Environmental constraint |  |  |  |  |

Check display fit, control discoverability, timing, legibility, non-color distinctions, localization/units, boundary values, reset/power loss, and recovery without guessing.

## 9. Durable decisions

| ID | Decision | Rationale | Alternatives rejected | Evidence/owner/date |
|---|---|---|---|---|
| D-01 |  |  |  |  |

Copy durable decisions into the scoped project `references/internal/ui-ux.md`. Do not leave long-term conventions only in a transient task response.
