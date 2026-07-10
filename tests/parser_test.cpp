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
