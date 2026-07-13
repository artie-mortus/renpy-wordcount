#include <catch2/catch_test_macros.hpp>

#include "core/routes.h"

using say_count::NamedScript;
using say_count::RouteEdgeKind;
using say_count::RouteBranchKind;
using say_count::analyze_project;
using say_count::build_route_graph;
using say_count::compute_routes;

TEST_CASE("route graph extracts branching transitions and source locations") {
    const auto graph = build_route_graph({NamedScript{
        "routes.rpy",
        "label start:\n"
        "    if quick:\n"
        "        jump short\n"
        "    else:\n"
        "        call long\n"
        "label short:\n"
        "    return\n"
        "label long:\n"
        "    return\n",
    }});

    REQUIRE(graph.start == "start");
    REQUIRE(graph.nodes.size() == 3);
    CHECK(graph.node_order == std::vector<std::string>{"start", "short", "long"});
    const auto& start = graph.nodes.at("start");
    CHECK(start.file == "routes.rpy");
    CHECK(start.line == 1);
    REQUIRE(start.edges.size() == 3);
    CHECK(start.edges[0] == say_count::RouteEdge{"short", RouteEdgeKind::jump, 3});
    CHECK(start.edges[1] == say_count::RouteEdge{"long", RouteEdgeKind::call, 5});
    CHECK(start.edges[2] == say_count::RouteEdge{"short", RouteEdgeKind::fall_through, 6});
    CHECK(start.condition_branches == 2);
    CHECK(start.falls_into == "short");
}

TEST_CASE("route graph qualifies local labels and static local targets") {
    const auto graph = build_route_graph({NamedScript{
        "local.rpy", "label start:\n    jump .end\nlabel .end:\n    return",
    }});

    REQUIRE(graph.nodes.count("start.end") == 1);
    REQUIRE(graph.nodes.at("start").edges.size() == 1);
    CHECK(graph.nodes.at("start").edges.front().target == "start.end");
    CHECK(graph.nodes.at("start").ends_with_jump);
    CHECK(graph.nodes.at("start.end").ends_with_return);
}

TEST_CASE("route graph adds fall-through only after nonterminal code") {
    const auto graph = build_route_graph({NamedScript{
        "flow.rpy",
        "label opening:\n"
        "    \"Continue.\"\n"
        "label next:\n"
        "    return\n"
        "label after:\n"
        "    return",
    }});

    REQUIRE(graph.start == "opening");
    REQUIRE(graph.nodes.at("opening").edges.size() == 1);
    CHECK(graph.nodes.at("opening").edges.front().kind == RouteEdgeKind::fall_through);
    CHECK(graph.nodes.at("opening").edges.front().target == "next");
    CHECK(graph.nodes.at("next").edges.empty());
    CHECK(graph.nodes.at("next").falls_into == "after");
}

TEST_CASE("route graph counts menu choices and conditional blocks") {
    const auto graph = build_route_graph({NamedScript{
        "branches.rpy",
        "label start:\n"
        "    menu:\n"
        "        \"Left\":\n"
        "            jump left\n"
        "        \"Right\" if unlocked:\n"
        "            jump right\n"
        "    if fallback:\n"
        "        jump left\n"
        "label left:\n"
        "    return\n"
        "label right:\n"
        "    return",
    }});

    const auto& start = graph.nodes.at("start");
    CHECK(start.menu_branches == 2);
    CHECK(start.condition_branches == 1);
    REQUIRE(start.edges.size() == 3);
}

TEST_CASE("route graph ignores comments, quoted hashes, and expression targets") {
    const auto graph = build_route_graph({NamedScript{
        "comments.rpy",
        "label start:\n"
        "    \"# remains dialogue\" # comment\n"
        "    jump expression destination\n"
        "    # jump nowhere\n"
        "label end:\n"
        "    return",
    }});

    // A dynamic jump has no representable edge, but remains terminal just as
    // it is in Ren'Py, so it must not create a false fall-through transition.
    CHECK(graph.nodes.at("start").edges.empty());
    CHECK(graph.nodes.at("start").ends_with_jump);
}

TEST_CASE("route graph prefers start over the first label across files") {
    const auto graph = build_route_graph({
        NamedScript{"chapter.rpy", "label opening:\n    return"},
        NamedScript{"script.rpy", "label start:\n    return"},
    });

    CHECK(graph.start == "start");
    CHECK(graph.node_order == std::vector<std::string>{"opening", "start"});
}

