# Status

- Completed: Steps 0.1 through 3.4.
- The dockable statistics pane now shows project, character, and scene/label totals.
- Character rows keep the 3.3 swatch, percentage, and balance bar, stably sorted by descending words; scenes retain parser first-seen order and empty labels are omitted from the UI.
- Project, character, and scene targets support word and line goals; blank, invalid, zero, and negative values mean unset.
- Targets persist in `targets.ini` next to `settings.json` (manually resolved XDG dir — `GetUserDataDir` returns legacy `~/.say-count`, same trap as Step 1.4) and reload on launch.
- Progress reports unset, remaining, reached, or over-target state; bars and displayed percentages cap at 100%, matching JavaScript.
- Core scene aggregates preserve `No label`, qualify local labels, retain empty labels, and merge project labels in file order.
- Core remains wx-free; dump JSON schema and `.rpy` parsing behavior are unchanged.
- Review fixes (2026-07-11): UTF-8 `·` composed via `std::string`+`FromUTF8` (locale pitfall again); restored 3.3 swatch/percent/balance rows; `wxWeakRef` guards + change-detection in target save handlers; targets map no longer polluted by `operator[]`; persistence path fixed (was silently failing to write).
- Verified (2026-07-11): build + ctest 44/44, parity diffs empty; live GUI — sections render, `start.beach` qualified, over/left/reached/unset target states correct, live edit updates panel, target persists across relaunch.
- Known issues: pre-existing (Step 1.3 era, not 3.4) — one SIGSEGV core dump shows `EditorNotebook::ConfirmCloseAll` crashing at `SetSelection` when quit is re-triggered while the discard-changes modal is open; not reproducible in normal flows.
- Next step: Phase 3, Step 3.5 — counted-lines browser.
