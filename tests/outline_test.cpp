#include <catch2/catch_test_macros.hpp>
#include "core/outline.h"

TEST_CASE("outline records labels choices and flow with resolved navigation") {
    const auto items = say_count::build_outline(
        "label start:\n    \"Choose\":\n        call .aside\n    jump missing\nlabel .aside:\n    \"Hi\"\n");
    REQUIRE(items.size() == 5);
    REQUIRE(items[0].kind == say_count::OutlineKind::Label); REQUIRE(items[0].name == "start");
    REQUIRE(items[1].kind == say_count::OutlineKind::Choice); REQUIRE(items[1].line_number == 2);
    REQUIRE(items[2].kind == say_count::OutlineKind::Call); REQUIRE(items[2].qualified_name == "start.aside");
    REQUIRE(items[2].target_found); REQUIRE(items[2].target_line == 5);
    REQUIRE_FALSE(items[3].target_found); REQUIRE(items[3].target_line == 4);
    REQUIRE(items[4].qualified_name == "start.aside");
}
