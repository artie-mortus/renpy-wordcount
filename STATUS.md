# Status

- Completed: Steps 0.1 through 5.1.
- File → Connect Project Folder accepts either a Ren'Py root or `game/` folder and tracks canonical project/scripts roots independently from open tabs.
- Discovery recursively finds and sorts `.rpy` files, opens every script, and removes only untouched blank placeholder tabs.
- The recent-project submenu persists up to eight deduplicated roots in the existing atomic settings file.
- Project-wide search, autocomplete, and diagnostics immediately cover every discovered script because the folder model populates the shared open-document model.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 69/69, including recursive root/game discovery, ordering, recent-list behavior, and parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 5, Step 5.2 — external file watching.
