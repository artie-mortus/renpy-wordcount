# Progress Checklist

Session-resume tracker. **Read this file plus STATUS.md first in every session.** Mark a step `[x]` only after its verification passed and the squashed commit exists (note the commit hash). In-progress step marked `[~]` with newest `wip:` commit noted.

## Phase 0 — Repository setup

- [x] Step 0.1 — Create the native repository · mini · ≤300,000 tok — done 2026-07-10, commit `fdb2251`
- [x] Step 0.2 — Establish the build system · mini · ≤400,000 tok — done 2026-07-10, commit `fdb2251`

## Phase 1 — Application skeleton

- [x] Step 1.1 — Main window and application lifecycle · mini · ≤450,000 tok — done 2026-07-10, commit `044a962`
- [x] Step 1.2 — Notebook and editor tabs · mini · ≤500,000 tok — done 2026-07-10, commit `f0f55a0`
- [x] Step 1.3 — File opening and saving · mini · ≤600,000 tok — done 2026-07-10, commit `0df78a0`
- [x] Step 1.4 — Basic editor configuration · mini · ≤450,000 tok — done 2026-07-10, commit `d89b86d`

## Phase 2 — Tokenizer and parser

- [x] Step 2.1 — Inventory parser behavior · full · ≤500,000 tok — done 2026-07-10, commit recorded by the Step 2.1 completion commit
- [x] Step 2.2 — Line tokenizer · full · ≤700,000 tok — done 2026-07-10, commit recorded by the Step 2.2 completion commit
- [x] Step 2.3 — Core dialogue counting · full · ≤750,000 tok — done 2026-07-10, commit recorded by the Step 2.3 completion commit
- [x] Step 2.4 — Character definitions and display names · full · ≤650,000 tok — done 2026-07-10, commit recorded by the Step 2.4 completion commit
- [x] Step 2.5 — Menus and conditioned choices · full · ≤700,000 tok — done 2026-07-10, commit recorded by the Step 2.5 completion commit
- [x] Step 2.6 — Complete parser fixture translation · full · ≤1,000,000 tok — done 2026-07-11: 2.6A `cef918c`, 2.6B `8725ec7`, 2.6C `a4fa8a8` (route/symbol fixtures deferred to 7.1/7.2/5.7 per DECISIONS.md)
- [x] Step 2.7 — Syntax highlighting · mini · ≤750,000 tok — done 2026-07-11, commit `ddc9ea1`

## Phase 3 — Core analysis and statistics

- [x] Step 3.1 — Analysis data model · full · ≤650,000 tok — done 2026-07-11, commit `aa33d11`
- [x] Step 3.2 — Live document analysis · mini · ≤550,000 tok — done 2026-07-11, commit `18fc1a4`
- [x] Step 3.3 — Speaker statistics panel · mini · ≤700,000 tok — done 2026-07-11, commit `414ab59`
- [x] Step 3.4 — Scene and label statistics · mini · ≤650,000 tok — done 2026-07-11, commit `a1c962c`
- [x] Step 3.5 — Counted-lines browser · mini · ≤600,000 tok — done 2026-07-12, commit `90bb7e2`
- [x] Step 3.6 — Statistics export · mini · ≤650,000 tok — done 2026-07-12, commit `55b6f88`
- [x] Step 3.7 — Version comparison · mini · ≤700,000 tok — done 2026-07-12, commit `b7e6cfd`

## Phase 4 — Editor productivity features

- [x] Step 4.1 — Outline panel · mini · ≤750,000 tok — done 2026-07-12, commit `714b429`
- [x] Step 4.2 — Basic autocomplete · full · ≤750,000 tok — done 2026-07-12, commit `c113a1b`
- [x] Step 4.3 — Asset-aware autocomplete · full · ≤850,000 tok — done 2026-07-12, commit `dd404ba`
- [x] Step 4.4 — Find in current file · mini · ≤700,000 tok — done 2026-07-12, commit `73e3d28`
- [x] Step 4.5 — Find across files · mini · ≤800,000 tok — done 2026-07-12, commit `8a0f748`
- [x] Step 4.6 — Diagnostics engine · full · ≤900,000 tok — done 2026-07-12, commit `5ebb4cc`
- [x] Step 4.7 — Diagnostics UI · mini · ≤650,000 tok — done 2026-07-12, commit `c808b3a`
- [x] Step 4.8 — Editing commands and shortcuts · mini · ≤750,000 tok — done 2026-07-12, commit `4cb1cda`
- [x] Step 4.9 — Focus mode · mini · ≤400,000 tok — done 2026-07-12, commit `8e2bf5f`

