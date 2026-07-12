# Say Count Native — Self-Contained Conversion Plan

**Source repo:** `/home/artemis/Projects/words-til-vn` (branch `master`, commit `ea19811`, 2026-07-10)
**Target:** `/home/artemis/Projects/say-count-native/`.
**Stack:** C++17 · wxWidgets 3.2+ (static link) · wxStyledTextCtrl (Scintilla) · CMake · Catch2 for tests.
**Platform:** Linux only; Windows/macOS packaging out of scope.
**Execution:** complete session-by-session step breakdown is included below; this file governs both execution and architecture.

---

## 1. Why this stack

Say Count today is a browser PWA (~6,500 lines JS in 15 modules + 1,241 CSS + 356 HTML) plus a 467-line Python helper server (`serve.py`) that exists only because browsers can't touch the filesystem or spawn Ren'Py. The goal is one lightweight native program: small static binary, low RAM, no browser, no Python, no HTTP layer.

The dominant conversion cost is the code editor. The JS app hand-builds editor machinery on top of a `<textarea>`: syntax highlight overlay, autocomplete popups, find/replace with regex groups, diagnostics underlines, bracket type-over, line operations. **Scintilla (via wxStyledTextCtrl) provides all of this as API calls** — styling, AutoComp lists, call tips, indicators (squiggles), annotations, folding, markers, search with regex. That single fact cuts the estimate roughly in half versus Rust/egui or Go.

wxWidgets uses native widgets per platform, statically links into a ~10–15MB binary, idles at ~30–50MB RAM, and starts instantly.

**Estimate: ~20–30M tokens, 2–3 weeks of agent sessions.** Milestones 1–4 produce a usable daily-driver editor; 5–7 reach core parity; M8 is deferred long tail. With mini-model tiers on mechanical steps and M8 off the critical path (see usage-safe plan), the critical path drops to roughly **12–18M full-model-equivalent tokens**.

## 2. Architecture mapping

| Current (JS/Python) | Native replacement |
|---|---|
| `serve.py` — HTTP API: save/list/delete `.rpy`, launch/stop Ren'Py, lint, translate, asset listing, coverage tail, `.save-folder` config | **Deleted as a concept.** Direct filesystem access, `wxProcess` for Ren'Py launch/stop, read coverage JSONL directly. No HTTP, no CORS, no origin checks |
| `parser.js` (514), `analysis.js` (144), `routes.js` (166), `highlight.js` (84) — pure logic | `core/` static library, zero wx dependency: parser, word counts, speaker stats, route walker, diagnostics, prose analysis. Unit-tested with Catch2 |
| `editor.js`, `editor-ui.js` (555), `tabs.js` — textarea editor + overlay | `wxStyledTextCtrl` with a custom Ren'Py container lexer (`SCLEX_CONTAINER`, styled from `core/` tokenizer); `wxAuiNotebook` tabs with dirty-dot + inline rename |
| `app.js` (1016), `app-core.js` (899) — panel wiring, app state | `wxAuiManager` docked/collapsible panels; central `AppState` struct; JSON persistence |
| `story-tools.js` (1007) — voice tracker, translation dashboard, continuity notes, coverage, accessibility audit | Panel classes over `wxDataViewCtrl`; CSV/print export via `wxHtmlEasyPrinting` |
| `stats-panels.js` (407) — speaker breakdown, balance bars, targets | Custom-drawn panels with `wxGraphicsContext` |
| Flow map (SVG in browser) | Custom `wxPanel` + `wxGraphicsContext` drawing; hit-test rectangles for click-to-jump |
| `persistence.js` (localStorage), snapshots (last 50) | JSON settings + snapshot files under `wxStandardPaths::GetUserDataDir()` (XDG on Linux) |
| `project-io.js` — project import/export | Same JSON schema, read/write via `std::filesystem` — **keep format compatible so existing exported projects import cleanly** |
| `sw.js`, `manifest.webmanifest`, PWA install | Not applicable — native app is inherently offline |
| Find/replace (Ctrl+F), shortcuts, folder sync | wxSTC `SearchInTarget` + `wxRegEx`; `wxAcceleratorTable`; `wxFileSystemWatcher` for external-edit pickup + conflict review |
| `start-say-count.sh`, `say-count.desktop`, browser juggling | Just run the binary; ship a `.desktop` file pointing at it |

## 3. Milestones

Each milestone is independently buildable and verifiable. Implement in order; later milestones depend on earlier ones.

### M1 — Skeleton (~1–2M tokens)
- CMake project, wxWidgets static build (system package first; vendored via `FetchContent` as fallback).
- Main frame, menu bar, `wxAuiNotebook` tabs, `wxStyledTextCtrl` editor.
- Ren'Py syntax highlighting via container lexer fed by a `core/` tokenizer (say lines, speakers, strings, keywords, comments, Python lines).
- Open/save/import `.rpy` (multi-select + drag-drop), tab-to-4-spaces conversion on load.
- Line numbers, current-line highlight, adjustable font size (persisted), word wrap toggle.
- Dark mode via `wxSystemAppearance` + custom STC style set.
- **Verify:** open a real Ren'Py script, edit, save, diff against original — byte-identical except edits.

