# Status

- Completed: Steps 0.1 through 6.3.
- Run from Caret saves all modified disk-backed tabs and launches `--warp` with the active script's game-relative path and one-based caret line.
- Warp rejects unsaved, out-of-project, or invalid active scripts without launching.
- Interactive Director uses the SDK `director` command and the same captured asynchronous process/log controls.
- Known pre-6.99 SDKs are blocked for warp/Director; unknown custom launcher versions remain usable.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 86/86, including SDK capability boundaries and parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 6, Step 6.4 — runtime state presets.
