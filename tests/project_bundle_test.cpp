#include <catch2/catch_test_macros.hpp>

#include "core/project_bundle.h"

TEST_CASE("JavaScript project bundles import complete compatible state") {
    const std::string json = R"({
      "format":"say-count-project","version":1,"exportedAt":"2026-01-02T03:04:05.000Z",
      "files":[{"name":"script.rpy","content":"\tlabel start:\n    e \"Hi\""}],
      "activeFile":0,
      "settings":{"target":"1200","lineTarget":"80","countMenuChoices":true,"theme":"dark"},
      "targets":{"speakerTargets":{"Eileen":{"words":100,"lines":5}},"sceneTargets":{}},
      "future":{"enabled":true}
    })";
    const auto parsed = say_count::parse_project_bundle(json);
    REQUIRE(parsed.bundle);
    REQUIRE(parsed.bundle->files[0].content[0] == '\t');
    REQUIRE(parsed.bundle->settings.count_menu_choices);
    REQUIRE(parsed.bundle->settings.theme == "dark");
    REQUIRE(parsed.bundle->speaker_targets.at("Eileen").words == 100);
    REQUIRE(parsed.bundle->speaker_targets.at("Eileen").lines == 5);
    REQUIRE(parsed.bundle->extra_fields.count("future") == 1);
}

TEST_CASE("native project exports reimport and preserve unknown root fields") {
    say_count::ProjectBundle bundle;
    bundle.files = {{"one.rpy", "label start:\n    \"Café\""}, {"two.rpy", "jump start"}};
    bundle.active_file = 1;
    bundle.exported_at = "2026-07-12T12:00:00Z";
    bundle.settings = {"500", "20", false, "light"};
    bundle.scene_targets["start"] = {200, 10};
    bundle.extra_fields["pluginState"] = R"({"value":3})";
    const auto json = say_count::project_bundle_json(bundle);
    const auto parsed = say_count::parse_project_bundle(json);
    REQUIRE(parsed.bundle);
    REQUIRE(parsed.bundle->files.size() == bundle.files.size());
    REQUIRE(parsed.bundle->files[0].name == bundle.files[0].name);
    REQUIRE(parsed.bundle->files[0].content == bundle.files[0].content);
    REQUIRE(parsed.bundle->files[1].name == bundle.files[1].name);
    REQUIRE(parsed.bundle->files[1].content == bundle.files[1].content);
    REQUIRE(parsed.bundle->active_file == 1);
    REQUIRE(parsed.bundle->scene_targets.at("start").words == 200);
    REQUIRE(parsed.bundle->extra_fields.at("pluginState") == R"({"value":3})");
}

TEST_CASE("project import decodes surrogate pairs and survives out-of-range numbers") {
    const std::string json = R"({
      "format":"say-count-project","version":1,
      "files":[{"name":"emoji.rpy","content":"e \"go\ud83d\ude00\""}],
      "activeFile":1e300,
      "settings":{"target":"","lineTarget":"","countMenuChoices":false,"theme":"light"},
      "targets":{"speakerTargets":{"Eileen":{"words":1e300,"lines":3}},"sceneTargets":{}},
      "ratio":0.1
    })";
    const auto parsed = say_count::parse_project_bundle(json);
    REQUIRE(parsed.bundle);
    REQUIRE(parsed.bundle->files[0].content == "e \"go\xf0\x9f\x98\x80\"");
    REQUIRE(parsed.bundle->active_file == 0);
    REQUIRE(parsed.bundle->speaker_targets.at("Eileen").words == 0);
    REQUIRE(parsed.bundle->speaker_targets.at("Eileen").lines == 3);
    REQUIRE(parsed.bundle->extra_fields.at("ratio") == "0.1");
}

TEST_CASE("project import rejects incomplete or wrong-version bundles") {
    REQUIRE_FALSE(say_count::parse_project_bundle("{}").bundle);
    REQUIRE_FALSE(say_count::parse_project_bundle(
        R"({"format":"say-count-project","version":2,"files":[]})").bundle);
    REQUIRE_FALSE(say_count::parse_project_bundle(
        R"({"format":"say-count-project","version":1,"files":[{"name":"a.rpy","content":""}]})").bundle);
}
