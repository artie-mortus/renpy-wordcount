# Status

- Completed: Steps 0.1 through 4.7.
- Diagnostics render as severity-colored Scintilla squiggles plus error/warning/notice circles in a dedicated gutter margin.
- Dwelling on a diagnosed line shows all messages in a calltip; a sortable docked panel lists severity, file, line, and message.
- Activating a diagnostics-panel row switches to the owning tab and selects the exact source range.
- The 120 ms project-analysis refresh clears every prior indicator and marker before applying the new snapshot, so fixed and closed-file issues disappear.
- The wx-free diagnostics model and parser parity contract remain unchanged by the UI layer.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 63/63, including diagnostics refresh coverage and all parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 4, Step 4.8 — editing commands and shortcuts.
