#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
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

enum class ExternalChangeDecision { Unchanged, Reload, Conflict };

struct ExternalConflict {
    std::string path;
    std::string local_content;
    std::string disk_content;
};

struct ConflictDiffRow {
    std::size_t local_line = 0;
    std::size_t disk_line = 0;
    std::string local_text;
    std::string disk_text;
    bool changed = false;
};

enum class ConflictResolution { KeepLocal, UseDisk };

std::optional<ProjectFolder> discover_project_folder(const std::string& selected_path,
                                                     std::string* error = nullptr);
std::vector<std::string> update_recent_projects(const std::vector<std::string>& recent,
                                                const std::string& project_root,
                                                std::size_t limit = 8);
ExternalChangeDecision classify_external_change(std::string_view local_content,
                                                std::string_view disk_content,
                                                bool local_dirty);
std::vector<ConflictDiffRow> build_conflict_diff(std::string_view local_content,
                                                 std::string_view disk_content);
std::string resolve_conflict_content(const ExternalConflict& conflict,
                                     ConflictResolution resolution);

}  // namespace say_count
