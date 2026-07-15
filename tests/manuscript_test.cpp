#include <catch2/catch_test_macros.hpp>

#include "core/manuscript.h"

using namespace say_count;

TEST_CASE("manuscript dialogue and narration convert to a RenPy scene") {
    const auto result = convert_manuscript_to_renpy(
        "Rain pressed silver lines against the window.\n"
        "Eileen: \"We should go.\"\n"
        "\"Not yet,\" Lucy whispered.\n"
        "Lucy said, \"The road is flooded.\"");

    CHECK(result.script ==
        "define eileen = Character(\"Eileen\")\n"
        "define lucy = Character(\"Lucy\")\n\n"
        "label start:\n"
        "    \"Rain pressed silver lines against the window.\"\n"
        "    eileen \"We should go.\"\n"
        "    lucy \"Not yet,\"\n"
        "    lucy \"The road is flooded.\"\n");
    CHECK(result.dialogue_lines == 3);
    CHECK(result.narration_lines == 1);
}

TEST_CASE("manuscript conversion handles speaker blocks headings and RenPy escaping") {
    ManuscriptOptions options;
    options.label.clear();
    options.include_character_definitions = false;
    const auto result = convert_manuscript_to_renpy(
        "Captain Vale\n"
        "“Use [route] \\\\ north.”\n"
        ":: The Crossing\n"
        "A-B: He said \"now\".\n"
        "A B: Done.\n", options);

    CHECK(result.script ==
        "    captain_vale \"Use [[route] \\\\\\\\ north.\"\n\n"
        "label the_crossing:\n"
        "    a_b \"He said \\\"now\\\".\"\n"
        "    a_b_2 \"Done.\"\n");
    REQUIRE(result.characters.size() == 3);
    CHECK(result.characters[0].alias == "captain_vale");
    CHECK(result.characters[2].alias == "a_b_2");
}

TEST_CASE("manuscript conversion normalizes a requested label") {
    ManuscriptOptions options;
    options.label = "Chapter 2: Home";
    const auto result = convert_manuscript_to_renpy("A quiet room.", options);
    CHECK(result.script == "label chapter_2_home:\n    \"A quiet room.\"\n");
}

TEST_CASE("fully quoted narration with a colon is not treated as dialogue") {
    const auto result = convert_manuscript_to_renpy("“The clock read: midnight.”");
    CHECK(result.script == "label start:\n    \"The clock read: midnight.\"\n");
    CHECK(result.dialogue_lines == 0);
    CHECK(result.narration_lines == 1);
    CHECK(result.characters.empty());
}
