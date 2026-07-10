#include "core/parser.h"

#include "core/tokenizer.h"
#include "core/unicode_word_ranges.h"

#include <cctype>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace say_count {
namespace {

const std::unordered_set<std::string> ignored = {
    "define", "default", "image", "scene", "show", "hide", "play", "stop", "queue",
    "window", "with", "jump", "call", "return", "if", "elif", "else", "while", "for",
    "init", "label", "transform", "python", "screen", "style", "voice", "pause", "camera",
    "old", "new", "translate", "menu", "pass", "set", "zorder", "onlayer"};

bool word_codepoint(std::uint32_t point) {
    std::size_t low = 0;
    std::size_t high = std::size(unicode_data::word_ranges);
    while (low < high) {
        const auto middle = low + (high - low) / 2;
        const auto [first, last] = unicode_data::word_ranges[middle];
        if (point < first) high = middle;
        else if (point > last) low = middle + 1;
        else return true;
    }
    return false;
}

std::pair<std::uint32_t, std::size_t> decode_utf8(std::string_view text, std::size_t offset) {
    const auto first = static_cast<unsigned char>(text[offset]);
    if (first < 0x80) return {first, 1};
    std::size_t length = first < 0xE0 ? 2 : first < 0xF0 ? 3 : first < 0xF8 ? 4 : 0;
    if (!length || offset + length > text.size()) return {0, 1};
    std::uint32_t point = first & ((1u << (7 - length)) - 1);
    for (std::size_t i = 1; i < length; ++i) {
        const auto byte = static_cast<unsigned char>(text[offset + i]);
        if ((byte & 0xC0) != 0x80) return {0, 1};
        point = (point << 6) | (byte & 0x3F);
    }
    if (point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF) ||
        (length == 2 && point < 0x80) || (length == 3 && point < 0x800) || (length == 4 && point < 0x10000)) {
        return {0, 1};
    }
    return {point, length};
}

std::string json_escape(std::string_view value) {
    std::string out;
    for (unsigned char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(static_cast<char>(c));
        }
    }
    return out;
}

std::string label_name(const TokenizedLine& line) {
    auto value = line.subject;
    const auto colon = value.find(':');
    if (colon != std::string::npos) value.resize(colon);
    const auto paren = value.find('(');
    if (paren != std::string::npos) value.resize(paren);
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

void parse_characters(std::string_view script, std::map<std::string, std::string>& names,
                      std::map<std::string, std::string>& colors) {
    static const std::regex definition(
        R"(^\s*(?:(?:define|default)\s+)?([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?:renpy\.)?Character\s*\()",
        std::regex::ECMAScript);
    static const std::regex color(R"re(\bcolor\s*=\s*["'](#[0-9a-fA-F]{3,8})["'])re");
    std::size_t start = 0;
    while (start <= script.size()) {
        const auto newline = script.find('\n', start);
        auto raw = script.substr(start, newline == std::string_view::npos ? script.size() - start : newline - start);
        const auto token = tokenize_line(raw, 0);
        std::smatch match;
        if (std::regex_search(token.code, match, definition)) {
            const std::string alias = match[1].str();
            const auto arguments = token.code.substr(static_cast<std::size_t>(match.position() + match.length()));
            const auto argument_token = tokenize_line(arguments, 0);
            std::string display;
            if (!argument_token.quoted.empty()) display = clean_renpy_text(argument_token.quoted.front().text);
            std::string lowered = display;
            for (char& c : lowered) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (!display.empty() && lowered != "none") names[alias] = display;

            std::smatch color_match;
            if (std::regex_search(token.code, color_match, color)) {
                const std::string value = color_match[1].str();
                colors[alias] = value;
                // JS colors intentionally retain a nonempty `None` display key.
                if (!display.empty()) colors[display] = value;
            }
        }
        if (newline == std::string_view::npos) break;
        start = newline + 1;
    }
}

}  // namespace

std::string clean_renpy_text(std::string_view text) {
    std::string out;
    bool tag = false;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '{') { tag = true; out.push_back(' '); continue; }
        if (tag) { if (c == '}') tag = false; continue; }
        if (c == '[' || c == ']') { out.push_back(' '); continue; }
        if (c == '_' || c == '*' || c == '~' || c == '`') { out.push_back(' '); continue; }
        if (c == '\\' && i + 1 < text.size()) {
            const char next = text[i + 1];
            if (next == 'n') { out.push_back(' '); ++i; continue; }
            if (next == '\'' || next == '"') { out.push_back(next); ++i; continue; }
        }
        out.push_back(c);
    }
    const auto first = out.find_first_not_of(" \t\r\n");
    if (first == std::string::npos) return {};
    const auto last = out.find_last_not_of(" \t\r\n");
    return out.substr(first, last - first + 1);
}

std::size_t count_words(std::string_view text) {
    const std::string clean = clean_renpy_text(text);
    std::size_t count = 0;
    bool in_word = false;
    for (std::size_t i = 0; i < clean.size();) {
        const auto [point, length] = decode_utf8(clean, i);
        if (word_codepoint(point)) {
            if (!in_word) ++count;
            in_word = true;
        } else {
            const bool connector = point == '\'' || point == '-' || point == 0x2019;
            const auto next_offset = i + length;
            const bool followed_by_word = next_offset < clean.size() && word_codepoint(decode_utf8(clean, next_offset).first);
            if (!(connector && in_word && followed_by_word)) in_word = false;
        }
        i += length;
    }
    return count;
}

