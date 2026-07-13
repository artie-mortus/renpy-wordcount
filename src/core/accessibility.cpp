#include "core/accessibility.h"

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <regex>

namespace say_count {
namespace {

std::vector<std::string> Lines(const std::string& text) {
    std::vector<std::string> result;
    std::size_t start = 0;
    while (start <= text.size()) {
        const auto newline = text.find('\n', start);
        result.push_back(text.substr(start, newline == std::string::npos ? text.size() - start : newline - start));
        if (!result.back().empty() && result.back().back() == '\r') result.back().pop_back();
        if (newline == std::string::npos) break;
        start = newline + 1;
    }
    return result;
}

}  // namespace

std::vector<AccessibilityIssue> audit_accessibility(
    const std::vector<NamedScript>& scripts, const std::set<std::string>& acknowledged) {
    std::vector<AccessibilityIssue> result;
    auto add = [&](AccessibilitySeverity severity, AccessibilityKind kind,
                   const std::string& file, std::size_t line, std::string message) {
        const std::string id = file + ":" + std::to_string(line) + ":" + std::to_string(static_cast<int>(kind));
        result.push_back({id, severity, kind, file, line, std::move(message), acknowledged.count(id) != 0});
    };
    const std::regex alt_pattern(R"(^\s*(?:alt|tooltip)\s+)");
    const std::regex event_alt_pattern(R"re(^\s*alt\s+")re");
    for (const auto& script : scripts) {
        const auto lines = Lines(script.content);
        for (std::size_t index = 0; index < lines.size(); ++index) {
            const auto& line = lines[index];
            if (line.find("imagebutton") != std::string::npos) {
                bool described = false;
                for (std::size_t look = index; look < std::min(lines.size(), index + 8); ++look)
                    if (std::regex_search(lines[look], alt_pattern)) described = true;
                if (!described) add(AccessibilitySeverity::warning, AccessibilityKind::image_description,
                    script.name, index + 1, "imagebutton has no nearby alt or tooltip text.");
            }
            if (std::regex_search(line, std::regex(R"re(^\s*textbutton\s+")re")) &&
                line.find("_(") == std::string::npos) {
                add(AccessibilitySeverity::warning, AccessibilityKind::translation, script.name,
                    index + 1, "textbutton text is not marked for translation.");
            }
            if (line.find("Character") != std::string::npos && line.find("what_italic") != std::string::npos &&
                line.find("True") != std::string::npos && line.find("what_alt") == std::string::npos) {
                add(AccessibilitySeverity::warning, AccessibilityKind::visual_voice, script.name,
                    index + 1, "italic-only character voice has no what_alt description.");
            }
            if (std::regex_search(line, std::regex(R"(^\s*show\s+)")) &&
                std::regex_search(line, std::regex("(?:cg|event|illustration)", std::regex::icase))) {
                bool described = false;
                for (std::size_t look = index + 1; look < std::min(lines.size(), index + 5); ++look)
                    if (std::regex_search(lines[look], event_alt_pattern)) described = true;
                if (!described) add(AccessibilitySeverity::notice, AccessibilityKind::event_description,
                    script.name, index + 1, "event image may need descriptive alt narration.");
            }
        }
    }
    return result;
}

std::set<std::string> AccessibilityAcknowledgementStore::Load() const {
    std::ifstream input(path_);
    std::set<std::string> result;
    std::string line;
    if (!std::getline(input, line) || line != "SAYCOUNT_ACCESSIBILITY_V1") return result;
    while (std::getline(input, line)) if (!line.empty()) result.insert(line);
    return result;
}

bool AccessibilityAcknowledgementStore::Save(const std::set<std::string>& ids,
                                               std::string* error) const {
    const std::filesystem::path path(path_);
    std::error_code ec;
    if (!path.parent_path().empty()) std::filesystem::create_directories(path.parent_path(), ec);
    const std::string temporary = path_ + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) { if (error) *error = "Could not write accessibility acknowledgements."; return false; }
    output << "SAYCOUNT_ACCESSIBILITY_V1\n";
    for (const auto& id : ids) if (id.find('\n') == std::string::npos) output << id << '\n';
    output.close();
    std::filesystem::rename(temporary, path, ec);
    if (ec) { std::filesystem::remove(path, ec); ec.clear(); std::filesystem::rename(temporary, path, ec); }
    if (ec) { std::remove(temporary.c_str()); if (error) *error = "Could not replace accessibility acknowledgements."; return false; }
    return true;
}

}  // namespace say_count
