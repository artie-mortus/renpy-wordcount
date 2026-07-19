# Chat Program Integration Plan

> **Completed 2026-07-18:** All phases and acceptance criteria were implemented
> and verified with the native core/UI suites plus official Ren'Py lint, compile,
> channel-switching, and choice runtime smoke coverage. The implementation
> checklist was deleted after completion as requested.

## Goal

Add a first-class **Chat Format** tool to Say Count Native that can:

1. turn selected prose or a whole manuscript into `chat_program`-compatible Ren'Py;
2. turn supported `chat_program` dialogue back into ordinary, editable dialogue; and
3. preview every conversion before replacing editor text.

The integration targets [`robo-barbie/chat_program`](https://github.com/robo-barbie/chat_program) at commit `85ee08dd3e09ebf4492159f350b3a70de530b0ef` (2026-05-31). The upstream project is released under the Unlicense. Record that revision and license in the repository even if the first version only emits its syntax and does not bundle its runtime.

## Product decisions

- Treat this as a source-conversion tool, not as a second editor mode. Put **Convert to Chat Format…** and **Convert Chat to Dialogue…** beside the existing prose conversion commands and expose both in the command palette.
- Reuse the existing selection-or-active-document behavior, preview dialog, undo transaction, character discovery, and offline prose interpretation. Do not mutate files until the user accepts a preview.
- Define “regular dialogue” as Say Count's existing manuscript form (`Display Name: text`) for the reverse operation. From there, the existing converter can produce ordinary Ren'Py say statements. Also offer ordinary Ren'Py as a preview output option when that removes an unnecessary second step.
- Keep `chat_program` installation separate from text conversion. Detect whether the connected project contains the compatible runtime and offer an explicit **Install/Update Chat Runtime** project action; never silently overwrite `chat_program.rpy`, `screens.rpy`, or user assets.
- Preserve chat-only data in a typed intermediate representation. A reverse conversion to plain dialogue is inherently lossy for channels, concurrent typers, timing, player ownership, and choice `auto_send`; show those losses in the preview instead of implying byte-for-byte reversibility.
- Make the deterministic parser/formatter the default. The optional network-blocked local model may classify ambiguous prose into narration/dialogue first, but must never generate raw Ren'Py or bypass validation.

## Supported syntax

Version 1 should parse and emit the upstream public API:

```renpy
default r = ChatCharacter("Robobarbie", icon="images/Robo.png", name_color="#E4C443")
default mc = ChatCharacter("[player_fname]", is_player=True)

label chat_scene:
    r "Hello!" (c="#general", ot=[mc], fastmode=0.1)
    r "Still in general."
    menu:
        "Reply":
            r "Great."
        "Ignore" (auto_send=False):
            pass
```

Support `ChatCharacter` definitions, say statements, inherited channels, `c`, `ot`, `fastmode`, `is_player`, menus, and `auto_send`. Preserve unknown say/menu arguments as warnings and untouched source spans. Do not attempt to translate arbitrary Python, screen language, runtime state mutations, ADV/chat screen transitions, or customized forks in version 1.

## Internal design

Introduce a format-neutral chat model in `src/core/chat_document.{h,cpp}`:

```text
ChatDocument
  characters: alias, display name, icon, name color, is-player, source location
  events:
    Message: speaker, text, channel, other typers, fast mode, source location
    Choice: caption, auto-send, nested events, source location
    Narration: text, source location
    Passthrough: original text, warning, source location
```

Keep channel inheritance explicit while parsing: each message stores its resolved channel plus whether the source specified it. This lets the formatter produce concise output without losing meaning. Retain source locations and original fragments so diagnostics and preview rows can identify exactly what changed.

Add three focused core modules:

- `chat_program_parser.{h,cpp}`: recognize `ChatCharacter` declarations and the supported Ren'Py statements using the existing tokenizer where possible; never execute Python.
- `chat_program_formatter.{h,cpp}`: serialize `ChatDocument` to escaped, indented `chat_program` syntax with stable argument ordering (`c`, `ot`, `fastmode`) and character declarations.
- `chat_dialogue_adapter.{h,cpp}`: map manuscript conversion results to `ChatDocument`, and map chat events back to manuscript or ordinary Ren'Py dialogue while producing a structured loss report.

Do not add chat-specific branches throughout `core/parser.cpp`. Extend shared tokenization only where a generally useful Ren'Py say-argument representation is needed, then keep framework semantics inside the chat modules.

## Conversion behavior

### Prose to chat

1. Classify the selection/document with `review_manuscript_lines()` and protect recognized Ren'Py blocks.
2. Convert prose to narration/dialogue using the existing manuscript converter or its offline-model pre-pass.
3. Resolve speakers against project characters. In the conversion dialog, map each speaker to an existing `ChatCharacter`, create a new one, or mark it as narration/passthrough.
4. Ask for a default channel and optional per-message channel overrides. Default all messages to the chosen channel; only emit `c=` when the channel changes.
5. Format validated chat statements and show definitions, messages, preserved lines, warnings, and unresolved speakers in the preview.

For prose narration, default to **preserve as ordinary Ren'Py narration**. Optionally allow a user-selected “system/narrator” chat character, because upstream `chat_program` has no native narration-message type.

### Chat to dialogue

1. Parse only supported `chat_program` definitions and statements in the selection/document.
2. Render messages as `Display Name: text` (or ordinary Ren'Py say statements when selected).
3. Convert menus into ordinary manuscript/Ren'Py menus while preserving branch structure. Mark `auto_send=False` and any player auto-send semantics as losses unless an explicit equivalent can be emitted.
4. Omit runtime setup and `ChatCharacter` declarations only after listing them as intentionally removed metadata in the preview.
5. Preserve unrecognized code verbatim by default. The user must explicitly opt into dropping passthrough blocks.

### Round-trip contract

- Guarantee semantic round trips for supported messages: speaker, text, resolved channel, other typers, fast mode, and choice structure survive `chat_program -> ChatDocument -> chat_program`.
- Guarantee dialogue-content round trips for `prose/dialogue -> ChatDocument -> regular dialogue`: speaker, text, narration, and branch order survive.
- Do not guarantee that plain dialogue converted back to chat recreates metadata that plain dialogue cannot represent. Surface a summary such as “3 channel changes and 2 timing hints will be removed.”

## User interface

Add a `ChatConversionDialog` modeled on `ManuscriptDialog` with:

- direction: **Prose/dialogue → Chat** or **Chat → Dialogue**;
- scope: current selection or active document;
- output: chat Ren'Py, manuscript dialogue, or ordinary Ren'Py;
- default channel and character mapping table for forward conversion;
- side-by-side source/result preview;
- counts for messages, narration, choices, preserved lines, and warnings;
- a loss/ambiguity panel that must be acknowledged when conversion drops metadata; and
- **Replace selection/document** and **Copy result** actions.

Add **Install/Update Chat Runtime…** separately. Its preview should list every proposed file and classify it as new, identical, modified by the user, or upstream-changed. Initially install a pinned, minimal compatibility bundle into a new project subdirectory (for example `game/vendor/chat_program/`) and generate a small project-owned configuration file rather than editing the vendored runtime. If upstream still requires choice-screen edits, generate a patch/instructions and require confirmation instead of replacing `screens.rpy`.

## Runtime compatibility and provenance

- Add a small manifest such as `resources/chat_program/manifest.json` containing upstream repository, pinned commit, license, expected file hashes, and the supported API version.
- Prefer a minimal reviewed snapshot of required runtime code over cloning at application runtime. This keeps installs reproducible and offline, and avoids pulling demo scripts, project GUI files, or unrelated assets.
- Include the upstream `LICENSE.txt` beside installed runtime files and a `NOTICE` identifying local compatibility changes.
- Detect forks by hashing the installed runtime. Never update a modified copy automatically; offer side-by-side export or installation under a new versioned directory.
- Validate generated scripts with Say Count's parser and, when configured, official Ren'Py lint before applying them.

## Implementation phases

### Phase 1 — fixture and format contract

- Add representative upstream fixtures under `tests/fixtures/chat_program/`, trimmed to the public syntax and annotated with source revision/license.
- Write the `ChatDocument` types and an explicit support matrix for each upstream argument and statement.
- Add escaping rules for quotes, backslashes, Ren'Py interpolation brackets, Unicode, empty messages, and multiline text.

Exit criterion: fixtures describe every supported construct and every intentional loss.

### Phase 2 — parser and formatter

- Implement definition/message/menu parsing without executing embedded Python.
- Implement deterministic serialization and structured diagnostics.
- Add semantic equality helpers so tests compare models rather than formatting alone.

Exit criterion: `chat -> model -> chat -> model` is equal for every supported fixture.

### Phase 3 — manuscript adapters

- Refactor the smallest reusable portion of manuscript conversion so dialogue events can feed `ChatDocument` without parsing generated strings.
- Implement reverse rendering to manuscript and ordinary Ren'Py.
- Add loss reporting and passthrough preservation.
- Extend the offline response protocol only if needed with validated chat metadata records; continue to reject all other model output.

Exit criterion: prose fixtures convert to chat and back with dialogue content and order intact, with expected losses reported.

### Phase 4 — desktop workflow

- Add the conversion dialog, menu items, command-palette entries, keyboard-accessible controls, and a single undoable apply action.
- Reuse project character discovery to prefill `ChatCharacter` mappings.
- Keep long conversion/model work cancellable and off the UI thread.

Exit criterion: selection and whole-document conversions preview correctly, cancellation changes nothing, and acceptance creates one undo step.

### Phase 5 — runtime installer

- Package the pinned minimal runtime and provenance manifest.
- Implement new-file installation, identical-file detection, modified-file protection, and explicit upgrades.
- Add project configuration generation and non-destructive guidance for screen integration.

Exit criterion: a clean sample Ren'Py project can install the runtime and run generated chat; a customized project cannot be overwritten without an explicit file-level decision.

### Phase 6 — validation and release

- Run official Ren'Py lint/runtime smoke tests against a generated sample project.
- Add documentation for supported syntax, round-trip limits, vendored license, runtime installation, upgrades, and removal.
- Add telemetry-free status messages and actionable errors for unsupported syntax.

Exit criterion: core, UI, accessibility, snapshot, Ren'Py lint, and runtime tests pass; the verified desktop build is installed to the launcher according to `AGENTS.md`.

## Test plan

- Unit tests: definitions, messages, argument ordering, channel inheritance, `ot` scalar/list forms, numeric `fastmode`, player characters, menus, `auto_send`, escaping, Unicode, malformed syntax, unknown arguments, and nesting limits.
- Property tests: formatter output reparses to the same `ChatDocument`; random message text never escapes its Ren'Py string literal.
- Adapter tests: all currently supported manuscript forms; existing alias reuse; narration policy; menu branch order; chat metadata loss reports.
- Safety tests: Python is never executed; oversized/deep inputs fail predictably; unsupported blocks remain byte-identical; model output cannot inject Ren'Py.
- UI tests: selection/document scope, preview rejection, cancellation, copy, one-step undo, focus order, keyboard activation, and warning acknowledgement.
- Installer tests: clean install, repeat install, pinned upgrade, missing files, hash mismatch, locally modified files, partial install recovery, license/notice presence, and atomic writes.
- Integration tests: generated project passes official Ren'Py lint and a headless/runtime smoke path exercises channel switching and a choice.

## Acceptance criteria

- A writer can select prose, choose a channel and character mappings, preview valid `chat_program` Ren'Py, and apply it in one undoable edit.
- A writer can select supported chat statements and recover readable manuscript dialogue or ordinary Ren'Py without losing speaker, text, narration, or branch order.
- Every removed chat-only field is counted and explained before apply.
- Supported chat syntax round-trips semantically through the internal model.
- Unsupported or customized code is preserved and diagnosed, never executed or silently discarded.
- Runtime installation is pinned, licensed, reproducible, offline-capable, and non-destructive to existing project files.
- Existing prose conversion behavior remains unchanged when Chat Format is not selected.

## Recommended first implementation slice

Ship the deterministic core before the runtime installer: `ChatDocument`, parser/formatter, manuscript adapters, preview dialog, and tests for messages plus channel inheritance. This delivers the everyday writing loop quickly while keeping runtime vendoring and choice-screen compatibility as an independently reviewable follow-up.