TEST_CASE("route walker matches controlled shortest longest and reachability metrics") {
    const std::vector<NamedScript> scripts{{
        "routes.rpy",
        "label start:\n"
        "    if quick:\n"
        "        jump short\n"
        "    jump long\n"
        "label short:\n"
        "    \"Two words.\"\n"
        "    return\n"
        "label long:\n"
        "    \"One two three four five.\"\n"
        "    return\n"
        "label unused:\n"
        "    \"Never reached.\"\n"
        "    return",
    }};
    const auto report = compute_routes(analyze_project(scripts), scripts);

    REQUIRE(report);
    CHECK(report->extremes.min_words == 2);
    CHECK(report->extremes.max_words == 5);
    CHECK(report->shortest_reading_minutes == 1);
    CHECK(report->longest_reading_minutes == 1);
    CHECK(report->endings == 2);
    CHECK(report->branches == 1);
    CHECK(report->unreachable == std::vector<std::string>{"unused"});
    REQUIRE(report->paths.size() == 2);
    CHECK(report->paths[0].labels == std::vector<std::string>{"start", "short"});
    CHECK(report->paths[1].labels == std::vector<std::string>{"start", "long"});
}

TEST_CASE("route walker detects a loop and terminates its path") {
    const std::vector<NamedScript> scripts{{
        "loop.rpy", "label start:\n    \"Again.\"\n    jump start",
    }};
    const auto report = compute_routes(analyze_project(scripts), scripts);

    REQUIRE(report);
    CHECK(report->loops);
    CHECK(report->extremes.min_words == 1);
    CHECK(report->extremes.max_words == 1);
    REQUIRE(report->paths.size() == 1);
    CHECK(report->paths.front().loop_target == "start");
}

TEST_CASE("route walker falls back to the first label and follows calls") {
    const std::vector<NamedScript> scripts{{
        "call.rpy",
        "label opening:\n"
        "    call scene\n"
        "    return\n"
        "label scene:\n"
        "    \"Hi.\"\n"
        "    return",
    }};
    const auto report = compute_routes(analyze_project(scripts), scripts);

    REQUIRE(report);
    CHECK(report->graph.start == "opening");
    CHECK(report->unreachable.empty());
    CHECK(report->extremes.min_words == 1);
    CHECK(report->endings == 1);
}

TEST_CASE("route walker reports no routes without labels") {
    const std::vector<NamedScript> scripts{{"plain.rpy", "\"Just dialogue.\""}};
    CHECK_FALSE(compute_routes(analyze_project(scripts), scripts));
}

TEST_CASE("route walker totals dialogue inside choices and conditions") {
    const std::vector<NamedScript> scripts{{
        "inline.rpy",
        "label start:\n"
        "    menu:\n"
        "        \"Left\":\n"
        "            \"Two words.\"\n"
        "        \"Right\":\n"
        "            \"Three whole words.\"\n"
        "    if persistent.flag:\n"
        "        \"Four branch words here.\"\n"
        "    return",
    }};
    const auto report = compute_routes(analyze_project(scripts), scripts);

    REQUIRE(report);
    REQUIRE(report->branch_totals.count("start") == 1);
    const auto& totals = report->branch_totals.at("start");
    REQUIRE(totals.size() == 3);
    CHECK(totals[0].kind == RouteBranchKind::choice);
    CHECK(totals[0].text == "Left");
    CHECK(totals[0].words == 2);
    CHECK(totals[1].words == 3);
    CHECK(totals[2].kind == RouteBranchKind::condition);
    CHECK(totals[2].text == "if persistent.flag");
    CHECK(totals[2].words == 4);
    CHECK(report->menu_branches == 2);
    CHECK(report->condition_branches == 1);
    CHECK(report->branches == 1);
}

TEST_CASE("route reading time uses JavaScript rounding") {
    std::string dialogue;
    for (int index = 0; index < 300; ++index) dialogue += (index ? " word" : "word");
    const std::vector<NamedScript> scripts{{
        "long.rpy", "label start:\n    \"" + dialogue + "\"\n    return",
    }};
    const auto report = compute_routes(analyze_project(scripts), scripts);

    REQUIRE(report);
    CHECK(report->extremes.min_words == 300);
    CHECK(report->shortest_reading_minutes == 2);
    CHECK(report->longest_reading_minutes == 2);
}
