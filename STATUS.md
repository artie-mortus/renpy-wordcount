# Status

- Completed: Steps 0.1 through 2.5, Step 2.6A, and Step 2.6B.
- Step 2.6B: project analysis resolves cross-file character definitions and caches unchanged per-file analysis results.
- Project cache invalidates on file content, merged character definitions/colors, menu-count mode, and long-line threshold changes; closed-file entries are pruned.
- Project `count_menu_choices` now remains covered at both script and project levels.
- Project lint reports duplicate labels, missing jump/call targets, malformed labels/statements, and empty blocks.
- Lint and dialogue analysis qualify local labels against the current global label; `call screen` is not treated as a label reference.
- Python block contents remain opaque to project lint, including jump-like and label-like text.
- Writer warnings are suppressed for `gui.rpy`, `options.rpy`, `screens.rpy`, and `say_count_runtime.rpy`, including path/case variants; those files still participate in label resolution.
- Added Catch2 translations for all fixtures assigned to 2.6B and JS/native JSON parity for local-label qualification.
- Route analysis and branch totals remain untouched for their later phase; symbol collection/rename fixtures remain out of scope.
- Verified (2026-07-11): `cmake --build build -j` succeeded.
- Review fix (2026-07-11): bare `label:` lines now emit the malformed-label syntax warning (`^label\b`, matching JS `analysis.js:70`); divergence confirmed against the live JS parser and covered by a regression test.
- Verified (2026-07-11): `ctest --test-dir build --output-on-failure` passed 29/29 tests, including seven JS/native parity cases.
- Verified (2026-07-11): `git diff --check` passed; reference JavaScript repository was not modified.
- Known issues: none for Step 2.6B.
- Next step: 2.6C parity closeout.
