#include "core/routes.h"

#include <regex>
#include <string_view>

namespace say_count {
namespace {

std::string trim(std::string_view value) {
    const auto first = value.find_first_not_of(" \t\r\n");
    if (first == std::string_view::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n");
    return std::string(value.substr(first, last - first + 1));
}

std::string strip_comments(std::string_view line) {
    std::string result;
    char quote = '\0';
    bool escaped = false;
    for (const char character : line) {
        if (escaped) {
            result += character;
            escaped = false;
            continue;
        }
        if (character == '\\') {
            result += character;
            escaped = true;
            continue;
        }
        if ((character == '\"' || character == '\'') && quote == '\0') {
            quote = character;
            result += character;
            continue;
        }
        if (character == quote) {
            quote = '\0';
            result += character;
            continue;
        }
        if (character == '#' && quote == '\0') break;
        result += character;
    }
    return result;
}

std::size_t indentation(std::string_view line) {
    const auto first = line.find_first_not_of(" \t");
    return first == std::string_view::npos ? line.size() : first;
}

std::string qualify_local_label(std::string name, const std::string& global_label) {
    if (!name.empty() && name.front() == '.' && !global_label.empty()) return global_label + name;
    return name;
}

}  // namespace

RouteGraph build_route_graph(const std::vector<NamedScript>& scripts) {
    static const std::regex label_pattern(
        R"(^label\s+(\.?[A-Za-z_][A-Za-z0-9_.]*)\s*(?:\([^)]*\))?\s*:)");
    static const std::regex menu_pattern(R"(^menu\s*:)");
    static const std::regex menu_choice_pattern(R"(^\".*:\s*(?:if\b.*)?$)");
    static const std::regex condition_pattern(R"(^(if|elif|else)\b.*:)");
    static const std::regex flow_pattern(
        R"(^(jump|call)\s+(\.?[A-Za-z_][A-Za-z0-9_.]*))");
    static const std::regex terminal_pattern(R"(^(jump|return)\b)");

    RouteGraph graph;
    for (const auto& script : scripts) {
        RouteNode* current = nullptr;
        std::string current_global_label;
        std::optional<std::size_t> menu_indent;
        std::string last_code;
        std::size_t line_number = 1;
        std::size_t offset = 0;

        while (offset <= script.content.size()) {
            const auto newline = script.content.find('\n', offset);
            const auto length = newline == std::string::npos ? script.content.size() - offset : newline - offset;
            std::string_view raw_line(script.content.data() + offset, length);
            if (!raw_line.empty() && raw_line.back() == '\r') raw_line.remove_suffix(1);
            const std::string code = trim(strip_comments(raw_line));

            if (!code.empty()) {
                std::smatch match;
                if (std::regex_search(code, match, label_pattern)) {
                    const std::string raw_name = match[1].str();
                    const std::string name = qualify_local_label(raw_name, current_global_label);
                    if (raw_name.front() != '.') current_global_label = raw_name;

                    if (current) {
                        current->falls_into = name;
                        current->ends_with_jump = std::regex_search(last_code, terminal_pattern)
                                                  && last_code.rfind("jump", 0) == 0;
                        current->ends_with_return = std::regex_search(last_code, terminal_pattern)
                                                    && last_code.rfind("return", 0) == 0;
                        if (!current->ends_with_jump && !current->ends_with_return) {
                            current->edges.push_back({name, RouteEdgeKind::fall_through, line_number});
                        }
                    }

                    const auto inserted = graph.nodes.emplace(name, RouteNode{name, script.name, line_number});
                    if (inserted.second) graph.node_order.push_back(name);
                    current = inserted.second ? &inserted.first->second : nullptr;
                    if (!graph.start) graph.start = name;
                    if (name == "start") graph.start = name;
                    menu_indent.reset();
                    last_code.clear();
                } else if (current) {
                    const std::size_t indent = indentation(raw_line);
                    if (std::regex_search(code, menu_pattern)) {
                        menu_indent = indent;
                        last_code = code;
                    } else {
                        if (menu_indent && indent <= *menu_indent) menu_indent.reset();
                        if (menu_indent && std::regex_match(code, menu_choice_pattern)) {
                            ++current->menu_branches;
                        }
                        if (std::regex_search(code, condition_pattern)) ++current->condition_branches;
                        if (std::regex_search(code, match, flow_pattern) && match[2].str() != "expression") {
                            current->edges.push_back({
                                qualify_local_label(match[2].str(), current_global_label),
                                match[1].str() == "jump" ? RouteEdgeKind::jump : RouteEdgeKind::call,
                                line_number,
                            });
                        }
                        last_code = code;
                    }
                }
            }

            if (newline == std::string::npos) break;
            offset = newline + 1;
            ++line_number;
        }

        if (current) {
            current->ends_with_jump = std::regex_search(last_code, terminal_pattern)
                                      && last_code.rfind("jump", 0) == 0;
            current->ends_with_return = std::regex_search(last_code, terminal_pattern)
                                        && last_code.rfind("return", 0) == 0;
        }
    }

    if (graph.nodes.count("start")) graph.start = "start";
    return graph;
}

}  // namespace say_count
