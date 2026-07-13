#include <catch2/catch_test_macros.hpp>

#include "core/routes.h"

using say_count::NamedScript;
using say_count::RouteEdgeKind;
using say_count::build_route_graph;

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
