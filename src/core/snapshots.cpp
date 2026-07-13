#include "core/snapshots.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <limits>

namespace say_count {
namespace {

namespace fs = std::filesystem;
constexpr std::string_view kMagic = "SAY-COUNT-SNAPSHOT-1";

void write_blob(std::ostream& output, std::string_view value) {
    output << value.size() << '\n';
    output.write(value.data(), static_cast<std::streamsize>(value.size()));
    output.put('\n');
}

bool read_number(std::istream& input, std::uint64_t& value) {
    std::string line;
    if (!std::getline(input, line) || line.empty()) return false;
    try {
        std::size_t consumed = 0;
        value = std::stoull(line, &consumed);
        return consumed == line.size();
    } catch (...) {
        return false;
    }
}

bool read_blob(std::istream& input, std::string& value) {
    std::uint64_t length = 0;
    if (!read_number(input, length) || length > static_cast<std::uint64_t>(
        std::numeric_limits<std::streamsize>::max())) return false;
    value.resize(static_cast<std::size_t>(length));
    input.read(value.data(), static_cast<std::streamsize>(length));
    if (input.gcount() != static_cast<std::streamsize>(length)) return false;
    return input.get() == '\n';
}

bool same_files(const std::vector<NamedScript>& left, const std::vector<NamedScript>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index)
        if (left[index].name != right[index].name || left[index].content != right[index].content) return false;
    return true;
}

std::optional<Snapshot> read_snapshot(const fs::path& path, std::string* error) {
    std::ifstream input(path, std::ios::binary);
    std::string magic;
    if (!input || !std::getline(input, magic) || magic != kMagic) {
        if (error) *error = "Invalid snapshot header: " + path.string();
        return std::nullopt;
    }
    Snapshot snapshot;
    std::uint64_t automatic = 0, words = 0, count = 0;
    if (!read_number(input, snapshot.metadata.id) || !read_number(input, automatic) ||
        !read_number(input, words) || !read_blob(input, snapshot.metadata.label) ||
        !read_blob(input, snapshot.metadata.project_root) || !read_number(input, count)) {
        if (error) *error = "Invalid snapshot metadata: " + path.string();
        return std::nullopt;
    }
    snapshot.metadata.automatic = automatic != 0;
    snapshot.metadata.word_count = static_cast<std::size_t>(words);
    snapshot.metadata.file_count = static_cast<std::size_t>(count);
    snapshot.metadata.storage_path = path.string();
    snapshot.files.reserve(snapshot.metadata.file_count);
    for (std::size_t index = 0; index < snapshot.metadata.file_count; ++index) {
        NamedScript file;
        if (!read_blob(input, file.name) || !read_blob(input, file.content)) {
            if (error) *error = "Invalid snapshot file data: " + path.string();
            return std::nullopt;
        }
        snapshot.files.push_back(std::move(file));
    }
    return snapshot;
}

}  // namespace

SnapshotComparison compare_snapshot(const std::vector<NamedScript>& current,
                                    const Snapshot& snapshot) {
    SnapshotComparison result;
    std::vector<bool> matched(snapshot.files.size(), false);
    for (const auto& file : current) {
        const auto found = std::find_if(snapshot.files.begin(), snapshot.files.end(),
                                        [&](const auto& candidate) { return candidate.name == file.name; });
        SnapshotFileComparison row;
        row.name = file.name;
        row.current_words = count_words(file.content);
        result.current_words += row.current_words;
        if (found == snapshot.files.end()) {
            row.status = SnapshotFileStatus::Removed;
        } else {
            const auto index = static_cast<std::size_t>(found - snapshot.files.begin());
            matched[index] = true;
            row.snapshot_words = count_words(found->content);
            row.status = file.content == found->content ? SnapshotFileStatus::Unchanged
                                                       : SnapshotFileStatus::Changed;
        }
        if (row.status != SnapshotFileStatus::Unchanged) ++result.changed_files;
        result.files.push_back(std::move(row));
    }
    for (std::size_t index = 0; index < snapshot.files.size(); ++index) {
        if (matched[index]) continue;
        SnapshotFileComparison row;
        row.name = snapshot.files[index].name;
        row.status = SnapshotFileStatus::Added;
        row.snapshot_words = count_words(snapshot.files[index].content);
        ++result.changed_files;
        result.files.push_back(std::move(row));
    }
    for (const auto& file : snapshot.files) result.snapshot_words += count_words(file.content);
    std::sort(result.files.begin(), result.files.end(), [](const auto& left, const auto& right) {
        if (left.status != right.status) return left.status != SnapshotFileStatus::Unchanged;
        return left.name < right.name;
    });
    return result;
}

