#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct ContinuityNote {
    std::string id;
    std::string kind;
    std::string subject;
    std::string note;
    std::string file;
    std::size_t line = 0;
    long long created_ms = 0;
};

std::vector<ContinuityNote> filter_continuity_notes(
    const std::vector<ContinuityNote>& notes, std::string_view query,
    std::string_view kind = {});

class ContinuityStore {
public:
    explicit ContinuityStore(std::string path) : path_(std::move(path)) {}
    std::vector<ContinuityNote> Load(std::string* error = nullptr) const;
    bool Save(const std::vector<ContinuityNote>& notes, std::string* error = nullptr) const;
private:
    std::string path_;
};

}  // namespace say_count
