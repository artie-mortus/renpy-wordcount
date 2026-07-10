# Parser Behavior Inventory

This document is the behavioral contract for the native parser. It records the
JavaScript oracle as of commit `ea19811`; it does not prescribe the C++ API.

## Text preprocessing and words

- Comments begin at `#` only outside single or double quotes. Backslash escapes
  the next character, including a quote or `#`.
- Quoted segments support both quote styles and escapes. A normal counted line
  joins every quoted segment with one space before cleaning and counting.
- Cleaning replaces literal `\n` with a space, unescapes `\"` and `\'`, removes
  `{...}` text tags, retains the contents of `[...]` interpolation tags, replaces
  `_`, `*`, `~`, and backticks with spaces, then trims.
- A word is `Letter-or-Number+`, optionally followed by any number of groups
  consisting of one apostrophe (`'` or `’`) or hyphen (`-`) and another
  `Letter-or-Number+`. Thus `don't` and `well-known` are each one word, while
  `don't stop` and `well-known fact` are two. Letters and numbers are Unicode
  general categories `L*` and `N*`, not ASCII only.
- Empty cleaned text and text containing no words are not counted.

## Normal line classification

The parser strips comments and surrounding whitespace first. Blank lines,
comments, `$` Python lines, and lines without a complete quoted segment are not
counted. For the text before the first quote, the first ASCII identifier is the
candidate speaker alias.

| Form | Result |
|---|---|
| `e "Hello"` | Dialogue; alias `e`, resolved display name if defined |
| `e happy "Hello"` | Dialogue; the first identifier remains the alias |
| `"Narration"` | Narrator dialogue |
| `extend "More"` | Dialogue attributed to the most recent non-menu, non-extend speaker when available |
| `"Choice":` or `"Choice" if flag:` | Menu choice only when the option is enabled; speaker is `menu choice` |
| `define e = Character("Eileen")` | Ignored because pre-quote text contains `=` |
| Function/call-like pre-quote text | Ignored because pre-quote text contains `(` |
| Ren'Py statement starter plus quoted text | Ignored when its first word is in the application's `ignoredStarters` set |

Statements such as `scene`, `show`, `hide`, `jump`, `call`, `return`, `label`,
`define`, `default`, `image`, `screen`, `transform`, `style`, `python`, `init`,
and other configured starters are code, not narration. An otherwise speaker-like
identifier absent from the merged character definitions is still counted under
that alias and produces a `speaker` warning.

## Character definitions and colors

- Accepted definitions have an optional `define` or `default`, an ASCII
  identifier, `=`, optional `renpy.`, then `Character(`. Bare
  `e = Character(...)` is accepted.
- The first quoted argument after `Character(` is cleaned and becomes the
  display name. Missing, empty, or case-insensitive `None` names do not create a
  name mapping.
- `color="..."` or `color='...'` accepts a `#` followed by 3 through 8 hex
  digits. The color is keyed by both alias and nonempty display name.
- Definitions are merged project-wide in file order, so dialogue can resolve a
  definition from another file. Changed definitions invalidate every cached
  file analysis; changed contents invalidate that file; closed files are evicted.

## Script state, labels, and blocks

- Dialogue before any label belongs to `No label`. A syntactically complete
  `label name:` changes the scene and creates a zero-valued scene entry.
  Parameterized labels are accepted.
- Local labels such as `.ending` qualify against the latest global label
  (`start.ending`) in analysis, lint, and routes.
- Blocks opened by `init:`, numbered `init`, `python:` (including `init python`),
  `screen`, `transform`, `style`, or `image` are ignored through all more-deeply
  indented nonblank lines. Parsing resumes on dedent. Consequently quoted Python
  strings and screen text are not dialogue.
- Line indentation is measured as raw spaces/tabs for parser block skipping;
  lint and branch analysis normalize a tab to four spaces.

## Triple-quoted monologues

