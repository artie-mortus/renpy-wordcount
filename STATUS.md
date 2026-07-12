# Status

- Completed: Steps 0.1 through 3.5.
- The statistics pane now lists every counted line for the active editor with source line, speaker, label, and dialogue text.
- Counted lines filter immediately by exact speaker and case-insensitive text/raw-source and label substrings; filter values survive live-analysis rebuilds when still valid.
- Double-clicking a counted-line row selects, reveals, and focuses its one-based source line in the active editor.
- Existing project, character, scene/label totals, targets, progress, swatches, percentages, and balance bars remain intact.
- Core remains wx-free; dump JSON schema and `.rpy` parsing behavior are unchanged.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 44/44, including all parser parity tests.
- Verification notes: row identity retains the originating `CountedLine` index while filtering, and jumping consumes its parser-provided one-based `line_number`.
- Review fix (2026-07-12): text/label filter queries now trimmed before matching, matching JS `renderLineList` (`.trim().toLowerCase()`). JS's 80-row cap intentionally not replicated — `wxListCtrl` handles full lists.
- Live GUI verified (2026-07-12, xdotool): six fixture rows map to source lines 5/6/7/10/11/14; padded " sunscreen " matches line 6; label "beach" isolates `start.beach` rows; Eileen speaker filter shows 4 rows; double-click jumped editor to line 11 with focus; live edit added row 15 under epilogue with speaker filter preserved.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 3, Step 3.6 — statistics export.
