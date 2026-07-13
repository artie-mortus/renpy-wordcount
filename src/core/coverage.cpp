#include "core/coverage.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <regex>
#include <sstream>

namespace say_count {
namespace {
namespace fs = std::filesystem;
constexpr std::string_view kMagic = "SAY-COUNT-MANUAL-COVERAGE-1";

std::string Decode(std::string_view value) {
    std::string out;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] != '\\' || i + 1 >= value.size()) { out.push_back(value[i]); continue; }
        const char c = value[++i];
        if (c == 'n') out.push_back('\n'); else if (c == 'r') out.push_back('\r'); else if (c == 't') out.push_back('\t');
        else if (c == 'b') out.push_back('\b'); else if (c == 'f') out.push_back('\f');
        else if (c == 'u' && i + 4 < value.size()) {
            unsigned code = 0; bool valid = true;
            for (int n = 0; n < 4; ++n) {
                const char d = value[++i]; code <<= 4;
                if (d >= '0' && d <= '9') code += d - '0'; else if (d >= 'a' && d <= 'f') code += d - 'a' + 10;
                else if (d >= 'A' && d <= 'F') code += d - 'A' + 10; else valid = false;
            }
            if (!valid) continue;
            if (code <= 0x7f) out.push_back(static_cast<char>(code));
            else if (code <= 0x7ff) { out.push_back(static_cast<char>(0xc0 | (code >> 6))); out.push_back(static_cast<char>(0x80 | (code & 0x3f))); }
            else { out.push_back(static_cast<char>(0xe0 | (code >> 12))); out.push_back(static_cast<char>(0x80 | ((code >> 6) & 0x3f))); out.push_back(static_cast<char>(0x80 | (code & 0x3f))); }
        } else out.push_back(c);
    }
    return out;
}

void Blob(std::ostream& out, std::string_view value) { out << value.size() << '\n'; out.write(value.data(), value.size()); out.put('\n'); }
bool BlobIn(std::istream& in, std::string& value) {
    std::string line; if (!std::getline(in, line)) return false; std::size_t length = 0;
    try { length = std::stoull(line); } catch (...) { return false; }
    if (length > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())) return false;
    value.resize(length); in.read(value.data(), static_cast<std::streamsize>(length));
    return in.gcount() == static_cast<std::streamsize>(length) && in.get() == '\n';
}
}

std::vector<std::string> collect_project_labels(const std::vector<NamedScript>& scripts) {
    const std::regex declaration(R"(^\s*label\s+(\.?[A-Za-z_][A-Za-z0-9_.]*)\s*(?:\([^)]*\))?\s*:)");
    std::set<std::string> labels;
    for (const auto& script : scripts) {
        std::string global;
        std::size_t start = 0;
        while (start <= script.content.size()) {
            const auto newline = script.content.find('\n', start);
            auto line = std::string_view(script.content).substr(start, newline == std::string::npos ? script.content.size() - start : newline - start);
            std::match_results<std::string_view::const_iterator> match;
            if (std::regex_search(line.begin(), line.end(), match, declaration)) {
                std::string name = match[1].str();
                if (!name.empty() && name.front() == '.') name = global.empty() ? name : global + name;
                else global = name;
                labels.insert(std::move(name));
            }
            if (newline == std::string::npos) break; start = newline + 1;
        }
    }
    return {labels.begin(), labels.end()};
}

std::string coverage_file_name(std::string_view project_root) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const unsigned char c : project_root) { hash ^= c; hash *= 1099511628211ULL; }
    std::ostringstream out; out << "renpy-coverage-" << std::hex << std::setw(16) << std::setfill('0') << hash << ".jsonl";
    return out.str();
}

std::vector<std::string> CoverageTail::Read(const std::string& path, std::string* error) {
    if (error) error->clear(); std::vector<std::string> labels; std::error_code ec;
    if (!fs::exists(path, ec)) return labels;
    const auto size = fs::file_size(path, ec); if (ec) { if (error) *error = "Could not inspect coverage output."; return {}; }
    if (size < offset_) Reset();
    std::ifstream input(path, std::ios::binary); if (!input) { if (error) *error = "Could not read coverage output."; return {}; }
    input.seekg(static_cast<std::streamoff>(offset_)); std::ostringstream new_data; new_data << input.rdbuf();
    const std::string bytes = new_data.str(); offset_ += bytes.size(); pending_ += bytes;
    const std::regex record(R"REGEX(^\s*\{\s*"label"\s*:\s*"((?:\\.|[^"\\])*)"\s*\}\s*$)REGEX");
    std::size_t start = 0;
    while (true) {
        const auto newline = pending_.find('\n', start); if (newline == std::string::npos) break;
        std::string_view line(pending_.data() + start, newline - start); if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        std::match_results<std::string_view::const_iterator> match;
        if (std::regex_match(line.begin(), line.end(), match, record)) labels.push_back(Decode(match[1].str()));
        start = newline + 1;
    }
    pending_.erase(0, start); return labels;
}

void CoverageTail::Reset() { offset_ = 0; pending_.clear(); }

std::map<std::string, std::set<std::string>> ManualCoverageStore::Load(std::string* error) const {
    if (error) error->clear(); std::map<std::string, std::set<std::string>> result; std::ifstream in(path_, std::ios::binary);
    if (!in) return result; std::string magic, count_line;
    if (!std::getline(in, magic) || magic != kMagic || !std::getline(in, count_line)) { if (error) *error = "Invalid manual coverage file."; return {}; }
    std::size_t projects = 0; try { projects = std::stoull(count_line); } catch (...) { if (error) *error = "Invalid manual coverage file."; return {}; }
    for (std::size_t p = 0; p < projects; ++p) {
        std::string project, labels_line; if (!BlobIn(in, project) || !std::getline(in, labels_line)) { if (error) *error = "Invalid manual coverage data."; return {}; }
        std::size_t count = 0; try { count = std::stoull(labels_line); } catch (...) { if (error) *error = "Invalid manual coverage data."; return {}; }
        for (std::size_t i = 0; i < count; ++i) { std::string label; if (!BlobIn(in, label)) { if (error) *error = "Invalid manual coverage data."; return {}; } result[project].insert(label); }
    }
    return result;
}

bool ManualCoverageStore::Save(const std::map<std::string, std::set<std::string>>& projects, std::string* error) const {
    if (error) error->clear(); std::error_code ec; fs::create_directories(fs::path(path_).parent_path(), ec);
    if (ec) { if (error) *error = "Could not create coverage directory."; return false; }
    const fs::path temporary = path_ + ".tmp"; std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
    out << kMagic << '\n' << projects.size() << '\n';
    for (const auto& [project, labels] : projects) { Blob(out, project); out << labels.size() << '\n'; for (const auto& label : labels) Blob(out, label); }
    out.close(); if (!out) { fs::remove(temporary, ec); if (error) *error = "Could not write manual coverage."; return false; }
    fs::rename(temporary, path_, ec); if (ec) { fs::remove(path_, ec); ec.clear(); fs::rename(temporary, path_, ec); }
    if (ec) { fs::remove(temporary, ec); if (error) *error = "Could not finalize manual coverage."; return false; } return true;
}

}  // namespace say_count
