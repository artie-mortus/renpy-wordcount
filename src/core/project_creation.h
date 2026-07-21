#pragma once

#include <string>
#include <vector>

namespace say_count {

struct ProjectCreationFile {
    std::string relative_path;
    std::string contents;
};

struct ProjectCreationPlan {
    std::string root;
    std::string story_name;
    std::vector<ProjectCreationFile> files;
};

struct ProjectCreationResult {
    bool success = false;
    std::string error;
    std::string root;
};

std::string project_folder_name(std::string story_name);
ProjectCreationResult plan_project_creation(const std::string& parent_directory,
                                            const std::string& story_name,
                                            ProjectCreationPlan* plan);
ProjectCreationResult apply_project_creation(const ProjectCreationPlan& plan);

}  // namespace say_count
