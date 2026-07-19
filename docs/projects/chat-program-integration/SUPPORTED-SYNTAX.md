# Chat Program Conversion Contract

This contract targets `robo-barbie/chat_program` commit
`85ee08dd3e09ebf4492159f350b3a70de530b0ef`.

## Supported

| Construct | Parse | Emit | Reverse-conversion behavior |
|---|---:|---:|---|
| `ChatCharacter(name)` | Yes | Yes | Display name becomes the manuscript speaker |
| `icon=` | Yes | Yes | Reported as metadata loss |
| `name_color=` | Yes | Yes | Reported as metadata loss |
| `is_player=` | Yes | Yes | Reported as metadata loss |
| Chat say statement | Yes | Yes | Becomes `Name: text` or an ordinary Ren'Py say statement |
| `c=` channel | Yes | Yes | Inherited until changed; explicit changes are reported as losses |
| `ot=` alias | Yes | Yes | Reported as metadata loss |
| `ot=[...]` aliases | Yes | Yes | Order is preserved and reported as metadata loss |
| `fastmode=` finite non-negative number | Yes | Yes | Reported as metadata loss |
| `menu:` choice branches | Yes | Yes | Branch order and contents are preserved |
| `auto_send=False` | Yes | Yes | Reported as metadata loss in regular dialogue |
| Ordinary Ren'Py narration | Yes | Yes | Preserved as narration |
| Unknown say arguments | Yes | Yes | Preserved verbatim with a warning |
| Other Ren'Py/Python lines | Passthrough | Passthrough | Never executed; preserved verbatim |

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