SnapshotStore::SnapshotStore(std::string directory, std::size_t limit)
    : directory_(std::move(directory)), limit_(limit) {}

SnapshotCreateResult SnapshotStore::Create(const std::vector<NamedScript>& files, std::string label,
                                           bool automatic, std::string project_root,
                                           std::size_t word_count) {
    SnapshotCreateResult result;
    std::error_code ec;
    fs::create_directories(directory_, ec);
    if (ec) { result.error = "Could not create the snapshot directory."; return result; }

    const auto existing = List(&result.error);
    if (!result.error.empty()) return result;
    if (!existing.empty()) {
        const auto newest = Load(existing.front().id, &result.error);
        if (!result.error.empty()) return result;
        if (newest && same_files(newest->files, files)) {
            result.success = true;
            result.metadata = newest->metadata;
            return result;
        }
    }

    std::uint64_t id = static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count());
    if (!existing.empty()) id = std::max(id, existing.front().id + 1);
    fs::path final;
    do { final = fs::path(directory_) / (std::to_string(id++) + ".scsnap"); }
    while (fs::exists(final, ec) && !ec);
    --id;
    const fs::path temporary = final.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output << kMagic << '\n' << id << '\n' << (automatic ? 1 : 0) << '\n' << word_count << '\n';
    write_blob(output, label);
    write_blob(output, project_root);
    output << files.size() << '\n';
    for (const auto& file : files) { write_blob(output, file.name); write_blob(output, file.content); }
    output.close();
    if (!output) {
        fs::remove(temporary, ec);
        result.error = "Could not write the snapshot.";
        return result;
    }
    fs::rename(temporary, final, ec);
    if (ec) {
        fs::remove(temporary, ec);
        result.error = "Could not finalize the snapshot.";
        return result;
    }
    result.success = true;
    result.created = true;
    result.metadata = {id, std::move(label), automatic, std::move(project_root),
                       word_count, files.size(), final.string()};
    if (!Prune(&result.error)) result.success = false;
    return result;
}

std::vector<SnapshotMetadata> SnapshotStore::List(std::string* error) const {
    if (error) error->clear();
    std::vector<SnapshotMetadata> snapshots;
    std::error_code ec;
    if (!fs::exists(directory_, ec)) return snapshots;
    for (fs::directory_iterator item(directory_, fs::directory_options::skip_permission_denied, ec), end;
         item != end; item.increment(ec)) {
        if (ec) { ec.clear(); continue; }
        if (!item->is_regular_file() || item->path().extension() != ".scsnap") continue;
        std::string read_error;
        const auto snapshot = read_snapshot(item->path(), &read_error);
        if (!snapshot) {
            if (error) *error = read_error;
            return {};
        }
        snapshots.push_back(snapshot->metadata);
    }
    std::sort(snapshots.begin(), snapshots.end(), [](const auto& left, const auto& right) {
        return left.id > right.id;
    });
    return snapshots;
}

std::optional<Snapshot> SnapshotStore::Load(std::uint64_t id, std::string* error) const {
    if (error) error->clear();
    return read_snapshot(fs::path(directory_) / (std::to_string(id) + ".scsnap"), error);
}

bool SnapshotStore::Prune(std::string* error) const {
    auto snapshots = List(error);
    if (error && !error->empty()) return false;
    std::error_code ec;
    for (std::size_t index = limit_; index < snapshots.size(); ++index) {
        fs::remove(snapshots[index].storage_path, ec);
        if (ec) { if (error) *error = "Could not prune an old snapshot."; return false; }
    }
    return true;
}

}  // namespace say_count
