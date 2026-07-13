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

std::string voice_tracking_csv(const std::vector<VoiceLine>& rows,
                               const std::map<std::string, VoiceEntry>& entries) {
    auto quote = [](std::string_view value) {
        std::string result = "\"";
        for (const char character : value) { result += character; if (character == '\"') result += '\"'; }
        return result + '\"';
    };
    std::ostringstream output;
    output << "id,file,line,speaker,text,status,voice_file,notes\n";
    for (const auto& row : voice_script_rows(rows)) {
        const auto found = entries.find(row.id);
        const VoiceEntry entry = found == entries.end() ? VoiceEntry{} : found->second;
        output << quote(row.id) << ',' << quote(row.file) << ',' << quote(std::to_string(row.line)) << ','
               << quote(row.speaker) << ',' << quote(row.text) << ','
               << quote(voice_status_name(entry.status)) << ',' << quote(entry.voice_file) << ','
               << quote(entry.notes) << '\n';
    }
    return output.str();
}

std::string voice_script_html(const std::vector<VoiceLine>& rows,
                              const std::map<std::string, VoiceEntry>& entries,
                              std::string_view speaker, bool include_source) {
    auto escape = [](std::string_view value) {
        std::string result;
        for (const char character : value) {
            switch (character) {
                case '&': result += "&amp;"; break;
                case '<': result += "&lt;"; break;
                case '>': result += "&gt;"; break;
                case '\"': result += "&quot;"; break;
                case '\'': result += "&#39;"; break;
                default: result += character;
            }
        }
        return result;
    };
    const auto selected = voice_script_rows(rows, speaker);
    const std::string title = speaker.empty() ? "Full Cast — Voice Actor Script"
                                               : std::string(speaker) + " — Voice Actor Script";
    std::ostringstream output;
    output << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width\"><title>"
           << escape(title) << "</title><style>body{max-width:800px;margin:auto;padding:2rem;font:17px Georgia,serif;color:#191713}h1{border-bottom:3px double #8b6b31;padding-bottom:1rem}h2{margin-top:2rem;border-bottom:1px solid #cabd9e}.line{display:grid;grid-template-columns:2rem 1fr;gap:.8rem;padding:1rem 0;border-bottom:1px solid #ded4bd;break-inside:avoid}.speaker,.meta{font:12px ui-monospace,monospace}.speaker{font-weight:bold;text-transform:uppercase}.meta{color:#6c6253}@media print{body{padding:0}}</style></head><body><h1>"
           << escape(title) << "</h1><p>" << selected.size() << " lines</p>";
    std::string scene;
    for (std::size_t index = 0; index < selected.size(); ++index) {
        const auto& row = selected[index];
        const std::string next_scene = row.file + ":" + row.label;
        if (next_scene != scene) {
            scene = next_scene;
            output << "<h2>" << escape(row.label) << "</h2>";
            if (include_source) output << "<p class=\"meta\">" << escape(row.file) << "</p>";
        }
        const auto found = entries.find(row.id);
        const VoiceEntry entry = found == entries.end() ? VoiceEntry{} : found->second;
        output << "<article class=\"line\"><div>" << index + 1 << "</div><div><div class=\"speaker\">"
               << escape(row.speaker) << "</div><p>" << escape(row.text) << "</p><p class=\"meta\">"
               << escape(voice_status_name(entry.status));
        if (!entry.voice_file.empty()) output << " · Voice file: " << escape(entry.voice_file);
        if (!entry.notes.empty()) output << " · " << escape(entry.notes);
        if (include_source) output << " · Source " << escape(row.file) << ':' << row.line;
        output << "</p></div></article>";
    }
    return output.str() + "</body></html>\n";
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
