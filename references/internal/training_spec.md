# Koch training modes Receiving and Sending: implementation specification

Owner: `architector`. Status: T0-T10 implemented, final review `approved` on 2026-10-06 (section 22). Change
request D-T27 to D-T29 (four-button Receiving keyboard, user decision 2026-10-06): D1, T11 and Q2 done, final review
`approved` on 2026-10-06 (section 22.1). W2 is open. Change request D-T30/D-T31 (Sending retry screen and word
space, user decision 2026-10-07): D2, T12, Q3 and W3 done, final review `approved` on 2026-10-07 (section 22.2). Change request D-T33 (word pass rate 80 %, user
decision 2026-10-07): D3, T13, Q4 and W4 done, final review `approved` on 2026-10-07 (section 22.3). Change request
D-T35 to D-T41 (level selection in Receiving and Sending, user decisions 2026-10-07): specification published on
2026-10-07. D4 done, review `approved` on 2026-10-07 (D-T42). T14 and Q5 done, final review `approved` on
2026-10-07 after fix F1 (section 22.4). W5 ready to start (section 17.4). Change request D-T44 (Receiving waits for
`enter` after the third wrong attempt, user decision 2026-10-15): specification published on 2026-10-15. D5 done,
review `approved` on 2026-10-15 (U-32). T15 and Q6 done, final review `approved` on 2026-10-15 after test
fixes R1 and R2 (section 22.5). W6 manual and quick guide `approved`. The release notes wait for the version decision.
Change request D-T45 to D-T47 (start screen with a big level description, split end screen, user decisions
2026-10-15): specification published on 2026-10-15 (section 17.6). D6 done, review `approved` on 2026-10-15
(D-T48). T16 and Q7 done. Final review `approved` on 2026-10-15 after C1 to C6 (section 22.6). W7 ready to start. The release notes wait for the version decision. The user answered Q-T1 and Q-T2 (section 20). Section 21
reconciles this contract with the final `ui-ux.md` (sections 3.3.1, 5.11-5.15, 6.8-6.10, 9.4, UX-39 to UX-50).
Readers: `c-embedded-developer`, `qa-embedded`,
`ui-ux-embedded-designer`, `technical-writer`.

## 1. Sources and precedence

| Source | Role |
| --- | --- |
| This document | Interfaces, memory placement, level model, EEPROM record, player, session engine, task split and acceptance criteria. It refines the approved plan. It does not change a user decision. |
| Approved plan `~/.claude/plans/silly-knitting-flurry.md` | User decisions (section 2.1) and the module split. |
| Approved plan `~/.claude/plans/lively-exploring-reef.md` | Level selection: user requirements and user decisions of 2026-10-07 (D-T35), proposed interface. |
| Approved plan `~/.claude/plans/lucky-seeking-locket.md` | Start screen with a big level description and a split end screen: user decisions of 2026-10-15 (D-T45), proposed layouts. |
| `references/internal/ui-ux.md` (owner `ui-ux-embedded-designer`) | All that the user sees, hears or presses: layouts, texts, delays, control tables. Section 21 lists every value that this document takes from it. |
| `references/internal/keyer_spec.md` | Keyer contract. Decision D-T1 of this document revises D-K1 (section 3.1). The keyer timing of section 6.7 stays the reference for the timing extraction. |
| `references/internal/sidetone_spec.md` | Sidetone contract, envelope, ISR frame. |
| `references/internal/menu_subsystem_spec.md` | Menu, settings record, EEPROM driver, buttons. |
| `AGENTS.md`, `llm.md` | Project rules: memory, ISR rules, software SPI, team workflow. |

Precedence rules:

- For a UI value, `ui-ux.md` wins. UI values are layouts, glyphs, texts, delays and the reaction to a button. When
  `ui-ux.md` asks for a behavior that the interfaces of section 5 cannot give, stop and report it to `architector`.
  Do not change an interface yourself.
- For behavior, timing, memory, records and interfaces, this document wins.
- After implementation starts, only `architector` changes this document.

## 2. Scope

### 2.1 User decisions (fixed)

| Topic | Decision |
| --- | --- |
| Character session | 50 items. Pass at 45 or more correct on the first attempt (90 %). A fail repeats the level. |
| Word session | 10 words. Pass at 8 or more correct on the first attempt (80 %, user decision 2026-10-07, D-T33). |
| Final word levels | After the last character, every level is a word session. Each level adds one word: 10, 11, 12 ... capped at 100 words. Pass at 80 % (rounded up) of the word count (D-T33). |
| Item mix | All learned characters. About half of the items are the new character. Level 1 mixes K and M evenly. |
| Character set | `K M R S U A P T L O W I . N J E F 0 Y , V G 5 / Q 9 Z H 3 8 B ? 4 2 7 C 1 D 6 X`, then BT, SK, AR: 43 characters. The final word phase starts at L = 63 (Q-T1). |
| Words and prosigns | Random words contain no prosign. BT, SK and AR appear only in character sessions. Words and the Receiving word keyboard use the first 40 Koch characters. With fewer learned characters, they use the learned ones (Q-T2). |
| Word length | 2 for word sessions 1-2, 3 for 3-4, and so on (+1 every two word sessions), capped at `Max Word Length` (4..8). |
| Attempts | 3 per item. The correct answer shows only after the third wrong attempt. |
| Receiving word input | Pick characters one by one. Check the whole word after the last one. Long `prev` undoes the last pick or steps out of a group. Long `enter` replays (D-T27). |
| Progress key | One 16-bit level per mode per character WPM (5..50). The effective speed has no effect. The level stops at 65535 and never wraps. |
| Manual level | Revised by D-T35 (user decision 2026-10-07). The user selects the level of the next session on a start screen and on the session end screen. The range is level 1 up to the earned level, the stored level of the mode at the current speed. `Reset Statistics` sets all levels back to level 1. |
| Replay | A pass below the earned level never changes or saves the stored level. A pass at the earned level advances and saves it. After a session, the selector shows the next level after a pass and the same level after a fail. Example: earned L20, the user passes L5. The selector then shows L6, the store keeps L20, and the selector still reaches L20 (D-T35). |
| Level selector | `prev` and `next` move the selected level by one, with auto-repeat. No wrap: the steps stop at L1 and at the earned level. A line under the selector describes the selected level (`New char F`, `Words of 4`). Layout and texts: `ui-ux.md` (task D4). Revised by D-T45: the selector is on the start screen only. |
| Start screen and end screen | User decisions 2026-10-15 (D-T45). The start screen shows the selector in row 1. Below it, the level description shows in the 8 x 16 font, in the place of the item line, for every level kind. In Sending, it also shows the CW code of the new character, independent of `Show CW Code`. The end screen shows only the score and the result. An `enter` press there opens the start screen at the preselected level. A second `enter` starts the session. Layout and texts: `ui-ux.md` (task D6). |
| Main menu level | The main menu shows the earned level next to `Receiving` and `Sending`, at the current `RX Speed` and `TX Speed` (D-T41). |
| Button order on the case | `menu/back`, `prev`, `next`, `enter` from left to right (user fact 2026-10-06: `enter` is right of `next`). The four group labels follow this order (D-T27). |
| Receiving keyboard | Four groups, one per button, in the case order (user decision 2026-10-06, D-T27). Three groups were confusing. |
| Sending retry screen | At the retry, the code row (row 7) and the answer code row (row 5) are blank, as for a new item. The bar goes back to the first character. The status keeps `Try 2 of 3` or `Try 3 of 3`, as in Receiving (user decision 2026-10-07, D-T30). |
| Sending word space | Inside a word item, the space between two characters must stay shorter than the keyer word space (5 Farnsworth units at `TX Eff. Speed`). A word space after an accepted character of an unfinished item is a wrong attempt (user decision 2026-10-07, D-T30). |

"Done" means: host tests pass, the build checks pass, QA approves, `architector` approves. Device behavior stays
unverified until the user tests it (section 18).

### 2.2 In scope

- `morse_encode()`.
- Timing extraction from `keyer_start()` into `cw/timing`.
- CW player with its ISR service, the three-call Timer 0 ISR and the `check_asm.sh` rules R6 and R7.
- RNG, Koch tables, level model, item generation, group keyboard.
- CRC-8 move to `util/crc8`, progress record in EEPROM page 1, `stats_reset()` with a result.
- Button tap mode for the Receiving screen, all four buttons, with a long `enter` event (D-T7, D-T28).
- Session engine, Receiving mode, Sending mode, shared screen helpers, menu and `main.c` wiring.
- Host fakes, host tests, memory, stack and ISR-time report.

### 2.3 Out of scope

- A level above the earned level. The user cannot skip ahead (D-T35).
- Statistics other than the level.
- Any change in `firmware/drivers/`.
- Any change of the keyer behavior or timing.

## 3. Decisions

### 3.1 D-T1: three-call Timer 0 ISR (revises D-K1 of `keyer_spec.md`)

Decision:

- The ISR body of `sidetone_timer0_isr` is `keyer_service(); player_service(); sidetone_service();` in this order.
- `firmware/inc/cw/player_service.h` defines `static void player_service(void)` and the ISR-private player state,
  with the same rules as `keyer_service.h`. On the target, only `sidetone.c` includes it, after `keyer_service.h`
  and before the ISR. So `player_service()` is a function of `sidetone.c`.
- `sidetone.c` defines `PLAYER_SIDETONE_ON()` and `PLAYER_SIDETONE_OFF()` before the include, with the same bodies
  as the keyer macros. If an includer does not define both, the header gives an `#error`.
- At most one of the keyer and the player is enabled at a time (D-T12). Both write the same sidetone request bit.
- `check_asm.sh` checks the call list automatically (rule R6) and rejects `sdcc_atomic` symbols (rule R7). Section
  13.2 lists the tool changes.

Rationale:

- The player must run in the 4 kHz interrupt. A main-loop player on `tick_now()` jitters, because the LCD drawing
  delays the loop. The jitter changes element lengths, which a CW trainer must not do.
- The D-K1 layout already proves that a `static` function from a header is a function of the ISR file. A third
  service keeps the same rule with no compiler-specific behavior.
- The cost is one more `LCALL`/`RET` pair and one bit test per call, about 10 clocks on the disabled path. The ISR
  frame does not grow: all three calls are at the same depth.
- Rejected: the player inside `keyer_service()` (it mixes two state machines in one leaf and makes the keyer tests
  depend on the player). Rejected: a conditional call in the ISR (it saves no stack and adds a branch to review).

In these cases, stop and report to `architector`: the build shows an `sdcc_atomic` symbol, the link fails, or R6
fails. Do not try another layout.

### 3.2 Other decisions

| ID | Decision | Reason |
| --- | --- | --- |
| D-T2 | The dit and Farnsworth math moves from `keyer_start()` to `cw_timing_compute()` in `src/cw/timing.c`. The keyer and the player both call it. The keyer tick values stay bit-exact (AC-K9, AC-K16 unchanged). | One copy of the 32-bit math. |
| D-T3 | User decision (Q-T1). The Koch table holds the 43 characters of the user list, in that order. `KOCH_CHAR_COUNT` = 43, `KOCH_FINAL_LEVEL` = 63. All level formulas use `KOCH_CHAR_COUNT`. | The user answered 43. The 44 of the plan was a counting error. |
| D-T4 | User decision (Q-T2). Words and the Receiving word keyboard use only the first `min(m, KOCH_WORD_ALPHABET_MAX)` Koch characters, with `KOCH_WORD_ALPHABET_MAX` = 40. BT, SK and AR appear only in character sessions. `koch_alphabet_count()` gives this count. | A prosign takes up to 6 cells. A word of 8 prosigns does not fit one row in either font. In CW, a prosign is not part of a word. |
| D-T5 | The shown level is `L + 1`, saturated at 65535. Level indexes 65534 and 65535 both show 65535. | `L + 1` = 65536 does not fit `uint16_t` and `gfx_format_u16()`. The case needs 65535 passed sessions. |
| D-T6 | An attempt covers the whole item. A wrong check ends the attempt. The next attempt starts again at the first character, with an empty answer. | One rule for both modes and for words. The score counts items, not characters. UI dependency U-15. |
| D-T7 | Button tap mode (revised by D-T28). It applies on the Receiving screen to all four buttons. A button gives its press event at the release, and only for a release before the long-press time. A hold gives only the long event. `enter` (button or paddle tip) has a long event only in tap mode. | A press event comes before the long event of the same hold. Without the tap mode, a long `prev` first narrows a group (or picks a character and checks the word) and only then undoes. A long `enter` replays, so a tap of `enter` must not pick first. UI dependency U-11, U-23. |
| D-T8 | The placeholder screen and its text `Not ready` go away. `MENU_SCREEN_PLACEHOLDER` (2) is no longer defined. No other screen number changes. | No menu item uses it after this change. Dead code costs flash. |
| D-T9 | The progress module keeps the whole 188-byte record image in xdata. The image is the level cache. | No second buffer for the save. 4 bytes more than a 184-byte cache. |
| D-T10 | The CRC-8 function moves from `settings.c` to `src/util/crc8.c` as `crc8_compute()`. `src/settings/settings_crc.h` goes away. Existing tests change only the include and the function name. | Settings and progress share one function. |
| D-T11 | `stats_reset()` returns `bool`. True means that the reset levels are stored. The menu shows `Statistics reset` on true and `Not saved` on false. | A reset that is not stored comes back after a power cycle. The user must know. UI dependency U-17. |
| D-T12 | Receiving never starts the keyer. Sending never starts the player. Every leave path stops the module of its mode. A menu host test checks that the fake keyer and the fake player never run at the same time. | Both write `sidetone_is_on_requested`. |
| D-T13 | No `player_init()`. The C start-up RAM clear sets `player_is_enabled` and `player_is_start_requested` to 0 (same reason as R-5 of `keyer_spec.md`). | Flash. |
| D-T14 | Result hold (`ui-ux.md` Q-14), revised by D-T44. After the third wrong attempt, Receiving plays the item once more. `training_hold_result(true)` stops the RESULT delay. The hold stays on until the user taps `enter`. `training_hold_result(false)` clears the hold and sets the delay to 0, so the next `training_poll()` ends the RESULT. The mode does not call `training_hold_result()` on each poll. | The engine stays free of the player and of the buttons. One function gives both the hold and the release. |
| D-T15 | Dropped-character rule (`ui-ux.md` Q-15, item 1). In Sending, a DIT or DAH outside phase ITEM sets a drop flag. While the flag is set, the mode drops every keyer event up to and including the next CHAR_GAP, also in phase ITEM. That CHAR_GAP clears the flag. | A character that starts during a delay never counts and never uses an attempt. |
| D-T16 | Overflow (`ui-ux.md` Q-15, item 2). In phase ITEM, OVERFLOW empties the pattern and sets the drop flag of D-T15. No check, no attempt, no status. | A lost event is a fault of the device, not of the user. If the lost events held a CHAR_GAP, the next character is dropped too. The user keys it again. |
| D-T17 | Top level outcome. A pass at level index 65535 gives `TRAINING_OUTCOME_PASS_TOP`. The end screen then shows `Top level L65535`. Revised text (D-T42, UX-56): row 3 shows `Passed`, and the selector shows `L65535 of 65535`. | `ui-ux.md` UX-47 has its own text for this case. |
| D-T18 | Test seam for items. `koch_make_item()` is in its own file, `src/koch/koch_item.c`. The modes and menu test groups link `Tests/fakes/fake_koch_item.c` in its place. The level, and so the keyboard set and the session kind, come from a progress record in the fake EEPROM. | The V-tests of `ui-ux.md` 9.4 need scripted items. The production code has no test hook. |
| D-T20 | `check_asm.sh` R7 stays strict: any `sdcc_atomic` text in any scanned `.asm` file is a violation. The stale `accept/real_image` fixture is regenerated from the current build (section 13.2). | Any `sdcc_atomic` text in a new build means that `--norestartseqatomics` is lost. A narrow R7 (for example only `sdcc_atomic_maybe_rollback`) lets the helper code back in unseen. The fixture shows real compiler output, so it must come from the current flags. |
| D-T21 | Accepted implementation details of T0-T6. The koch test group links `src/cw/morse.c` (AC-T4 calls `morse_encode()`). The Koch tables use `[]` with a `_Static_assert` on the size. `keyboard_group_first()` returns `range_first + range_count` for an unknown group. The player start call loads `player_ticks = 1` in the gap state and uses the normal countdown, with the same result as section 9.2. The `stats_reset()` result wiring in `menu.c` came in T4. The clang-tidy finding `bugprone-easily-swappable-parameters` stays on `player_start()`, `koch_word_length()` and the internal `progress_offset()`: no suppression and no signature change. | Each detail keeps the contract behavior. AC-T12, AC-T4 and AC-T8 prove it. The signatures are part of the contract. |
| D-T22 | Tap mode and a late poll. Tap or hold follows the debounced state (D-K4 of `keyer_spec.md`). A release that debounces at or after `BUTTONS_LONG_PRESS_MS` after the press event is a hold. Then only the long event comes. A late poll does not change this. | One rule for the long event and for the tap. The user held the button for the long time. |
| D-T23 | Final memory of T0-T10 accepted: flash +7538 B (target 6144, hard limit 8192), xdata +411 B (target 384, hard limit 512). No cut is required. | Both stay under the hard limits. The image uses 26 593 of 61 440 B and xdata 1811 of 3072 B. The internal RAM grew by 4 B, the stack margin is 108 B and the ISR worst path is 403 clocks. A cut now costs rework during QA for no user benefit. If a later change needs flash, the first candidate is the largest new file, `training_screen.c` (1707 B). |
| D-T24 | `ui-ux.md` V-104, second run, is wrong. The start pause ends at `TRAINING_START_MS` = 1000 ms, and the first item plays from then (`ui-ux.md` 5.12.2, this document 12.2). A hold from 300 ms leaves at 1100 ms with the player running, so the log is `play`, then `stop`. The designer changes the second run to a hold from 100 ms: the main menu at 900 ms, empty player log. The developer test follows the new text. It keeps the 300 ms case as an extra check with `play` then `stop`. | The contract and the `ui-ux.md` item flow agree. Only the test vector has the wrong time. |
| D-T25 | Accepted implementation details of T7-T10. The Receiving and Sending tests link the menu group, because the `ui-ux.md` 9.4 scenarios start from the menu. `src/modes/training_screen_internal.h` holds the draw helpers that `training_screen.c`, `receiving.c` and `sending.c` share. `training_screen_draw_header()` reads the speeds with `settings_get()`. No settings edit can run on a training screen, so the values equal those of the mode start (UX-49). `sending_leave()` at END does not call `keyer_stop()` again: END already stopped the keyer and gave P3.2 back. Most scenario taps go to `menu_handle()` directly. V-94 to V-97, V-104, V-108 and V-120 use the real buttons. The extra `training_hold_result(true)` at the FAILED check stays. The clang-tidy finding `bugprone-easily-swappable-parameters` stays also on `training_start()` and the internal `training_set_phase()` (D-T21). | Each detail keeps the contract behavior. AC-T15 tests tap mode itself. |
| D-T26 | Two V-test steps have no host test: the P5.5 step of V-97 and the entry-mask step of V-110. They are keyer behavior. AC-K3 covers the entry mask, and no code outside the keyer reads P5.5. The V-110 test still checks that the keyer starts once with `Iambic B`, `Normal`, 20 WPM and 20 WPM. | The menu-group tests use the fake keyer, which has no pins. |
| D-T27 | User decision and user facts (2026-10-06). The Receiving keyboard has four groups, one per button. The case order is `menu/back`, `prev`, `next`, `enter` from left to right (closes the `ui-ux.md` C-4 note "position of `enter` not documented"). Run `k` (0..3, left to right) of a range of `c` characters has `(c + 3 - k) / 4` characters (integer division). This equals the proposed rule "each run gets ceil(remaining / buttons left)": 43 -> 11/11/11/10, 40 -> 10/10/10/10. `KEYBOARD_GROUP_COUNT` = 4, `KEYBOARD_DEPTH_MAX` = 3: at most 3 selects per pick for 2..64 characters. In Receiving: an `enter` tap selects the right run, a long `enter` replays, long `menu/back` exits, long `prev` undoes, long `next` does nothing. On the end screen, an `enter` tap (or the tip) starts the next session and a `menu/back` tap leaves. P3.2 is also the paddle tip. A short dit-lever or straight-key closure selects the right run. A closure of 800 ms replays. A plug insertion can select a run (accepted, `ui-ux.md` F-20 and D-5). Sending, Free Input and the menu do not change. | Three keys for four buttons confused the user. The balanced split keeps the keyboard order and gives sizes that differ by at most 1, larger runs first. The closed form needs no division chain (a shift by 2 on the target). |
| D-T28 | Interface of D-T27. The long `enter` event is `BUTTON_EVENT_ENTER \| BUTTON_EVENT_LONG_FLAG` (0x44). No new event macro: every long event is a code with `BUTTON_EVENT_LONG_FLAG`, and the modes decode it with `BUTTON_EVENT_CODE()`. `buttons_poll()` gives long `enter` only in tap mode. Tap mode covers all four buttons. `buttons_set_tap_mode()` and both values of `buttons_set_enter_enabled()` drop the pending press and the long-pending state of `enter`. Long events come in the order `menu/back`, `enter`, `prev`, `next`, the same order as the press events. The keyboard group names follow the buttons (`KEYBOARD_GROUP_MENU` ... `KEYBOARD_GROUP_ENTER`). `LEFT`, `MIDDLE` and `RIGHT` go away. | A second spelling of a long event adds a name with no new meaning. Outside tap mode, `enter` stays the paddle tip with no long event (AC-K45 unchanged). New group names make every old use a compile error, so no use keeps the old "right = `next`" meaning in silence. |
| D-T29 | Decisions on the D1 result (`ui-ux.md` UX-51). (1) The labels use row 7 only. The fields are at pixel x 2-26, 30-54 and 58-82 (5 cells each) and x 87-126 (8 cells, right-aligned at x = 127 - 5n). (2) Accepted: the key bar is four 1 px segments at y 55 over the four fields, not one line. The mode draws the segments only in a full render. A label redraw does not touch them, and they stay in GAP. (3) Accepted: a label of one character uses the short text (the `rx_short_style()` style), so `<AR/+>` never shows in a label. The item line keeps the full text. (4) R-T7 is corrected: a glitch pick can fill the last slot and use an attempt. | The script check of D1 passes for 84 sets, 3 `Prosigns` values and 12 096 labels, so row 6 stays free. The segments show which field belongs to which button. They cost four line draws on a full render only. One label style removes a branch from the label code. |
| D-T30 | User decisions (2026-10-07), Sending only. (1) Retry screen: at the RETRY event, rows 5 and 7 go blank, so the screen looks like a new item. The bar goes back to the first character. The status keeps `Try 2 of 3` or `Try 3 of 3` (the user first asked to clear it, then kept it, so that both modes act the same). This applies after every kind of wrong attempt. During RESULT, row 7 still shows the marks of the keyed character. Receiving already clears its slots at a retry and keeps `Try N of 3`: no change. (2) Word space: the keyer gives KEYER_EVENT_WORD_GAP when a space reaches `KEYER_WORD_GAP_UNITS` = 5 Farnsworth units at `TX Eff. Speed` (the point where Free Input inserts a space). In phase ITEM, a WORD_GAP in the space right after a character that gave PARTIAL is a wrong attempt, the same as a wrong character: it uses an attempt, the retry is the whole item (D-T6), and the third one gives FAILED. A WORD_GAP before the first character of the item (also at a retry), after the item is complete, in GAP or RESULT, or after a dropped character (D-T15, D-T16) does nothing. Character sessions do not change, because their item is complete after one character. The keyer, its timing and Free Input do not change. The user chose 5 units, not 7. | The user wants a word keyed as one word, not as single characters with long pauses. 5 units is the keyer threshold that already exists, so no new timing value and no keyer change. Farnsworth spacing (lower `TX Eff. Speed`) gives more room between characters. A blank retry screen shows that the attempt starts again. |
| D-T31 | Interface of D-T30. (1) `modes/training.h` defines `TRAINING_CHAR_WORD_SPACE` = `' '`. `sending.c` gives it to `training_add_char()` for a word-space error. No Koch item character and no `morse_decode()` result is `' '`, so the Sending compare of section 11.3 always gives a wrong attempt, and the status code can tell a word space from a keyed character. The engine code does not change: no new function, no new state. (2) `sending.c` has the flag `tx_is_word_gap_armed` (`static MEM_XDATA bool`). Only a CHAR_GAP that gives PARTIAL sets it. Every other keyer event that the mode reads clears it, also a dropped event, the WORD_GAP itself and OVERFLOW. `tx_start_session()` clears it. A WORD_GAP counts only with the flag set and the phase ITEM. (3) The word-space status text and the RESULT look are UI values (`ui-ux.md`, task D2) within the limits WS-1 to WS-6 (section 12.3). | The engine already treats every mismatch as a wrong attempt with the right delays, so a sentinel character costs no engine code. The keyer gives at most one WORD_GAP per space, always after the CHAR_GAP of that space, and a new mark cancels it (AC-K15). So "the last event was a PARTIAL CHAR_GAP" means exactly "this space follows an accepted character". The answer length alone is not enough: after OVERFLOW, a CHAR_GAP clears the drop flag and the WORD_GAP of the same space then follows a lost character. That pause is not the fault of the user (D-T16). One xdata byte buys this. |
| D-T32 | Accepted implementation details of T12. The WORD_GAP test in `tx_handle_keyer_event()` keeps its phase check (D-T31), although the armed flag is set only in ITEM. The push of `event` around `training_phase()` raises the summed-push stack bound from 65 to 66 B (margin 107 B). The real call-site depth stays 35 B. The ITEM/RETRY branch of `sending_poll()` clears rows 5 to 7 in one call: row 6 is always off in Sending. The CHAR_GAP and WORD_GAP paths share one check tail. | The phase check keeps the rule local: it does not depend on the fact that ITEM has no time-out in Sending. 1 B of stack is far inside the 64 B margin rule. The "no deeper chain" target of 14.2 is waived for this 1 B. |
| D-T33 | User decision (2026-10-07). Every word session, the final word levels included, passes at `ceil(0.8 n)` correct on the first attempt: 10 -> 8, 11 -> 9, 15 -> 12, 50 -> 40, 100 -> 80. Character sessions keep `ceil(0.9 n)` (45 of 50). Interface: `koch_pass_threshold()` takes the session kind as its first parameter, because the item count alone cannot tell the kind (a final level of 50 words has the count of a character session). `training_start()` gives `koch_kind()` of the session level. A kind other than `KOCH_KIND_WORD` uses the character rule. No change in the progress record, the level model or the engine state. The rounding rule for the final levels is an assumption, confirmed with the user by the coordinator. | One function keeps one place for the rule. Taking the kind (not the level) keeps the pure arithmetic testable for every count 0..255. Defaulting an unknown kind to the stricter character rule never lets a session pass easier by mistake. |
| D-T34 | Accepted details of T13. Flash +41 B, 1 B above the +40 B target and far below the +100 B hard limit. `koch_pass_threshold()` selects the numerator and denominator by kind and uses one formula `(p n + p) / q`. The test helper `pick()` uses 0xFF, not 0, for "not found". `training_harness.h` gets an `end_screen_is` probe. | 1 B over a target is not worth a rework. The helper change fixes test code only. QA showed that the old helper failed loudly and hid no result. |
| D-T35 | User decisions (2026-10-07), level selection. (1) Receiving and Sending open on a start screen with a level selector. No session runs before the user starts it. (2) The session end screen has the same selector. (3) The range is level index 0 up to the earned level, `progress_level(mode, wpm)` at the speed of the mode (`RX Speed` or `TX Speed`). `prev` steps down and `next` steps up. The steps stop at the limits and never wrap. (4) A pass below the earned level never changes or saves the stored level. A pass at the earned level advances and saves it, as before. (5) After a session, the selector preselects the session level + 1 after a pass, and the same level after a fail. (6) `prev` and `next` auto-repeat on the selector. If the repeat costs more than the D-T39 limit, `architector` replaces it with a long `prev` or `next` that steps by 10. (7) The description line under the selector names the selected level: `New char F`, `Words of 4`. (8) The main menu shows the earned level next to `Receiving` and `Sending`, at the current speed. This revises "Manual level: none" of section 2.1. | The user wants to repeat levels that they passed without losing progress. |
| D-T36 | Engine interface of D-T35 (section 5.10). (1) New phase `TRAINING_PHASE_SELECT` = 5: no session, the selector is active. (2) `training_prepare(mode, wpm, max_word_len)` takes the parameters of the old `training_start()`. It stores them, sets the selected level to the earned level and sets phase SELECT. It reads the progress RAM only. (3) `training_start(void)` starts a session at the selected level. It does not read the progress any more. (4) `training_select_step(bool is_up)` acts in SELECT and END only. (5) The selected level is one `static MEM_XDATA uint16_t`. The earned level has no copy: `training_earned_level()` calls `progress_level()` with the prepared mode and speed. (6) Invariant: 0 <= selected <= earned after every engine call. (7) Finish (section 11.4): only a pass with the session level equal to the earned level calls `progress_advance()` and `progress_save()`. A pass at another level gives the new outcome `TRAINING_OUTCOME_PASS_REPLAY` = 4. (8) `training_new_shown_level()` keeps its name. It now gives the shown selected level. (9) In SELECT, only `training_phase()`, `training_mode()`, `training_wpm()`, `training_selected_level()`, `training_earned_level()` and `training_new_shown_level()` have a meaning. The session accessors keep the values of the last session. | The engine owns the level rules, so both modes get the same replay rule. No copy of the earned level means no stale value. The test `!=` (not `<`) makes sure that no level other than the earned one ever reaches the store. A start without parameters makes the next session at END use the values of the mode entry (UX-49). |
| D-T37 | `koch_shown_level(uint16_t level)` in `koch/koch.h` is the one copy of the D-T5 rule (level + 1, saturated at 65535). `training.c`, `training_screen.c` and `menu_render.c` call it. The private `training_shown_of()` goes away. The D-T5 limit stays: level indexes 65534 and 65535 both show 65535, also on the selector. | Three files need the rule. The menu needs it without the engine. The level model belongs to `koch`. |
| D-T38 | Shared selector code in `training_screen.c` (section 5.11). `training_screen_draw_start()`, `training_screen_draw_select()` and `training_screen_select_input()` serve both modes. The selector input acts on `prev` and `next` press and repeat events, and on `enter` and `menu/back` press events. It ignores long events. It returns an action, and the mode starts the session or leaves. The description line reads `Max Word Length` and `Prosigns` with `settings_get()`, as the header reads the effective speed (D-T25). No `morse_format()` style changes. No table per level in flash: the text comes from the level model. | One copy of the selector code for both modes. The settings cannot change on a training screen, so `settings_get()` gives the values of the mode entry. |
| D-T39 | Tap mode in Receiving (revises D-T7 and U-11). Tap mode is on only in GAP, ITEM and RESULT. `receiving_enter()` leaves it off. `rx_start_session()` turns it on. The END event of `training_poll()` and `receiving_leave()` turn it off. So `prev` and `next` auto-repeat on the start screen and on the end screen (400 ms, then every 80 ms, `buttons_config.h`). `buttons_set_tap_mode()` drops pending presses, long-pending states and the repeat, so a button held across a switch gives no event. Cost limit for the repeat: the tap switches and the repeat acceptance can add at most 100 B of flash. Above it, the developer reports, and `architector` decides on the fallback of D-T35 (6). | The buttons module already gives repeats with tap mode off. No buttons change is necessary. |
| D-T40 | Keyer in Sending (revises 12.3 entry and the `sending_leave()` detail of D-T25). `sending_enter()` does not start the keyer and does not call `buttons_set_enter_enabled()`. P3.2 stays with the menu on the start screen, so `enter` and the paddle tip act there. `tx_start_session()` starts the keyer as before. `sending_leave()` calls `keyer_stop()` and `buttons_set_enter_enabled(true)` only in GAP, ITEM and RESULT. In SELECT and END the keyer does not run. | The keyer runs only during a session, so the selector gets every button. |
| D-T41 | Main menu level. `draw_item_row()` in `menu_render.c` draws `L<n>` for a mode item with target `MENU_MODE_RECEIVING` or `MENU_MODE_SENDING`. `n` = `koch_shown_level(progress_level(target, settings_get(SETTINGS_ID_RX_WPM or SETTINGS_ID_TX_WPM)))`. The value is read at each render. No cache. The text uses the color of the label (inverted on the selected row). The column comes from `ui-ux.md` (task D4). `Free Input` and `Settings` rows do not change. `menu_render.c` checks at compile time that `MENU_MODE_RECEIVING` equals `PROGRESS_MODE_RECEIVING` and that `MENU_MODE_SENDING` equals `PROGRESS_MODE_SENDING`. | The user sees the earned level before entering a mode. A read at each render always shows the RAM value, also after a session or a speed change. |
| D-T42 | Review of D4 (2026-10-07): `approved`. Accepted UI values of `ui-ux.md` 5.11.1, 5.15, 5.16 (UX-53 to UX-57), V-01, V-19, V-105 to V-108, V-121, V-136, V-137 to V-151 and D-35 to D-37. (1) `Start` replaces `Next` on the end screen. One band helper serves both screens (cells 9-15, x 47-81). (2) `Not saved` moves to row 1. (3) Row 3: `Passed, new level` for PASS and PASS_NOT_SAVED. `Passed` for PASS_REPLAY and PASS_TOP. `Not passed, need <t>` for FAIL. The texts `New level L`, `Repeat level L`, `Top level L` and `Next` go away. (4) Start screen header: level field `training_new_shown_level()`, item field `1/<n>` with `n` = `koch_item_count(training_selected_level())`. A step redraws pages 0, 4 and 5. The end screen header keeps the session that ended, and a step there redraws pages 4 and 5 only. (5) A-L1 to A-L5 hold for D-T36. A pass at index 65534 below an earned index 65535 is a replay and preselects 65535 (V-148). (6) The steps "no DIT from the held contact" of V-108 run 5 and V-142 have no host test, as D-T26: the keyer entry mask (AC-K3) gives this behavior. The host test checks that the keyer starts once. D-35 to D-37 are the device checks of DC-T9. | The draft meets D-T35 to D-T41 and SL-1 to SL-7. The texts are short `MEM_CODE` constants, and four old texts go away, so the flash budget of T14 holds. |
| D-T43 | Decisions on the T14 and Q5 results (2026-10-07). (1) F1, fix required: in Sending, a `prev` or `next` held from the last RESULT across END keeps its repeat and steps the selector on the end screen. The END branch of `sending_poll()` calls `buttons_set_tap_mode(false)` with `keyer_stop()` and `buttons_set_enter_enabled(true)`. The call drops the pending press, the long-pending state and the repeat, as in Receiving (D-T39). Tap mode stays off. (2) F2, accepted: `receiving_leave()` calls `player_stop()` in every phase, also in SELECT and END. A second stop has no effect (section 5.4), and D-T12 asks every leave path to stop the module of its mode. The V-143 oracle "no player call" means "no playback starts": `fake_player_start_count()` stays 0. No phase guard. (3) Accepted deviation: `training_finish()` does not assign the selected level on a fail. `training_start()` copies the selected level into the session level. `training_select_step()` acts only in SELECT and END. So the selected level equals the session level at the finish. QA mutants M5 and M9 prove that the tests guard this. (4) Accepted: the developer changed QA-owned test files (`test_qa_*`) only for the interface change and the new entry. QA reviewed each change, and no check is removed or loosened (F3). Later interface changes in QA files go through QA. | (1) The user must not lose the preselection through a button that was pressed during the session. One call gives both modes the same rule. (2) A guard costs flash and changes nothing that the user can see or hear. (3) The invariant holds by construction, and the assignment costs flash. |
| D-T44 | Receiving waits for `enter` after the third wrong attempt (user decision 2026-10-15, `ui-ux.md` UX-64). (1) Answer state: phase RESULT and `training_last_check() == TRAINING_CHECK_FAILED`, in Receiving only. `receiving.c` tests it in one helper, `rx_is_answer_shown()`, and `rx_draw_item_line()` uses the same helper. (2) Engine: in phase RESULT, `training_hold_result(true)` sets the hold. `training_hold_result(false)` clears the hold and sets the phase duration to 0. The phase start does not change. So the next `training_poll()` ends the RESULT with the normal rule of 11.2: GAP, or END after the last item (RETRY after a WRONG check). Other phases: no effect. A phase change clears the hold. (3) The FAILED check sets the hold as before (12.2). `receiving_poll()` does not call `training_hold_result()`. The hold stays on after the answer playback ends, for any time. (4) Events in the answer state (tap mode is on): an `enter` tap (button, or a short tip closure) calls `player_stop()` and `training_hold_result(false)`. It is allowed also while the answer plays. A long `enter` (button, or a tip closure of 800 ms) plays the item again from its start with `player_start()`. The screen does not change. A long `menu/back` leaves, as in every phase. All other events do nothing and draw nothing: `menu/back`, `prev` and `next` taps, long `prev`, long `next`. An acted event calls `rng_mix(tick_now())`. (5) Row 7 in the answer state: the labels of `menu/back`, `prev` and `next` are blank. On the last item (`training_item_number() == training_item_count()`), the `enter` field shows `Finish`. On other items, it shows `Next`. Both texts are `MEM_CODE` constants. LC-4 does not apply to these two texts. Position and alignment: `ui-ux.md` UX-64. The key bar segments do not change. (6) The GAP event redraws the labels of the whole set (`rx_draw_labels()`), because row 7 can hold `Next`. (7) `TRAINING_FAILED_MS` applies to Sending only. Sending does not change: it has no button input in a session (the tip is the keyer), so it keeps the 2500 ms delay after the third wrong attempt. | The fixed 2500 ms delay gave too little time to compare the answer with the wrong picks. `enter` confirms and continues on every other training screen, and it is the paddle tip, so the hand can stay on the paddle. Tap `enter` = continue and long `enter` = replay is the same pair as in phase ITEM. The changed meaning of `training_hold_result(false)` keeps one function and costs the least flash: its only caller was the poll call of D-T14. |
| D-T45 | User decisions (2026-10-15, plan `lucky-seeking-locket.md`). (1) Start screen: the selector moves to row 1. Row 2 has a label for the kind of the level. The level description shows in the 8 x 16 font in the place of the item line (y 24-39), at every level kind. Index 0 shows K and M. A character level shows the new character. A word level and the final word phase show `Words of <len>`. (2) Sending start screen: row 5 shows the CW code of the new character, for every `Show CW Code` value. At index 0 it shows the codes of K and M as one group. At a word level, row 5 is blank. Receiving: row 5 is blank at every level. (3) End screen: the score and the result only, with one band in row 7. No selector and no description. (4) End screen controls: an `enter` press or a tip closure opens the start screen at the preselected level. The preselection rule of 11.4 does not change. A second `enter` starts the session. `menu/back` opens the main menu. `prev`, `next`, long events and the ring do nothing. (5) `ui-ux-embedded-designer` fixes the texts, the L1 code gap and the band label of the end screen (task D6). This revises D-T35 (2), D-T42 (1) and (4), and SL-2, SL-4, SL-5 and SL-7. | The user wants the level details to stand out. The end screen gets simpler. The user sees the next level on the start screen before the next session starts. |
| D-T46 | Engine interface of D-T45 (section 5.10). (1) `training_select_step()` acts only in phase SELECT. In END it returns false and changes nothing. (2) New call `bool training_reopen_select(void)`. In phase END it sets phase SELECT and returns true. The selected level does not change. So the start screen shows the preselection of 11.4: the session level + 1 after a pass (saturated, D-T5), the session level after a fail. The call reads no tick and does no progress or EEPROM access. In IDLE, SELECT, GAP, ITEM and RESULT it changes nothing and returns false. (3) `TRAINING_PHASE_END` is the session end screen: the outcome and the score are ready, and no selector acts. (4) After the call, the rules of D-T36 (9) apply. The session accessors keep the values of the last session, and no screen reads them in SELECT. (5) The invariant `training_selected_level() <= training_earned_level()` holds after the call. (6) The modes call `training_start()` only in SELECT. This revises D-T36 (4) and the wording of D-T43 (3). The selected level still equals the session level at the finish, because no step acts during a session or in END. | The engine owns the phase, so one engine call does the switch. The bool result lets the selector input test and act in one call, with no extra phase read. The preselection rule stays in one place (11.4). |
| D-T47 | Shared drawing and mode rules of D-T45 (section 5.11). (1) Code marks: `tx_draw_mark()`, `tx_draw_answer_code()` and the mark constants (`TX_MARK_Y_OFFSET`, `TX_MARK_HEIGHT`, `TX_DOT_WIDTH`, `TX_DASH_WIDTH`, `TX_MARK_GAP`) move from `sending.c` to `training_screen.c`. Two shared functions replace them: `training_screen_draw_code_mark()` and `training_screen_draw_code_group()`. Sending row 7, Sending row 5 and the start screen row 5 use them. The Sending pixels do not change. (2) Big line: the static helper `screen_draw_big_text()` in `training_screen.c` is the one 8 x 16 draw path. `training_screen_draw_item_text()` keeps its signature and calls the helper for each slot. The start screen calls the helper with a centered x. It never calls `training_item_length()`, which has no meaning in SELECT. (3) `training_screen_draw_end()` draws rows 1-7 of the end screen itself. It does not call `training_screen_draw_start()`. (4) `training_screen_select_input()` in END: an `enter` press calls `training_reopen_select()`. Then it draws row 0 and the start screen (all pages) and returns NONE. All other events in END give NONE with no redraw, except a `menu/back` press (LEAVE). In SELECT the rules of D-T38 stay. So START comes only from SELECT. `receiving_handle()` and `sending_handle()` lose the branch that draws the header after a START from END. (5) Buttons across END, SELECT and the session: see the rules B-1 to B-6 (section 12.6). (6) No change of the ISR files, `buttons.c`, `keyer.c`, `player.c`, `koch`, `progress` or `firmware/drivers/`. | One copy of the code drawing costs less flash and keeps Sending and the start screen equal. A static helper keeps the public interface and the three call sites of `training_screen_draw_item_text()`. A count parameter costs a store at each call. A centered x serves all three big texts: the slots of a 2-character item (x 52 and 68) are the cells of the centered 3-cell text `K M`. The selector input owns the screen switch, so both modes get the same rule with no mode code. |
| D-T48 | Review of D6 (2026-10-15): `approved`. Accepted UI values of `ui-ux.md`: UX-65 to UX-70, A-L6 to A-L8, 5.15 and 5.16.1 to 5.16.4. Also the revised V-vectors and V-171 of 9.4, and D-42 to D-44. (1) Labels: `New chars` at L1, `New char` at a character level, blank at a word level and in the final word phase. Both texts are centered, so both start in column 8. (2) Big line: `K M` (x 52), the character text in the `Prosigns` style (x 60, 48 or 40), `Words of <len>` (x 24). One rule: `x = 64 - 4 m`. (3) Code row: the L1 code gap is 9 px. It replaces the 3 px mark gap between the last mark of K and the first mark of M. So the L1 group is 44 px wide and starts at x 42. Widest single code: `,` (43 px). (4) End screen band: `OK` in columns 11-12, band x 52-71, y 57-63. This is the SL-5 rule for a label of 2 cells. (5) B-3 is in `ui-ux.md` 5.15. (6) Designer question 1, accepted, revised by D-T49 (1): a step redraws pages 0-5 in both modes, and Receiving writes page 5 blank. One code path for both modes costs less flash than a mode branch that saves one page in Receiving. (7) Designer question 2: no fallback code now. R-T14 stays open with the fallback order of section 19, and D-43 (DC-T11) gives the decision data. (8) No change of `ui-ux.md` is necessary. | The draft meets D-T45 to D-T47, SL-1 to SL-10 and B-1 to B-6. The pixel values of V-137, V-142 and V-171 follow the formulas of SL-2b and SL-2c. |
| D-T49 | Decisions on the T16 and Q7 results (2026-10-15). (1) Dirty pages, fix required (revises D-T48 (6), SL-4, SL-10 and P5, P6). On a byte change, `gfx` marks the page dirty. Unchanged bytes leave the page clean. `lcd.c` sends only dirty pages. T16 marks pages 0-5 dirty by hand after each step, and all pages at the switch from END to SELECT. These calls send pages with no pixel change over the software SPI: page 5 of Receiving at each step, and an unchanged label or big line. They follow the D-T48 (6) text, so this is a contract fix, not a developer fault. New rule: no screen code marks a page dirty by hand on a step or at the switch. The dirty pages are the pages whose bytes change. A step changes only pages 0-5. (2) Flash, accepted: +339 B is above the +250 B target and below the +400 B hard limit. The cause is the xdata parameter stores of the contract functions and helpers. Fix (1) makes it smaller. (3) xdata, accepted: +1 B is above the +0 B target and below the +2 B hard limit. (4) Stack, accepted: the deepest chain is 71 B (+5 B, target +6 B), the margin 102 B. `architector` checked the new step: `training_screen_draw_item_text()` saves 6 registers around the `screen_draw_big_text()` call. That fits the +5 B. (5) The band draws the label in normal video, inverts y 57-63 and clears y 56. The pixels equal the old band: accepted. (6) QA notes, accepted, not blocking: M7a is killed by one check, and only the new random walk reaches END. | (1) The user asked for as few redrawn pages as possible (`ui-ux.md` UX-70). The forced pages make each step up to 2 page transfers longer, and R-T14 is the open risk of this change. With the rule of `gfx`, the fallbacks (b) and (c) of R-T14 come at no cost. (2) to (4) Each value is inside its hard limit, and a rework to register parameters changes the contract signatures for a few bytes. |
| D-T50 | Decisions on the C1 to C3 results (2026-10-15). (1) Header at the saturation step, accepted deviation from D-T49 (1). `training_screen_draw_header()` fills row 0 and draws the inverted text again, so page 0 gets dirty at every step. Indexes 65534 and 65535 both show `L65535` with 100 items (D-T5). So at the step between them, the header bytes stay the same, and the step gives `DIRTY(0,1)`, not `DIRTY(1)`. The header code does not change. (2) Flash after C1: 28 561 B (+352 against the T16 baseline, +13 against the first T16 pass), accepted under the +400 B hard limit. The +13 B buys the byte-stable label and big line: the label is one 9-cell text with no clear, and the big line clears only at a change of its length. So a step between two character levels sends pages 0, 1, 3 and 4 only (`ui-ux.md` V-138). (3) The cache `screen_big_length` is safe: every other writer of rows 3 and 4 runs before a full start screen draw, and that draw clears rows 2-6 first. | (1) The case needs about 65 000 passed sessions and costs one page transfer. A byte-stable header costs flash on every build. (2) Fewer page transfers per step lower the R-T14 risk, and the cost is far below the hard limit. |
| D-T19 | A failed `Reset Statistics` shows `Not saved` over the Settings list. The menu stack does not change, so `OK` returns to Settings with the cursor on `Reset Statistics`. A failed settings save still shows the message after the pop, so its `OK` returns to the main menu. | This is the behavior of the current `menu_show_message()`. No menu change beyond D-T11. |

