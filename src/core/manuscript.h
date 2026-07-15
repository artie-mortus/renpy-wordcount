#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct ManuscriptOptions {
    std::string label = "start";
    bool include_character_definitions = true;
};

struct ManuscriptCharacter {
    std::string name;
    std::string alias;
};

struct ManuscriptConversion {
    std::string script;
    std::vector<ManuscriptCharacter> characters;
    std::size_t dialogue_lines = 0;
    std::size_t narration_lines = 0;
};

// Converts manuscript-style dialogue into paste-ready Ren'Py. Supported forms:
//   Eileen: We should go.
//   Eileen said, "We should go."
//   "We should go," Eileen said.
//   Eileen             (speaker line followed by a quoted line)
//   "We should go."
// All other non-empty lines become narration. A heading written as
// ":: scene name" starts a new Ren'Py label.
ManuscriptConversion convert_manuscript_to_renpy(
    std::string_view manuscript, const ManuscriptOptions& options = {});

}  // namespace say_count