### M2 — Core port + tests (~2–3M tokens) — correctness anchor
- Port `parser.js` counting rules: dialogue/narration counted; labels, defines, jumps, `$` lines, comments, scene/show ignored; `extend` attributed to previous speaker; optional menu-choice counting incl. conditioned choices; `Character("Eileen")` name resolution; speaker colors.
- Port `analysis.js`: totals, per-speaker, per-label, reading time (~200 wpm), session counter.
- **Translate every fixture in `test/parser.test.js` (295 lines) 1:1 to Catch2.** Cross-check: run both JS and C++ counters on the same sample project; totals must match exactly.
- **Build the parity harness early (not at the end):** a node script that runs `parser.js` over a fixture and prints JSON, a `say-count --dump-json` CLI mode printing the same shape, and a ctest that diffs them. Every later milestone's "matches the JS app" verification runs through this harness, not eyeballs.
- **Word-count Unicode decision:** `parser.js:121` counts words with `/[\p{L}\p{N}]+(?:['’\-][\p{L}\p{N}]+)*/gu` — Unicode property classes `std::regex` cannot express. Decide ICU vs hand-rolled UTF-8 letter/number classifier in `DECISIONS.md` before writing the tokenizer.
- **Verify:** `ctest` green; parity harness diff empty on identical input.

### M3 — Stats UI (~2–3M tokens)
- Speaker breakdown with talk-time balance bars and color swatches; per-character/per-scene word+line targets with progress; total target progress; scene/label breakdown; counted-line list with speaker/search filters; version diff (paste old script → words added/cut per speaker); CSV/JSON stats export; standalone HTML report export.
- **Verify:** side-by-side with JS app on same project — same numbers, same orderings.

### M4 — Editor features (~3–5M tokens)
- Autocomplete via STC AutoComp: speaker aliases at line start; label names after `jump`/`call`; image names after `show`; audio after `play`; screens after `call screen`; variables after `$`. Snippets (`label`/`menu`/`define`) with placeholder selection.
- Find/replace dialog: case, regex with `$1` groups, whole-word, all-files scope, F3/Shift+F3, highlight-all via indicators.
- Inline diagnostics (syntax, unresolved label, quote, speaker, long-line) as squiggle indicators + gutter markers.
- Outline panel (labels with word counts, menu choices, jumps/calls; missing-label jumps red); status bar (line/col, current label breadcrumb, selected-word count); go-to-line, comment toggle, line move/duplicate, prev/next label, bracket auto-close with type-over and escaped-quote awareness, Ctrl+K cheat sheet; focus mode with floating word-count pill.
- **Verify:** keyboard-drive every shortcut from README list against sample script.

### M5 — Project features (~3–4M tokens)
- Folder connect: every `.rpy` opens; `wxFileSystemWatcher` picks up external edits; conflict review dialog with side-by-side local/disk comparison.
- Snapshots: auto every 10 min + manual, keep 50, restore or diff any point.
- Project import/export (**JSON schema identical to JS app** — migration path for existing users), safe project-wide alias/label rename with affected-line preview, project-wide text search.
- **Verify:** export project from JS app → import into native app → identical counts and settings. Edit a file in VS Code while native app open → change picked up.

### M6 — Ren'Py integration (~2–4M tokens)
- Launch/stop via `wxProcess`: normal run, `--warp` from caret line, Interactive Director; `RENPY_EXECUTABLE` setting + PATH lookup; live status + persistent launch log panel.
- State presets → generate `say_count_runtime.rpy` (port template from `serve.py:410`); playthrough coverage via label-callback JSONL, tailed live; manual label coverage.
- Official lint output panel; translation generation; dialogue export.
- Asset browser: image/audio preview, one-click `show` / `play music` insertion.
- **Verify:** against a real Ren'Py SDK — launch, warp to line, preset applied, coverage recorded, stop works.

### M7 — Routes + flow map (~2–3M tokens)
- Port `routes.js` walker: jumps/calls/fall-through from `start`, shortest/longest route words + reading time, ending and branch-point counts, unreachable labels; branch-aware details (words per inline menu choice / conditional block).
- Flow map panel: `wxGraphicsContext` graph — gold border `start`, red endings, dashed red loop-backs, dashed unreachable; click node → jump to label.
- **Verify:** same route numbers as JS app on a branching sample; click-to-jump lands on correct line.

### M8 — Long tail (~3–4M tokens) — deferred
**Not on the daily-driver critical path.** Run after M9 packaging, or skip until actually wanted.
- Prose panel (vocabulary size, avg sentence length per speaker, overused words/phrases → click opens find); voice-production tracker (recorded/retake/approved, voice filenames, tracking CSV, printable per-role VA scripts); translation dashboard (missing strings grouped by file, label/speaker context, open matching line); continuity notes (characters/locations/timeline/relationships/inventory/facts); accessibility audit; "Fix indents".
- **Verify:** walk README's 82-item feature list as final parity checklist.

### M9 — Packaging (~0.5M tokens)
- Static release build (`-O2`, stripped), `.desktop` file + `icon.svg`, README with build instructions, optional AppImage.
- **Verify:** binary runs on clean system without wxWidgets installed; size ≤ ~20MB.

## 4. Risks and unknowns

