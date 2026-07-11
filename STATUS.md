# Status

- Completed: Steps 0.1 through 2.7; Phase 2 is complete.
- Added wx-free highlight span classification to the core tokenizer for comments, keywords, labels, known speakers, strings, Python lines, and Ren'Py statement bodies.
- Matched the JavaScript distinction between known `Character(...)` aliases and unknown dialogue-like prefixes.
- Connected every editor tab to a `wxSTC_LEX_CONTAINER` lexer using `wxEVT_STC_STYLENEEDED` and whole-line invalidated-range styling.
- Ordinary edits style only requested lines; character-definition edits exceptionally refresh the alias cache and recolor the document because they can affect speaker tokens globally.
- Added coordinated light/dark/system editor colors for every syntax class while preserving Step 1.4 editor settings.
- Added Catch2 coverage for reference categories, comments inside strings, escaped quotes, unknown speakers, labels, Python, and Ren'Py statements.
- Review fixes (2026-07-11): keyword set widened to the full JS ignored-starter list plus `extend` with case-insensitive matching (highlight.js:41), with regression tests; `OnModified` now filters `wxSTC_MOD_INSERTTEXT|DELETETEXT` so style-change events cannot recurse through `Colourise`.
- Verified (2026-07-11): `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j` succeeded with no warnings introduced.
- Verified (2026-07-11): `ctest --test-dir build --output-on-failure` passed 35/35 tests; all parity diffs remained empty.
- Verified (2026-07-11): live GUI review (xdotool-driven, isolated XDG data dir) — typed edits styled immediately per category; new `Character("Quinn")` define recolored the `q` speaker live; fixture opens styled; 12,001-line file opened, jumped to end, and accepted edits with no stalls and full highlighting; dark theme colors readable across all classes.
- Known issues: none.
- Next step: Phase 3, Step 3.1 — analysis data model.
