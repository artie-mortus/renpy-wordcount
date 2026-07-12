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

TEST_CASE("external changes reload only clean documents") {
    using say_count::ExternalChangeDecision;
    REQUIRE(say_count::classify_external_change("same", "same", true) ==
            ExternalChangeDecision::Unchanged);
    REQUIRE(say_count::classify_external_change("old", "new", false) ==
            ExternalChangeDecision::Reload);
    REQUIRE(say_count::classify_external_change("local draft", "disk revision", true) ==
            ExternalChangeDecision::Conflict);
}

TEST_CASE("conflict diff aligns common context and highlights changed rows") {
    const auto rows = say_count::build_conflict_diff(
        "label start:\n    \"Local draft.\"\n    return\n",
        "label start:\n    \"Disk revision.\"\n    $ flag = True\n    return\n");
    REQUIRE(rows.size() == 5);
    REQUIRE_FALSE(rows.front().changed);
    REQUIRE(rows[1].changed);
    REQUIRE(rows[2].changed);
    REQUIRE_FALSE(rows[3].changed);
    REQUIRE_FALSE(rows.back().changed);
    REQUIRE(rows.back().local_text == "");
}

TEST_CASE("conflict resolution never mixes versions") {
    const say_count::ExternalConflict conflict{"story.rpy", "local", "disk"};
    REQUIRE(say_count::resolve_conflict_content(conflict, say_count::ConflictResolution::KeepLocal) == "local");
    REQUIRE(say_count::resolve_conflict_content(conflict, say_count::ConflictResolution::UseDisk) == "disk");
}
