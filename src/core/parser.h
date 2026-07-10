#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct CountedLine {
    std::size_t line_number = 0;
    std::string speaker;
    std::string text;
    std::size_t words = 0;
    std::string scene;
};

struct ScriptAnalysis {
    std::size_t total_words = 0;
    std::size_t dialogue_lines = 0;
    std::size_t script_lines = 0;
    std::vector<CountedLine> counted;
};

std::string clean_renpy_text(std::string_view text);
std::size_t count_words(std::string_view text);
ScriptAnalysis analyze_script(std::string_view script);
std::string analysis_json(const ScriptAnalysis& analysis);

}  // namespace say_count
