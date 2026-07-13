#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <cmath>

#include "core/flow_layout.h"

using say_count::NamedScript;
using say_count::analyze_project;
using say_count::build_flow_layout;
using say_count::compute_routes;
using say_count::flow_fit_zoom;
using say_count::hit_test_flow_node;

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

TEST_CASE("flow hit testing resolves every controlled node and rejects gaps") {
    const std::vector<NamedScript> scripts{{
        "hits.rpy",
        "label start:\n"
        "    if flag:\n"
        "        jump left\n"
        "    jump right\n"
        "label left:\n"
        "    return\n"
        "label right:\n"
        "    return",
    }};
    const auto report = compute_routes(analyze_project(scripts), scripts);
    REQUIRE(report);
    const auto layout = build_flow_layout(*report);

    for (const auto& node : layout.nodes) {
        const auto* hit = hit_test_flow_node(
            layout, node.bounds.x + node.bounds.width / 2.0,
            node.bounds.y + node.bounds.height / 2.0);
        REQUIRE(hit);
        CHECK(hit->name == node.name);
        CHECK(hit->file == "hits.rpy");
        CHECK(hit->line > 0);
    }
    CHECK(hit_test_flow_node(layout, -1, -1) == nullptr);
    CHECK(hit_test_flow_node(layout, layout.width + 1, layout.height + 1) == nullptr);
}

TEST_CASE("flow fit zoom respects viewport padding and zoom limits") {
    say_count::FlowLayout layout;
    layout.width = 250;
    layout.height = 263;
    const double fitted = flow_fit_zoom(layout, 500, 300);
    CHECK(std::abs(fitted - (268.0 / 263.0)) < 0.0001);
    CHECK(flow_fit_zoom(layout, 2000, 2000) == 2.0);
    CHECK(flow_fit_zoom(layout, 20, 20) == 0.5);
    CHECK(flow_fit_zoom({}, 500, 300) == 1.0);
}
