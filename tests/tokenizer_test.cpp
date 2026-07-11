#include "core/tokenizer.h"

#include <catch2/catch_test_macros.hpp>
#include <unordered_set>

using say_count::LineType;
using say_count::tokenize_line;
using say_count::HighlightClass;
using say_count::highlight_line;

TEST_CASE("tokenizer recognizes core RenPy line forms") {
    CHECK(tokenize_line("", 1).type == LineType::Blank);
    CHECK(tokenize_line("  # note", 2).type == LineType::Comment);
    CHECK(tokenize_line("label start:", 3).type == LineType::Label);
    CHECK(tokenize_line("e \"Hello\"", 4).type == LineType::Dialogue);
    CHECK(tokenize_line("\"Narration\"", 5).type == LineType::Narration);
    CHECK(tokenize_line("define e = Character(\"Eileen\")", 6).type == LineType::Define);
    CHECK(tokenize_line("jump ending", 7).type == LineType::Jump);
    CHECK(tokenize_line("call helper", 8).type == LineType::Call);
    CHECK(tokenize_line("menu:", 9).type == LineType::Menu);
    CHECK(tokenize_line("$ score += 1", 10).type == LineType::Python);
    CHECK(tokenize_line("python:", 11).type == LineType::Python);
    CHECK(tokenize_line("scene bg room", 12).type == LineType::Scene);
    CHECK(tokenize_line("show eileen happy", 13).type == LineType::Show);
    CHECK(tokenize_line("play music theme", 14).type == LineType::Play);
}

TEST_CASE("highlighter classifies reference syntax without wx dependencies") {
    const std::unordered_set<std::string> speakers{"e"};
    auto spans = highlight_line("e \"Hi # there\" # note", speakers);
    REQUIRE(spans.size() == 3);
    CHECK(spans[0].token_class == HighlightClass::Speaker);
    CHECK(spans[1].token_class == HighlightClass::String);
    CHECK(spans[2].token_class == HighlightClass::Comment);
    CHECK(spans[1].begin == 2);
    CHECK(spans[1].end == 14);

    spans = highlight_line("jump ending");
    REQUIRE(spans.size() == 2);
    CHECK(spans[0].token_class == HighlightClass::Keyword);
    CHECK(spans[1].token_class == HighlightClass::Statement);

    spans = highlight_line("$ score = 'ready' # code");
    REQUIRE(spans.size() == 2);
    CHECK(spans[0].token_class == HighlightClass::Python);
    CHECK(spans[1].token_class == HighlightClass::Comment);
}

TEST_CASE("highlighter keywords match the JS ignored starters case-insensitively") {
    for (const auto* line : {"image bg street", "hide eileen", "transform fade", "translate french start", "window show"}) {
        const auto spans = highlight_line(line);
        REQUIRE_FALSE(spans.empty());
        CHECK(spans[0].token_class == HighlightClass::Keyword);
    }
    CHECK(highlight_line("Label start:")[0].token_class == HighlightClass::Label);
    CHECK(highlight_line("Extend \"more\"")[0].token_class == HighlightClass::Keyword);
}

TEST_CASE("highlighter distinguishes labels, unknown aliases, and escaped strings") {
    auto spans = highlight_line("  label chapter_one:");
    REQUIRE(spans.size() == 1);
    CHECK(spans[0].token_class == HighlightClass::Label);
    CHECK(spans[0].begin == 2);

    CHECK(highlight_line("unknown \"Hi\"")[0].token_class == HighlightClass::String);
    spans = highlight_line("e \"She said \\\"hi\\\"\"", {"e"});
    REQUIRE(spans.size() == 2);
    CHECK(spans[1].token_class == HighlightClass::String);
}

TEST_CASE("tokenizer preserves escaped quotes and finds real comments") {
    const auto line = tokenize_line("e \"She said \\\"hi\\\" # aloud\" # note", 7);
    REQUIRE(line.type == LineType::Dialogue);
    REQUIRE(line.quoted.size() == 1);
    CHECK(line.quoted[0].text == "She said \\\"hi\\\" # aloud");
    CHECK(line.comment == "# note");
    CHECK(line.line_number == 7);
}

TEST_CASE("tokenizer records indentation with tabs as four spaces") {
    const auto line = tokenize_line("\t  e \"Indented\"", 23);
    CHECK(line.indentation == 6);
    CHECK(line.line_number == 23);
    CHECK(line.type == LineType::Dialogue);
}

TEST_CASE("tokenizer rejects malformed input without throwing") {
    CHECK(tokenize_line("e \"unclosed", 1).type == LineType::Malformed);
    CHECK(tokenize_line("label missing_colon", 2).type == LineType::Malformed);
    CHECK(tokenize_line("jump", 3).type == LineType::Malformed);
    CHECK(tokenize_line("call", 4).type == LineType::Malformed);
    CHECK(tokenize_line("menu", 5).type == LineType::Malformed);
    CHECK(tokenize_line("scene", 6).type == LineType::Malformed);
}
