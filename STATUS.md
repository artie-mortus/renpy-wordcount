# Status

- Completed: Steps 0.1 through 6.1.
- Ren'Py SDK discovery checks the persisted executable, `RENPY_EXECUTABLE`, `PATH`, and conventional SDK folders in priority order.
- Configured paths may name the launcher or SDK directory and must resolve to a regular executable.
- The Ren'Py menu displays detected status/version and lets the user choose and atomically persist an SDK launcher.
- Version detection uses SDK directory names and launcher `--version` output.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 85/85, including configured/environment/PATH priority, invalid executable, version parsing, and parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 6, Step 6.2 — launch and stop Ren'Py.
