#include "core/parser.h"

#include <catch2/catch_test_macros.hpp>

TEST_CASE("parser counts dialogue and narration but ignores code") {
    const auto result = say_count::analyze_script(
        "label start:\n    e \"Hello there.\"\n    \"Narration counts.\"\n    scene bg room\n    $ x = \"nope\"\n    jump end");
    CHECK(result.total_words == 4);
    REQUIRE(result.counted.size() == 2);
    CHECK(result.counted[0].speaker == "e");
    CHECK(result.counted[0].scene == "start");
    CHECK(result.counted[0].line_number == 2);
    CHECK(result.counted[1].speaker == "narrator");
}

TEST_CASE("extend inherits the previous speaker") {
    const auto result = say_count::analyze_script("e \"One.\"\nextend \"Two more.\"");
    REQUIRE(result.counted.size() == 2);
    CHECK(result.counted[1].speaker == "e");
    CHECK(result.total_words == 3);
}

TEST_CASE("word cleaning matches simple JavaScript fixtures") {
    CHECK(say_count::count_words("don't stop") == 2);
    CHECK(say_count::count_words("well-known fact") == 2);
    CHECK(say_count::clean_renpy_text("{b}bold{/b}") == "bold");
    CHECK(say_count::count_words("Hi [name] there") == 3);
    CHECK(say_count::count_words("café 東京 １２") == 3);
    CHECK(say_count::count_words("emoji 😀 does not count") == 4);
    CHECK(say_count::count_words("rock’n’roll") == 1);
}

TEST_CASE("character definitions resolve names and colors") {
    const auto result = say_count::analyze_script(
        "define e = Character(\"Eileen\", color=\"#c8ffc8\")\n"
        "m = renpy.Character('Mara', color='#ABC')\n"
        "default n = Character(\"None\", color=\"#12345678\")\n"
        "e \"Hello.\"\nm \"Hi.\"\nq \"Unknown.\"");
    REQUIRE(result.counted.size() == 3);
    CHECK(result.counted[0].speaker == "Eileen");
    CHECK(result.counted[1].speaker == "Mara");
    CHECK(result.counted[2].speaker == "q");
    CHECK(result.character_names.at("e") == "Eileen");
    CHECK(result.character_names.at("m") == "Mara");
    CHECK(result.character_names.count("n") == 0);
    CHECK(result.speaker_colors.at("e") == "#c8ffc8");
    CHECK(result.speaker_colors.at("Eileen") == "#c8ffc8");
    CHECK(result.speaker_colors.at("None") == "#12345678");
}

TEST_CASE("project character definitions resolve across files") {
    const auto result = say_count::analyze_project({
        {"defs.rpy", "define e = Character(\"Eileen\", color=\"#c8ffc8\")"},
        {"scene.rpy", "e \"Hello.\""},
    });
    REQUIRE(result.counted.size() == 1);
    CHECK(result.counted[0].speaker == "Eileen");
}

TEST_CASE("later project character definitions win in file order") {
    const auto result = say_count::analyze_project({
        {"first.rpy", "define e = Character(\"Eileen\", color=\"#111\")"},
        {"second.rpy", "define e = Character(\"Eve\", color=\"#222\")\ne \"Hello.\""},
    });
    REQUIRE(result.counted.size() == 1);
    CHECK(result.counted[0].speaker == "Eve");
    CHECK(result.character_names.at("e") == "Eve");
    CHECK(result.speaker_colors.at("e") == "#222");
    CHECK(result.speaker_colors.at("Eileen") == "#111");
    CHECK(result.speaker_colors.at("Eve") == "#222");
}
