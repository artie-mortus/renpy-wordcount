#include <catch2/catch_test_macros.hpp>

#include <algorithm>

#include "core/flow_layout.h"

using say_count::NamedScript;
using say_count::analyze_project;
using say_count::build_flow_layout;
using say_count::compute_routes;

TEST_CASE("flow layout layers reachable labels and groups unreachable labels") {
    const std::vector<NamedScript> scripts{{
        "flow.rpy",
        "label start:\n"
        "    jump choice\n"
        "label choice:\n"
        "    if flag:\n"
        "        jump left\n"
        "    jump right\n"
        "label left:\n"
        "    return\n"
        "label right:\n"
        "    return\n"
        "label unused_a:\n"
        "    return\n"
        "label unused_b:\n"
        "    return",
    }};
    const auto report = compute_routes(analyze_project(scripts), scripts);
    REQUIRE(report);
    const auto layout = build_flow_layout(*report);

    REQUIRE(layout.nodes.size() == 6);
    CHECK(layout.width == 250.0);
    CHECK(layout.height == 263.0);
    const auto find = [&](const std::string& name) -> const say_count::FlowNodeLayout& {
        return *std::find_if(layout.nodes.begin(), layout.nodes.end(),
                             [&](const auto& node) { return node.name == name; });
    };
    CHECK(find("start").bounds.y == 15.0);
    CHECK(find("choice").bounds.y == 77.0);
    CHECK(find("left").bounds.y == 139.0);
    CHECK(find("right").bounds.y == 139.0);
    CHECK(find("unused_a").bounds.y == 201.0);
    CHECK(find("unused_b").bounds.y == 201.0);
    CHECK(find("start").start);
    CHECK(find("left").ending);
    CHECK(find("unused_a").unreachable);
}

TEST_CASE("flow layout deduplicates edges and marks loop backs") {
    const std::vector<NamedScript> scripts{{
        "loop.rpy",
        "label start:\n"
        "    if again:\n"
        "        jump start\n"
        "    jump next\n"
        "label next:\n"
        "    jump start",
    }};
    const auto report = compute_routes(analyze_project(scripts), scripts);
    REQUIRE(report);
    const auto layout = build_flow_layout(*report);

    REQUIRE(layout.edges.size() == 3);
    CHECK(std::count_if(layout.edges.begin(), layout.edges.end(),
                        [](const auto& edge) { return edge.back_edge; }) == 2);
}

TEST_CASE("flow layout preserves node words and enforces its drawing cap") {
    const std::vector<NamedScript> scripts{{
        "words.rpy", "label start:\n    \"Three whole words.\"\n    return",
    }};
    const auto report = compute_routes(analyze_project(scripts), scripts);
    REQUIRE(report);
    const auto layout = build_flow_layout(*report);
    REQUIRE(layout.nodes.size() == 1);
    CHECK(layout.nodes.front().words == 3);

    const auto capped = build_flow_layout(*report, 0);
    CHECK(capped.too_many_nodes);
    CHECK(capped.requested_nodes == 1);
    CHECK(capped.nodes.empty());
}
