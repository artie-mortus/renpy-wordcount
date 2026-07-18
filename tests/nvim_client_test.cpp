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
}