ScriptAnalysis analyze_with_characters(std::string_view script,
                                       const std::map<std::string, std::string>& names,
                                       const std::map<std::string, std::string>& colors,
                                       AnalysisOptions options) {
    ScriptAnalysis result;
    result.character_names = names;
    result.speaker_colors = colors;
    if (script.empty()) return result;
    std::string current_scene = "No label";
    std::string current_global;
    std::string last_speaker;
    std::size_t start = 0;
    std::size_t line_number = 1;
    while (start <= script.size()) {
        const auto newline = script.find('\n', start);
        auto raw = script.substr(start, newline == std::string_view::npos ? script.size() - start : newline - start);
        if (!raw.empty() && raw.back() == '\r') raw.remove_suffix(1);
        ++result.script_lines;
        const auto token = tokenize_line(raw, line_number);
        if (token.type == LineType::Label) {
            auto name = label_name(token);
            if (!name.empty() && name.front() == '.' && !current_global.empty()) name = current_global + name;
            else if (!name.empty() && name.front() != '.') current_global = name;
            current_scene = name;
        } else if (token.type == LineType::Dialogue || token.type == LineType::Narration) {
            const std::string alias = token.type == LineType::Narration ? "" : token.keyword;
            const bool menu_choice = token.type == LineType::Narration && !token.quoted.empty() &&
                token.quoted.front().begin == 0 && token.quoted.front().closed &&
                [&] {
                    auto suffix = std::string_view(token.code).substr(token.quoted.front().end + 1);
                    while (!suffix.empty() && std::isspace(static_cast<unsigned char>(suffix.front()))) suffix.remove_prefix(1);
                    return (!suffix.empty() && suffix.front() == ':') ||
                        (suffix.size() >= 2 && suffix.substr(0, 2) == "if" &&
                         (suffix.size() == 2 || std::isspace(static_cast<unsigned char>(suffix[2]))) &&
                         !suffix.empty() && suffix.back() == ':');
                }();
            if (!ignored.count(alias) && (!menu_choice || options.count_menu_choices)) {
                std::string combined;
                for (const auto& quote : token.quoted) {
                    if (!combined.empty()) combined.push_back(' ');
                    combined += quote.text;
                }
                const auto words = count_words(combined);
                if (words) {
                    std::string speaker = menu_choice ? "menu choice" : alias.empty() ? "narrator" : alias;
                    if (alias == "extend" && !last_speaker.empty()) speaker = last_speaker;
                    else if (const auto found = names.find(alias); found != names.end()) speaker = found->second;
                    if (alias != "extend" && !menu_choice) last_speaker = speaker;
                    result.counted.push_back({line_number, speaker, clean_renpy_text(combined), words,
                                              current_scene, menu_choice});
                    result.total_words += words;
                }
            }
        }
        if (newline == std::string_view::npos) break;
        start = newline + 1;
        ++line_number;
    }
    result.dialogue_lines = result.counted.size();
    return result;
}

ScriptAnalysis analyze_script(std::string_view script, AnalysisOptions options) {
    std::map<std::string, std::string> names;
    std::map<std::string, std::string> colors;
    parse_characters(script, names, colors);
    return analyze_with_characters(script, names, colors, options);
}

ScriptAnalysis analyze_project(const std::vector<NamedScript>& scripts, AnalysisOptions options) {
    std::map<std::string, std::string> names;
    std::map<std::string, std::string> colors;
    for (const auto& script : scripts) parse_characters(script.content, names, colors);
    ScriptAnalysis merged;
    merged.character_names = names;
    merged.speaker_colors = colors;
    for (const auto& script : scripts) {
        auto item = analyze_with_characters(script.content, names, colors, options);
        merged.total_words += item.total_words;
        merged.dialogue_lines += item.dialogue_lines;
        merged.script_lines += item.script_lines;
        merged.counted.insert(merged.counted.end(), item.counted.begin(), item.counted.end());
    }
    return merged;
}

std::string analysis_json(const ScriptAnalysis& analysis) {
    std::ostringstream out;
    out << "{\"totalWords\":" << analysis.total_words << ",\"dialogueLines\":" << analysis.dialogue_lines
        << ",\"scriptLines\":" << analysis.script_lines << ",\"counted\":[";
    for (std::size_t i = 0; i < analysis.counted.size(); ++i) {
        if (i) out << ',';
        const auto& item = analysis.counted[i];
        out << "{\"lineNumber\":" << item.line_number << ",\"speaker\":\"" << json_escape(item.speaker)
            << "\",\"text\":\"" << json_escape(item.text) << "\",\"words\":" << item.words
            << ",\"scene\":\"" << json_escape(item.scene) << "\",\"isMenuChoice\":"
            << (item.is_menu_choice ? "true" : "false") << "}";
    }
    out << "],\"characterNames\":{";
    std::size_t field = 0;
    for (const auto& [alias, name] : analysis.character_names) {
        if (field++) out << ',';
        out << '\"' << json_escape(alias) << "\":\"" << json_escape(name) << '\"';
    }
    out << "},\"speakerColors\":{";
    field = 0;
    for (const auto& [speaker, color] : analysis.speaker_colors) {
        if (field++) out << ',';
        out << '\"' << json_escape(speaker) << "\":\"" << json_escape(color) << '\"';
    }
    out << "}}";
    return out.str();
}

}  // namespace say_count
