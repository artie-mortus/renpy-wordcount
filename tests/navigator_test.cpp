#include <catch2/catch_test_macros.hpp>

#include "core/navigator.h"

TEST_CASE("fuzzy score rewards ordered contiguous and boundary matches") {
    CHECK(say_count::fuzzy_score("scr", "script.rpy") > say_count::fuzzy_score("scr", "scene character"));
    CHECK(say_count::fuzzy_score("xyz", "script.rpy") == -1);
    CHECK(say_count::fuzzy_score("", "anything") == 0);
}

TEST_CASE("navigation index contains files labels and character definitions") {
    const std::vector<say_count::NamedScript> scripts{{"characters.rpy",
        "define e = Character(\"Eileen\")\n"}, {"script.rpy",
        "label start:\n    e \"Hello.\"\nlabel ending:\n    return\n"}};
    const auto items = say_count::build_navigation_index(scripts);
    bool file = false, label = false, character = false;
    for (const auto& item : items) {
        if (item.kind == say_count::NavigationKind::File && item.title == "script.rpy") file = true;
        if (item.kind == say_count::NavigationKind::Label && item.title == "ending" && item.line == 3) label = true;
        if (item.kind == say_count::NavigationKind::Character && item.title == "Eileen (e)" &&
            item.file_index == 0 && item.line == 1) character = true;
    }
    CHECK(file);
    CHECK(label);
    CHECK(character);
}
