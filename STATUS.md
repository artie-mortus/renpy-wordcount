# Status

- Completed: Steps 0.1 through 9.5; Phases 8 and 9 and the release gate are complete.
- The application shell now uses a modern script-desk design with a live cue bar, quieter default layout, paper/ink editor palette, and purpose-led production workflows.
- Workspace sessions restore the project, open files, active tab, caret/scroll positions, and dock layout; Ctrl+P and Ctrl+Shift+P provide fuzzy Quick Open and command access.
- Production Desk includes prose analysis, voice tracking/exports, translation gaps, continuity notes, and accessibility findings.
- Fix Indents provides a confirmed preview and snapshot before applying tab/four-space normalization.
- Chat Format converts prose/dialogue to and from chat_program syntax (pinned upstream 85ee08d), installs the runtime without overwriting local edits, bridges in/out of the chat screen via say_count_chat_begin/end, and ships an in-app Chat Format Guide under Help.
- Release 1.0.0 builds optimized and stripped; the bundled-wx option produces a 16 MiB binary with wxWidgets linked statically.
- Linux installation includes the executable, desktop entry, SVG icon, Ren'Py MIME registration, command-line `.rpy` opening, and uninstall target.
- AppImage staging is locally verified; the packaging script and tagged/manual GitHub workflow use linuxdeploy and appimagetool.
- The repository desk replaces Google Drive with host-agnostic Git operations:
  open an existing local repository, clone, init, resolve its configured push
  remote, fetch, fast-forward pull, commit, and push. Private access uses the
  system Git client's SSH agent or credential helper.
- Final verification (2026-07-13): native 140/140 tests and reference JavaScript parity tests passed.
- Release UI smoke test opened a real `.rpy` positional argument and remained stable for the three-second observation window.
- All 55 reference features are classified as Done, Changed, Deferred, or Not applicable in PARITY-CHECKLIST.md; none remain Pending.
- Known parity choices: native disk saves replace browser autosave/download APIs; native panes replace the browser divider; app chrome follows the desktop theme.
- Known issue: route/diagnostic jumps match scripts by basename, so duplicate `.rpy` names across subfolders jump to the first match (same as the JS app).
- Resource note: bundled wxWidgets should be built with a bounded job count (`cmake --build build-portable -j2`) on memory-constrained systems.
