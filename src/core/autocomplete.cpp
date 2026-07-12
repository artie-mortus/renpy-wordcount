#include "core/autocomplete.h"
#include "core/outline.h"
#include "core/parser.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace say_count {
namespace {
bool starts_with_case_insensitive(std::string_view value, std::string_view prefix) {
    if (prefix.size() > value.size()) return false;
    for (std::size_t i = 0; i < prefix.size(); ++i)
        if (std::tolower(static_cast<unsigned char>(value[i])) !=
            std::tolower(static_cast<unsigned char>(prefix[i]))) return false;
    return true;
}
}

CompletionResult basic_completions(std::string_view source, std::size_t caret) {
    CompletionResult result;
    caret = std::min(caret, source.size());
    const auto line_start_pos = source.rfind('\n', caret ? caret - 1 : 0);
    const std::size_t line_start = line_start_pos == std::string_view::npos ? 0 : line_start_pos + 1;
    const std::string before(source.substr(line_start, caret - line_start));
    std::smatch match;
    std::string prefix;
    if (std::regex_match(before, match, std::regex(R"(^\s*(?:jump|call)\s+(\.?[A-Za-z0-9_.]*)$)"))) {
        result.kind = CompletionKind::Label; prefix = match[1].str();
        for (const auto& item : build_outline(source)) {
            if (item.kind != OutlineKind::Label || !starts_with_case_insensitive(item.qualified_name, prefix) ||
                item.qualified_name == prefix) continue;
            result.items.push_back({item.qualified_name, item.qualified_name});
        }
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
        const auto analysis = analyze_script(source);
        for (const auto& [alias, display] : analysis.character_names) {
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
}
