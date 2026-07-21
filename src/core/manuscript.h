#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct ManuscriptOptions {
    std::string label = "start";
    bool include_character_definitions = true;
    std::size_t indentation = 4;
    std::map<std::string, std::string> existing_characters;
};

struct ManuscriptCharacter {
    std::string name;
    std::string alias;
    bool uses_image_attributes = false;
};

struct ManuscriptConversion {
    std::string script;
    std::vector<ManuscriptCharacter> characters;
    std::vector<std::string> reused_aliases;
    std::size_t dialogue_lines = 0;
    std::size_t narration_lines = 0;
};

enum class ManuscriptLineKind { Blank, Prose, Renpy, Uncertain };

struct ManuscriptLineReview {
    std::size_t line_number = 0;
    ManuscriptLineKind kind = ManuscriptLineKind::Blank;
    std::string text;
};

std::vector<ManuscriptLineReview> review_manuscript_lines(std::string_view text);

// Returns true when the text contains recognizable Ren'Py structure and should
// not be passed through the prose converter. This is intentionally conservative:
// ambiguous natural-language forms remain convertible.
bool manuscript_contains_renpy_code(std::string_view text);

// Converts prose within a mixed manuscript/Ren'Py document while preserving
// recognized code lines. Whole-document conversion can add missing character
// definitions; selection conversion should pass false.
ManuscriptConversion convert_mixed_manuscript_to_renpy(
    std::string_view text, bool include_character_definitions,
    const std::map<std::string, std::string>& existing_characters = {},
    bool include_uncertain_lines = true);

// Converts manuscript-style dialogue into paste-ready Ren'Py. Supported forms:
//   Eileen: We should go.
//   Eileen said, "We should go."
//   "We should go," Eileen said.
//   We should go, said Eileen.       (quotation marks optional for a trailing tag)
//   Eileen — We should go.
//   Eileen             (speaker line followed by a quoted line)
//   EILEEN             (screenplay cue followed by dialogue, with an optional parenthetical)
//   "We should go."
// Straight, curly, guillemet, German, and CJK quotation marks are accepted.
// All other non-empty lines become narration. A heading written as
// ":: scene name" starts a new Ren'Py label.
ManuscriptConversion convert_manuscript_to_renpy(
    std::string_view manuscript, const ManuscriptOptions& options = {});

}  // namespace say_count
