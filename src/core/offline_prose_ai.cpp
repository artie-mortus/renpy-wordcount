#include "core/offline_prose_ai.h"

#include "core/manuscript.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace say_count {
namespace {

std::vector<std::string> Lines(std::string_view text) {
    std::vector<std::string> lines;
    std::istringstream input{std::string(text)};
    for (std::string line; std::getline(input, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(std::move(line));
    }
    return lines;
}

bool SafeField(std::string_view value, std::size_t maximum) {
    return !value.empty() && value.size() <= maximum &&
        std::none_of(value.begin(), value.end(), [](unsigned char c) {
            return c == '\n' || c == '\r' || c == '\t' || c < 0x20;
        });
}

bool SafeSpeaker(std::string_view value) {
    if (!SafeField(value, 100)) return false;
    std::string name(value);
    if (!name.empty() && name.back() == ']') {
        const auto open = name.find_last_of('[');
        if (open == std::string::npos || open == 0) return false;
        std::string attributes = name.substr(open + 1, name.size() - open - 2);
        std::replace(attributes.begin(), attributes.end(), ',', ' ');
        std::istringstream words(attributes);
        std::size_t count = 0;
        for (std::string word; words >> word;) {
            if (++count > 6 || word.empty() ||
                !(std::isalpha(static_cast<unsigned char>(word.front())) || word.front() == '_') ||
                !std::all_of(word.begin() + 1, word.end(), [](unsigned char c) {
                    return std::isalnum(c) || c == '_';
                })) return false;
        }
        if (count == 0) return false;
        name = name.substr(0, open);
        while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.pop_back();
    }
    bool letter = false;
    for (const unsigned char c : name) {
        if (c >= 0x80) { letter = true; continue; }
        if (std::isalpha(c)) letter = true;
        if (!(std::isalnum(c) || c == ' ' || c == '_' || c == '-' || c == '\'' || c == '.'))
            return false;
    }
    return letter;
}

std::string EscapePrompt(std::string value) {
    std::replace(value.begin(), value.end(), '\t', ' ');
    std::replace(value.begin(), value.end(), '\r', ' ');
    std::replace(value.begin(), value.end(), '\n', ' ');
    return value;
}

}  // namespace

std::string build_offline_prose_prompt(
    std::string_view manuscript,
    const std::map<std::string, std::string>& existing_characters) {
    const auto reviews = review_manuscript_lines(manuscript);
    std::ostringstream prompt;
    prompt << "You are a private, offline screenplay parser. Convert each supplied book-prose line "
              "into ordered narration and dialogue events. Use context to resolve action beats and "
              "pronouns, but do not invent, summarize, or rewrite words. Infer a sprite expression "
              "only from a clear mood cue: happy, angry, sad, nervous, surprised, or embarrassed. "
              "Write an inferred mood after the speaker as [happy], for example. Preserve an explicit "
              "[expression] instead; it always overrides inference.\n"
              "Reply with records only. Narration: SC|line|N|exact text. Dialogue: "
              "SC|line|D|speaker|exact spoken text. Preserve a speaker's [expression attributes] "
              "exactly in the speaker field. A source line may produce multiple records. "
              "Keep their reading order. If uncertain, use one narration record.\n";
    if (!existing_characters.empty()) {
        prompt << "Known characters:";
        for (const auto& [alias, display] : existing_characters)
            prompt << " " << EscapePrompt(display) << " (" << EscapePrompt(alias) << ");";
        prompt << '\n';
    }
    prompt << "SOURCE\n";
    for (const auto& review : reviews) {
        if (review.kind == ManuscriptLineKind::Prose || review.kind == ManuscriptLineKind::Uncertain)
            prompt << review.line_number << '\t' << EscapePrompt(review.text) << '\n';
    }
    prompt << "END SOURCE\nRECORDS\n";
    return prompt.str();
}

OfflineProseRewrite apply_offline_prose_response(
    std::string_view manuscript, std::string_view response) {
    const auto source_lines = Lines(manuscript);
    const auto reviews = review_manuscript_lines(manuscript);
    std::set<std::size_t> allowed;
    for (const auto& review : reviews)
        if (review.kind == ManuscriptLineKind::Prose || review.kind == ManuscriptLineKind::Uncertain)
            allowed.insert(review.line_number);

    std::map<std::size_t, std::vector<std::string>> replacements;
    std::istringstream input{std::string(response)};
    std::size_t records = 0;
    for (std::string line; std::getline(input, line);) {
        if (line.rfind("SC|", 0) != 0) continue;
        std::vector<std::string> fields;
        for (std::size_t begin = 0;;) {
            const auto separator = line.find('|', begin);
            fields.push_back(line.substr(begin, separator - begin));
            if (separator == std::string::npos) break;
            begin = separator + 1;
        }
        if (fields.size() < 4 || fields[0] != "SC") continue;
        std::size_t number = 0;
        try { number = static_cast<std::size_t>(std::stoull(fields[1])); }
        catch (...) { continue; }
        if (!allowed.count(number) || ++records > allowed.size() * 4) continue;
        if (fields[2] == "N" && fields.size() == 4 && SafeField(fields[3], 4000)) {
            replacements[number].push_back("\xe2\x80\x9c" + fields[3] + "\xe2\x80\x9d");
        } else if (fields[2] == "D" && fields.size() == 5 &&
                   SafeSpeaker(fields[3]) && SafeField(fields[4], 4000)) {
            replacements[number].push_back(fields[3] + ": " + fields[4]);
        }
    }
    if (replacements.empty()) return {{}, 0, "The local model did not return any valid prose records."};

    const std::string newline = manuscript.find("\r\n") != std::string_view::npos ? "\r\n" : "\n";
    const bool trailing = !manuscript.empty() && manuscript.back() == '\n';
    std::ostringstream output;
    for (std::size_t index = 0; index < source_lines.size(); ++index) {
        if (index) output << newline;
        const auto found = replacements.find(index + 1);
        if (found == replacements.end()) output << source_lines[index];
        else {
            for (std::size_t part = 0; part < found->second.size(); ++part) {
                if (part) output << newline;
                output << found->second[part];
            }
        }
    }
    if (trailing) output << newline;
    return {output.str(), replacements.size(), {}};
}

}  // namespace say_count
