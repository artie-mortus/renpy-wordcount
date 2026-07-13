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

// Extracts the static label-transition graph used by route analysis. Dynamic
// `jump expression` / `call expression` targets cannot be represented and are
// intentionally omitted.
RouteGraph build_route_graph(const std::vector<NamedScript>& scripts);

}  // namespace say_count