| Risk | Mitigation |
|---|---|
| Parser edge cases diverge from JS (regex semantics differ in `std::regex`) | M2 fixture translation is the gate; prefer hand-written tokenizer over regex where JS relied on subtle regex behavior. `std::regex` is also slow — fine at this file scale, but tokenizer avoids it in hot paths |
| Word counting uses Unicode property classes (`\p{L}\p{N}` in `parser.js:121`) that `std::regex` cannot express — "exact match" parity breaks silently on non-ASCII text | Decide ICU vs hand-rolled UTF-8 classifier in `DECISIONS.md` before the tokenizer; parity harness fixtures must include non-ASCII names and dialogue |
| wxSTC autocomplete/calltip styling less flexible than DOM popups | Accept stock look; function over pixel parity |
| Flow map layout (JS likely uses simple layered layout) | Port the same layout algorithm; don't reach for graphviz |
| Static wxWidgets build friction on Arch | System `wxwidgets-gtk3` for dev; static/`FetchContent` only for release packaging |
| `wxSystemAppearance` dark-mode gaps on GTK | Explicit STC + panel color scheme, toggleable, persisted |
| Interactive Director / Ren'Py flags drift between SDK versions | Same invocation strings as `serve.py`; treat SDK ≥ 8.x as supported |

## 5. Do not change

- **The JS project (`words-til-vn`) is frozen as reference.** No edits, no commits there. It's the behavioral oracle for every milestone.
- Project export JSON schema — keep byte-compatible until native app is the daily driver.
- `.rpy` on-disk format handling (4-space indent conversion) — must match, files feed straight into Ren'Py.

## 6. Verification commands (native repo)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug && cmake --build build -j
ctest --test-dir build --output-on-failure    # includes parity-harness diffs
./build/say-count            # manual drive
# parity harness: node tools/parity-dump.mjs <fixture> vs ./build/say-count --dump-json <fixture>, diff must be empty
cd /home/artemis/Projects/words-til-vn && npm test   # reference suite still green (untouched)
```

## 7. Session discipline (usage-limit safety)

Milestones are decomposed into one-session steps in `say-count-native-usage-safe-plan.html`; sessions run against that step list, not raw milestones. Rules that keep usage-limit cutoffs cheap:

- **One step per fresh session.** Check the remaining usage window first; if smaller than the step's ceiling, run a filler step (4.9, 6.1, 9.1, 9.3) or stop.
- **Model tier per step:** mini model for mechanical steps (boilerplate, UI wiring, exports, packaging), full model for design-heavy ones (parser, diagnostics, routing, compatibility). Tiers are tagged on each step card.
- **`wip:` commits during the step** — every compiling, test-passing increment. Squash at step end. A cutoff loses minutes, not the session.
- **Read only the files listed for the step in `PLAN.md`** (per-step Files-to-read list built in Step 0.1). No re-exploring either repo.
- **Checkpoint files in the native repo:** `STATUS.md` (under 40 lines, updated every session), `DECISIONS.md` (created Step 0.1; records tokenizer, Unicode word-count, JSON-compat choices), `PARITY-CHECKLIST.md` (seeded Step 0.1, completed at release).
- **Recovery after a cutoff:** new session, `git status`/`diff`/`log`, resume from the newest `wip:` commit, finish only the interrupted step.

For each step, hand the agent the reusable prompt from the usage-safe plan:

> Use `PLAN.md` in `/home/artemis/Projects/say-count-native/` as source of truth. Implement Step <N> only. Reference (read-only!) the JS implementation at `/home/artemis/Projects/words-til-vn/` for exact behavior, reading only the files listed for the step. Restate the step's tasks before editing, commit wip increments, run the verification steps listed, update STATUS.md, squash, report results.

# 8. Complete session-by-session step list

Every future session begins here. Each card preserves the source plan's tasks, token ceiling, model tier, and verification. Read only its listed files. The JavaScript paths are strictly read-only.

## Phase 0 — Repository setup

### Step 0.1 — Create the native repository

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/README.md`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/PARITY-CHECKLIST.md`, `/home/artemis/Projects/say-count-native/.gitignore`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/README.md`, `/home/artemis/Projects/words-til-vn/package.json`.

**Token ceiling / model tier:** 200,000–300,000 tokens · mini model

**Tasks:**

- Create `/home/artemis/Projects/say-count-native/`.
- Initialize Git.
- Add `README.md`, `PLAN.md`, `STATUS.md`, `DECISIONS.md`, `PARITY-CHECKLIST.md`, `.gitignore`, and top-level `CMakeLists.txt`.
- Populate `PLAN.md` by adapting `say-count-native-plan/plan.md` plus the full step list from this document — every session begins by reading it.
- Add a per-step **Files to read** list to `PLAN.md` (exact source files in both repositories) so later sessions never re-explore.
- Seed `PARITY-CHECKLIST.md` with one row per feature from the JavaScript app, all marked pending.
- Create `src/app`, `src/core`, `src/ui`, `tests`, and `resources`.
- Document that the JavaScript repository is read-only.

**Verification:**

- CMake configures successfully.
- Git shows only expected files.
- Initial commit exists.

### Step 0.2 — Establish the build system

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/README.md`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/PARITY-CHECKLIST.md`, `/home/artemis/Projects/say-count-native/.gitignore`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/README.md`, `/home/artemis/Projects/words-til-vn/package.json`.

**Token ceiling / model tier:** 250,000–400,000 tokens · mini model

**Tasks:**

- Require C++17.
- Detect wxWidgets 3.2 or newer.
- Support system wxWidgets for development.
- Add targets `say_count_core`, `say-count`, and `say-count-tests`.
- Add Catch2 and a smoke test.

**Verification:**

- Run the standard CMake build and `ctest` commands successfully.

## Phase 1 — Application skeleton

### Step 1.1 — Main window and application lifecycle

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/styles.css`.

