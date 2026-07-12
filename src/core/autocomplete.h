#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {
enum class CompletionKind { None, Alias, Label };
struct CompletionItem {
    std::string text;
    std::string insert;
    std::size_t select_start = std::string::npos;
    std::size_t select_end = std::string::npos;
};
struct CompletionResult {
    CompletionKind kind = CompletionKind::None;
    std::size_t replace_length = 0;
    std::vector<CompletionItem> items;
};
CompletionResult basic_completions(std::string_view source, std::size_t caret);
}
