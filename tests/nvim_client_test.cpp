#include <catch2/catch_test_macros.hpp>

#include "app/nvim_client.h"

#include <wx/init.h>

TEST_CASE("embedded Neovim supplies motions operators and visual selections") {
    wxInitializer initializer;
    REQUIRE(initializer.IsOk());
    say_count::app::NvimClient client;
    std::string error;
    INFO(error);
    REQUIRE(client.Start(&error));
    const auto buffer = client.CreateBuffer("one two three\nlast line", "motion-test.rpy", &error);
    INFO(error);
    REQUIRE(buffer);

    auto unchanged = client.ApplyKey(*buffer, "one two three\nlast line", 0, "u", &error);
    REQUIRE(unchanged);
    CHECK(unchanged->text == "one two three\nlast line");

    auto state = client.ApplyKey(*buffer, "one two three\nlast line", 0, "w", &error);
    REQUIRE(state);
    CHECK(state->caret == 4);
    CHECK(state->mode == "n");

    state = client.ApplyKey(*buffer, state->text, state->caret, "d", &error);
    REQUIRE(state);
    state = client.ApplyKey(*buffer, state->text, state->caret, "w", &error);
    REQUIRE(state);
    CHECK(state->text == "one three\nlast line");

    state = client.ApplyKey(*buffer, state->text, state->caret, "v", &error);
    REQUIRE(state);
    REQUIRE(state->visual);
    state = client.ApplyKey(*buffer, state->text, state->caret, "e", &error);
    REQUIRE(state);
    CHECK(state->visual);
    CHECK(state->selection_end > state->selection_start);

    const auto command_buffer = client.CreateBuffer(
        "alpha beta alpha", "command-test.rpy", &error);
    REQUIRE(command_buffer);
    auto command_state = client.ApplyKey(
        *command_buffer, "alpha beta alpha", 0, ":%s/alpha/omega/g<CR>", &error);
    REQUIRE(command_state);
    CHECK(command_state->text == "omega beta omega");

    const auto count_buffer = client.CreateBuffer(
        "one two three four", "count-test.rpy", &error);
    REQUIRE(count_buffer);
    auto count_state = client.ApplyKey(
        *count_buffer, "one two three four", 0, "2dw", &error);
    REQUIRE(count_state);
    CHECK(count_state->text == "three four");

    std::string large_source;
    for (int line = 0; line < 2000; ++line) {
        if (line) large_source.push_back('\n');
        large_source += "screen text and properties repeated across this line";
    }
    const auto large_buffer = client.CreateBuffer(large_source, "large-test.rpy", &error);
    INFO(error);
    REQUIRE(large_buffer);
    const std::size_t edit_offset = large_source.size() / 2;
    auto insert_state = client.ApplyKey(*large_buffer, large_source, edit_offset, "i", &error);
    REQUIRE(insert_state);
    std::string edited = insert_state->text;
    edited.insert(edit_offset, 1, 'x');
    auto escaped_state = client.ApplyKey(*large_buffer, edited, edit_offset + 1, "<Esc>", &error);
    REQUIRE(escaped_state);
    CHECK(escaped_state->mode == "n");
    CHECK(escaped_state->text.size() == large_source.size() + 1);
}
