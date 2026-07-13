#include "core/indent.h"

namespace say_count {

IndentPreview preview_indent_fix(std::string_view text) {
    IndentPreview result;
    result.original = std::string(text);
    std::size_t offset = 0, line_number = 1;
    while (offset <= text.size()) {
        const auto newline = text.find('\n', offset);
        const auto length = newline == std::string_view::npos ? text.size() - offset : newline - offset;
        const std::string before(text.substr(offset, length));
        std::string expanded;
        expanded.reserve(before.size());
        for (const char character : before) expanded += character == '\t' ? "    " : std::string(1, character);
        const auto first = expanded.find_first_not_of(' ');
        const std::size_t width = first == std::string::npos ? expanded.size() : first;
        const std::size_t fixed_width = ((width + 2) / 4) * 4;
        const std::string after = std::string(fixed_width, ' ') + expanded.substr(width);
        result.fixed += after;
        if (newline != std::string_view::npos) result.fixed += '\n';
        if (after != before) result.changes.push_back({line_number, width, fixed_width, before, after});
        if (newline == std::string_view::npos) break;
        offset = newline + 1;
        ++line_number;
    }
    return result;
}

}  // namespace say_count
