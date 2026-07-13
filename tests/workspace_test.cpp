#include <catch2/catch_test_macros.hpp>

#include "core/workspace.h"

#include <filesystem>

TEST_CASE("workspace state round trips paths positions and perspective") {
    const auto path = (std::filesystem::temp_directory_path() / "say-count-workspace-test.dat").string();
    say_count::WorkspaceStore store(path);
    say_count::WorkspaceState expected{"/games/demo", {{"/games/demo/game/script.rpy", 42, 7}},
                                       "/games/demo/game/script.rpy", "layout|with\nbytes"};
    REQUIRE(store.Save(expected));
    const auto loaded = store.Load();
    REQUIRE(loaded);
    CHECK(loaded->project_root == expected.project_root);
    CHECK(loaded->active_file == expected.active_file);
    CHECK(loaded->perspective == expected.perspective);
    REQUIRE(loaded->files.size() == 1);
    CHECK(loaded->files[0].path == expected.files[0].path);
    CHECK(loaded->files[0].caret == 42);
    CHECK(loaded->files[0].first_visible_line == 7);
    std::filesystem::remove(path);
}

TEST_CASE("workspace store rejects malformed input") {
    const auto path = (std::filesystem::temp_directory_path() / "say-count-workspace-missing.dat").string();
    std::filesystem::remove(path);
    CHECK_FALSE(say_count::WorkspaceStore(path).Load());
}
