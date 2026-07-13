#include <catch2/catch_test_macros.hpp>

#include "core/assets.h"

#include <chrono>
#include <filesystem>
#include <fstream>

TEST_CASE("asset discovery finds supported media and skips generated project folders") {
    namespace fs = std::filesystem;
    const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path game = fs::temp_directory_path() / ("say-count-assets-" + unique);
    for (const auto& path : {game / "images", game / "audio", game / "cache", game / "tl"}) fs::create_directories(path);
    std::ofstream(game / "images" / "bg-room.PNG") << "image";
    std::ofstream(game / "audio" / "theme.ogg") << "audio";
    std::ofstream(game / "cache" / "ignored.png") << "cache";
    std::ofstream(game / "tl" / "ignored.wav") << "tl";
    std::ofstream(game / "notes.txt") << "notes";
    const auto assets = say_count::discover_project_assets(game.string());
    REQUIRE(assets.size() == 2);
    REQUIRE(assets[0].relative_path == "audio/theme.ogg");
    REQUIRE(assets[0].kind == say_count::AssetKind::Audio);
    REQUIRE(assets[1].relative_path == "images/bg-room.PNG");
    fs::remove_all(game);
}

TEST_CASE("asset insertion builds each supported RenPy statement") {
    const say_count::ProjectAsset image{"images/bg_room.png", "/game/images/bg_room.png", say_count::AssetKind::Image};
    const say_count::ProjectAsset audio{"audio/theme.ogg", "/game/audio/theme.ogg", say_count::AssetKind::Audio};
    REQUIRE(say_count::asset_insert_statement(image, say_count::AssetInsertAction::Show) == "show images bg room");
    REQUIRE(say_count::asset_insert_statement(image, say_count::AssetInsertAction::Scene) == "scene images bg room");
    REQUIRE(say_count::asset_insert_statement(audio, say_count::AssetInsertAction::PlayMusic) == "play music \"audio/theme.ogg\"");
    REQUIRE(say_count::asset_insert_statement(audio, say_count::AssetInsertAction::PlaySound) == "play sound \"audio/theme.ogg\"");
}
