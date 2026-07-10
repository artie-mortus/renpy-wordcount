#include "core/tokenizer.h"

#include <catch2/catch_test_macros.hpp>

using say_count::LineType;
using say_count::tokenize_line;

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
