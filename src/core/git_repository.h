#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct GitFileChange {
    std::string state;
    std::string path;
};

struct GitRepositoryStatus {
    std::string branch;
    std::string upstream;
    std::size_t ahead = 0;
    std::size_t behind = 0;
    std::vector<GitFileChange> changes;
};

GitRepositoryStatus parse_git_status(std::string_view porcelain);
std::string git_repository_name(std::string_view remote_url);
bool valid_git_remote(std::string_view remote_url, std::string* error = nullptr);

}  // namespace say_count