## 4. Baseline (measured)

`firmware/build.sh` at HEAD `06b28ec`, 2026-10-06, `build/putikeeg_cw_trainer.mem` and `.map`:

| Resource | Value |
| --- | --- |
| Flash | 19 055 of 61 440 B |
| xdata | 1400 of 3072 B (0x0001-0x0578) |
| DSEG (data) | 43 B (0x08-0x1F and 0x25-0x37) |
| Bit bytes | 4 B (0x20-0x23), bit bank 1 B (0x24) |
| Overlay | 5 B (0x38-0x3C) |
| Stack | starts at 0x3D, 195 B free |
| Deepest main-loop stack | 65 B, the `menu_render` text path (upper bound from the final review of `keyer_spec.md` section 17.12, not measured again here) |
| Largest ISR frame | 18 B (195 - 65 - 112 B margin of section 17.12) |
| Stack margin | 112 B |

T0 measures the deepest main-loop chain and the ISR frame again from the `.asm`.

## 5. Interfaces

General rules:

- All new modules are main-loop only, except the ISR side of the player.
- Parameters after the first are `MEM_XDATA` on the target, as in `gfx.h` and `keyer.h`. This keeps them out of
  internal RAM.
- Work variables of long calculations are `static MEM_XDATA`.
- Every public function with a parameter range clamps or rejects out-of-range values as stated. No function indexes a
  table out of range for any input.

### 5.1 Files

| File | Content | Users |
| --- | --- | --- |
| `inc/cw/morse.h`, `src/cw/morse.c` (change) | `morse_encode()` | player |
| `inc/cw/timing.h`, `src/cw/timing.c` (new) | `cw_timing_compute()` and its results | keyer, player |
| `inc/cw/keyer_shared.h`, `src/cw/keyer.c` (change) | Timing factors removed. `keyer_start()` calls `cw_timing_compute()`. | - |
| `inc/cw/player.h`, `src/cw/player.c` (new) | Main-loop player API and the shared player state | receiving |
| `inc/cw/player_shared.h` (new) | `extern` declarations of the shared player state, constants | player.c, player_service.h |
| `inc/cw/player_service.h` (new) | `static void player_service(void)` and ISR-private state | `sidetone.c` (target only), player tests |
| `src/sidetone/sidetone.c` (change) | Player macros, include, three-call ISR | - |
| `inc/util/rng.h`, `src/util/rng.c` (new) | 16-bit xorshift | koch, modes |
| `inc/util/crc8.h`, `src/util/crc8.c` (new) | `crc8_compute()` | settings, progress |
| `src/settings/settings_crc.h` (removed) | - | - |
| `inc/koch/koch.h`, `src/koch/koch.c` (new) | Koch and keyboard order tables, level model | training, keyboard |
| `src/koch/koch_item.c` (new) | `koch_make_item()` (D-T18) | training |
| `inc/koch/keyboard.h`, `src/koch/keyboard.c` (new) | Group keyboard | receiving |
| `inc/progress/progress.h`, `src/progress/progress.c` (new) | Progress record | training, stats, main |
| `inc/eeprom/eeprom_map.h` (change) | Progress page | progress |
| `inc/stats/stats.h`, `src/stats/stats.c` (change) | `bool stats_reset(void)` | menu |
| `inc/buttons/buttons.h`, `src/buttons/buttons.c` (change) | `buttons_set_tap_mode()` | receiving |
| `inc/modes/training.h`, `src/modes/training.c` (new) | Session engine | receiving, sending, training_screen |
| `inc/modes/training_screen.h`, `src/modes/training_screen.c` (new) | Header row, start screen, end screen, level selector (D-T38) | receiving, sending |
| `src/modes/training_screen_internal.h` (new, private) | Draw helpers shared by the training screens (D-T25) | `training_screen.c`, receiving, sending |
| `inc/modes/receiving.h`, `src/modes/receiving.c` (new) | Receiving mode | menu |
| `inc/modes/sending.h`, `src/modes/sending.c` (new) | Sending mode | menu |
| `inc/menu/menu.h`, `src/menu/*.c` (change) | New screens, placeholder removed | main |
| `src/main.c` (change) | `progress_load()` | - |

`firmware/CMakeLists.txt` takes all `src/**/*.c` with a glob. No firmware CMake change.

### 5.2 `cw/morse.h` addition

```c
/**
 * Returns the pattern of @p character (a table character or a prosign code MORSE_CHAR_SK to MORSE_CHAR_AS), or
 * MORSE_PATTERN_INVALID when the table has no such character. MORSE_CHAR_UNKNOWN gives MORSE_PATTERN_INVALID.
 */
uint8_t morse_encode(char character);
```

Behavior: a linear search of `morse_table` from index 2 to 127. It returns the first index whose entry equals
`character`. No new table. `morse_decode(morse_encode(c)) == c` for every table character.

### 5.3 `cw/timing.h` (new)

```c
#include <stdint.h>
#include "board/sidetone_config.h"
#include "mcu/compiler.h"

#define CW_TIMING_WPM_MIN 5U
#define CW_TIMING_WPM_MAX 50U
#define CW_TIMING_TICKS_HZ SIDETONE_SERVICE_HZ   /* the keyer and the player run in the Timer 0 ISR */
#define CW_TIMING_DIT_NUMERATOR 4800UL           /* CW_TIMING_TICKS_HZ * 1.2 s */
#define CW_TIMING_FARNSWORTH_C_FACTOR 240000UL   /* CW_TIMING_TICKS_HZ * 60 */
#define CW_TIMING_FARNSWORTH_S_FACTOR 148800UL   /* CW_TIMING_TICKS_HZ * 37.2 */
#define CW_TIMING_FARNSWORTH_DIVISOR 19UL

/* Results of the last cw_timing_compute() call, in Timer 0 calls. Main loop only. */
extern MEM_XDATA uint16_t cw_timing_dit_ticks;   /* round(4800 / wpm), 960..96 */
extern MEM_XDATA uint16_t cw_timing_unit_ticks;  /* Farnsworth unit, at least cw_timing_dit_ticks, at most 2370 */

/**
 * Computes the dit and the Farnsworth unit. @p wpm is clamped to CW_TIMING_WPM_MIN..CW_TIMING_WPM_MAX, then
 * @p eff_wpm to CW_TIMING_WPM_MIN..wpm.
 */
void cw_timing_compute(uint8_t wpm, MEM_XDATA uint8_t eff_wpm);
```

- The formulas are those of `keyer_spec.md` section 6.7, unchanged, with the same 32-bit step and the same clamp of
  the unit to the dit.
- The `#error` checks for 4000 calls per second and for the dit at 50 WPM (longer than two sidetone ramps) move to
  `timing.h`.
- `keyer_shared.h` loses `KEYER_DIT_TICKS_NUMERATOR` and the three `KEYER_FARNSWORTH_*` macros. `keyer.h` keeps
  `KEYER_WPM_MIN` and `KEYER_WPM_MAX`, defined as `CW_TIMING_WPM_MIN` and `CW_TIMING_WPM_MAX`.
- `keyer_start()` keeps its signature and its order of section 6.1 of `keyer_spec.md`. Step 2 (clamps) and the
  tick math become one call, `cw_timing_compute(wpm, eff_wpm)`, then:
  `keyer_dit_ticks = cw_timing_dit_ticks`, `keyer_dah_ticks = 3 * dit`, `keyer_two_dit_ticks = 2 * dit`,
  `keyer_word_gap_ticks = 5 * unit`.
- The static work variables `start_dit`, `start_denominator` and `start_unit` move to `timing.c` as `static
  MEM_XDATA`. `start_key_type` stays in `keyer.c`.

### 5.4 `cw/player.h`, `cw/player_shared.h`, `cw/player_service.h` (new)

`player.h`:

```c
#include <stdbool.h>
#include <stdint.h>
#include "mcu/compiler.h"

#define PLAYER_TEXT_MAX 8U  /* equal to KOCH_WORD_LEN_MAX */

/**
 * Stops a running playback, then plays @p length characters of @p text at @p wpm with the Farnsworth spacing of
 * @p eff_wpm (both clamped as in cw_timing_compute()). Returns false and plays nothing when @p length is 0 or above
 * PLAYER_TEXT_MAX, or when a character has no Morse code. The keyer must be stopped (D-T12).
 */
bool player_start(const char MEM_XDATA* text, MEM_XDATA uint8_t length, MEM_XDATA uint8_t wpm,
                  MEM_XDATA uint8_t eff_wpm);

/** Returns true from player_start() until the last mark ends or player_stop() runs. */
bool player_is_busy(void);

/** Stops the playback and requests the sidetone off. A second call has no other effect. */
void player_stop(void);
```

`player_shared.h` (defined in `player.c`):

```c
#define PLAYER_EARLY_RELEASE_TICKS SIDETONE_RAMP_STEPS  /* 32: the fall ends at the mark end */
#define PLAYER_CHAR_GAP_UNITS 3U

extern volatile MEM_BIT player_is_enabled;          /* main sets; main and ISR clear */
extern volatile MEM_BIT player_is_start_requested;  /* main sets; ISR clears */
extern volatile MEM_XDATA uint8_t player_codes[PLAYER_TEXT_MAX];  /* play form, see below */
extern volatile MEM_XDATA uint8_t player_code_count;
extern volatile MEM_XDATA uint16_t player_dit_ticks;
extern volatile MEM_XDATA uint16_t player_dah_ticks;
extern volatile MEM_XDATA uint16_t player_char_gap_ticks;
```

Play form of one character: bit 0 is the next element (1 = dah). After each element the ISR shifts the value right
by 1. The value 1 means "no element left". `player_start()` builds it from the `morse_encode()` pattern. The
pattern has the first element in its highest bit below the start marker. Conversion:

```c
code = 1U;
while (pattern > 1U) {
  code = (uint8_t)((uint8_t)(code << 1) | (pattern & 1U));
  pattern >>= 1;
}
```

Example: A (`.-`) has pattern 0b101 and code 0b110.

`player_service.h`: same structure as `keyer_service.h`. ISR-private state, names are part of the contract (host
tests read them):

```c
static MEM_BIT player_is_mark;      /* 1: a mark runs, 0: a gap runs */
static MEM_DATA uint16_t player_ticks;  /* calls left in the current mark or gap */
static MEM_DATA uint8_t player_code;    /* play form of the current character */
static MEM_DATA uint8_t player_index;   /* index of the current character */
```

The behavior is in section 9.

### 5.5 `util/rng.h`, `util/crc8.h` (new)

```c
#define RNG_SEED_FALLBACK 0xACE1U

/** Sets the state to @p seed; a seed of 0 gives RNG_SEED_FALLBACK. */
void rng_seed(uint16_t seed);
/** XORs @p entropy into the state; a result of 0 gives RNG_SEED_FALLBACK. Does not step. */
void rng_mix(uint16_t entropy);
/** Steps the xorshift (x ^= x << 7; x ^= x >> 9; x ^= x << 8) and returns the new state. Never 0. */
uint16_t rng_next(void);
/** Returns 0..limit-1 from the high byte of rng_next(): (hi * limit) >> 8. Returns 0 and does not step for 0. */
uint8_t rng_below(uint8_t limit);
```

- State: `static MEM_XDATA uint16_t`. The C start-up value 0 acts as `RNG_SEED_FALLBACK` on the first `rng_next()`.
- Reference: from seed 1, `rng_next()` gives 0x8181, 0x6021, 0xE999, 0x2E0B, 0xB59E. The period is 65535.
- `rng_below()` multiplies two bytes into 16 bits (`MUL AB`). No library helper.

```c
/** CRC-8: polynomial 0x07, initial value 0, no reflection, no final XOR. "123456789" gives 0xF4. */
uint8_t crc8_compute(const MEM_XDATA uint8_t* data, uint8_t length);
```

The body is the current `settings_crc8()`, unchanged.

### 5.6 `koch/koch.h` (new)

```c
#include <stdint.h>
#include "mcu/compiler.h"

typedef uint8_t koch_kind_t;
#define KOCH_KIND_CHAR 0U
#define KOCH_KIND_WORD 1U

#define KOCH_CHAR_COUNT 43U          /* entries of the Koch table (D-T3, Q-T1) */
#define KOCH_FIRST_LEARNED 2U        /* level index 0 has K and M */
#define KOCH_WORD_ALPHABET_MAX 40U   /* D-T4: BT, SK and AR stay out of words */
#define KOCH_WORD_LEN_MIN 2U
#define KOCH_WORD_LEN_MAX 8U         /* equal to settings_max(SETTINGS_ID_MAX_WORD_LEN) */
#define KOCH_CHAR_ITEMS 50U
#define KOCH_WORD_ITEMS 10U
#define KOCH_FINAL_ITEMS_MAX 100U
#define KOCH_LEVEL_MAX 65535U
/* First level index of the final word phase: the first character level whose learned count is above the table. */
#define KOCH_FINAL_LEVEL (((KOCH_CHAR_COUNT % 2U) != 0U) ? (3U * ((KOCH_CHAR_COUNT - 1U) / 2U)) \
                                                         : ((3U * ((KOCH_CHAR_COUNT - 2U) / 2U)) + 1U))

/** Returns character @p index of the Koch order, or '\0' for an index at or above KOCH_CHAR_COUNT. */
char koch_char(uint8_t index);
/** Returns character @p index of the keyboard order, or '\0' for an index at or above KOCH_CHAR_COUNT. */
char koch_keyboard_char(uint8_t index);
/** Returns the index of @p character in the Koch order, or KOCH_CHAR_COUNT when it is not in the table. */
uint8_t koch_index_of(char character);

koch_kind_t koch_kind(uint16_t level);
/** Learned characters at @p level: KOCH_FIRST_LEARNED..KOCH_CHAR_COUNT. */
uint8_t koch_learned_count(uint16_t level);
/** Items of a session at @p level: KOCH_CHAR_ITEMS, KOCH_WORD_ITEMS or 10..KOCH_FINAL_ITEMS_MAX. */
uint8_t koch_item_count(uint16_t level);
/** Word length at @p level, or 0 at a character level. @p max_word_len is clamped to 2..KOCH_WORD_LEN_MAX. */
uint8_t koch_word_length(uint16_t level, MEM_XDATA uint8_t max_word_len);
/** Characters that an item of @p level can hold: the learned count, or min(learned, 40) at a word level (D-T4). */
uint8_t koch_alphabet_count(uint16_t level);
/** Level number that the user sees: @p level + 1, saturated at KOCH_LEVEL_MAX (D-T5, D-T37). */
uint16_t koch_shown_level(uint16_t level);
/**
 * Smallest score that passes a session of @p item_count items (D-T33): ceil(0.8 * item_count) for KOCH_KIND_WORD,
 * else ceil(0.9 * item_count).
 */
uint8_t koch_pass_threshold(koch_kind_t kind, MEM_XDATA uint8_t item_count);
/**
 * Writes one random item of @p level and a NUL to @p text (KOCH_WORD_LEN_MAX + 1 bytes) and returns its length.
 * Uses rng_below() in the order of section 7. Defined in src/koch/koch_item.c (D-T18).
 */
uint8_t koch_make_item(uint16_t level, MEM_XDATA uint8_t max_word_len, char MEM_XDATA* MEM_XDATA text);
```

Tables in `koch.c`, both `static const MEM_CODE char[]` with a `_Static_assert` that the size is `KOCH_CHAR_COUNT`
(D-T21):

- `koch_order`: `K M R S U A P T L O W I . N J E F 0 Y , V G 5 / Q 9 Z H 3 8 B ? 4 2 7 C 1 D 6 X`, then
  `MORSE_CHAR_BT`, `MORSE_CHAR_SK`, `MORSE_CHAR_AR`.
- `koch_keyboard_order`: the letters `A` to `Z`, the digits `0` to `9`, then `. , ? /`, then `MORSE_CHAR_BT`,
  `MORSE_CHAR_SK`, `MORSE_CHAR_AR`.

The two tables hold the same set. A host test proves it. `koch.c` adds `_Static_assert` checks: both tables have
`KOCH_CHAR_COUNT` entries, `KOCH_WORD_ALPHABET_MAX <= KOCH_CHAR_COUNT`, `KOCH_FINAL_LEVEL <= 255`.

