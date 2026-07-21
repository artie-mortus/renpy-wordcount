#include "core/project_creation.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <utility>

namespace say_count {
namespace {
namespace fs = std::filesystem;

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

}  // namespace

std::string project_folder_name(std::string name) {
    name = trim(std::move(name));
    for (char& value : name) {
        const auto c = static_cast<unsigned char>(value);
        if (c >= 0x80 || std::isalnum(c) || value == '-' || value == '_') continue;
        value = '-';
    }
    while (name.find("--") != std::string::npos) name.replace(name.find("--"), 2, "-");
    while (!name.empty() && (name.front() == '-' || name.front() == '.')) name.erase(name.begin());
    while (!name.empty() && (name.back() == '-' || name.back() == '.')) name.pop_back();
    return name;
}

ProjectCreationResult plan_project_creation(const std::string& parent_directory,
                                            const std::string& story_name,
                                            ProjectCreationPlan* plan) {
    if (!plan) return {false, "The project plan could not be prepared.", {}};
    const std::string title = trim(story_name);
    if (title.empty()) return {false, "Enter a name for the story.", {}};
    const std::string folder = project_folder_name(title);
    if (folder.empty() || folder == "." || folder == "..")
        return {false, "Use letters or numbers in the story name.", {}};
    std::error_code error;
    fs::path parent = fs::weakly_canonical(parent_directory, error);
    if (error || !fs::is_directory(parent, error))
        return {false, "Choose an existing folder for the new story.", {}};
    const fs::path root = parent / folder;
    if (fs::exists(root, error) && (!fs::is_directory(root, error) || !fs::is_empty(root, error)))
        return {false, "A non-empty folder with this story name already exists.", root.string()};
    const std::string escaped = [&] {
        std::string value;
        for (char c : title) { if (c == '\\' || c == '"') value.push_back('\\'); value.push_back(c); }
        return value;
    }();
    *plan = {root.string(), title, {
        {"game/script.rpy", "# " + title + "\n\nlabel start:\n    narrator \"The story starts here.\"\n    return\n"},
        {"game/options.rpy", "define config.name = _(\"" + escaped + "\")\n"},
    }};
    for (const auto& file : plan->files) {
        if (fs::exists(root / file.relative_path, error))
            return {false, "A file required by the new story already exists: " + file.relative_path, root.string()};
    }
    return {true, {}, root.string()};
}

ProjectCreationResult apply_project_creation(const ProjectCreationPlan& plan) {
    if (plan.root.empty() || plan.files.empty()) return {false, "The project plan is empty.", {}};
    namespace fs = std::filesystem;
    std::error_code error;
    const fs::path root(plan.root);
    const bool root_existed = fs::exists(root, error);
    std::vector<fs::path> written;
    for (const auto& file : plan.files) {
        const fs::path destination = root / file.relative_path;
        if (fs::exists(destination, error)) {
            error = std::make_error_code(std::errc::file_exists);
            break;
        }
        fs::create_directories(destination.parent_path(), error);
        if (error) break;
        std::ofstream output(destination, std::ios::binary | std::ios::trunc);
        output << file.contents;
        output.close();
        if (!output) { error = std::make_error_code(std::errc::io_error); break; }
        written.push_back(destination);
    }
    if (!error) return {true, {}, root.string()};
    for (auto it = written.rbegin(); it != written.rend(); ++it) fs::remove(*it, error);
    if (!root_existed) fs::remove_all(root, error);
    return {false, "The story could not be created. No existing files were replaced.", root.string()};
}

}  // namespace say_count
