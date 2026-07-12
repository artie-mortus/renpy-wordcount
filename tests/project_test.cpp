#include <catch2/catch_test_macros.hpp>

#include "core/project.h"

#include <chrono>
#include <filesystem>
#include <fstream>

TEST_CASE("project discovery separates root game folder and recursive scripts") {
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() / ("say-count-project-" + unique);
    const fs::path game = root / "game";
    fs::create_directories(game / "chapters");
    std::ofstream(game / "script.rpy") << "label start:\n    return\n";
    std::ofstream(game / "chapters" / "two.rpy") << "label two:\n    return\n";
    std::ofstream(game / "ignored.txt") << "not a script";

    std::string error;
    const auto project = say_count::discover_project_folder(root.string(), &error);
    REQUIRE(project);
    REQUIRE(error.empty());
    REQUIRE(fs::path(project->root) == fs::weakly_canonical(root));
    REQUIRE(fs::path(project->scripts_root) == fs::weakly_canonical(game));
    REQUIRE(project->scripts.size() == 2);
    REQUIRE(project->scripts[0].relative_path == "chapters/two.rpy");
    REQUIRE(project->scripts[1].relative_path == "script.rpy");

    const auto from_game = say_count::discover_project_folder(game.string());
    REQUIRE(from_game);
    REQUIRE(fs::path(from_game->root) == fs::weakly_canonical(root));
    fs::remove_all(root);
}

TEST_CASE("recent projects move current root to front and enforce cap") {
    const auto recent = say_count::update_recent_projects({"/one", "/two", "/three"}, "/two", 3);
    REQUIRE(recent == std::vector<std::string>{"/two", "/one", "/three"});
    REQUIRE(say_count::update_recent_projects(recent, "/four", 2) ==
            std::vector<std::string>{"/four", "/two"});
}