### 5.7 `koch/keyboard.h` (new)

```c
#include <stdbool.h>
#include <stdint.h>

/* Runs from left to right, named after the button under each run (D-T27, D-T28). */
#define KEYBOARD_GROUP_MENU 0U    /* menu/back, left */
#define KEYBOARD_GROUP_PREV 1U
#define KEYBOARD_GROUP_NEXT 2U
#define KEYBOARD_GROUP_ENTER 3U   /* right */
#define KEYBOARD_GROUP_COUNT 4U
#define KEYBOARD_NO_PICK '\0'
#define KEYBOARD_DEPTH_MAX 3U     /* selects per pick, enough for 4^3 = 64 characters */

/**
 * Builds the list of the first @p koch_count Koch characters in keyboard order and shows the whole list. The count
 * is clamped to 2..KOCH_CHAR_COUNT. Receiving passes training_alphabet_count(), so a word session has no prosign.
 */
void keyboard_reset(uint8_t koch_count);
/** Number of characters in the list. */
uint8_t keyboard_count(void);
/** Character @p index of the list, or KEYBOARD_NO_PICK out of range. */
char keyboard_char(uint8_t index);
/** List index of the first character of @p group in the current range. An unknown group gives the range end. */
uint8_t keyboard_group_first(uint8_t group);
/** Characters in @p group of the current range, 0 for an empty group or an unknown group. */
uint8_t keyboard_group_size(uint8_t group);
/**
 * Selects @p group. An empty or unknown group changes nothing. A group of one character returns that character and
 * shows the whole list again. A larger group becomes the current range. Returns KEYBOARD_NO_PICK unless it picks.
 */
char keyboard_select(uint8_t group);
/** Goes back to the range before the last narrowing. Returns false at the whole list. */
bool keyboard_back(void);
/** Shows the whole list. */
void keyboard_top(void);
/** Narrowings since the whole list, 0..KEYBOARD_DEPTH_MAX - 1. */
uint8_t keyboard_depth(void);
```

The algorithm is in section 8.

### 5.8 `progress/progress.h` (new) and `eeprom/eeprom_map.h`

```c
#include <stdbool.h>
#include <stdint.h>
#include "mcu/compiler.h"

#define PROGRESS_MODE_RECEIVING 0U
#define PROGRESS_MODE_SENDING 1U
#define PROGRESS_WPM_MIN 5U          /* equal to settings_min(SETTINGS_ID_RX_WPM) and TX */
#define PROGRESS_WPM_MAX 50U         /* equal to settings_max(SETTINGS_ID_RX_WPM) and TX */
#define PROGRESS_SPEED_COUNT 46U
#define PROGRESS_RECORD_SIZE 188U
#define PROGRESS_MAGIC_0 0x4BU       /* 'K' */
#define PROGRESS_MAGIC_1 0x50U       /* 'P' */
#define PROGRESS_LAYOUT_VERSION 1U

typedef uint8_t progress_status_t;
#define PROGRESS_OK 0U
#define PROGRESS_READ_FAILED 1U
#define PROGRESS_NO_RECORD 2U        /* magic mismatch, for example an erased page */
#define PROGRESS_BAD_VERSION 3U
#define PROGRESS_BAD_CRC 4U
#define PROGRESS_SAVE_FAILED 5U

/** Reads and checks the record. On any failure every level is 0. Never writes the EEPROM. Call once at start-up. */
progress_status_t progress_load(void);
/** Level index of @p mode at @p wpm (clamped to 5..50). 0 for an unknown mode. */
uint16_t progress_level(uint8_t mode, MEM_XDATA uint8_t wpm);
/** Adds 1 to the level in RAM. Returns false and changes nothing at 65535 or for an unknown mode. */
bool progress_advance(uint8_t mode, MEM_XDATA uint8_t wpm);
/** Erases page 1 and writes the record. Returns PROGRESS_OK or PROGRESS_SAVE_FAILED. */
progress_status_t progress_save(void);
/** Sets every level to 0 in RAM, then saves. Returns PROGRESS_OK or PROGRESS_SAVE_FAILED. */
progress_status_t progress_reset(void);
```

`eeprom_map.h`:

```c
#define EEPROM_MAP_PROGRESS_PAGE 1U
#define EEPROM_MAP_PROGRESS_ADDRESS 0x0200U
#define EEPROM_MAP_FIRST_FREE_PAGE 2U     /* pages 2-7 stay free */
#define EEPROM_MAP_FREE_ADDRESS 0x0400U
#define EEPROM_MAP_FREE_SIZE 0x0C00U      /* 0x0400-0x0FFF */
```

The record is in section 10.

### 5.9 `stats/stats.h` and `buttons/buttons.h` changes

```c
/** Sets every training level back to level 1 and stores it. Returns true when the store succeeded. */
bool stats_reset(void);
```

`stats.c`: `return progress_reset() == PROGRESS_OK;`.

```c
/**
 * Tap mode on: MENU, PREV, NEXT and ENTER give no event at the press. A release before BUTTONS_LONG_PRESS_MS gives
 * the press event then. A hold of BUTTONS_LONG_PRESS_MS gives only the long event. No repeat events. The call drops
 * the pending presses, the long-pending state and the repeat of all four buttons, so a hold that started before the
 * call gives no event. buttons_init() sets tap mode off.
 */
void buttons_set_tap_mode(bool is_enabled);
```

Changes of D-T28 (`buttons.h` declarations keep their signatures, and only the comments and the behavior change):

| Item | Contract |
| --- | --- |
| Event values | No new macro. Long `enter` = `BUTTON_EVENT_ENTER \| BUTTON_EVENT_LONG_FLAG` (0x44). `BUTTON_EVENT_CODE()` gives `BUTTON_EVENT_ENTER`. |
| Tap mode off | As today: ENTER gives its press event at the press, no long event, no repeat (AC-K45 unchanged). MENU, PREV and NEXT as today. |
| Tap mode on | All four buttons: press event at a release before `BUTTONS_LONG_PRESS_MS`, only the long event for a hold, no repeat. |
| `buttons_poll()` order | Pending presses: MENU, ENTER, PREV, NEXT (unchanged). Long events: MENU, ENTER, PREV, NEXT (ENTER comes only in tap mode). Then a repeat. |
| `buttons_set_enter_enabled(false)` | Also clears the long-pending ENTER bit. |
| `buttons_set_enter_enabled(true)` | Also clears the long-pending ENTER bit. A contact closed at the call gives no tap and no long event until it opens and closes again. |
| `buttons_poll()` header comment | Names ENTER among the long-press buttons "in tap mode only". |
| File header comment of `buttons.h` | Names the long press of ENTER in tap mode. |

- "Release" and "hold" use the debounced state (D-K4 of `keyer_spec.md`). A release that debounces at or after
  `BUTTONS_LONG_PRESS_MS` is a hold, also with a late poll (D-T22).
- The order rules of `buttons_poll()` stay: pending presses first, then long events, then repeats.
- Tap mode state: `static MEM_XDATA bool buttons_is_tap_mode`.
- Press times: one 16-bit entry per long-press button, so ENTER adds one entry (+2 B xdata). The developer can merge
  the press-order and long-order tables, because both orders are now MENU, ENTER, PREV, NEXT.
- Entry by `enter`: the menu `enter` press opens Receiving and `receiving_enter()` turns tap mode on while the
  button or the tip is still closed. The call clears the long-pending ENTER, so its release gives no event and a
  hold gives no long event.

### 5.10 `modes/training.h` (new): session engine

```c
#include <stdbool.h>
#include <stdint.h>
#include "koch/koch.h"
#include "mcu/compiler.h"

#define TRAINING_MODE_RECEIVING 0U   /* equal to PROGRESS_MODE_RECEIVING */
#define TRAINING_MODE_SENDING 1U     /* equal to PROGRESS_MODE_SENDING */
#define TRAINING_ATTEMPTS 3U
#define TRAINING_TEXT_SIZE (KOCH_WORD_LEN_MAX + 1U)
/* Sending answer character for a word space inside a word (D-T31). No item character and no decoded character is ' '. */
#define TRAINING_CHAR_WORD_SPACE ' '

/* Delays in ms, ui-ux.md sections 5.14 and 6.10 (UX-46). Each has an #ifndef guard, so -D can set it for host tests. */
#define TRAINING_START_MS 1000U
#define TRAINING_CORRECT_MS 600U
#define TRAINING_WRONG_MS 1200U
#define TRAINING_FAILED_MS 2500U          /* Sending only: Receiving waits for an enter tap (D-T44) */
#define TRAINING_RECEIVING_GAP_MS 500U
#define TRAINING_SENDING_GAP_MS 300U

typedef uint8_t training_phase_t;
#define TRAINING_PHASE_IDLE 0U    /* no session */
#define TRAINING_PHASE_GAP 1U     /* pause before an item; Sending shows a blank item area */
#define TRAINING_PHASE_ITEM 2U    /* the item is shown or played; answers count */
#define TRAINING_PHASE_RESULT 3U  /* a check result shows; answers are dropped */
#define TRAINING_PHASE_END 4U     /* session end screen: the outcome is ready; no selector acts (D-T46) */
#define TRAINING_PHASE_SELECT 5U  /* start screen: the level selector, no session (D-T36) */

typedef uint8_t training_check_t;
#define TRAINING_CHECK_PARTIAL 0U  /* the answer so far is accepted; more characters needed */
#define TRAINING_CHECK_CORRECT 1U
#define TRAINING_CHECK_WRONG 2U    /* an attempt is used; attempts remain */
#define TRAINING_CHECK_FAILED 3U   /* third wrong attempt; the answer shows */
#define TRAINING_CHECK_IGNORED 4U  /* not in TRAINING_PHASE_ITEM; the character is dropped */

typedef uint8_t training_event_t;
#define TRAINING_EVENT_NONE 0U
#define TRAINING_EVENT_ITEM 1U   /* ITEM began with a new item */
#define TRAINING_EVENT_RETRY 2U  /* ITEM began again with the same item after a wrong attempt */
#define TRAINING_EVENT_GAP 3U    /* GAP began after a resolved item */
#define TRAINING_EVENT_END 4U    /* END began; the outcome is ready */

typedef uint8_t training_outcome_t;
#define TRAINING_OUTCOME_FAIL 0U            /* the level repeats */
#define TRAINING_OUTCOME_PASS 1U            /* the level went up and is stored */
#define TRAINING_OUTCOME_PASS_NOT_SAVED 2U  /* the level went up in RAM only */
#define TRAINING_OUTCOME_PASS_TOP 3U        /* passed at level index 65535; no change, no save (D-T17) */
#define TRAINING_OUTCOME_PASS_REPLAY 4U     /* passed below the earned level; no change, no save (D-T36) */

/**
 * Prepares @p mode (unknown gives RECEIVING) at @p wpm with @p max_word_len and selects the earned level. Phase
 * SELECT. No session runs. Reads the progress RAM only (D-T36).
 */
void training_prepare(uint8_t mode, MEM_XDATA uint8_t wpm, MEM_XDATA uint8_t max_word_len);
/**
 * Starts a session at training_selected_level() with the values of training_prepare(). Phase GAP, TRAINING_START_MS.
 * No phase check: the modes call it in SELECT only (D-T46).
 */
void training_start(void);
/**
 * In phase SELECT: moves the selected level one step up (@p is_up) or down, inside 0..training_earned_level().
 * No wrap. Returns true when the level changed. Other phases, END included: no change, returns false (D-T46).
 */
bool training_select_step(bool is_up);
/**
 * In phase END: sets phase SELECT and keeps the selected level, so the start screen shows the preselection of
 * section 11.4. Reads no tick, no progress. Returns true. Other phases: no change, returns false (D-T46).
 */
bool training_reopen_select(void);
/** Level index that the next training_start() uses: 0..training_earned_level(). */
uint16_t training_selected_level(void);
/** Earned level index: progress_level() of the prepared mode and speed, read at each call. */
uint16_t training_earned_level(void);
/** Ends the session without a store. Phase IDLE. */
void training_stop(void);
/** Runs the time-based steps of section 11. Returns at most one event. Reads the tick once. */
training_event_t training_poll(void);
/** Adds @p character to the answer and checks it (section 11.3). */
training_check_t training_add_char(char character);
/** Removes the last answer character in phase ITEM. Returns false when there is none. */
bool training_undo_char(void);
/**
 * In phase RESULT: true stops the RESULT delay. False clears the hold and sets the delay to 0, so the next
 * training_poll() ends the RESULT. Other phases: no effect. A phase change clears the hold (D-T14, D-T44).
 */
void training_hold_result(bool is_held);

training_phase_t training_phase(void);
uint8_t training_mode(void);
uint8_t training_wpm(void);                 /* the progress key of the session */
koch_kind_t training_kind(void);
uint16_t training_level(void);              /* level index of the session */
uint16_t training_shown_level(void);        /* koch_shown_level(training_level()) */
uint16_t training_new_shown_level(void);    /* koch_shown_level(training_selected_level()) (D-T36) */
uint8_t training_learned_count(void);
uint8_t training_alphabet_count(void);      /* koch_alphabet_count() of the session level */
uint8_t training_item_number(void);         /* 0 before the first item, then 1..item count */
uint8_t training_shown_item_number(void);   /* header number: GAP gives item number + 1, else the item number */
uint8_t training_item_count(void);
uint8_t training_score(void);               /* items correct on the first attempt */
uint8_t training_pass_threshold(void);
uint8_t training_wrong_attempts(void);      /* of the current item, 0..TRAINING_ATTEMPTS */
training_check_t training_last_check(void);
training_outcome_t training_outcome(void);  /* meaningful in phase END */
const char MEM_XDATA* training_item_text(void);    /* NUL-terminated, 1..KOCH_WORD_LEN_MAX characters */
uint8_t training_item_length(void);
const char MEM_XDATA* training_answer_text(void);  /* NUL-terminated, 0..item length characters */
uint8_t training_answer_length(void);
```

Rules of D-T36 for the level selection:

- `training_prepare()` replaces the level read of the old `training_start()`. The clang-tidy finding
  `bugprone-easily-swappable-parameters` of D-T25 moves with the parameters to `training_prepare()`: no suppression.
- `training_start()` has no phase check. The modes call it only in SELECT (revised by D-T46: no start from END).
- `training_reopen_select()` (D-T46) is the only way from END to SELECT. `training_prepare()` also sets SELECT, but
  it selects the earned level, so it loses the preselection.
- Invariant: `training_selected_level() <= training_earned_level()` after every engine call.
- In phase SELECT, only `training_phase()`, `training_mode()`, `training_wpm()`, `training_selected_level()`,
  `training_earned_level()` and `training_new_shown_level()` have a meaning. The session accessors keep the values
  of the last session. Screen code in SELECT reads only these six functions and the `koch` level model.
- `training_poll()` returns `TRAINING_EVENT_NONE` in SELECT. `training_add_char()` returns IGNORED,
  `training_undo_char()` returns false and `training_hold_result()` does nothing, as in every phase other than ITEM
  and RESULT.

### 5.11 `modes/training_screen.h`, `modes/receiving.h`, `modes/sending.h` (new)

```c
/* training_screen.h: drawing shared by Receiving and Sending. Layouts: ui-ux.md (U-7, U-10). */
/** Draws row 0 (ui-ux.md 5.11.1): mode tag, speeds, shown level, training_shown_item_number() and the count. */
void training_screen_draw_header(void);
/** Draws the session end screen in rows 1-7 from the engine state, with the level selector (D-T38). */
void training_screen_draw_end(void);
```

Additions of D-T38 to `training_screen.h` (it includes `buttons/buttons.h` for `button_event_t`):

```c
typedef uint8_t training_select_action_t;
#define TRAINING_SELECT_NONE 0U   /* the event did nothing, or it only changed the selection */
#define TRAINING_SELECT_START 1U  /* the user starts a session at the selected level */
#define TRAINING_SELECT_LEAVE 2U  /* the user leaves the mode */

/** Draws the start screen in rows 1-7 (ui-ux.md, task D4): the selector, the description and the Start band. */
void training_screen_draw_start(void);
/**
 * Redraws the rows that show the selection: the selector line and the description line of the start screen (SELECT)
 * or of the end screen (END), and row 0 when ui-ux.md shows the selected level in the header of that screen.
 */
void training_screen_draw_select(void);
/**
 * Applies @p event in phase SELECT or END. prev or next, press or repeat: training_select_step() (next is up), then
 * training_screen_draw_select() when the level changed. enter press: START. menu/back press: LEAVE. A long event
 * and every other event: NONE, no redraw.
 */
training_select_action_t training_screen_select_input(button_event_t event);
```

Revision of D-T47 (task T16). The signatures above do not change. The doc comments and the behavior change to:

```c
/** Draws rows 1-7 of the session end screen from the engine state (SL-7). Does not draw the start screen (D-T47). */
void training_screen_draw_end(void);
/**
 * Draws rows 1-7 of the start screen (ui-ux.md 5.16.1, task D6): the selector, the label, the big line, the code
 * group in Sending, and the Start band. Does not draw row 0.
 */
void training_screen_draw_start(void);
/** In phase SELECT: redraws rows 0-5 for the selected level: the header and the start screen above the band (SL-4). */
void training_screen_draw_select(void);
/**
 * Applies @p event in phase SELECT or END (D-T38, D-T47). A long event: NONE, no redraw.
 * SELECT: prev or next, press or repeat: training_select_step() (next is up), then training_screen_draw_select()
 * when the level changed. enter press: START. menu/back press: LEAVE. Every other event: NONE, no redraw.
 * END: enter press: training_reopen_select(), training_screen_draw_header() and training_screen_draw_start(), then
 * NONE. menu/back press: LEAVE. Every other event, prev and next included: NONE, no redraw.
 */
training_select_action_t training_screen_select_input(button_event_t event);
```

The END branch needs no phase read: `training_reopen_select()` returns false in SELECT, and the `enter` press then
gives START. In END, `training_select_step()` returns false, so the existing `prev` and `next` path draws nothing.

Additions of D-T47 to `src/modes/training_screen_internal.h` (names and signatures binding):

```c
/* The centered code group: the Sending answer code and the start screen code, marks on y 43 and 44. */
#define TRAINING_SCREEN_CODE_GROUP_ROW 5U

/**
 * Draws one code mark in row @p row from pixel x @p x: a dah when @p is_dah, else a dit, on y = 8 row + 3 and
 * 8 row + 4 (ui-ux.md 5.13.3). Returns the x of the next mark: x + mark width + mark gap.
 */
uint8_t training_screen_draw_code_mark(uint8_t x, MEM_XDATA uint8_t row, MEM_XDATA bool is_dah);
/**
 * Draws the marks of @p first and, when @p second is above MORSE_PATTERN_EMPTY, the character gap and the marks of
 * @p second, as one group centered in row TRAINING_SCREEN_CODE_GROUP_ROW: x = (128 - w) / 2, w = the pixel width
 * from the first mark to the end of the last mark. Does not clear the row.
 */
void training_screen_draw_code_group(uint8_t first, MEM_XDATA uint8_t second);
```

Static helper of D-T47 in `src/modes/training_screen.c` (name and signature binding):

```c
/* Selects FONTS_ID_8X16, draws @p text in normal video from pixel (@p x, 24), then selects FONTS_ID_5X8 again. */
static void screen_draw_big_text(const char MEM_XDATA* text, MEM_XDATA int16_t x);
```

Rules of D-T47 for the drawing code:

- The mark geometry constants are private to `training_screen.c`: dit 2 px, dah 6 px, mark gap 3 px, mark height
  2 px, y offset 3 px (`ui-ux.md` 5.13.3, the values of the moved `TX_*` constants). The character gap of the L1 group
  is a private constant with the value of `ui-ux.md` (task D6). Use the prefix `SCREEN_CODE_`, because
  `SCREEN_MARK_WIDTH` already names the block and bar width.
- `sending.c` keeps `TX_CODE_MAX_MARKS`, `TX_CODE_ROW` (7) and `TX_CODE_X` (2). Its live mark becomes
  `tx_code_x = training_screen_draw_code_mark(tx_code_x, TX_CODE_ROW, tx_is_dah);`. Its FAILED row 5 becomes
  `training_screen_draw_code_group(morse_encode(...), MORSE_PATTERN_EMPTY)`. `tx_draw_mark()`,
  `tx_draw_answer_code()`, `tx_mark_x` and `tx_shift` go away. `sending.c` still clears row 5 before it draws.
- `training_screen_draw_item_text()` keeps its signature and its pixels. It clears the item line, then calls
  `screen_draw_big_text()` once for each slot. A font switch for each character is accepted.
- The start screen draws the big line with one `screen_draw_big_text()` call at `x = 64 - 4 m`, `m` = the cells of
  the text. It reads no session accessor.
- One static function draws rows 1-5 of the start screen. `training_screen_draw_start()` and
  `training_screen_draw_select()` both call it, so a step and a full render give the same pixels.
- One band helper draws row 7 of both screens. Its label text is a parameter: `Start`, or the end screen label of
  task D6 (SL-5).
- New work variables are `static MEM_XDATA`. If no call nests inside its use, an existing xdata work value can serve
  again (for example `screen_x`). No new `MEM_DATA`, no new `bool` outside xdata.

Contract of the selector screens. `ui-ux.md` (task D4) owns the rows, columns, texts and markers. This document owns
the data and the limits:

| ID | Rule |
| --- | --- |
| SL-1 | Selector line data: the shown selected level `training_new_shown_level()` and the shown earned level `koch_shown_level(training_earned_level())`. With `training_selected_level() > 0`, the `<` marker shows. Else it does not. With `training_selected_level() < training_earned_level()`, the `>` marker shows. Else it does not. Example of the plan: `<  L5 of 20  >`. |
| SL-2 | Revised by D-T45 and D-T47: SL-2a to SL-2c replace the description line. Old rule, kept for the data: description line data, from `L = training_selected_level()`. Level 0: the text for K and M (plan: `K and M`). Other character level (`koch_kind(L) == KOCH_KIND_CHAR`): the new character `koch_char(koch_learned_count(L) - 1)` through `morse_format()` in the `Prosigns` style (plan: `New char F`). Word level: `koch_word_length(L, max)` with `max` = `settings_get(SETTINGS_ID_MAX_WORD_LEN)` (plan: `Words of 4`). The final word levels use the word rule. Any extra value (for example the item count `koch_item_count(L)`) comes from the `koch` level model. |
| SL-2a | Start screen, row 1: the selector line of SL-1 (`ui-ux.md` 5.16.2 rules, row 1 in place of row 4). Row 2: the label of the level kind, a `MEM_CODE` constant for each of level 0, another character level, and a word level. A blank label is allowed. Texts: task D6. |
| SL-2b | Start screen, big line. It uses the 8 x 16 font with the cell top at y 24 (rows 3 and 4). One `screen_draw_big_text()` call draws it at `x = 64 - 4 m`, `m` = the cells of the text. The text has at most 15 cells. Level 0: K and M (task D6 text, for example `K M`: then K is at x 52 and M at x 68, the slots of a 2-character item). A `MEM_CODE` constant or a text built from `koch_char(0)` and `koch_char(1)` is allowed. Character level: the new character `koch_char(koch_learned_count(L) - 1)` through `morse_format()` in the `Prosigns` style, 1 to 6 cells. Word level and final word phase: `Words of <len>`, `len` = `koch_word_length(L, settings_get(SETTINGS_ID_MAX_WORD_LEN))`. |
| SL-2c | Start screen, row 5. Sending (`training_mode() == TRAINING_MODE_SENDING`): `training_screen_draw_code_group()` with `morse_encode()` of the new character and `MORSE_PATTERN_EMPTY`. Level 0: the codes of `koch_char(0)` and `koch_char(1)` as one group. Word level and final word phase: blank. `Show CW Code` has no effect. Receiving: blank at every level. |
| SL-3 | Each text is at most `TRAINING_SCREEN_COLUMNS` (25) cells, ASCII 0x20-0x7E, built in `training_screen_text`. No new `morse_format()` style, no text table per level. Fixed words are `MEM_CODE` constants. |
| SL-4 | Revised by D-T47 and D-T49. A step redraws only rows 0-5 (`training_screen_draw_select()`). Only the pages whose bytes change get dirty, inside pages 0-5. No code marks a page dirty by hand. Exception (D-T50 (1)): page 0 is dirty at every step, also at the step between indexes 65534 and 65535. Pages 6 and 7 stay. A press or a repeat at a limit changes nothing and redraws nothing. In END no step acts. |
| SL-5 | Revised by D-T47. One helper draws the band of the start screen and of the end screen. The label is its only parameter: `Start` on the start screen, the end screen label `OK` of task D6 on the end screen. The helper derives the band from the label length `n`. The text starts in column `c = floor((25 - n) / 2)`. The band covers cells `c - 1` to `c + n`, y 57-63, and the gap line y 56 stays off across it. `Start`: band x 47-81 (as before). `OK`: band x 52-71. If D6 gives another geometry, `architector` decides first. |
| SL-6 | Header in SELECT: the level field shows `training_new_shown_level()`. The item field shows `1/<n>` with `n` = `koch_item_count(training_selected_level())` (`ui-ux.md` 5.16.1). It reads no session accessor. Header in END: the header of the session that ended (`ui-ux.md` 5.11.1). No step acts in END (D-T46). |
| SL-7 | Revised by D-T45. End screen (`ui-ux.md` 5.15, task D6). Row 1: `Not saved` (PASS_NOT_SAVED only). Row 2: the score. Row 3: the result text of D-T42 (3). Rows 4-6: blank. Row 7: the band with the end screen label. No selector, no description. |
| SL-8 | The start screen reads only the six SELECT accessors of D-T36 (9), the `koch` level model, `morse_format()`, `morse_encode()` and `settings_get()`. It never reads `training_item_length()` or another session accessor. |
| SL-9 | Pixel limits. Big line pixels inside x 4-123. Code marks inside x 2-126. Pixels with x = 0, 1 and 127 off in rows 1-7. Widest texts: `<AR/+>` and `<BT/=>` (6 cells, 48 px, x 40), and `Words of 8` (10 cells, x 24). Widest codes: `,` (43 px) and the L1 group (K 20 px, the D6 gap, M 15 px). |
| SL-10 | Page budget, revised by D-T49: only the pages whose bytes change get dirty. END to SELECT (`enter` on the end screen): rows 0-7 redrawn, row 0 with the SELECT header of SL-6. START from SELECT: pages 1-7, row 0 stays (section 12.2). END event: all pages, as before. |

```c
/* receiving.h. Main loop only; only menu.c calls these functions. */
/** Seeds the RNG, reads the settings, sets the tone and prepares the engine (phase SELECT). Tap mode stays off. Draws nothing. */
void receiving_enter(void);
/** Draws the whole Receiving screen for the current state. menu_render() calls it after the clear. */
void receiving_render(void);
/** Runs the engine and the playback, and redraws the rows that change. */
void receiving_poll(void);
/** Applies a button event (section 12). Returns true when the user asks to leave. */
bool receiving_handle(button_event_t event);
/** Stops the playback and the session and turns tap mode off. Draws nothing. */
void receiving_leave(void);
```

```c
/* sending.h. Main loop only; only menu.c calls these functions. */
/** Seeds the RNG, reads the settings, sets the tone and prepares the engine (phase SELECT). No keyer. P3.2 stays with the menu. */
void sending_enter(void);
void sending_render(void);
/** Reads at most KEYER_QUEUE_SIZE + 1 keyer events, runs the engine and redraws the rows that change. */
void sending_poll(void);
bool sending_handle(button_event_t event);
/** In GAP, ITEM and RESULT: stops the keyer and gives P3.2 back. In SELECT and END no keyer runs (D-T40). Then stops the session. Draws nothing. */
void sending_leave(void);
```

### 5.12 `menu/menu.h` changes

```c
#define MENU_SCREEN_LIST 0U
#define MENU_SCREEN_EDIT 1U
/* 2 is unused (D-T8). */
#define MENU_SCREEN_DIALOG 3U
#define MENU_SCREEN_MESSAGE 4U
#define MENU_SCREEN_FREE_INPUT 5U
#define MENU_SCREEN_RECEIVING 6U
#define MENU_SCREEN_SENDING 7U
```

The header comment of `menu_handle()` changes: Free Input acts only on long events. Receiving and Sending get every
event except a long `menu/back` (section 12).

## 6. Level model

Level index `L` is `uint16_t`, 0..65535. Every function takes any `L`.

| Range of `L` | Kind | Learned count `m` | Items | Word length |
| --- | --- | --- | --- | --- |
| `L < KOCH_FINAL_LEVEL`, `L % 3 == 0` | CHAR | `2 + 2 * (L / 3)` | 50 | 0 |
| `L < KOCH_FINAL_LEVEL`, `L % 3 == 1` | CHAR | `3 + 2 * (L / 3)` | 50 | 0 |
| `L < KOCH_FINAL_LEVEL`, `L % 3 == 2` | WORD, word number `L / 3 + 1` | `3 + 2 * (L / 3)` | 10 | `min(2 + (L / 3) / 2, max)` |
| `L >= KOCH_FINAL_LEVEL` | WORD | `KOCH_CHAR_COUNT` | `L - F >= 90 ? 100 : 10 + (L - F)`, `F = KOCH_FINAL_LEVEL` | `max` |

- `max` = `Max Word Length`, clamped to `KOCH_WORD_LEN_MIN..KOCH_WORD_LEN_MAX`.
- The new character of a character level is `koch_char(m - 1)`. At `L = 0` it is M, and the item rule of section 7
  mixes K and M evenly.
- The word length `2 + (L / 3) / 2` equals `2 + (wordNo - 1) / 2` of the plan.
- For `L < KOCH_FINAL_LEVEL` the value fits `uint8_t`, and the code divides in 8 bits (`DIV AB`). From
  `KOCH_FINAL_LEVEL` on, the code compares first, so `L - F` never overflows and no 16-bit division runs.
- `KOCH_FINAL_LEVEL` = 63 for the 43 characters.
- Pass threshold (D-T33), in `uint16_t`. Character kind: `(9 * count + 9) / 10`, values 50 gives 45 (and 10 gives 9,
  100 gives 90 for the arithmetic). Word kind: `(4 * count + 4) / 5`, values 10 gives 8, 11 gives 9, 15 gives 12,
  50 gives 40, 100 gives 80. For any count 0..255 the products are at most 2304 and 1024, so 16 bits never overflow.
  Count 0 gives 0 in both rules.
- Advance: `if (L != 65535) { L = L + 1; }`. No wrap.

Reference values for `KOCH_CHAR_COUNT` = 43:

| `L` | Kind | `m` | New char / word number | Items | Length (max 8) |
| ---: | --- | ---: | --- | ---: | ---: |
| 0 | CHAR | 2 | M (with K) | 50 | - |
| 1 | CHAR | 3 | R | 50 | - |
| 2 | WORD | 3 | word 1 | 10 | 2 |
| 3 | CHAR | 4 | S | 50 | - |
| 5 | WORD | 5 | word 2 | 10 | 2 |
| 8 | WORD | 7 | word 3 | 10 | 3 |
| 59 | WORD | 41 | word 20 | 10 | 8 |
| 60 | CHAR | 42 | SK | 50 | - |
| 61 | CHAR | 43 | AR | 50 | - |
| 62 | WORD | 43 | word 21 | 10 | 8 |
| 63 | WORD (final) | 43 | - | 10 | 8 |
| 64 | WORD (final) | 43 | - | 11 | 8 |
| 152 | WORD (final) | 43 | - | 99 | 8 |
| 153 | WORD (final) | 43 | - | 100 | 8 |
| 65535 | WORD (final) | 43 | - | 100 | 8 |

