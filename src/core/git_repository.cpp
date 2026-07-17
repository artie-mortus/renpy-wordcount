#include "core/git_repository.h"

#include <algorithm>
#include <cctype>

namespace say_count {
namespace {

std::string trim(std::string_view value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())))
        value.remove_prefix(1);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())))
        value.remove_suffix(1);
    return std::string(value);
}

std::size_t parse_count(std::string_view value) {
    std::size_t result = 0;
    for (const char byte : value) {
        if (!std::isdigit(static_cast<unsigned char>(byte))) break;
        result = result * 10 + static_cast<std::size_t>(byte - '0');
    }
    return result;
}

std::string display_state(char index, char worktree) {
    if (index == '?' && worktree == '?') return "Untracked";
    if (index == '!' && worktree == '!') return "Ignored";
    if (index == 'U' || worktree == 'U' || (index == 'A' && worktree == 'A') ||
        (index == 'D' && worktree == 'D')) return "Conflict";
    if (index != ' ' && worktree != ' ') return "Staged + modified";
    if (index != ' ') return "Staged";
    switch (worktree) {
        case 'M': return "Modified";
        case 'D': return "Deleted";
        case 'A': return "Added";
        case 'R': return "Renamed";
        case 'C': return "Copied";
        default: return "Changed";
    }
}

}  // namespace

GitRepositoryStatus parse_git_status(std::string_view porcelain) {
    GitRepositoryStatus result;
    std::size_t start = 0;
    while (start <= porcelain.size()) {
        const auto newline = porcelain.find('\n', start);
        std::string_view line = porcelain.substr(
            start, newline == std::string_view::npos ? porcelain.size() - start : newline - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.rfind("## ", 0) == 0) {
            auto header = line.substr(3);
            const auto bracket = header.find(" [");
            const auto tracking = header.substr(0, bracket);
            const auto dots = tracking.find("...");
            if (dots == std::string_view::npos) {
                constexpr std::string_view unborn = "No commits yet on ";
                result.branch = tracking.rfind(unborn, 0) == 0
                    ? std::string(tracking.substr(unborn.size())) : std::string(tracking);
                if (result.branch == "HEAD (no branch)") result.branch.clear();
            } else {
                result.branch = std::string(tracking.substr(0, dots));
                result.upstream = std::string(tracking.substr(dots + 3));
            }
            if (bracket != std::string_view::npos) {
                auto relation = header.substr(bracket + 2);
                if (!relation.empty() && relation.back() == ']') relation.remove_suffix(1);
                const auto ahead = relation.find("ahead ");
                const auto behind = relation.find("behind ");
                if (ahead != std::string_view::npos)
                    result.ahead = parse_count(relation.substr(ahead + 6));
                if (behind != std::string_view::npos)
                    result.behind = parse_count(relation.substr(behind + 7));
            }
        } else if (line.size() >= 3 && line[2] == ' ' && !(line[0] == '!' && line[1] == '!')) {
            std::string path = std::string(line.substr(3));
            const auto rename = path.find(" -> ");
            if (rename != std::string::npos) path.erase(0, rename + 4);
            result.changes.push_back({display_state(line[0], line[1]), std::move(path)});
        }
        if (newline == std::string_view::npos) break;
        start = newline + 1;
    }
    return result;
}

std::string git_repository_name(std::string_view remote_url) {
    std::string value = trim(remote_url);
    while (!value.empty() && value.back() == '/') value.pop_back();
    const auto slash = value.find_last_of("/\\:");
    if (slash != std::string::npos) value.erase(0, slash + 1);
    if (value.size() > 4 && value.compare(value.size() - 4, 4, ".git") == 0)
        value.resize(value.size() - 4);
    return value.empty() ? "repository" : value;
}

bool valid_git_remote(std::string_view remote_url, std::string* error) {
    const std::string value = trim(remote_url);
    if (value.empty()) {
        if (error) *error = "Enter a Git remote URL or repository path.";
        return false;
    }
    if (value.front() == '-') {
        if (error) *error = "A Git remote cannot begin with a dash.";
        return false;
    }
    if (std::any_of(value.begin(), value.end(), [](unsigned char byte) {
            return byte < 0x20 || byte == 0x7f;
        })) {
        if (error) *error = "The Git remote contains a control character.";
        return false;
    }
    return true;
}

}  // namespace say_count
