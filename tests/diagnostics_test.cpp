#include <catch2/catch_test_macros.hpp>

#include "core/diagnostics.h"

#include <algorithm>
#include <set>

TEST_CASE("diagnostics engine covers project-aware editor checks") {
    const std::vector<say_count::NamedScript> scripts{
        {"definitions.rpy", "define e = Character(\"Eileen\")\n"},
        {"broken.rpy",
         "label start:\n"
         "  e \"bad indent\"\n"
         "    q \"Unknown speaker\"\n"
         "    jump nowhere\n"
         "    if flag\n"
         "    e \"unfinished\n"
         "    \"one two three four\"\n"}
    };
    const auto diagnostics = say_count::diagnose_project(scripts, {3});
    std::set<std::string> types;
    for (const auto& diagnostic : diagnostics) types.insert(diagnostic.type);
    for (const auto* expected : {"missing-label", "quote", "speaker", "indentation", "length", "syntax"})
        REQUIRE(types.count(expected) == 1);

    const auto missing = std::find_if(diagnostics.begin(), diagnostics.end(), [](const auto& diagnostic) {
        return diagnostic.type == "missing-label";
    });
    REQUIRE(missing != diagnostics.end());
    REQUIRE(missing->severity == say_count::DiagnosticSeverity::Error);
    REQUIRE(missing->file == "broken.rpy");
    REQUIRE(missing->line_number == 4);
    REQUIRE(missing->length > 0);
}

TEST_CASE("valid project fixture has no diagnostics") {
    const std::vector<say_count::NamedScript> scripts{
        {"definitions.rpy", "define e = Character(\"Eileen\")\n"},
        {"story.rpy",
         "label start:\n"
         "    e \"A concise line.\"\n"
         "    jump ending\n"
         "\n"
         "label ending:\n"
         "    return\n"}
    };
    REQUIRE(say_count::diagnose_project(scripts).empty());
}

TEST_CASE("diagnostics refresh when the source is fixed") {
    std::vector<say_count::NamedScript> scripts{{"story.rpy", "label start:\n  q \"Unfinished\n"}};
    REQUIRE_FALSE(say_count::diagnose_project(scripts).empty());
    scripts[0].content = "define q = Character(\"Quinn\")\nlabel start:\n    q \"Finished.\"\n";
    REQUIRE(say_count::diagnose_project(scripts).empty());
}
