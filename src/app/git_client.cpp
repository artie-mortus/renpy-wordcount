#include "app/git_client.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <sys/wait.h>

namespace say_count::app {
namespace {

constexpr std::size_t kMaximumOutput = 8 * 1024 * 1024;

std::string trim(std::string value) {
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r' || value.back() == ' '))
        value.pop_back();
    std::size_t start = 0;
    while (start < value.size() && (value[start] == '\n' || value[start] == '\r' || value[start] == ' '))
        ++start;
    if (start != 0) value.erase(0, start);
    return value;
}

std::string shell_quote(std::string_view value) {
    std::string result = "'";
    for (const char byte : value) {
        if (byte == '\'') result += "'\\''";
        else result += byte;
    }
    result += "'";
    return result;
}

std::vector<std::string> lines(std::string_view value) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start < value.size()) {
        const auto newline = value.find('\n', start);
        auto line = value.substr(start, newline == std::string_view::npos ? value.size() - start
                                                                         : newline - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (!line.empty()) result.emplace_back(line);
        if (newline == std::string_view::npos) break;
        start = newline + 1;
    }
    return result;
}

}  // namespace

GitClient::CommandResult GitClient::Run(const std::vector<std::string>& arguments) {
    std::string command = "GIT_TERMINAL_PROMPT=0 ";
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        if (index != 0) command += ' ';
        command += shell_quote(arguments[index]);
    }
    command += " 2>&1";

    CommandResult result;
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return result;
    std::array<char, 4096> buffer{};
    while (true) {
        const std::size_t count = std::fread(buffer.data(), 1, buffer.size(), pipe);
        const std::size_t available = result.output.size() < kMaximumOutput
            ? kMaximumOutput - result.output.size() : 0;
        result.output.append(buffer.data(), std::min(count, available));
        if (count < buffer.size()) {
            if (std::feof(pipe) || std::ferror(pipe)) break;
        }
    }
    const int status = pclose(pipe);
    if (result.output.size() == kMaximumOutput)
        result.output += "\n[Git output truncated by Say Count]";
    result.output = trim(std::move(result.output));
    if (status != -1 && WIFEXITED(status)) result.exit_code = WEXITSTATUS(status);
    return result;
}

std::string GitClient::Failure(const CommandResult& command, std::string_view fallback) {
    if (!command.output.empty()) return command.output;
    if (command.exit_code == -1) return "Could not start Git. Install Git and ensure it is on PATH.";
    return std::string(fallback) + " (Git exited with code " + std::to_string(command.exit_code) + ").";
}

std::vector<std::string> GitClient::Git(std::initializer_list<std::string> arguments) const {
    std::vector<std::string> result{"git", "-C", project_root_};
    result.insert(result.end(), arguments.begin(), arguments.end());
    return result;
}

GitResult<GitSnapshot> GitClient::Status() const {
    GitResult<GitSnapshot> result;
    const auto version = Run({"git", "--version"});
    if (version.exit_code != 0) {
        result.error = Failure(version, "Git is unavailable");
        return result;
    }
    const auto inside = Run(Git({"rev-parse", "--is-inside-work-tree"}));
    if (inside.exit_code != 0 || trim(inside.output) != "true") return result;
    result.value.is_repository = true;

    const auto root = Run(Git({"rev-parse", "--show-toplevel"}));
    if (root.exit_code != 0) {
        result.error = Failure(root, "Could not locate the Git repository root");
        return result;
    }
    result.value.repository_root = trim(root.output);
    const auto status = Run(Git({"status", "--porcelain=v1", "--branch"}));
    if (status.exit_code != 0) {
        result.error = Failure(status, "Could not read repository status");
        return result;
    }
    result.value.status = parse_git_status(status.output);

    const auto remotes = Run(Git({"remote"}));
    if (remotes.exit_code == 0) {
        const auto names = lines(remotes.output);
        const auto configured_remote = [this](const std::string& key) {
            const auto command = Run(Git({"config", "--get", key}));
            return command.exit_code == 0 ? trim(command.output) : std::string{};
        };
        if (!result.value.status.branch.empty())
            result.value.remote_name = configured_remote(
                "branch." + result.value.status.branch + ".pushRemote");
        if (result.value.remote_name.empty())
            result.value.remote_name = configured_remote("remote.pushDefault");
        if (result.value.remote_name.empty() && !result.value.status.upstream.empty()) {
            for (const auto& name : names) {
                if (result.value.status.upstream.rfind(name + "/", 0) == 0 &&
                    name.size() > result.value.remote_name.size())
                    result.value.remote_name = name;
            }
        }
        if (result.value.remote_name.empty() &&
            std::find(names.begin(), names.end(), "origin") != names.end())
            result.value.remote_name = "origin";
        if (result.value.remote_name.empty() && !names.empty())
            result.value.remote_name = names.front();
    }
    if (!result.value.remote_name.empty()) {
        auto url = Run(Git({"remote", "get-url", "--push", result.value.remote_name}));
        if (url.exit_code != 0)
            url = Run(Git({"remote", "get-url", result.value.remote_name}));
        if (url.exit_code == 0) result.value.remote_url = trim(url.output);
    }
    const auto commit = Run(Git({"log", "-1", "--pretty=format:%h  %s"}));
    if (commit.exit_code == 0) result.value.last_commit = trim(commit.output);
    return result;
}

