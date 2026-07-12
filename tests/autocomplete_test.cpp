#include <catch2/catch_test_macros.hpp>
#include "core/autocomplete.h"

TEST_CASE("basic autocomplete is limited to RenPy-aware contexts") {
    const std::string source = "define eileen = Character(\"Eileen\")\nlabel start:\nlabel beach:\n    jump be";
    auto result = say_count::basic_completions(source, source.size());
    REQUIRE(result.kind == say_count::CompletionKind::Label);
    REQUIRE(result.replace_length == 2);
    REQUIRE(result.items.size() == 1); REQUIRE(result.items[0].text == "beach");
    const std::string alias_source = "define eileen = Character(\"Eileen\")\n    eile";
    const auto alias = say_count::basic_completions(alias_source, alias_source.size());
    REQUIRE(alias.items.size() == 1); REQUIRE(alias.items[0].insert == "eileen ");
    REQUIRE(say_count::basic_completions("e = eile", 8).items.empty());
    REQUIRE(say_count::basic_completions("\"eile", 5).items.empty());
}

TEST_CASE("basic autocomplete supplies snippets and placeholder ranges") {
    auto label = say_count::basic_completions("    lab", 7);
    REQUIRE(label.items[0].text == "label");
    REQUIRE(label.items[0].insert == "label name:\n        ");
    REQUIRE(label.items[0].insert.substr(label.items[0].select_start,
                                         label.items[0].select_end - label.items[0].select_start) == "name");
    auto menu = say_count::basic_completions("    men", 7);
    REQUIRE(menu.items[0].text == "menu");
    REQUIRE(menu.items[0].insert.find("\n        \"Choice.\":\n            ") != std::string::npos);
}
