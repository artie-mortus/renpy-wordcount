# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Where am I? (session resume)

To find the current state of the project, read in this order — nothing else:

1. `CHECKLIST.md` — which steps are done (`[x]`), in progress (`[~]`), or pending. The first unchecked step is the next one.
2. `STATUS.md` — what the last session did, test results, known issues, next step.
3. `git log --oneline -5` — if the newest commit is `wip:`-prefixed, the last session was cut off mid-step: resume that step from the wip commit, finish it, squash.
4. `PLAN.md` — read only the card for the step being worked (tasks, verification, and its "Files to read" list).

## Project

Native C++17 / wxWidgets rewrite of Say Count (Ren'Py script word counter). The JS original at `/home/artemis/Projects/words-til-vn` is a **read-only behavioral reference — never edit it**. The plan documents live in `/home/artemis/Projects/say-count-native-plan` (also read-only; `PLAN.md` here is the working copy).

## Workflow

- One step per session, dispatched to Codex CLI (`codex exec -m gpt-5.6-sol -c model_reasoning_effort=low --sandbox workspace-write`), then reviewed by Claude. See the plan repo's `CLAUDE.md` for the full orchestration rules.
- Codex sandbox limits (observed): no network (Catch2 FetchContent fails — run configure outside sandbox) and `.git` mounted read-only (Claude commits on Codex's behalf).
- Model name is `gpt-5.6-sol` — `gpt-6-sol` does not exist on this account and 400s. Account models: `gpt-5.5`, `gpt-5.6-sol` (config default already `gpt-5.6-sol` + low effort).
- `wip:` commits during a step; squash to one commit at step end; then update `STATUS.md` (≤40 lines) and tick the step in `CHECKLIST.md` with the commit hash.
- Record architectural choices in `DECISIONS.md`; tick features in `PARITY-CHECKLIST.md` as they land.

## Reasoning-level gate

Before starting a step, inspect its entry in `CHECKLIST.md`. If it contains **HIGH REASONING REQUIRED**, do not implement it with low reasoning. Stop and warn the user to restart or rerun the step with high reasoning. Do not treat a request such as "do next step" as permission to bypass this gate.

Steps requiring high reasoning: 4.2, 4.3, 4.6, 5.3, 5.6, 5.7, 6.3, 6.4, 7.1, 7.2, 7.4, 7.5, and 9.5.

## Build and test

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/say-count
```

Targets: `say_count_core` (static lib, `src/core`, **zero wx dependency**), `say-count` (GUI, `src/app` + `src/ui`), `say-count-tests` (Catch2 via FetchContent).

## Hard constraints

- Project export JSON schema and `.rpy` handling (tab→4-space conversion on load) must stay byte-compatible with the JS app.
- From Step 2.x on, parity harness (JS dump vs `say-count --dump-json`) must diff empty — it gates every later milestone.
- Unicode word-counting decision (ICU vs hand-rolled classifier) must be made in `DECISIONS.md` before Step 2.2.
