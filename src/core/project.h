#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace say_count {

struct ProjectScriptFile {
    std::string relative_path;
    std::string absolute_path;
};

struct ProjectFolder {
    std::string root;
    std::string scripts_root;
    std::vector<ProjectScriptFile> scripts;
};

std::optional<ProjectFolder> discover_project_folder(const std::string& selected_path,
                                                     std::string* error = nullptr);
std::vector<std::string> update_recent_projects(const std::vector<std::string>& recent,
                                                const std::string& project_root,
                                                std::size_t limit = 8);

}  // namespace say_count
