#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/voice.h"

using namespace say_count;

TEST_CASE("voice model finds stable source ids characters narration and extend") {
    const auto rows = parse_voice_dialogue({{
        "script.rpy",
        "define e = Character(\"Eileen\")\nlabel start:\n    e \"Hello.\"\n"
        "    \"Narration.\"\n    color \"#fff\"\n    extend \"Still Eileen.\"\n    scene bg room",
    }});
    REQUIRE(rows.size() == 4);
    CHECK(rows[0].id == "script.rpy:3");
    CHECK(rows[0].speaker == "Eileen");
    CHECK(rows[1].speaker == "Narrator");
    CHECK(rows[2].speaker == "color");
    CHECK(rows[3].speaker == "Eileen");
    CHECK(rows[0].label == "start");
    const auto usable = voice_script_rows(rows);
    REQUIRE(usable.size() == 3);
    CHECK(voice_script_rows(rows, "Eileen").size() == 2);
}

TEST_CASE("voice tracker state saves and reloads filenames notes and statuses") {
    const auto path = std::filesystem::temp_directory_path() / "say-count-voice-test.dat";
    std::filesystem::remove(path);
    VoiceStore store(path.string());
    const std::map<std::string, VoiceEntry> entries{
        {"script.rpy:3", {VoiceStatus::approved, "voice/eileen/003.ogg", "Clean take\nkeep"}},
        {"章.rpy:8", {VoiceStatus::retake, "", "Noise"}},
    };
    REQUIRE(store.Save(entries));
    const auto loaded = store.Load();
    CHECK(loaded.size() == 2);
    CHECK(loaded.at("script.rpy:3").status == VoiceStatus::approved);
    CHECK(loaded.at("script.rpy:3").voice_file == "voice/eileen/003.ogg");
    CHECK(loaded.at("script.rpy:3").notes == "Clean take\nkeep");
    CHECK(loaded.at("章.rpy:8").status == VoiceStatus::retake);
    std::filesystem::remove(path);
}
