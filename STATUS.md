# Status

- Completed: Steps 0.1 through 5.7 (Phase 5 complete).
- Edit → Rename Ren'Py Symbol previews and applies project-wide speaker-alias or label renames.
- Only supported declarations, leading dialogue aliases, and static jump/call targets change; comments, strings, Python, expression flow, and similar substrings remain untouched.
- Invalid names and declaration collisions are rejected, exact line endings are retained, and every affected line is shown before apply.
- Apply requires a successful pre-rename snapshot; changed files become undoable unsaved edits while unchanged files and disk content stay untouched.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 82/82, including similar-substring, expression-flow, collision, jump/call, bundle, snapshot, and parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 6, Step 6.1 — Ren'Py SDK detection and settings.
