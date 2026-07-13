#include "core/routes.h"

#include <algorithm>
#include <functional>
#include <regex>
#include <string_view>
#include <unordered_map>
#include <unordered_set>

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

std::size_t reading_minutes(std::size_t words) {
    return words ? std::max<std::size_t>(1, (words + 100) / 200) : 0;
}

std::vector<std::string> existing_targets(const RouteNode& node, const RouteGraph& graph) {
    std::vector<std::string> result;
    std::unordered_set<std::string> seen;
    for (const auto& edge : node.edges) {
        if (graph.nodes.count(edge.target) && seen.insert(edge.target).second)
            result.push_back(edge.target);
    }
    return result;
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

std::map<std::string, std::vector<RouteBranchTotal>> collect_inline_branch_totals(
    const std::vector<NamedScript>& scripts, const ScriptAnalysis& analysis) {
    static const std::regex label_pattern(
        R"(^label\s+(\.?[A-Za-z_][A-Za-z0-9_.]*)\s*(?:\([^)]*\))?\s*:)");
    static const std::regex menu_pattern(R"(^menu\s*:)");
    static const std::regex choice_pattern(
        R"re(^(?:"((?:[^"\\]|\\.)*)"|'((?:[^'\\]|\\.)*)')(?:\s+if\b.+)?\s*:)re");
    static const std::regex condition_pattern(R"(^(if|elif|else)\b.*:\s*$)");

    std::unordered_map<std::string, std::size_t> counted_words;
    for (const auto& item : analysis.counted) {
        counted_words[item.file + ":" + std::to_string(item.line_number)] = item.words;
    }

    std::map<std::string, std::vector<RouteBranchTotal>> totals;
    for (const auto& script : scripts) {
        std::string current_label;
        std::string current_global_label;
        std::optional<std::size_t> menu_indent;
        std::optional<std::size_t> active_index;
        std::size_t line_number = 1;
        std::size_t offset = 0;

        while (offset <= script.content.size()) {
            const auto newline = script.content.find('\n', offset);
            const auto length = newline == std::string::npos ? script.content.size() - offset : newline - offset;
            std::string_view raw_line(script.content.data() + offset, length);
            if (!raw_line.empty() && raw_line.back() == '\r') raw_line.remove_suffix(1);
            const std::string code = trim(strip_comments(raw_line));

            if (!code.empty()) {
                const std::size_t indent = [&] {
                    std::size_t width = 0;
                    for (const char character : raw_line) {
                        if (character == ' ') ++width;
                        else if (character == '\t') width += 4;
                        else break;
                    }
                    return width;
                }();
                std::smatch match;
                if (std::regex_search(code, match, label_pattern)) {
                    const std::string raw_name = match[1].str();
                    current_label = qualify_local_label(raw_name, current_global_label);
                    if (raw_name.front() != '.') current_global_label = raw_name;
                    menu_indent.reset();
                    active_index.reset();
                } else if (!current_label.empty()) {
                    auto& entries = totals[current_label];
                    if (active_index && indent <= entries[*active_index].indent) active_index.reset();
                    if (menu_indent && indent <= *menu_indent) menu_indent.reset();

                    if (std::regex_search(code, menu_pattern)) {
                        menu_indent = indent;
                        active_index.reset();
                    } else if (menu_indent && indent > *menu_indent &&
                               std::regex_search(code, match, choice_pattern)) {
                        const std::string choice = match[1].matched ? match[1].str() : match[2].str();
                        entries.push_back({RouteBranchKind::choice, clean_renpy_text(choice), indent, 0,
                                           line_number});
                        active_index = entries.size() - 1;
                    } else if (std::regex_match(code, match, condition_pattern)) {
                        const std::string text = match[1].str() == "else"
                            ? "else" : code.substr(0, code.find_last_of(':'));
                        entries.push_back({RouteBranchKind::condition, text, indent, 0, line_number});
                        active_index = entries.size() - 1;
                    }

                    if (active_index) {
                        const auto found = counted_words.find(
                            script.name + ":" + std::to_string(line_number));
                        if (found != counted_words.end()) entries[*active_index].words += found->second;
                    }
                    if (entries.empty()) totals.erase(current_label);
                }
            }

            if (newline == std::string::npos) break;
            offset = newline + 1;
            ++line_number;
        }
    }
    return totals;
}

std::optional<RouteReport> compute_routes(const ScriptAnalysis& analysis,
                                          const std::vector<NamedScript>& scripts) {
    constexpr std::size_t kMaxDetailedPaths = 512;
    RouteReport report;
    report.graph = build_route_graph(scripts);
    if (!report.graph.start) return std::nullopt;

    std::unordered_map<std::string, std::size_t> words;
    for (const auto& scene : analysis.scenes) words[scene.name] = scene.words;
    auto words_of = [&](const std::string& name) {
        const auto found = words.find(name);
        return found == words.end() ? std::size_t{0} : found->second;
    };

    std::unordered_map<std::string, RouteExtremes> memo;
    std::unordered_set<std::string> visiting;
    std::function<std::optional<RouteExtremes>(const std::string&)> walk;
    walk = [&](const std::string& name) -> std::optional<RouteExtremes> {
        if (const auto found = memo.find(name); found != memo.end()) return found->second;
        if (!visiting.insert(name).second) {
            report.loops = true;
            return std::nullopt;
        }
        std::optional<std::size_t> minimum;
        std::optional<std::size_t> maximum;
        for (const auto& target : existing_targets(report.graph.nodes.at(name), report.graph)) {
            const auto subroute = walk(target);
            if (!subroute) continue;
            minimum = minimum ? std::min(*minimum, subroute->min_words) : subroute->min_words;
            maximum = maximum ? std::max(*maximum, subroute->max_words) : subroute->max_words;
        }
        visiting.erase(name);
        RouteExtremes result{words_of(name) + minimum.value_or(0),
                             words_of(name) + maximum.value_or(0)};
        memo.emplace(name, result);
        return result;
    };
    report.extremes = *walk(*report.graph.start);
    report.shortest_reading_minutes = reading_minutes(report.extremes.min_words);
    report.longest_reading_minutes = reading_minutes(report.extremes.max_words);

    std::unordered_set<std::string> reachable;
    std::vector<std::string> queue{*report.graph.start};
    while (!queue.empty()) {
        std::string name = std::move(queue.back());
        queue.pop_back();
        if (!reachable.insert(name).second) continue;
        for (const auto& target : existing_targets(report.graph.nodes.at(name), report.graph))
            queue.push_back(target);
    }

    for (const auto& name : report.graph.node_order) {
        const auto& node = report.graph.nodes.at(name);
        if (!reachable.count(name)) {
            report.unreachable.push_back(name);
            continue;
        }
        const auto targets = existing_targets(node, report.graph);
        if (targets.empty()) ++report.endings;
        if (targets.size() > 1 || node.menu_branches > 1 || node.condition_branches > 0)
            ++report.branches;
        report.menu_branches += node.menu_branches;
        report.condition_branches += node.condition_branches;
    }
    report.branch_totals = collect_inline_branch_totals(scripts, analysis);

    std::vector<std::string> path;
    std::unordered_set<std::string> path_labels;
    std::function<void(const std::string&, std::size_t)> enumerate;
    enumerate = [&](const std::string& name, std::size_t path_words) {
        if (report.paths.size() >= kMaxDetailedPaths) {
            report.paths_truncated = true;
            return;
        }
        path.push_back(name);
        path_labels.insert(name);
        path_words += words_of(name);
        const auto targets = existing_targets(report.graph.nodes.at(name), report.graph);
        for (const auto& target : targets) {
            if (report.paths.size() >= kMaxDetailedPaths) {
                report.paths_truncated = true;
                break;
            }
            if (path_labels.count(target)) {
                report.paths.push_back({path, path_words, reading_minutes(path_words), target});
            } else {
                enumerate(target, path_words);
            }
        }
        if (targets.empty()) report.paths.push_back({path, path_words, reading_minutes(path_words), std::nullopt});
        path_labels.erase(name);
        path.pop_back();
    };
    enumerate(*report.graph.start, 0);
    return report;
}

}  // namespace say_count