**Token ceiling / model tier:** 300,000–450,000 tokens · mini model

**Tasks:**

- Implement `wxApp` and the main frame.
- Add File, Edit, View, and Help menus.
- Add a status bar.
- Persist window size and position.
- Handle clean startup and shutdown.

**Verification:**

- Application starts and exits cleanly.
- Window state persists.

### Step 1.2 — Notebook and editor tabs

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/styles.css`.

**Token ceiling / model tier:** 350,000–500,000 tokens · mini model

**Tasks:**

- Add `wxAuiNotebook`.
- Add one `wxStyledTextCtrl` per tab.
- Support new, close, select, and dirty-state behavior.
- Warn before closing modified documents.

**Verification:**

- Multiple tabs work.
- Dirty state appears after editing.
- Unsaved changes trigger a warning.

### Step 1.3 — File opening and saving

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/styles.css`.

**Token ceiling / model tier:** 400,000–600,000 tokens · mini model

**Tasks:**

- Open one or multiple `.rpy` files.
- Implement Save and Save As.
- Support drag and drop.
- Track file paths per tab.
- Convert tabs to four spaces on load.

**Verification:**

- Open, edit, save, and compare a real Ren'Py script.
- Confirm multiple files open correctly.

### Step 1.4 — Basic editor configuration

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/styles.css`.

**Token ceiling / model tier:** 300,000–450,000 tokens · mini model

**Tasks:**

- Add line numbers and current-line highlighting.
- Add word-wrap and font-size controls.
- Persist editor settings.
- Add light and dark editor styles.
- Use system appearance where practical.

**Verification:**

- Settings survive restart.
- Both themes are readable.
- Wrap and font controls work.

## Phase 2 — Tokenizer and parser

### Step 2.1 — Inventory parser behavior

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/tests/core_smoke_test.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/dom-stubs.js`.

**Token ceiling / model tier:** 300,000–500,000 tokens · full model

**Tasks:**

- Inspect `parser.js`, `analysis.js`, `routes.js`, `highlight.js`, and `test/parser.test.js`.
- Create `docs/parser-behavior.md`.
- Document line types, ignored lines, speaker rules, menu rules, character resolution, and edge cases.
- Document the word-count rule: `parser.js` matches `/[\p{L}\p{N}]+(?:['’\-][\p{L}\p{N}]+)*/gu` — Unicode property classes that `std::regex` cannot express. Record the chosen replacement (ICU, or a hand-rolled UTF-8 letter/number classifier) in `DECISIONS.md` before Step 2.2 begins.
- Do not implement the parser yet.

**Verification:**

- Every JavaScript parser fixture maps to a documented behavior.
- No JavaScript source files are changed.

### Step 2.2 — Line tokenizer

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/tests/core_smoke_test.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/dom-stubs.js`.

**Token ceiling / model tier:** 500,000–700,000 tokens · full model

**Tasks:**

- Implement a hand-written Ren'Py line tokenizer.
- Recognize blank lines, comments, labels, dialogue, narration, defines, jumps, calls, menus, Python, scene/show/play statements.
- Record line number and indentation.
- Avoid overusing `std::regex`; word counting must use the Unicode-aware approach chosen in `DECISIONS.md` (Step 2.1), not `std::regex`.

**Verification:**

- Add tokenizer tests for escaped quotes, comments, indentation, and malformed input.

### Step 2.3 — Core dialogue counting

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/tests/core_smoke_test.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/dom-stubs.js`.

**Token ceiling / model tier:** 500,000–750,000 tokens · full model

**Tasks:**

- Count dialogue and narration.
- Ignore code and control statements.
- Associate dialogue with speakers.
- Implement `extend`.
- Track line numbers and current label.
- Build the parity harness: a node script that runs `parser.js` over a fixture and prints JSON, a native CLI mode (`say-count --dump-json`) that prints the same shape, and a ctest that diffs them.

**Verification:**

- Translate simple JS fixtures into Catch2.
- Parity harness diff is empty for all current fixtures.

### Step 2.4 — Character definitions and display names

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/tests/core_smoke_test.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/dom-stubs.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · full model

**Tasks:**

- Parse `Character(...)` definitions.
- Resolve aliases to display names.
- Parse speaker colors when available.
- Handle unknown speakers safely.

**Verification:**

- Names and colors match via the parity harness from Step 2.3.
- Unknown aliases do not crash.

### Step 2.5 — Menus and conditioned choices

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/tests/core_smoke_test.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/dom-stubs.js`.

**Token ceiling / model tier:** 450,000–700,000 tokens · full model

**Tasks:**

- Detect menu choices.
- Support counting or ignoring menu-choice text.
- Handle conditioned choices.
- Preserve label and indentation context.

**Verification:**

- Menu totals match via the parity harness from Step 2.3.
- Both menu-count settings are tested.

### Step 2.6 — Complete parser fixture translation

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/tests/core_smoke_test.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/dom-stubs.js`.

**Token ceiling / model tier:** 700,000–1,000,000 tokens · full model

**Tasks:**

- Translate every fixture from `test/parser.test.js` into Catch2.
- Organize tests by behavior.
- Run the parity harness (Step 2.3) over every fixture.
- Fix all divergences.
- Split into 2.6A, 2.6B, and 2.6C if needed.

**Verification:**

- All translated fixtures pass.
- Parity harness diff is empty for every fixture — word totals exactly match.

