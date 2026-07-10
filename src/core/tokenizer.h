#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

enum class LineType {
    Blank,
    Comment,
    Label,
    Dialogue,
    Narration,
    Define,
    Jump,
    Call,
    Menu,
    Python,
    Scene,
    Show,
    Play,
    Unknown,
    Malformed,
};

struct QuotedSegment {
    std::string text;
    std::size_t begin = 0;
    std::size_t end = 0;
    char quote = '\0';
    bool closed = false;
};

struct TokenizedLine {
    LineType type = LineType::Blank;
    std::size_t line_number = 0;
    std::size_t indentation = 0;
    std::string code;
    std::string comment;
    std::string keyword;
    std::string subject;
    std::vector<QuotedSegment> quoted;
};

TokenizedLine tokenize_line(std::string_view line, std::size_t line_number);

}  // namespace say_count
