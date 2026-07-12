# Status

- Completed: Steps 0.1 through 4.8.
- Editing shortcuts now cover go-to-line, comment toggle, line move/duplicate, previous/next label, and tab switching.
- Ctrl+K opens a shortcut cheat sheet; menu accelerators expose go-to-line and comment toggle alongside their keyboard handling.
- Quotes, parentheses, and brackets auto-close or wrap selected text; existing closers type over and escaped quotes insert normally.
- Selection-aware transformations live in wx-free core code, preserve indentation/selection, and apply to Scintilla as one undo action.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 67/67, including comment, move, duplicate, pair/type-over, escaped-quote, label-navigation, and parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 4, Step 4.9 — focus mode.
