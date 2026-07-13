# Feature Parity Checklist

Source: `/home/artemis/Projects/words-til-vn/README.md`. Update throughout development; complete the audit in Step 9.5.

| # | Feature | Status |
|---:|---|---|
| 1 | live Ren'Py editor with line numbers, syntax highlighting, and file tabs | Done — tabs (Step 1.2), line numbers (Step 1.4), syntax highlighting (Step 2.7) |
| 2 | run the project from a chosen source line through the local save server | Pending |
| 3 | Production desk with normal run, current-line warp, Interactive Director, stop/restart controls, live status, and a persistent launch log | Done — process controls/logging (Step 6.2), warp and Interactive Director (Step 6.3) |
| 4 | reusable JSON state presets applied before startup (generates `say_count_runtime.rpy` in the connected game folder) | Done (Step 6.4) |
| 5 | official Ren'Py lint output alongside Say Count's instant script checks | Done (Step 6.5) |
| 6 | project asset browser with image/audio previews and one-click `show` / `play music` insertion | Done (Step 6.7) — also supports `scene` and `play sound` insertion |
| 7 | localization tools for missing-string counts, translation generation, and dialogue export | Partial — translation generation and dialogue export done (Step 6.6); missing-string dashboard remains pending |
| 8 | translation dashboard groups missing strings by source file, shows available label/speaker context, and opens matching loaded lines | Pending |
| 9 | voice-production tracker with recorded/retake/approved states, optional voice filenames, tracking CSV, and printable per-role VA scripts for characters and narration | Pending |
| 10 | accessibility audit for untranslated controls, missing image-button descriptions, and visually encoded character voices | Pending |
| 11 | automatic playthrough coverage captured from launched games, plus manual label coverage | Done (Step 6.8) |
| 12 | persistent continuity notes for characters, locations, timeline, relationships, inventory, and story facts | Pending |
| 13 | multi-file projects: import several `.rpy` files, totals across all | Partial — multi-file open and recursive project-folder model done (Steps 1.3/5.1); combined main statistics pending |
| 14 | counts dialogue/narration words, not labels, definitions, jumps, variables, comments, or scene commands | Pending |
| 15 | optional menu choice counting (including conditioned choices) | Pending |
| 16 | `extend` lines attributed to the previous speaker | Pending |
| 17 | speaker breakdown with talk-time balance bars | Done — Step 3.3 (`414ab59`): dockable panel, words-desc ordering, swatches, percentages, balance bars |
| 18 | per-character and per-scene dialogue word and line targets | Done (Step 3.4) |
| 19 | uses `define e = Character("Eileen")` names instead of `e` aliases | Pending |
| 20 | total word and line target progress | Done (Step 3.4) |
| 21 | reading-time estimate (~200 wpm) and words-written-this-session counter | Pending |
| 22 | version diff: paste an old script to see words added or cut per speaker | Done (Step 3.7) |
| 23 | scene/label breakdown | Done (Step 3.4) |
| 24 | counted-line speaker/search filters | Done (Step 3.5) |
| 25 | CSV/JSON stats export | Done (Step 3.6) |
| 26 | complete project import/export, including files, targets, and settings | Done (Step 5.6) — browser-app version-1 bundle import/export with round-trip coverage |
| 27 | standalone HTML report export | Done (Step 3.6) |
| 28 | dark mode | Partial — system/light/dark editor themes landed (Step 1.4); app-wide dark mode pending |
| 29 | import `.rpy` files (multi-select or drag & drop onto the editor), copy script, download `.rpy` | Partial — multi-select open and drag & drop landed (Step 1.3); copy script and download pending |
| 30 | project folder sync: connect a folder and every `.rpy` in it opens automatically; edits made outside (e.g. VS Code) are picked up while you write | Done — connection (Step 5.1), watcher (Step 5.2), conflict safety (Step 5.3) |
| 31 | find & replace (Ctrl+F): case, regex (`.*`, `$1` groups), and whole-word toggles, all-files scope, F3/Shift+F3 to cycle, every match highlighted in the editor | Done — current file (Step 4.4), all open files (Step 4.5) |
| 32 | editor shortcuts: Ctrl+G go to line, Ctrl+/ comment toggle, Alt+↑/↓ move lines, Shift+Alt+↑/↓ duplicate lines, Ctrl+↑/↓ previous/next label, Ctrl+PageUp/PageDown switch tabs, Ctrl+Shift+T reopen closed tab, Ctrl+S save + snapshot, Ctrl+= / Ctrl+− / Ctrl+0 font size — Ctrl+K shows the full cheat sheet | Partial — editing/navigation commands and cheat sheet done (Step 4.8), Ctrl+S save + snapshot done (Step 5.4); reopen-closed pending |
| 33 | status bar with line/column, current label (also shown as a breadcrumb in the header), and selected-word count | Pending |
| 34 | auto-close and selection-wrap for `"` `(` `[`, with type-over for closers and escaped-quote awareness | Done (Step 4.8) |
| 35 | snippets in autocomplete: `label`, `menu`, `define` expand to full blocks with the placeholder selected | Done (Step 4.2) |
| 36 | word wrap toggle for long dialogue lines (hides line numbers while on) | Pending |
| 37 | current-line highlight, adjustable editor font size (persisted) | Pending |
| 38 | collapsible side panels (fold state remembered), focus mode with floating word-count pill, draggable editor/stats divider | Partial — exact-layout focus mode and live word-count pill done (Step 4.9); fold persistence/divider pending |
| 39 | tabs: dirty dot when a file differs from what's on disk, inline rename on double-click | Pending |
| 40 | snapshots: automatic backup every 10 minutes plus manual "Snapshot now"; restore or diff any point (last 50 kept) | Done — storage/retention (Step 5.4), comparison and confirmed restore (Step 5.5) |
| 41 | outline panel: labels with word counts, menu choices, and jumps/calls — click to navigate; jumps to missing labels shown in red | Done (Step 4.1) |
| 42 | routes panel: walks jumps/calls/fall-through from `start` for shortest/longest route word counts and reading time, ending and branch-point counts, and unreachable labels (click to jump) | Pending |
| 43 | branch-aware route details: shows dialogue words inside each inline menu choice and conditional block | Pending |
| 44 | flow map: SVG story graph of labels — gold border marks `start`, red marks endings, dashed red curves are loops back, dashed boxes are unreachable; click a box to jump | Pending |
| 45 | speaker colors: `Character(..., color="#c8ffc8")` tints the alias in the editor and adds swatches to the speaker, balance, and counted-line lists | Pending |
| 46 | autocomplete while typing: speaker aliases at line start, label names after `jump`/`call` (arrows to pick, Tab/Enter to accept, Esc to dismiss) | Done (Step 4.2) |
| 47 | Ren'Py-aware autocomplete for image names after `show`, audio paths after `play`, screen names after `call screen`, and declared variables after `$` | Done (Step 4.3) |
| 48 | inline editor diagnostics for syntax, unresolved-label, quote, speaker, and long-line checks | Done — engine (Step 4.6), squiggles/gutter/tooltips/panel (Step 4.7) |
| 49 | project-wide text search across every `.rpy` file in the connected game folder (or all open tabs offline) | Done — open tabs (Step 4.5), recursively connected folder (Step 5.1) |
| 50 | folder-sync conflict review with a side-by-side local/disk comparison before choosing a version | Done (Step 5.3) |
| 51 | safe project-wide character-alias and label rename with an affected-line preview before applying | Done (Step 5.7) |
| 52 | prose panel: vocabulary size, average sentence length per speaker, and overused words/phrases as clickable chips that open find | Pending |
| 53 | local browser autosave | Pending |
| 54 | optional save folder: pick a directory on your computer (Chrome/Edge) and autosave writes the `.rpy` files there | Pending |
| 55 | tab characters auto-converted to 4 spaces (Ren'Py rejects tabs); "Fix indents" snaps indentation to multiples of 4 | Pending |
