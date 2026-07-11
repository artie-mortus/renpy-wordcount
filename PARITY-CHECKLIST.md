# Feature Parity Checklist

Source: `/home/artemis/Projects/words-til-vn/README.md`. Update throughout development; complete the audit in Step 9.5.

| # | Feature | Status |
|---:|---|---|
| 1 | live Ren'Py editor with line numbers, syntax highlighting, and file tabs | Done — tabs (Step 1.2), line numbers (Step 1.4), syntax highlighting (Step 2.7) |
| 2 | run the project from a chosen source line through the local save server | Pending |
| 3 | Production desk with normal run, current-line warp, Interactive Director, stop/restart controls, live status, and a persistent launch log | Pending |
| 4 | reusable JSON state presets applied before startup (generates `say_count_runtime.rpy` in the connected game folder) | Pending |
| 5 | official Ren'Py lint output alongside Say Count's instant script checks | Pending |
| 6 | project asset browser with image/audio previews and one-click `show` / `play music` insertion | Pending |
| 7 | localization tools for missing-string counts, translation generation, and dialogue export | Pending |
| 8 | translation dashboard groups missing strings by source file, shows available label/speaker context, and opens matching loaded lines | Pending |
| 9 | voice-production tracker with recorded/retake/approved states, optional voice filenames, tracking CSV, and printable per-role VA scripts for characters and narration | Pending |
| 10 | accessibility audit for untranslated controls, missing image-button descriptions, and visually encoded character voices | Pending |
| 11 | automatic playthrough coverage captured from launched games, plus manual label coverage | Pending |
| 12 | persistent continuity notes for characters, locations, timeline, relationships, inventory, and story facts | Pending |
| 13 | multi-file projects: import several `.rpy` files, totals across all | Pending |
| 14 | counts dialogue/narration words, not labels, definitions, jumps, variables, comments, or scene commands | Pending |
| 15 | optional menu choice counting (including conditioned choices) | Pending |
| 16 | `extend` lines attributed to the previous speaker | Pending |
| 17 | speaker breakdown with talk-time balance bars | Done — Step 3.3 (`414ab59`): dockable panel, words-desc ordering, swatches, percentages, balance bars |
| 18 | per-character and per-scene dialogue word and line targets | Pending |
| 19 | uses `define e = Character("Eileen")` names instead of `e` aliases | Pending |
| 20 | total word and line target progress | Pending |
| 21 | reading-time estimate (~200 wpm) and words-written-this-session counter | Pending |
| 22 | version diff: paste an old script to see words added or cut per speaker | Pending |
| 23 | scene/label breakdown | Pending |
| 24 | counted-line speaker/search filters | Pending |
| 25 | CSV/JSON stats export | Pending |
| 26 | complete project import/export, including files, targets, and settings | Pending |
| 27 | standalone HTML report export | Pending |
| 28 | dark mode | Partial — system/light/dark editor themes landed (Step 1.4); app-wide dark mode pending |
| 29 | import `.rpy` files (multi-select or drag & drop onto the editor), copy script, download `.rpy` | Partial — multi-select open and drag & drop landed (Step 1.3); copy script and download pending |
| 30 | project folder sync: connect a folder and every `.rpy` in it opens automatically; edits made outside (e.g. VS Code) are picked up while you write | Pending |
| 31 | find & replace (Ctrl+F): case, regex (`.*`, `$1` groups), and whole-word toggles, all-files scope, F3/Shift+F3 to cycle, every match highlighted in the editor | Pending |
| 32 | editor shortcuts: Ctrl+G go to line, Ctrl+/ comment toggle, Alt+↑/↓ move lines, Shift+Alt+↑/↓ duplicate lines, Ctrl+↑/↓ previous/next label, Ctrl+PageUp/PageDown switch tabs, Ctrl+Shift+T reopen closed tab, Ctrl+S save + snapshot, Ctrl+= / Ctrl+− / Ctrl+0 font size — Ctrl+K shows the full cheat sheet | Partial — Ctrl+= / Ctrl+− / Ctrl+0 font size landed (Step 1.4); rest pending |
| 33 | status bar with line/column, current label (also shown as a breadcrumb in the header), and selected-word count | Pending |
| 34 | auto-close and selection-wrap for `"` `(` `[`, with type-over for closers and escaped-quote awareness | Pending |
| 35 | snippets in autocomplete: `label`, `menu`, `define` expand to full blocks with the placeholder selected | Pending |
| 36 | word wrap toggle for long dialogue lines (hides line numbers while on) | Pending |
| 37 | current-line highlight, adjustable editor font size (persisted) | Pending |
| 38 | collapsible side panels (fold state remembered), focus mode with floating word-count pill, draggable editor/stats divider | Pending |
| 39 | tabs: dirty dot when a file differs from what's on disk, inline rename on double-click | Pending |
| 40 | snapshots: automatic backup every 10 minutes plus manual "Snapshot now"; restore or diff any point (last 50 kept) | Pending |
| 41 | outline panel: labels with word counts, menu choices, and jumps/calls — click to navigate; jumps to missing labels shown in red | Pending |
| 42 | routes panel: walks jumps/calls/fall-through from `start` for shortest/longest route word counts and reading time, ending and branch-point counts, and unreachable labels (click to jump) | Pending |
| 43 | branch-aware route details: shows dialogue words inside each inline menu choice and conditional block | Pending |
| 44 | flow map: SVG story graph of labels — gold border marks `start`, red marks endings, dashed red curves are loops back, dashed boxes are unreachable; click a box to jump | Pending |
| 45 | speaker colors: `Character(..., color="#c8ffc8")` tints the alias in the editor and adds swatches to the speaker, balance, and counted-line lists | Pending |
| 46 | autocomplete while typing: speaker aliases at line start, label names after `jump`/`call` (arrows to pick, Tab/Enter to accept, Esc to dismiss) | Pending |
| 47 | Ren'Py-aware autocomplete for image names after `show`, audio paths after `play`, screen names after `call screen`, and declared variables after `$` | Pending |
| 48 | inline editor diagnostics for syntax, unresolved-label, quote, speaker, and long-line checks | Partial — core lint engine (syntax, duplicate/missing label, empty block, python opacity, support-file suppression) landed (Step 2.6B); editor UI pending |
| 49 | project-wide text search across every `.rpy` file in the connected game folder (or all open tabs offline) | Pending |
| 50 | folder-sync conflict review with a side-by-side local/disk comparison before choosing a version | Pending |
| 51 | safe project-wide character-alias and label rename with an affected-line preview before applying | Pending |
| 52 | prose panel: vocabulary size, average sentence length per speaker, and overused words/phrases as clickable chips that open find | Pending |
| 53 | local browser autosave | Pending |
| 54 | optional save folder: pick a directory on your computer (Chrome/Edge) and autosave writes the `.rpy` files there | Pending |
| 55 | tab characters auto-converted to 4 spaces (Ren'Py rejects tabs); "Fix indents" snaps indentation to multiples of 4 | Pending |

