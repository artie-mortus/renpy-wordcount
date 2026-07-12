#include <catch2/catch_test_macros.hpp>
#include "core/version.h"

TEST_CASE("statistics exports preserve Unicode and escape content") {
    const auto analysis = say_count::analyze_script("label start:\n    define x = Character(\"Élodie\")\n    x \"Café & <sun>.\"\n");
    const auto csv = say_count::statistics_csv(analysis);
    const auto json = say_count::statistics_json(analysis);
    const auto html = say_count::statistics_html(analysis, "Say <Count>", "now");
    REQUIRE(csv.find("Élodie") != std::string::npos);
    REQUIRE(json.find("Café & <sun>.") != std::string::npos);
    REQUIRE(html.find("Élodie") != std::string::npos);
    REQUIRE(html.find("Café") == std::string::npos); // report summarizes, avoiding raw dialogue
    REQUIRE(html.find("Say &lt;Count&gt;") != std::string::npos);
    REQUIRE(html.find("<meta charset=\"utf-8\">") != std::string::npos);
}
