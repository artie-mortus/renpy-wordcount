#include <catch2/catch_test_macros.hpp>

#include "core/prose.h"

using namespace say_count;

TEST_CASE("prose analysis reports vocabulary sentences speakers and repetition") {
    const std::string script =
        "define e = Character(\"Eileen\")\nlabel start:\n"
        "    e \"Really bright stars shine. Really bright stars glow.\"\n"
        "    e \"Really bright stars dance! Really bright stars remain.\"\n"
        "    \"A short sentence.\"";
    const auto prose = analyze_prose(analyze_script(script));

    CHECK(prose.total_words == 19);
    CHECK(prose.unique_words >= 9);
    REQUIRE_FALSE(prose.overused_words.empty());
    CHECK(prose.overused_words.front().text == "really");
    CHECK(prose.overused_words.front().count == 4);
    REQUIRE_FALSE(prose.repeated_phrases.empty());
    CHECK(prose.repeated_phrases.front().text == "really bright stars");
    REQUIRE(prose.speakers.size() == 2);
    CHECK(prose.speakers[0].speaker == "Eileen");
    CHECK(prose.speakers[0].sentences == 4);
    CHECK(prose.speakers[0].longest_sentence == 4);
    CHECK(prose.speakers[1].speaker == "narrator");
}

TEST_CASE("prose ignores menu choices names stopwords and custom exclusions") {
    const auto analysis = analyze_script(
        "define e = Character(\"Eileen\")\nlabel start:\n"
        "    e \"Eileen watches quietly quietly quietly quietly.\"\n"
        "    menu:\n        \"Quietly quietly quietly quietly\":\n            pass",
        {true});
    const auto prose = analyze_prose(analysis, {"quietly"});
    CHECK(prose.total_words == 6);
    CHECK(prose.overused_words.empty());
}

TEST_CASE("word tokenization preserves Unicode connectors") {
    const auto tokens = word_tokens("Café déjà-vu l’esprit 42 😀");
    REQUIRE(tokens.size() == 4);
    CHECK(tokens[0] == "Café");
    CHECK(tokens[1] == "déjà-vu");
    CHECK(tokens[2] == "l’esprit");
    CHECK(tokens[3] == "42");
}
