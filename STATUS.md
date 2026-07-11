# Status

- Completed: Steps 0.1 through 3.1; Phase 3 analysis data model is in place.
- Added wx-free ordered aggregates for totals, average words, dialogue/narration words and lines, per-speaker words/lines, and per-label words/lines.
- Labels and project aggregates preserve JavaScript `Map` first-seen order, including zero-count labels; local labels remain qualified.
- Reading time matches the app export: 200 WPM, zero for empty input, otherwise at least one minute with JavaScript-compatible rounding.
- Extended `--dump-json` and the JS parity dump with the new fields without changing existing keys.
- Added Catch2 cases for empty, narration-only, multi-speaker, multi-label, menu-choice, and reading-time boundary behavior.
- Added a two-file analysis parity fixture covering resolved speakers, narration, labels, counted menu choices, and reading time.
- Review fixes (2026-07-11): removed invented `dialogueWords`/`narrationWords`/`spokenLines`/`narrationLines` fields — the JS model has no such aggregates (narration is the `narrator` entry in `speakers`, parser.js:201); `averageWords` now serializes via `std::to_chars` shortest round-trip so fractions like 1.1 match `JSON.stringify` byte-for-byte (was `setprecision(17)` → `1.1000000000000001`), covered by a unit test and the `average-tenth` parity fixture.
- Verified (2026-07-11): `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j` succeeded with no new warnings.
- Verified (2026-07-11): `ctest --test-dir build --output-on-failure` passed 42/42 tests; every parity diff was empty.
- Known issues: none.
- Next step: Phase 3, Step 3.2 — live document analysis.
