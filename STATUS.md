# Status

- Completed: Steps 0.1 through 3.3.
- Live single-document analysis remains debounced at 120 ms and follows the active editor tab.
- Added a dockable right-side Speaker Statistics panel using `wxAuiManager`.
- Each speaker row shows the resolved name, color swatch, words, lines, rounded percentage, and a proportional balance bar.
- Speaker rows use JavaScript parity ordering: stable descending word count, preserving first-seen order for ties.
- Empty scripts show `No dialogue yet.`; panel content updates on edits, tab changes, new tabs, and opened files.
- Core remains wx-free and unchanged; no scene/label targets, counted-lines browser, exports, or later-step features were added.
- Verified (2026-07-11): `cmake --build /home/artemis/Projects/say-count-native/build -j` succeeded.
- Verified (2026-07-11): `ctest --test-dir /home/artemis/Projects/say-count-native/build --output-on-failure` passed 42/42 tests; parity diffs were empty.
- Review fix (2026-07-11): the counts line concatenated a narrow `" words · "` literal (non-ASCII `·`) onto `wxString`, which converts through the locale — same pitfall that crashed 3.2; composed via `std::string` + `wxString::FromUTF8` instead.
- Verified (2026-07-11): live GUI review — empty tab showed "No dialogue yet."; two-speaker script showed Eileen "24 words · 2 lines · 92%" above Lucy "2 words · 1 lines · 8%" with correct swatch colors and proportional balance bars (matches JS ordering and Math.round percentages).
- Known issues: none.
- Next step: Phase 3, Step 3.4 — scene and label statistics.
