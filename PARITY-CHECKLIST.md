# Feature Parity Checklist

Source: `/home/artemis/Projects/words-til-vn/README.md`. Update throughout development; complete the audit in Step 9.5.

| # | Feature | Status |
|---:|---|---|
| 1 | live Ren'Py editor with line numbers, syntax highlighting, and file tabs | Done — tabs (Step 1.2), line numbers (Step 1.4), syntax highlighting (Step 2.7) |
| 2 | run the project from a chosen source line through the local save server | Done — native current-line warp replaces the browser save-server bridge (Step 6.3) |
| 3 | Production desk with normal run, current-line warp, Interactive Director, stop/restart controls, live status, and a persistent launch log | Done — process controls/logging (Step 6.2), warp and Interactive Director (Step 6.3) |
| 4 | reusable JSON state presets applied before startup (generates `say_count_runtime.rpy` in the connected game folder) | Done (Step 6.4) |
| 5 | official Ren'Py lint output alongside Say Count's instant script checks | Done (Step 6.5) |
| 6 | project asset browser with image/audio previews and one-click `show` / `play music` insertion | Done (Step 6.7) — also supports `scene` and `play sound` insertion |
| 7 | localization tools for missing-string counts, translation generation, and dialogue export | Done — commands (Step 6.6) and missing-string dashboard (Step 8.5) |
| 8 | translation dashboard groups missing strings by source file, shows available label/speaker context, and opens matching loaded lines | Done (Step 8.5) |
| 9 | voice-production tracker with recorded/retake/approved states, optional voice filenames, tracking CSV, and printable per-role VA scripts for characters and narration | Done (Steps 8.3–8.4) |
| 10 | accessibility audit for untranslated controls, missing image-button descriptions, and visually encoded character voices | Done (Step 8.7) |
| 11 | automatic playthrough coverage captured from launched games, plus manual label coverage | Done (Step 6.8) |
| 12 | persistent continuity notes for characters, locations, timeline, relationships, inventory, and story facts | Done (Step 8.6) |
| 13 | multi-file projects: import several `.rpy` files, totals across all | Changed — native project folders/import are complete; combined analyses are exposed in project production/export tools rather than replacing the active-tab status summary |
| 14 | counts dialogue/narration words, not labels, definitions, jumps, variables, comments, or scene commands | Done (Phase 2 parser parity suite) |
| 15 | optional menu choice counting (including conditioned choices) | Done (Step 2.5) |
| 16 | `extend` lines attributed to the previous speaker | Done (Step 2.6 fixtures) |
| 17 | speaker breakdown with talk-time balance bars | Done — Step 3.3 (`414ab59`): dockable panel, words-desc ordering, swatches, percentages, balance bars |
| 18 | per-character and per-scene dialogue word and line targets | Done (Step 3.4) |
| 19 | uses `define e = Character("Eileen")` names instead of `e` aliases | Done (Step 2.4) |
| 20 | total word and line target progress | Done (Step 3.4) |
| 21 | reading-time estimate (~200 wpm) and words-written-this-session counter | Changed — reading-time estimate is live; session delta counter is omitted in favor of snapshots/version comparison |
| 22 | version diff: paste an old script to see words added or cut per speaker | Done (Step 3.7) |
| 23 | scene/label breakdown | Done (Step 3.4) |
| 24 | counted-line speaker/search filters | Done (Step 3.5) |
| 25 | CSV/JSON stats export | Done (Step 3.6) |
| 26 | complete project import/export, including files, targets, and settings | Done (Step 5.6) — browser-app version-1 bundle import/export with round-trip coverage |
| 27 | standalone HTML report export | Done (Step 3.6) |
| 28 | dark mode | Changed — persisted system/light/dark editor themes use native desktop chrome (Step 1.4) |
| 29 | import `.rpy` files (multi-select or drag & drop onto the editor), copy script, download `.rpy` | Changed — native multi-select/drop import and ordinary clipboard/Save As replace browser copy/download actions |
| 30 | project folder sync: connect a folder and every `.rpy` in it opens automatically; edits made outside (e.g. VS Code) are picked up while you write | Done — connection (Step 5.1), watcher (Step 5.2), conflict safety (Step 5.3) |
| 31 | find & replace (Ctrl+F): case, regex (`.*`, `$1` groups), and whole-word toggles, all-files scope, F3/Shift+F3 to cycle, every match highlighted in the editor | Done — current file (Step 4.4), all open files (Step 4.5) |
| 32 | editor shortcuts: Ctrl+G go to line, Ctrl+/ comment toggle, Alt+↑/↓ move lines, Shift+Alt+↑/↓ duplicate lines, Ctrl+↑/↓ previous/next label, Ctrl+PageUp/PageDown switch tabs, Ctrl+Shift+T reopen closed tab, Ctrl+S save + snapshot, Ctrl+= / Ctrl+− / Ctrl+0 font size — Ctrl+K shows the full cheat sheet | Changed — all editing/navigation/save/font shortcuts and cheat sheet are present; reopen-closed is deferred |
| 33 | status bar with line/column, current label (also shown as a breadcrumb in the header), and selected-word count | Deferred — native status bar reports live dialogue totals and operational state; detailed caret breadcrumb was not release-critical |
| 34 | auto-close and selection-wrap for `"` `(` `[`, with type-over for closers and escaped-quote awareness | Done (Step 4.8) |
| 35 | snippets in autocomplete: `label`, `menu`, `define` expand to full blocks with the placeholder selected | Done (Step 4.2) |
| 36 | word wrap toggle for long dialogue lines (hides line numbers while on) | Changed — persisted word wrap is implemented; line numbers remain visible for navigation |
| 37 | current-line highlight, adjustable editor font size (persisted) | Done (Steps 1.4 and 4.8) |
| 38 | collapsible side panels (fold state remembered), focus mode with floating word-count pill, draggable editor/stats divider | Changed — dockable native panes plus exact-layout focus mode and live word-count pill replace the browser divider model |
| 39 | tabs: dirty dot when a file differs from what's on disk, inline rename on double-click | Changed — dirty dots are implemented; native Save As replaces inline tab rename |
| 40 | snapshots: automatic backup every 10 minutes plus manual "Snapshot now"; restore or diff any point (last 50 kept) | Done — storage/retention (Step 5.4), comparison and confirmed restore (Step 5.5) |
| 41 | outline panel: labels with word counts, menu choices, and jumps/calls — click to navigate; jumps to missing labels shown in red | Done (Step 4.1) |
| 42 | routes panel: walks jumps/calls/fall-through from `start` for shortest/longest route word counts and reading time, ending and branch-point counts, and unreachable labels (click to jump) | Done (Steps 7.2–7.3) |
| 43 | branch-aware route details: shows dialogue words inside each inline menu choice and conditional block | Done (Steps 7.2–7.3) |
| 44 | flow map: SVG story graph of labels — gold border marks `start`, red marks endings, dashed red curves are loops back, dashed boxes are unreachable; click a box to jump | Done — native wxGraphicsContext equivalent (Steps 7.4–7.5) |
| 45 | speaker colors: `Character(..., color="#c8ffc8")` tints the alias in the editor and adds swatches to the speaker, balance, and counted-line lists | Changed — character colors are parsed and shown as speaker/balance swatches; editor and counted-line text retain theme colors |
| 46 | autocomplete while typing: speaker aliases at line start, label names after `jump`/`call` (arrows to pick, Tab/Enter to accept, Esc to dismiss) | Done (Step 4.2) |
| 47 | Ren'Py-aware autocomplete for image names after `show`, audio paths after `play`, screen names after `call screen`, and declared variables after `$` | Done (Step 4.3) |
| 48 | inline editor diagnostics for syntax, unresolved-label, quote, speaker, and long-line checks | Done — engine (Step 4.6), squiggles/gutter/tooltips/panel (Step 4.7) |
| 49 | project-wide text search across every `.rpy` file in the connected game folder (or all open tabs offline) | Done — open tabs (Step 4.5), recursively connected folder (Step 5.1) |
| 50 | folder-sync conflict review with a side-by-side local/disk comparison before choosing a version | Done (Step 5.3) |
| 51 | safe project-wide character-alias and label rename with an affected-line preview before applying | Done (Step 5.7) |
| 52 | prose panel: vocabulary size, average sentence length per speaker, and overused words/phrases as clickable chips that open find | Done (Steps 8.1–8.2) |
| 53 | local browser autosave | Not applicable — native editor uses atomic disk saves and snapshots |
| 54 | optional save folder: pick a directory on your computer (Chrome/Edge) and autosave writes the `.rpy` files there | Not applicable — native project-folder connection writes directly to the filesystem |
| 55 | tab characters auto-converted to 4 spaces (Ren'Py rejects tabs); "Fix indents" snaps indentation to multiples of 4 | Done (Step 8.8) |
