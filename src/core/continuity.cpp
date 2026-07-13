#include "core/continuity.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace say_count {
namespace {

std::string Lower(std::string value) {
    for (char& character : value) {
        const auto byte = static_cast<unsigned char>(character);
        if (byte < 128) character = static_cast<char>(std::tolower(byte));
    }
    return value;
}

std::string Hex(std::string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string out;
    for (unsigned char byte : value) { out += digits[byte >> 4]; out += digits[byte & 15]; }
    return out;
}

std::optional<std::string> Unhex(std::string_view value) {
    if (value.size() % 2) return std::nullopt;
    auto digit = [](char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; };
    std::string out;
    for (std::size_t i = 0; i < value.size(); i += 2) {
        const int high = digit(value[i]), low = digit(value[i + 1]);
        if (high < 0 || low < 0) return std::optional<std::string>{};
        out += static_cast<char>((high << 4) | low);
    }
    return out;
}

}  // namespace

std::vector<ContinuityNote> filter_continuity_notes(
    const std::vector<ContinuityNote>& notes, std::string_view query,
    std::string_view kind) {
    const std::string needle = Lower(std::string(query));
    std::vector<ContinuityNote> result;
    for (const auto& item : notes) {
        if (!kind.empty() && item.kind != kind) continue;
        const std::string haystack = Lower(item.subject + "\n" + item.note + "\n" + item.file);
        if (!needle.empty() && haystack.find(needle) == std::string::npos) continue;
        result.push_back(item);
    }
    return result;
}

std::vector<ContinuityNote> ContinuityStore::Load(std::string* error) const {
    std::ifstream input(path_, std::ios::binary);
    if (!input) return {};
    std::string line;
    if (!std::getline(input, line) || line != "SAYCOUNT_CONTINUITY_V1") {
        if (error) *error = "Continuity file has an unsupported format.";
        return {};
    }
    std::vector<ContinuityNote> result;
    while (std::getline(input, line)) {
        std::vector<std::string_view> fields;
        std::size_t start = 0;
        for (std::size_t i = 0; i <= line.size(); ++i) if (i == line.size() || line[i] == '\t') {
            fields.emplace_back(line.data() + start, i - start); start = i + 1;
        }
        if (fields.size() != 7) continue;
        const auto id = Unhex(fields[0]), kind = Unhex(fields[1]), subject = Unhex(fields[2]);
        const auto note = Unhex(fields[3]), file = Unhex(fields[4]);
        if (!id || !kind || !subject || !note || !file) continue;
        try {
            result.push_back({*id, *kind, *subject, *note, *file,
                              static_cast<std::size_t>(std::stoull(std::string(fields[5]))),
                              std::stoll(std::string(fields[6]))});
        } catch (...) {}
    }
    return result;
}

bool ContinuityStore::Save(const std::vector<ContinuityNote>& notes, std::string* error) const {
    const std::filesystem::path path(path_);
    std::error_code ec;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), ec);
    const std::string temporary = path_ + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) { if (error) *error = "Could not write continuity notes."; return false; }
    output << "SAYCOUNT_CONTINUITY_V1\n";
    for (const auto& item : notes)
        output << Hex(item.id) << '\t' << Hex(item.kind) << '\t' << Hex(item.subject) << '\t'
               << Hex(item.note) << '\t' << Hex(item.file) << '\t' << item.line << '\t'
               << item.created_ms << '\n';
    output.close();
    if (!output) { std::remove(temporary.c_str()); if (error) *error = "Could not finish continuity notes."; return false; }
    std::filesystem::rename(temporary, path, ec);
    if (ec) { std::filesystem::remove(path, ec); ec.clear(); std::filesystem::rename(temporary, path, ec); }
    if (ec) { std::remove(temporary.c_str()); if (error) *error = "Could not replace continuity notes."; return false; }
    return true;
}

}  // namespace say_count