### Step 2.7 — Syntax highlighting

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/tests/core_smoke_test.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/dom-stubs.js`.

**Token ceiling / model tier:** 500,000–750,000 tokens · mini model

**Tasks:**

- Connect the tokenizer to `wxStyledTextCtrl`.
- Style comments, keywords, labels, speakers, strings, Python lines, and Ren'Py statements.
- Restyle only affected ranges where practical.

**Verification:**

- Open real `.rpy` files.
- Confirm styling survives edits.
- Confirm large files remain responsive.

## Phase 3 — Core analysis and statistics

### Step 3.1 — Analysis data model

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`.

**Token ceiling / model tier:** 450,000–650,000 tokens · full model

**Tasks:**

- Port totals, dialogue, narration, per-speaker, per-label, and reading-time calculations.
- Extend the parity harness to cover analysis output.
- Add unit tests.

**Verification:**

- Native output matches the JavaScript app via the parity harness.

### Step 3.2 — Live document analysis

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`.

**Token ceiling / model tier:** 350,000–550,000 tokens · mini model

**Tasks:**

- Reanalyze after edits with debounce.
- Show basic totals in the status bar.
- Avoid blocking typing.

**Verification:**

- Counts update correctly.
- Large documents stay responsive.
- Tab switching updates totals.

### Step 3.3 — Speaker statistics panel

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`.

**Token ceiling / model tier:** 450,000–700,000 tokens · mini model

**Tasks:**

- Add a dockable speaker panel.
- Show name, color, words, lines, percentage, and balance bar.
- Match JavaScript ordering.

**Verification:**

- Numbers and ordering match the JavaScript app.

### Step 3.4 — Scene and label statistics

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · mini model

**Tasks:**

- Add per-label and per-scene statistics.
- Add project, character, and scene targets.
- Show progress values and bars.

**Verification:**

- Compare totals against the JavaScript app.

### Step 3.5 — Counted-lines browser

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/parser.h`, `/home/artemis/Projects/say-count-native/src/core/parser.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/main_frame.h`, `/home/artemis/Projects/say-count-native/src/ui/main_frame.cpp`, `/home/artemis/Projects/say-count-native/src/ui/editor_notebook.h`, `/home/artemis/Projects/say-count-native/src/ui/editor_notebook.cpp`, `/home/artemis/Projects/say-count-native/src/ui/speaker_stats_panel.h`, `/home/artemis/Projects/say-count-native/src/ui/speaker_stats_panel.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`.

**Token ceiling / model tier:** 400,000–600,000 tokens · mini model

**Tasks:**

- List counted lines.
- Filter by speaker, text, and label.
- Double-click to jump to the source line.

**Verification:**

- Rows map to correct lines.
- Filters update correctly.

### Step 3.6 — Statistics export

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · mini model

**Tasks:**

- Export CSV, JSON, and standalone HTML reports.
- Preserve Unicode.

**Verification:**

- Open and validate each export.
- Test non-ASCII names and dialogue.

### Step 3.7 — Version comparison

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/test/parser.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`.

**Token ceiling / model tier:** 450,000–700,000 tokens · mini model

**Tasks:**

- Load or paste an older script.
- Calculate added, removed, net, and per-speaker changes.
- Display comparison results.

**Verification:**

- Validate using a controlled old/new fixture.

## Phase 4 — Editor productivity features

### Step 4.1 — Outline panel

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/find-replace.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 500,000–750,000 tokens · mini model

**Tasks:**

- Show labels, menu choices, jumps, calls, and word counts.
- Double-click to jump.
- Highlight unresolved targets.

**Verification:**

- Outline updates after edits.
- Navigation is accurate.

### Step 4.2 — Basic autocomplete

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/find-replace.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 500,000–750,000 tokens · full model

**Tasks:**

- Complete speaker aliases, labels after `jump`/`call`, and snippets for `label`, `menu`, and `define`.
- Support placeholder selection.

**Verification:**

- Completions appear only in appropriate contexts.

### Step 4.3 — Asset-aware autocomplete

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/find-replace.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 550,000–850,000 tokens · full model

**Tasks:**

- Complete images, audio, screens, and variables from the open project.
- Refresh indexes after project changes.

**Verification:**

- Completion reflects current project contents.

### Step 4.4 — Find in current file

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/find-replace.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 450,000–700,000 tokens · mini model

**Tasks:**

- Implement find, replace, replace all, case, whole word, regex, F3, Shift+F3, and highlight-all.

**Verification:**

- Test literal, regex, capture-group, and zero-match cases.

### Step 4.5 — Find across files

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/find-replace.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 500,000–800,000 tokens · mini model

**Tasks:**

- Add all-files search.
- Show file, line, and preview.
- Add project-wide replace preview and confirmation.

**Verification:**

- Confirm only selected files are changed.

### Step 4.6 — Diagnostics engine

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/find-replace.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 600,000–900,000 tokens · full model

**Tasks:**

- Detect unresolved labels, unclosed quotes, unknown speakers, suspicious indentation, long lines, and basic syntax issues.
- Keep diagnostics independent from UI.
- Add tests.
- Split into 4.6A (engine and label/quote rules) and 4.6B (remaining rules and tests) if needed.

**Verification:**

- Controlled fixtures produce expected diagnostics.
- Valid fixtures have no false errors.

### Step 4.7 — Diagnostics UI

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/find-replace.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · mini model

**Tasks:**

- Add squiggles, gutter markers, tooltips, and a diagnostics panel.
- Click to jump.

**Verification:**

