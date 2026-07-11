#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_set>
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

enum class HighlightClass { Default, Comment, Keyword, Label, Speaker, String, Python, Statement };

struct HighlightSpan {
    std::size_t begin = 0;
    std::size_t end = 0;
    HighlightClass token_class = HighlightClass::Default;
};

std::vector<HighlightSpan> highlight_line(
    std::string_view line, const std::unordered_set<std::string>& speaker_aliases = {});

}  // namespace say_count
