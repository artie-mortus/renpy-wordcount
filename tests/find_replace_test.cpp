#include <catch2/catch_test_macros.hpp>

#include "core/find_replace.h"

TEST_CASE("literal find supports case whole-word and non-overlapping matches") {
    auto insensitive = say_count::find_matches("Eileen eileen", "eileen");
    REQUIRE(insensitive.matches.size() == 2);
    REQUIRE(insensitive.matches[0].position == 0);
    REQUIRE(insensitive.matches[1].position == 7);

    say_count::FindOptions sensitive; sensitive.case_sensitive = true;
    REQUIRE(say_count::find_matches("Eileen eileen", "eileen", sensitive).matches.size() == 1);

    say_count::FindOptions words; words.whole_word = true;
    const auto whole = say_count::find_matches("cat cat_name bobcat cat.", "cat", words);
    REQUIRE(whole.matches.size() == 2);
    REQUIRE(whole.matches[1].position == 20);
    REQUIRE(say_count::find_matches("aaaa", "aa", sensitive).matches.size() == 2);
}

TEST_CASE("regex find supports captures whole words invalid patterns and zero matches") {
    say_count::FindOptions regex; regex.case_sensitive = true; regex.use_regex = true;
    auto found = say_count::find_matches("e \"Hi\"\nm \"Bye\"", "([em])\\s+\"", regex);
    REQUIRE(found.valid);
    REQUIRE(found.matches.size() == 2);
    REQUIRE(found.matches[0].length == 3);

    regex.whole_word = true;
    REQUIRE(say_count::find_matches("end ending end", "end", regex).matches.size() == 2);
    REQUIRE_FALSE(say_count::find_matches("text", "[", regex).valid);
    REQUIRE(say_count::find_matches("text", "absent", regex).matches.empty());
    REQUIRE(say_count::find_matches("text", "", regex).matches.empty());
}

TEST_CASE("literal replacement keeps dollar syntax literal") {
    const auto result = say_count::replace_all_matches("one ONE", "one", "$1");
    REQUIRE(result.valid);
    REQUIRE(result.count == 2);
    REQUIRE(result.text == "$1 $1");
}

TEST_CASE("regex replacement expands capture groups") {
    say_count::FindOptions regex; regex.case_sensitive = true; regex.use_regex = true;
    const auto result = say_count::replace_all_matches(
        "e \"Hi\"\nm \"Bye\"", "([em])\\s+\"([^\"]+)\"", "$2 ($1)", regex);
    REQUIRE(result.valid);
    REQUIRE(result.count == 2);
    REQUIRE(result.text == "Hi (e)\nBye (m)");
}

TEST_CASE("whole-word replacement and zero-match replacement preserve other text") {
    say_count::FindOptions words; words.whole_word = true;
    const auto changed = say_count::replace_all_matches("route route_old reroute", "route", "path", words);
    REQUIRE(changed.count == 1);
    REQUIRE(changed.text == "path route_old reroute");
    const auto unchanged = say_count::replace_all_matches("text", "missing", "x");
    REQUIRE(unchanged.count == 0);
    REQUIRE(unchanged.text == "text");
}

TEST_CASE("project find reports file line column and preview") {
    const std::vector<say_count::NamedScript> scripts{
        {"one.rpy", "label start:\n    e \"Hello there\"\n"},
        {"two.rpy", "# Hello comment\nlabel next:\n    \"hello again\"\n"}
    };
    const auto found = say_count::find_project_matches(scripts, "hello");
    REQUIRE(found.valid);
    REQUIRE(found.matches.size() == 3);
    REQUIRE(found.matches[0].file_name == "one.rpy");
    REQUIRE(found.matches[0].line_number == 2);
    REQUIRE(found.matches[0].column == 8);
    REQUIRE(found.matches[0].preview == "e \"Hello there\"");
    REQUIRE(found.matches[2].file_name == "two.rpy");
    REQUIRE(found.matches[2].line_number == 3);
}

TEST_CASE("project replacement changes only selected files") {
    const std::vector<say_count::NamedScript> scripts{
        {"one.rpy", "route route\n"},
        {"two.rpy", "route\n"},
        {"three.rpy", "route\n"}
    };
    const auto preview = say_count::preview_project_replacement(scripts, "route");
    REQUIRE(preview.size() == 3);
    REQUIRE(preview[0].count == 2);

    const auto changed = say_count::replace_project_matches(scripts, {0, 2}, "route", "path");
    REQUIRE(changed.valid);
    REQUIRE(changed.count == 3);
    REQUIRE(changed.scripts[0].content == "path path\n");
    REQUIRE(changed.scripts[1].content == scripts[1].content);
    REQUIRE(changed.scripts[2].content == "path\n");
    REQUIRE(changed.per_file_counts == std::vector<std::size_t>{2, 0, 1});
}
