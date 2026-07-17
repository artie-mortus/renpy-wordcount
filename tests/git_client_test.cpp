#include <catch2/catch_test_macros.hpp>

#include "app/git_client.h"

#include <chrono>
#include <filesystem>
#include <fstream>

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

TEST_CASE("Git client reports the repository configured push destination") {
    TemporaryGitWorkspace workspace;
    const auto project = workspace.root / "configured-push";
    std::filesystem::create_directories(project);
    GitClient client(project.string());
    REQUIRE(client.Initialize());
    REQUIRE(client.SetRemote("https://github.com/team/origin.git"));
    {
        std::ofstream config(project / ".git" / "config", std::ios::app);
        REQUIRE(config);
        config << "\n[remote \"publishing\"]\n"
                  "\turl = https://github.com/team/fetch.git\n"
                  "\tpushurl = git@github.com:team/private-story.git\n"
                  "[remote]\n"
                  "\tpushDefault = publishing\n";
    }

    const auto status = client.Status();
    REQUIRE(status);
    CHECK(status.value.remote_name == "publishing");
    CHECK(status.value.remote_url == "git@github.com:team/private-story.git");
}

TEST_CASE("Git client commits local project updates and pushes its configured remote") {
    TemporaryGitWorkspace workspace;
    const auto remote = workspace.root / "remote";
    const auto project = workspace.root / "working-copy";
    std::filesystem::create_directories(remote);
    std::filesystem::create_directories(project / "game");
    GitClient remote_client(remote.string());
    GitClient project_client(project.string());
    REQUIRE(remote_client.Initialize());
    REQUIRE(project_client.Initialize());
    {
        std::ofstream remote_config(remote / ".git" / "config", std::ios::app);
        REQUIRE(remote_config);
        remote_config << "\n[receive]\n\tdenyCurrentBranch = updateInstead\n";
    }
    {
        std::ofstream project_config(project / ".git" / "config", std::ios::app);
        REQUIRE(project_config);
        project_config << "\n[user]\n\tname = Say Count Test\n"
                          "\temail = tests@say-count.invalid\n";
        std::ofstream script(project / "game" / "script.rpy");
        REQUIRE(script);
        script << "label start:\n    \"Local update\"\n";
    }
    REQUIRE(project_client.SetRemote(remote.string()));

    const auto pushed = project_client.CommitAndPush("Update local story");
    REQUIRE(pushed);
    const auto local_status = project_client.Status();
    REQUIRE(local_status);
    CHECK(local_status.value.status.changes.empty());
    CHECK_FALSE(local_status.value.status.upstream.empty());
    const auto remote_status = remote_client.Status();
    REQUIRE(remote_status);
    CHECK(remote_status.value.last_commit.find("Update local story") != std::string::npos);
    CHECK(std::filesystem::exists(remote / "game" / "script.rpy"));
}
