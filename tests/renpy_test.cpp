#include <catch2/catch_test_macros.hpp>

#include "core/renpy.h"

#include <chrono>
#include <filesystem>
#include <fstream>

namespace {

std::filesystem::path Executable(const std::filesystem::path& directory, const std::string& name) {
    namespace fs = std::filesystem;
    fs::create_directories(directory);
    const fs::path path = directory / name;
    std::ofstream(path) << "#!/bin/sh\n";
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write | fs::perms::owner_exec);
    return path;
}

}  // namespace

TEST_CASE("RenPy detection prioritizes configured environment and PATH executables") {
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() / ("say-count-renpy-" + unique);
    const auto configured = Executable(root / "renpy-8.3.4-sdk", "renpy.sh");
    const auto environment = Executable(root / "environment", "renpy.sh");
    const auto on_path = Executable(root / "bin", "renpy");
    auto found = say_count::detect_renpy_sdk(
        {configured.string(), environment.string(), {(root / "bin").string()}, root.string()});
    REQUIRE(found);
    REQUIRE(found->source == say_count::RenpySdkSource::Configured);
    REQUIRE(found->version == "8.3.4");
    found = say_count::detect_renpy_sdk(
        {(root / "missing").string(), environment.string(), {(root / "bin").string()}, {}});
    REQUIRE(found->source == say_count::RenpySdkSource::Environment);
    found = say_count::detect_renpy_sdk({{}, {}, {(root / "bin").string()}, {}});
    REQUIRE(found->source == say_count::RenpySdkSource::Path);
    fs::remove_all(root);
}

TEST_CASE("RenPy detection accepts SDK directories and rejects non-executables") {
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() / ("say-count-renpy-dir-" + unique);
    Executable(root / "renpy-7.8.0-sdk", "renpy.sh");
    const auto found = say_count::detect_renpy_sdk({(root / "renpy-7.8.0-sdk").string(), {}, {}, {}});
    REQUIRE(found);
    REQUIRE(found->version == "7.8.0");
    const fs::path plain = root / "plain";
    std::ofstream(plain) << "no";
    REQUIRE_FALSE(say_count::is_renpy_executable(plain.string()));
    fs::remove_all(root);
}

TEST_CASE("RenPy version parsing accepts launcher output") {
    REQUIRE(say_count::parse_renpy_version("Ren'Py 8.3.4.25011703") == "8.3.4.25011703");
    REQUIRE(say_count::parse_renpy_version("version 7.8") == "7.8");
    REQUIRE(say_count::parse_renpy_version("unknown").empty());
}

TEST_CASE("RenPy capabilities reject known legacy SDKs and allow modern or custom launchers") {
    REQUIRE_FALSE(say_count::renpy_capabilities("6.18.3").warp);
    REQUIRE(say_count::renpy_capabilities("6.99.14").warp);
    REQUIRE(say_count::renpy_capabilities("7.8.4").director);
    REQUIRE(say_count::renpy_capabilities("8.5.3").director);
    REQUIRE(say_count::renpy_capabilities("").warp);
    REQUIRE_FALSE(say_count::renpy_capabilities("invalid").director);
}

TEST_CASE("RenPy translation language validation accepts safe SDK identifiers") {
    REQUIRE(say_count::valid_renpy_language("spanish"));
    REQUIRE(say_count::valid_renpy_language("pt_br"));
    REQUIRE_FALSE(say_count::valid_renpy_language(""));
    REQUIRE_FALSE(say_count::valid_renpy_language("../english"));
    REQUIRE_FALSE(say_count::valid_renpy_language("two words"));
    REQUIRE_FALSE(say_count::valid_renpy_language("___"));
}