- Indicators update after fixes.
- Stale diagnostics disappear.

### Step 4.8 — Editing commands and shortcuts

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/find-replace.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 500,000–750,000 tokens · mini model

**Tasks:**

- Add go-to-line, comment toggle, move, duplicate, previous/next label, auto-close, type-over, escaped-quote handling, and a shortcut cheat sheet.

**Verification:**

- Keyboard-test every command.
- Confirm indentation is preserved.

### Step 4.9 — Focus mode

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/editor.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/highlight.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/highlight.test.js`, `/home/artemis/Projects/words-til-vn/test/find-replace.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 250,000–400,000 tokens · mini model

**Tasks:**

- Hide nonessential panels.
- Add a floating word-count display.
- Preserve and restore the previous layout.

**Verification:**

- Focus mode restores the exact prior layout.

## Phase 5 — Project management

### Step 5.1 — Project folder model

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/project-io.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 450,000–700,000 tokens · mini model

**Tasks:**

- Connect a Ren'Py project folder.
- Discover `.rpy` files.
- Track project root separately from open files.
- Persist recent projects.

**Verification:**

- Open a real project and confirm expected scripts are found.

### Step 5.2 — External file watching

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/project-io.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · mini model

**Tasks:**

- Add `wxFileSystemWatcher`.
- Detect external changes.
- Reload clean files safely.
- Never overwrite dirty files silently.

**Verification:**

- Modify a file in another editor and confirm detection.

### Step 5.3 — Conflict review

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/project-io.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 500,000–800,000 tokens · full model

**Tasks:**

- Show local and disk versions side by side.
- Allow keep local, use disk, or save elsewhere.
- Show changed-line highlighting.

**Verification:**

- Simulate external edits while dirty and confirm no data loss.

### Step 5.4 — Snapshot storage

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/project-io.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 450,000–700,000 tokens · mini model

**Tasks:**

- Add manual and ten-minute automatic snapshots.
- Keep the newest 50.
- Store snapshots in the user-data directory.
- Include metadata.

**Verification:**

- Create, list, and prune snapshots.

### Step 5.5 — Snapshot restore and comparison

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/project-io.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 450,000–700,000 tokens · mini model

**Tasks:**

- Preview, compare, and restore snapshots.
- Require confirmation before replacement.

**Verification:**

- Restore a controlled snapshot and verify dirty-state behavior.

### Step 5.6 — Import and export compatibility

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/project-io.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 600,000–900,000 tokens · full model

**Tasks:**

- Implement the existing project JSON schema.
- Import JavaScript-app exports.
- Export data the JavaScript app can read.
- Preserve compatible unknown fields where practical.
- Split into 5.6A (import) and 5.6B (export and round-trip) if needed.

**Verification:**

- Round-trip a real project export.

### Step 5.7 — Safe symbol renaming

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/tabs.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/project-io.test.js`, `/home/artemis/Projects/words-til-vn/test/snapshots.test.js`, `/home/artemis/Projects/words-til-vn/test/tabs.test.js`.

**Token ceiling / model tier:** 600,000–900,000 tokens · full model

**Tasks:**

- Rename speaker aliases and labels project-wide.
- Show a preview.
- Avoid comments and unrelated strings where possible.
- Create a snapshot first.
- Split into 5.7A (rename engine and preview) and 5.7B (apply and reference updates) if needed.

**Verification:**

- Test similar substrings.
- Confirm jump/call references update.

## Phase 6 — Ren'Py integration

### Step 6.1 — Ren'Py SDK detection and settings

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/test/test_server.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 350,000–550,000 tokens · mini model

**Tasks:**

- Add SDK path setting.
- Check configured path, environment variable, and PATH.
- Validate the executable.
- Display detected version.

**Verification:**

- Test valid and invalid paths.
- Confirm persistence.

### Step 6.2 — Launch and stop Ren'Py

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/test/test_server.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 500,000–800,000 tokens · mini model

**Tasks:**

- Launch with `wxProcess`.
- Capture stdout and stderr.
- Add stop, status, and persistent log.

**Verification:**

- Launch and stop a real project.
- Show useful errors on failure.

### Step 6.3 — Warp and Interactive Director

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/test/test_server.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 450,000–750,000 tokens · full model

**Tasks:**

- Launch from caret with `--warp`.
- Add Interactive Director behavior.
- Reuse Python helper semantics.
- Handle unsupported SDK versions.

**Verification:**

- Warp to a known location.
- Confirm commands are logged.

### Step 6.4 — Runtime state presets

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/test/test_server.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 550,000–850,000 tokens · full model

**Tasks:**

- Port runtime-generation logic from `serve.py`.
- Generate `say_count_runtime.rpy`.
- Support saved presets.
- Protect unrelated files.

**Verification:**

- Generate and launch a preset successfully.

### Step 6.5 — Lint integration

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/test/test_server.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · mini model

**Tasks:**

- Run Ren'Py lint.
- Capture and parse file/line output.
- Display results and support jump-to-source.

**Verification:**

- Test clean and intentionally broken projects.

### Step 6.6 — Translation and dialogue commands

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/test/test_server.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · mini model

**Tasks:**

- Add translation generation and dialogue export.
- Display progress and output.
- Prevent overlapping operations.

**Verification:**

- Generate translations and export dialogue.

### Step 6.7 — Asset browser

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/test/test_server.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 550,000–850,000 tokens · mini model

**Tasks:**

- Discover image and audio assets.
- Preview images and audio.
- Insert `show`, `scene`, `play music`, and `play sound` commands.

**Verification:**

- Browse and insert assets in a real project.

### Step 6.8 — Coverage collection

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/project-io.js`, `/home/artemis/Projects/words-til-vn/test/test_server.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 600,000–900,000 tokens · full model

**Tasks:**

- Generate label callback instrumentation.
- Read and tail coverage JSONL.
- Track manual and playthrough coverage.
- Split into 6.8A (instrumentation) and 6.8B (tailing and coverage tracking) if needed.

**Verification:**

- Visit labels and confirm live coverage updates.

## Phase 7 — Routes and flow map

### Step 7.1 — Route graph data model

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/routes.test.js`.

