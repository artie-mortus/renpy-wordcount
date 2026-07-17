#include <catch2/catch_test_macros.hpp>

#include "core/git_repository.h"

using namespace say_count;

TEST_CASE("Git porcelain status exposes branch tracking and changed files") {
    const auto status = parse_git_status(
        "## feature/story...origin/feature/story [ahead 2, behind 1]\n"
        " M game/script.rpy\n"
        "A  game/new.rpy\n"
        "R  old.rpy -> game/renamed.rpy\n"
        "?? notes.txt\n"
        "!! ignored.bin\n");
    CHECK(status.branch == "feature/story");
    CHECK(status.upstream == "origin/feature/story");
    CHECK(status.ahead == 2);
    CHECK(status.behind == 1);
    REQUIRE(status.changes.size() == 4);
    CHECK(status.changes[0].state == "Modified");
    CHECK(status.changes[1].state == "Staged");
    CHECK(status.changes[2].path == "game/renamed.rpy");
    CHECK(status.changes[3].state == "Untracked");
}

TEST_CASE("Git porcelain status handles a repository before its first commit") {
    const auto status = parse_git_status("## No commits yet on main\n?? game/script.rpy\n");
    CHECK(status.branch == "main");
    CHECK(status.upstream.empty());
    REQUIRE(status.changes.size() == 1);
}

TEST_CASE("Git porcelain status identifies detached HEAD") {
    const auto status = parse_git_status("## HEAD (no branch)\n");
    CHECK(status.branch.empty());
}

TEST_CASE("Git remote helpers accept hosting-neutral addresses") {
    CHECK(git_repository_name("git@code.example:team/story.git") == "story");
    CHECK(git_repository_name("https://gitlab.example/team/story/") == "story");
    CHECK(git_repository_name("/srv/git/story.git") == "story");
    CHECK(valid_git_remote("ssh://writer@code.example:2222/team/story.git"));
    CHECK(valid_git_remote("../nearby-repository"));
    std::string error;
    CHECK_FALSE(valid_git_remote("-uhoh", &error));
    CHECK_FALSE(error.empty());
}
