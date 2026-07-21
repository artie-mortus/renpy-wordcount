# Status

- Completed: Steps 0.1 through 9.5; Phases 8 and 9 and the release gate are complete.
- The application shell now uses a modern script-desk design with a live cue bar, quieter default layout, paper/ink editor palette, and purpose-led production workflows.
- Home is reusable from the File menu, new-story creation previews its destination and supports Unicode titles, and the stable action bar keeps writing available alongside problem badges.
- Story Library now provides project-wide cast lookup plus searchable image/audio previews and context-safe insertion, replacing the separate asset-browser surface while retaining its saved pane identity.
- Workspace sessions restore the project, open files, active tab, caret/scroll positions, and dock layout; Ctrl+P and Ctrl+Shift+P provide fuzzy Quick Open and command access.
- The Script Index provides project-wide, folder-relative navigation while opening editor tabs lazily; duplicate script basenames in different folders now retain distinct identities for route, diagnostic, search, and production navigation.
- Command-bar actions use native focusable buttons, and routine feedback is delivered through a non-modal inline notification bar instead of interrupting writing with dialogs.
- Production Desk includes prose analysis, voice tracking/exports, translation gaps, continuity notes, and accessibility findings.
- Fix Indents provides a confirmed preview and snapshot before applying tab/four-space normalization.
- Chat Format converts prose/dialogue to and from chat_program syntax (pinned upstream 85ee08d), installs the runtime without overwriting local edits, bridges in/out of the chat screen via say_count_chat_begin/end, and ships an in-app Chat Format Guide under Help.
- Chat scenes render through two vendored skins — Discord-style channels or a Kik-style bubble messenger (say_count_chat_kik.rpy); the conversion dialog asks per scene and can wrap output in skin-aware bridge calls.
- Release 1.0.0 builds optimized and stripped; the bundled-wx option produces a 16 MiB binary with wxWidgets linked statically.
- Linux installation includes the executable, desktop entry, SVG icon, Ren'Py MIME registration, command-line `.rpy` opening, and uninstall target.
- AppImage staging is locally verified; the packaging script and tagged/manual GitHub workflow use linuxdeploy and appimagetool.
- Compiler warnings are enabled for GCC/Clang, CI treats them as errors, and a dedicated sanitizer job runs the core suite under AddressSanitizer and UndefinedBehaviorSanitizer.
- Ren'Py lint/compile validation is mandatory whenever an SDK is available; the display-dependent runtime smoke is isolated behind `SAY_COUNT_GRAPHICAL_TESTS` for controlled graphical environments.
- The repository desk replaces Google Drive with host-agnostic Git operations:
  open an existing local repository, clone, init, resolve its configured push
  remote, fetch, fast-forward pull, commit, and push. Private access uses the
  system Git client's SSH agent or credential helper.
- Final verification (2026-07-21): 235/235 default tests, reference JavaScript parity, Ren'Py lint/compile, all performance thresholds, and 1,154 sanitizer-backed core assertions passed.
- Release UI smoke test opened a real `.rpy` positional argument and remained stable for the three-second observation window.
- All 55 reference features are classified as Done, Changed, Deferred, or Not applicable in PARITY-CHECKLIST.md; none remain Pending.
- Known parity choices: native disk saves replace browser autosave/download APIs; native panes replace the browser divider; app chrome follows the desktop theme.
- Resource note: bundled wxWidgets should be built with a bounded job count (`cmake --build build-portable -j2`) on memory-constrained systems.
