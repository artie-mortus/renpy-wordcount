#pragma once

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/parser.h"

namespace say_count {

struct FindOptions {
    bool case_sensitive = false;
    bool use_regex = false;
    bool whole_word = false;
};

struct FindMatch {
    std::size_t position = 0;
    std::size_t length = 0;
};

struct FindResult {
    bool valid = true;
    std::vector<FindMatch> matches;
};

struct ReplaceResult {
    bool valid = true;
    std::string text;
    std::size_t count = 0;
};

struct ProjectFindMatch {
    std::size_t file_index = 0;
    std::string file_name;
    std::size_t line_number = 0;
    std::size_t column = 0;
    std::size_t position = 0;
    std::size_t length = 0;
    std::string preview;
};

struct ProjectFindResult {
    bool valid = true;
    std::vector<ProjectFindMatch> matches;
};

struct ProjectReplacePreview {
    std::size_t file_index = 0;
    std::string file_name;
    std::size_t count = 0;
};

struct ProjectReplaceResult {
    bool valid = true;
    std::vector<NamedScript> scripts;
    std::vector<std::size_t> per_file_counts;
    std::size_t count = 0;
};

FindResult find_matches(std::string_view text, std::string_view query,
                        FindOptions options = {});
ReplaceResult replace_all_matches(std::string_view text, std::string_view query,
                                  std::string_view replacement, FindOptions options = {});
ProjectFindResult find_project_matches(const std::vector<NamedScript>& scripts,
                                       std::string_view query, FindOptions options = {});
std::vector<ProjectReplacePreview> preview_project_replacement(
    const std::vector<NamedScript>& scripts, std::string_view query, FindOptions options = {});
ProjectReplaceResult replace_project_matches(
    const std::vector<NamedScript>& scripts, const std::vector<std::size_t>& selected_files,
    std::string_view query, std::string_view replacement, FindOptions options = {});

}  // namespace say_count
