#include <catch2/catch_test_macros.hpp>

#include "core/renpy_lint.h"

TEST_CASE("official RenPy lint parser accepts compact and traceback locations") {
    const auto issues = say_count::parse_renpy_lint(
        "game/script.rpy:12 The label end is not reachable.\n"
        "File \"game/chapter two.rpy\", line 7: could not find image bg room\n"
        "Statistics:\n  100 words\n");
    REQUIRE(issues.size() == 2);
    REQUIRE(issues[0].file == "game/script.rpy");
    REQUIRE(issues[0].line == 12);
    REQUIRE(issues[0].message == "The label end is not reachable.");
    REQUIRE(issues[1].file == "game/chapter two.rpy");
    REQUIRE(issues[1].line == 7);
}

TEST_CASE("official RenPy lint parser ignores summaries and malformed locations") {
    REQUIRE(say_count::parse_renpy_lint("Lint is not a substitute for thorough testing.\n").empty());
    REQUIRE(say_count::parse_renpy_lint("script.rpy:not-a-line bad\n").empty());
}
