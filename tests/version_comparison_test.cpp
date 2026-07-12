#include <catch2/catch_test_macros.hpp>
#include "core/version.h"

TEST_CASE("version comparison reports total and per-speaker changes") {
    const auto old_version = say_count::analyze_script("label start:\n    e \"one two three\"\n    l \"four five\"\n");
    const auto new_version = say_count::analyze_script("label start:\n    e \"one two three four five six\"\n    n \"new voice\"\n");
    const auto result = say_count::compare_versions(old_version, new_version);
    REQUIRE(result.before_words == 5); REQUIRE(result.after_words == 8);
    REQUIRE(result.added_words == 3); REQUIRE(result.removed_words == 0); REQUIRE(result.net_words == 3);
    REQUIRE(result.speakers.size() == 3);
    REQUIRE(result.speakers[0].name == "e"); REQUIRE(result.speakers[0].delta == 3);
    REQUIRE(result.speakers[1].name == "l"); REQUIRE(result.speakers[1].delta == -2);
    REQUIRE(result.speakers[2].name == "n"); REQUIRE(result.speakers[2].delta == 2);
}

TEST_CASE("version comparison reports removals") {
    const auto result = say_count::compare_versions(say_count::analyze_script("\"one two three\"\n"),
                                                     say_count::analyze_script("\"one\"\n"));
    REQUIRE(result.added_words == 0); REQUIRE(result.removed_words == 2); REQUIRE(result.net_words == -2);
}