## 7. Item generation

`koch_make_item(level, max_word_len, text)`, with `m = koch_learned_count(level)`:

- CHAR level: `r = rng_below(2)`. For `r == 0` the item is `koch_char(m - 1)`. Else the item is
  `koch_char(rng_below(m - 1))`. Length 1. So the new character has a share of 1/2, and the older characters share
  the other half evenly. At `L = 0` this gives K and M with 1/2 each.
- WORD level: `n = koch_alphabet_count(level)` = `min(m, KOCH_WORD_ALPHABET_MAX)`,
  `len = koch_word_length(level, max_word_len)`. For each position
  in order, the character is `koch_char(rng_below(n))`.
- Then `text[len] = '\0'`. The return value is `len`.

The call order of `rng_below()` is part of the contract, so tests can predict items from a seed.

Seeding: `receiving_enter()` and `sending_enter()` call `rng_seed(tick_now())`. Each accepted button event in the
modes and each keyer DIT or DAH event in Sending calls `rng_mix(tick_now())`. On the selector screens (D-T38), only
the session start (`TRAINING_SELECT_START`) mixes. A level step does not mix.

## 8. Group keyboard

List: `keyboard_reset(a)` copies each `koch_keyboard_char(k)`, `k` = 0..`KOCH_CHAR_COUNT - 1`, whose Koch index
(`koch_index_of()`) is below `a`, into `keyboard_list[KOCH_CHAR_COUNT]`. The list is in keyboard order. `a` is
`training_alphabet_count()`: all learned characters in a character session, at most the first 40 Koch characters in
a word session (D-T4). Example: a word session with all 43 characters learned has 40 characters.

Range: `range_first` and `range_count`. After a reset or a pick: `0` and `keyboard_count()`.

Group sizes of a range of `c` characters (four runs, D-T27):

- Run `k` = 0..3 (`KEYBOARD_GROUP_MENU`, `_PREV`, `_NEXT`, `_ENTER`) has `s_k = (c + 3 - k) / 4` characters. The
  runs are contiguous from `range_first` in this order. `s0 >= s1 >= s2 >= s3 >= s0 - 1`, and the sum is `c`.
- Results: 2 gives 1/1/0/0, 3 gives 1/1/1/0, 4 gives 1/1/1/1, 5 gives 2/1/1/1, 9 gives 3/2/2/2, 13 gives 4/3/3/3,
  17 gives 5/4/4/4, 40 gives 10/10/10/10, 43 gives 11/11/11/10.
- Selects per pick: 1 for 2..4 characters, at most 2 for 5..16, at most 3 for 17..64.
- Example, character session with all 43 characters: A to K, L to V, W to 6, then `7 8 9 . , ? /` BT SK AR. The
  right run of 10 gives 3/3/2/2: `789`, `.,?`, `/` BT, SK AR. Word session with 40 characters: A to J, K to T,
  U to 3, 4 to `/`.
- Empty runs are on the right. With 2 or 3 characters, `enter` (and `next` for 2) selects nothing.
- `keyboard_group_first(g)` for `g >= KEYBOARD_GROUP_COUNT` returns `range_first + range_count` (D-T21 unchanged).

`keyboard_select(g)`:

1. Size 0 or `g >= KEYBOARD_GROUP_COUNT`: no change, return `KEYBOARD_NO_PICK`.
2. Size 1: return that character, then `keyboard_top()`.
3. Else push `(range_first, range_count)` on the history (depth + 1), set the range to the group, return
   `KEYBOARD_NO_PICK`.

`keyboard_back()`: with depth > 0, pop the range and return true. Else return false.

History: `keyboard_history_first[KEYBOARD_DEPTH_MAX]` and `keyboard_history_count[KEYBOARD_DEPTH_MAX]`. A
`_Static_assert` proves that `KEYBOARD_DEPTH_MAX` covers `KOCH_CHAR_COUNT` (four runs: 4^3 = 64 >= 43). The last
select picks, so at most `KEYBOARD_DEPTH_MAX - 1` = 2 narrowings go on the history. The arrays can have
`KEYBOARD_DEPTH_MAX - 1` entries. `keyboard_sizes[KEYBOARD_GROUP_COUNT]` grows to 4 entries.

## 9. CW player ISR design

### 9.1 `player_start()` (main loop)

1. `player_stop()`. From here the ISR returns at once for the player, so the shared values can change.
2. Reject `length == 0` or `length > PLAYER_TEXT_MAX`: return false.
3. For each character: `pattern = morse_encode(text[i])`. `MORSE_PATTERN_INVALID`: return false (the player stays
   stopped). Else write the play form (section 5.4) to `player_codes[i]`.
4. `cw_timing_compute(wpm, eff_wpm)`. Write `player_dit_ticks = dit`, `player_dah_ticks = 3 * dit`,
   `player_char_gap_ticks = 3 * unit`. Write `player_code_count = length`.
5. Set `player_is_start_requested` (one `SETB`), then `player_is_enabled` (one `SETB`). All shared values are
   `volatile`, so C keeps the stores of steps 3 and 4 before step 5.
6. Return true.

`player_stop()`: clear `player_is_enabled`, clear `player_is_start_requested`, then `sidetone_set_on(false)`.

`player_is_busy()`: return `player_is_enabled`.

### 9.2 `player_service()` (ISR, one call)

```
if player_is_enabled == 0: return                       (menu path: one bit test)
if player_is_start_requested:
    clear player_is_start_requested
    player_index = 0; player_code = player_codes[0]
    start mark; return
player_ticks = player_ticks - 1
if player_is_mark:
    if player_ticks == 0:                               (mark end)
        player_code = player_code >> 1
        if player_code != 1:  player_is_mark = 0; player_ticks = player_dit_ticks          (element gap)
        else:
            player_index = player_index + 1
            if player_index == player_code_count:  clear player_is_enabled; return        (done)
            player_code = player_codes[player_index]
            player_is_mark = 0; player_ticks = player_char_gap_ticks                       (character gap)
    else if player_ticks <= PLAYER_EARLY_RELEASE_TICKS:  PLAYER_SIDETONE_OFF()
else if player_ticks == 0:  start mark                   (gap end)

start mark: player_is_mark = 1; player_ticks = (player_code & 1) ? player_dah_ticks : player_dit_ticks;
            PLAYER_SIDETONE_ON()
```

Countdown rule (the same as section 6.4 of `keyer_spec.md`): the call that starts a mark or a gap loads N. The phase
lasts exactly N calls, the start call included. A mark that starts on call `s` covers calls `s`..`s + N - 1`. The
gap after it covers calls `s + N`..`s + N + G - 1`, and the next mark starts on call `s + N + G`.

Exact lengths, in calls:

| Part | Length |
| --- | --- |
| Dit mark | `dit` |
| Dah mark | `3 * dit` |
| Gap between elements of one character | `dit` |
| Gap between characters | `3 * unit`. Without Farnsworth, `unit` equals `dit`. |
| After the last mark | none: `player_is_enabled` clears on the call after the last mark call |

Early release: the request goes off on the call where `player_ticks` becomes 32. The sidetone runs after the player
in the same ISR call, so the 32-step fall writes compare 0 on the last mark call. The mark keeps its full N calls.
The shortest mark is 96 calls (dit at 50 WPM), longer than two 32-step ramps (static check in `timing.h`).

Reference values:

| WPM / eff | dit | dah | element gap | character gap |
| --- | ---: | ---: | ---: | ---: |
| 5 / 5 | 960 | 2880 | 960 | 2880 |
| 20 / 20 | 240 | 720 | 240 | 720 |
| 20 / 10 | 240 | 720 | 240 | 2616 |
| 20 / 5 | 240 | 720 | 240 | 6405 |
| 25 / 15 | 192 | 576 | 192 | 1587 |
| 50 / 50 | 96 | 288 | 96 | 288 |
| 50 / 5 | 96 | 288 | 96 | 7110 |

All values fit `uint16_t`. The ISR only compares, counts, shifts and copies. It never multiplies or divides.

### 9.3 Sharing the sidetone

- The keyer and the player write the same request bit through their macros. D-T12 keeps only one of them enabled.
- The Tone preview runs only in the Tone edit. No mode screen runs at that time.
- The tone: `receiving_enter()` and `sending_enter()` call `sidetone_set_tone_hz(settings_get(SETTINGS_ID_TONE_HZ))`.
- A stop during a mark gives the normal 8 ms fall, so no click.

## 10. EEPROM progress record (page 1)

### 10.1 Layout

| Bytes | Content |
| --- | --- |
| 0 | `PROGRESS_MAGIC_0` (0x4B, 'K') |
| 1 | `PROGRESS_MAGIC_1` (0x50, 'P') |
| 2 | `PROGRESS_LAYOUT_VERSION` (1) |
| 3..94 | Receiving levels, WPM 5..50: level of `wpm` at `3 + 2 * (wpm - 5)`, low byte first |
| 95..186 | Sending levels, WPM 5..50: level of `wpm` at `95 + 2 * (wpm - 5)`, low byte first |
| 187 | CRC-8 (`crc8_compute`) of bytes 0..186 |

Total 188 bytes at IAP address 0x0200. Bytes 188..511 of page 1 stay erased. The code reads and writes the levels
byte by byte. It never reads a `uint16_t` through a cast pointer (alignment on the host).

### 10.2 Load (`progress_load`)

Read the 188 bytes into `progress_record` (xdata). Check in this order: read status, magic, version, CRC. The first
failure gives its status: `PROGRESS_READ_FAILED`, `PROGRESS_NO_RECORD`, `PROGRESS_BAD_VERSION` or
`PROGRESS_BAD_CRC`. On any failure every level byte becomes 0. Then, in all cases, bytes 0..2 get the magic and the
version. Load never writes the EEPROM.

### 10.3 Save (`progress_save`)

1. `eeprom_clear_low_supply()`.
2. Byte 187 = CRC of bytes 0..186.
3. `eeprom_erase_page(EEPROM_MAP_PROGRESS_PAGE)`.
4. `eeprom_write(0x0202, &progress_record[2], 186)`.
5. `eeprom_write(0x0200, progress_record, 2)`. The magic goes last, so an interrupted save never leaves a valid
   record.
6. Any status other than `EEPROM_OK` stops the sequence and returns `PROGRESS_SAVE_FAILED`. The levels in RAM stay.

### 10.4 Rules

- The engine saves only after a passed session that changed a level (section 11.4). Only a pass at the earned level
  changes a level (D-T36). A pass at another level calls neither `progress_advance()` nor `progress_save()`.
- `progress_reset()` sets every level byte to 0, then saves. Also with all levels already at 0, it saves.
- A failed save loses the stored record, because the erase came first. The next start then loads level 0 everywhere.
  Risk R-T1.
- An unknown mode returns level 0 and never advances. A WPM outside 5..50 is clamped.

## 11. Session engine

### 11.1 State (all `static MEM_XDATA` in `training.c`)

- Session: phase, mode, wpm, kind, level, learned count, max word length.
- Selection: the selected level (`uint16_t`, D-T36). The earned level has no copy.
- Counters: item number, item count, score, pass threshold, wrong attempts, last check, outcome.
- Time: phase start tick (`uint16_t`), phase duration (`uint16_t`), result hold flag (`bool`, D-T14, D-T44).
- Texts: item text and answer text (`TRAINING_TEXT_SIZE` each), item length, answer length.

### 11.2 Transitions

| From | Trigger | Actions | To | Event |
| --- | --- | --- | --- | --- |
| any | `training_prepare(mode, wpm, max)` | Store the mode (unknown gives RECEIVING), wpm and max. Selected level = `progress_level(mode, wpm)` (D-T36). | SELECT | - |
| SELECT | `training_select_step(is_up)` | Up with selected < earned: selected + 1. Down with selected > 0: selected - 1. Else no change. A change returns true, no change returns false. END no longer steps (D-T46). | same | - |
| END | `training_reopen_select()` | Selected level unchanged (the preselection of 11.4). No tick read, no progress access. Returns true (D-T46). | SELECT | - |
| any | `training_start()` | Level = selected level. Set kind, learned count, item count, threshold (`koch_pass_threshold(koch_kind(level), item count)`, D-T33). Item number 0, score 0. Empty answer. Duration `TRAINING_START_MS`. | GAP | - |
| any | `training_stop()` | - | IDLE | - |
| GAP | duration passed (poll) | Item number + 1. Wrong attempts 0. `koch_make_item()`. Empty answer. | ITEM | ITEM |
| ITEM | `training_add_char()` gives CORRECT | First attempt (wrong attempts 0): score + 1. Duration `TRAINING_CORRECT_MS`. | RESULT | - |
| ITEM | gives WRONG | Wrong attempts + 1 (now 1 or 2). Duration `TRAINING_WRONG_MS`. | RESULT | - |
| ITEM | gives FAILED | Wrong attempts = 3. Duration `TRAINING_FAILED_MS`. Receiving sets the hold at once (12.2), so only Sending uses this delay. | RESULT | - |
| RESULT | `training_hold_result(true)` | Hold on. The delay does not run. | RESULT | - |
| RESULT | `training_hold_result(false)` | Hold off. Duration 0. The phase start does not change. So the next poll ends the RESULT by the three rows below (D-T44). | RESULT | - |
| RESULT | duration passed with the hold off, last check WRONG | Empty answer. | ITEM | RETRY |
| RESULT | duration passed with the hold off, CORRECT or FAILED, item number < item count | Duration `TRAINING_RECEIVING_GAP_MS` or `TRAINING_SENDING_GAP_MS` by mode. | GAP | GAP |
| RESULT | duration passed with the hold off, CORRECT or FAILED, last item | Finish (11.4). No GAP after the last item. | END | END |
| SELECT, END, IDLE | poll | - | - | NONE |
| END, GAP, ITEM, RESULT, IDLE | `training_select_step()` | No change. Returns false. | same | - |
| IDLE, SELECT, GAP, ITEM, RESULT | `training_reopen_select()` | No change of phase, timing, hold or selection. Returns false. | same | - |

Time: `training_poll()` reads `tick_now()` once and compares `(uint16_t)(now - phase start) >= duration`. Each
phase change sets the phase start to the tick of the call that makes the change. So `training_start()` and a
`training_add_char()` that gives CORRECT, WRONG or FAILED also read the tick. Each phase change clears the hold.

### 11.3 `training_add_char(c)`

1. Phase not ITEM: return IGNORED. Nothing changes.
2. Append `c` to the answer (the answer is shorter than the item in phase ITEM).
3. Sending: compare `c` with the item character at the same position. A mismatch is a wrong attempt. A match with the
   answer still shorter than the item gives PARTIAL. A match that completes the item gives CORRECT.
   `TRAINING_CHAR_WORD_SPACE` never matches, so it always gives a wrong attempt (D-T31). The answer then ends with it.
4. Receiving: with the answer shorter than the item, return PARTIAL. At full length, compare the whole answer. Equal
   gives CORRECT, else a wrong attempt.
5. A wrong attempt gives WRONG for wrong attempts 1 and 2, and FAILED for the third.
6. CORRECT, WRONG and FAILED change the phase as in 11.2 and set the last check.

`training_undo_char()`: in phase ITEM with a non-empty answer, remove the last character and return true. Else false.

### 11.4 Finish

Revised by D-T36:

1. `passed = score >= pass threshold`.
2. Not passed: outcome FAIL. The stored level stays. Selected level = session level.
3. Passed, and the session level is not equal to `progress_level(mode, wpm)`: outcome PASS_REPLAY. No
   `progress_advance()`, no `progress_save()`. Selected level = `koch_shown_level(session level)` (level + 1,
   saturated).
4. Passed and `progress_advance(mode, wpm)` returns false (level 65535): outcome PASS_TOP. No save. Selected level =
   65535.
5. Passed and changed: `progress_save()`. OK gives PASS. SAVE_FAILED gives PASS_NOT_SAVED. The new level stays in
   RAM in both cases. Selected level = session level + 1, which equals the new earned level.
6. In steps 3 to 5 the selected level is `koch_shown_level(session level)`, so one assignment covers all passes.
   Then `training_new_shown_level()` = `koch_shown_level(selected level)`.

Example (D-T35): earned index 19 (L20), the user selects index 4 (L5) and passes. The outcome is PASS_REPLAY. The
EEPROM gets no erase and no write. The earned level stays 19. The selected level becomes 5 (L6), and
`training_select_step(true)` still reaches 19.

The save stops the CPU for the erase (4-6 ms). The keyer and the player are idle or near their end at that time. The
tick loses at most the stopped time, as with a settings save.

## 12. Modes and menu integration

### 12.1 Menu routing

| Screen | Event | Action |
| --- | --- | --- |
| LIST, EDIT, DIALOG, MESSAGE | as today | As today. DIALOG Yes uses the result of `stats_reset()` (D-T11). |
| FREE_INPUT | as today | as today |
| RECEIVING | long `menu/back` | `receiving_leave()`, then LIST |
| RECEIVING | every other event | `receiving_handle(event)`. A true result gives `receiving_leave()`, then LIST. |
| SENDING | long `menu/back` | `sending_leave()`, then LIST |
| SENDING | every other event | `sending_handle(event)`. A true result gives `sending_leave()`, then LIST. |

- `menu_enter_item()`: mode Receiving shows MENU_SCREEN_RECEIVING and calls `receiving_enter()`. Sending likewise.
- `menu_render()`: after the full clear, RECEIVING calls `receiving_render()` and SENDING calls `sending_render()`.
- `menu_poll()`: with no redraw pending, RECEIVING calls `receiving_poll()` and SENDING calls `sending_poll()`.
- `menu_show_message()`: on a mode screen it calls the leave function of that mode first. It keeps the menu stack
  (D-T19).
- A short or long `menu/back` that leaves a training screen shows the main menu with the cursor on the mode item. The
  stack did not change on entry, so LIST shows it.
- After the first `menu_render()`, the modes draw directly into `lcd_fb`. They redraw only the rows that change.

### 12.2 Receiving (`ui-ux.md` sections 5.12, 5.14, 6.8)

Entry (`receiving_enter()`), in this order (revised by D-T39): `rng_seed(tick_now())`. Read `RX Speed`,
`RX Eff. Speed`, `Tone`, `Max Word Length` and `Prosigns` once into xdata (UX-49). `sidetone_set_tone_hz()`.
`training_prepare(TRAINING_MODE_RECEIVING, rx_wpm, max_word_len)`. No tap mode call, no session, no player, no
keyboard reset. `menu_render()` then calls `receiving_render()`, which draws the start screen.

Session start (`rx_start_session()`, from SELECT or END), in this order: `buttons_set_tap_mode(true)`,
`training_start()`, `keyboard_reset(training_alphabet_count())`. The caller then clears rows 1-7 and draws the
session rows 1-7 (status, item line, labels, key bars). From END it also redraws the header. From SELECT it does not
touch row 0: the start screen header already equals the header of the start pause (`ui-ux.md` 5.11.1 and the page
budget of 5.16.4).

Revised by D-T47: a session starts only from SELECT. So the caller never redraws row 0 at the start, and
`receiving_handle()` loses its `if (phase == TRAINING_PHASE_END)` header branch. On the end screen, an `enter` press
opens the start screen inside `training_screen_select_input()`, which returns NONE.

Tap mode (D-T39) is on in GAP, ITEM and RESULT only. In these phases `menu/back`, `prev`, `next` and `enter`
(button or tip) arrive at the release (D-T7, D-T28). On the start screen and on the end screen tap mode is off.
There `menu/back` and `enter` act at the press, and `prev` and `next` give press and repeat events. The `enter`
press that opens the mode gives no event at its release, because tap mode is off. The `enter` press that starts a
session gives no event at its release, because `buttons_set_tap_mode(true)` drops its long-pending state.

`receiving_render()`: SELECT draws the header and `training_screen_draw_start()`. END draws the header and
`training_screen_draw_end()`. Other phases as before.

| Phase | Event | Action |
| --- | --- | --- |
| SELECT, END | any event | `training_screen_select_input(event)`. START: `rng_mix(tick_now())`, `rx_start_session()`, then the redraw of the session start above. LEAVE: return true. NONE: return false. The repeat filter of the other phases does not apply here. START comes only in SELECT (D-T47). |
| ITEM | `menu/back`, `prev`, `next`, `enter` (button or tip) | `keyboard_select()` of `KEYBOARD_GROUP_MENU`, `_PREV`, `_NEXT`, `_ENTER`. A pick goes to `training_add_char()`. Redraw the labels and the item line. An empty run: ignored, no redraw. |
| ITEM | long `enter` (button or tip) | Replay: `player_start()` of the item. A running playback starts again. The picks and the current run stay. |
| ITEM | long `prev` | Undo one step: first `keyboard_back()`. If it returns false, `training_undo_char()`. Redraw. |
| RESULT, answer state (D-T44) | `enter` tap (button or short tip closure) | `player_stop()`, then `training_hold_result(false)`. No redraw: the next poll gives GAP or END. Allowed also during the answer playback. |
| RESULT, answer state (D-T44) | long `enter` (button or tip closure of 800 ms) | `player_start()` of the item. A running playback starts again. No redraw. |
| RESULT, answer state (D-T44) | `menu/back`, `prev`, `next` taps, long `prev` | Ignored. No redraw. |
| GAP, RESULT other than the answer state | any | Ignored. No redraw. |
| GAP, ITEM, RESULT | long `next`, repeats | Ignored. |

Button to run, left to right: MENU -> 0, PREV -> 1, NEXT -> 2, ENTER -> 3. `BUTTON_EVENT_MENU - code` does not
give the run for ENTER. `receiving.c` maps the code in any small form (a `MEM_CODE` table, a switch, or
`(uint8_t)(BUTTON_EVENT_MENU - code) & 3U`). A compile-time check (`#if` or `_Static_assert`) proves the mapping for
all four codes.

Labels of the four runs (D-T27). `ui-ux-embedded-designer` owns the field positions, the alignment and the label text
rule (task D1). The rule must stay inside these limits:

| ID | Limit |
| --- | --- |
| LC-1 | One label per run, left to right in the button order. An empty run has a blank label. |
| LC-2 | Labels use the 5 x 8 font in normal video, in row 7. If no one-row rule fits, a second label row is allowed in row 6 only. Then the key bar line moves, and rows 0-5 do not change. |
| LC-3 | A field has a fixed position and width per run (compile-time constants). A field can start at any pixel x (`gfx_draw_text()` takes a pixel x). Pixels with x = 0, 1 and 127 stay off (`ui-ux.md` V-121). |
| LC-4 | `receiving.c` builds the label text at run time. The inputs are `keyboard_group_first()`, `keyboard_group_size()` and `keyboard_char()`. The text comes from `morse_format()` in an existing style (`MORSE_STYLE_PUNCTUATION`, `_NAME`, `_BOTH`). It can derive a shorter text from that result (for example, drop `<` and `>`, or keep only the first text). No new `morse_format()` style and no label table in flash. A new style needs an `architector` decision, because `keyer_spec.md` section 17 owns `morse.h`. |
| LC-5 | A label is at most its field width and at most `TRAINING_SCREEN_COLUMNS` cells (it is built in `training_screen_text`). |
| LC-6 | The rule fits every run of every Koch prefix (2..43 characters, character and word sessions), for all three `Prosigns` values. D1 gives the script check result. Known hard cases: `7-<AR>` has 6 cells (whole set). For the run SK AR, the list `<SK><AR>` has 8 cells and the range `<SK>-<AR>` has 9. The full texts `<BT/=>` and `<AR/+>` have 6 cells. |
| LC-7 | A label redraw writes only the label rows (row 7, and row 6 under LC-2). The key bar segments are drawn only in a full render (D-T29). |

Each acted event calls `rng_mix(tick_now())`. Tap mode gives no repeat events.

Check results (the return value of `training_add_char()`):

| Result | Action |
| --- | --- |
| PARTIAL | Redraw the item line: the block moves to the next slot. |
| CORRECT, WRONG | `player_stop()`. Status and item line of `ui-ux.md` 5.14. |
| FAILED | `player_stop()`. Status `Wrong. The answer is:`, the item text on the item line. Row 7 of the answer state (D-T44 (5)). `player_start()` of the item. `training_hold_result(true)`. The hold stays on until an `enter` tap. |

Answer state (D-T44): phase RESULT with `training_last_check() == TRAINING_CHECK_FAILED`. The helper
`rx_is_answer_shown()` gives it. `rx_draw_item_line()` and `rx_draw_labels()` use the helper. In the answer state,
`rx_draw_labels()` clears row 7 and draws only the `enter` field: `Next`, or `Finish` on the last item.
`receiving_handle()` sends events in the answer state to `rx_handle_answer()`, after the repeat filter. As
`rx_handle_item()`, it returns true for an acted event. For such an event, the caller calls `rng_mix()`.

Poll (`receiving_poll()`): `training_poll()`, then the reaction to its event. `receiving_poll()` does not call
`training_hold_result()`.

| Event | Action |
| --- | --- |
| ITEM | Empty slots, block on the first slot, labels of the whole set, header number. `player_start()` of the item. |
| RETRY | Status `Try 2 of 3` or `Try 3 of 3`. Empty slots, block on the first slot, labels of the whole set (`keyboard_top()`). `player_start()` of the item. |
| GAP | Blank moment: rows 1-5 off, header shows `training_shown_item_number()`. `rx_draw_labels()` draws the labels of the whole set in row 7, because row 7 can hold `Next` (D-T44 (6)). A pick already put the keyboard at the top. The key bar segments stay. |
| END | `buttons_set_tap_mode(false)` (D-T39), then `training_screen_draw_end()` and the header with `i` = `n`. A button held at this call gives no event until it is released and pressed again. |

### 12.3 Sending (`ui-ux.md` sections 5.13, 5.14, 6.9)

Entry (`sending_enter()`), in this order: `rng_seed(tick_now())`. Read `Key Type`, `Paddle`, `TX Speed`,
`TX Eff. Speed`, `Tone`, `Max Word Length`, `Show CW Code` and `Prosigns` once into xdata (UX-49).
`sidetone_set_tone_hz()`. `training_prepare(TRAINING_MODE_SENDING, tx_wpm, max_word_len)`. Tap mode stays off.
Revised by D-T40: no `buttons_set_enter_enabled()` call and no `keyer_start()` at the entry. P3.2 stays with the
menu, so `enter` and the paddle tip act on the start screen.

Session start (`tx_start_session()`, from SELECT or END), in this order: `buttons_set_enter_enabled(false)`,
`keyer_start()` with the stored values, clear the drop flag, the armed flag and the pattern, `training_start()`. The
caller then clears rows 1-7 and draws the session rows 1-7. From END it also redraws the header. From SELECT it does
not touch row 0, as in Receiving.

Revised by D-T47: a session starts only from SELECT, as in Receiving. `sending_handle()` loses its
`if (phase == TRAINING_PHASE_END)` header branch.

The keyer owns P3.2 during the session, so `enter` never comes then. The keyer runs in GAP, ITEM and RESULT only.

`sending_render()`: SELECT draws the header and `training_screen_draw_start()`. END draws the header and
`training_screen_draw_end()`. Other phases as before.

`sending_poll()` needs no SELECT branch. `training_poll()` gives NONE in SELECT. No keyer runs, and any keyer event
left in the queue is dropped by the phase rules below. `tx_start_session()` clears the flags.

| Phase | Event | Action |
| --- | --- | --- |
| SELECT, END | any event | `training_screen_select_input(event)`. START: `rng_mix(tick_now())`, `tx_start_session()`, then the redraw of the session start above. LEAVE: return true. NONE: return false. START comes only in SELECT (D-T47). |
| GAP, ITEM, RESULT | short events, repeats | Ignored. No redraw. |
| GAP, ITEM, RESULT | long `prev`, long `next` | Ignored. |

`sending_leave()` (D-T40): in GAP, ITEM and RESULT, `keyer_stop()` and `buttons_set_enter_enabled(true)`. In SELECT
and END, no keyer call and no buttons call. Then `training_stop()` in all phases.

Mode state (`static MEM_XDATA`): the pattern, the drop flag `sending_is_dropping` (D-T15), the armed flag
`tx_is_word_gap_armed` (D-T31), the code-row mark count (0..`TX_CODE_MAX_MARKS`) and the code-row x position.

`#define TX_CODE_MAX_MARKS 12U` (`ui-ux.md` 6.10), in `sending.c`.

Keyer events in `sending_poll()`, at most `KEYER_QUEUE_SIZE + 1` per call, in queue order. The drop flag comes first.
Every event that the mode reads clears the armed flag, in every row. Only the CHAR_GAP row with the result PARTIAL
sets it again. The WORD_GAP row reads the value that the flag had before this event.

| Drop flag | Phase | Event | Action |
| --- | --- | --- | --- |
| set | any | CHAR_GAP | Clear the flag. Nothing else. |
| set | any | other | Dropped. |
| clear | not ITEM | DIT, DAH | Set the flag. Dropped. No mark. |
| clear | not ITEM | other | Dropped. |
| clear | ITEM | DIT, DAH | `rng_mix(tick_now())`. On the first element of a character, clear row 7. `morse_add_element()`. With `Show CW Code` on and fewer than 12 marks: draw the mark with `training_screen_draw_code_mark()` (D-T47). |
| clear | ITEM | CHAR_GAP | Empty pattern: nothing. Else `training_add_char(morse_decode(pattern))`, then the pattern is empty. React to the result (below). PARTIAL sets the armed flag. |
| clear | ITEM | OVERFLOW | Empty the pattern, set the drop flag (D-T16). No check, no attempt, no status. |
| clear | ITEM | WORD_GAP | Armed flag was set: `training_add_char(TRAINING_CHAR_WORD_SPACE)` and react to the result (WRONG or FAILED, below). Armed flag was clear: nothing (D-T30). |

A pattern with more than 6 elements decodes as unknown (`*`), so it is a wrong character.

Check results:

| Result | Action |
| --- | --- |
| PARTIAL | Move the bar to the next character of the word. |
| CORRECT | Status `Correct`. No bar. |
| WRONG | Keyed character: status `Wrong: ` and the `morse_format()` text of the keyed character. Word space (the last answer character is `TRAINING_CHAR_WORD_SPACE`): status `Wrong: pause too long` (`ui-ux.md` 5.14, UX-52), a `MEM_CODE` constant. Row 7 keeps its marks (the last correct character). The bar stays under the failed position. |
| FAILED | As WRONG. Row 5 shows the centered marks of the expected character (`morse_encode()` of the item character at the failed position), for every `Show CW Code` value. The draw is `training_screen_draw_code_group(pattern, MORSE_PATTERN_EMPTY)` (D-T47). For a word space, the failed position is the character that the user had to key next. |

`training_poll()` event reactions:

| Event | Action |
| --- | --- |
| ITEM | Draw the item in the 8 x 16 font. Word item: bar under the first character. |
| RETRY | Status `Try 2 of 3` or `Try 3 of 3`. Bar back to the first character. Rows 5 and 7 off (D-T30), as at ITEM. The next mark starts at x 2. |
| GAP | Blank moment: rows 1-7 off, header shows `training_shown_item_number()`. |
| END | `keyer_stop()`, `buttons_set_enter_enabled(true)`, `buttons_set_tap_mode(false)` (D-T43: drops a held `prev` or `next` and its repeat), `training_screen_draw_end()`. |

Each 8 x 16 draw selects `FONTS_ID_8X16` and selects `FONTS_ID_5X8` again before it returns.

Word-space edge cases (D-T30, D-T31). The keyer gives CHAR_GAP at 2 dits and WORD_GAP at 5 Farnsworth units after
the end of the last mark, once per space, and a new mark before the threshold cancels both (AC-K15). So the user must
start the next character of a word within 5 units of the end of the last mark. The standard space is 3 units.

