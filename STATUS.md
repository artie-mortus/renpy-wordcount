# Status

- Completed: Steps 0.1 through 6.8 (Phase 6 complete).
- The generated runtime helper records visited labels to project-specific JSONL under user data for every run, warp, or Director session.
- A live coverage panel combines declared global/qualified-local labels with automatic playthrough visits and persistent manual marks.
- Incremental tailing handles appends, partial records, truncation, escapes, and malformed lines; internal Ren'Py callbacks are filtered out.
- Manual coverage persists atomically per canonical project root, while playthrough coverage can be cleared independently.
- Verified (2026-07-12): 97/97 tests passed; a real Ren'Py 8.5.3 controlled launch recorded the declared `start` label before exiting.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 7, Step 7.1 — route graph data model.
