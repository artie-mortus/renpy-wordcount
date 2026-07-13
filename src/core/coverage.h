#pragma once

#include "core/parser.h"

#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace say_count {

std::vector<std::string> collect_project_labels(const std::vector<NamedScript>& scripts);
std::string coverage_file_name(std::string_view project_root);

class CoverageTail final {
public:
    std::vector<std::string> Read(const std::string& path, std::string* error = nullptr);
    void Reset();
private:
    std::uintmax_t offset_ = 0;
    std::string pending_;
};

class ManualCoverageStore final {
public:
    explicit ManualCoverageStore(std::string path) : path_(std::move(path)) {}
    std::map<std::string, std::set<std::string>> Load(std::string* error = nullptr) const;
    bool Save(const std::map<std::string, std::set<std::string>>& projects,
              std::string* error = nullptr) const;
private:
    std::string path_;
};

}  // namespace say_count