| Case | Result |
| --- | --- |
| WORD_GAP before the first character of the item, also at a retry | Armed flag clear: nothing. The keyer gives no gap event before the first mark after a start. |
| WORD_GAP after the last character of the item | That CHAR_GAP gave CORRECT, WRONG or FAILED, so the phase is RESULT: dropped. |
| WORD_GAP after a wrong character | The phase is RESULT: dropped. |
| WORD_GAP in the start pause, RESULT or GAP | Dropped (phase not ITEM), also with the armed flag set. |
| CHAR_GAP (PARTIAL) and WORD_GAP in the same poll | Read in order: wrong attempt. |
| OVERFLOW, then WORD_GAP | OVERFLOW clears the armed flag. With the drop flag still set, the WORD_GAP is dropped. If a CHAR_GAP came first and cleared the drop flag, the armed flag is still clear: nothing. No attempt either way (D-T16). |
| A character dropped by D-T15, then WORD_GAP | The dropped events clear the armed flag: nothing. |
| DIT or DAH after the PARTIAL CHAR_GAP, then WORD_GAP | The real keyer cannot give this order. The DIT clears the armed flag: nothing. |
| Lost WORD_GAP (queue overflow) | OVERFLOW rule (D-T16): no attempt. |
| Late WORD_GAP of an earlier space | At a low `TX Eff. Speed`, 5 units can be longer than the delays (20/5 WPM: about 2669 ms, more than 600 + 300 ms or 1200 ms). The WORD_GAP of the space after the last character of an item, or after a wrong character, then comes in ITEM of the next item or of the retry. The CHAR_GAP of that space did not give PARTIAL, so the armed flag is clear: nothing. A mark before it cancels it. |
| Straight key | The keyer counts the space from the key release in the same way (AC-K15 covers straight and iambic). Same rule. |
| Farnsworth | The threshold is 5 units at `TX Eff. Speed`. 20/20 WPM: 1200 ticks, 300 ms after the mark end. 20/10 WPM: unit 872 ticks, threshold 4360 ticks, about 1090 ms. 50/50 WPM: 480 ticks, 120 ms. A lower effective speed gives more room. |
| Character session | The item is complete after one character, so no WORD_GAP is armed in ITEM. |

Limits for the word-space look. `ui-ux-embedded-designer` owns the text and the look (task D2), inside these limits:

| ID | Limit |
| --- | --- |
| WS-1 | The status text is in row 1 in the 5 x 8 font, centered like every status text, at most `TRAINING_SCREEN_COLUMNS` (25) cells, ASCII 0x20-0x7E only (`ui-ux.md` V-121). |
| WS-2 | The mode builds the text at run time from `training_answer_text()` (the accepted characters, then `TRAINING_CHAR_WORD_SPACE`) and `training_item_text()`, with `morse_format()` in the session `Prosigns` style, or uses a fixed text. No new `morse_format()` style. Word items hold no prosign (D-T4), so each word character is one cell. The worst case is 7 accepted characters. D2 result: the fixed text `Wrong: pause too long` (21 cells, column 2), chosen when the last answer character is `TRAINING_CHAR_WORD_SPACE`. |
| WS-3 | No new delay and no sound: `TRAINING_WRONG_MS` and `TRAINING_FAILED_MS` as for a wrong character. |
| WS-4 | The item line does not change: the same 8 x 16 text, and the bar under the failed position (the character expected next). A new glyph or another bar position on the item line needs an `architector` decision. |
| WS-5 | FAILED: row 5 shows the code of the expected next character (the rule above). During RESULT, row 7 keeps the marks of the last accepted character, unless D2 asks for row 7 off at a word-space error. D2 result: row 7 keeps the marks. |
| WS-6 | The retry screen is the ITEM screen plus the `Try 2 of 3` or `Try 3 of 3` status, for every kind of wrong attempt and every `Show CW Code` value (D-T30). |

### 12.4 `main.c`

`(void)progress_load();` right after `(void)settings_load();`. No other change.

### 12.5 Main menu level (D-T41)

`draw_item_row()` in `src/menu/menu_render.c`, for an item with `kind == MENU_KIND_MODE`:

| Target | Text |
| --- | --- |
| `MENU_MODE_RECEIVING` | `L` and `koch_shown_level(progress_level(PROGRESS_MODE_RECEIVING, settings_get(SETTINGS_ID_RX_WPM)))` |
| `MENU_MODE_SENDING` | `L` and `koch_shown_level(progress_level(PROGRESS_MODE_SENDING, settings_get(SETTINGS_ID_TX_WPM)))` |
| `MENU_MODE_FREE_INPUT` | nothing |

- The text has 2 to 6 cells (`L1` to `L65535`). Column and alignment: `ui-ux.md` (task D4). The label column 0 and
  the longest label `Free Input` (10 cells) leave room up to column 24.
- Color: the color of the label. On the selected row it is inverted inside the band.
- The text is built in the existing `value_text` buffer (12 bytes) with `gfx_format_u16()`. No new xdata.
- The level is read at each render. A session, a speed change or `Reset Statistics` shows at the next render of
  the main menu with no extra code.
- The new draw path is no deeper than the existing `draw_value()` path (`draw_item_row()` -> helper ->
  `draw_text()` -> `gfx_draw_text()`).
- A compile-time check (`#if` or `_Static_assert`) proves `MENU_MODE_RECEIVING == PROGRESS_MODE_RECEIVING` and
  `MENU_MODE_SENDING == PROGRESS_MODE_SENDING`. Then the target can go to `progress_level()` directly.

### 12.6 End screen, start screen and session start (D-T45 to D-T47)

Flow: END (end screen) -> `enter` -> SELECT (start screen, preselected level) -> `enter` -> GAP (session). The table
gives the button and keyer rules at each switch. The rules hold for the button and for the paddle tip, which is
`enter` while tap mode is off and the keyer does not run.

| ID | Rule |
| --- | --- |
| B-1 | END event (D-T39, D-T43, unchanged). Receiving: `buttons_set_tap_mode(false)`. Sending: `keyer_stop()`, `buttons_set_enter_enabled(true)`, `buttons_set_tap_mode(false)`. A button held from the last RESULT gives no event in END. It also gives no event in the SELECT that follows, until the user releases it and presses it again. |
| B-2 | `enter` in END acts at the press, because tap mode is off. Its release gives nothing. It has no long event (tap mode off) and no repeat (only `prev` and `next` repeat). So one press never opens the start screen and starts a session. The second `enter` must be a new press. |
| B-3 | The switch END to SELECT does not call a `buttons` function. A `prev` or `next` pressed in END does nothing there. If the user holds it and presses `enter`, its later repeats step the selector on the start screen. Accepted: the hold is a new user action after the session. |
| B-4 | Receiving. Tap mode stays off in END and in the SELECT that follows. `rx_start_session()` turns it on and drops the start press (D-T39). So the start press gives no tap at its release and no long `enter`. |
| B-5 | Sending. The keyer does not run in END or in the SELECT that follows, and P3.2 stays `enter` (D-T40). A leave from that SELECT calls no `keyer_stop()` and no `buttons_set_enter_enabled()`. `tx_start_session()` starts the keyer once and disables `enter`. A held contact at that time gives no DIT (keyer entry mask, AC-K3). |
| B-6 | Leave. In END and SELECT, a `menu/back` press gives LEAVE. The long event of the same hold then does nothing on the main menu (AC-T32 (5)). After a leave from END, the next mode entry preselects the earned level (`training_prepare()`), not the preselection of the end screen. |

## 13. ISR rules and `check_asm.sh`

### 13.1 Rules

- `keyer_service()`, `player_service()` and `sidetone_service()` are leaves: no `lcall`, no `acall`, no jump to a
  function, no library helper. All three use `#pragma nooverlay`.
- The ISR has no `P_SW2` access, no `EA` or `IE` byte write and no port write. The player touches no pin.
- The ISR frame does not grow. The three calls are at the same depth.
- ISR time: count the whole Timer 0 ISR from the `.asm` with the STC8H instruction timing. Report three paths: the
  disabled path (menu), the worst keyer path with the player disabled, the worst player path with the keyer
  disabled. Name each path. Budget in section 14.

### 13.2 `check_asm.sh` changes (developer task T5)

A function body starts at its label line (`_name:` at column 0). It ends before the next column-0 label that starts
with `_`, or at the end of the file.

- **R6 (new), ISR call list.**
  - Module `sidetone.c` has the label `_sidetone_timer0_isr:`. Without it, R6 reports a violation (fail closed).
  - The ISR body has exactly three calls (`lcall` or `acall`). The order is `_keyer_service`, `_player_service`,
    `_sidetone_service`.
  - The ISR body has no `ljmp`, `ajmp` or `sjmp` to a label that starts with `_`. It has a `reti`.
  - In any module, the bodies of `_keyer_service`, `_player_service` and `_sidetone_service` are leaves. They have
    no `lcall` and no `acall`. They have no `ljmp`, `ajmp` or `sjmp` to a label that starts with `_`.
  - R6 does not check other modules or other functions.
- **R7 (new), no atomics.** A line of any `.asm` file that contains `sdcc_atomic` (any case) is a violation.
- Update the header comment of the tool with R6 and R7.
- Built-in self-test cases (names binding, text free):
  - `accept_isr_three_calls` (sidetone.c, the three calls in order, `reti`, three leaf services).
  - `r6_isr_two_calls` (no `_player_service` call), `r6_isr_wrong_order`, `r6_isr_extra_call` (a fourth `lcall`),
    `r6_isr_tail_jump` (`ljmp _x`), `r6_isr_missing` (sidetone.c without the ISR label), `r6_service_calls`
    (`_player_service` body with `lcall __mulint`), `r6_keyer_service_calls` (in another module name).
  - `r7_sdcc_atomic` (a line with `sdcc_atomic_maybe_rollback`).
- R7 stays strict (D-T20). The fixture `Tests/tools/check_asm/accept/real_image` comes from commit `7bf1aa7`, before
  `--norestartseqatomics`. Its `main.c.asm` holds `sdcc_atomic_exchange_*` helper code. The developer regenerates
  the whole fixture from the current build: the same seven files (`delay.c.asm`, `lcd.c.asm`, `lcd_fb.c.asm`,
  `lcd_spi.c.asm`, `main.c.asm`, `tick.c.asm`, `tick_timer3.c.asm`), copied unchanged from one build. The commit
  message names the source commit of the copy.
- New accept fixture `Tests/tools/check_asm/accept/real_isr/sidetone.c.asm`: the unchanged `sidetone.c.asm` of the
  same build. It proves R6 and R7 on real SDCC output.
- R6 is fail-closed. So every built-in self-test case with the module name `sidetone.c` also holds the three leaf
  services and the three-call ISR. The R5 meaning of these cases does not change: each `r5_` case still reports R5,
  and each accept case stays clean.
- The firmware build runs the tool as today, so a wrong ISR now fails the build.

## 14. Memory placement and budget

### 14.1 Placement

| Variable | Qualifier | Writer | Readers |
| --- | --- | --- | --- |
| `player_is_enabled` | `volatile MEM_BIT`, `player.c` | Main sets and clears. ISR clears at the end. | both |
| `player_is_start_requested` | `volatile MEM_BIT`, `player.c` | Main sets. ISR clears. | ISR |
| `player_codes[8]`, `player_code_count` | `volatile MEM_XDATA uint8_t`, `player.c` | main, only while disabled | ISR |
| `player_dit_ticks`, `player_dah_ticks`, `player_char_gap_ticks` | `volatile MEM_XDATA uint16_t`, `player.c` | main, only while disabled | ISR |
| `player_is_mark` | `static MEM_BIT`, `player_service.h` | ISR | ISR (tests read it) |
| `player_ticks` | `static MEM_DATA uint16_t`, `player_service.h` | ISR | ISR (tests) |
| `player_code`, `player_index` | `static MEM_DATA uint8_t`, `player_service.h` | ISR | ISR (tests) |
| `cw_timing_dit_ticks`, `cw_timing_unit_ticks` | `MEM_XDATA uint16_t`, `timing.c` | main | main |
| Timing work values | `static MEM_XDATA`, `timing.c` (moved from `keyer.c`) | main | main |
| RNG state | `static MEM_XDATA uint16_t`, `rng.c` | main | main |
| `koch_order`, `koch_keyboard_order` | `static const MEM_CODE char[43]`, `koch.c` | - | main |
| Keyboard list, range, history, depth | `static MEM_XDATA`, `keyboard.c` | main | main |
| `progress_record[188]` | `static MEM_XDATA uint8_t`, `progress.c` | main | main |
| Engine state (11.1) | `static MEM_XDATA`, `training.c` | main | main |
| Mode state: pattern, settings read at entry, text buffers for `morse_format()` and `gfx_format_u16()` | `static MEM_XDATA`, mode files | main | main |
| `buttons_is_tap_mode` | `static MEM_XDATA bool`, `buttons.c` | main | main |
| Screen texts | `static const MEM_CODE char[]` | - | main |

No new main-loop `bool` in `MEM_DATA`. No `bool` for an ISR flag. Every shared access is one bit or one byte, and the
16-bit player values change only while the player is disabled.

### 14.2 Budget

| Resource | Baseline | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- | --- |
| Flash added by the whole change | 19 055 B | at most 6144 B | 8192 B |
| xdata added | 1400 B | at most 384 B (estimate 330 B) | 512 B |
| New static internal RAM: DSEG + overlay + bit bytes | 52 B | at most 6 B (estimate 4 B DSEG, 3 bits) | 8 B |
| Deepest main-loop stack | 65 B | at most 100 B | stack margin rule |
| Stack margin: free stack minus (deepest main-loop use + largest ISR frame) | 112 B | | at least 64 B |
| Timer 0 ISR, disabled path (menu) | not measured | baseline + at most 12 clocks | - |
| Timer 0 ISR, worst path | keyer worst at most 420 clocks (`keyer_spec.md` 8) | at most 440 clocks | 800 clocks |

xdata estimate: player 18, timing 4 net, RNG 2, keyboard 54, progress 188, engine 42, modes 20, buttons 1.

Final (T10): flash 26 593 B (+7538), xdata 1811 B (+411), DSEG 47 B (+4). Stack: 191 B free, deepest main-loop
chain 65 B, ISR frame 18 B, margin 108 B. ISR worst paths: keyer 403, player 186, disabled 63 clocks. D-T23 accepts the
flash and xdata above target.

Change D-T27/D-T28 (task T11). It is small: no new file, no new module.

| Resource | Baseline (after T10) | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- | --- |
| Flash | 26 593 B | at most +300 B | +600 B |
| xdata | 1811 B | at most +4 B (estimate: press time +2, run size +1, history -2) | +8 B |
| DSEG | 47 B | +0 B | +0 B |
| Stack | 191 B free, margin 108 B | no deeper main-loop chain | margin at least 64 B |
| Timer 0 ISR | keyer 403, player 186, disabled 63 clocks | unchanged (no ISR file changes) | - |

T11 measures the baseline again before the first change. It reports these values before and after, with the table
below: flash, xdata, DSEG, overlay, bit bytes, stack start, stack free and the deepest main-loop chain.

Change D-T30/D-T31 (task T12). It is small: no new file, no engine code, no ISR change.

| Resource | Baseline (after T11) | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- | --- |
| Flash | 26 642 B | at most +150 B | +300 B |
| xdata | 1810 B | at most +1 B (`tx_is_word_gap_armed`) | +2 B |
| DSEG, overlay, bit bytes | 47 B DSEG | +0 B | +0 B |
| Stack | 191 B free, margin 108 B | no deeper main-loop chain (the WORD_GAP path is the CHAR_GAP path) | margin at least 64 B |
| Timer 0 ISR | keyer 403, player 186, disabled 63 clocks | unchanged (no ISR file changes) | - |

T12 measures the baseline again before the first change and reports the same values before and after.

Final (T12): flash 26 740 B (+98), xdata 1811 B (+1), DSEG, overlay and bit bytes unchanged, 191 B stack free,
summed-push bound 66 B (+1, D-T32), margin 107 B.

Change D-T33 (task T13). It is small: one function gets a parameter and a second rule.

| Resource | Baseline (after T12) | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- | --- |
| Flash | 26 740 B | at most +40 B | +100 B |
| xdata | 1811 B | at most +1 B (the second parameter `item_count` is `MEM_XDATA` per section 5; `kind` comes in a register) | +2 B |
| DSEG, overlay, bit bytes | unchanged | +0 B | +0 B |
| Stack | summed-push bound 66 B, margin 107 B | no deeper main-loop chain | margin at least 64 B |
| Timer 0 ISR | keyer 403, player 186, disabled 63 clocks | unchanged | - |

T13 measures the baseline again before the first change and reports the 14.2 table before and after.

Final (T13): flash 26 781 B (+41, D-T34), xdata 1812 B (+1), DSEG, overlay and bit bytes unchanged, 191 B stack free,
margin 107 B.

Change D-T35 to D-T41 (task T14): level selection. No new file, no ISR change. Baseline measured by the coordinator
on the unchanged tree (2026-10-07), equal to the final T13 values.

| Resource | Baseline (after T13) | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- | --- |
| Flash | 26 781 B | at most +800 B | +1200 B |
| xdata | 1812 B | at most +2 B (the selected level) | +4 B |
| DSEG, overlay, bit bytes | unchanged | +0 B | +0 B |
| Stack | starts at 0x41, 191 B free, summed-push bound 66 B, margin 107 B | no deeper main-loop chain | margin at least 64 B |
| Timer 0 ISR | keyer 403, player 186, disabled 63 clocks | unchanged: no change in `sidetone.c`, `keyer_service.h`, `player_service.h`, `keyer.c` or `player.c` | - |
| Repeat cost (D-T39) | - | tap switches and repeat acceptance in the modes | +100 B, then the D-T35 (6) fallback decision |

Flash estimate: engine +150 B, selector screens and description +400 B, mode changes +60 B. The main menu level
adds about +100 B. `koch_shown_level()` adds about 0 B net, because it replaces `training_shown_of()`. The texts of the description and the
`Start` band are short `MEM_CODE` constants.

Stack candidates for T14, to compare with the 66 B bound:

- `menu_handle` -> `receiving_handle` -> `training_screen_select_input` -> `training_screen_draw_select` ->
  `training_screen_draw_centered` (or the selector draw) -> `training_screen_draw_text` -> `gfx_draw_text` -> ...
- `menu_handle` -> `sending_handle` -> `training_screen_select_input` -> ... (same tail).
- `menu_poll` -> `menu_render` -> `draw_list` -> `draw_item_row` -> level helper -> `draw_text` -> `gfx_draw_text`
  -> ...

T14 measures the baseline again before the first change. It reports the 14.2 table before and after. It also
reports the flash per changed file and the deepest chain by name.

Change D-T44 (task T15): Receiving waits for `enter` after the third wrong attempt. No new file, no ISR change. Later
commits (startup screen, About item, backlight) changed the build after T14. So T15 measures the baseline on the
unchanged tree before the first change.

| Resource | Baseline (measured by T15) | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- | --- |
| Flash | T15 measures it | at most +120 B | +200 B |
| xdata | T15 measures it | +0 B (the hold flag exists) | +1 B |
| DSEG, overlay, bit bytes | unchanged | +0 B | +0 B |
| Stack | T15 measures it | no deeper main-loop chain: `rx_handle_answer()` -> `player_start()` is not deeper than `rx_handle_item()` -> `rx_pick()` -> `player_start()` | margin at least 64 B |
| Timer 0 ISR | keyer 403, player 186, disabled 63 clocks | unchanged: no change in `sidetone.c`, `keyer_service.h`, `player_service.h`, `keyer.c` or `player.c` | - |

Change D-T45 to D-T47 (task T16): start screen with a big level description, split end screen. No new file, no ISR
change. The coordinator measured the baseline on the unchanged tree (2026-10-15). It equals the final T15 values.
T16 measures it again before the first change.

| Resource | Baseline (after T15) | Target | Hard limit (above it, `architector` decides) |
| --- | --- | --- | --- |
| Flash | 28 209 of 61 440 B | Part A: at most +40 B. Whole T16: at most +250 B | +400 B |
| xdata | 1817 of 3072 B | +0 B: `tx_mark_x` and `tx_shift` go away, the code helpers use at most the same bytes | +2 B |
| DSEG, overlay, bit bytes | unchanged | +0 B | +0 B |
| Stack | starts at 0x41, 191 B free. At T14: summed-push bound 66 B, ISR frame 18 B, margin 107 B | deepest main-loop chain at most +6 B | margin at least 64 B |
| Timer 0 ISR | keyer 403, player 186, disabled 63 clocks | unchanged: no change in `sidetone.c`, `keyer_service.h`, `player_service.h`, `keyer.c` or `player.c` | - |
| Step redraw | pages 0, 4 and 5 | pages 0-5 (SL-4) | device check DC-T11 |

Flash estimate: the engine call about +15 B, the shorter step check about -8 B. The code helpers in
`training_screen.c` with the two-pattern group about +30 B net, because the `sending.c` copy goes away. Start screen
rows (label, big line, code group, three level kinds) about +100 B. The END branch of the selector input about +20 B.
The end screen and the two mode branches about -30 B. Net about +130 B.

Stack candidates for T16, to compare with the bound that T16 measures:

- `menu_handle` -> `receiving_handle` -> `training_screen_select_input` -> `training_screen_draw_start` -> (static
  rows 1-5 helper) -> `screen_draw_big_text` -> `gfx_draw_text` -> `font_u8g2_draw_glyph` -> `lcd_fb_set_pixel`.
- `menu_handle` -> `sending_handle` -> `training_screen_select_input` -> `training_screen_draw_select` -> (static
  rows 1-5 helper) -> `training_screen_draw_code_group` -> `training_screen_draw_code_mark` -> `gfx_fill_rect` -> ...
- `menu_poll` -> `sending_poll` -> `tx_draw_input` -> `tx_draw_item_line` -> `training_screen_draw_item_text` ->
  `screen_draw_big_text` -> `gfx_draw_text` -> ... (one level deeper than before).
- `menu_poll` -> `sending_poll` -> `tx_handle_keyer_event` -> `tx_add_element` -> `training_screen_draw_code_mark`
  -> `gfx_fill_rect` -> ...

T16 reports the 14.2 table before and after each part, the flash per changed file and the deepest chain by name.

Report table from `build.sh`, `.map` and `.mem`, before and after each task:

| Task | Flash B | DSEG B | Overlay B | Bit bytes | xdata B | Stack start | Stack free B | Deepest main-loop stack B | Largest ISR frame B | Margin B |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |

Also report the flash per new file, the newly linked library helpers and the deepest main-loop chain by name.
Candidates:

- `menu_handle` -> `receiving_handle` -> `training_add_char` -> ...
- `menu_poll` -> `receiving_poll` -> `training_poll` -> `progress_save` -> `eeprom_write` -> ...
- `menu_poll` -> `sending_poll` -> `gfx_draw_text` (8 x 16) -> `font_u8g2_draw_glyph` -> `lcd_fb_set_pixel`

## 15. Host test infrastructure

### 15.1 `Tests/CMakeLists.txt`

| Pattern | Group sources |
| --- | --- |
| `TIMING_TEST_PATTERN "^test_(qa_)?timing"` | `src/cw/timing.c`. `TARGET_DEFINITIONS`. |
| `PLAYER_TEST_PATTERN "^test_(qa_)?player"` | `src/cw/player.c`, `src/cw/timing.c`, `src/cw/morse.c`, `src/sidetone/sidetone.c`, `fakes/fake_sidetone_pwm.c`, `fakes/fake_tick.c`, `src/settings/settings.c`, `src/util/crc8.c`, EEPROM sources. `TARGET_DEFINITIONS`. |
| `RNG_TEST_PATTERN "^test_(qa_)?rng"` | `src/util/rng.c`. |
| `KOCH_TEST_PATTERN "^test_(qa_)?koch"` | `src/koch/koch.c`, `src/koch/koch_item.c`, `src/koch/keyboard.c`, `src/util/rng.c`, `src/cw/morse.c` (D-T21). |
| `PROGRESS_TEST_PATTERN "^test_(qa_)?progress"` | `src/progress/progress.c`, `src/util/crc8.c`, EEPROM sources. `TARGET_DEFINITIONS`. |
| `TRAINING_TEST_PATTERN "^test_(qa_)?training"` | `src/modes/training.c`, koch group sources, progress group sources, `fakes/fake_tick.c`. |
| `MODES_TEST_PATTERN "^test_(qa_)?(receiving\|sending)"` | The menu group sources, with `fakes/fake_koch_item.c` and `fakes/fake_player.c` (D-T25). The scenarios of `ui-ux.md` 9.4 start from the menu. |
| menu group (existing) | Adds the modes group sources except the duplicates, with `fakes/fake_koch_item.c` and `fakes/fake_player.c`. |
| keyer, sidetone, settings, free_input groups | Add `src/util/crc8.c` where they link `settings.c`. The keyer group adds `src/cw/timing.c`. |

Add the new patterns to `MODULE_TEST_PATTERNS`. All host flags stay. No warning suppression.

### 15.2 Fakes

`Tests/fakes/fake_player.c/.h` implements `player.h`. Minimum API (names binding):

```c
void fake_player_reset(void);
size_t fake_player_start_count(void);
size_t fake_player_stop_count(void);
bool fake_player_is_running(void);          /* after a successful start, before stop or finish */
void fake_player_finish(void);              /* the playback ends: busy becomes false */
void fake_player_set_start_result(bool is_accepted);
const char* fake_player_last_text(void);    /* copy of the last text, NUL-terminated */
uint8_t fake_player_last_wpm(void);
uint8_t fake_player_last_eff_wpm(void);
```

`fake_stats.c`: `stats_reset()` returns the value of `fake_stats_set_reset_result(bool)`, default true. The existing
counter API stays.

`Tests/fakes/fake_koch_item.c/.h` implements `koch_make_item()` (D-T18). It is the "fake training source" of
`ui-ux.md` section 9.4. Minimum API:

```c
void fake_koch_item_reset(void);
void fake_koch_item_push(const char* text);  /* FIFO of at least 128 items; overflow fails the test */
size_t fake_koch_item_count(void);           /* koch_make_item() calls so far */
```

`koch_make_item()` of the fake returns the next pushed item. After the last one it returns the last one again. With
no item pushed, it fails the test.

Owner of `Tests/support/` (`progress_fixture`, `training_harness.h`): `c-embedded-developer`. QA uses these files and
reports a needed change to `architector`. The four-field `KEYS` helper of `training_harness.h` (T11) follows
`ui-ux.md` 9.4 and is accepted by QA.

`Tests/support/progress_fixture.c/.h`:

```c
/** Writes a valid progress record to the fake IAP: all levels 0, then @p level for (@p mode, @p wpm). */
void progress_fixture_write(uint8_t mode, uint8_t wpm, uint16_t level);
```

Map from the `ui-ux.md` 9.4 notation to the level index (the real level model gives the set and the kind):

| Notation | Level index | Set | Kind |
| --- | ---: | --- | --- |
| `("KM", chars)` | 0 (empty EEPROM) | K, M | chars, 50 items |
| `("KMR", chars)` | 1 | K, M, R | chars |
| `("KMRSUAPTLOWI.", words)` | 17 | 13 characters | words of 4 |
| `(ALL, chars)` | 61 | 43 characters | chars |
| `(ALL, words)` | 62 | 40 characters (D-T4) | words of `Max Word Length` |
| V-109 | 65534, 65535 | 43 / 40 | final words, 100 |

Player tests define `PLAYER_SIDETONE_ON()` and `PLAYER_SIDETONE_OFF()`, include `cw/player_service.h` and call
`player_service()` once per tick. An integration test runs the real `sidetone_service()` after each call.

### 15.3 Allowed changes of existing tests

- `settings_crc.h` -> `util/crc8.h` and `settings_crc8` -> `crc8_compute` (D-T10). No other change in those lines.
- Placeholder tests (`test_menu.c`, `test_menu_preview.c`, `test_qa_menu.c`) change to the Receiving and Sending
  screens (D-T8).
- `stats_reset()` returns `bool` (D-T11).
- The `check_asm.sh` fixture `accept/real_image` is regenerated from the current build, and the built-in `sidetone.c`
  cases get the valid ISR and services (section 13.2, D-T20).
- D-T27/D-T28 (task T11): tests that expect three runs, the old run sizes, `KEYBOARD_GROUP_LEFT`, `_MIDDLE`,
  `_RIGHT`, `KEYBOARD_DEPTH_MAX` = 4, the old labels, `enter` at the press in tap mode, or an `enter` replay change
  to the new contract. This covers `test_koch_keyboard.c`, `test_buttons_tap.c`, `test_qa_buttons.c`,
  `test_receiving.c`, `test_qa_receiving.c` and any menu test with a Receiving `enter`. Each changed expected value
  traces to D-T27, D-T28 or a revised V-vector of `ui-ux.md` 9.4. Tests with tap mode off do not change.
- D-T30/D-T31 (task T12): Sending tests that expect row 7 (or row 5) to keep its content at RETRY, or a WORD_GAP to do
  nothing after an accepted character of an unfinished word, change to the new contract. This covers
  `test_sending.c`, `test_qa_sending.c` and any menu test with these steps. Each changed expected value traces to
  D-T30, D-T31 or a revised V-vector of `ui-ux.md` 9.4 (V-112, V-116 and the vectors of task D2).
- D-T33 (task T13): the `koch_pass_threshold()` calls in `test_koch.c` and `test_qa_koch*.c` get the kind
  argument. Expected thresholds and end-screen texts of word sessions change from `ceil(0.9 n)` to `ceil(0.8 n)`:
  `test_training.c` (10 words: 9 -> 8, 100 words: 90 -> 80), V-121 in `test_sending.c` (`Not passed, need 90` ->
  `need 80`), and every QA or menu test with a word-session threshold. Character-session values do not change.
- D-T35 to D-T41 (task T14): a call `training_start(mode, wpm, max)` becomes `training_prepare(mode, wpm, max)` and
  then `training_start()`. A scenario that expects a session right after the mode entry gets one `enter` press on the
  start screen first. The entry helper of `training_harness.h` can do this. Expected values of the end
  screen level row (`New level`, `Repeat level`, `Top level`) change to the selector of `ui-ux.md` (task D4).
  Receiving tests that expect tap mode on the end screen (an `enter` or `menu/back` at the release there) change to
  the press. Sending tests that expect `keyer_start()` at `sending_enter()` change to the start from the start
  screen. This covers `test_training.c`, `test_receiving.c`, `test_sending.c`, `test_menu_training.c`, the
  `test_qa_*` files of these groups and `Tests/support/`. Each changed expected value traces to D-T35 to D-T41 or to
  a revised V-vector of task D4.
- D-T44 (task T15): Receiving tests that expect the answer state to end by itself after the answer playback (with
  `fake_player_finish()` and a wait of `TRAINING_FAILED_MS`) get an `enter` tap to continue. Tests that expect a
  long `enter` in the answer state to do nothing change to the replay. Engine tests that expect
  `training_hold_result(false)` to start `TRAINING_FAILED_MS` change to the end at the next poll. The files:
  `test_training.c`, `test_receiving.c`, `test_qa_receiving.c` and any other `test_qa_*` file with a Receiving
  third wrong attempt. Known cases: the `test_receiving.c` session helper that calls `fake_player_finish()` after a
  third wrong attempt, and `test_long_enter_outside_item_and_long_next` in `test_qa_receiving.c`. Each file keeps its owner: `qa-embedded` changes the
  `test_qa_*` files (D-T43 (4)). Each changed expected value traces to D-T44 or to a V-vector of task D5. Sending
  tests with `TRAINING_FAILED_MS` do not change.
