# Status

- Completed: Steps 0.1 through 4.3.
- Autocomplete now indexes every open script for image declarations, audio definitions and quoted paths, screen declarations, and declared/assigned variables.
- Ren'Py-aware contexts cover `show`/`hide`/`scene`, quoted `play`/`queue`, `call screen`, and bare `$` expressions; audio acceptance closes the quote.
- Speaker aliases and labels are now project-wide, and the completion index refreshes when tabs or document text change.
- Suggestions remain case-insensitive, prefix-filtered, capped at eight, and suppressed in unrelated contexts; core remains wx-free and parser parity output is unchanged.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 53/53, including symbol extraction, cross-file contexts, index rebuilding, and all parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 4, Step 4.4 — find in current file.
