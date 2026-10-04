# Putikeeg CW Trainer -- UT2UBM mod

The idea of the repo is to create custom firmware for Putikeeg CW Trainer because the original has poor functionality 
and big issues with Iambic paddle.

The repository contains the embedded firmware, hardware design files, and
project reference material.

## Agent instructions

- Read this file and the repository-root `llm.md` before starting work.
- Treat this file as user-maintained project guidance. Do not change it unless the user explicitly requests an update.
- Put agent-maintained workflow and project instructions in `llm.md`.
- Preserve existing user changes and limit edits to the requested scope.
- Consult the relevant documents listed in `llm.md` before making hardware or firmware claims.
- Treat `firmware/drivers/` as containing third-party drivers that should generally not be modified. If a change there 
  is necessary, discuss and document the reason before making it.

## Firmware

These rules are mandatory for every firmware change.

- Memory on the MCU is small: 64 KB flash, 256 bytes of internal RAM, and 3072 bytes of `xdata` RAM
  (STC8H3K64S2, see `references/context/STC8H_datasheet.md`). Optimize every change for flash size and RAM use.
  Report the flash and RAM use of the build before and after each change.
- CMake builds the firmware.
- `firmware/build.sh` builds the firmware. Use it for every build.
- `firmware/flash.sh` downloads the firmware into the MCU.

## Team

Firmware work runs through a team of subagents. Each agent definition is in `.claude/agents/` (Claude Code) and in
`.codex/agents/` (Codex, with `_` instead of `-` in the name). The definition is the source of truth for that role.

| Role | Agent | Owns |
| --- | --- | --- |
| Architect | `architector` | Requirements, plan, interfaces, task split, final review verdict. Writes no code. |
| Firmware developer | `c-embedded-developer` | Production C code, its host tests and test doubles, build and static-analysis results. |
| Embedded QA | `qa-embedded` | Independent verification, extra tests and fault injection, the QA verdict. Edits no production code. |
| UI/UX designer | `ui-ux-embedded-designer` | Display, buttons, paddle and sidetone interaction specifications, `references/internal/ui-ux.md`. Writes no firmware. |
| Technical writer | `technical-writer` | Evidence-backed documentation, user manual, release notes. |

Each deliverable has exactly one owner. An agent that disagrees with the output of another agent reports the
disagreement to `architector` and leaves that output unchanged.

### Workflow

1. `architector` clarifies scope and acceptance criteria, then publishes the plan and the task specifications.
2. If the change touches what the user sees, hears, or presses, `ui-ux-embedded-designer` specifies the interaction
   before implementation starts.
3. `c-embedded-developer` implements the change with its tests and runs the available validation.
4. `qa-embedded` verifies the change independently and returns `approved`, `changes_requested`, or `blocked`.
5. `architector` reviews on top of the QA verdict. The work is done only after its `approved`.
6. If the change affects users or the public interface, `technical-writer` documents the result.

Only `architector` changes a plan or an interface after work starts. Host builds never count as hardware evidence:
behavior that depends on the MCU, timing, or the electrical circuit stays unverified until someone tests it on the device.

## Project structure

`hardware` - KiCad reverse engineered schematic.

`firmware` - custom firmware.

### Folder structure

- `firmware/src/` - C source files, grouped into subdirectories by module as the firmware grows.
- `firmware/inc/` - project header files, following the source module structure where practical.
- `firmware/drivers/` - third-party drivers; preserve them unchanged whenever possible.
- `firmware/cmake/` - CMake toolchain and reusable build configuration.

Keep related source and header files in clear module directories instead of accumulating all files in one folder.
