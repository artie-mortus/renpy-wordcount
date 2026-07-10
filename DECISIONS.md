# Architectural Decisions

| Topic | Status | Decision / question |
|---|---|---|
| Unicode word counting | Decided (Step 2.1) | Use a hand-written UTF-8 scanner backed by a generated, vendored table of Unicode general-category ranges for letters (`L*`) and numbers (`N*`). Accept `'`, `’`, or `-` internally only when followed by another letter/number run, exactly matching `parser.js:121`: `/[\p{L}\p{N}]+(?:['’\-][\p{L}\p{N}]+)*/gu`. Pin and document the Unicode data version when the table is added with word counting in Step 2.3 (Step 2.2 tokenizes lines but does not count text). Invalid UTF-8 bytes are separators. This keeps `say_count_core` dependency-free and avoids ICU's binary/runtime cost; parity fixtures will cover ASCII, accented, non-Latin, numeric, apostrophe, and hyphen cases. |
| Settings location | Decided (Step 1.4) | Settings stored at `~/.local/share/say-count/settings.json` via `app::Settings` with atomic writes. wx 3.2 ignores `FileLayout_XDG` for `GetUserDataDir`, so the XDG path is resolved manually. `say_count_core` stays wx-free. |