## Phase 5 — Project management

- [x] Step 5.1 — Project folder model · mini · ≤700,000 tok — done 2026-07-12, commit `795a072`
- [x] Step 5.2 — External file watching · mini · ≤650,000 tok — done 2026-07-12, commit `3e12e18`
- [x] Step 5.3 — Conflict review · full · ≤800,000 tok — done 2026-07-12, commit `feebb74`
- [x] Step 5.4 — Snapshot storage · mini · ≤700,000 tok — done 2026-07-12, commit `01d1d88`
- [x] Step 5.5 — Snapshot restore and comparison · mini · ≤700,000 tok — done 2026-07-12, commit `a262e8c`
- [x] Step 5.6 — Import and export compatibility · full · ≤900,000 tok — **HIGH REASONING REQUIRED** — done 2026-07-12, commit `052044c`
- [x] Step 5.7 — Safe symbol renaming · full · ≤900,000 tok — **HIGH REASONING REQUIRED** — done 2026-07-12, commit `f202f7f`

## Phase 6 — Ren'Py integration

- [ ] Step 6.1 — Ren'Py SDK detection and settings · mini · ≤550,000 tok
- [ ] Step 6.2 — Launch and stop Ren'Py · mini · ≤800,000 tok
- [ ] Step 6.3 — Warp and Interactive Director · full · ≤750,000 tok — **HIGH REASONING REQUIRED**
- [ ] Step 6.4 — Runtime state presets · full · ≤850,000 tok — **HIGH REASONING REQUIRED**
- [ ] Step 6.5 — Lint integration · mini · ≤650,000 tok
- [ ] Step 6.6 — Translation and dialogue commands · mini · ≤650,000 tok
- [ ] Step 6.7 — Asset browser · mini · ≤850,000 tok
- [ ] Step 6.8 — Coverage collection · full · ≤900,000 tok

## Phase 7 — Routes and flow map

- [ ] Step 7.1 — Route graph data model · full · ≤850,000 tok — **HIGH REASONING REQUIRED**
- [ ] Step 7.2 — Route walker · full · ≤950,000 tok — **HIGH REASONING REQUIRED**
- [ ] Step 7.3 — Route details UI · mini · ≤650,000 tok
- [ ] Step 7.4 — Basic flow map rendering · full · ≤850,000 tok — **HIGH REASONING REQUIRED**
- [ ] Step 7.5 — Flow map layout and interaction · full · ≤1,000,000 tok — **HIGH REASONING REQUIRED**

## Phase 8 — Long-tail production tools (deferred)

- [ ] Step 8.1 — Prose analysis engine · full · ≤800,000 tok
- [ ] Step 8.2 — Prose analysis UI · mini · ≤650,000 tok
- [ ] Step 8.3 — Voice-production tracker model · mini · ≤850,000 tok
- [ ] Step 8.4 — Voice-production exports · mini · ≤700,000 tok
- [ ] Step 8.5 — Translation dashboard · mini · ≤900,000 tok
- [ ] Step 8.6 — Continuity notes · mini · ≤650,000 tok
- [ ] Step 8.7 — Accessibility audit · mini · ≤750,000 tok
- [ ] Step 8.8 — Fix indents · full · ≤800,000 tok

## Phase 9 — Packaging and release

- [ ] Step 9.1 — Release configuration · mini · ≤500,000 tok
- [ ] Step 9.2 — Static or portable wxWidgets build · mini · ≤800,000 tok
- [ ] Step 9.3 — Linux desktop integration · mini · ≤500,000 tok
- [ ] Step 9.4 — AppImage · mini · ≤700,000 tok
- [ ] Step 9.5 — Final parity audit · full · ≤1,000,000 tok — **HIGH REASONING REQUIRED**

## Milestone gates

- [x] M1 daily-driver editor usable (end of Phase 1 + 2.7) — reached 2026-07-11
- [x] M2 parity harness green — correctness anchor (end of Phase 2) — reached 2026-07-11: ten JS/native parity tests, all diffs empty
- [ ] Core parity (end of Phase 7)
- [ ] Release (end of Phase 9; Phase 8 deferred, optional)
