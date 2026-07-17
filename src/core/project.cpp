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

std::vector<std::string> split_lines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto newline = text.find('\n', start);
        auto line = text.substr(start, newline == std::string_view::npos ? text.size() - start
                                                                         : newline - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        lines.emplace_back(line);
        if (newline == std::string_view::npos) break;
        start = newline + 1;
    }
    return lines;
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
        if (lower(relative.generic_string()) == "say_count_runtime.rpy") continue;
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

ExternalChangeDecision classify_external_change(std::string_view local_content,
                                                std::string_view disk_content,
                                                bool local_dirty) {
    if (local_content == disk_content) return ExternalChangeDecision::Unchanged;
    return local_dirty ? ExternalChangeDecision::Conflict : ExternalChangeDecision::Reload;
}

std::vector<ConflictDiffRow> build_conflict_diff(std::string_view local_content,
                                                 std::string_view disk_content) {
    const auto local = split_lines(local_content);
    const auto disk = split_lines(disk_content);
    std::size_t prefix = 0;
    while (prefix < local.size() && prefix < disk.size() && local[prefix] == disk[prefix]) ++prefix;
    std::size_t suffix = 0;
    while (suffix < local.size() - prefix && suffix < disk.size() - prefix &&
           local[local.size() - suffix - 1] == disk[disk.size() - suffix - 1]) ++suffix;

    std::vector<ConflictDiffRow> rows;
    rows.reserve(std::max(local.size(), disk.size()));
    for (std::size_t index = 0; index < prefix; ++index)
        rows.push_back({index + 1, index + 1, local[index], disk[index], false});
    const std::size_t local_middle = local.size() - prefix - suffix;
    const std::size_t disk_middle = disk.size() - prefix - suffix;
    for (std::size_t index = 0; index < std::max(local_middle, disk_middle); ++index) {
        ConflictDiffRow row;
        row.changed = true;
        if (index < local_middle) {
            row.local_line = prefix + index + 1;
            row.local_text = local[prefix + index];
        }
        if (index < disk_middle) {
            row.disk_line = prefix + index + 1;
            row.disk_text = disk[prefix + index];
        }
        rows.push_back(std::move(row));
    }
    for (std::size_t index = 0; index < suffix; ++index) {
        const std::size_t local_index = local.size() - suffix + index;
        const std::size_t disk_index = disk.size() - suffix + index;
        rows.push_back({local_index + 1, disk_index + 1, local[local_index], disk[disk_index], false});
    }
    return rows;
}

std::string resolve_conflict_content(const ExternalConflict& conflict,
                                     ConflictResolution resolution) {
    return resolution == ConflictResolution::KeepLocal ? conflict.local_content : conflict.disk_content;
}

}  // namespace say_count
