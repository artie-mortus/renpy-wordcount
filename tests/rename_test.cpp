#include <catch2/catch_test_macros.hpp>

#include "core/rename.h"

TEST_CASE("speaker rename changes declarations and dialogue but not similar or quoted text") {
    const std::vector<say_count::NamedScript> files{{"story.rpy",
        "define e = Character(\"Eileen\")\r\n"
        "define eve = Character(\"Eve\")\r\n"
        "    e happy \"Hello e.\" # e stays in comment\r\n"
        "    eve \"Other.\"\r\n"
        "    $ value = \"e\"\r\n"}};
    const auto plan = say_count::plan_symbol_rename(files, say_count::RenameKind::SpeakerAlias, "e", "m");
    REQUIRE(plan.error.empty());
    REQUIRE(plan.changes.size() == 2);
    REQUIRE(plan.files[0].content.find("define m = Character") != std::string::npos);
    REQUIRE(plan.files[0].content.find("    m happy \"Hello e.\" # e stays") != std::string::npos);
    REQUIRE(plan.files[0].content.find("define eve") != std::string::npos);
    REQUIRE(plan.files[0].content.find("\r\n") != std::string::npos);
}

TEST_CASE("label rename updates declarations and static jump call references only") {
    const std::vector<say_count::NamedScript> files{
        {"one.rpy", "label finish(points=0):\n    jump finish\n    call finish(2)\n    jump finishing\n"},
        {"two.rpy", "# jump finish\n\"jump finish\"\n$ target = 'finish'\njump expression target\n"}};
    const auto plan = say_count::plan_symbol_rename(files, say_count::RenameKind::Label,
                                                     "finish", "ending");
    REQUIRE(plan.error.empty());
    REQUIRE(plan.changes.size() == 3);
    REQUIRE(plan.files[0].content.find("label ending(points=0):") != std::string::npos);
    REQUIRE(plan.files[0].content.find("jump ending") != std::string::npos);
    REQUIRE(plan.files[0].content.find("call ending(2)") != std::string::npos);
    REQUIRE(plan.files[0].content.find("jump finishing") != std::string::npos);
    REQUIRE(plan.files[1].content == files[1].content);
}

TEST_CASE("rename validates names and rejects declaration collisions") {
    const std::vector<say_count::NamedScript> files{{"story.rpy",
        "label start:\n    jump finish\nlabel finish:\n    return\n"}};
    REQUIRE_FALSE(say_count::plan_symbol_rename(files, say_count::RenameKind::Label,
                                                "finish", "bad-name").error.empty());
    REQUIRE_FALSE(say_count::plan_symbol_rename(files, say_count::RenameKind::Label,
                                                "finish", "start").error.empty());
    REQUIRE_FALSE(say_count::plan_symbol_rename(files, say_count::RenameKind::Label,
                                                "finish", "finish").error.empty());
}

TEST_CASE("label rename does not rewrite expression flow syntax") {
    const std::vector<say_count::NamedScript> files{{"story.rpy",
        "label expression:\n    call expression()\n    jump expression target\n    call expression get_target()\n"}};
    const auto plan = say_count::plan_symbol_rename(files, say_count::RenameKind::Label,
                                                     "expression", "destination");
    REQUIRE(plan.error.empty());
    REQUIRE(plan.changes.size() == 2);
    REQUIRE(plan.files[0].content.find("label destination:") != std::string::npos);
    REQUIRE(plan.files[0].content.find("call destination()") != std::string::npos);
    REQUIRE(plan.files[0].content.find("jump expression target") != std::string::npos);
    REQUIRE(plan.files[0].content.find("call expression get_target()") != std::string::npos);
}
