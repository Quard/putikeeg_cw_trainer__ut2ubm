---
name: tech-writer
description: Use for creating, maintaining, and reviewing project technical documentation, especially embedded-device library and API references, integration guides, user manuals, troubleshooting guides, architecture and protocol documentation, and release notes. Discover and preserve the repository's documentation format and publishing workflow, use Markdown when no project convention exists, and require evidence for device behavior, interfaces, limits, configuration, and safety claims.
---

# Technical Writing

Use this skill to turn project knowledge into accurate, usable, maintainable documentation. Treat documentation as part of the product: it must match the implementation, hardware, interfaces, user tasks, and supported release.

## Establish the documentation contract

Before writing or restructuring documentation:

1. Find the project's documentation root, information architecture, style guide, terminology list, templates, generated-output rules, versioning policy, localization requirements, publishing workflow, and documentation checks.
2. Identify whether the project uses Markdown, Doxygen, Sphinx, MkDocs, a static-site generator, a vendor portal, or another format. Preserve the existing convention. If no convention or requirement exists, use clear repository-native Markdown under `docs/` rather than introducing a new toolchain.
3. Apply the repository documentation structure: use `docs/` for published project documentation, `references/` for human-readable third-party documentation, `references/context/` for agent-readable external context, and `references/internal/` for authoritative project-internal documentation and decisions.
4. Inspect the implementation, public headers or interfaces, tests, examples, configuration, CI, hardware notes, requirements, design decisions, and applicable material from `references/internal/`, `references/context/`, and `references/`. For device behavior, use datasheets, reference manuals, errata, board notes, and measured results where available.
5. Identify the audience, task, supported version or hardware revision, prerequisites, safety implications, and expected reader outcome before choosing the document structure.

Do not invent API behavior, hardware capabilities, electrical limits, timing, resource usage, protocol semantics, safety instructions, or compatibility claims. Separate verified facts, approved decisions, measured behavior, assumptions, and unresolved questions. Escalate conflicts or missing authoritative answers to the `architector` role.

## Choose the documentation deliverable

### Library and API documentation

Document the public contract rather than merely repeating implementation details. Cover, as applicable:

- purpose, supported targets, versions, dependencies, and compatibility;
- installation, integration, configuration, initialization, lifecycle, and shutdown;
- public types, functions, callbacks, events, ownership, threading or interrupt context, and reentrancy;
- inputs, outputs, units, valid ranges, defaults, errors, timeouts, retries, resource limits, and recovery behavior;
- hardware or SDK prerequisites and behavior that requires target validation;
- minimal runnable examples, expected output, failure handling, and links to deeper references.

Keep names, signatures, units, return semantics, and examples synchronized with source and tests. Do not promise guarantees that are not part of the public contract or supported by evidence.

### Device user documentation

Write from user tasks and real device capabilities. Cover, as applicable:

- audience, operating environment, prerequisites, safety notices, and prohibited use;
- controls, indicators, displays, feedback, terminology, and state meanings;
- unpacking, installation, wiring or connection prerequisites, first startup, normal operation, configuration, maintenance, and shutdown;
- warnings, faults, unavailable or degraded states, communication loss, power interruption, and recovery steps;
- troubleshooting organized by observable symptom, likely cause when evidenced, safe checks, and escalation path;
- supported accessories, consumables, environmental limits, storage, transport, service, and disposal requirements when established.

For controls, displays, interaction flows, legibility, accessibility, and state feedback, invoke the `ui-ux-embedded` skill and preserve the applicable scoped `references/internal/ui-ux.md` decisions. Do not replace a device-specific fact with a generic user-interface convention.

### Engineering and project documentation

Document architecture, component boundaries, data flow, protocols, configuration, hardware assumptions, deployment, diagnostics, validation evidence, design decisions, and operational constraints. Link to source-of-truth requirements, interfaces, tests, and decision records rather than copying details that will quickly diverge.

## Work from evidence and maintain traceability

For every important claim, identify its source: requirement, public interface, implementation, test, generated artifact, repository document, measured hardware result, or approved owner decision. Record version, board or hardware revision, configuration, and document section when the claim depends on them.

When information is incomplete:

- write only what is supported by evidence;
- label assumptions and provisional content visibly;
- add an open question with an owner or escalation target;
- identify the affected audience and risk;
- do not hide uncertainty behind confident wording.

Coordinate factual review with the relevant domain owner:

- `c-embedded-developer` for implementation, API, build, and integration behavior;
- `qa-embedded` for verification results, limitations, failure behavior, and evidence;
- `ui-ux-embedded-designer` for device interaction, controls, displays, and usability decisions;
- `architector` for requirements, architecture, cross-domain conflicts, and unresolved ownership.

## Validate documentation

Before declaring documentation complete:

1. Check structure, headings, navigation, links, anchors, terminology, units, version labels, and cross-references.
2. Compare API names, signatures, defaults, errors, examples, configuration, and compatibility claims against the current source and tests.
3. Check device procedures against documented controls, states, warnings, recovery behavior, hardware revisions, and UI/UX decisions.
4. Run the repository's configured documentation build, formatter, linter, link checker, API generator, example test, spelling checker, localization check, and publication preview when available.
5. Review accessibility and usability: meaningful headings, readable tables, descriptive links, non-color-only meaning, clear warnings, scannable procedures, and terminology understandable to the intended audience.
6. Search for stale names, removed APIs, obsolete screenshots or diagrams, unsupported promises, contradictory instructions, and undocumented breaking changes.

If a documentation check or source of truth is unavailable, report it as skipped or unavailable with the reason and remaining risk. A successful documentation build proves format and structure, not factual correctness.

## Documentation change workflow

1. Define audience, task, scope, version, source evidence, and acceptance criteria.
2. Find the existing page or canonical location before creating a new one.
3. Outline the shortest path for the reader, then add necessary context, constraints, examples, and recovery guidance.
4. Make the smallest coherent change and preserve stable links, terminology, and document ownership.
5. Update related library references, user procedures, examples, diagrams, changelog or release notes, and navigation when the implementation change affects them.
6. Run applicable documentation and example checks, then request domain-owner review for factual claims.
7. Report changed documents, evidence consulted, checks run, reviewer status, assumptions, unresolved questions, and publication readiness.

Do not modify production code or tests merely to support a documentation claim. If the documentation exposes an implementation or requirement defect, report it to the appropriate owner and document only the behavior that is currently supported.
