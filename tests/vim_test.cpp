#include <catch2/catch_test_macros.hpp>

#include "core/vim.h"

#include <string>
#include <string_view>
#include <utility>

namespace {

say_count::VimState Press(say_count::VimEmulator& vim,
                          say_count::VimState state,
                          std::string_view key) {
    return vim.ApplyKey(state.text, state.caret, key);
}

say_count::VimState Type(say_count::VimEmulator& vim,
                         say_count::VimState state,
                         std::string_view keys) {
    for (const char key : keys) state = Press(vim, std::move(state), std::string(1, key));
    return state;
}

}  // namespace

TEST_CASE("built-in Vim switches modes and exits insert mode with Escape") {
    say_count::VimEmulator vim;
    auto state = vim.ApplyKey("one two three", 0, "i");
    REQUIRE(state.mode == "i");

    state = vim.ApplyKey("xone two three", 1, "<Esc>");
    CHECK(state.mode == "n");
    CHECK(state.caret == 0);
    CHECK(state.text == "xone two three");

    state = Press(vim, std::move(state), "w");
    CHECK(state.caret == 5);
}

TEST_CASE("built-in Vim supports counts operators registers and undo delegation") {
    say_count::VimEmulator vim;
    auto state = vim.ApplyKey("one two three four", 0, "2");
    state = Press(vim, std::move(state), "w");
    CHECK(state.caret == 8);

    state = Press(vim, std::move(state), "d");
    CHECK(state.mode == "no");
    state = Press(vim, std::move(state), "w");
    CHECK(state.text == "one two four");

    state = Press(vim, std::move(state), "P");
    CHECK(state.text == "one two three four");

    state = Press(vim, std::move(state), "u");
    CHECK(state.host_undo);
    state = Press(vim, std::move(state), "<C-r>");
    CHECK(state.host_redo);
}

TEST_CASE("built-in Vim supports visual editing search and Ex commands") {
    say_count::VimEmulator vim;
    auto state = vim.ApplyKey("alpha beta alpha", 0, "v");
    REQUIRE(state.visual);
    state = Press(vim, std::move(state), "e");
    CHECK(state.selection_start == 0);
    CHECK(state.selection_end == 5);
    state = Press(vim, std::move(state), "y");
    CHECK(state.mode == "n");

    state = Press(vim, std::move(state), "/");
    state = Type(vim, std::move(state), "beta");
    state = Press(vim, std::move(state), "<CR>");
    CHECK(state.caret == 6);
    CHECK(state.search_match);
    CHECK(state.search_match_start == 6);
    CHECK(state.search_match_end == 10);
    CHECK(state.command_line == "/beta");

    state = Press(vim, std::move(state), ":");
    state = Type(vim, std::move(state), "%s/alpha/omega/g");
    state = Press(vim, std::move(state), "<CR>");
    CHECK(state.text == "omega beta omega");

    state = Press(vim, std::move(state), ":");
    state = Press(vim, std::move(state), "w");
    state = Press(vim, std::move(state), "<CR>");
    CHECK(state.host_save);

    state = Press(vim, std::move(state), "/");
    state = Type(vim, std::move(state), "missing");
    state = Press(vim, std::move(state), "<CR>");
    CHECK(state.command_line == "Pattern not found: missing");

    vim.Reset();
    state = vim.ApplyKey("Iconic lines", 0, "/");
    state = Type(vim, std::move(state), "iconic");
    state = Press(vim, std::move(state), "<CR>");
    CHECK(state.search_match);
    CHECK(state.search_match_start == 0);

    state = Press(vim, std::move(state), "/");
    state = Type(vim, std::move(state), "ICONIC");
    state = Press(vim, std::move(state), "<CR>");
    CHECK(state.command_line == "Pattern not found: ICONIC");
}

TEST_CASE("built-in Vim supports line motions find replace and indentation") {
    say_count::VimEmulator vim;
    auto state = vim.ApplyKey("zero\none\n(two)\nthree", 0, "j");
    CHECK(state.caret == 5);
    state = Press(vim, std::move(state), "d");
    state = Press(vim, std::move(state), "d");
    CHECK(state.text == "zero\n(two)\nthree");

    state = Press(vim, std::move(state), "g");
    state = Press(vim, std::move(state), "g");
    CHECK(state.caret == 0);
    state = Press(vim, std::move(state), "G");
    CHECK(state.caret == 11);

    state = vim.ApplyKey("abc def", 0, "f");
    state = Press(vim, std::move(state), "d");
    CHECK(state.caret == 4);
    state = Press(vim, std::move(state), "r");
    state = Press(vim, std::move(state), "X");
    CHECK(state.text == "abc Xef");

    vim.Reset();
    state = vim.ApplyKey("one\ntwo", 0, ">");
    state = Press(vim, std::move(state), ">");
    CHECK(state.text == "    one\ntwo");
}

TEST_CASE("built-in Vim handles line-heavy documents without transport") {
    std::string source;
    for (int line = 0; line < 2000; ++line) {
        if (line) source.push_back('\n');
        source += "screen text and properties repeated across this line";
    }

    say_count::VimEmulator vim;
    const std::size_t middle = source.size() / 2;
    auto state = vim.ApplyKey(source, middle, "i");
    REQUIRE(state.mode == "i");
    source.insert(middle, 1, 'x');
    state = vim.ApplyKey(source, middle + 1, "<Esc>");
    CHECK(state.mode == "n");
    CHECK(state.text == source);
}
