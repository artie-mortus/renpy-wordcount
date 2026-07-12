# Status

- Completed: Steps 0.1 through 4.1.
- A dockable Outline panel lists labels with word counts, menu choices, jumps, and calls for the active editor.
- Local label targets are qualified; resolved flow nodes navigate to definitions, while missing targets appear red and navigate to their reference line.
- Double-click navigation selects/reveals the exact one-based source line; outline content follows the existing debounced live-analysis updates and tab changes.
- Core remains wx-free; dump JSON schema and `.rpy` parsing behavior are unchanged.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 48/48, including controlled outline ordering, local-label resolution, missing targets, line destinations, and all parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 4, Step 4.2 — basic autocomplete.
