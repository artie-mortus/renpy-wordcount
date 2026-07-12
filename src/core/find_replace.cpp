#include "core/find_replace.h"

#include <algorithm>
#include <cctype>
#include <regex>

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

}  // namespace say_count