GitResult<bool> GitClient::Initialize() const {
    GitResult<bool> result;
    const auto command = Run(Git({"init"}));
    result.output = command.output;
    if (command.exit_code != 0) result.error = Failure(command, "Could not initialize the repository");
    else result.value = true;
    return result;
}

GitResult<bool> GitClient::SetRemote(std::string_view remote_url) const {
    GitResult<bool> result;
    if (!valid_git_remote(remote_url, &result.error)) return result;
    const auto snapshot = Status();
    if (!snapshot) { result.error = snapshot.error; return result; }
    if (!snapshot.value.is_repository) {
        result.error = "Initialize this project as a Git repository first.";
        return result;
    }
    const std::string url = trim(std::string(remote_url));
    const auto remotes = Run(Git({"remote"}));
    const auto names = lines(remotes.output);
    const bool has_origin = std::find(names.begin(), names.end(), "origin") != names.end();
    const auto command = has_origin
        ? Run(Git({"remote", "set-url", "origin", url}))
        : Run(Git({"remote", "add", "origin", url}));
    result.output = command.output;
    if (command.exit_code != 0) result.error = Failure(command, "Could not set the origin remote");
    else result.value = true;
    return result;
}

GitResult<bool> GitClient::Fetch() const {
    GitResult<bool> result;
    const auto command = Run(Git({"fetch", "--all", "--prune"}));
    result.output = command.output;
    if (command.exit_code != 0) result.error = Failure(command, "Could not fetch remote changes");
    else result.value = true;
    return result;
}

GitResult<bool> GitClient::PullFastForward() const {
    GitResult<bool> result;
    const auto snapshot = Status();
    if (!snapshot) { result.error = snapshot.error; return result; }
    if (!snapshot.value.status.changes.empty()) {
        result.error = "The repository has local changes. Commit or discard them before pulling.";
        return result;
    }
    if (snapshot.value.status.upstream.empty()) {
        result.error = "This branch has no upstream. Push it once to establish tracking before pulling.";
        return result;
    }
    const auto command = Run(Git({"pull", "--ff-only"}));
    result.output = command.output;
    if (command.exit_code != 0)
        result.error = Failure(command, "Could not fast-forward the current branch");
    else result.value = true;
    return result;
}

GitResult<bool> GitClient::CommitAndPush(std::string_view message) const {
    GitResult<bool> result;
    auto snapshot = Status();
    if (!snapshot) { result.error = snapshot.error; return result; }
    if (!snapshot.value.is_repository) {
        result.error = "Initialize this project as a Git repository first.";
        return result;
    }
    if (snapshot.value.remote_name.empty()) {
        result.error = "Set a remote before pushing.";
        return result;
    }
    if (std::any_of(snapshot.value.status.changes.begin(), snapshot.value.status.changes.end(),
                    [](const auto& change) { return change.state == "Conflict"; })) {
        result.error = "Resolve repository conflicts before committing.";
        return result;
    }
    if (!snapshot.value.status.changes.empty()) {
        if (message.empty()) {
            result.error = "Enter a commit message.";
            return result;
        }
        const auto add = Run(Git({"add", "--all"}));
        if (add.exit_code != 0) { result.error = Failure(add, "Could not stage repository changes"); return result; }
        const auto commit = Run(Git({"commit", "-m", std::string(message)}));
        result.output = commit.output;
        if (commit.exit_code != 0) { result.error = Failure(commit, "Could not create the commit"); return result; }
        snapshot = Status();
        if (!snapshot) { result.error = snapshot.error; return result; }
    }
    const auto push = snapshot.value.status.upstream.empty()
        ? Run(Git({"push", "--set-upstream", snapshot.value.remote_name, "HEAD"}))
        : Run(Git({"push"}));
    if (!result.output.empty() && !push.output.empty()) result.output += "\n";
    result.output += push.output;
    if (push.exit_code != 0) result.error = Failure(push, "Could not push the current branch");
    else result.value = true;
    return result;
}

GitResult<std::string> GitClient::Clone(std::string_view remote_url,
                                        std::string_view destination) {
    GitResult<std::string> result;
    if (!valid_git_remote(remote_url, &result.error)) return result;
    std::error_code ec;
    if (std::filesystem::exists(std::filesystem::path(destination), ec)) {
        result.error = "The clone destination already exists. Choose another parent folder or repository name.";
        return result;
    }
    const auto command = Run({"git", "clone", "--", trim(std::string(remote_url)),
                              std::string(destination)});
    result.output = command.output;
    if (command.exit_code != 0) result.error = Failure(command, "Could not clone the repository");
    else result.value = std::string(destination);
    return result;
}

}  // namespace say_count::app
