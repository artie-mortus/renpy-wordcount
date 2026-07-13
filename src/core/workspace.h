#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace say_count {

struct WorkspaceFileState {
    std::string path;
    std::size_t caret = 0;
    std::size_t first_visible_line = 0;
};

struct WorkspaceState {
    std::string project_root;
    std::vector<WorkspaceFileState> files;
    std::string active_file;
    std::string perspective;
};

class WorkspaceStore final {
public:
    explicit WorkspaceStore(std::string path) : path_(std::move(path)) {}
    std::optional<WorkspaceState> Load() const;
    bool Save(const WorkspaceState& state) const;

private:
    std::string path_;
};

}  // namespace say_count
