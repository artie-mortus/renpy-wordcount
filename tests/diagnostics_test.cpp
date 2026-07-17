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

TEST_CASE("basic diagnostics can be fixed across a project in one pass") {
    const std::vector<say_count::NamedScript> scripts{
        {"definitions.rpy", "define e = Character(\"Eileen\")\n"},
        {"story.rpy",
         "label start:\n"
         "  e \"Hello there\n"
         "\n"
         "label choice:\n"
         "    if ready  # continue\n"
         "        e \"Ready.\"\n"
         "\n"
         "label empty:\n"
         "label ending:\n"
         "    return\n"}
    };

    const auto before = say_count::diagnose_project(scripts);
    CHECK(say_count::count_basic_fixes(before) == 4);
    const auto fixed = say_count::fix_basic_diagnostics(scripts);
    CHECK(fixed.changed_files == 1);
    CHECK(fixed.indentation_fixes == 1);
    CHECK(fixed.quote_fixes == 1);
    CHECK(fixed.colon_fixes == 1);
    CHECK(fixed.empty_block_fixes == 1);
    CHECK(fixed.total_fixes() == 4);
    CHECK(fixed.scripts[1].content ==
          "label start:\n"
          "    e \"Hello there\"\n"
          "\n"
          "label choice:\n"
          "    if ready:  # continue\n"
          "        e \"Ready.\"\n"
          "\n"
          "label empty:\n"
          "    pass\n"
          "label ending:\n"
          "    return\n");
    CHECK(say_count::diagnose_project(fixed.scripts).empty());
}

TEST_CASE("basic fixes close multiline monologues and preserve CRLF") {
    const std::vector<say_count::NamedScript> scripts{
        {"definitions.rpy", "define e = Character(\"Eileen\")\r\n"},
        {"story.rpy", "label start:\r\n    e \"\"\"\r\n    Still speaking.\r\n"}
    };
    const auto fixed = say_count::fix_basic_diagnostics(scripts);
    CHECK(fixed.quote_fixes == 1);
    CHECK(fixed.scripts[1].content ==
          "label start:\r\n    e \"\"\"\r\n    Still speaking.\r\n    \"\"\"\r\n");
    CHECK(say_count::diagnose_project(fixed.scripts).empty());
}

TEST_CASE("basic fixes leave ambiguous diagnostics alone") {
    const std::vector<say_count::NamedScript> scripts{
        {"story.rpy", "label start:\n    q \"Hello.\"\n    jump missing\n"}
    };
    const auto diagnostics = say_count::diagnose_project(scripts);
    CHECK(say_count::count_basic_fixes(diagnostics) == 0);
    const auto fixed = say_count::fix_basic_diagnostics(scripts);
    CHECK(fixed.total_fixes() == 0);
    REQUIRE(fixed.scripts.size() == scripts.size());
    CHECK(fixed.scripts[0].name == scripts[0].name);
    CHECK(fixed.scripts[0].content == scripts[0].content);
}
