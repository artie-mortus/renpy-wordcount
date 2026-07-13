# Status

- Completed: Steps 0.1 through 7.5 (Phase 7 and core parity complete).
- The generated runtime helper records visited labels to project-specific JSONL under user data for every run, warp, or Director session.
- A live coverage panel combines declared global/qualified-local labels with automatic playthrough visits and persistent manual marks.
- Incremental tailing handles appends, partial records, truncation, escapes, and malformed lines; internal Ren'Py callbacks are filtered out.
- Manual coverage persists atomically per canonical project root, while playthrough coverage can be cleared independently.
- The wx-independent route graph records labels in declaration order with source locations, qualified local targets, typed jump/call/fall-through edges, terminal flow, and menu/conditional branch counts.
- Static graph construction ignores expression targets while retaining their terminal behavior and prefers `start`, falling back to the first declared label.
- Route traversal matches the JavaScript metrics for shortest/longest route words, reading time, endings, branch points, unreachable labels, and loops; inline choice/condition word totals retain source-file identity.
- A dockable Route Details panel shows live summaries, expandable finite paths, branch totals, route notes, and double-click navigation to labels and branch lines.
- The native flow map ports the reference BFS layers, four-wide unreachable rows, node/edge styling, and 80-node cap; it supports scrolling, 50–200% zoom, fit-to-view, hover details, and click-to-jump.
- Verified (2026-07-12): 116/116 tests passed; live UI inspection confirmed legible rendering and clicking `start.ending` navigated to its declared line.
- Bug sweep (2026-07-12, 60d8aab): fixed surrogate-pair decoding and stale-offset reset in coverage tail, non-ASCII manual marks, stale Mark/Unmark toggle, wxProcess double free, UTF-8 chunk splits in the Ren'Py log, and the Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash (OnClose re-entry guard).
- Known issues: none.
- Next step: Phase 8, Step 8.1 — prose analysis engine (deferred long-tail work).