- D-T45 to D-T47 (task T16):
  - Engine tests that step the selection in END call `training_reopen_select()` first. Tests that expect a step in
    END to act change to "no step in END". Known cases: `test_select_step_phases` and `test_start_at_selected_level`
    in `test_training.c`, the random walk of AC-T31 (7), and `test_qa_training_select.c`.
  - Scenario tests that start the next session from END with one `enter` get a second `enter`. The first one opens
    the start screen. Tests that step on the end screen open the start screen first.
  - End screen expectations change to SL-7: rows 4-6 blank, the band with the D6 label. Start screen expectations
    change to SL-2a to SL-2c: the selector in row 1, the label in row 2, the big line, the code group in Sending.
  - `Tests/support/training_harness.h`: `lvl()` reads row 1. `desc()` and `start_button()` change to probes for the
    label, the big line, the code group and the band with its label. `end_screen_is()` follows SL-7. The probe names
    follow the 9.4 notation of task D6.
  - The pixel goldens of the start screen and the end screen in `test_qa_receiving_level_select.c` come again from
    the D6 layout.
  - Developer files: `test_training.c`, `test_receiving.c`, `test_sending.c`, `test_menu_training.c` and
    `Tests/support/training_harness.h`. QA files, changed by `qa-embedded` (D-T43 (4)): `test_qa_training_select.c`,
    `test_qa_receiving_level_select.c`, `test_qa_receiving.c`, `test_qa_sending.c`, `test_qa_sending_pass_rate.c`,
    `test_qa_sending_word_space.c` and `test_qa_menu.c`. Also every other `test_qa_*` file with an end screen or a
    next session.
  - In-session expectations do not change: the Sending code rows 5 and 7, the item line, the status and the
    Receiving labels. `test_qa_golden_capture.c` does not change: it decodes an LCD bus capture, not a training
    screen.
  - Each changed expected value traces to D-T45 to D-T47 or to a V-vector of task D6.
- No change removes a check or loosens an expected value.

## 16. Acceptance criteria

Developer test files: `test_morse_encode.c`, `test_timing.c`, `test_player.c`, `test_rng.c`, `test_koch.c`,
`test_koch_keyboard.c`, `test_progress.c`, `test_buttons_tap.c`, `test_training.c`, `test_receiving.c`,
`test_sending.c`, `test_menu_training.c`. QA test files: `test_qa_<group>*.c`.

| ID | Criterion |
| --- | --- |
| AC-T1 | Encode. For every index 2..127 with a table entry, `morse_encode(morse_decode(i)) == i`. Every other value 0..255 gives `MORSE_PATTERN_INVALID`, also `'\0'`, `' '`, `'*'` and lowercase letters. |
| AC-T2 | Timing. `cw_timing_compute()` gives the dit and unit of the formula for every pair 5 <= eff <= wpm <= 50 and the clamps of AC-K20. All AC-K tests pass with no change (AC-K9, AC-K16 prove the keyer values). |
| AC-T3 | RNG. Reference values of section 5.5. Period 65535 from seed 1. Never 0. Seed 0 and a mix to 0 give the fallback. `rng_below(n)` < n for n 1..255 over 65535 steps. `rng_below(0)` gives 0 and does not step. |
| AC-T4 | Tables. Both Koch tables have `KOCH_CHAR_COUNT` entries, the same set, no duplicate, and every entry has a Morse code (`morse_encode`). The Koch order equals the list of section 5.6. `koch_index_of()` is the inverse of `koch_char()`. |
| AC-T5 | Level model. For every `L` 0..`KOCH_FINAL_LEVEL + 1`: kind, learned count, new character, item count and word length for each `Max Word Length` 4..8 match section 6. Item count at `F`, `F + 1`, `F + 89`, `F + 90` and 65535: 10, 11, 99, 100, 100. Out-of-range `max_word_len` 0, 1, 9 and 255 clamp. |
| AC-T6 | Threshold (D-T33). `koch_pass_threshold(KOCH_KIND_CHAR, n) == ceil(0.9 n)` and `koch_pass_threshold(KOCH_KIND_WORD, n) == ceil(0.8 n)` for n 1..100, computed by an independent integer reference in the test. Char: 50 -> 45, 10 -> 9, 100 -> 90. Word: 10 -> 8, 11 -> 9, 15 -> 12, 50 -> 40, 100 -> 80. n = 0 gives 0 and n = 255 gives 230 (char) and 204 (word). An unknown kind (2 and 255) gives the char value. |
| AC-T7 | Item mix. The test makes 10 000 items for each of 5 seeds. At character levels 0, 1, 30 and 61, the new character share is 45..55 %. Each older character appears. Level 0 gives K and M at 45..55 % each. Word items have the level length. They use only the first `koch_alphabet_count()` Koch characters, so never BT, SK or AR. |
| AC-T8 | Keyboard, for every count 2..43 given to `keyboard_reset()`. With `training_alphabet_count()` of a word level, the list has no prosign. The list is the learned set in keyboard order. The four group sizes follow section 8 (D-T27) in every range, not only the whole list. Every character is reachable with at most 3 selects, and with exactly the select count of section 8. `keyboard_depth()` never passes `KEYBOARD_DEPTH_MAX - 1`. Group 4 and 255 are unknown: size 0, no change. An empty group changes nothing. `keyboard_back()` restores the ranges in reverse order and returns false at the top. A pick returns to the top. |
| AC-T9 | Progress load. An erased page, a wrong magic, version 0 and 2, a bad CRC and a read failure give their status and level 0 everywhere. A valid record gives its levels, including 0xFFFF. Load never writes. |
| AC-T10 | Progress save. The written page equals the layout of 10.1. The magic is programmed last. A failure in the erase, the first write or the second write gives SAVE_FAILED and keeps the RAM levels. A power loss after any operation never leaves a valid record with wrong data. |
| AC-T11 | Advance and reset. Advance at 65534 gives 65535 and true. At 65535 it gives false and no change. Each (mode, wpm) changes only its own two bytes. Reset sets all 92 levels to 0 and saves. Unknown mode: level 0, no advance. |
| AC-T12 | Player timing. Speeds: 5, 20 and 50 WPM, and 20/10 and 50/5 Farnsworth. Texts: `E`, `T`, `K`, `5`, `0`, `PARIS`, a prosign and an 8-character word. Every mark and gap has the exact length of section 9.2. The request goes off on the call where 32 mark calls remain. Busy clears on the call after the last mark. |
| AC-T13 | Player envelope. With the real `sidetone_service()` after each call, compare 0 is written on the last mark call of every element, for the speaker and the headphones. |
| AC-T14 | Player control. `player_start()` rejects length 0, length 9 and a character without a code, and then plays nothing. A start during a playback restarts from the first element. `player_stop()` clears busy, and the next calls change no player state. A disabled service reads no shared value except `player_is_enabled`. |
| AC-T15 | Tap mode (D-T28). A tap of `menu/back`, `prev`, `next` or `enter` gives its event at the debounced release. A hold gives only the long event at `BUTTONS_LONG_PRESS_MS` (long `enter` = 0x44), and nothing at the release. The edge case of D-T22 holds for `enter` too. No repeat in tap mode. Long events due in one poll come in the order `menu/back`, `enter`, `prev`, `next`. A hold that started before the switch gives no event, also an `enter` hold (the entry from the menu). `buttons_set_enter_enabled(false)` and `(true)` drop a long-pending `enter`. Off: `enter` gives its event at the press and never a long event (AC-K45), and all existing tap-off button tests pass unchanged. |
| AC-T16 | Engine flow. Fake tick: start -> GAP -> ITEM after `TRAINING_START_MS` -> answers -> RESULT -> GAP or RETRY or END with the exact delays of 11.2. One event per poll. |
| AC-T17 | Engine checks. Receiving: PARTIAL until the last character, then one whole-word check. Sending: a wrong character ends the attempt at once. Three wrong attempts give FAILED. A correct item after a wrong attempt does not count in the score. Characters out of phase ITEM give IGNORED and change nothing. Undo works only in ITEM. A held RESULT never ends. `training_hold_result(false)` in RESULT ends it at the next poll (D-T44, AC-T37). A phase change clears the hold. Sending: `TRAINING_CHAR_WORD_SPACE` after k accepted characters (k = 1 and k = length - 1) gives WRONG for attempts 1 and 2 and FAILED for the third, with `TRAINING_WRONG_MS` and `TRAINING_FAILED_MS`. The answer ends with it, the answer length is k + 1, and the item does not count in the score (D-T31). |
| AC-T18 | Engine finish. `training_pass_threshold()` is 45 at a character level, 8 at the first word level (10 words) and 80 at a final level of 100 words (D-T33). Score at threshold - 1 fails and keeps the level. Score at threshold passes, advances and saves. A failed save gives PASS_NOT_SAVED and the new level stays in RAM. A pass at 65535 gives PASS_TOP with no save. Shown levels follow D-T5. |
| AC-T19 | Receiving scenarios. Basic steps: enter the mode, play, pick a character with each of the four buttons, undo, step out. Replay with a long `enter` and with a tip closure of 800 ms. A short tip closure selects the `enter` run. A long `enter` in GAP, RESULT and END does nothing, except in the answer state of D-T44 (AC-T38). The `enter` that opens the mode gives no pick at its release. Results: three wrong attempts, pass, fail. Next session with `enter`. Leave with a short `menu/back` at END, and with a long `menu/back` in each phase. The player stops on every leave. Tap mode is off after every leave. |
| AC-T20 | Sending scenarios. Keyed input: a correct character, a wrong character, an unknown pattern, an overflow, a word. Events in RESULT and GAP are dropped. A character that starts in a delay is dropped up to its CHAR_GAP, also after the delay ends (D-T15). An overflow uses no attempt (D-T16). The blank GAP shows also for a repeated item. END stops the keyer and gives P3.2 back. Next session with `enter`. Leave with a long `menu/back` in each phase. |
| AC-T21 | Exclusive sidetone users. In every menu scenario of AC-T19, AC-T20 and the QA random walk, the fake keyer and the fake player never run at the same time. |
| AC-T22 | Menu. Receiving and Sending items open their screens. `Reset Statistics` Yes: stored gives `Statistics reset`, failed gives `Not saved`. The placeholder screen is gone. `main.c` calls `progress_load()` after `settings_load()`. |
| AC-T23 | UI. V-90 to V-127 of `ui-ux.md` section 9.4 pass, with the fakes and the fixture of section 15.2. |
| AC-T24 | ISR. `check_asm.sh` R6 and R7 pass on the build. The self-test passes with the new cases. ISR times within section 14 with the named paths. |
| AC-T25 | Memory within section 14 with the full report. |
| AC-T27 | Change D-T27/D-T28. Memory within the T11 budget of section 14.2, with the before and after report. `check_asm.sh` passes. The revised and new V-vectors of task D1 pass. The labels meet LC-1 to LC-7 (section 12.2). A host test checks the button-to-run mapping for all four buttons. |
| AC-T28 | Change D-T30/D-T31, through the menu with the fake keyer. (1) Retry screen: after a wrong character, an unknown pattern (`*`) and a word-space error, with `Show CW Code` `On` and `Off`, a character item and a word item: at RETRY, rows 5 and 7 are off, the status is `Try 2 of 3` (then `Try 3 of 3`), a word item has the bar under its first character, a character item has no bar. The first DIT after the retry draws its mark at x 2. During RESULT, row 7 keeps the marks of the keyed character. (2) Word space: in a word item, a WORD_GAP right after the PARTIAL CHAR_GAP of character k (k = 1 and k = length - 1) gives the word-space result of `ui-ux.md` 5.14 with `TRAINING_WRONG_MS`. Three in a row give FAILED with row 5 showing the code of item character k + 1. The retry is the whole item and the score does not count it. (3) No effect, with no status, no attempt and no redraw: a WORD_GAP before the first character of the item (also after a retry), after the last character, after a wrong character, in the start pause, RESULT and GAP, after OVERFLOW (with the drop flag set, and after the CHAR_GAP that clears it), after a character dropped by D-T15, and after a DIT that follows the PARTIAL CHAR_GAP. A WORD_GAP in a character session. (4) CHAR_GAP (PARTIAL) and WORD_GAP in one poll give the wrong attempt. (5) The revised and new V-vectors of task D2 pass. (6) Memory within the T12 budget of section 14.2, with the before and after report. `check_asm.sh` passes, and no ISR file changes. |
| AC-T29 | Change D-T33. (1) AC-T6 and AC-T18 as revised. (2) Through the menu: a Receiving and a Sending word session of 10 items with 8 first-attempt correct pass (`Passed`, `New level`, progress saved), with 7 fail (`Not passed, need 8`, `Repeat level`, no EEPROM write). A character session with 44 of 50 still fails (`need 45`). A final level of 100 words with 79 fails (`need 80`) and with 80 passes. (3) The revised and new V-vectors of task D3 (`ui-ux.md` V-121, V-136 and any other) pass. (4) Memory within the T13 budget of section 14.2, with the before and after report. `check_asm.sh` passes. |
| AC-T30 | Selection engine (D-T36), `test_training.c`. (1) `training_prepare()` with stored levels 0, 1, 17, 62 and 65535 gives phase SELECT and selected = earned. An unknown mode (2, 255) acts as RECEIVING. Prepare does no IAP erase and no IAP write. In SELECT, `training_poll()` gives NONE at any tick, `training_add_char()` gives IGNORED and `training_undo_char()` gives false. (2) From earned E = 19: 19 down steps reach 0 and each returns true. The next down step returns false and changes nothing. Then 19 up steps reach 19, and the next up step returns false. With E = 0 both directions return false. With E = 65535 an up step at 65535 returns false. In GAP, ITEM, RESULT and IDLE a step returns false and keeps the selection. In END a step acts (revised by AC-T40 (1): no step in END). (3) `training_start()` from SELECT and from END runs at the selected level: `training_level()`, the kind, the item count and the threshold of that level (index 0: 50 items, threshold 45. Index 2: 10 words, threshold 8). (4) `training_new_shown_level()` equals `koch_shown_level(training_selected_level())`. `koch_shown_level()` gives 1 for 0, 65534 for 65533, and 65535 for 65534 and 65535. |
| AC-T31 | Finish and replay (D-T36), `test_training.c`. (1) Pass at the earned level E: outcome PASS, one save, stored level E + 1, selected E + 1. (2) Earned 19, pass at index 4: outcome PASS_REPLAY, no IAP erase, no IAP write, record bytes unchanged, earned 19, selected 5, and up steps reach 19. (3) Fail at index 4 with earned 19, and fail at the earned level: outcome FAIL, no IAP write, selected = session level. (4) Pass at 65535 with earned 65535: PASS_TOP, no write, selected 65535. (5) Pass at E with a failed save: PASS_NOT_SAVED, earned E + 1 in RAM, selected E + 1. (6) Pass at E - 1: PASS_REPLAY, selected = E. (7) Random walk of at least 1000 sessions: prepare, steps, start, pass or fail. After every call, 0 <= selected <= earned. The stored record changes only after a pass at the earned level. |
| AC-T32 | Receiving with the selector, through the menu. (1) The entry shows the start screen of `ui-ux.md` (task D4). No player start, no keyboard drawing. Tap mode is off: a `prev` hold gives one step at the press and steps at 400 ms and every 80 ms after it. The `enter` press that opened the mode gives no start at its release, and a hold of it gives nothing. (2) `prev` and `next`, press and repeat, step the level and redraw only the rows of `training_screen_draw_select()`. All other LCD pages stay bit-identical. At a limit a press or a repeat redraws nothing. Long `prev`, long `next` and long `enter` do nothing. (3) `enter` and a tip closure start a session at the selected level: the header level and the session kind follow the selection. In ITEM, tap mode is on (a `prev` tap selects its run at the release). (4) At END, tap mode is off. The preselection follows AC-T31. `prev` and `next` step with repeats. `enter` starts the next session at the selected level. A short `menu/back` leaves at the press. Revised by AC-T44: no step at END, and `enter` opens the start screen. (5) A `menu/back` press leaves from SELECT and from END. The long event of the same hold then does nothing on the main menu. After every leave, tap mode is off, the player is stopped, and the main menu shows the cursor on `Receiving`. (6) Hold a button through the END event. It gives no event. After a release and a new press, it acts. (7) A start from SELECT leaves page 0 clean. A start from END redraws it (revised by D-T47: no start from END). |
| AC-T33 | Sending with the selector, through the menu. (1) The entry shows the start screen. The fake keyer does not start, and `enter` stays enabled. (2) Steps as AC-T32 (2). (3) `enter` and a tip closure start the keyer once with the stored values and a session at the selected level. `enter` is disabled during the session. (4) At END: steps, `enter` starts the next session, `menu/back` leaves. Revised by AC-T44: no step at END, and `enter` opens the start screen. (5) A leave from SELECT or END calls no `keyer_stop()` and does not change the `enter` state. A long `menu/back` leave from GAP, ITEM or RESULT stops the keyer once and gives P3.2 back. (6) AC-T21 still holds: the fake keyer and the fake player never run at the same time. |
| AC-T34 | Main menu level (D-T41). The `Receiving` row shows `L<n>` of the stored level at `RX Speed`. The `Sending` row shows it at `TX Speed`. Column and video follow `ui-ux.md` (task D4), inverted on the selected row. Stored levels 0, 11, 65534 and 65535 show `L1`, `L12`, `L65535` and `L65535`. After a `RX Speed` change, the main menu shows the level of the new speed. After a pass at the earned level and a leave, the menu shows the new level. After a replay pass it does not change. `Free Input` and `Settings` rows are bit-identical to the baseline. |
| AC-T35 | UI vectors. The vectors of task D4 pass: V-01, V-19, V-105 to V-108, V-121, V-136 and V-137 to V-151. V-90 to V-136 use the new `RX()` and `TX()` entry through the start screen. QA golden LCD dumps cover three screens. Start screen: both modes, L1, a character level, a word level, a final level, the limits with and without `<` and `>`. End screen with the selector: each outcome, PASS_REPLAY included (revised by AC-T43: no selector). Main menu rows. |
| AC-T36 | Change D-T35 to D-T41: memory within the T14 budget of section 14.2, with the before and after report and the named deepest chain. `check_asm.sh` passes. No change in `sidetone.c`, `keyer_service.h`, `player_service.h`, `keyer.c`, `player.c`, `buttons.c` or `firmware/drivers/`. |
| AC-T37 | Engine hold (D-T44), `test_training.c`. (1) Receiving item with three wrong attempts, then `training_hold_result(true)`. Poll at every 1000 ms step up to 70 000 ms after the check. This passes one 16-bit tick wrap. Each poll gives NONE, and the phase stays RESULT. (2) Then `training_hold_result(false)` at tick P. If items remain, the poll at P gives GAP, and the next item comes `TRAINING_RECEIVING_GAP_MS` after P. On the last item, the poll at P gives END with the outcome of 11.4, and the FAILED item does not count in the score. (3) `training_hold_result(false)` in RESULT with the hold off also ends the RESULT at the next poll: after CORRECT (GAP before `TRAINING_CORRECT_MS`) and after WRONG (RETRY before `TRAINING_WRONG_MS`). (4) In SELECT, GAP, ITEM, END and IDLE, both calls change nothing (phase, events and timing). (5) Hold on in one RESULT, then a phase change: the next RESULT runs its normal delay. (6) The Sending FAILED test stays unchanged: GAP exactly at `TRAINING_FAILED_MS`, not one tick before. |
| AC-T38 | Receiving answer state (D-T44), `test_receiving.c`, with V-170 and the revised V-99, V-121 and V-136 of task D5, through the menu with the fake player. (1) After the third wrong attempt: status `Wrong. The answer is:`, the answer on the item line, one new `player_start()` of the item. (2) After `fake_player_finish()` and also 70 000 ms later, the phase is RESULT and the screen is unchanged. (3) Row 7 shows only `Next` in the `enter` field (position of `ui-ux.md` UX-64). The other three label fields are blank. The key bar segments are unchanged. (4) `menu/back`, `prev` and `next` taps, long `prev` and long `next` change nothing: no phase change, no LCD page dirty, no player call, no answer change. (5) A long `enter` starts the player once more with the item. A tip closure of 800 ms does the same. Both act during the playback and after its end. The screen does not change. (6) An `enter` tap during the playback stops the player at the tap. The next poll gives the blank moment: rows 1-5 off, the header shows the next item number, row 7 shows the labels of the whole set. The next item comes after `TRAINING_RECEIVING_GAP_MS`. The same after the playback end, and with a short tip closure. (7) On the last item, row 7 shows `Finish`. An `enter` tap gives the end screen at the next poll, with tap mode off, and the score does not count the item. (8) A long `menu/back` in the answer state leaves: the player is stopped, tap mode is off, and the main menu shows. |
| AC-T39 | Change D-T44: memory within the T15 budget of section 14.2, with the before and after report and the deepest chain by name. `check_asm.sh` passes. No change in `sending.c`, `sidetone.c`, `keyer_service.h`, `player_service.h`, `keyer.c`, `player.c`, `buttons.c` or `firmware/drivers/`. All Sending tests pass with no change. |
| AC-T40 | Engine (D-T46), `test_training.c`. (1) In END after each outcome (FAIL, PASS, PASS_NOT_SAVED, PASS_TOP, PASS_REPLAY), `training_select_step(true)` and `training_select_step(false)` return false. The selected level, the phase and the outcome do not change. (2) Then `training_reopen_select()` returns true, and the phase is SELECT. The selected level is the preselection of 11.4. After a pass at the earned level E (also PASS_NOT_SAVED), it is E + 1. After a replay pass at index 4 with earned 19, it is 5. After a fail, it is the session level. After PASS_TOP, it is 65535. The earned level does not change. No IAP command comes. (3) After the call, the steps act inside 0..earned with the limits of AC-T30 (2). `training_start()` then starts at the selected level. In this SELECT, `training_poll()` gives NONE, `training_add_char()` gives IGNORED and `training_undo_char()` gives false. (4) A second call returns false. In IDLE, SELECT, GAP, ITEM and RESULT (hold on and hold off), the call returns false and changes nothing: the phase, the selected level, and the events and times of the next polls equal a run without the call. (5) The random walk of AC-T31 (7) calls `training_reopen_select()` after each session. It checks the invariant after every call, also after the reopen. The steps after the reopen act. |
| AC-T41 | Start screen (D-T45, SL-2a to SL-2c, SL-8, SL-9), through the menu, both modes. (1) The layout of `ui-ux.md` 5.16.1 (task D6). Row 0: the header of SL-6. Row 1: the selector with the markers of SL-1. Row 2: the label. Rows 3 and 4: the big line at y 24 in the 8 x 16 font. Row 5: see (3) and (4). Row 7: the `Start` band. Row 6 is blank. (2) Levels: L1, a character level (L20, `J`), the AR level with each `Prosigns` value, and the level of `,`. Also a word level, a final word level, and the limits with and without `<` and `>`. (3) Sending row 5: the centered code of the new character. L1: the codes of K and M as one group with the D6 gap. Word levels: blank. `Show CW Code` On and Off give the same pixels. (4) Receiving: row 5 is blank at every level of (2). (5) The widest cases of SL-9 keep the pixel limits. All texts are ASCII 0x20-0x7E. (6) A word session of 5 characters, then the start screen at a character level: the big character is centered at `x = 64 - 4 m`. A character session, then the start screen at L1: K and M at the D6 position. So the start screen does not use the item length of the last session (SL-8). (7) The revised and new V-vectors of task D6 for the start screen pass. |
| AC-T42 | Steps on the start screen (SL-4). (1) A step that changes the level makes pages 0-5 dirty. Pages 6 and 7 stay bit-identical. (2) Step from the earned level down to L1 and up again. After every step, rows 0-7 equal the start screen of a full `menu_render()` at the same level. The walk crosses a change of kind (L2 and L3). In Sending, it also crosses a change from a level with a code group to a word level. (3) A press or a repeat at a limit makes no page dirty. |
| AC-T43 | End screen (SL-7), both modes, for each outcome: PASS, PASS_NOT_SAVED, PASS_REPLAY, PASS_TOP and FAIL. Row 0 shows the header of the session that ended. Rows 1-3 follow SL-7. Rows 4-6 are blank. Row 7 shows the band with the D6 label. No selector, no description, no code group. The revised V-vectors of task D6 for the end screen pass. |
| AC-T44 | End screen controls (D-T45 (4), D-T47 (4), SL-10), through the menu, both modes. (1) `prev` and `next` presses, a hold of 2000 ms of each (repeats and the long event), and the ring do nothing: no dirty page, no change of phase or selected level. (2) An `enter` press, and also a tip closure, shows the start screen of AC-T41 at the preselected level. All pages 0-7 are redrawn. The header shows the selected level and `1/<n>`. Pass at the earned level: the new earned level, no `>`. Fail: the same level. Replay pass: the session level + 1, with `>`. PASS_TOP: `L65535 of 65535`. PASS_NOT_SAVED: the new level from RAM. (3) No session starts at that press. Receiving: no player start, and tap mode stays off. Sending: no keyer start, and `enter` stays enabled. The release of the press gives no event. A hold of 2000 ms gives no event. (4) A second `enter` press starts the session at the selected level, and page 0 is not redrawn. Receiving: tap mode is on. Sending: the keyer starts once, and `enter` is disabled. (5) A step on that start screen before the start changes the session level. (6) A `menu/back` press on the end screen and on that start screen opens the main menu with the cursor on the mode item. Sending calls no `keyer_stop()` at that leave. The next entry preselects the earned level. |
| AC-T45 | Buttons across the switches (section 12.6), both modes. (1) Hold `prev` or `next` from the last RESULT across END and across the `enter` that opens the start screen. No step and no dirty page come until the release. A new press then steps once (B-1). (2) Press and hold `next` in END, then press `enter`. On the start screen, the repeats step the selector (B-3). (3) Receiving: the start press from the reopened start screen gives no pick at its release and no replay at 800 ms (B-4). (4) Sending: the keyer runs only in GAP, ITEM and RESULT. A leave from the reopened start screen stops no keyer (B-5). AC-T21 holds. |
| AC-T46 | Code helpers (D-T47 (1) and (2)). (1) Every Sending test of rows 5 and 7 (V-110 to V-135, AC-T28) passes with no change of its expected values. (2) `sending.c` has no `tx_draw_mark()`, `tx_draw_answer_code()`, `tx_mark_x`, `tx_shift` and no mark width constant. `training_screen.c` holds the only mark drawing code. (3) For every Koch character, `training_screen_draw_code_group(morse_encode(c), MORSE_PATTERN_EMPTY)` draws the marks that the test computes from `ui-ux.md` 5.13.3. With a second pattern, the group is centered with the D6 gap. `training_screen_draw_code_mark()` returns x + 5 after a dit and x + 9 after a dah. (4) `training_screen_draw_item_text()` gives the same pixels as before for items of 1 to 8 characters, prosigns included. |
| AC-T47 | Change D-T45 to D-T47: memory within the T16 budget of section 14.2, with the before and after report of each part and the deepest chain by name. `check_asm.sh` passes. `pre-commit run --files <changed files>` passes. No change in `sidetone.c`, `keyer_service.h`, `player_service.h`, `keyer.c`, `player.c`, `buttons.c`, `koch.c`, `koch.h`, `progress.c`, `progress.h` or `firmware/drivers/`. In-session tests of both modes pass with no change of their expected values. |
| AC-T26 | `firmware/test.sh` and `firmware/build.sh` pass. All host tests pass under ASan and UBSan. No regression: existing tests change only as section 15.3 allows. |

## 17. Tasks

`c-embedded-developer` owns T0-T10. Each task ends with: `build.sh` passes with `check_asm.sh`, `test.sh` passes,
the memory table of section 14.2, the assumptions and the open device checks.

| Task | Content | Host tests | Depends on |
| --- | --- | --- | --- |
| T0 | Baseline. Measure section 4 again, with the deepest main-loop chain and the ISR frame and times from the `.asm`. | - | - |
| T1 | `morse_encode()`. | `test_morse_encode.c`: AC-T1. | T0 |
| T2 | `cw/timing` extraction, keyer change. | `test_timing.c`: AC-T2. All keyer tests unchanged. | T0 |
| T3 | `util/rng`, `koch/koch`, `koch/koch_item`, `koch/keyboard`. | `test_rng.c`, `test_koch.c`, `test_koch_keyboard.c`: AC-T3 to AC-T8. | T1 |
| T4 | `util/crc8` move, `progress`, EEPROM map, `stats_reset()` result, `fake_stats` change. | `test_progress.c`: AC-T9 to AC-T11. Changed settings tests (15.3). | T0 |
| T5 | Player, `player_service.h`, three-call ISR, `check_asm.sh` R6 and R7 with self-test cases, `llm.md` check (already updated by `architector`). | `test_player.c`: AC-T12 to AC-T14. Self-test: AC-T24. ISR times. | T1, T2 |
| T6 | Button tap mode. | `test_buttons_tap.c`: AC-T15. | T0 |
| T7 | Session engine. | `test_training.c`: AC-T16 to AC-T18. | T3, T4 |
| T8 | `training_screen`, Receiving mode, `fake_player`, `fake_koch_item`, `progress_fixture`. | `test_receiving.c`: AC-T19, V-90 to V-109 and V-120 of AC-T23. | T5, T6, T7 |
| T9 | Sending mode. | `test_sending.c`: AC-T20, V-110 to V-119 and V-121 of AC-T23. | T7, T8 (shared screen and fakes) |
| T10 | Menu wiring, placeholder removal, `main.c`, close-out report. | `test_menu_training.c`: AC-T21, AC-T22. Changed placeholder tests. AC-T25, AC-T26. | T8, T9 |

Then `qa-embedded` (Q1) verifies independently, `architector` reviews, and `technical-writer` writes the user manual
section for the two modes.

### 17.1 Change D-T27/D-T28: four-button Receiving keyboard

| Task | Owner | Content | Depends on |
| --- | --- | --- | --- |
| D1 | `ui-ux-embedded-designer` | `ui-ux.md` changes: the four-run keyboard and the label rule within LC-1 to LC-7. The `enter` tap, the long `enter`, the end screen and the shared tip. Revised and new V-vectors. A new UX decision. | - |
| T11 | `c-embedded-developer` | Part A (done): `buttons` (D-T28), `keyboard` (section 8), the button-to-run mapping and the ITEM events, with their tests. Part B: labels and key bar segments of D-T29 and `ui-ux.md` 5.12.1, the end screen events, and the V-tests of AC-T23 for Receiving. Memory report. | Part A: -. Part B: D1, Part A |
| Q2 | `qa-embedded` | Independent verification of AC-T8, AC-T15, AC-T19, AC-T23 and AC-T27. Mutation and fault cases on tap `enter`, long `enter`, the mapping and the four-run split. | T11 |
| W2 | `technical-writer` | User manual: Receiving keyboard, replay, end screen, tip behavior. Tell the user to insert the plug before opening Receiving (R-T7). | `architector` approval |

### 17.2 Change D-T30/D-T31: Sending retry screen and word space

