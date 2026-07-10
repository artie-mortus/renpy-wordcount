#include "core/parser.h"

#include "core/tokenizer.h"
#include "core/unicode_word_ranges.h"

#include <cctype>
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

ScriptAnalysis analyze_script(std::string_view script) {
    ScriptAnalysis result;
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
            if (!ignored.count(alias)) {
                std::string combined;
                for (const auto& quote : token.quoted) {
                    if (!combined.empty()) combined.push_back(' ');
                    combined += quote.text;
                }
                const auto words = count_words(combined);
                if (words) {
                    std::string speaker = alias.empty() ? "narrator" : alias;
                    if (alias == "extend" && !last_speaker.empty()) speaker = last_speaker;
                    if (alias != "extend") last_speaker = speaker;
                    result.counted.push_back({line_number, speaker, clean_renpy_text(combined), words, current_scene});
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

std::string analysis_json(const ScriptAnalysis& analysis) {
    std::ostringstream out;
    out << "{\"totalWords\":" << analysis.total_words << ",\"dialogueLines\":" << analysis.dialogue_lines
        << ",\"scriptLines\":" << analysis.script_lines << ",\"counted\":[";
    for (std::size_t i = 0; i < analysis.counted.size(); ++i) {
        if (i) out << ',';
        const auto& item = analysis.counted[i];
        out << "{\"lineNumber\":" << item.line_number << ",\"speaker\":\"" << json_escape(item.speaker)
            << "\",\"text\":\"" << json_escape(item.text) << "\",\"words\":" << item.words
            << ",\"scene\":\"" << json_escape(item.scene) << "\"}";
    }
    out << "]}";
    return out.str();
}

}  // namespace say_count