- `"""` starts a monologue unless the stripped line starts with `$`. Text may
  end on the same line or a later line.
- Text before the opener must be empty or a sequence of identifiers whose first
  word is not an ignored starter. The first word is the alias; empty means
  narrator.
- Blank lines split a monologue into counted paragraphs. Nonblank physical lines
  in one paragraph are joined with spaces. Each paragraph is one dialogue line,
  located at its first physical line.
- An unterminated monologue is still flushed and counted, and emits an unclosed
  triple-quote warning at its opening line. Ordinary unmatched quotes emit a
  quote warning and may leave the line uncounted.
- A following `extend` inherits the monologue speaker.

## Results and warnings

Each counted item records 1-based line number, resolved speaker, cleaned text,
word count, raw source, alias, file, qualified scene, and flags for unknown
speaker, extend, and menu choice. Analysis aggregates word/line totals by
speaker and scene. `dialogueLines` is the counted-item count; `scriptLines` is
zero for an empty string and otherwise the newline-split length; average words
is zero with no dialogue.

Warnings include unknown speaker, ordinary/unclosed triple quote, and lines over
the configurable long-line threshold (35 words by default). Project lint adds:

- duplicate labels and missing static `jump`/`call` targets;
- malformed labels, targetless `jump`/`call`, and missing colons on
  `menu`/`if`/`elif`/`else`/`while`/`for`;
- empty label, Python, control-flow, and menu-choice blocks.

Lint ignores apparent Ren'Py statements inside Python blocks. `call screen` and
`jump/call expression` are not label references. Support files `gui.rpy`,
`options.rpy`, `screens.rpy`, and `say_count_runtime.rpy` participate in label
resolution but have all writer-facing warnings filtered out.

## Route and highlighting dependencies

Route parsing uses the same comment stripping, local-label qualification, and
text cleaning. It records static jump/call edges, fall-through unless the last
code is `jump` or `return`, inline menu/conditional branch counts, unreachable
labels, loops, and shortest/longest word totals. Branch dialogue is assigned by
file and source line. These are downstream consumers of parser semantics, not
additional counted-line forms.

Highlighting uses the same comment boundary and starter set. It marks `$` lines
as code, statement starters and `extend` as keywords, known colored aliases as
speakers, quoted spans as strings, and the suffix after the parser's code part as
a comment. Output escaping and highlight cache behavior are UI concerns rather
than parser behavior.

## JavaScript fixture coverage map

Every fixture in `test/parser.test.js` is represented below.

| Fixture | Contract section(s) |
|---|---|
| dialogue and non-dialogue classification | Normal line classification |
| menu choices and unknown aliases | Normal line classification |
| word count and Ren'Py markup | Text preprocessing and words |
| character names and colors | Character definitions and colors |
| labels, extend, and warnings | Script state; Results and warnings |
| ignored Ren'Py code blocks | Script state, labels, and blocks |
| monologue dialogue | Triple-quoted monologues |
| cross-file definitions and cached results | Character definitions and colors |
| cache invalidation | Character definitions and colors |
| project menu-choice totals | Normal line classification |
| duplicate labels and missing targets | Results and warnings |
| local labels and `call screen` | Script state; Results and warnings |
| local labels in analysis and routes | Script state; Route dependencies |
| malformed statements and empty blocks | Results and warnings |
| jump-like Python text | Script state; Results and warnings |
| warning-filtered support files | Results and warnings |
| route menu and conditional branches | Route dependencies |
| inline branch word totals | Route dependencies |
| symbol collection and rename preview | Out of parser scope; later project tooling |
| symbol cache refresh and closed files | Out of parser scope; later project tooling |

The five `test/highlight.test.js` fixtures cover safe HTML escaping/template
composition, escaped dialogue markup, keyword/code/comment/speaker styling, line
order, and color-map cache invalidation. Only their shared lexical boundaries
are part of the parser contract above.