**Token ceiling / model tier:** 550,000–850,000 tokens · full model

**Tasks:**

- Parse jumps, calls, returns, fall-through, menus, and conditional blocks.
- Build a label-transition graph.
- Add graph-construction tests.

**Verification:**

- Graph matches controlled branching fixtures.

### Step 7.2 — Route walker

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/routes.test.js`.

**Token ceiling / model tier:** 650,000–950,000 tokens · full model

**Tasks:**

- Port route traversal.
- Calculate shortest/longest routes, words, reading time, endings, branches, unreachable labels, and loops.
- Split into 7.2A (traversal) and 7.2B (metrics) if needed.

**Verification:**

- Compare output with the JavaScript app.

### Step 7.3 — Route details UI

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/routes.test.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · mini model

**Tasks:**

- Add route summaries, expandable paths, and jump-to-label/branch behavior.

**Verification:**

- UI matches tested route output.

### Step 7.4 — Basic flow map rendering

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/routes.test.js`.

**Token ceiling / model tier:** 550,000–850,000 tokens · full model

**Tasks:**

- Render nodes and edges with `wxGraphicsContext`.
- Add scrolling and zoom.
- Distinguish start, endings, normal, and unreachable labels.

**Verification:**

- Render controlled graphs legibly.

### Step 7.5 — Flow map layout and interaction

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/routes.js`, `/home/artemis/Projects/words-til-vn/parser.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/styles.css`, `/home/artemis/Projects/words-til-vn/index.html`, `/home/artemis/Projects/words-til-vn/test/routes.test.js`.

**Token ceiling / model tier:** 650,000–1,000,000 tokens · full model

**Tasks:**

- Port layout behavior where practical.
- Add loop-back/unreachable styling, hit testing, click-to-jump, fit-to-view, and edit refresh.
- Split into 7.5A and 7.5B if needed.

**Verification:**

- Click every test node and verify navigation.

## Phase 8 — Long-tail production tools (deferred)

### Step 8.1 — Prose analysis engine

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 500,000–800,000 tokens · full model

**Tasks:**

- Calculate vocabulary size, sentence length, per-speaker metrics, repeated words, and repeated phrases.
- Add tests.

**Verification:**

- Compare with controlled fixtures.

### Step 8.2 — Prose analysis UI

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · mini model

**Tasks:**

- Add prose panel, click-to-search, speaker filters, and common-word exclusions.

**Verification:**

- Clicking a term opens the correct search.

### Step 8.3 — Voice-production tracker model

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 550,000–850,000 tokens · mini model

**Tasks:**

- Track not recorded, recorded, retake, and approved states.
- Track filenames and notes.
- Persist stable line IDs where practical.

**Verification:**

- Save and reload tracker state.

### Step 8.4 — Voice-production exports

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 450,000–700,000 tokens · mini model

**Tasks:**

- Export tracking CSV.
- Generate printable per-role scripts.
- Include source context optionally.

**Verification:**

- Open exports and compare role totals.

### Step 8.5 — Translation dashboard

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 600,000–900,000 tokens · mini model

**Tasks:**

- Detect missing translations.
- Group by file.
- Show label/speaker context.
- Jump to source and translation lines.

**Verification:**

- Test with a partially translated project.

### Step 8.6 — Continuity notes

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 400,000–650,000 tokens · mini model

**Tasks:**

- Add project-level and line-level notes.
- Support filtering, search, persistence, and jump-to-source.

**Verification:**

- Create, edit, search, and restore notes.

### Step 8.7 — Accessibility audit

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 450,000–750,000 tokens · mini model

**Tasks:**

- Port accessibility checks.
- Group by severity and type.
- Add click-to-jump and acknowledged findings where supported.

**Verification:**

- Test good and bad fixtures.

### Step 8.8 — Fix indents

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/src/core/version.h`, `/home/artemis/Projects/say-count-native/src/core/version.cpp`, `/home/artemis/Projects/say-count-native/src/app/main.cpp`, `/home/artemis/Projects/say-count-native/src/ui/placeholder.cpp`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/STATUS.md`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/analysis.js`, `/home/artemis/Projects/words-til-vn/story-tools.js`, `/home/artemis/Projects/words-til-vn/stats-panels.js`, `/home/artemis/Projects/words-til-vn/app-core.js`, `/home/artemis/Projects/words-til-vn/app.js`, `/home/artemis/Projects/words-til-vn/editor-ui.js`, `/home/artemis/Projects/words-til-vn/persistence.js`, `/home/artemis/Projects/words-til-vn/serve.py`, `/home/artemis/Projects/words-til-vn/test/studio.test.js`.

**Token ceiling / model tier:** 500,000–800,000 tokens · full model

**Tasks:**

