# Status

- Completed: Steps 0.1 through 6.8 (Phase 6 complete).
- The generated runtime helper records visited labels to project-specific JSONL under user data for every run, warp, or Director session.
- A live coverage panel combines declared global/qualified-local labels with automatic playthrough visits and persistent manual marks.
- Incremental tailing handles appends, partial records, truncation, escapes, and malformed lines; internal Ren'Py callbacks are filtered out.
- Manual coverage persists atomically per canonical project root, while playthrough coverage can be cleared independently.
- Verified (2026-07-12): 98/98 tests passed; a real Ren'Py 8.5.3 controlled launch recorded the declared `start` label before exiting.
- Bug sweep (2026-07-12, 60d8aab): fixed surrogate-pair decoding and stale-offset reset in coverage tail, non-ASCII manual marks, stale Mark/Unmark toggle, wxProcess double free, UTF-8 chunk splits in the Ren'Py log, and the Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash (OnClose re-entry guard).
- Known issues: none.
- Next step: Phase 7, Step 7.1 — route graph data model.
