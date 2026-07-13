#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <filesystem>

#include "core/accessibility.h"

using namespace say_count;

TEST_CASE("accessibility audit groups actionable bad fixtures") {
    const std::vector<NamedScript> scripts{{
        "screens.rpy",
        "screen nav():\n    imagebutton:\n        idle \"go.png\"\n        action Return()\n"
        "    textbutton \"Go\" action Return()\n"
        "define thoughts = Character(\"Me\", what_italic=True)\n"
        "label cg:\n    show event sunset",
    }};
    const auto issues = audit_accessibility(scripts);
    REQUIRE(issues.size() == 4);
    CHECK(std::count_if(issues.begin(), issues.end(), [](const auto& item) {
        return item.severity == AccessibilitySeverity::warning;
    }) == 3);
    CHECK(issues[0].kind == AccessibilityKind::image_description);
    CHECK(issues[1].kind == AccessibilityKind::translation);
    CHECK(issues[3].kind == AccessibilityKind::event_description);
    const auto acknowledged = audit_accessibility(scripts, {issues[0].id});
    CHECK(acknowledged[0].acknowledged);
}

TEST_CASE("accessibility audit accepts described and translated controls") {
    const std::vector<NamedScript> scripts{{
        "good.rpy",
        "screen nav():\n    imagebutton:\n        idle \"go.png\"\n        tooltip _(\"Go\")\n"
        "    textbutton _(\"Go\") action Return()\n"
        "define thoughts = Character(\"Me\", what_italic=True, what_alt=\"thought\")\n"
        "label cg:\n    show event sunset\n    alt \"A sunset over town.\"",
    }};
    CHECK(audit_accessibility(scripts).empty());
}

TEST_CASE("accessibility acknowledgements persist") {
    const auto path = std::filesystem::temp_directory_path() / "say-count-accessibility-test.dat";
    std::filesystem::remove(path);
    AccessibilityAcknowledgementStore store(path.string());
    REQUIRE(store.Save({"screen.rpy:2:0", "script.rpy:8:1"}));
    CHECK(store.Load() == std::set<std::string>{"screen.rpy:2:0", "script.rpy:8:1"});
    std::filesystem::remove(path);
}
