#pragma once

#include "core/git_repository.h"

#include <string>
#include <string_view>
#include <vector>

namespace say_count::app {

template <typename T>
struct GitResult {
    T value{};
    std::string error;
    std::string output;
    explicit operator bool() const { return error.empty(); }
};

struct GitSnapshot {
    bool is_repository = false;
    std::string repository_root;
    std::string remote_name;
    std::string remote_url;
    std::string last_commit;
    GitRepositoryStatus status;
};

class GitClient final {
public:
    explicit GitClient(std::string project_root) : project_root_(std::move(project_root)) {}

    GitResult<GitSnapshot> Status() const;
    GitResult<bool> Initialize() const;
    GitResult<bool> SetRemote(std::string_view remote_url) const;
    GitResult<bool> Fetch() const;
    GitResult<bool> PullFastForward() const;
    GitResult<bool> CommitAndPush(std::string_view message) const;

    static GitResult<std::string> Clone(std::string_view remote_url,
                                        std::string_view destination);

private:
    struct CommandResult {
        int exit_code = -1;
        std::string output;
    };

    static CommandResult Run(const std::vector<std::string>& arguments);
    static std::string Failure(const CommandResult& command, std::string_view fallback);
    std::vector<std::string> Git(std::initializer_list<std::string> arguments) const;

    std::string project_root_;
};

}  // namespace say_count::app
