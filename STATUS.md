# Status

- Completed: Steps 0.1 through 1.3 — repository/build setup, main window lifecycle, editor tabs, and file I/O.
- Step 1.3: multi-select `.rpy` open; Save / Save As; absolute file paths tracked per tab; repeat opens select the existing tab; frame-wide `.rpy` drag/drop; tabs normalized to four spaces on load; UTF-8 reads/writes.
- Step 1.2: client-filling `wxAuiNotebook`; one plain `wxStyledTextCtrl` per tab; New (`Ctrl+N`) / Close Tab (`Ctrl+W`) / notebook close buttons; save-point dirty dots (` ●`); discard/cancel warnings for dirty tab close and app close; one tab always remains.
- Parity alignment with JS app during review: untitled tabs named `scene-N.rpy` (JS editor-ui.js:292) and dirty dot `●` (editor-ui.js:257).
- Settings live in `~/.local/share/say-count/settings.json` via reusable `app::Settings` (wx 3.2 ignores `FileLayout_XDG` for `GetUserDataDir`, resolved manually); `say_count_core` stays wx-free.
- Verified (2026-07-10): build + ctest green (1/1), `git diff --check` clean, and application launches on the live display.
- Review: subagent code review found no issues; JS tab-behavior spec extracted for parity.
- Known issues: none.
- Next step: 1.4 — basic editor configuration.
