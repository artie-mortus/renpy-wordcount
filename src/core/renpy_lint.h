#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct RenpyLintIssue {
    std::string file;
    std::size_t line = 0;
    std::string message;
};

std::vector<RenpyLintIssue> parse_renpy_lint(std::string_view output);

}  // namespace say_count
