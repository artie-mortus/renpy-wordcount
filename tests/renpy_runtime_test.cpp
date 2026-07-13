#include <catch2/catch_test_macros.hpp>

#include "core/renpy_runtime.h"

#include <chrono>
#include <filesystem>
#include <fstream>

TEST_CASE("runtime state validation accepts JSON values and identifier keys") {
    REQUIRE(say_count::validate_runtime_state(R"({"route":"good","score":3,"flags":[true,false],"nested":{"x":null}})").valid);
    REQUIRE_FALSE(say_count::validate_runtime_state("[]").valid);
    REQUIRE_FALSE(say_count::validate_runtime_state(R"({"bad-key":1})").valid);
    REQUIRE_FALSE(say_count::validate_runtime_state(R"({"ok": nope})").valid);
}

TEST_CASE("runtime helper is generated atomically without replacing unrelated files") {
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path game = fs::temp_directory_path() / ("say-count-runtime-" + unique);
    fs::create_directories(game);
    std::string error;
    REQUIRE(say_count::write_renpy_runtime(game.string(), &error));
    std::ifstream generated(game / "say_count_runtime.rpy"); std::stringstream text; text << generated.rdbuf();
    REQUIRE(text.str().find("SAY_COUNT_STATE") != std::string::npos);
    REQUIRE(say_count::write_renpy_runtime(game.string(), &error));
    std::ofstream(game / "say_count_runtime.rpy", std::ios::trunc) << "# User file\n";
    REQUIRE_FALSE(say_count::write_renpy_runtime(game.string(), &error));
    REQUIRE(error.find("not owned") != std::string::npos);
    fs::remove_all(game);
}

TEST_CASE("runtime presets persist exact validated JSON") {
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() / ("say-count-presets-" + unique);
    say_count::RuntimePresetStore store((root / "presets.dat").string());
    const std::map<std::string, std::string> presets{{"Act Two", R"({"route":"two","score":5})"}};
    REQUIRE(store.Save(presets));
    REQUIRE(store.Load() == presets);
    fs::remove_all(root);
}
