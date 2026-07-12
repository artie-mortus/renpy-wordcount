# Status

- Completed: Steps 0.1 through 4.9 (Phase 4 complete).
- View → Focus Mode (Ctrl+Shift+F) saves the full wxAUI perspective and hides every managed pane except the editor.
- Leaving focus mode reloads the exact saved perspective, restoring prior visibility, docking positions, sizes, and find/diagnostics state.
- A floating bottom-right pill shows the active document's live dialogue-word count and refits/repositions on edits and frame resizing.
- The pill is unmanaged by wxAUI, so it cannot contaminate the serialized layout being preserved.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 67/67, including all editing-command coverage and parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 5, Step 5.1 — project folder model.
