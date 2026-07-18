#include <catch2/catch_test_macros.hpp>

#include "app/nvim_client.h"

#include <wx/init.h>

TEST_CASE("embedded Neovim supplies motions operators commands and visual selections") {
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
    auto state = client.ApplyKey(*buffer, unchanged->text, 0, "w", &error);
    REQUIRE(state);
    CHECK(state->caret == 4);
    state = client.ApplyKey(*buffer, state->text, state->caret, "dw", &error);
    REQUIRE(state);
    CHECK(state->text == "one three\nlast line");
    state = client.ApplyKey(*buffer, state->text, state->caret, "v", &error);
    REQUIRE(state);
    REQUIRE(state->visual);
    state = client.ApplyKey(*buffer, state->text, state->caret, "e", &error);
    REQUIRE(state);
    CHECK(state->selection_end > state->selection_start);

    const auto command_buffer = client.CreateBuffer("alpha beta alpha", "command-test.rpy", &error);
    REQUIRE(command_buffer);
    auto command = client.ApplyKey(*command_buffer, "alpha beta alpha", 0,
                                   ":%s/alpha/omega/g<CR>", &error);
    REQUIRE(command);
    CHECK(command->text == "omega beta omega");

    const auto large_buffer = client.CreateBuffer(std::string(100000, 'a'), "large-test.rpy", &error);
    INFO(error);
    REQUIRE(large_buffer);
    auto insert = client.ApplyKey(*large_buffer, std::string(100000, 'a'), 0, "i", &error);
    REQUIRE(insert);
    std::string edited = insert->text;
    edited.insert(edited.begin(), 'x');
    auto escaped = client.ApplyKey(*large_buffer, edited, 1, "<Esc>", &error);
    REQUIRE(escaped);
    CHECK(escaped->mode == "n");
    CHECK(escaped->text.size() == 100001);
}
