# Status

- Completed: Steps 0.1 through 3.2; live single-document analysis is wired into the editor UI.
- Text insertions/deletions restart a one-shot 120 ms `wxTimer`, matching the JavaScript `ANALYSIS_DELAY_MS` cadence.
- The timer callback synchronously runs the existing wx-free `analyze_script` only for the active tab.
- The existing modification filter and `Character()` speaker-alias refresh/colourise behavior remain intact.
- The status bar shows comparable totals: dialogue words, dialogue lines, and reading time in minutes.
- Notebook page changes cancel pending debounce work and immediately analyze the newly active tab.
- No threads, stats panels, counted-lines browser, exports, project-wide analysis, or core changes were added.
- Verified (2026-07-11): `cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j` succeeded with no new warnings.
- Verified (2026-07-11): `ctest --test-dir build --output-on-failure` passed 42/42 tests; all parity diffs were empty.
- Review fix (2026-07-11): the status-bar lambda used `wxString::Format("%zu", ...)`, which aborts in wx 3.2's format-string parser and crashed the app at startup; totals are now composed with `std::to_string` + `wxString::FromUTF8`.
- Verified (2026-07-11): live GUI review — typed two dialogue lines and saw "8 dialogue words · 2 dialogue lines · 1 min reading time" (matching JS labels, index.html:105); a new tab showed zeros immediately and clicking back restored the totals instantly.
- Known issues: none.
- Next step: Phase 3, Step 3.3 — speaker statistics panel.
