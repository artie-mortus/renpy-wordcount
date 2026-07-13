#include <catch2/catch_test_macros.hpp>

#include <filesystem>
#include <fstream>

#include "core/translation.h"

using namespace say_count;

TEST_CASE("translation dashboard detects and enriches missing strings") {
    const auto root = std::filesystem::temp_directory_path() / "say-count-translation-test";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "tl" / "spanish" / "chapter");
    {
        std::ofstream output(root / "tl" / "spanish" / "chapter" / "one.rpy");
        output << "# game/chapter/one.rpy:3\n# e \"Hello.\"\ntranslate spanish strings:\n    old \"Hello.\"\n    new \"\"\n"
               << "# game/chapter/one.rpy:4\n# \"Done.\"\n    \"Hecho.\"\n";
    }
    const std::vector<NamedScript> source{{
        "chapter/one.rpy", "define e = Character(\"Eileen\")\nlabel arrival:\n    e \"Hello.\"\n    \"Done.\"",
    }};
    const auto dashboard = scan_translation_dashboard(root.string(), "spanish", source);
    REQUIRE(dashboard.ready);
    REQUIRE(dashboard.missing.size() == 1);
    const auto& entry = dashboard.missing.front();
    CHECK(entry.file == "chapter/one.rpy");
    CHECK(entry.line == 3);
    CHECK(entry.text == "Hello.");
    CHECK(entry.source_file == "chapter/one.rpy");
    CHECK(entry.speaker == "Eileen");
    CHECK(entry.label == "arrival");
    CHECK(entry.translation_file == "tl/spanish/chapter/one.rpy");
    CHECK(entry.translation_line == 5);
    std::filesystem::remove_all(root);
}

TEST_CASE("translation groups sort source files and missing folders are not ready") {
    const auto groups = group_translation_entries({
        {"script.rpy", 4}, {"chapter/two.rpy", 2}, {"script.rpy", 8},
    });
    REQUIRE(groups.size() == 2);
    CHECK(groups.begin()->first == "chapter/two.rpy");
    CHECK(groups.at("script.rpy").size() == 2);
    const auto dashboard = scan_translation_dashboard("/definitely/missing", "spanish", {});
    CHECK_FALSE(dashboard.ready);
    CHECK(normalize_translation_source_path("./game/chapter\\one.rpy") == "chapter/one.rpy");
}
