# Status

- Completed: Steps 0.1 through 4.2.
- Scintilla autocomplete now offers speaker aliases at otherwise empty line starts and label names after bare `jump`/`call` targets.
- `label`, `menu`, and `define` snippets preserve current indentation, expand to complete forms, and select their editable placeholder.
- Suggestions are case-insensitive, prefix-filtered, capped at eight, keyboard-driven by Scintilla, and suppressed in strings, assignments, and unrelated contexts.
- Core remains wx-free; dump JSON schema and `.rpy` parsing behavior are unchanged.
- Verified (2026-07-12): `cmake --build build -j` succeeded; `ctest --test-dir build --output-on-failure` passed 50/50, including positive/negative completion contexts, insertion text, snippet indentation/selection ranges, and all parser parity tests.
- Known issues: pre-existing Step 1.3-era re-entrant quit/discard-modal `ConfirmCloseAll` crash remains; not reproducible in normal flows.
- Next step: Phase 4, Step 4.3 — asset-aware autocomplete (HIGH REASONING REQUIRED).
