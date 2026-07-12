#include "core/editor_commands.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <vector>

namespace say_count {
namespace {

struct LineBlock {
    std::size_t start = 0;
    std::size_t end = 0;
    std::string_view text;
};

LineBlock selected_line_block(std::string_view text, std::size_t start, std::size_t end) {
    start = std::min(start, text.size());
    end = std::min(std::max(start, end), text.size());
    const auto previous = start == 0 ? std::string_view::npos : text.rfind('\n', start - 1);
    const std::size_t block_start = previous == std::string_view::npos ? 0 : previous + 1;
    const auto following = text.find('\n', end);
    const std::size_t block_end = following == std::string_view::npos ? text.size() : following;
    return {block_start, block_end, text.substr(block_start, block_end - block_start)};
}

std::vector<std::string> split_lines(std::string_view value) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= value.size()) {
        const auto newline = value.find('\n', start);
        lines.emplace_back(value.substr(start, newline == std::string_view::npos
            ? value.size() - start : newline - start));
        if (newline == std::string_view::npos) break;
        start = newline + 1;
    }
    return lines;
}

std::string join_lines(const std::vector<std::string>& lines) {
    std::string result;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index) result.push_back('\n');
        result += lines[index];
    }
    return result;
}

std::string replace_range(std::string_view source, std::size_t start, std::size_t end,
                          std::string_view replacement) {
    std::string result;
    result.reserve(source.size() - (end - start) + replacement.size());
    result.append(source.substr(0, start));
    result.append(replacement);
    result.append(source.substr(end));
    return result;
}

bool blank(std::string_view line) {
    return std::all_of(line.begin(), line.end(), [](unsigned char byte) { return std::isspace(byte) != 0; });
}

}  // namespace

EditorCommandResult toggle_line_comments(std::string_view text, std::size_t selection_start,
                                         std::size_t selection_end) {
    const auto block = selected_line_block(text, selection_start, selection_end);
    auto lines = split_lines(block.text);
    bool has_content = false;
    bool all_commented = true;
    for (const auto& line : lines) {
        if (blank(line)) continue;
        has_content = true;
        const auto first = line.find_first_not_of(" \t");
        if (first == std::string::npos || line[first] != '#') all_commented = false;
    }
    all_commented = has_content && all_commented;
    for (auto& line : lines) {
        if (blank(line)) continue;
        const auto first = line.find_first_not_of(" \t");
        if (all_commented) {
            std::size_t erase = 1;
            if (first + 1 < line.size() && line[first + 1] == ' ') ++erase;
            line.erase(first, erase);
        } else {
            line.insert(first, "# ");
        }
    }
    const std::string changed = join_lines(lines);
    const std::string result = replace_range(text, block.start, block.end, changed);
    if (lines.size() == 1) {
        const auto delta = static_cast<std::ptrdiff_t>(changed.size()) -
                           static_cast<std::ptrdiff_t>(block.text.size());
        const auto caret = static_cast<std::ptrdiff_t>(selection_start) + delta;
        const auto bounded = static_cast<std::size_t>(std::max<std::ptrdiff_t>(block.start, caret));
        return {result, std::min(bounded, block.start + changed.size()),
                std::min(bounded, block.start + changed.size())};
    }
    return {result, block.start, block.start + changed.size()};
}

EditorCommandResult move_selected_lines(std::string_view text, std::size_t selection_start,
                                        std::size_t selection_end, int direction) {
    const auto block = selected_line_block(text, selection_start, selection_end);
    if (direction < 0) {
        if (block.start == 0) return {std::string(text), selection_start, selection_end};
        const auto previous = block.start < 2 ? std::string_view::npos : text.rfind('\n', block.start - 2);
        const std::size_t previous_start = previous == std::string_view::npos ? 0 : previous + 1;
        const auto previous_text = text.substr(previous_start, block.start - previous_start - 1);
        const std::string replacement = std::string(block.text) + "\n" + std::string(previous_text);
        return {replace_range(text, previous_start, block.end, replacement),
                previous_start + (selection_start - block.start),
                previous_start + (selection_end - block.start)};
    }
    if (block.end == text.size()) return {std::string(text), selection_start, selection_end};
    const auto next_newline = text.find('\n', block.end + 1);
    const std::size_t next_end = next_newline == std::string_view::npos ? text.size() : next_newline;
    const auto next = text.substr(block.end + 1, next_end - block.end - 1);
    const std::string replacement = std::string(next) + "\n" + std::string(block.text);
    const std::size_t shift = next.size() + 1;
    return {replace_range(text, block.start, next_end, replacement),
            selection_start + shift, selection_end + shift};
}

EditorCommandResult duplicate_selected_lines(std::string_view text, std::size_t selection_start,
                                             std::size_t selection_end, int direction) {
    const auto block = selected_line_block(text, selection_start, selection_end);
    const std::string insertion = "\n" + std::string(block.text);
    const std::string result = replace_range(text, block.end, block.end, insertion);
    if (direction > 0) {
        const std::size_t shift = block.text.size() + 1;
        return {result, selection_start + shift, selection_end + shift};
    }
    return {result, selection_start, selection_end};
}

std::optional<EditorCommandResult> apply_pair_input(std::string_view text,
                                                    std::size_t selection_start,
                                                    std::size_t selection_end, char typed) {
    selection_start = std::min(selection_start, text.size());
    selection_end = std::min(std::max(selection_start, selection_end), text.size());
    const bool escaped_quote = typed == '"' && selection_start > 0 && text[selection_start - 1] == '\\';
    if (!escaped_quote && selection_start == selection_end &&
        (typed == '"' || typed == ')' || typed == ']') &&
        selection_start < text.size() && text[selection_start] == typed) {
        return EditorCommandResult{std::string(text), selection_start + 1, selection_start + 1};
    }
    char close = '\0';
    if (typed == '"') close = '"';
    else if (typed == '(') close = ')';
    else if (typed == '[') close = ']';
    if (close == '\0' || (selection_start > 0 && text[selection_start - 1] == '\\')) return std::nullopt;
    const std::string selected(text.substr(selection_start, selection_end - selection_start));
    const std::string insertion = std::string(1, typed) + selected + close;
    const std::string result = replace_range(text, selection_start, selection_end, insertion);
    if (!selected.empty()) return EditorCommandResult{result, selection_start + 1,
                                                       selection_start + 1 + selected.size()};
    return EditorCommandResult{result, selection_start + 1, selection_start + 1};
}

std::optional<std::size_t> adjacent_label_line(std::string_view text, std::size_t current_line,
                                              int direction) {
    static const std::regex label(R"(^\s*label\s+)");
    std::optional<std::size_t> previous;
    std::size_t start = 0;
    std::size_t line_number = 1;
    while (start <= text.size()) {
        const auto newline = text.find('\n', start);
        const auto line = text.substr(start, newline == std::string_view::npos
            ? text.size() - start : newline - start);
        if (std::regex_search(line.begin(), line.end(), label)) {
            if (direction > 0 && line_number > current_line) return line_number;
            if (direction < 0 && line_number < current_line) previous = line_number;
        }
        if (newline == std::string_view::npos) break;
        start = newline + 1;
        ++line_number;
    }
    return previous;
}

}  // namespace say_count