- Analyze indentation.
- Preview proposed changes.
- Create a snapshot first.
- Apply only after confirmation.

**Verification:**

- Test nested menus, labels, and Python blocks.

## Phase 9 — Packaging and release

### Step 9.1 — Release configuration

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/README.md`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/PARITY-CHECKLIST.md`, `/home/artemis/Projects/say-count-native/resources/.gitkeep`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/README.md`, `/home/artemis/Projects/words-til-vn/icon.svg`, `/home/artemis/Projects/words-til-vn/say-count.desktop`, `/home/artemis/Projects/words-til-vn/start-say-count.sh`, `/home/artemis/Projects/words-til-vn/manifest.webmanifest`, `/home/artemis/Projects/words-til-vn/sw.js`, `/home/artemis/Projects/words-til-vn/package.json`, `/home/artemis/Projects/words-til-vn/test/pwa.test.js`.

**Token ceiling / model tier:** 300,000–500,000 tokens · mini model

**Tasks:**

- Add optimized release settings.
- Strip distributable symbols.
- Confirm resource paths.
- Add version information.

**Verification:**

- Build Release successfully.

### Step 9.2 — Static or portable wxWidgets build

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/README.md`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/PARITY-CHECKLIST.md`, `/home/artemis/Projects/say-count-native/resources/.gitkeep`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/README.md`, `/home/artemis/Projects/words-til-vn/icon.svg`, `/home/artemis/Projects/words-til-vn/say-count.desktop`, `/home/artemis/Projects/words-til-vn/start-say-count.sh`, `/home/artemis/Projects/words-til-vn/manifest.webmanifest`, `/home/artemis/Projects/words-til-vn/sw.js`, `/home/artemis/Projects/words-til-vn/package.json`, `/home/artemis/Projects/words-til-vn/test/pwa.test.js`.

**Token ceiling / model tier:** 500,000–800,000 tokens · mini model

**Tasks:**

- Add release-only wxWidgets build support.
- Keep system wxWidgets for development.
- Run without system wxWidgets installed.

**Verification:**

- Test in a clean environment.

### Step 9.3 — Linux desktop integration

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/README.md`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/PARITY-CHECKLIST.md`, `/home/artemis/Projects/say-count-native/resources/.gitkeep`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/README.md`, `/home/artemis/Projects/words-til-vn/icon.svg`, `/home/artemis/Projects/words-til-vn/say-count.desktop`, `/home/artemis/Projects/words-til-vn/start-say-count.sh`, `/home/artemis/Projects/words-til-vn/manifest.webmanifest`, `/home/artemis/Projects/words-til-vn/sw.js`, `/home/artemis/Projects/words-til-vn/package.json`, `/home/artemis/Projects/words-til-vn/test/pwa.test.js`.

**Token ceiling / model tier:** 300,000–500,000 tokens · mini model

**Tasks:**

- Add `.desktop`, icon, MIME association where appropriate, and install/uninstall targets.

**Verification:**

- Install locally and open an `.rpy` file from the desktop.

### Step 9.4 — AppImage

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/README.md`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/PARITY-CHECKLIST.md`, `/home/artemis/Projects/say-count-native/resources/.gitkeep`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/README.md`, `/home/artemis/Projects/words-til-vn/icon.svg`, `/home/artemis/Projects/words-til-vn/say-count.desktop`, `/home/artemis/Projects/words-til-vn/start-say-count.sh`, `/home/artemis/Projects/words-til-vn/manifest.webmanifest`, `/home/artemis/Projects/words-til-vn/sw.js`, `/home/artemis/Projects/words-til-vn/package.json`, `/home/artemis/Projects/words-til-vn/test/pwa.test.js`.

**Token ceiling / model tier:** 400,000–700,000 tokens · mini model

**Tasks:**

- Create an optional AppImage workflow.
- Bundle required libraries and resources.
- Document release steps.

**Verification:**

- Run on a clean Linux environment.

### Step 9.5 — Final parity audit

**Files to read:**

- Native: `/home/artemis/Projects/say-count-native/CMakeLists.txt`, `/home/artemis/Projects/say-count-native/README.md`, `/home/artemis/Projects/say-count-native/STATUS.md`, `/home/artemis/Projects/say-count-native/DECISIONS.md`, `/home/artemis/Projects/say-count-native/PARITY-CHECKLIST.md`, `/home/artemis/Projects/say-count-native/resources/.gitkeep`.
- Read-only reference: `/home/artemis/Projects/words-til-vn/README.md`, `/home/artemis/Projects/words-til-vn/icon.svg`, `/home/artemis/Projects/words-til-vn/say-count.desktop`, `/home/artemis/Projects/words-til-vn/start-say-count.sh`, `/home/artemis/Projects/words-til-vn/manifest.webmanifest`, `/home/artemis/Projects/words-til-vn/sw.js`, `/home/artemis/Projects/words-til-vn/package.json`, `/home/artemis/Projects/words-til-vn/test/pwa.test.js`.

**Token ceiling / model tier:** 600,000–1,000,000 tokens · full model

**Tasks:**

- Complete `PARITY-CHECKLIST.md` (created in Step 0.1).
- Mark every feature complete, changed, deferred, or not applicable.
- Run all native tests and real-project smoke tests.
- Document limitations.
- Split into 9.5A–9.5D if needed.

**Verification:**

- All native tests pass.
- JavaScript tests still pass.
- No JavaScript source files changed.
- Every parity item has a status.
