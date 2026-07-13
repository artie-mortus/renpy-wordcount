#include <catch2/catch_test_macros.hpp>

#include "core/indent.h"

using say_count::preview_indent_fix;

TEST_CASE("indent preview snaps tabs and spaces to nearest four") {
    const auto preview = preview_indent_fix(
        "label start:\n"
        "  menu:\n"
        "      \"Choice\":\n"
        "\t   jump end\n"
        "label end:\n");
    CHECK(preview.fixed ==
        "label start:\n"
        "    menu:\n"
        "        \"Choice\":\n"
        "        jump end\n"
        "label end:\n");
    REQUIRE(preview.changes.size() == 3);
    CHECK(preview.changes[0].line == 2);
    CHECK(preview.changes[0].before_width == 2);
    CHECK(preview.changes[0].after_width == 4);
}

TEST_CASE("indent preview preserves nested Python text and line endings") {
    const std::string source =
        "python:\r\n"
        "     if value:\r\n"
        "         print(\"x\")\r\n";
    const auto preview = preview_indent_fix(source);
    CHECK(preview.fixed ==
        "python:\r\n"
        "    if value:\r\n"
        "        print(\"x\")\r\n");
    CHECK(preview.changes.size() == 2);
}

TEST_CASE("clean indentation produces an empty preview") {
    const std::string source = "label start:\n    return";
    const auto preview = preview_indent_fix(source);
    CHECK(preview.fixed == source);
    CHECK(preview.changes.empty());
}
