# Chat Program Conversion Contract

This contract targets `robo-barbie/chat_program` commit
`85ee08dd3e09ebf4492159f350b3a70de530b0ef`.

## Supported

| Construct | Parse | Emit | Reverse-conversion behavior |
|---|---:|---:|---|
| `ChatCharacter(name)` | Yes | Yes | Display name becomes the manuscript speaker |
| `icon=` | Yes | Yes | Reported as metadata loss |
| `name_color=` | Yes | Yes | Reported as metadata loss |
| `is_player=` | Yes | Yes | Manuscript: emitted as `[I am Name]`; Ren'Py say output: metadata loss |
| Chat say statement | Yes | Yes | Becomes `Name: text` or an ordinary Ren'Py say statement |
| `c=` channel | Yes | Yes | Manuscript: emitted as `[in channel]` on change; Ren'Py say output: loss |
| `ot=` alias | Yes | Yes | Manuscript: emitted as `[Name is typing]`; Ren'Py say output: loss |
| `ot=[...]` aliases | Yes | Yes | Manuscript: `[A and B are typing]`, order preserved; Ren'Py say output: loss |
| `fastmode=` finite non-negative number | Yes | Yes | Manuscript: `[fast]`/`[fast N]`/`[normal speed]` on change; Ren'Py say output: loss |
| `menu:` choice branches | Yes | Yes | Manuscript: `Choice:` block with `- option` lines; branch order and contents preserved |
| `auto_send=False` | Yes | Yes | Reported as metadata loss in regular dialogue |
| Ordinary Ren'Py narration | Yes | Yes | Preserved as narration |
| Unknown say arguments | Yes | Yes | Preserved verbatim with a warning |
| Other Ren'Py/Python lines | Passthrough | Passthrough | Never executed; preserved verbatim |

## Prose stage directions (forward conversion)

Natural-language directives on their own line convert to chat arguments:

| Prose | Emitted |
|---|---|
| `[in #room]`, `[to Name]`, `[channel X]`, `[room X]`, `[chat with X]`, `[switch to X]`, `[#room]` | `c="..."` on the next message; inherited afterwards |
| `[Name is typing]`, `[A and B are typing]`, `[A, B are typing]` | `ot=` on the next message; unknown names create a `ChatCharacter` |
| `[fast]` (0.1), `[very fast]` (0.05), `[quickly]` (0.1), `[fast N]` | `fastmode=N` on following messages until reset |
| `[normal speed]`, `[normal]`, `[normal pace]` | stops emitting `fastmode` |
| `[I am Name]`, `[player is Name]`, `[you are Name]` | `is_player=True` on that generated character |
| Speaker written `Me:` or `I:` | that character gets `is_player=True` automatically |
| `Choice:` / `Choices:` / `Menu:` / `Options:` header, then `- option` (or `*` / `>`) lines; indented lines under an option are its branch | `menu:` block with quoted options and branch contents |

Unrecognized `[...]` lines stay in the scene as narration and produce a
warning. Stage directions and further `Choice:` blocks work inside a choice
branch; state (channel, pacing) is shared with the surrounding scene. Blank
lines inside a branch are allowed as paragraph breaks.

## Deliberate limits

- Input is limited to 2 MiB by default.
- Nested chat menus beyond one choice level are preserved as passthrough and are
  not structurally converted yet.
- Multiline/triple-quoted chat messages are not structurally converted yet.
- Only Python/Ren'Py identifier aliases are accepted for speakers and `ot`.
- `fastmode` must consume the complete value and be finite and non-negative.
- Arbitrary Python, screen language, runtime state mutation, ADV/chat screen
  transitions, and customized framework forks are never executed or inferred.
- Unknown character-constructor arguments are ignored with no generated code;
  unknown say arguments are retained because they can safely remain inside the
  validated say-argument list.

## Escaping

- Generated Ren'Py strings escape backslashes and double quotes.
- A literal opening bracket is doubled as `[[` for Ren'Py interpolation safety.
- UTF-8 text is preserved.
- Line breaks cannot enter a single generated string field.
- Formatting uses four-space indentation and stable argument order: `c`, `ot`,
  `fastmode`, then preserved unknown arguments.

## Round-trip guarantees

For the supported subset, `chat -> ChatDocument -> chat -> ChatDocument`
preserves character metadata, speaker, message text, resolved channel, explicit
channel changes, other typers, timing, choice order, choice `auto_send`,
narration, and passthrough lines. Converting to regular dialogue intentionally
cannot preserve chat-only presentation metadata; every supported loss is listed
before replacement is enabled.
