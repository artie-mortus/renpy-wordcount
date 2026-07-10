# Status

- Completed: Steps 0.1 through 2.2 — editor shell, parser inventory, and line tokenizer.
- Step 2.2: added a dependency-free Ren'Py line tokenizer with source line/indentation metadata, quote-aware comment splitting, escaped-quote segments, requested statement classifications, and safe malformed-input classification. Word counting remains Step 2.3 work.
- Step 2.1: documented the complete JavaScript parser contract and fixture mapping in `docs/parser-behavior.md`; selected a dependency-free UTF-8 scanner with vendored Unicode L*/N* category ranges for exact word-count semantics. No parser implementation was added.
- Step 1.4: five-digit line-number margin; current-line highlight; word-wrap toggle; 10–32px font controls (`Ctrl+=`, `Ctrl+-`, `Ctrl+0`); system/light/dark editor themes; atomic persistence alongside window settings.
- Step 1.3: multi-select `.rpy` open; Save / Save As; absolute file paths tracked per tab; repeat opens select the existing tab; frame-wide `.rpy` drag/drop; tabs normalized to four spaces on load; UTF-8 reads/writes.
- Step 1.2: client-filling `wxAuiNotebook`; one plain `wxStyledTextCtrl` per tab; New (`Ctrl+N`) / Close Tab (`Ctrl+W`) / notebook close buttons; save-point dirty dots (` ●`); discard/cancel warnings for dirty tab close and app close; one tab always remains.
- Parity alignment with JS app during review: untitled tabs named `scene-N.rpy` (JS editor-ui.js:292) and dirty dot `●` (editor-ui.js:257).
- Settings live in `~/.local/share/say-count/settings.json` via reusable `app::Settings` (wx 3.2 ignores `FileLayout_XDG` for `GetUserDataDir`, resolved manually); `say_count_core` stays wx-free.
- Verified (2026-07-10): build + ctest green (1/1), `git diff --check` clean; live GUI controls persisted and restored 18px font, word wrap, and dark theme in an isolated XDG settings directory.
- Review: subagent code review found no issues; JS tab-behavior spec extracted for parity.
- Known issues: none.
- Verified (2026-07-10): every `test/parser.test.js` fixture maps to a documented behavior; reference JavaScript repository remains unchanged.
- Verified (2026-07-10): debug build and ctest green (5/5), including tokenizer forms, escaped quotes/comments, indentation, and malformed input; `git diff --check` clean.
- Next step: 2.3 — core dialogue counting and the parity harness.
