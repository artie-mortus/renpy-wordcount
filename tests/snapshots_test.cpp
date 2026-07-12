#include <catch2/catch_test_macros.hpp>

#include "core/snapshots.h"

#include <chrono>
#include <filesystem>

TEST_CASE("snapshot store creates lists loads and preserves metadata") {
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path directory = fs::temp_directory_path() / ("say-count-snapshots-" + unique);
    say_count::SnapshotStore store(directory.string());
    const std::vector<say_count::NamedScript> files{{"script.rpy", "label start:\n\t\"Hello.\"\n"}};
    const auto created = store.Create(files, "Manual checkpoint", false, "/project", 1);
    REQUIRE(created.success);
    REQUIRE(created.created);
    REQUIRE(created.metadata.label == "Manual checkpoint");
    REQUIRE(created.metadata.project_root == "/project");
    REQUIRE(created.metadata.word_count == 1);
    REQUIRE(created.metadata.file_count == 1);

    const auto listed = store.List();
    REQUIRE(listed.size() == 1);
    const auto loaded = store.Load(listed[0].id);
    REQUIRE(loaded);
    REQUIRE(loaded->files[0].name == files[0].name);
    REQUIRE(loaded->files[0].content == files[0].content);
    const auto duplicate = store.Create(files, "Duplicate", true, "/project", 1);
    REQUIRE(duplicate.success);
    REQUIRE_FALSE(duplicate.created);
    fs::remove_all(directory);
}

TEST_CASE("snapshot store prunes to newest configured limit") {
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path directory = fs::temp_directory_path() / ("say-count-prune-" + unique);
    say_count::SnapshotStore store(directory.string(), 3);
    for (int index = 0; index < 5; ++index) {
        const auto result = store.Create({{"script.rpy", "version " + std::to_string(index)}},
                                         "Snapshot " + std::to_string(index), index % 2 == 0,
                                         "/project", static_cast<std::size_t>(index));
        REQUIRE(result.success);
    }
    const auto snapshots = store.List();
    REQUIRE(snapshots.size() == 3);
    REQUIRE(snapshots[0].id > snapshots[1].id);
    REQUIRE(snapshots[1].id > snapshots[2].id);
    REQUIRE(snapshots[0].label == "Snapshot 4");
    fs::remove_all(directory);
}
