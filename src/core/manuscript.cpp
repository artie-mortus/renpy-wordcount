#include "core/manuscript.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>

namespace say_count {
namespace {

std::string Trim(std::string value) {
    const auto whitespace = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), whitespace));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), whitespace).base(), value.end());
    return value;
}

bool StartsWith(std::string_view value, std::string_view prefix) {
    return value.size() >= prefix.size() && value.substr(0, prefix.size()) == prefix;
}

bool EndsWith(std::string_view value, std::string_view suffix) {
    return value.size() >= suffix.size() && value.substr(value.size() - suffix.size()) == suffix;
}

bool IsQuoted(std::string_view value) {
    return value.size() >= 2 &&
        ((value.front() == '"' && value.back() == '"') ||
         (StartsWith(value, "\xe2\x80\x9c") && EndsWith(value, "\xe2\x80\x9d")));
}

std::string Unquote(std::string value) {
    if (value.size() >= 2 && value.front() == '"' && value.back() == '"')
        return value.substr(1, value.size() - 2);
    if (StartsWith(value, "\xe2\x80\x9c") && EndsWith(value, "\xe2\x80\x9d"))
        return value.substr(3, value.size() - 6);
    return value;
}

std::string Identifier(std::string_view value, std::string_view fallback) {
    std::string result;
    bool separator = false;
    for (const unsigned char byte : value) {
        if (byte < 0x80 && std::isalnum(byte)) {
            if (separator && !result.empty()) result += '_';
            result += static_cast<char>(std::tolower(byte));
            separator = false;
        } else {
            separator = true;
        }
    }
    while (!result.empty() && result.back() == '_') result.pop_back();
    if (result.empty()) result = std::string(fallback);
    if (std::isdigit(static_cast<unsigned char>(result.front()))) result.insert(0, "character_");
    return result;
}

std::string Escape(std::string_view value) {
    std::string result;
    result.reserve(value.size() + 8);
    for (char character : value) {
        if (character == '\\' || character == '"') result += '\\';
        if (character == '[') result += '[';  // Ren'Py's literal opening bracket is [[.
        result += character;
    }
    return result;
}

bool IsSpeakerName(std::string_view value) {
    if (value.empty() || value.size() > 60) return false;
    bool has_letter = false;
    for (const unsigned char byte : value) {
        if (byte < 0x80 && std::isalpha(byte)) has_letter = true;
        if (byte < 0x80 && !(std::isalnum(byte) || byte == ' ' || byte == '_' || byte == '-' || byte == '\'' || byte == '.'))
            return false;
    }
    return has_letter || std::any_of(value.begin(), value.end(), [](unsigned char c) { return c >= 0x80; });
}

const std::vector<std::string>& SpeechVerbs() {
    static const std::vector<std::string> verbs{
        "said", "asked", "replied", "answered", "whispered", "shouted", "yelled",
        "murmured", "muttered", "cried", "called", "added", "continued"
    };
    return verbs;
}

bool ParseAttributed(std::string_view line, std::string* speaker, std::string* dialogue) {
    // Alice said, "Hello."
    for (const auto& verb : SpeechVerbs()) {
        const std::string marker = " " + verb + ", ";
        const auto found = line.find(marker);
        if (found != std::string_view::npos) {
            std::string name = Trim(std::string(line.substr(0, found)));
            std::string quote = Trim(std::string(line.substr(found + marker.size())));
            if (IsSpeakerName(name) && IsQuoted(quote)) {
                *speaker = std::move(name); *dialogue = Unquote(std::move(quote)); return true;
            }
        }
    }

    // "Hello," Alice said. Smart and straight quotes are both accepted.
    std::size_t quote_end = std::string_view::npos;
    std::size_t quote_bytes = 1;
    if (!line.empty() && line.front() == '"') quote_end = line.find('"', 1);
    else if (StartsWith(line, "\xe2\x80\x9c")) {
        quote_end = line.find("\xe2\x80\x9d", 3);
        quote_bytes = 3;
    }
    if (quote_end == std::string_view::npos) return false;
    const std::string quote = std::string(line.substr(0, quote_end + quote_bytes));
    std::string attribution = Trim(std::string(line.substr(quote_end + quote_bytes)));
    if (!attribution.empty() && attribution.front() == ',') attribution = Trim(attribution.substr(1));
    if (!attribution.empty() && attribution.back() == '.') attribution.pop_back();
    for (const auto& verb : SpeechVerbs()) {
        const std::string suffix = " " + verb;
        if (!EndsWith(attribution, suffix)) continue;
        std::string name = Trim(attribution.substr(0, attribution.size() - suffix.size()));
        if (IsSpeakerName(name)) {
            *speaker = std::move(name); *dialogue = Unquote(quote); return true;
        }
    }
    return false;
}

}  // namespace

