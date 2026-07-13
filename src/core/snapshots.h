#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "core/parser.h"

namespace say_count {

struct SnapshotMetadata {
    std::uint64_t id = 0;
    std::string label;
    bool automatic = false;
    std::string project_root;
    std::size_t word_count = 0;
    std::size_t file_count = 0;
    std::string storage_path;
};

struct Snapshot {
    SnapshotMetadata metadata;
    std::vector<NamedScript> files;
};

struct SnapshotCreateResult {
    bool success = false;
    bool created = false;
    SnapshotMetadata metadata;
    std::string error;
};

enum class SnapshotFileStatus { Unchanged, Changed, Added, Removed };

struct SnapshotFileComparison {
    std::string name;
    SnapshotFileStatus status = SnapshotFileStatus::Unchanged;
    std::size_t current_words = 0;
    std::size_t snapshot_words = 0;
};

struct SnapshotComparison {
    std::vector<SnapshotFileComparison> files;
    std::size_t current_words = 0;
    std::size_t snapshot_words = 0;
    std::size_t changed_files = 0;
};

SnapshotComparison compare_snapshot(const std::vector<NamedScript>& current,
                                    const Snapshot& snapshot);

class SnapshotStore final {
public:
    explicit SnapshotStore(std::string directory, std::size_t limit = 50);

    SnapshotCreateResult Create(const std::vector<NamedScript>& files, std::string label,
                                bool automatic, std::string project_root,
                                std::size_t word_count);
    std::vector<SnapshotMetadata> List(std::string* error = nullptr) const;
    std::optional<Snapshot> Load(std::uint64_t id, std::string* error = nullptr) const;
    bool Prune(std::string* error = nullptr) const;

    const std::string& directory() const { return directory_; }
    std::size_t limit() const { return limit_; }

private:
    std::string directory_;
    std::size_t limit_ = 50;
};

}  // namespace say_count
