# Status

- Completed: Steps 0.1, 0.2, 1.1, and 1.2 — repository/build setup, main window lifecycle, and editor tabs.
- Step 1.2: client-filling `wxAuiNotebook`; one plain `wxStyledTextCtrl` per tab; New (`Ctrl+N`) / Close Tab (`Ctrl+W`) / notebook close buttons; save-point dirty dots (` ●`); discard/cancel warnings for dirty tab close and app close; one tab always remains.
- Parity alignment with JS app during review: untitled tabs named `scene-N.rpy` (JS editor-ui.js:292) and dirty dot `●` (editor-ui.js:257).
- Settings live in `~/.local/share/say-count/settings.json` via reusable `app::Settings` (wx 3.2 ignores `FileLayout_XDG` for `GetUserDataDir`, resolved manually); `say_count_core` stays wx-free.
- Verified (2026-07-10): build + ctest green (1/1); GUI driven on live display — typing marks tab dirty, `Ctrl+N` adds tab, clean tab closes silently, dirty close and dirty quit both raise the "Unsaved changes" dialog, cancel keeps app alive.
- Review: subagent code review found no issues; JS tab-behavior spec extracted for parity.
- Known issues: none.
- Next step: 1.3 — file opening and saving.
