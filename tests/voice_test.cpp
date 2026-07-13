#include <catch2/catch_test_macros.hpp>

#include <algorithm>
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

TEST_CASE("voice exports preserve role totals statuses context and escaping") {
    const auto rows = parse_voice_dialogue({{
        "script.rpy", "define e = Character(\"Eileen\")\nlabel start:\n    e \"Hello & goodbye.\"\n    \"Narration.\"",
    }});
    const std::map<std::string, VoiceEntry> entries{
        {"script.rpy:3", {VoiceStatus::approved, "eileen,003.ogg", "Use \"final\""}},
    };
    const auto csv = voice_tracking_csv(rows, entries);
    CHECK(csv.find("\"eileen,003.ogg\"") != std::string::npos);
    CHECK(csv.find("\"Use \"\"final\"\"\"") != std::string::npos);
    CHECK(std::count(csv.begin(), csv.end(), '\n') == 3);

    const auto role = voice_script_html(rows, entries, "Eileen", true);
    CHECK(role.find("1 lines") != std::string::npos);
    CHECK(role.find("Hello &amp; goodbye.") != std::string::npos);
    CHECK(role.find("Source script.rpy:3") != std::string::npos);
    CHECK(role.find("Narration") == std::string::npos);
    const auto no_context = voice_script_html(rows, entries, {}, false);
    CHECK(no_context.find("Source script.rpy") == std::string::npos);
}
