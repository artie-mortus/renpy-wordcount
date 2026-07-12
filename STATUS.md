# Status

- Completed: Steps 0.1 through 3.6.
- File → Export Statistics now writes speaker CSV, full statistics JSON, or a standalone HTML report for the active analysis.
- Export generation is wx-free and preserves UTF-8; CSV fields are quoted, JSON strings/control characters are escaped, and report content is HTML-escaped.
- JSON includes totals, speaker/scene aggregates, warnings, and counted-line details; HTML includes totals plus speaker and scene tables.
- Core remains wx-free; dump JSON schema and `.rpy` parsing behavior are unchanged.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 45/45, including all parser parity tests and a non-ASCII export fixture (`Élodie`, `Café`).
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 3, Step 3.7 — version comparison.
