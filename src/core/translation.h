#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/parser.h"

namespace say_count {

struct TranslationEntry {
    TranslationEntry() = default;
    TranslationEntry(std::string file_value, std::size_t line_value = 0,
                     std::string text_value = {}, std::string source_file_value = {},
                     std::string speaker_value = {}, std::string label_value = {},
                     std::string translation_file_value = {}, std::size_t translation_line_value = 0)
        : file(std::move(file_value)), line(line_value), text(std::move(text_value)),
          source_file(std::move(source_file_value)), speaker(std::move(speaker_value)),
          label(std::move(label_value)), translation_file(std::move(translation_file_value)),
          translation_line(translation_line_value) {}

    std::string file;
    std::size_t line = 0;
    std::string text;
    std::string source_file;
    std::string speaker;
    std::string label;
    std::string translation_file;
    std::size_t translation_line = 0;
};

struct TranslationDashboard {
    std::string language;
    bool ready = false;
    bool truncated = false;
    std::vector<TranslationEntry> missing;
};

std::string normalize_translation_source_path(std::string path);
std::map<std::string, std::vector<TranslationEntry>> group_translation_entries(
    const std::vector<TranslationEntry>& entries);
TranslationDashboard scan_translation_dashboard(
    const std::string& game_directory, std::string language,
    const std::vector<NamedScript>& source_scripts, std::size_t limit = 2000);

}  // namespace say_count
