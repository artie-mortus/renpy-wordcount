#pragma once
#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "core/parser.h"

namespace say_count {
enum class CompletionKind { None, Alias, Label, Image, Audio, Screen, Variable };
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

struct CompletionIndex {
    std::map<std::string, std::string> character_names;
    std::vector<std::string> labels;
    std::vector<std::string> images;
    std::vector<std::string> audio;
    std::vector<std::string> screens;
    std::vector<std::string> variables;
};

CompletionIndex build_completion_index(const std::vector<NamedScript>& scripts);
CompletionIndex build_completion_index(const NamedScript& script);
CompletionIndex merge_completion_indexes(const std::vector<CompletionIndex>& indexes);
CompletionResult project_completions(std::string_view source, std::size_t caret,
                                     const CompletionIndex& index);
CompletionResult basic_completions(std::string_view source, std::size_t caret);
}
