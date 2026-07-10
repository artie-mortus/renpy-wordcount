# Status

- Completed: Steps 0.1 through 1.4 — repository/build setup and the basic daily-driver editor shell.
- Step 1.4: five-digit line-number margin; current-line highlight; word-wrap toggle; 10–32px font controls (`Ctrl+=`, `Ctrl+-`, `Ctrl+0`); system/light/dark editor themes; atomic persistence alongside window settings.
- Step 1.3: multi-select `.rpy` open; Save / Save As; absolute file paths tracked per tab; repeat opens select the existing tab; frame-wide `.rpy` drag/drop; tabs normalized to four spaces on load; UTF-8 reads/writes.
- Step 1.2: client-filling `wxAuiNotebook`; one plain `wxStyledTextCtrl` per tab; New (`Ctrl+N`) / Close Tab (`Ctrl+W`) / notebook close buttons; save-point dirty dots (` ●`); discard/cancel warnings for dirty tab close and app close; one tab always remains.
- Parity alignment with JS app during review: untitled tabs named `scene-N.rpy` (JS editor-ui.js:292) and dirty dot `●` (editor-ui.js:257).
- Settings live in `~/.local/share/say-count/settings.json` via reusable `app::Settings` (wx 3.2 ignores `FileLayout_XDG` for `GetUserDataDir`, resolved manually); `say_count_core` stays wx-free.
- Verified (2026-07-10): build + ctest green (1/1), `git diff --check` clean; live GUI controls persisted and restored 18px font, word wrap, and dark theme in an isolated XDG settings directory.
- Review: subagent code review found no issues; JS tab-behavior spec extracted for parity.
- Known issues: none.
- Next step: 2.1 — inventory parser behavior.
