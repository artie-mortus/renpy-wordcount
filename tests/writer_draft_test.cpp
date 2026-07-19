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
