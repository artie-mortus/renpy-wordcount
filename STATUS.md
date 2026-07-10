# Status

- Completed: Steps 0.1, 0.2, and 1.1 — repository/build setup and main window lifecycle.
- Step 1.1: real `wxApp`; `MainFrame` split into `src/ui/`; File/Edit/View/Help menus with Quit (`Ctrl+Q`) and About; status bar; window geometry + maximized state persisted as JSON, with safe fallback for missing/corrupt/off-screen geometry.
- Settings live in `~/.local/share/say-count/settings.json` via reusable `app::Settings`; `say_count_core` stays wx-free.
- Fix applied during review: wxWidgets 3.2 ignores `FileLayout_XDG` for `GetUserDataDir()` (returns legacy `~/.say-count`), so `Settings` resolves `$XDG_DATA_HOME`/`~/.local/share` itself.
- Verified (2026-07-10): build + ctest green (1/1); GUI exercised on live display — start, quit via WM close, settings written, geometry restored on relaunch, clean exit.
- Known issues: none.
- Next step: 1.2 — Notebook and editor tabs.