ManuscriptConversion convert_manuscript_to_renpy(
    std::string_view manuscript, const ManuscriptOptions& options) {
    std::vector<std::string> lines;
    std::istringstream input{std::string(manuscript)};
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(Trim(std::move(line)));
    }

    struct Entry { std::string label; std::string speaker; std::string text; };
    std::vector<Entry> entries;
    std::vector<ManuscriptCharacter> characters;
    std::map<std::string, std::string> alias_by_name;
    std::set<std::string> used_aliases;
    auto alias_for = [&](const std::string& name) -> std::string {
        const auto existing = alias_by_name.find(name);
        if (existing != alias_by_name.end()) return existing->second;
        std::string base = Identifier(name, "character");
        std::string alias = base;
        for (std::size_t suffix = 2; used_aliases.count(alias); ++suffix)
            alias = base + "_" + std::to_string(suffix);
        used_aliases.insert(alias);
        alias_by_name.emplace(name, alias);
        characters.push_back({name, alias});
        return alias;
    };

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const auto& line = lines[index];
        if (line.empty()) continue;
        if (StartsWith(line, "::")) {
            const std::string label = Identifier(Trim(line.substr(2)), "scene");
            entries.push_back({label, {}, {}});
            continue;
        }

        std::string speaker, dialogue;
        const auto colon = line.find(':');
        if (IsQuoted(line)) {
            dialogue = Unquote(line);
        } else if (colon != std::string::npos && IsSpeakerName(Trim(line.substr(0, colon)))) {
            speaker = Trim(line.substr(0, colon));
            dialogue = Trim(line.substr(colon + 1));
            dialogue = Unquote(std::move(dialogue));
        } else if (ParseAttributed(line, &speaker, &dialogue)) {
            // Parsed above.
        } else if (IsSpeakerName(line) && index + 1 < lines.size() && IsQuoted(lines[index + 1])) {
            speaker = line;
            dialogue = Unquote(lines[++index]);
        } else {
            dialogue = Unquote(line);
        }
        if (!speaker.empty()) {
            entries.push_back({{}, alias_for(speaker), dialogue});
        } else {
            entries.push_back({{}, {}, dialogue});
        }
    }

    ManuscriptConversion result;
    result.characters = characters;
    std::ostringstream script;
    if (options.include_character_definitions && !characters.empty()) {
        for (const auto& character : characters)
            script << "define " << character.alias << " = Character(\"" << Escape(character.name) << "\")\n";
        script << '\n';
    }
    bool inside_label = false;
    if (!options.label.empty()) {
        script << "label " << Identifier(options.label, "start") << ":\n";
        inside_label = true;
    }
    for (const auto& entry : entries) {
        if (!entry.label.empty()) {
            if (script.tellp() > 0 && !EndsWith(script.str(), "\n\n")) script << '\n';
            script << "label " << entry.label << ":\n";
            inside_label = true;
            continue;
        }
        // A blank opening label means this is a snippet intended for an existing label.
        const char* indent = (inside_label || options.label.empty()) ? "    " : "";
        script << indent;
        if (!entry.speaker.empty()) {
            script << entry.speaker << ' ';
            ++result.dialogue_lines;
        } else {
            ++result.narration_lines;
        }
        script << '"' << Escape(entry.text) << "\"\n";
    }
    result.script = script.str();
    return result;
}

}  // namespace say_count
