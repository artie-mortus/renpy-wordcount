#include <catch2/catch_test_macros.hpp>

#include "core/chat_runtime.h"

#include <filesystem>
#include <fstream>

namespace {

class TemporaryDirectory {
public:
    TemporaryDirectory() {
        static std::size_t counter = 0;
        path = std::filesystem::temp_directory_path() /
            ("say-count-chat-runtime-test-" + std::to_string(++counter));
        std::filesystem::remove_all(path);
        std::filesystem::create_directories(path);
    }
    ~TemporaryDirectory() { std::filesystem::remove_all(path); }
    std::filesystem::path path;
};

void Write(const std::filesystem::path& path, const std::string& text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream output(path, std::ios::binary);
    output << text;
}

void CreateResources(const std::filesystem::path& path) {
    Write(path / "chat_program.rpy", "# runtime\n");
    Write(path / "say_count_chat_bridge.rpy", "# transition bridge\n");
    Write(path / "say_count_chat_kik.rpy", "# kik skin\n");
    Write(path / "LICENSE.txt", "license\n");
    Write(path / "NOTICE", "notice\n");
    Write(path / "INTEGRATION.md", "instructions\n");
    Write(path / "manifest.json", "{\"commit\":\"pinned\"}\n");
    Write(path / "say_count_chat_config.rpy", "default channel = \"#general\"\n");
    Write(path / "assets/images/chat ui/control.png", "bundled image bytes\n");
}

}  // namespace

TEST_CASE("chat runtime installs cleanly and repeat inspection is identical") {
    TemporaryDirectory temporary;
    const auto resources = temporary.path / "resources";
    const auto game = temporary.path / "game";
    CreateResources(resources);
    const auto clean = say_count::inspect_chat_runtime(resources.string(), game.string());
    CHECK(clean.error.empty());
    CHECK(clean.has_new_files());
    CHECK_FALSE(clean.has_modified_files());
    const auto installed = say_count::install_chat_runtime(clean);
    CHECK(installed.error.empty());
    CHECK(installed.installed == 9);
    CHECK(std::filesystem::exists(game / "images/chat ui/control.png"));
    const auto repeat = say_count::inspect_chat_runtime(resources.string(), game.string());
    CHECK_FALSE(repeat.has_new_files());
    CHECK_FALSE(repeat.has_modified_files());
    CHECK(say_count::install_chat_runtime(repeat).identical == 9);
}

TEST_CASE("chat runtime never overwrites project-owned configuration") {
    TemporaryDirectory temporary;
    const auto resources = temporary.path / "resources";
    const auto game = temporary.path / "game";
    CreateResources(resources);
    REQUIRE(say_count::install_chat_runtime(
        say_count::inspect_chat_runtime(resources.string(), game.string())).error.empty());
    const auto configuration = game / "say_count_chat_config.rpy";
    Write(configuration, "# writer customization\n");
    const auto inspection = say_count::inspect_chat_runtime(resources.string(), game.string());
    CHECK_FALSE(inspection.has_modified_files());
    CHECK(say_count::install_chat_runtime(inspection).identical == 9);
    std::ifstream input(configuration);
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    CHECK(contents == "# writer customization\n");
}

TEST_CASE("chat runtime can export a pinned upgrade outside a customized game") {
    TemporaryDirectory temporary;
    const auto resources = temporary.path / "resources";
    const auto game = temporary.path / "game";
    CreateResources(resources);
    REQUIRE(say_count::install_chat_runtime(
        say_count::inspect_chat_runtime(resources.string(), game.string())).error.empty());
    Write(game / "vendor/chat_program/chat_program.rpy", "# customized old runtime\n");
    const auto export_root = temporary.path / "chat-program-upgrade";
    const auto side_by_side = say_count::inspect_chat_runtime(
        resources.string(), export_root.string());
    REQUIRE_FALSE(side_by_side.has_modified_files());
    CHECK(say_count::install_chat_runtime(side_by_side).installed == 9);
    CHECK(std::filesystem::exists(export_root / "vendor/chat_program/chat_program.rpy"));
    CHECK_FALSE(std::filesystem::exists(game / "vendor/chat_program-pinned"));
    std::ifstream original(game / "vendor/chat_program/chat_program.rpy");
    std::string contents((std::istreambuf_iterator<char>(original)), std::istreambuf_iterator<char>());
    CHECK(contents == "# customized old runtime\n");
}

TEST_CASE("chat runtime preserves existing project assets") {
    TemporaryDirectory temporary;
    const auto resources = temporary.path / "resources";
    const auto game = temporary.path / "game";
    CreateResources(resources);
    Write(game / "images/chat ui/control.png", "writer customization\n");
    const auto inspection = say_count::inspect_chat_runtime(resources.string(), game.string());
    REQUIRE_FALSE(inspection.has_modified_files());
    REQUIRE(say_count::install_chat_runtime(inspection).error.empty());
    std::ifstream input(game / "images/chat ui/control.png");
    std::string contents((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    CHECK(contents == "writer customization\n");
}

TEST_CASE("chat runtime protects any locally modified file as one operation") {
    TemporaryDirectory temporary;
    const auto resources = temporary.path / "resources";
    const auto game = temporary.path / "game";
    CreateResources(resources);
    REQUIRE(say_count::install_chat_runtime(
        say_count::inspect_chat_runtime(resources.string(), game.string())).error.empty());
    const auto runtime = game / "vendor/chat_program/chat_program.rpy";
    Write(runtime, "# customized\n");
    std::filesystem::remove(game / "vendor/chat_program/NOTICE");
    const auto changed = say_count::inspect_chat_runtime(resources.string(), game.string());
    CHECK(changed.has_modified_files());
    CHECK(changed.has_new_files());
    CHECK_FALSE(say_count::install_chat_runtime(changed).error.empty());
    CHECK_FALSE(std::filesystem::exists(game / "vendor/chat_program/NOTICE"));
}

TEST_CASE("chat runtime reports missing provenance resources") {
    TemporaryDirectory temporary;
    CreateResources(temporary.path / "resources");
    std::filesystem::remove(temporary.path / "resources/manifest.json");
    const auto inspection = say_count::inspect_chat_runtime(
        (temporary.path / "resources").string(), (temporary.path / "game").string());
    CHECK_FALSE(inspection.error.empty());
    CHECK(inspection.files.empty());
}
