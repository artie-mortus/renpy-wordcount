#include <catch2/catch_test_macros.hpp>

#include "app/git_client.h"

#include <chrono>
#include <filesystem>

using namespace say_count::app;

namespace {

class TemporaryGitWorkspace {
public:
    TemporaryGitWorkspace() {
        const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
        root = std::filesystem::temp_directory_path() /
            ("say-count-git-test-" + std::to_string(stamp));
        std::filesystem::create_directories(root);
    }
    ~TemporaryGitWorkspace() {
        std::error_code error;
        std::filesystem::remove_all(root, error);
    }
    std::filesystem::path root;
};

}  // namespace

TEST_CASE("Git client initializes and reads a hosting-neutral repository") {
    TemporaryGitWorkspace workspace;
    const auto project = workspace.root / "writer's-project";
    std::filesystem::create_directories(project);
    GitClient client(project.string());

    const auto before = client.Status();
    REQUIRE(before);
    CHECK_FALSE(before.value.is_repository);
    REQUIRE(client.Initialize());
    REQUIRE(client.SetRemote("ssh://git@git.example/team/private-story.git"));

    const auto after = client.Status();
    REQUIRE(after);
    CHECK(after.value.is_repository);
    CHECK(after.value.repository_root == project.string());
    CHECK(after.value.remote_name == "origin");
    CHECK(after.value.remote_url == "ssh://git@git.example/team/private-story.git");
}

TEST_CASE("Git client safely clones a local repository with quoted paths") {
    TemporaryGitWorkspace workspace;
    const auto source = workspace.root / "source's story";
    const auto destination = workspace.root / "destination's story";
    std::filesystem::create_directories(source);
    GitClient source_client(source.string());
    REQUIRE(source_client.Initialize());

    const auto cloned = GitClient::Clone(source.string(), destination.string());
    REQUIRE(cloned);
    CHECK(cloned.value == destination.string());
    const auto status = GitClient(destination.string()).Status();
    REQUIRE(status);
    CHECK(status.value.is_repository);
}
