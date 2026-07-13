#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/continuity.h"

using namespace say_count;

TEST_CASE("continuity notes save reload filter and retain source locations") {
    const auto path = std::filesystem::temp_directory_path() / "say-count-continuity-test.dat";
    std::filesystem::remove(path);
    ContinuityStore store(path.string());
    const std::vector<ContinuityNote> notes{
        {"1", "character", "Eileen", "Carries the brass key", "chapter.rpy", 12, 1000},
        {"2", "location", "Station", "Clock is broken", "", 0, 1001},
        {"3", "timeline", "Day two", "Rain begins", "weather.rpy", 8, 1002},
    };
    REQUIRE(store.Save(notes));
    const auto loaded = store.Load();
    REQUIRE(loaded.size() == 3);
    CHECK(loaded[0].file == "chapter.rpy");
    CHECK(loaded[0].line == 12);
    CHECK(filter_continuity_notes(loaded, "brass").front().id == "1");
    CHECK(filter_continuity_notes(loaded, "", "location").front().id == "2");
    CHECK(filter_continuity_notes(loaded, "rain", "character").empty());
    std::filesystem::remove(path);
}
