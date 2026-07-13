#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <map>
#include <vector>

namespace say_count {

struct CountedLine {
    std::size_t line_number = 0;
    std::string speaker;
    std::string text;
    std::size_t words = 0;
    std::string scene;
    bool is_menu_choice = false;
    std::string raw;
    std::string alias;
    bool unknown_speaker = false;
    bool is_extend = false;
    std::string file;
};

struct ParserWarning {
    std::string type;
    std::size_t line_number = 0;
    std::string file;
    std::string message;
};

struct AggregateStats {
    std::string name;
    std::size_t words = 0;
    std::size_t lines = 0;
};

struct ScriptAnalysis {
    std::size_t total_words = 0;
    std::size_t dialogue_lines = 0;
    std::size_t script_lines = 0;
    double average_words = 0;
    std::size_t reading_minutes = 0;
    std::vector<AggregateStats> speakers;
    std::vector<AggregateStats> scenes;
    std::vector<CountedLine> counted;
    std::map<std::string, std::string> character_names;
    std::map<std::string, std::string> speaker_colors;
    std::vector<ParserWarning> warnings;
};

struct NamedScript {
    std::string name;
    std::string content;
};

struct AnalysisOptions {
    bool count_menu_choices = false;
    std::string file_name;
    std::size_t long_line_words = 35;
};

std::string clean_renpy_text(std::string_view text);
std::size_t count_words(std::string_view text);
ScriptAnalysis analyze_script(std::string_view script, AnalysisOptions options = {});
ScriptAnalysis analyze_project(const std::vector<NamedScript>& scripts, AnalysisOptions options = {});
std::string analysis_json(const ScriptAnalysis& analysis);

}  // namespace say_count
