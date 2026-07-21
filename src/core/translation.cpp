#include "core/translation.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <regex>
#include <unordered_map>

#include "core/renpy.h"
#include "core/voice.h"

namespace say_count {

std::string normalize_translation_source_path(std::string path) {
    std::replace(path.begin(), path.end(), '\\', '/');
    while (path.rfind("./", 0) == 0) path.erase(0, 2);
    if (path.rfind("game/", 0) == 0) path.erase(0, 5);
    return path;
}

std::map<std::string, std::vector<TranslationEntry>> group_translation_entries(
    const std::vector<TranslationEntry>& entries) {
    std::map<std::string, std::vector<TranslationEntry>> result;
    for (const auto& entry : entries) result[entry.file].push_back(entry);
    return result;
}

TranslationDashboard scan_translation_dashboard(
    const std::string& game_directory, std::string language,
    const std::vector<NamedScript>& source_scripts, std::size_t limit) {
    TranslationDashboard result;
    result.language = std::move(language);
    if (!valid_renpy_language(result.language)) return result;
    const std::filesystem::path root = std::filesystem::path(game_directory) / "tl" / result.language;
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) return result;
    result.ready = true;

    std::unordered_map<std::string, VoiceLine> context;
    for (const auto& row : parse_voice_dialogue(source_scripts))
        context[normalize_translation_source_path(row.file) + ":" + std::to_string(row.line)] = row;

    static const std::regex marker_pattern(R"(^\s*#\s*game/(.+):(\d+)\s*$)");
    static const std::regex old_pattern(R"re(^\s*old\s+"(.*)"\s*$)re");
    static const std::regex new_pattern(R"re(^\s*new\s+"(.*)"\s*$)re");
    static const std::regex dialogue_pattern(R"re(^\s*(?:[A-Za-z_]\w*(?:\s+[A-Za-z_]\w*)*\s+)?"(.*)"\s*$)re");
    std::vector<std::filesystem::path> files;
    for (std::filesystem::recursive_directory_iterator it(root, ec), end; !ec && it != end; it.increment(ec)) {
        if (it->is_regular_file(ec) && it->path().extension() == ".rpy") files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    for (const auto& path : files) {
        std::ifstream input(path);
        if (!input) continue;
        std::string source_file, source_text, line;
        std::size_t source_line = 0, translation_line = 0;
        while (std::getline(input, line)) {
            ++translation_line;
            std::smatch match;
            if (std::regex_match(line, match, marker_pattern)) {
                source_file = normalize_translation_source_path(match[1].str());
                source_line = static_cast<std::size_t>(std::stoull(match[2].str()));
                source_text.clear();
                continue;
            }
            const auto first = line.find_first_not_of(" \t");
            if (first != std::string::npos && line[first] == '#' && line.find('"', first) != std::string::npos) {
                source_text = line.substr(first + 1);
                const auto text_first = source_text.find_first_not_of(" \t");
                if (text_first != std::string::npos) source_text.erase(0, text_first);
                continue;
            }
            if (std::regex_match(line, match, old_pattern)) { source_text = match[1].str(); continue; }
            const bool translated = std::regex_match(line, match, new_pattern) ||
                                    std::regex_match(line, match, dialogue_pattern);
            if (!translated || source_file.empty() || !source_line) continue;
            if (match[1].str().empty()) {
                TranslationEntry entry{source_file, source_line, source_text, {}, {}, {}, {}, 0};
                entry.translation_file = std::filesystem::relative(path, game_directory, ec).generic_string();
                entry.translation_line = translation_line;
                const auto found = context.find(source_file + ":" + std::to_string(source_line));
                if (found != context.end()) {
                    entry.source_file = found->second.file;
                    entry.speaker = found->second.speaker;
                    entry.label = found->second.label;
                    if (entry.text.empty()) entry.text = found->second.text;
                }
                if (entry.text.empty()) entry.text = "Source string";
                if (result.missing.size() < limit) result.missing.push_back(std::move(entry));
                else result.truncated = true;
            }
            source_file.clear(); source_line = 0; source_text.clear();
        }
    }
    return result;
}

}  // namespace say_count
