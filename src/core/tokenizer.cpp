#include "core/tokenizer.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace say_count {
namespace {

bool ascii_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\f' || c == '\v';
}

std::string_view trim(std::string_view value) {
    while (!value.empty() && ascii_space(value.front())) value.remove_prefix(1);
    while (!value.empty() && ascii_space(value.back())) value.remove_suffix(1);
    return value;
}

bool identifier_start(char c) {
    const auto byte = static_cast<unsigned char>(c);
    return std::isalpha(byte) != 0 || c == '_';
}

bool identifier_continue(char c) {
    const auto byte = static_cast<unsigned char>(c);
    return std::isalnum(byte) != 0 || c == '_' || c == '.';
}

std::pair<std::string, std::string_view> take_word(std::string_view value) {
    value = trim(value);
    if (value.empty() || !identifier_start(value.front())) return {{}, value};
    std::size_t length = 1;
    while (length < value.size() && identifier_continue(value[length])) ++length;
    return {std::string(value.substr(0, length)), trim(value.substr(length))};
}

bool is_statement(std::string_view value, std::string_view word) {
    if (value.substr(0, word.size()) != word) return false;
    return value.size() == word.size() || ascii_space(value[word.size()]) || value[word.size()] == ':';
}

bool ends_with_colon(std::string_view value) {
    value = trim(value);
    return !value.empty() && value.back() == ':';
}

std::size_t comment_start(std::string_view line) {
    char quote = '\0';
    bool escaped = false;
    for (std::size_t i = 0; i < line.size(); ++i) {
        const char c = line[i];
        if (escaped) {
            escaped = false;
        } else if (c == '\\') {
            escaped = true;
        } else if (quote == '\0' && (c == '\'' || c == '"')) {
            quote = c;
        } else if (c == quote) {
            quote = '\0';
        } else if (quote == '\0' && c == '#') {
            return i;
        }
    }
    return std::string_view::npos;
}

std::vector<QuotedSegment> quoted_segments(std::string_view code) {
    std::vector<QuotedSegment> result;
    for (std::size_t i = 0; i < code.size();) {
        if (code[i] != '\'' && code[i] != '"') {
            ++i;
            continue;
        }
        QuotedSegment segment;
        segment.begin = i;
        segment.quote = code[i++];
        bool escaped = false;
        for (; i < code.size(); ++i) {
            const char c = code[i];
            if (escaped) {
                segment.text.push_back(c);
                escaped = false;
            } else if (c == '\\') {
                segment.text.push_back(c);
                escaped = true;
            } else if (c == segment.quote) {
                segment.end = i;
                segment.closed = true;
                ++i;
                break;
            } else {
                segment.text.push_back(c);
            }
        }
        if (!segment.closed) segment.end = code.size();
        result.push_back(std::move(segment));
    }
    return result;
}

}  // namespace

TokenizedLine tokenize_line(std::string_view line, std::size_t line_number) {
    TokenizedLine result;
    result.line_number = line_number;
    for (char c : line) {
        if (c == ' ') ++result.indentation;
        else if (c == '\t') result.indentation += 4;
        else break;
    }

    const auto hash = comment_start(line);
    const auto code_part = hash == std::string_view::npos ? line : line.substr(0, hash);
    result.code = std::string(trim(code_part));
    if (hash != std::string_view::npos) result.comment = std::string(line.substr(hash));

    if (result.code.empty()) {
        result.type = result.comment.empty() ? LineType::Blank : LineType::Comment;
        return result;
    }

    result.quoted = quoted_segments(result.code);
    if (std::any_of(result.quoted.begin(), result.quoted.end(),
                    [](const QuotedSegment& item) { return !item.closed; })) {
        result.type = LineType::Malformed;
        return result;
    }

    const std::string_view code = result.code;
    auto [word, rest] = take_word(code);
    result.keyword = word;
    result.subject = std::string(rest);

    if (code.front() == '$' || is_statement(code, "python") ||
        (is_statement(code, "init") && rest.substr(0, 6) == "python")) {
        result.type = LineType::Python;
    } else if (word == "label") {
        result.type = !rest.empty() && ends_with_colon(rest) ? LineType::Label : LineType::Malformed;
    } else if (word == "define" || word == "default" || code.find("Character") != std::string_view::npos) {
        result.type = code.find('=') != std::string_view::npos ? LineType::Define : LineType::Malformed;
    } else if (word == "jump") {
        result.type = rest.empty() ? LineType::Malformed : LineType::Jump;
    } else if (word == "call") {
        result.type = rest.empty() ? LineType::Malformed : LineType::Call;
    } else if (word == "menu") {
        result.type = ends_with_colon(code) ? LineType::Menu : LineType::Malformed;
    } else if (word == "scene") {
        result.type = rest.empty() ? LineType::Malformed : LineType::Scene;
    } else if (word == "show") {
        result.type = rest.empty() ? LineType::Malformed : LineType::Show;
    } else if (word == "play") {
        result.type = rest.empty() ? LineType::Malformed : LineType::Play;
    } else if (!result.quoted.empty()) {
        result.type = result.quoted.front().begin == 0 ? LineType::Narration : LineType::Dialogue;
    } else {
        result.type = LineType::Unknown;
    }
    return result;
}

}  // namespace say_count