| Task | Owner | Content | Depends on |
| --- | --- | --- | --- |
| D2 | `ui-ux-embedded-designer` | Delivered as UX-52, V-128 to V-134 and D-34. Review fixes F-1 to F-3 are open (below). `ui-ux.md` changes: the retry screen (5.13.1, 5.14), the word-space rule in 5.13.1 and 5.13.2 (WORD_GAP row), the word-space status text and RESULT look within WS-1 to WS-6, the revisions of UX-45 and UX-46, a new UX decision. Revised V-112, V-115 (state that its WORD_GAP comes before the first character of item 2, so nothing still happens) and V-116 (rows 5 and 7 off at the retry). New V-vectors from V-128: a word-space error at k = 1 and at k = length - 1, three word-space errors to FAILED, a WORD_GAP before the first character and after the last character, OVERFLOW then WORD_GAP, the retry screen with `Show CW Code` = `On`. | - |
| T12 | `c-embedded-developer` | Part A (can start now): `TRAINING_CHAR_WORD_SPACE` in `modes/training.h`. In `src/modes/sending.c`: the armed flag and the WORD_GAP row of section 12.3, the clear in `tx_start_session()`, and rows 5 and 7 off at RETRY. The engine case of AC-T17 in `test_training.c`. The behavior cases of AC-T28 (1), (3) and (4) in `test_sending.c`. Part B: in `training_screen_draw_status()` (`src/modes/training_screen.c`), the Sending RESULT branch shows `Wrong: pause too long` when the last answer character is `TRAINING_CHAR_WORD_SPACE`, else the current `Wrong: <text>`. No other RESULT change: the bar and row 5 already use the failed position (answer length - 1), and row 7 keeps its marks. Tests: V-112, V-113, V-115, V-116, V-121 (revised) and V-128 to V-132, V-134 and V-135 (after fix F-2) in `test_sending.c`. V-133 part A in `test_sending.c`, part B (real `keyer_service()`, 1199 and 1200 calls at 20/20, 4359 and 4360 calls at 20/10) in `test_keyer.c`, with no keyer code change. AC-T28 (2) and (5), and the existing tests that 15.3 lets change. Memory report (AC-T28 (6)). No change in `training.c`, the keyer, `keyer_service.h`, `sidetone.c` or Free Input. | Part A: -. Part B: D2, Part A |
| Q3 | `qa-embedded` | Independent verification of AC-T17 (Sending word space), AC-T20, AC-T23 and AC-T28. Mutation and fault cases: the armed flag replaced by an answer-length test (must fail the OVERFLOW case), the armed flag not cleared by a dropped event or a DIT, the WORD_GAP acting in RESULT, GAP or before the first character, row 7 cleared at RESULT instead of RETRY, row 5 or row 7 kept at RETRY, the status of a word space shown as a keyed character, a random walk of keyer events that checks the attempt count. | T12 |
| W3 | `technical-writer` | User manual, Sending: start the next character of a word within 5 units (a word space ends the attempt), a lower `TX Eff. Speed` gives more room, the retry screen. | `architector` approval |

Review of D2 (2026-10-07). The retry screen, the status text, the bar, row 5, row 7, the timing table of 5.13.1,
V-112, V-113, V-116, V-121 and V-128 to V-134 match D-T30 and D-T31. Open points of the designer: (1) yes, the
status code chooses `Wrong: pause too long` when the last answer character is `TRAINING_CHAR_WORD_SPACE`. (2) Yes,
row 5 shows the item character at answer length - 1, which is the character under the bar. (4) D-T31 stays: after
OVERFLOW, a WORD_GAP never counts (D-T16). (5) Yes, a WORD_GAP after the retry or in the next item does nothing.
(6) Accepted within the T12 flash budget.

The arming rule of D-T31 and the D2 rule "the attempt has a correct character and the item is not complete" differ
only after OVERFLOW. With the real keyer, CHAR_GAP comes at 2 dits and WORD_GAP at 5 units or more of the same
space, and a mark cancels both. So no DIT or DAH can come between a CHAR_GAP and the WORD_GAP of the same space, and
the last event before a WORD_GAP is always the CHAR_GAP of that space, or OVERFLOW. Fixes for `ui-ux.md`:

| ID | Fix |
| --- | --- |
| F-1 | Arming condition, in the same words in 5.13.1 (bullet "Pause too long" and bullet "A WORD_GAP does nothing"), 5.13.2 (WORD_GAP row) and UX-52 item (2): a WORD_GAP in Item counts when it ends the space right after a correct character that is not the last one of the item. Add to the "does nothing" list: after OVERFLOW (a lost event is a device fault, D-T16). |
| F-2 | 5.13.2, last bullet: remove "the dropped-character rule never drops it" and the "Proposed" text. Replace them with the decision: if the lost events held the CHAR_GAP, the drop flag is still set at the WORD_GAP, and the WORD_GAP is dropped with the rest of the character. If the CHAR_GAP came after OVERFLOW, the WORD_GAP of that space does nothing: the pause follows a lost character. New vector V-135, `TX("KMRSUAPTLOWI.", words, "KMRS")`. Run 1: `KEY("K")`, DIT, OVERFLOW, CHAR_GAP, WORD_GAP, then `KEY("MRS")`. Run 2: `KEY("K")`, DIT, OVERFLOW, WORD_GAP, then `KEY("M")` (dropped up to its CHAR_GAP), then `KEY("MRS")`. Both runs: `STATUS()` and `BAR(52)` up to the M that counts, no attempt, then `STATUS("Correct")`, and the score counts the item as first-attempt correct. Extend the global rule to V-135. |
| F-3 | 5.13.1, bullet "A WORD_GAP does nothing": "a later WORD_GAP falls in the result delay" is not always true. At a low `TX Eff. Speed` (20/5: 5 units are about 2669 ms), the WORD_GAP of the last space can come in the next item or in the retry, before its first character. Say: in the result delay, the blank moment, or the next attempt before its first correct character, it does nothing. |

### 17.3 Change D-T33: word pass rate 80 %

| Task | Owner | Content | Depends on |
| --- | --- | --- | --- |
| D3 | `ui-ux-embedded-designer` | `ui-ux.md`: the 80 % rule for word sessions (5.15 and every 90 % statement), the end-screen example, the longest text (`Not passed, need 80` and `need 45`, 19 cells: `need 90` can no longer show), V-121 and a new word-session pass/fail vector (V-136). | - |
| T13 | `c-embedded-developer` | `inc/koch/koch.h` and `src/koch/koch.c`: `koch_pass_threshold(koch_kind_t kind, MEM_XDATA uint8_t item_count)` as section 5.6 and the rule of section 6. `src/modes/training.c` `training_start()`: `koch_pass_threshold(koch_kind(training_level_value), training_item_count_value)`. No other production change: the end screen already prints `training_pass_threshold()`. Tests: `test_koch.c` (AC-T6), `test_training.c` (AC-T18), `test_receiving.c` or `test_sending.c` (AC-T29 (2), V-136), the 15.3 changes. The clang-tidy finding `bugprone-easily-swappable-parameters` on the new signature, if any, stays as in D-T21: no suppression. Memory report. | D3 for the V-vectors only |
| Q4 | `qa-embedded` | Independent verification of AC-T6, AC-T18 and AC-T29. Mutation cases: the kind ignored, the kinds swapped, 0.8 rounded down, an unknown kind on the word rule, the old rule at final levels. | T13 |
| W4 | `technical-writer` | User manual: the pass rule (`docs/user_manual.md` line ~415: 45 of 50 characters, 8 of 10 words, 80 of 100 words, 80 % rounded up for words), and every other 90 % statement for words. | `architector` approval |

### 17.4 Change D-T35 to D-T41: level selection

| Task | Owner | Content | Depends on |
| --- | --- | --- | --- |
| D4 | `ui-ux-embedded-designer` | Done, review `approved` (D-T42). `ui-ux.md` gets a new section after 5.15 for the start screen and the end screen with the selector. It gives the rows, the columns, the selector line and the `<` and `>` markers. It also gives the description texts, the `Start` band, the header in SELECT and END, and the PASS_REPLAY text. Main menu level column (5.2, 5.3). Control rows in 6.1, 6.8 and 6.9 (tap mode off on both selector screens, repeats). Revision of 5.15 (tap mode on the end screen, the level row) and of U-11. New UX decisions from UX-53. Revised and new V-vectors in 9.4, with the global rule for the entry (the start screen comes first). Device checks. All within SL-1 to SL-7 (section 5.11) and D-T41. | - |
| T14 | `c-embedded-developer` | Part A (can start now): `koch_shown_level()` in `koch.h` and `koch.c` (D-T37). `modes/training.h` and `src/modes/training.c` as section 5.10 and 11 (D-T36). In `receiving.c` and `sending.c`, only the call sites change: `training_prepare()` then `training_start()` at the places of the old `training_start()`. So the mode behavior stays as today and all existing scenario tests pass. Tests: AC-T30 and AC-T31 in `test_training.c`, `koch_shown_level()` cases in `test_koch.c`. Part B: the scope of section 17.4.1. Tests: AC-T32 to AC-T35 in `test_receiving.c`, `test_sending.c` and `test_menu_training.c`. Memory report (AC-T36), with the repeat cost of D-T39 named. No change in the ISR files, `buttons.c`, `keyer.c`, `player.c` or `firmware/drivers/`. | Part A: -. Part B: D4, Part A |
| Q5 | `qa-embedded` | Independent verification of AC-T30 to AC-T36 and AC-T26. Golden LCD dumps of AC-T35. Mutation and fault cases for the engine: a replay pass that saves or that advances the RAM level. `<` in place of `!=` in the finish test. Preselect = level after a pass, or level + 1 after a fail. A step that wraps, that passes the earned level, or that acts in GAP or ITEM. PASS_TOP not saturated. Cases for the modes: tap mode left on at END (no repeat), or off in ITEM. A stray event after the END switch. A held button across a phase switch. The keyer started at `sending_enter()`. `keyer_stop()` called by `sending_leave()` in SELECT. Cases for the menu: the level read at the effective speed or at the speed of the other mode. | T14 |
| W5 | `technical-writer` | Ready (T14 `approved`). `docs/user_manual.md`: remove "You cannot choose a level". Describe the start screen, the selector with auto-repeat and the description line. Describe the replay rule: a pass below the earned level keeps the stored level. Describe the preselection after a session, the end screen and the level in the main menu. Check every text that says a session starts at the mode entry. `docs/release_notes.md`: one entry for the change. | `architector` approval of T14 |

T14 Part A runs. T14 Part B can start (D4 `approved`, D-T42). It builds on Part A. If Part A changes an interface
of section 5, Part B stops and reports to `architector`.

#### 17.4.1 Scope of T14 Part B

| Area | Files | Content |
| --- | --- | --- |
| Shared screens | `inc/modes/training_screen.h`, `src/modes/training_screen.c` | `training_select_action_t` and `TRAINING_SELECT_*`. `training_screen_draw_start()`: rows 1-3 and 6 blank, selector row 4, description row 5, `Start` band row 7 (`ui-ux.md` 5.16.1). `training_screen_draw_select()`: rows 4 and 5 in full, and row 0 in SELECT only (SL-4, D-T42 (4)). `training_screen_select_input()` (D-T38, `ui-ux.md` 5.16.4). Selector line: `<`, two blanks, `L<s> of <e>`, two blanks, `>`, from column `floor((19 - len) / 2)`, markers by index (5.16.2). Description (5.16.3, SL-2). Header in SELECT (SL-6). End screen rows of SL-7, with one `Start` band helper for both screens. The texts `Next`, `New level L`, `Repeat level L` and `Top level L` go away. |
| Receiving | `src/modes/receiving.c` | Section 12.2 and D-T39: entry with `training_prepare()`, no tap mode call. `rx_start_session()` turns tap mode on. The END event turns it off. SELECT and END events go to `training_screen_select_input()`, also repeats. START from SELECT redraws rows 1-7 only, START from END also row 0. `receiving_render()` for SELECT and END. |
| Sending | `src/modes/sending.c` | Section 12.3 and D-T40: entry with `training_prepare()`, no keyer, no `buttons_set_enter_enabled()`. `tx_start_session()` unchanged apart from `training_start()`. SELECT and END events go to `training_screen_select_input()`. The same START redraw rule. `sending_render()` for SELECT and END. `sending_leave()` stops the keyer only in GAP, ITEM and RESULT. |
| Main menu | `src/menu/menu_render.c` | Section 12.5 and D-T41: `L<n>` right-aligned with its last character in column 22, in the label color, built in `value_text`. The compile-time mode check. |
| Test support | `Tests/support/training_harness.h` and helpers | `RX()`, `TX()`, `EARN()` and `OPEN()` of `ui-ux.md` 9.4: the entry passes the start screen with an `enter` press. Probes `LVL`, `DESC`, `STARTBTN`, `DIRTY`. |
| Tests | `test_receiving.c`, `test_sending.c`, `test_menu_training.c`, `test_menu.c` (V-01, V-19) | V-01, V-19, V-105 to V-108, V-121, V-136, V-137 to V-151. All of V-90 to V-136 through the new entry. The 15.3 changes. AC-T32 to AC-T35. |
| Report | - | AC-T36: the 14.2 table before and after, the flash per changed file, the repeat cost of D-T39, the deepest chain by name, `check_asm.sh` result. |

Not in Part B: any change of `training.h`, `training.c`, `koch.h` or `koch.c` beyond Part A, the ISR files,
`buttons.c`, `keyer.c`, `player.c` and `firmware/drivers/`. A need for such a change goes to `architector` first.

### 17.5 Change D-T44: Receiving waits for `enter` after the third wrong attempt

User decision 2026-10-15. Plan approved by the user. D5 done, review `approved` on 2026-10-15 (U-32). T15 Part A
runs. After Part A, T15 Part B can start.

Review of D5 (2026-10-15). `ui-ux.md` UX-64, 5.12.2, 5.14, 6.1, 6.4, 6.8, 6.10, 7, F-20, V-170, the revised V-99,
V-121 and V-136, and D-41 match D-T44. Notes for T15 Part B:

| ID | Note |
| --- | --- |
| N1 | V-170 has five runs. Run 1: the answer state and its no-op events. Run 2: a tap during the first playback. Run 3: the last item with `Finish`. Run 4: tip closures of 900 ms and 100 ms. Run 5: a long `menu/back` exit. AC-T38 also asks for a wait of 70 000 ms (one tick wrap). V-170 waits 10 s. Test both. |
| N2 | The blank moment and the end screen come at the tap tick. `harness_event()` runs `menu_handle()` and then `menu_poll()` at the same tick, so `training_hold_result(false)` must not change the phase start: duration 0 is enough. The GAP then starts at the tap tick, and the next item comes exactly 500 ms later (V-170: still blank at w + 499, item at w + 500). |
| N3 | Row 7 positions: `Next` at x 107 and `Finish` at x 97 are the existing right-alignment rule of the `enter` field (x = 127 - 5n). Use the code path of the `enter` label. No new position constant. |
| N4 | In the answer state, `rx_draw_labels()` runs only from the FAILED check (through `rx_draw_input()`). The GAP event draws the labels of the whole set again. Long `enter` and the no-op events draw nothing (V-170: no dirty page). |
| N5 | Three existing vectors change. In V-99, an `E` at w closes the answer state. In V-136, an `E` closes each Receiving answer state. V-121 also renders the answer state with `Next` and `Finish`. These are 15.3 changes, traced to D5. |
| N6 | V-170 Run 1 checks `stop()` in `PLOG` at the tap. Every `enter` tap in the answer state calls `player_stop()`. This includes a tap with the player idle (Run 3). |

| Task | Owner | Content | Depends on |
| --- | --- | --- | --- |
| D5 | `ui-ux-embedded-designer` | Done, review `approved` (U-32). `ui-ux.md`: new decision UX-64. 5.12.2: a new state "Answer, waiting". 5.14: the row "Wrong, attempt 3, Receiving", the delay table, the timeline and the answer playback text. 6.8: the event column of the answer state. 7: the feedback rows. 6.10: the `TRAINING_FAILED_MS` row applies to Sending only. Row 7 of the answer state: the `enter` field position and alignment for `Next` and `Finish`, the other fields blank. New and revised V-vectors in 9.4 and a device check. All within D-T44. | - |
| T15 | `c-embedded-developer` | Done, final review `approved` (section 22.5). Part A: `src/modes/training.c` and the doc comments in `inc/modes/training.h` (D-T44 (2), section 5.10): `training_hold_result(false)` in RESULT clears the hold and sets the duration to 0. The `TRAINING_FAILED_MS` comment says Sending only. AC-T37 in `test_training.c`. Part B: `src/modes/receiving.c` as section 12.2 and D-T44: `rx_is_answer_shown()`, used also by `rx_draw_item_line()`. The poll call of `training_hold_result()` goes away. The GAP event calls `rx_draw_labels()`. `rx_draw_labels()` in the answer state. `rx_handle_answer()`. AC-T38 in `test_receiving.c`, the V-vectors of D5 and the 15.3 changes of developer files. Build with `build.sh`, run `test.sh` and `pre-commit run --files <changed files>`. Report AC-T39. | Part A: -. Part B: D5, Part A |
| Q6 | `qa-embedded` | Done, `approved`. Independent verification of AC-T17, AC-T19, AC-T26 and AC-T37 to AC-T39, and the 15.3 changes of the `test_qa_*` files. New cases in `test_qa_receiving.c`. Fault cases: (F1) An `enter` tap in the same poll as the playback end. (F2) A long `menu/back` in the answer state. (F3) A tip closure from a plug insertion in the answer state: it continues, as a tap. (F4) The `enter` tap that makes the third wrong pick does not also continue. (F5) A second `enter` tap in the blank moment does nothing. Mutation cases: (M1) `training_hold_result(false)` restarts `TRAINING_FAILED_MS`. (M2) The per-poll `training_hold_result(player_is_busy())` stays. (M3) The answer state has no FAILED test, so it acts after CORRECT or WRONG. (M4) The `enter` tap does not call `player_stop()`. (M5) No label redraw at GAP. (M6) `Next` and `Finish` swapped, or the last-item test off by one. (M7) Another event acts in the answer state. (M8) A long `enter` does nothing. | T15 |
| W6 | `technical-writer` | Manual and quick guide done, review `approved` (section 22.5). Release notes open. `docs/user_manual.md`: the result table row of the third wrong attempt, and the Receiving button text. Tell the user that `enter` continues and long `enter` plays the answer again. Describe `Next` and `Finish`. `docs/release_notes.md`: the new version section, after the user decides the version. | `architector` approval of T15 |

If Part A changes an interface of section 5, Part B stops and reports to `architector`. Not in T15: a change of
`sending.c`, `training_screen.c` (unless D5 needs it, then `architector` decides first), the ISR files,
`buttons.c`, `keyer.c`, `player.c` and `firmware/drivers/`.

### 17.6 Change D-T45 to D-T47: start screen with a big level description, split end screen

User decisions 2026-10-15. Plan `lucky-seeking-locket.md` approved by the user. D6 done, review `approved` on
2026-10-15 (D-T48). T16 Part A runs. T16 Part B is released: it builds on Part A. Q7 Part A (the QA test files) can
start now. The change is a MINOR behavior change: after the work, the coordinator asks
the user about the version (AGENTS.md).

Review of D6 (2026-10-15): `approved` (D-T48). Notes for T16 Part B:

| ID | Note |
| --- | --- |
| P1 | L1 code group. The private constant `SCREEN_CODE_CHAR_GAP` is 9 px. The first mark of M starts 9 px after the end of the last mark of K. `training_screen_draw_code_mark()` returns the end + 3, so the group code adds 6 to that value. V-137: dash 42-47, dot 51-52, dash 56-61, dash 71-76, dash 80-85. |
| P2 | Labels. `New chars` and `New char` go through `training_screen_draw_centered(2)`, so both start in column 8. One `MEM_CODE` text and an append of `s` is allowed. A word level clears row 2. |
| P3 | Probes in `Tests/support/training_harness.h`, as `ui-ux.md` 9.4 defines them: `LVL` in row 1, `LABEL`, `DESC(l, s)` and `CODE(p)`. In `CODE(p)`, a space is the 9 px gap. Also `OKBTN`, `STARTBTN`, `BLANK(r)` and `DIRTY`. |
| P4 | Developer tests: every V-vector that D6 revised, and V-171. Among them V-105 to V-108, V-118, V-121, V-136 and V-137 to V-151. V-151 walks every start screen from L1 to L65. |
| P5 | Revised by D-T49 (1): the `enter` on the end screen dirties only the pages that change. The next `enter` starts the session, and page 0 stays clean (V-105, V-146). |
| P6 | Revised by D-T49 (1): a step dirties only the pages that change, inside pages 0-5. A Receiving step never dirties page 5. |

| Task | Owner | Content | Depends on |
| --- | --- | --- | --- |
| D6 | `ui-ux-embedded-designer` | Done, review `approved` (D-T48). `ui-ux.md` 5.15, 5.16.1 to 5.16.4, 6.1, 6.8, 6.9, 6.10 and 9.4, new decisions from UX-65. Start screen of D-T45: the selector in row 1 and the label texts of row 2 for each level kind. Also the big line for each level kind (L1 position of K and M), the Sending code row 5 and the L1 code gap. End screen: rows 1-3, rows 4-6 blank, the band label and its position. Controls of the end screen (D-T45 (4), section 12.6). The redraw budget of SL-4 and SL-10. Revised V-vectors: V-01, V-19, V-105 to V-108, V-121, V-136 and V-137 to V-151. Also every other vector with an end screen or a start screen. New V-vectors from V-171. Device checks. All within SL-1 to SL-10 and B-1 to B-6. A need outside these limits goes to `architector`. | - |
| T16 | `c-embedded-developer` | Done, final review `approved` after C1 and C6 (section 22.6). Part A: the engine and the helper move, with no visible change. Part B: the screens and the modes (section 17.6.1). | Part A: -. Part B: D6 review, Part A |
| Q7 | `qa-embedded` | Done, `approved` after C3 and C5 (section 22.6). Part A: update the `test_qa_*` files of section 15.3 to the contract, after the D6 review. Part B: independent verification of AC-T26 and AC-T40 to AC-T47 after T16 Part B. Golden pixel dumps of the new start screen and end screen in `test_qa_receiving_level_select.c`. Fault and mutation cases below. Verdict `approved`, `changes_requested` or `blocked`. | Part A: D6 review. Part B: T16 |
| W7 | `technical-writer` | Ready (T16 `approved`). `docs/user_manual.md` and `docs/quick_guide.md`. The start screen: the selector at the top, the big level text, and in Sending the CW code of the new character for every `Show CW Code` value. The end screen: score and result only. The two presses from the end screen: `enter` opens the start screen at the preselected level, and a second `enter` starts. Every text that says the end screen has a selector, or that `enter` there starts the next session. `docs/release_notes.md`: one entry, after the user decides the version. | `architector` approval of T16 |

Q7 fault cases:

| ID | Case |
| --- | --- |
| F1 | A tip closure on the end screen (paddle and straight key), and a plug insertion there: the start screen shows, no session starts. A second closure starts the session (R-T10). |
| F2 | `next` held in END, then `enter`: the repeats step on the start screen (B-3). |
| F3 | `prev` or `next` held from the last RESULT across END and across the reopen (B-1). |
| F4 | A long `menu/back` on the end screen and on the reopened start screen. |
| F5 | Sending: a leave from the reopened start screen. No `keyer_stop()`, the `enter` state stays. |
| F6 | PASS_NOT_SAVED: the end screen shows `Not saved`. The reopened start screen shows the new level from RAM. |
| F7 | PASS_TOP at index 65535: the reopened start screen shows `L65535 of 65535` with no `>`. |
| F8 | A leave from END and a new entry: the start screen shows the earned level, not the preselection. |
| F9 | Auto-repeat from L1 to the earned level and back on the start screen in Sending: every frame equals a full render (AC-T42). |

Q7 mutation cases:

| ID | Mutant |
| --- | --- |
| M1 | `training_select_step()` still acts in END. |
| M2 | `training_reopen_select()` resets the selected level to the earned level, or to the session level. |
| M3 | `training_reopen_select()` returns true in SELECT, so `enter` never starts. |
| M4 | The END `enter` returns START (the old behavior). |
| M5 | The reopen does not redraw row 0, so the header keeps the session that ended. |
| M6 | `training_screen_draw_end()` still calls `training_screen_draw_start()`. |
| M7 | The start screen big line uses `training_item_length()` of the last session. |
| M8 | The Sending code row follows `Show CW Code`. |
| M9 | Receiving shows the code row. |
| M10 | The L1 code group has no character gap, or it is not centered. |
| M11 | The code group centering is off by one pixel, so the Sending row 5 of FAILED changes. |
| M12 | The live mark of row 7 does not start at x 2, or its gap changes. |
| M13 | A step from a character level to a word level in Sending leaves the old code group in row 5. |
| M14 | A step redraws page 6 or 7. |

#### 17.6.1 Scope of T16

| Part | Area | Files | Content |
| --- | --- | --- | --- |
| A | Engine | `inc/modes/training.h`, `src/modes/training.c` | `bool training_reopen_select(void)` as section 5.10 and 11.2 (D-T46). `training_select_step()` keeps its END case in Part A, so every existing test passes. Tests: AC-T40 (2) to (4) in `test_training.c`. |
| A | Code helpers | `src/modes/training_screen_internal.h`, `src/modes/training_screen.c`, `src/modes/sending.c` | `training_screen_draw_code_mark()`, `training_screen_draw_code_group()` and `TRAINING_SCREEN_CODE_GROUP_ROW` (section 5.11). The mark code and constants leave `sending.c`. The static `screen_draw_big_text()`, used by `training_screen_draw_item_text()`. No pixel changes. Tests: AC-T46. All existing tests pass with no change. |
| A | Report | - | The 14.2 table before and after Part A. Part A target: at most +40 B flash, +0 B xdata. |
| B | Engine | `inc/modes/training.h`, `src/modes/training.c` | `training_select_step()` acts only in SELECT. The comments of `TRAINING_PHASE_END`, `training_start()` and `training_select_step()` as section 5.10. Tests: AC-T40 (1) and (5), and the 15.3 changes of `test_training.c`. |
| B | Shared screens | `inc/modes/training_screen.h`, `src/modes/training_screen.c` | `training_screen_draw_start()`, `training_screen_draw_select()`, `training_screen_draw_end()` and `training_screen_select_input()` as section 5.11, SL-1 to SL-10 and `ui-ux.md` (task D6). One static function for rows 1-5 of the start screen. One band helper with a label parameter. The doc comments of section 5.11. |
| B | Modes | `src/modes/receiving.c`, `src/modes/sending.c` | `receiving_handle()` and `sending_handle()` lose the `if (phase == TRAINING_PHASE_END)` header branch at START (D-T47 (4)). No other mode change: the END events, the leave paths and the render functions stay. |
| B | Test support | `Tests/support/training_harness.h` | The probes of 15.3 and of the D6 notation in `ui-ux.md` 9.4. |
| B | Tests | `test_training.c`, `test_receiving.c`, `test_sending.c`, `test_menu_training.c` | AC-T41 to AC-T45, the D6 V-vectors, and the 15.3 changes of developer files. |
| B | Report | - | AC-T47: the 14.2 table before and after, the flash per changed file, the deepest chain by name, the `check_asm.sh` result, the `pre-commit` result. A list of the `test_qa_*` tests that fail only for a 15.3 reason, each with its reason. |

Not in T16: a change of the ISR files, `buttons.c`, `keyer.c`, `player.c`, `koch`, `progress`, `menu` or
`firmware/drivers/`. T16 does not change `test_qa_*` files (D-T43 (4)). If Part A changes an interface of section 5,
Part B stops and reports to `architector`. If D6 needs a behavior outside SL-1 to SL-10 or B-1 to B-6, the designer
reports to `architector` first.

Exit rule. T16 hands over with `build.sh`, `check_asm.sh` and `pre-commit` passing. In `test.sh`, only `test_qa_*`
tests that the 15.3 changes cover can fail. Q7 Part A updates them. Q7 ends with `test.sh` passing in full.

## 18. Device checks (unverified until the user tests on the device)

| ID | Check |
| --- | --- |
| DC-T1 | Record the Receiving playback at 20/20 and 50/50 WPM and at 20/10 Farnsworth. Element and gap lengths match section 9.2 within the oscillator trim. No click at mark edges. |
| DC-T2 | Sending: the blank before the next item is visible, also for a repeated character. The 8 x 16 text is readable. |
| DC-T3 | Receiving: the four group labels sit above `menu/back`, `prev`, `next` and `enter` in this order (D-T27). |
| DC-T4 | Tap mode. A tap feels prompt. A long `prev` undoes with no visible group change first. A long `enter` replays with no pick first. |
| DC-T7 | Receiving with a paddle and a straight key on the tip: a short closure selects the `enter` run, a closure of 800 ms replays. Record whether a plug insertion selects a run. |
| DC-T5 | Progress survives a power cycle. `Reset Statistics` gives level 1 after a power cycle. |
| DC-T6 | The main loop keeps up: no keyer queue overflow during the Sending redraw at 50 WPM. |
| DC-T8 | `ui-ux.md` D-34. Sending word session with a paddle and with a straight key, at 20/20, 13/13 and 20/10 WPM: words keyed with the normal 3-unit space pass. A pause of about 6 units after a character gives `Wrong: pause too long`, a pause of about 4 units does not. At the retry, rows 5 and 7 are blank. |
| DC-T9 | D-T35 to D-T41. In both modes: the selector steps feel right with auto-repeat (400 ms, then every 80 ms), from L1 to an earned level above L50. `enter` and the paddle tip start a session from the start screen and from the end screen. Record whether a plug insertion on a selector screen starts a session (R-T10). The main menu shows the level next to `Receiving` and `Sending`. |
| DC-T10 | D-T44. Receiving: after the third wrong attempt, wait 10 s. The answer stays on the screen, and row 7 shows only `Next`. A long `enter` and a tip closure of 800 ms play the answer again. An `enter` tap and a short tip closure go on to the blank moment. On the last item, row 7 shows `Finish`, and an `enter` tap shows the end screen. Record whether a plug insertion in the answer state goes on (R-T12). |
| DC-T11 | D-T45 to D-T47, both modes, `ui-ux.md` D-42 to D-44. On the start screen, the 8 x 16 level text and the Sending code marks are clear on the glass: L1, a character level, the AR level with `<AR/+>`, the level of `,`, and a word level. During auto-repeat, a step redraws pages 0-5. The last level shows within 100 ms after the release (`ui-ux.md` UX-16, D-35). Record the time of one step redraw and of the full redraw from the end screen to the start screen. On the end screen, `enter` and a tip closure show the start screen, and a second press starts the session. |

## 19. Risks

