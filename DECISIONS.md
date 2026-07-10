# Architectural Decisions

| Topic | Status | Decision / question |
|---|---|---|
| Unicode word counting | Decided (Steps 2.1/2.3) | Use a hand-written UTF-8 scanner backed by a generated, vendored table of Unicode general-category ranges for letters (`L*`) and numbers (`N*`). The checked-in table is Unicode 17.0, generated with Node 24.16.0 property escapes. Accept `'`, `’`, or `-` internally only when followed by another letter/number run, exactly matching `parser.js:121`: `/[\p{L}\p{N}]+(?:['’\-][\p{L}\p{N}]+)*/gu`. Invalid UTF-8 bytes are separators. This keeps `say_count_core` dependency-free and avoids ICU's binary/runtime cost; tests cover ASCII, accented, non-Latin, numeric, apostrophe, hyphen, and non-word emoji cases. |
| Settings location | Decided (Step 1.4) | Settings stored at `~/.local/share/say-count/settings.json` via `app::Settings` with atomic writes. wx 3.2 ignores `FileLayout_XDG` for `GetUserDataDir`, so the XDG path is resolved manually. `say_count_core` stays wx-free. |
