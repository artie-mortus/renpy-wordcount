# Status

- Completed: Steps 0.1 through 4.4.
- Ctrl+F opens a current-file find/replace bar with next/previous navigation, match count, case, regex, and whole-word options.
- F3/Shift+F3 navigate with wraparound; Escape closes the bar, and opening it seeds a single-line editor selection.
- Every current-file match is highlighted through a Scintilla indicator; highlights and counts refresh after edits and tab changes.
- Replace-current and replace-all support literal replacements and regex `$1` capture expansion; each replacement is one undo action and invalid regexes never alter text.
- Core matching remains wx-free; all-files scope is reserved for Step 4.5 and parser parity output is unchanged.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 58/58, including literal, regex, capture-group, whole-word, invalid-regex, zero-match, and all parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 4, Step 4.5 — find across files.
