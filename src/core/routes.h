#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "core/parser.h"

namespace say_count {

enum class RouteEdgeKind {
    jump,
    call,
    fall_through,
};

struct RouteEdge {
    std::string target;
    RouteEdgeKind kind = RouteEdgeKind::jump;
    std::size_t line = 0;

    bool operator==(const RouteEdge& other) const noexcept {
        return target == other.target && kind == other.kind && line == other.line;
    }
};

struct RouteNode {
    std::string name;
    std::string file;
    std::size_t line = 0;
    std::vector<RouteEdge> edges;
    std::optional<std::string> falls_into;
    std::size_t menu_branches = 0;
    std::size_t condition_branches = 0;
    bool ends_with_jump = false;
    bool ends_with_return = false;
};

struct RouteGraph {
    std::map<std::string, RouteNode> nodes;
    // std::map supplies convenient lookup; this preserves JavaScript Map/file
    // declaration order for route issue lists and later flow-map layout.
    std::vector<std::string> node_order;
    std::optional<std::string> start;
};

enum class RouteBranchKind {
    choice,
    condition,
};

struct RouteBranchTotal {
    RouteBranchKind kind = RouteBranchKind::choice;
    std::string text;
    std::size_t indent = 0;
    std::size_t words = 0;
    std::size_t line = 0;
};

struct RouteExtremes {
    std::size_t min_words = 0;
    std::size_t max_words = 0;
};

struct RoutePath {
    std::vector<std::string> labels;
    std::size_t words = 0;
    std::size_t reading_minutes = 0;
    std::optional<std::string> loop_target;
};

struct RouteReport {
    RouteGraph graph;
    RouteExtremes extremes;
    std::size_t shortest_reading_minutes = 0;
    std::size_t longest_reading_minutes = 0;
    std::size_t endings = 0;
    std::size_t branches = 0;
    std::size_t menu_branches = 0;
    std::size_t condition_branches = 0;
    std::vector<std::string> unreachable;
    bool loops = false;
    std::map<std::string, std::vector<RouteBranchTotal>> branch_totals;
    std::vector<RoutePath> paths;
    bool paths_truncated = false;
};

// Extracts the static label-transition graph used by route analysis. Dynamic
// `jump expression` / `call expression` targets cannot be represented and are
// intentionally omitted.
RouteGraph build_route_graph(const std::vector<NamedScript>& scripts);

std::map<std::string, std::vector<RouteBranchTotal>> collect_inline_branch_totals(
    const std::vector<NamedScript>& scripts, const ScriptAnalysis& analysis);

// Returns no report when the project declares no labels.
std::optional<RouteReport> compute_routes(const ScriptAnalysis& analysis,
                                          const std::vector<NamedScript>& scripts);

}  // namespace say_count
