#include <catch2/catch_test_macros.hpp>

#include "core/script_builder.h"

using namespace say_count;

TEST_CASE("story builder escapes character dialogue and narration") {
    auto character = build_story_element({StoryElementKind::Character, "e", "Eileen \"E\""});
    REQUIRE(character.valid);
    CHECK(character.text == "define e = Character(\"Eileen \\\"E\\\"\")\n");
    auto dialogue = build_story_element({StoryElementKind::Dialogue, "e", "Hello \"there\"", {}, "    "});
    REQUIRE(dialogue.valid);
    CHECK(dialogue.text == "    e \"Hello \\\"there\\\"\"\n");
}

TEST_CASE("story builder creates indented choices with editable placeholders") {
    auto result = build_story_element({StoryElementKind::Choice, "What now?", {}, "Open the door\nStay quiet", "    "});
    REQUIRE(result.valid);
    CHECK(result.text == "    menu:\n        \"What now?\"\n        \"Open the door\":\n            pass\n        \"Stay quiet\":\n            pass\n");
}

TEST_CASE("story builder validates aliases targets and choice counts") {
    CHECK_FALSE(build_story_element({StoryElementKind::Character, "bad name", "Name"}).valid);
    CHECK_FALSE(build_story_element({StoryElementKind::Jump, "scene one"}).valid);
    CHECK_FALSE(build_story_element({StoryElementKind::Choice, {}, {}, "Only one"}).valid);
    CHECK_FALSE(build_story_element({StoryElementKind::Pause, "soon"}).valid);
    CHECK_FALSE(build_story_element({StoryElementKind::Show, "eileen happy", "far left"}).valid);
}

TEST_CASE("story builder creates scene audio and jump statements") {
    CHECK(build_story_element({StoryElementKind::Scene, "bg kitchen"}).text == "scene bg kitchen\n");
    CHECK(build_story_element({StoryElementKind::Music, "audio/theme.ogg"}).text == "play music \"audio/theme.ogg\"\n");
    CHECK(build_story_element({StoryElementKind::Sound, "audio/knock.ogg"}).text == "play sound \"audio/knock.ogg\"\n");
    CHECK(build_story_element({StoryElementKind::Jump, "ending"}).text == "jump ending\n");
}

TEST_CASE("story builder creates stage timing and reusable flow commands") {
    CHECK(build_story_element({StoryElementKind::Show, "eileen happy", "left", {}, "    "}).text ==
          "    show eileen happy at left\n");
    CHECK(build_story_element({StoryElementKind::Hide, "eileen"}).text == "hide eileen\n");
    CHECK(build_story_element({StoryElementKind::StopMusic, "0.5"}).text ==
          "stop music fadeout 0.5\n");
    CHECK(build_story_element({StoryElementKind::Pause, "1.0"}).text == "pause 1.0\n");
    CHECK(build_story_element({StoryElementKind::Call, "chapter_one.aside"}).text ==
          "call chapter_one.aside\n");
    CHECK(build_story_element({StoryElementKind::Return}).text == "return\n");
    CHECK(build_story_element({StoryElementKind::Transition, "dissolve"}).text == "with dissolve\n");
}
