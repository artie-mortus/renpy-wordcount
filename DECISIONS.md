# Architectural Decisions

| Topic | Status | Decision / question |
|---|---|---|
| Unicode word counting | Undecided | Before Step 2.2, choose ICU or a hand-rolled UTF-8 letter/number classifier matching `parser.js:121`: `/[\p{L}\p{N}]+(?:['’\-][\p{L}\p{N}]+)*/gu`. `std::regex` cannot express these Unicode property classes. |

