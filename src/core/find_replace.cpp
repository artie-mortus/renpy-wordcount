#include "core/find_replace.h"

#include <algorithm>
#include <cctype>
#include <regex>
#include <unordered_set>

namespace say_count {
namespace {

bool is_word_byte(char value) {
    const auto byte = static_cast<unsigned char>(value);
    return std::isalnum(byte) != 0 || value == '_';
}

bool has_word_boundaries(std::string_view text, std::size_t position, std::size_t length) {
    const bool before = position > 0 && is_word_byte(text[position - 1]);
    const bool after = position + length < text.size() && is_word_byte(text[position + length]);
    return !before && !after;
}

std::string ascii_lower(std::string_view value) {
    std::string lowered(value);
    std::transform(lowered.begin(), lowered.end(), lowered.begin(), [](unsigned char byte) {
        return static_cast<char>(std::tolower(byte));
    });
    return lowered;
}

std::regex make_regex(std::string_view query, const FindOptions& options) {
    auto flags = std::regex_constants::ECMAScript;
    if (!options.case_sensitive) flags |= std::regex_constants::icase;
    return std::regex(std::string(query), flags);
}

std::string trimmed_line(std::string_view text, std::size_t position) {
    const auto start_pos = position == 0 ? std::string_view::npos : text.rfind('\n', position - 1);
    std::size_t start = start_pos == std::string_view::npos ? 0 : start_pos + 1;
    const auto end_pos = text.find('\n', position);
    std::size_t end = end_pos == std::string_view::npos ? text.size() : end_pos;
    if (end > start && text[end - 1] == '\r') --end;
    while (start < end && std::isspace(static_cast<unsigned char>(text[start]))) ++start;
    while (end > start && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(start, end - start));
}

}  // namespace

FindResult find_matches(std::string_view text, std::string_view query, FindOptions options) {
    FindResult result;
    if (query.empty()) return result;

    if (options.use_regex) {
        try {
            const std::string source(text);
            const auto expression = make_regex(query, options);
            for (std::sregex_iterator item(source.begin(), source.end(), expression), end;
                 item != end; ++item) {
                const auto length = static_cast<std::size_t>(item->length());
                if (length == 0) continue;
                const auto position = static_cast<std::size_t>(item->position());
                if (!options.whole_word || has_word_boundaries(text, position, length))
                    result.matches.push_back({position, length});
            }
        } catch (const std::regex_error&) {
            result.valid = false;
        }
        return result;
    }

    const std::string haystack = options.case_sensitive ? std::string(text) : ascii_lower(text);
    const std::string needle = options.case_sensitive ? std::string(query) : ascii_lower(query);
    for (std::size_t position = haystack.find(needle); position != std::string::npos;
         position = haystack.find(needle, position + needle.size())) {
        if (!options.whole_word || has_word_boundaries(text, position, needle.size()))
            result.matches.push_back({position, needle.size()});
    }
    return result;
}

ReplaceResult replace_all_matches(std::string_view text, std::string_view query,
                                  std::string_view replacement, FindOptions options) {
    ReplaceResult result;
    result.text = std::string(text);
    const auto found = find_matches(text, query, options);
    result.valid = found.valid;
    if (!found.valid || found.matches.empty()) return result;

    if (!options.use_regex) {
        result.text.clear();
        std::size_t cursor = 0;
        for (const auto& match : found.matches) {
            result.text.append(text.substr(cursor, match.position - cursor));
            result.text.append(replacement);
            cursor = match.position + match.length;
        }
        result.text.append(text.substr(cursor));
        result.count = found.matches.size();
        return result;
    }

    try {
        const std::string source(text);
        const std::string replacement_text(replacement);
        const auto expression = make_regex(query, options);
        result.text.clear();
        std::size_t cursor = 0;
        for (std::sregex_iterator item(source.begin(), source.end(), expression), end;
             item != end; ++item) {
            const auto length = static_cast<std::size_t>(item->length());
            const auto position = static_cast<std::size_t>(item->position());
            if (length == 0 || (options.whole_word && !has_word_boundaries(text, position, length))) continue;
            result.text.append(text.substr(cursor, position - cursor));
            result.text += item->format(replacement_text);
            cursor = position + length;
            ++result.count;
        }
        result.text.append(text.substr(cursor));
    } catch (const std::regex_error&) {
        result.valid = false;
        result.text = std::string(text);
        result.count = 0;
    }
    return result;
}

ProjectFindResult find_project_matches(const std::vector<NamedScript>& scripts,
                                       std::string_view query, FindOptions options) {
    ProjectFindResult result;
    for (std::size_t file_index = 0; file_index < scripts.size(); ++file_index) {
        const auto& script = scripts[file_index];
        const auto found = find_matches(script.content, query, options);
        if (!found.valid) {
            result.valid = false;
            result.matches.clear();
            return result;
        }
        for (const auto& match : found.matches) {
            const auto line_start_pos = match.position == 0 ? std::string::npos
                                                            : script.content.rfind('\n', match.position - 1);
            const std::size_t line_start = line_start_pos == std::string::npos ? 0 : line_start_pos + 1;
            const std::size_t line_number = 1 + static_cast<std::size_t>(
                std::count(script.content.begin(), script.content.begin() + match.position, '\n'));
            result.matches.push_back({file_index, script.name, line_number,
                                      match.position - line_start + 1, match.position,
                                      match.length, trimmed_line(script.content, match.position)});
        }
    }
    return result;
}

std::vector<ProjectReplacePreview> preview_project_replacement(
    const std::vector<NamedScript>& scripts, std::string_view query, FindOptions options) {
    std::vector<ProjectReplacePreview> preview;
    for (std::size_t index = 0; index < scripts.size(); ++index) {
        const auto found = find_matches(scripts[index].content, query, options);
        if (!found.valid) return {};
        if (!found.matches.empty()) preview.push_back({index, scripts[index].name, found.matches.size()});
    }
    return preview;
}

ProjectReplaceResult replace_project_matches(
    const std::vector<NamedScript>& scripts, const std::vector<std::size_t>& selected_files,
    std::string_view query, std::string_view replacement, FindOptions options) {
    ProjectReplaceResult result;
    result.scripts = scripts;
    result.per_file_counts.resize(scripts.size());
    const std::unordered_set<std::size_t> selected(selected_files.begin(), selected_files.end());
    for (const auto index : selected) {
        if (index >= scripts.size()) continue;
        const auto changed = replace_all_matches(scripts[index].content, query, replacement, options);
        if (!changed.valid) {
            result.valid = false;
            result.scripts = scripts;
            std::fill(result.per_file_counts.begin(), result.per_file_counts.end(), 0);
            result.count = 0;
            return result;
        }
        result.scripts[index].content = changed.text;
        result.per_file_counts[index] = changed.count;
        result.count += changed.count;
    }
    return result;
}

}  // namespace say_count
