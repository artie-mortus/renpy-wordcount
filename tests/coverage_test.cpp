#include <catch2/catch_test_macros.hpp>

#include "core/coverage.h"

#include <chrono>
#include <filesystem>
#include <fstream>

TEST_CASE("coverage labels qualify locals and deduplicate project declarations") {
    const auto labels = say_count::collect_project_labels({{"one.rpy", "label start:\nlabel .inside:\n"}, {"two.rpy", "label end(x=1):\nlabel start:\n"}});
    REQUIRE(labels == std::vector<std::string>{"end", "start", "start.inside"});
}

TEST_CASE("coverage tail reads appended complete JSONL records and handles truncation") {
    namespace fs = std::filesystem; const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path path = fs::temp_directory_path() / ("say-count-coverage-" + unique + ".jsonl");
    { std::ofstream out(path); out << "{\"label\": \"start\"}\n{\"label\": \"caf\\u00e9\""; }
    say_count::CoverageTail tail; REQUIRE(tail.Read(path.string()) == std::vector<std::string>{"start"});
    { std::ofstream out(path, std::ios::app); out << "}\ninvalid\n"; }
    REQUIRE(tail.Read(path.string()) == std::vector<std::string>{"caf\xc3\xa9"});
    { std::ofstream out(path, std::ios::trunc); out << "{\"label\":\"reset\"}\n"; }
    REQUIRE(tail.Read(path.string()) == std::vector<std::string>{"reset"}); fs::remove(path);
}

TEST_CASE("manual coverage persists per project and coverage names are stable") {
    namespace fs = std::filesystem; const auto unique = std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    const fs::path root = fs::temp_directory_path() / ("say-count-manual-" + unique);
    say_count::ManualCoverageStore store((root / "coverage.dat").string());
    const std::map<std::string, std::set<std::string>> values{{"/one", {"start"}}, {"/two", {"end"}}};
    REQUIRE(store.Save(values)); REQUIRE(store.Load() == values);
    REQUIRE(say_count::coverage_file_name("/one") == say_count::coverage_file_name("/one"));
    REQUIRE(say_count::coverage_file_name("/one") != say_count::coverage_file_name("/two")); fs::remove_all(root);
}
