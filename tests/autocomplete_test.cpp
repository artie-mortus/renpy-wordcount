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

TEST_CASE("project autocomplete indexes RenPy assets and declarations across files") {
    const std::vector<say_count::NamedScript> scripts{
        {"definitions.rpy",
         "image eileen happy = \"eileen_happy.png\"\n"
         "image bg beach day = \"beach.png\" # a comment\n"
         "screen preferences_panel():\n"
         "default affection = 0\n"
         "define persistent_name = \"Eileen\"\n"
         "define e = Character(\"Eileen\")\n"
         "define audio.theme = \"audio/theme.ogg\"\n"},
        {"scene.rpy",
         "label beach:\n"
         "    $ visited_beach = True\n"
         "    play music \"audio/shore.ogg\"\n"}
    };
    const auto index = say_count::build_completion_index(scripts);
    REQUIRE(index.images == std::vector<std::string>{"bg beach day", "eileen happy"});
    REQUIRE(index.screens == std::vector<std::string>{"preferences_panel"});
    REQUIRE(index.variables == std::vector<std::string>{"affection", "persistent_name", "visited_beach"});
    REQUIRE(index.audio == std::vector<std::string>{"audio/shore.ogg", "theme"});
    REQUIRE(index.labels == std::vector<std::string>{"beach"});
    REQUIRE(index.character_names.count("e") == 1);
}

TEST_CASE("project autocomplete offers assets only in their RenPy contexts") {
    const std::vector<say_count::NamedScript> scripts{{
        "definitions.rpy",
        "image eileen happy = \"eileen.png\"\n"
        "screen preferences_panel():\n"
        "default affection = 0\n"
        "define audio.theme = \"theme.ogg\"\n"
    }};
    const auto index = say_count::build_completion_index(scripts);
    const auto complete = [&](const std::string& source) {
        return say_count::project_completions(source, source.size(), index);
    };

    auto image = complete("    show eile");
    REQUIRE(image.kind == say_count::CompletionKind::Image);
    REQUIRE(image.replace_length == 4);
    REQUIRE(image.items[0].text == "eileen happy");

    auto audio = complete("    play music \"th");
    REQUIRE(audio.kind == say_count::CompletionKind::Audio);
    REQUIRE(audio.items[0].insert == "theme\"");

    auto screen = complete("    call screen pref");
    REQUIRE(screen.kind == say_count::CompletionKind::Screen);
    REQUIRE(screen.items[0].text == "preferences_panel");

    auto variable = complete("    $ aff");
    REQUIRE(variable.kind == say_count::CompletionKind::Variable);
    REQUIRE(variable.items[0].text == "affection");

    REQUIRE(complete("    show = eile").items.empty());
    REQUIRE(complete("    play music theme").items.empty());
    REQUIRE(complete("    call pref").items.empty());
    REQUIRE(complete("    $ total = aff").items.empty());
}

TEST_CASE("rebuilding the completion index reflects project edits") {
    std::vector<say_count::NamedScript> scripts{{"script.rpy", "image old pose = \"old.png\"\n"}};
    auto index = say_count::build_completion_index(scripts);
    REQUIRE(index.images == std::vector<std::string>{"old pose"});

    scripts[0].content = "image new pose = \"new.png\"\n";
    index = say_count::build_completion_index(scripts);
    REQUIRE(index.images == std::vector<std::string>{"new pose"});
}

TEST_CASE("per-document completion indexes merge and remove stale symbols") {
    auto first = say_count::build_completion_index(say_count::NamedScript{
        "first.rpy", "define e = Character(\"Early Eileen\")\nlabel first:\nimage shared = \"a.png\"\n"});
    auto second = say_count::build_completion_index(say_count::NamedScript{
        "second.rpy", "define e = Character(\"Later Eileen\")\nlabel second:\ndefault score = 0\n"});
    auto merged = say_count::merge_completion_indexes({first, second});
    CHECK(merged.character_names.at("e") == "Later Eileen");
    CHECK(merged.labels == std::vector<std::string>{"first", "second"});
    CHECK(merged.images == std::vector<std::string>{"shared"});
    CHECK(merged.variables == std::vector<std::string>{"score"});

    first = say_count::build_completion_index(say_count::NamedScript{"first.rpy", "label replacement:\n"});
    merged = say_count::merge_completion_indexes({first, second});
    CHECK(merged.labels == std::vector<std::string>{"replacement", "second"});
    CHECK(merged.images.empty());
}
