#include "core/workspace.h"

#include <filesystem>
#include <fstream>
#include <limits>

namespace say_count {
namespace {

constexpr std::size_t kMaxString = 4 * 1024 * 1024;
constexpr std::size_t kMaxFiles = 10000;

void WriteString(std::ostream& output, const std::string& value) {
    output << value.size() << '\n';
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    output.put('\n');
}

bool ReadSize(std::istream& input, std::size_t& value) {
    unsigned long long raw = 0;
    if (!(input >> raw) || raw > std::numeric_limits<std::size_t>::max()) return false;
    input.get();
    value = static_cast<std::size_t>(raw);
    return true;
}

bool ReadString(std::istream& input, std::string& value) {
    std::size_t size = 0;
    if (!ReadSize(input, size) || size > kMaxString) return false;
    value.resize(size);
    if (size && !input.read(value.data(), static_cast<std::streamsize>(size))) return false;
    return input.get() == '\n';
}

}  // namespace

std::optional<WorkspaceState> WorkspaceStore::Load() const {
    std::ifstream input(path_, std::ios::binary);
    std::string header;
    if (!std::getline(input, header) || header != "SAYCOUNT-WORKSPACE-1") return std::nullopt;
    WorkspaceState state;
    if (!ReadString(input, state.project_root) || !ReadString(input, state.active_file) ||
        !ReadString(input, state.perspective)) return std::nullopt;
    std::size_t count = 0;
    if (!ReadSize(input, count) || count > kMaxFiles) return std::nullopt;
    state.files.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
        WorkspaceFileState file;
        if (!ReadString(input, file.path) || !ReadSize(input, file.caret) ||
            !ReadSize(input, file.first_visible_line)) return std::nullopt;
        state.files.push_back(std::move(file));
    }
    return state;
}

bool WorkspaceStore::Save(const WorkspaceState& state) const {
    const std::filesystem::path path(path_);
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output << "SAYCOUNT-WORKSPACE-1\n";
    WriteString(output, state.project_root);
    WriteString(output, state.active_file);
    WriteString(output, state.perspective);
    output << state.files.size() << '\n';
    for (const auto& file : state.files) {
        WriteString(output, file.path);
        output << file.caret << '\n' << file.first_visible_line << '\n';
    }
    output.close();
    if (!output) { std::filesystem::remove(temporary, error); return false; }
    std::filesystem::rename(temporary, path, error);
    if (!error) return true;
    std::filesystem::remove(path, error);
    error.clear();
    std::filesystem::rename(temporary, path, error);
    if (error) std::filesystem::remove(temporary, error);
    return !error;
}

}  // namespace say_count
