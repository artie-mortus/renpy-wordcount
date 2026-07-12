#include "core/project.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <system_error>

namespace say_count {
namespace {

namespace fs = std::filesystem;

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char byte) {
        return static_cast<char>(std::tolower(byte));
    });
    return value;
}

}  // namespace

std::optional<ProjectFolder> discover_project_folder(const std::string& selected_path,
                                                     std::string* error) {
    std::error_code ec;
    fs::path selected = fs::weakly_canonical(fs::path(selected_path), ec);
    if (ec || !fs::is_directory(selected, ec)) {
        if (error) *error = "The selected project folder does not exist or is not readable.";
        return std::nullopt;
    }

    fs::path root = selected;
    fs::path scripts_root = selected;
    if (lower(selected.filename().string()) == "game") {
        root = selected.parent_path();
    } else if (fs::is_directory(selected / "game", ec) && !ec) {
        scripts_root = selected / "game";
    }

    ProjectFolder project{root.string(), scripts_root.string(), {}};
    fs::recursive_directory_iterator iterator(
        scripts_root, fs::directory_options::skip_permission_denied, ec), end;
    if (ec) {
        if (error) *error = "The project scripts folder could not be scanned.";
        return std::nullopt;
    }
    for (; iterator != end; iterator.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!iterator->is_regular_file(ec) || ec) { ec.clear(); continue; }
        if (lower(iterator->path().extension().string()) != ".rpy") continue;
        const auto relative = fs::relative(iterator->path(), scripts_root, ec);
        if (ec) { ec.clear(); continue; }
        project.scripts.push_back({relative.generic_string(), iterator->path().string()});
    }
    std::sort(project.scripts.begin(), project.scripts.end(), [](const auto& left, const auto& right) {
        return left.relative_path < right.relative_path;
    });
    return project;
}

std::vector<std::string> update_recent_projects(const std::vector<std::string>& recent,
                                                const std::string& project_root,
                                                std::size_t limit) {
    std::vector<std::string> result;
    if (!project_root.empty() && limit != 0) result.push_back(project_root);
    for (const auto& path : recent) {
        if (path.empty() || path == project_root || result.size() >= limit) continue;
        result.push_back(path);
    }
    return result;
}

}  // namespace say_count
