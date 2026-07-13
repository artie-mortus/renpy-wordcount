#include "core/navigator.h"

#include "core/outline.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>

namespace say_count {

int fuzzy_score(std::string_view query, std::string_view candidate) {
    if (query.empty()) return 0;
    int score = 0;
    std::size_t position = 0;
    int previous = -2;
    for (const char raw : query) {
        const char wanted = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
        bool found = false;
        for (; position < candidate.size(); ++position) {
            const char current = static_cast<char>(std::tolower(static_cast<unsigned char>(candidate[position])));
            if (current != wanted) continue;
            score += 10;
            if (static_cast<int>(position) == previous + 1) score += 8;
            if (position == 0 || candidate[position - 1] == '/' || candidate[position - 1] == '_' ||
                candidate[position - 1] == ' ' || candidate[position - 1] == '.') score += 6;
            score -= static_cast<int>(position / 8);
            previous = static_cast<int>(position++);
            found = true;
            break;
        }
        if (!found) return -1;
    }
    return score - static_cast<int>(candidate.size() / 20);
}

std::vector<NavigationItem> build_navigation_index(const std::vector<NamedScript>& scripts) {
    std::vector<NavigationItem> result;
    for (std::size_t file_index = 0; file_index < scripts.size(); ++file_index) {
        const auto& script = scripts[file_index];
        result.push_back({NavigationKind::File, script.name, "File", file_index, 1});
        for (const auto& item : build_outline(script.content)) {
            if (item.kind != OutlineKind::Label) continue;
            result.push_back({NavigationKind::Label, item.qualified_name,
                              "Label · " + script.name + ":" + std::to_string(item.line_number),
                              file_index, item.line_number});
        }
    }
    const auto analysis = analyze_project(scripts);
    const std::regex definition(R"(^\s*(?:(?:define|default)\s+)?([A-Za-z_]\w*)\s*=\s*(?:renpy\.)?Character\b)");
    for (const auto& [alias, display] : analysis.character_names) {
        bool located = false;
        for (std::size_t file_index = 0; file_index < scripts.size() && !located; ++file_index) {
            std::istringstream lines(scripts[file_index].content);
            std::string line;
            std::size_t number = 0;
            while (std::getline(lines, line)) {
                ++number;
                std::smatch match;
                if (std::regex_search(line, match, definition) && match[1].str() == alias) {
                    result.push_back({NavigationKind::Character,
                        display.empty() ? alias : display + " (" + alias + ")",
                        "Character · " + scripts[file_index].name + ":" + std::to_string(number),
                        file_index, number});
                    located = true;
                    break;
                }
            }
        }
    }
    return result;
}

}  // namespace say_count
