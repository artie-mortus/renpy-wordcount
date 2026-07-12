#include "core/autocomplete.h"
#include "core/outline.h"
#include "core/parser.h"
#include "core/tokenizer.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <set>
#include <sstream>

namespace say_count {
namespace {
bool starts_with_case_insensitive(std::string_view value, std::string_view prefix) {
    if (prefix.size() > value.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(value[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
    return true;
}

template <typename Container>
std::vector<std::string> sorted_vector(const Container& values) {
    return {values.begin(), values.end()};
}

void append_symbol_items(CompletionResult& result, const std::vector<std::string>& values,
                         std::string_view prefix, std::string_view suffix = {}) {
    for (const auto& value : values) {
        if (!starts_with_case_insensitive(value, prefix) || value == prefix) continue;
        result.items.push_back({value, value + std::string(suffix)});
    }
}
}

CompletionIndex build_completion_index(const std::vector<NamedScript>& scripts) {
    CompletionIndex index;
    std::set<std::string> labels, images, audio, screens, variables;
    index.character_names = analyze_project(scripts).character_names;

    static const std::regex image_pattern(
        R"(^image\s+([A-Za-z_][A-Za-z0-9_.]*(?:\s+[A-Za-z_][A-Za-z0-9_.]*)*)\s*=)");
    static const std::regex screen_pattern(R"(^screen\s+([A-Za-z_]\w*))");
    static const std::regex default_pattern(R"(^default\s+([A-Za-z_]\w*)\s*=)");
    static const std::regex define_pattern(R"(^define\s+([A-Za-z_]\w*)\s*=)");
    static const std::regex character_pattern(
        R"(^define\s+[A-Za-z_]\w*\s*=\s*(?:renpy\.)?Character\b)");
    static const std::regex assignment_pattern(R"(^\$\s*([A-Za-z_]\w*)\s*=)");
    static const std::regex audio_definition_pattern(R"(^define\s+audio\.([A-Za-z0-9_.]+)\s*=)");
    static const std::regex audio_path_pattern(
        R"audio(^(?:play|queue)\s+(?:music|sound|audio|voice)\s+"([^"\\]+)")audio");

    for (const auto& script : scripts) {
        for (const auto& item : build_outline(script.content)) {
            if (item.kind == OutlineKind::Label) labels.insert(item.qualified_name);
        }
        std::istringstream lines(script.content);
        std::string line;
        while (std::getline(lines, line)) {
            const std::string& code = tokenize_line(line, 0).code;
            std::smatch match;
            if (std::regex_search(code, match, image_pattern)) images.insert(match[1].str());
            if (std::regex_search(code, match, screen_pattern)) screens.insert(match[1].str());
            if (std::regex_search(code, match, default_pattern)) variables.insert(match[1].str());
            if (!std::regex_search(code, character_pattern) &&
                std::regex_search(code, match, define_pattern)) variables.insert(match[1].str());
            if (std::regex_search(code, match, assignment_pattern)) variables.insert(match[1].str());
            if (std::regex_search(code, match, audio_definition_pattern)) audio.insert(match[1].str());
            if (std::regex_search(code, match, audio_path_pattern)) audio.insert(match[1].str());
        }
    }
    index.labels = sorted_vector(labels);
    index.images = sorted_vector(images);
    index.audio = sorted_vector(audio);
    index.screens = sorted_vector(screens);
    index.variables = sorted_vector(variables);
    return index;
}

CompletionResult project_completions(std::string_view source, std::size_t caret,
                                     const CompletionIndex& index) {
    CompletionResult result;
    caret = std::min(caret, source.size());
    const auto line_start_pos = caret == 0 ? std::string_view::npos : source.rfind('\n', caret - 1);
    const std::size_t line_start = line_start_pos == std::string_view::npos ? 0 : line_start_pos + 1;
    const std::string before(source.substr(line_start, caret - line_start));
    std::smatch match;
    std::string prefix;
    if (std::regex_match(before, match, std::regex(R"(^\s*call\s+screen\s+([A-Za-z0-9_.]*)$)"))) {
        result.kind = CompletionKind::Screen; prefix = match[1].str();
        append_symbol_items(result, index.screens, prefix);
    } else if (std::regex_match(before, match, std::regex(R"(^\s*(?:jump|call)\s+(\.?[A-Za-z0-9_.]*)$)"))) {
        result.kind = CompletionKind::Label; prefix = match[1].str();
        append_symbol_items(result, index.labels, prefix);
    } else if (std::regex_match(before, match,
                                std::regex(R"(^\s*(?:show|hide|scene)\s+([A-Za-z0-9_. ]*)$)"))) {
        result.kind = CompletionKind::Image; prefix = match[1].str();
        append_symbol_items(result, index.images, prefix);
    } else if (std::regex_match(before, match, std::regex(
                   R"(^\s*(?:play|queue)\s+(?:music|sound|audio|voice)\s+"([^"\\]*)$)"))) {
        result.kind = CompletionKind::Audio; prefix = match[1].str();
        append_symbol_items(result, index.audio, prefix, "\"");
    } else if (std::regex_match(before, match, std::regex(R"(^\s*\$\s*([A-Za-z_]\w*)$)"))) {
        result.kind = CompletionKind::Variable; prefix = match[1].str();
        append_symbol_items(result, index.variables, prefix);
    } else if (std::regex_match(before, match, std::regex(R"(^\s*([A-Za-z_]\w*)$)"))) {
        result.kind = CompletionKind::Alias; prefix = match[1].str();
        const std::string indent = before.substr(0, before.size() - prefix.size());
        const std::vector<CompletionItem> snippets = {
            {"label", "label name:\n" + indent + "    ", 6, 10},
            {"menu", "menu:\n" + indent + "    \"Choice.\":\n" + indent + "        ", 11 + indent.size(), 18 + indent.size()},
            {"define", "define x = Character(\"Name\")", 7, 8}
        };
        for (const auto& item : snippets)
            if (starts_with_case_insensitive(item.text, prefix) && item.text != prefix) result.items.push_back(item);
        for (const auto& [alias, display] : index.character_names) {
            (void)display;
            if (starts_with_case_insensitive(alias, prefix) && alias != prefix)
                result.items.push_back({alias, alias + " "});
        }
        if (starts_with_case_insensitive("extend", prefix) && prefix != "extend")
            result.items.push_back({"extend", "extend "});
    }
    result.replace_length = prefix.size();
    std::stable_sort(result.items.begin(), result.items.end(), [](const auto& left, const auto& right) {
        const bool left_snippet = left.select_start != std::string::npos;
        const bool right_snippet = right.select_start != std::string::npos;
        return left_snippet != right_snippet ? left_snippet : left.text < right.text;
    });
    result.items.erase(std::unique(result.items.begin(), result.items.end(),
        [](const auto& left, const auto& right) { return left.text == right.text; }), result.items.end());
    if (result.items.size() > 8) result.items.resize(8);
    if (result.items.empty()) result.kind = CompletionKind::None;
    return result;
}

CompletionResult basic_completions(std::string_view source, std::size_t caret) {
    const std::vector<NamedScript> scripts{{"script.rpy", std::string(source)}};
    return project_completions(source, caret, build_completion_index(scripts));
}
}
