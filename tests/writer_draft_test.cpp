#include <catch2/catch_test_macros.hpp>

#include <filesystem>

#include "core/writer_draft.h"

TEST_CASE("writer drafts save beside scripts and round trip Unicode") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "say-count-writer-draft-test";
    fs::remove_all(root); fs::create_directories(root);
    const std::string script = (root / "scene.rpy").string();
    REQUIRE(say_count::save_writer_draft(script, "Eileen: Héllo"));
    REQUIRE(say_count::load_writer_draft(script));
    CHECK(*say_count::load_writer_draft(script) == "Eileen: Héllo");
    CHECK(fs::exists(script + ".saycount.txt"));
    fs::remove_all(root);
}

TEST_CASE("writer draft comparison detects manual script edits") {
    const std::string writing = "Eileen: Hello";
    const auto generated = say_count::render_writer_draft(writing);
    CHECK(say_count::compare_writer_draft(writing, generated) == say_count::WriterDraftState::InSync);
    CHECK(say_count::compare_writer_draft(writing, generated + "# manual\n") ==
          say_count::WriterDraftState::ScriptDiffers);
    CHECK(say_count::compare_writer_draft({}, generated) == say_count::WriterDraftState::NoDraft);
}

TEST_CASE("empty writer drafts generate no script") {
    CHECK(say_count::render_writer_draft(" \n\t").empty());
}

TEST_CASE("writer draft output always includes the RenPy start label") {
    const auto generated = say_count::render_writer_draft(
        "She said it'd be here, said Leon.\n\n"
        "Well, it's not here, said Robin.");
    CHECK(generated.find("label start:\n") != std::string::npos);
    CHECK(generated.find("leon \"She said it'd be here.\"") != std::string::npos);
    CHECK(generated.find("robin \"Well, it's not here.\"") != std::string::npos);
}

TEST_CASE("writer drafts can generate a complete social media chat scene") {
    say_count::WriterDraftRenderOptions options;
    options.output = say_count::WriterDraftOutput::SocialMediaChat;
    options.chat_channel = "#ops";
    options.chat_skin = "discord";
    const auto generated = say_count::render_writer_draft(
        "[Leon is typing]\nAnd I gotta stop him, said Leon.", options);
    CHECK(generated.find("default leon = ChatCharacter(\"Leon\")") != std::string::npos);
    CHECK(generated.find("label start:\n") != std::string::npos);
    CHECK(generated.find("label chat_scene:") == std::string::npos);
    CHECK(generated.find("call say_count_chat_begin(\"#ops\", skin=\"discord\")") != std::string::npos);
    CHECK(generated.find("leon \"And I gotta stop him.\"") != std::string::npos);
    CHECK(generated.find("call say_count_chat_end") != std::string::npos);
}