| Risk | Mitigation |
| --- | --- |
| R-T1: a power loss or a write failure during a progress save loses all stored levels (one page, erase first). | Magic last, so the next start sees no record and starts at level 1, never with wrong levels. The result screen shows `Not saved`. If the user wants it, a two-page scheme is a follow-up. |
| R-T2: the third ISR call pushes the worst ISR path over 440 clocks. | The keyer and the player never run together. Report the three paths. Above 440 clocks, stop and report. |
| R-T3: the 8 x 16 font draw is slow or deeper on the stack. | Stack report of section 14 and DC-T6. |
| R-T4: SDCC turns the player shifts or the 16-bit compares into a helper call. | R6 rejects any call in a service. |
| R-T6: tap mode adds the press duration to every Receiving answer. | U-11 and DC-T4. |
| R-T7: the paddle tip shares P3.2 with `enter`. In Receiving, a short closure selects the `enter` run. A plug insertion can do the same (D-T27). When that run has one character and the pick fills the last slot, the check comes at once and uses an attempt. Long `prev` cannot undo a check. | Accepted by the user. A narrowing or a pick in an earlier slot can be undone with long `prev`. The user manual (W2) tells the user to insert the plug before opening Receiving. DC-T7. |
| R-T8: the four labels do not fit one row for every run (LC-6). | D1 runs the script check over all Koch prefixes. LC-2 allows a second row. |
| R-T10: on the start screen and the end screen, tap mode is off, so `enter` acts at the press. A tip closure or a plug insertion there starts a session (D-T39, D-T40). | A session that the user leaves changes no level. A long `menu/back` leaves in every phase. W5 tells the user to insert the plug before opening a mode. DC-T9. Revised by D-T45: on the end screen, a closure only opens the start screen. Only a closure on the start screen starts a session. |
| R-T11: at 12.5 steps per second, a far level takes time. From L1 to L150 takes about 12 s. | The selector opens at the earned level, so a replay is usually a few steps down. D-T35 (6) keeps the fallback (long `prev` or `next` steps by 10) for an `architector` decision after DC-T9. |
| R-T12: in the answer state of D-T44, a short tip closure or a plug insertion continues to the next item. | The answer was already on the screen, and the item already counts as wrong. W6 and the existing R-T7 text tell the user to insert the plug before opening Receiving. DC-T10. |
| R-T13: in the answer state, row 7 shows `Next` above `enter`. A user can read it as the name of the `next` button and press `next`. | A `next` tap does nothing there, so no wrong action and no lost point follow. The `enter` label position is the same as in phase ITEM. W6 says "tap `enter`". DC-T10 and `ui-ux.md` D-41 record whether users press `next`. If they do, `ui-ux-embedded-designer` proposes a new text and `architector` decides. |
| R-T14: a step on the start screen redraws pages 0-5 (before: pages 0, 4 and 5), with an 8 x 16 text and code marks. During auto-repeat (80 ms), the screen can skip more levels. | No step is lost: the engine steps at each event, and the last level shows after the release. DC-T11 measures the time. Fallback order, decided by `architector` with `ui-ux-embedded-designer` after D-43 (D-T48 (7)). (a) The last level shows within 100 ms after the release (UX-16): no change. (b) and (c) come with D-T49 (1): unchanged pages, such as page 5 of Receiving or an unchanged label, are not sent. (d) Else the D-T35 (6) fallback: a long `prev` or `next` steps by 10. |
| R-T9: a slow or uneven fist gets word-space errors at equal character and effective speeds (5 units leave 2 units above the normal space). | User decision (5 units, D-T30). The user can lower `TX Eff. Speed`. W3 tells the user. DC-T8. |

## 20. Answered questions

| ID | Question | User answer |
| --- | --- | --- |
| Q-T1 | The list has 43 entries, the plan says 44. | 43 is correct. The 44 was a counting error. `KOCH_CHAR_COUNT` = 43, final word phase from L = 63 (D-T3). |
| Q-T2 | Prosigns in random words? | No. BT, SK and AR appear only in character sessions. Words and the word keyboard use the first 40 Koch characters. With fewer learned characters, they use the learned ones (D-T4). |

No open question remains for this contract.

## 21. UI reconciliation (`ui-ux.md`, final)

All dependencies are resolved. T12 Part B can start. Fixes F-1 to F-3 of D2 change only wording and add V-135.

Level selection (U-26 to U-31): resolved by D4, review `approved` (D-T42). T14 Part B can start.

| ID | Item | Resolution | Source in `ui-ux.md` |
| --- | --- | --- | --- |
| U-1 | `TRAINING_START_MS` | 1000 ms | 5.14, 6.10 |
| U-2 | `TRAINING_RECEIVING_GAP_MS` | 500 ms | 5.14, 6.10 |
| U-3 | `TRAINING_SENDING_GAP_MS` | 300 ms | 5.14, 6.10 |
| U-4 | `TRAINING_CORRECT_MS` | 600 ms | 5.14, 6.10 |
| U-5 | `TRAINING_WRONG_MS` | 1200 ms | 5.14, 6.10 |
| U-6 | `TRAINING_FAILED_MS` | 2500 ms, Sending only. Receiving has no delay after the third wrong attempt: the answer stays until an `enter` tap (D-T44). | 5.14, 6.10, Q-14, UX-64 |
| U-7 | Header | Tag, speed from column 3, level from column 9, item right-aligned to column 24. No short form. In a gap the header shows the next item number (`training_shown_item_number()`). | 5.11.1 |
| U-8 | Receiving layout | Block and bar marks, slot x `68 - 8n + 16k`, key bar segments at y 55 (D-T29), label fields and label texts. | 5.11.1, 5.11.2, 5.12, 5.12.1 |
| U-9 | Sending layout | Item line, bar under the next word character, status `Wrong: <text>`, code row 7, answer code row 5. `TX_CODE_MAX_MARKS` = 12. | 5.13, 5.13.3 |
| U-10 | End screen | Revised by U-28 (D-T42). Before: score, `Passed` or `Not passed, need t`, `New level`, `Repeat level` or `Top level` (D-T17), `Not saved`, focused `Next`. | 5.15 |
| U-11 | Tap mode | Adopted for the whole Receiving screen, end screen included. | 6.8 |
| U-12 | Long `prev` | One step: step out one level, else undo the last pick, else nothing. | 6.8 |
| U-13 | Replay on RETRY | At once, with empty slots. | 5.12.2 |
| U-14 | Picks during playback | Allowed. A check stops the playback. | 5.12.2 |
| U-15 | Sending retry | The whole item again (D-T6). | 5.13.1 |
| U-16 | Score | First-attempt answers only (user decision). | 5.15, 12 |
| U-17 | Failed reset | `Not saved` over Settings. `OK` returns to Settings (D-T11, D-T19). | 5.7, F-23 |
| U-18 | Long `next`, repeats | Nothing on both training screens. | 6.8, 6.9 |
| U-19 | Shown level | `L + 1`, saturated at 65535 (D-T5). | 5.11.1 |
| U-20 | Sending long `prev`, long `next` | Nothing. | 6.9 |
| U-21 | Sending keyer events in delays | Dropped-character rule (D-T15). OVERFLOW uses no attempt (D-T16). | 5.13.2, Q-15 |
| U-22 | Word keyboard | No prosign (D-T4, `keyboard_reset(training_alphabet_count())`). | 5.12.1, V-102 |
| U-23 | Four-run keyboard, tap `enter`, long `enter` replay, labels (D-T27, D-T28) | Resolved by D1 (UX-51) and D-T29. | 3.1, 5.12, 5.15, 6.1, 6.2, 6.4, 6.6, 6.8, 9.4 |
| U-24 | Sending retry screen (D-T30) | Rows 5 and 7 off, `Try N of 3` kept, bar at the first character (WS-6). Resolved by D2 (UX-52). | 5.13.1, 5.13.3, 5.14 |
| U-26 | Start screen layout (D-T35, D-T38) | Resolved by D4 (UX-53): selector in row 4, description in row 5, rows 1-3 and 6 blank, `Start` band in row 7 (cells 9-15, x 47-81). Meets SL-1 to SL-5. | 5.16.1 |
| U-27 | Header in SELECT and END (SL-6) | Resolved by D4: in SELECT the level field shows the selected level and the item field shows `1/<n>`, `n` = `koch_item_count(training_selected_level())`. A step redraws row 0. In END the header of the session that ended stays, also while the selection changes. A start from SELECT leaves row 0 as it is (section 12.2). | 5.11.1, 5.16.1, 5.16.4 |
| U-28 | End screen with the selector, PASS_REPLAY text (SL-7) | Resolved by D4 (UX-56). Row 1: `Not saved`. Row 2: the score. Row 3: `Passed, new level` (PASS, PASS_NOT_SAVED), `Passed` (PASS_REPLAY, PASS_TOP) or `Not passed, need <t>` (FAIL). Row 4: the selector. Row 5: the description. Row 7: the `Start` band. | 5.15 |
| U-29 | Description texts (SL-2, SL-3) | Resolved by D4 (UX-54): `K and M`, `New char <full text>` in the `Prosigns` style (longest 15 cells), `Words of <len>` also for the final word phase. | 5.16.3 |
| U-30 | Controls on the selector screens (D-T38, D-T39, D-T40) | Resolved by D4 (UX-55), matches D-T38 to D-T40. `next` up, `prev` down, press and repeat, no wrap. `enter` and the tip start at the press. `menu/back` leaves at the press. Long events do nothing. | 6.1, 6.8, 6.9, 5.16.4 |
| U-31 | Main menu level column (D-T41) | Resolved by D4 (UX-57): right-aligned, last character in column 22, the video of a value. | 5.2, 5.16.6 |
| U-32 | Receiving answer state (D-T44) | Resolved by D5 (UX-64), review `approved` on 2026-10-15. Row 7: only the `enter` field, right-aligned as every `enter` label: `Next` at x 107, or `Finish` at x 97 on the last item. The other three fields are blank, and the key bars stay. Events of 6.1 and 6.8 match D-T44 (4). New vector V-170, revised V-99, V-121 and V-136. Device check D-41 (DC-T10). | 5.12.2, 5.14, 6.1, 6.8, 6.10, 7, 8 (F-20), 9.4 |
| U-33 | Start screen with a big level description, split end screen (D-T45 to D-T47) | Resolved by D6 (UX-65 to UX-70), review `approved` on 2026-10-15 (D-T48). Labels `New chars` and `New char` (column 8). Big line at `x = 64 - 4 m`. L1 code gap 9 px. End screen band `OK`, x 52-71. Steps redraw pages 0-5 in both modes. New vector V-171, device checks D-42 to D-44. | 5.15, 5.16, 6.1, 6.8, 6.9, 6.10, 9.4, UX-65 and up |
| U-25 | Sending word-space result (D-T30, D-T31) | `Wrong: pause too long`, bar under the next character, row 5 at FAILED the code of that character, row 7 kept (WS-1 to WS-5). Resolved by D2 (UX-52). The arming condition text waits for fix F-1. | 5.13.1, 5.13.2, 5.14, 9.4 |

## 22. Final review (2026-10-06)

Verdict: `approved`. QA verdict: `approved` (host level).

| Item | Result |
| --- | --- |
| `test.sh` | 70 of 70 host tests pass with ASan and UBSan. `check_asm.sh` self-test: 168 of 168. Run again by `architector`. |
| `build.sh` | Passes. `check_asm.sh` reports no violation. |
| ISR | `sidetone_timer0_isr` has exactly `lcall _keyer_service`, `lcall _player_service`, `lcall _sidetone_service`, then `reti`. `_player_service` has no call. No `sdcc_atomic` text in the build. `accept/real_isr/sidetone.c.asm` equals the build output. |
| Memory | Flash 19 055 -> 26 593 B, xdata 1400 -> 1811 B, DSEG 43 -> 47 B, stack 191 B free, margin 108 B. Above target and below the hard limits, accepted (D-T23). |
| ISR time | Worst paths: keyer 403, player 186, disabled 63 clocks. |
| Contract | Engine, player, keyboard, progress, tap mode, Receiving, Sending and menu wiring match sections 5 to 12 and D-T14 to D-T26. |
| QA | 70 mutants: 55 killed, 9 equivalent, 6 gaps closed by `test_qa_receiving.c`, `test_qa_sending.c` and `test_qa_player.c`. No product defect. |

Open: device checks DC-T1 to DC-T6 and D-26 to D-33 of `ui-ux.md`. They stay unverified until the user tests on the
device. Risk R-T1 stays accepted. Next: `technical-writer` writes the user manual section for the two modes.

### 22.1 Final review of D-T27 to D-T29 (2026-10-06)

Verdict: `approved`. QA verdict (Q2): `approved` (host level).

| Item | Result |
| --- | --- |
| `build.sh` | Passes. Run again by `architector`. `check_asm.sh` reports no violation. The Timer 0 ISR still has exactly the three calls of D-T1. |
| `test.sh` | 71 of 71 host tests pass with ASan and UBSan. `check_asm.sh` self-test: 168 of 168. Run again by `architector`. |
| Memory | Flash 26 593 -> 26 642 B (+49, target +300). xdata 1811 -> 1810 B (-1). DSEG 47 B. Stack 191 B free, deepest main-loop chain 65 B, ISR frame 18 B, margin 108 B. Within the T11 budget. |
| Contract | `keyboard` matches section 8 (closed form, history of 2). `buttons` matches D-T28. Tap mode covers four buttons, and long `enter` (0x44) comes in tap mode only. One order table gives MENU, ENTER, PREV, NEXT. Both `buttons_set_enter_enabled()` values clear the long-pending ENTER. `receiving.c` matches 12.2, LC-1 to LC-7 and D-T29. `RX_GROUP_OF()` has a `_Static_assert` for all four codes. The fields start at x 2, 30, 58 and 87. Every label uses the short text. The key bars come only in a full render. |
| Tests | AC-T8, AC-T15, AC-T19, AC-T23 (V-90 to V-127) and AC-T27 have tests. Tap-off button tests are unchanged. QA added `test_qa_koch_keyboard.c` and tap `enter` cases, and rewrote `test_qa_receiving.c` (label fit walk, R-T7 glitch cases). 17 mutants killed. |

Open, not blocking:

- Mutant M12 (undo the last pick before stepping out of a group) survives `test_receiving.c`, because V-94 cannot tell the two orders apart. `test_qa_receiving.c` kills it, so the suite covers the rule. A developer case is optional.
- Device checks DC-T3, DC-T4 and DC-T7 stay unverified until the user tests on the device.
- W2: the user manual (R-T7: insert the plug before you open Receiving).

### 22.2 Final review of D-T30 to D-T32 (2026-10-07)

Verdict: `approved`. QA verdict (Q3): `approved` (host level).

| Item | Result |
| --- | --- |
| `build.sh` | Passes. Run again by `architector`. Flash 26 740 B, xdata 1811 B, 191 B stack free. `check_asm.sh` reports no violation. |
| `test.sh` | 72 of 72 host tests pass with ASan and UBSan. `check_asm.sh` self-test: 168 of 168. Run again by `architector`. |
| Memory | Flash +98 B (target +150), xdata +1 B (target +1), DSEG, overlay and bit bytes +0. Stack bound 65 -> 66 B, margin 107 B, accepted (D-T32). |
| Contract | `TRAINING_CHAR_WORD_SPACE` in `training.h`. `sending.c`: every event clears the armed flag, only a PARTIAL CHAR_GAP sets it, a WORD_GAP with the flag set in ITEM gives `training_add_char(' ')`, `tx_start_session()` clears it. The armed flag implies a clear drop flag, so the early WORD_GAP branch skips no needed drop step. RETRY clears rows 5 to 7. `training_screen.c` shows `Wrong: pause too long` for a last answer character `' '`. Bar and row 5 use answer length - 1. No change in `training.c`, the keyer or the ISR. |
| Tests | AC-T17 (word space), AC-T28 (1) to (6), V-112, V-113, V-115, V-116, V-121, V-128 to V-135, V-133 part B in `test_keyer.c`. QA: `test_qa_sending_word_space.c`, 21 mutants, 17 killed, 4 equivalent (the phase check and the second row 5 clear), the armed-flag versus answer-length mutant killed. |
| `ui-ux.md` | UX-52 with fixes F-1 to F-3 and V-135 in place. |
| User manual (W3) | `approved`. The pause limit, the no-effect cases, the timing table, the retry screen and the trouble row match D-T30, D-T31 and UX-52. |

Open, not blocking: device checks DC-T8 (`ui-ux.md` D-34) and DC-T6 (the extra page flush at ITEM and RETRY). They
stay unverified until the user tests on the device. R-T9 stays accepted.

### 22.3 Final review of D-T33 and D-T34 (2026-10-07)

Verdict T13: `approved`. Verdict W4: `approved`. QA verdict (Q4): `approved` (host level).

| Item | Result |
| --- | --- |
| `build.sh` | Passes. Run again by `architector`. Flash 26 781 B, xdata 1812 B, 191 B stack free. No `check_asm.sh` violation. |
| `test.sh` | 73 of 73 host tests pass with ASan and UBSan. `check_asm.sh` self-test: 168 of 168. Run again by `architector`. |
| Memory | Flash +41 B (target +40, hard limit +100, accepted by D-T34). xdata +1 B (target +1). DSEG, overlay and bit bytes +0. Margin 107 B. |
| Contract | `koch_pass_threshold(koch_kind_t kind, MEM_XDATA uint8_t item_count)` as section 5.6. Word kind 4/5, every other kind 9/10, `(p n + p) / q` in `uint16_t`. `training_start()` passes `koch_kind(level)`. No other production change. |
| Tests | AC-T6 (n 0..255 against a counting reference), AC-T18 (`test_finish_thresholds`, a final level of 50 words gives 40), AC-T29 (V-121 `need 80`, V-136 in both modes). QA: `test_qa_sending_pass_rate.c`, levels 0..170 and 65533..65535, menu boundaries at 10, 11, 50 and 100 words and 50 characters, EEPROM write only on a pass, 14 of 14 mutants killed. |
| `ui-ux.md` (D3) | Line 1180 and the user decision record state 80 % for word sessions, 45 of 50 for characters. |
| User manual (W4) | The pass rules in `docs/user_manual.md` state 45 of 50 characters (90 %) and 8 of 10, 80 of 100 words (80 %, rounded up), for every word session. No other text states a word rule. |

Open, not blocking: the rounding rule for the final levels is an assumption that the coordinator confirms with the
user. Device checks from earlier changes stay open.

### 22.4 Final review of D-T35 to D-T43, task T14 (2026-10-07)

Verdict T14: `approved` on 2026-10-07, after fix F1. The first pass gave `changes_requested` for F1. QA verdict
(Q5): `approved` (host level), also for F1.

| Item | Result |
| --- | --- |
| `test.sh` | 75 of 75 host tests pass with ASan and UBSan. `check_asm.sh` self-test: 168 of 168. Run again by `architector`. |
| `build.sh` | Passes. Run again by `architector`. `check_asm.sh` reports no violation. |
| Memory | Flash 26 781 -> 27 570 B (+789, target +800, hard limit +1200). xdata 1812 -> 1814 B (+2, target +2). DSEG, overlay and bit bytes unchanged. Stack starts at 0x41, 191 B free. Summed-push bound 66 B, ISR frame 18 B, margin 107 B. |
| ISR and protected files | No change in `sidetone.c`, `keyer_service.h`, `player_service.h`, `keyer.c`, `player.c`, `buttons.c` or `firmware/drivers/`. |
| Engine | `training_prepare()`, `training_start(void)`, `training_select_step()`, `training_selected_level()`, `training_earned_level()` and the finish rule match section 5.10, 11.2 and 11.4. The finish test is `!=`. `koch_shown_level()` replaces `training_shown_of()` (D-T37). The fail branch leaves the selected level as it is (D-T43 (3)). |
| Screens | `training_screen_select_input()` ignores long events and acts on repeats of `prev` and `next` only. The selector line and the description match `ui-ux.md` 5.16.2 and 5.16.3. A step redraws rows 4 and 5, and row 0 in SELECT only. The end screen has the rows of SL-7 and one `Start` band. The old end screen texts are gone. |
| Modes | Receiving: tap mode on in `rx_start_session()` and off at the END event. Sending: no keyer at the entry, and `sending_leave()` stops it only in GAP, ITEM and RESULT. A start from SELECT does not redraw row 0. |
| Main menu | `L<n>` right-aligned to column 22, in the label video, from the speed of the mode. The compile-time checks of D-T41 are present. |
| Tests | AC-T30 to AC-T35. QA: `test_qa_training_select.c`, `test_qa_receiving_level_select.c` (pixel goldens), 36 mutants, 34 killed, M33 and M34 equivalent. |

Change request for `c-embedded-developer`:

| ID | Change |
| --- | --- |
| F1 | `src/modes/sending.c`, END branch of `sending_poll()`: call `buttons_set_tap_mode(false)` (D-T43 (1)). Test in `test_sending.c`, the Sending form of V-144: a `TX(...)` session, `next` pressed 100 ms after the last keyed character and held for 2000 ms across END. At the end screen: no step, no repeat and no long event act, `DIRTY()`. After the release, a new `M` steps once. The same with `prev`. Run `test.sh` and `build.sh`. Report the flash: up to +820 B is accepted for this change. No other change. |

F1 result. `sending_poll()` END branch: `keyer_stop()`, `buttons_set_enter_enabled(true)`,
`buttons_set_tap_mode(false)`, then the end screen. `test_v144_held_through_end` in `test_sending.c` covers `next`
and `prev`: the hold through END gives no step and no dirty page, and a new press steps once (pages 4 and 5). QA
killed the mutants "call removed" and "tap mode true". The three order mutants are equivalent: the three calls
commute, and no `buttons_poll()` runs between them. Run again by `architector`: `test.sh` 75 of 75, `build.sh`
passes with no `check_asm.sh` violation. Flash 27 576 B (+795, inside the +820 B allowance of F1). xdata 1814 B.
Stack 191 B free, margin 107 B. No protected file changed.

T14 is closed. W5 (`technical-writer`) can start with the scope of section 17.4. Open, not blocking: device check DC-T9 (`ui-ux.md` D-35 to D-37). `ui-ux.md` V-143 can say "no playback
starts" in its next edit (D-T43 (2), owner `ui-ux-embedded-designer`).

### 22.5 Final review of D-T44, task T15 (2026-10-15)

Verdict T15: `approved` on 2026-10-15, after the test fixes R1 and R2. The first pass gave `changes_requested`
for the test file `test_receiving.c` only. QA verdict (Q6): `approved` (host level).

| Item | Result |
| --- | --- |
| `test.sh` | 80 of 80 host tests pass with ASan and UBSan. `check_asm.sh` self-test: 168 of 168. Run again by `architector`. |
| `build.sh` | Passes, with the `check_asm.sh` post-build step. Run again by `architector`. |
| Memory | Flash 28 090 -> 28 209 B (+119, target +120, hard limit +200). xdata 1817 B (+0). Stack starts at 0x41, 191 B free. |
| `pre-commit` | `reuse lint-file` and `clang-format` pass on the seven changed files. |
| ISR and protected files | No change in `sending.c`, `sidetone.c`, `keyer_service.h`, `player_service.h`, `keyer.c`, `player.c`, `buttons.c` or `firmware/drivers/`. |
| Engine | `training_hold_result()` sets the hold from the argument. False sets the duration to 0 and keeps the phase start (D-T44 (2), N2). The phase guard stays. |
| Receiving | `rx_is_answer_shown()` serves `rx_draw_item_line()`, `rx_build_label()` and `receiving_handle()`. `receiving_poll()` has no hold call. GAP redraws the labels. `rx_handle_answer()` acts only on `enter`: long replays, tap stops the player and releases the hold. `Next` and `Finish` are `MEM_CODE` texts on the `enter` label path (N3). The `\|\|` in `receiving_handle()` cannot act twice: an ITEM event that gives FAILED returns true first. |
| Tests | AC-T37 in `test_training.c`. V-170 runs 1-5, revised V-99, V-121 and V-136 in `test_receiving.c`. QA: F1-F5 and extra cases in `test_qa_receiving.c`, M1-M8 (14 variants) killed. The change in `test_qa_sending_pass_rate.c` is in the Receiving branch of a shared helper (15.3). |

Change requests for `c-embedded-developer` (tests only, no production change):

| ID | Change |
| --- | --- |
| R1 | `test_v170_answer_state_waits`: read the player call count before the no-op taps (`L`, `M`, `R`, long `M`, long `R`). Assert the same count after them (AC-T38 (4)). `expect_answer_state_quiet_to()` reads the count after the taps, so a stray player call is not seen. Also check `Next` on item 49 of 50 in `test_receiving.c`. Then the developer file tests both sides of the last-item rule of D-T44 (5). |
| R2 | `run_word_session()`: the inner RESULT/GAP loop checks `guard` but does not increase it. With the new `enter` tap in the loop, a fault that keeps RESULT makes the test loop with no time step (QA mutant M1). Give the inner loop its own bound. At that bound, the test fails with an assertion. The `while (training_phase() == TRAINING_PHASE_ITEM)` loop before it gets the same bound. |

R1 and R2 result. Only `test_receiving.c` changed. The production diff is the one of the first pass.
`test_v170_answer_state_waits` reads the player call count before the no-op taps and asserts it after them. The new
`test_answer_state_next_on_item_49` checks `Next` on item 49 of 50 and the blank moment after the tap, with header
`50/50`. Both inner loops of `run_word_session()` stop at 5000 steps with an assertion. With M1, `test_receiving`
fails by that assertion in about 1 s, with no hang (developer evidence). No new QA run is necessary: the change adds
checks to a developer test file and removes none. Run again by `architector`: `test.sh` passes 80 of 80. The
`check_asm.sh` self-test passes 168 of 168. `build.sh` passes with no `check_asm.sh` violation. Flash 28 209 B,
xdata 1817 B, 191 B stack free. `pre-commit` passes.

T15 is closed. Open, not blocking: DC-T10 and `ui-ux.md` D-41 (R-T13).

Review of W6 (2026-10-15): `approved`. `docs/user_manual.md` and `docs/quick_guide.md` match D-T44 and UX-64.

| Item | Result |
| --- | --- |
| Behavior | The answer shows and plays one time, with no time limit. An `enter` tap continues, also during the playback. A hold of `enter` plays the answer again. A long `menu/back` leaves. The other buttons do nothing. `Next`, or `Finish` on the last item. The end screen comes at the tap after the last item. Sending keeps 2.5 s. |
| Places | The manual: the hold table, the Receiving steps, the replay rules and the tip closures. Also the results table and the troubleshooting table. A new section "After the third wrong attempt" has the screen example of `ui-ux.md` 5.14. The quick guide: the steps and the button table. |
| R-T12 | The plug-insertion note says that a closure in the answer state goes on, and that this costs no point. The troubleshooting row names it. |
| R-T13 | The manual says that `Next` is the label of `enter`, not the name of the `next` button, and that `next` does nothing there. The quick guide says the same. |
| Version | `docs/release_notes.md` and `firmware/VERSION` do not change until the user decides the version. |

Note for the next edit of the manual, not blocking: in the results bullet about the last item, put the action first.
Write: "tap `enter` to show the session end screen."

Next: the version decision of the user (plan: MINOR, 0.1.0 to 0.2.0). Then W6 adds the release notes section.

### 22.6 Final review of D-T45 to D-T48, task T16 (2026-10-15)

Verdict T16: `changes_requested` (C1 to C3). QA verdict (Q7): `approved` (host level).

| Item | Result |
| --- | --- |
| `test.sh` | 81 of 81 host tests pass with ASan and UBSan. `check_asm.sh` self-test: 168 of 168. Run again by `architector`. |
| `build.sh` | Passes. Run again by `architector`. `check_asm.sh` reports no violation. |
| Memory | Flash 28 209 -> 28 548 B (+339, target +250, hard limit +400). xdata 1817 -> 1818 B (+1, target +0, hard limit +2). Stack starts at 0x41, 191 B free. Deepest chain 71 B, margin 102 B (developer figure, checked in part by `architector`). Accepted by D-T49 (2) to (4). |
| ISR and protected files | No change in `sidetone.c`, `keyer_service.h`, `player_service.h`, `keyer.c`, `player.c`, `buttons.c`, `koch`, `progress`, `menu` or `firmware/drivers/`. |
| Engine | `training_select_step()` acts only in SELECT. `training_reopen_select()` acts only in END, keeps the selection and reads no tick. It matches D-T46. |
| Screens | `screen_draw_big_text()` is the one 8 x 16 path. The code group formula gives `x = (128 - w) / 2` for one and two patterns, with the 9 px gap. The band derives its cells from the label length (SL-5). The end screen no longer calls the start screen. START comes only from SELECT. The forced dirty pages break D-T49 (1). |
| Modes | `receiving_handle()` and `sending_handle()` lost the END header branch. `sending.c` uses the shared code helpers, and its own mark code is gone. |
| Tests | AC-T40 to AC-T47 have tests. QA: `test_qa_sending_code_group.c`, `qa/qa_training_screens.h`, golden pixel dumps, 19 mutants killed. |

Change requests:

| ID | Owner | Change |
| --- | --- | --- |
| C1 | `c-embedded-developer` | `src/modes/training_screen.c`: remove the `lcd_fb_mark_dirty()` loop from `training_screen_draw_select()` and the `lcd_fb_mark_all_dirty()` call from the END branch of `training_screen_select_input()`. With no other use, the `lcd/lcd_fb.h` include goes too. Developer tests: a step and the switch from END to SELECT expect the pages whose bytes change. The test computes them from a copy of the frame before the event. A step must never dirty page 6 or 7. A Receiving step must never dirty page 5. Run `build.sh`, `test.sh` and `pre-commit`. Report the flash. |
| C2 | `ui-ux-embedded-designer` | Change `ui-ux.md` UX-70 and the redraw budget table of 5.16.4. Also change every vector with `DIRTY(0,1,2,3,4,5)` or "all pages dirty" for a step or for the switch, for example V-105, V-108, V-138, V-142 and V-145. The new rule: a step dirties only the pages that change, inside pages 0-5. The switch dirties only the pages that change. Give the expected masks, or the rule "the changed pages" with a bound. |
| C3 | `qa-embedded` | The `test_qa_*` files: the same change of the dirty expectations as C1. A mutant that marks a page dirty by hand must fail. Then the Q7 verdict again. |

C2 comes first, because C1 and C3 test against its vectors. `architector` updates SL-4, SL-10 and the notes P5 and P6.

C1 to C3 result (2026-10-15). C2: `ui-ux.md` UX-70, 5.16.4 and the vectors give the changed pages. C1: no page is
marked dirty by hand. The label and the big line are byte-stable (D-T50 (2)). C3: QA golden frames match every page
set of the designer, and 23 mutants are killed, 5 of them hand-marking mutants. One expectation fails: the step
between indexes 65534 and 65535 gives `DIRTY(0,1)`. D-T50 (1) accepts it. Flash 28 561 B, xdata 1818 B, stack
starts at 0x41 with 191 B free.

Follow-ups of D-T50:

| ID | Owner | Change |
| --- | --- | --- |
| C4 | `ui-ux-embedded-designer` | UX-70 and the 5.16.4 table: one note for the exception of D-T50 (1). Page 0 is dirty at every step, also at the step between L65535 (index 65534) and L65535 (index 65535). |
| C5 | `qa-embedded` | The one expectation of the saturation step changes from `DIRTY(1)` to `DIRTY(0,1)`, with a reference to D-T50 (1). Then the Q7 verdict again. |
| C6 | `c-embedded-developer` | No code change. Deliver the C1 report: the flash and xdata before and after C1, and the place of `screen_big_length` in xdata. Also the reason for the +13 B, and the `build.sh`, `test.sh` and `pre-commit` results. |

After C4 to C6, `architector` gives the final verdict.

C4 to C6 result (2026-10-15). C4: `ui-ux.md` UX-70 and the 5.16.4 table note the page 0 exception, and V-148 run 2
expects `DIRTY(0,1)`. C5: QA `approved` (host level). The helper `saturation_step()` cites D-T50 (1), and every other
step check stays strict. 23 mutants are killed on the current code. C6: no code change after C1. The cache byte
`screen_big_length` replaces `screen_is_receiving`, so xdata stays at 1818 B. A byte-stable header cost 35 B and was
reverted, as D-T50 (1) asks.

Verdict T16: `approved` on 2026-10-15. Run again by `architector`: `build.sh` passes, and `check_asm.sh` reports no
violation. `test.sh` passes 81 of 81, and the `check_asm.sh` self-test passes 168 of 168. Flash 28 561 B (+352, hard
limit +400, D-T49 (2), D-T50 (2)). xdata 1818 B (+1, hard limit +2). The stack starts at 0x41 with 191 B free. The
deepest chain is 71 B and the margin is 102 B. No protected file changed.

T16 is closed. W7 (`technical-writer`) can start with the scope of section 17.6. The release notes entry waits for
the version decision of the user (proposal: MINOR). Open, not blocking: the device checks DC-T11 and `ui-ux.md` D-42
to D-44, the ring on the end screen, and B-5 on the device. Do not commit `firmware/Tests/tools/__pycache__/`.
