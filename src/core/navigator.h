#pragma once

#include "core/parser.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

enum class NavigationKind { File, Label, Character };

struct NavigationItem {
    NavigationKind kind = NavigationKind::File;
    std::string title;
    std::string detail;
    std::size_t file_index = 0;
    std::size_t line = 1;
};

int fuzzy_score(std::string_view query, std::string_view candidate);
std::vector<NavigationItem> build_navigation_index(const std::vector<NamedScript>& scripts);

}  // namespace say_count
