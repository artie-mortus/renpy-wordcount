#include "core/voice.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace say_count {
namespace {

std::string Hex(std::string_view value) {
    constexpr char digits[] = "0123456789abcdef";
    std::string result;
    result.reserve(value.size() * 2);
    for (const unsigned char byte : value) {
        result += digits[byte >> 4];
        result += digits[byte & 15];
    }
    return result;
}

std::optional<std::string> Unhex(std::string_view value) {
    if (value.size() % 2) return std::nullopt;
    auto digit = [](char value) -> int {
        if (value >= '0' && value <= '9') return value - '0';
        if (value >= 'a' && value <= 'f') return value - 'a' + 10;
        if (value >= 'A' && value <= 'F') return value - 'A' + 10;
        return -1;
    };
    std::string result;
    for (std::size_t index = 0; index < value.size(); index += 2) {
        const int high = digit(value[index]), low = digit(value[index + 1]);
        if (high < 0 || low < 0) return std::nullopt;
        result += static_cast<char>((high << 4) | low);
    }
    return result;
}

}  // namespace

std::vector<VoiceLine> parse_voice_dialogue(const std::vector<NamedScript>& scripts) {
    static const std::regex dialogue(R"re(^\s*(?:(\w+)\s+)?(?:\w+\s+)*"(.+)"\s*$)re");
    static const std::regex label_pattern(R"(^\s*label\s+([A-Za-z_]\w*))");
    static const std::regex ignored(R"(^\s*(?:define|default|image|voice|play|queue|stop|scene|show|hide|jump|call|return|window|with)\b)");
    const auto project = analyze_project(scripts);
    std::vector<VoiceLine> rows;
    for (const auto& script : scripts) {
        std::string label = "Start";
        std::string previous_character;
        std::size_t line_number = 1;
        std::size_t offset = 0;
        while (offset <= script.content.size()) {
            const auto newline = script.content.find('\n', offset);
            const auto length = newline == std::string::npos ? script.content.size() - offset : newline - offset;
            std::string line = script.content.substr(offset, length);
            if (!line.empty() && line.back() == '\r') line.pop_back();
            std::smatch match;
            if (std::regex_search(line, match, label_pattern)) label = match[1].str();
            if (!std::regex_search(line, ignored) && std::regex_match(line, match, dialogue)) {
                std::string alias = match[1].matched ? match[1].str() : std::string{};
                if (alias == "extend") alias = previous_character;
                const auto character = project.character_names.find(alias);
                const bool is_character = !alias.empty() && character != project.character_names.end();
                if (is_character) previous_character = alias;
                rows.push_back({
                    script.name + ":" + std::to_string(line_number), script.name, line_number,
                    label, is_character ? character->second : alias.empty() ? "Narrator" : alias,
                    alias.empty(), is_character, match[2].str(),
                });
            }
            if (newline == std::string::npos) break;
            offset = newline + 1;
            ++line_number;
        }
    }
    return rows;
}

std::vector<VoiceLine> voice_script_rows(const std::vector<VoiceLine>& rows,
                                         std::string_view speaker) {
    std::vector<VoiceLine> result;
    for (const auto& row : rows) {
        if (!(row.character || row.narration)) continue;
        if (!speaker.empty() && row.speaker != speaker) continue;
        result.push_back(row);
    }
    return result;
}

std::string voice_status_name(VoiceStatus status) {
    switch (status) {
        case VoiceStatus::recorded: return "recorded";
        case VoiceStatus::retake: return "retake";
        case VoiceStatus::approved: return "approved";
        default: return "unrecorded";
    }
}

std::optional<VoiceStatus> parse_voice_status(std::string_view status) {
    if (status == "unrecorded") return VoiceStatus::unrecorded;
    if (status == "recorded") return VoiceStatus::recorded;
    if (status == "retake") return VoiceStatus::retake;
    if (status == "approved") return VoiceStatus::approved;
    return std::nullopt;
}

std::map<std::string, VoiceEntry> VoiceStore::Load(std::string* error) const {
    std::map<std::string, VoiceEntry> result;
    std::ifstream input(path_, std::ios::binary);
    if (!input) return result;
    std::string line;
    if (!std::getline(input, line) || line != "SAYCOUNT_VOICE_V1") {
        if (error) *error = "Voice tracker file has an unsupported format.";
        return {};
    }
    while (std::getline(input, line)) {
        std::vector<std::string_view> fields;
        std::size_t start = 0;
        for (std::size_t index = 0; index <= line.size(); ++index) {
            if (index == line.size() || line[index] == '\t') {
                fields.emplace_back(line.data() + start, index - start);
                start = index + 1;
            }
        }
        if (fields.size() != 4) continue;
        const auto id = Unhex(fields[0]), status_text = Unhex(fields[1]);
        const auto file = Unhex(fields[2]), notes = Unhex(fields[3]);
        const auto status = status_text ? parse_voice_status(*status_text) : std::nullopt;
        if (id && status && file && notes) result[*id] = {*status, *file, *notes};
    }
    return result;
}

bool VoiceStore::Save(const std::map<std::string, VoiceEntry>& entries, std::string* error) const {
    const std::filesystem::path path(path_);
    std::error_code ec;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), ec);
    const std::string temporary = path_ + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) { if (error) *error = "Could not write voice tracker state."; return false; }
    output << "SAYCOUNT_VOICE_V1\n";
    for (const auto& [id, entry] : entries)
        output << Hex(id) << '\t' << Hex(voice_status_name(entry.status)) << '\t'
               << Hex(entry.voice_file) << '\t' << Hex(entry.notes) << '\n';
    output.close();
    if (!output) { std::remove(temporary.c_str()); if (error) *error = "Could not finish voice tracker state."; return false; }
    std::filesystem::rename(temporary, path, ec);
    if (ec) { std::filesystem::remove(path, ec); ec.clear(); std::filesystem::rename(temporary, path, ec); }
    if (ec) { std::remove(temporary.c_str()); if (error) *error = "Could not replace voice tracker state."; return false; }
    return true;
}

}  // namespace say_count
