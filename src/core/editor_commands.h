#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace say_count {

struct EditorCommandResult {
    std::string text;
    std::size_t selection_start = 0;
    std::size_t selection_end = 0;
};

EditorCommandResult toggle_line_comments(std::string_view text, std::size_t selection_start,
                                         std::size_t selection_end);
EditorCommandResult move_selected_lines(std::string_view text, std::size_t selection_start,
                                        std::size_t selection_end, int direction);
EditorCommandResult duplicate_selected_lines(std::string_view text, std::size_t selection_start,
                                             std::size_t selection_end, int direction);
std::optional<EditorCommandResult> apply_pair_input(std::string_view text,
                                                    std::size_t selection_start,
                                                    std::size_t selection_end, char typed);
std::optional<std::size_t> adjacent_label_line(std::string_view text, std::size_t current_line,
                                              int direction);

}  // namespace say_count
