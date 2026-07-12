# Status

- Completed: Steps 0.1 through 3.7 (Phase 3 complete).
- The statistics pane now accepts a pasted older script or loads one from disk for comparison with the active editor.
- Comparison shows total before/after, added, removed, and net words plus the union of speakers sorted by absolute word-count change.
- The old-version source survives live current-document analysis rebuilds; unchanged speakers are identified explicitly.
- Core remains wx-free; dump JSON schema and `.rpy` parsing behavior are unchanged.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 47/47, including controlled additions, removals, speaker entry/exit, and all parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 4, Step 4.1 — outline panel.
