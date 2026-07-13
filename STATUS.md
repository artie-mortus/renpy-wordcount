# Status

- Completed: Steps 0.1 through 6.2.
- Ren'Py → Run Project launches the connected project asynchronously through an argv array with redirected stdout/stderr.
- Stop Running Project terminates the owned process tree; launch, running, exited, failed, and stopped states are reported.
- A dockable Ren'Py Log shows the latest 30,000 characters while the complete append-only log persists under user data.
- Missing SDK/project and overlapping-operation errors are reported before launch; application teardown detaches and terminates its owned child.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 85/85, including SDK discovery and parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 6, Step 6.3 — warp and Interactive Director.
