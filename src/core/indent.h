#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct IndentChange {
    std::size_t line = 0;
    std::size_t before_width = 0;
    std::size_t after_width = 0;
    std::string before;
    std::string after;
};

struct IndentPreview {
    std::string original;
    std::string fixed;
    std::vector<IndentChange> changes;
};

IndentPreview preview_indent_fix(std::string_view text);
std::size_t next_line_indentation(std::string_view previous_line);

}  // namespace say_count
