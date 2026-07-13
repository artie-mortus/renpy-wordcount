#pragma once

#include "core/parser.h"

#include <cstddef>
#include <string>
#include <vector>

namespace say_count {

enum class RenameKind { SpeakerAlias, Label };

struct RenameChange {
    std::size_t file_index = 0;
    std::string file;
    std::size_t line = 0;
    std::string before;
    std::string after;
};

struct RenamePlan {
    std::vector<NamedScript> files;
    std::vector<RenameChange> changes;
    std::string error;
};

RenamePlan plan_symbol_rename(const std::vector<NamedScript>& files, RenameKind kind,
                              std::string_view from, std::string_view to);

}  // namespace say_count
