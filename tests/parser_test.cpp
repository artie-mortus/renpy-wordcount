#include "core/parser.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>

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

TEST_CASE("menu choices can be ignored or counted") {
    const std::string script =
        "label start:\n    menu:\n        \"Choice text\":\n            \"Narration.\"\n"
        "        \"Conditioned choice\" if flag:\n            e \"Dialogue remains.\"";
    const auto ignored = say_count::analyze_script(script);
    CHECK(ignored.total_words == 3);
    CHECK(ignored.counted.size() == 2);

    const auto counted = say_count::analyze_script(script, say_count::AnalysisOptions{true});
    CHECK(counted.total_words == 7);
    REQUIRE(counted.counted.size() == 4);
    CHECK(counted.counted[0].speaker == "menu choice");
    CHECK(counted.counted[0].scene == "start");
    CHECK(counted.counted[0].line_number == 3);
    CHECK(counted.counted[0].is_menu_choice);
    CHECK(counted.counted[2].speaker == "menu choice");
    CHECK(counted.counted[2].is_menu_choice);
}

TEST_CASE("parser reports quote speaker and long-line warnings") {
    const auto quoted = say_count::analyze_script("e \"unclosed");
    CHECK(quoted.warnings.size() == 1);
    CHECK(quoted.warnings[0].type == "quote");

    const auto unknown = say_count::analyze_script("q \"Hi.\"");
    REQUIRE(unknown.warnings.size() == 1);
    CHECK(unknown.warnings[0].type == "speaker");
    CHECK(unknown.counted[0].unknown_speaker);

    std::string long_line = "\"";
    for (int i = 0; i < 36; ++i) long_line += "word" + std::to_string(i) + (i == 35 ? "\"" : " ");
    const auto lengthy = say_count::analyze_script(long_line);
    REQUIRE(lengthy.warnings.size() == 1);
    CHECK(lengthy.warnings[0].type == "length");
    CHECK(say_count::analyze_script("").script_lines == 0);
    CHECK(say_count::analyze_script("jump").warnings.empty());

    const auto partial = say_count::analyze_script("e \"Hello\" \"unfinished");
    CHECK(partial.total_words == 1);
    CHECK(std::any_of(partial.warnings.begin(), partial.warnings.end(),
                      [](const auto& warning) { return warning.type == "quote"; }));
}

TEST_CASE("parser ignores indented RenPy implementation blocks") {
    CHECK(say_count::analyze_script("init python:\n    greeting = \"nope\"\n    \"also nope\"").total_words == 0);
    const auto dedent = say_count::analyze_script(
        "init python:\n    x = \"nope\"\nlabel start:\n    e \"One two.\"");
    CHECK(dedent.total_words == 2);
    CHECK(dedent.counted[0].scene == "start");
    CHECK(say_count::analyze_script("screen hud():\n    text \"Score display here\"").total_words == 0);
    CHECK(say_count::analyze_script("style say_label:\n    color \"#ffffff\"").total_words == 0);
    CHECK(say_count::analyze_script("transform slidein:\n    xalign 0.0\n    linear 1.0 xalign 1.0").total_words == 0);
    CHECK(say_count::analyze_script("python:\n    if enabled:\n        text = \"deep string should not count\"").total_words == 0);
}

TEST_CASE("parser counts RenPy triple-quoted monologues") {
    const auto speaker = say_count::analyze_script("e \"\"\"\n    One two three.\n\n    Four five.\n    \"\"\"");
    CHECK(speaker.total_words == 5);
    REQUIRE(speaker.counted.size() == 2);
    CHECK(speaker.counted[0].speaker == "e");
    CHECK(speaker.counted[1].speaker == "e");
    CHECK(std::none_of(speaker.warnings.begin(), speaker.warnings.end(),
                       [](const auto& warning) { return warning.type == "quote"; }));

    const auto narrator = say_count::analyze_script("\"\"\"\n    Just narration here.\n    \"\"\"");
    REQUIRE(narrator.counted.size() == 1);
    CHECK(narrator.counted[0].speaker == "narrator");
    CHECK(narrator.total_words == 3);
    CHECK(say_count::analyze_script("e \"\"\"Hi there.\"\"\"").total_words == 2);

    const auto unclosed = say_count::analyze_script("e \"\"\"\n    Dangling text.");
    CHECK(unclosed.total_words == 2);
    REQUIRE(unclosed.warnings.size() == 2); // unknown speaker plus unclosed quote
    CHECK(unclosed.warnings[0].type == "quote");
    CHECK(say_count::analyze_script("$ doc = \"\"\"nope\"\"\"").total_words == 0);

    const auto extended = say_count::analyze_script("e \"\"\"\n    One two.\n    \"\"\"\nextend \" three four.\"");
    CHECK(extended.total_words == 4);
    REQUIRE(extended.counted.size() == 2);
    CHECK(extended.counted[1].speaker == "e");
}
