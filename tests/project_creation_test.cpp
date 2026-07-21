#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/project_creation.h"
#include "core/project.h"

TEST_CASE("project creation plans and creates a discoverable minimal story") {
    namespace fs = std::filesystem;
    const fs::path parent = fs::temp_directory_path() / "say-count-create-story-test";
    fs::remove_all(parent); fs::create_directories(parent);
    say_count::ProjectCreationPlan plan;
    const auto preview = say_count::plan_project_creation(parent.string(), "My First Story", &plan);
    REQUIRE(preview.success);
    CHECK(fs::path(plan.root).filename() == "My-First-Story");
    REQUIRE(say_count::apply_project_creation(plan).success);
    const auto discovered = say_count::discover_project_folder(plan.root);
    REQUIRE(discovered);
    REQUIRE(discovered->scripts.size() == 2);
    CHECK(fs::exists(fs::path(plan.root) / "game/script.rpy"));
    fs::remove_all(parent);
}

TEST_CASE("project creation rejects collisions without replacing files") {
    namespace fs = std::filesystem;
    const fs::path parent = fs::temp_directory_path() / "say-count-create-collision-test";
    fs::remove_all(parent); fs::create_directories(parent / "Story");
    std::ofstream(parent / "Story/notes.txt") << "keep me";
    say_count::ProjectCreationPlan plan;
    const auto result = say_count::plan_project_creation(parent.string(), "Story", &plan);
    CHECK_FALSE(result.success);
    CHECK(fs::exists(parent / "Story/notes.txt"));
    fs::remove_all(parent);
}

TEST_CASE("project creation validates its inputs") {
    say_count::ProjectCreationPlan plan;
    CHECK_FALSE(say_count::plan_project_creation("/definitely/missing", "Story", &plan).success);
    CHECK_FALSE(say_count::plan_project_creation(std::filesystem::temp_directory_path().string(), "  ", &plan).success);
}

TEST_CASE("project creation preserves Unicode story names") {
    namespace fs = std::filesystem;
    const fs::path parent = fs::temp_directory_path() / "say-count-create-unicode-test";
    fs::remove_all(parent); fs::create_directories(parent);
    say_count::ProjectCreationPlan plan;
    const auto result = say_count::plan_project_creation(parent.string(), "星の物語", &plan);
    REQUIRE(result.success);
    CHECK(fs::path(plan.root).filename().u8string() == "星の物語");
    CHECK(say_count::project_folder_name("Café Story") == "Café-Story");
    fs::remove_all(parent);
}
