#include "core/parser.h"

#include <catch2/catch_test_macros.hpp>

#include <string>

namespace {
const say_count::AggregateStats* find(const std::vector<say_count::AggregateStats>& values,
                                      const std::string& name) {
    for (const auto& value : values) if (value.name == name) return &value;
    return nullptr;
}
}

TEST_CASE("analysis aggregates empty and narration-only scripts") {
    const auto empty = say_count::analyze_script("");
    CHECK(empty.total_words == 0);
    CHECK(empty.average_words == 0);
    CHECK(empty.reading_minutes == 0);
    CHECK(empty.speakers.empty());
    CHECK(empty.scenes.empty());

    const auto narration = say_count::analyze_script("\"One two three.\"");
    REQUIRE(find(narration.speakers, "narrator"));
    CHECK(find(narration.speakers, "narrator")->words == 3);
    CHECK(find(narration.speakers, "narrator")->lines == 1);
    CHECK(narration.speakers.size() == 1);
}

TEST_CASE("analysis averageWords serializes like JavaScript JSON.stringify") {
    // 11 words over 10 counted lines: 1.1 must print as "1.1", not 1.1000000000000001.
    std::string script = "e \"one two\"\n";
    for (int i = 0; i < 9; ++i) script += "e \"one\"\n";
    const auto result = say_count::analyze_script(script);
    REQUIRE(result.dialogue_lines == 10);
    CHECK(say_count::analysis_json(result).find("\"averageWords\":1.1,") != std::string::npos);
}

TEST_CASE("analysis aggregates speakers and labels in first-seen order") {
    const auto result = say_count::analyze_script(
        "define e = Character(\"Eileen\")\nlabel start:\ne \"One two.\"\n\"Three.\"\n"
        "label second:\nq \"Four five six.\"\ne \"Seven.\"");
    REQUIRE(result.speakers.size() == 3);
    CHECK(result.speakers[0].name == "Eileen");
    CHECK(result.speakers[0].words == 3);
    CHECK(result.speakers[0].lines == 2);
    CHECK(result.speakers[1].name == "narrator");
    CHECK(result.speakers[2].name == "q");
    REQUIRE(result.scenes.size() == 2);
    CHECK(result.scenes[0].name == "start");
    CHECK(result.scenes[0].words == 3);
    CHECK(result.scenes[1].name == "second");
    CHECK(result.scenes[1].words == 4);
}

TEST_CASE("analysis menu choices participate in non-narration aggregates only when enabled") {
    const std::string script = "label start:\nmenu:\n    \"Choose these words\":\n        \"Narrated response.\"";
    const auto ignored = say_count::analyze_script(script);
    CHECK_FALSE(find(ignored.speakers, "menu choice"));
    REQUIRE(find(ignored.speakers, "narrator"));
    CHECK(find(ignored.speakers, "narrator")->words == 2);
    const auto counted = say_count::analyze_script(script, say_count::AnalysisOptions{true});
    REQUIRE(find(counted.speakers, "menu choice"));
    CHECK(find(counted.speakers, "menu choice")->words == 3);
    CHECK(find(counted.speakers, "narrator")->words == 2);
}

TEST_CASE("analysis reading time uses JavaScript rounding at 200 words per minute") {
    auto script = [](std::size_t words) {
        std::string text = "\"";
        for (std::size_t i = 0; i < words; ++i) text += (i ? " word" : "word");
        return text + "\"";
    };
    CHECK(say_count::analyze_script(script(1)).reading_minutes == 1);
    CHECK(say_count::analyze_script(script(299)).reading_minutes == 1);
    CHECK(say_count::analyze_script(script(300)).reading_minutes == 2);
    CHECK(say_count::analyze_script(script(499)).reading_minutes == 2);
    CHECK(say_count::analyze_script(script(500)).reading_minutes == 3);
}
