# Architectural Decisions

| Topic | Status | Decision / question |
|---|---|---|
| Unicode word counting | Undecided | Before Step 2.2, choose ICU or a hand-rolled UTF-8 letter/number classifier matching `parser.js:121`: `/[\p{L}\p{N}]+(?:['’\-][\p{L}\p{N}]+)*/gu`. `std::regex` cannot express these Unicode property classes. |
| Settings location | Decided (Step 1.4) | Settings stored at `~/.local/share/say-count/settings.json` via `app::Settings` with atomic writes. wx 3.2 ignores `FileLayout_XDG` for `GetUserDataDir`, so the XDG path is resolved manually. `say_count_core` stays wx-free. |

