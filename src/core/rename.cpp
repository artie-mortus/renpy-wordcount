#include "core/rename.h"

#include <cctype>
#include <regex>

namespace say_count {
namespace {

bool Identifier(std::string_view value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_'))
        return false;
    for (const char c : value.substr(1))
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    return true;
}

bool LabelName(std::string_view value) {
    if (!value.empty() && value.front() == '.') value.remove_prefix(1);
    if (value.empty()) return false;
    bool segment_start = true;
    for (const char c : value) {
        if (c == '.') { if (segment_start) return false; segment_start = true; continue; }
        if (segment_start) {
            if (!(std::isalpha(static_cast<unsigned char>(c)) || c == '_')) return false;
            segment_start = false;
        } else if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '_')) return false;
    }
    return !segment_start;
}

std::string RegexEscape(std::string_view value) {
    std::string result;
    for (const char c : value) {
        if (std::string_view(".^$|()[]*+?{}\\").find(c) != std::string_view::npos) result.push_back('\\');
        result.push_back(c);
    }
    return result;
}

struct LineRename { std::string text; bool changed = false; };

LineRename RenameLine(std::string_view line, RenameKind kind, std::string_view from,
                      std::string_view to) {
    const std::string source = RegexEscape(from);
    const auto replace_match = [&](const std::regex& expression) -> LineRename {
        std::match_results<std::string_view::const_iterator> match;
        if (!std::regex_search(line.begin(), line.end(), match, expression)) return {std::string(line), false};
        const std::size_t position = static_cast<std::size_t>(match.position(2));
        std::string result(line);
        result.replace(position, match.length(2), to);
        return {std::move(result), true};
    };
    if (kind == RenameKind::SpeakerAlias) {
        const std::regex definition("^(\\s*(?:(?:define|default)\\s+)?)(" + source +
            ")(?=\\s*=\\s*(?:renpy\\.)?Character\\b)");
        auto result = replace_match(definition);
        if (result.changed) return result;
        const std::regex dialogue("^(\\s*)(" + source +
            ")(?=(?:\\s+[A-Za-z_]\\w*)*\\s+(?:\"|'))");
        return replace_match(dialogue);
    }
    const std::regex declaration("^(\\s*label\\s+)(" + source +
        ")(?=\\s*(?:\\([^)]*\\))?\\s*:)");
    auto result = replace_match(declaration);
    if (result.changed) return result;
    const std::regex reference("^(\\s*(?:jump|call)\\s+)(" + source + ")(?=\\s|\\(|$)");
    if (from == "expression") {
        std::match_results<std::string_view::const_iterator> match;
        if (std::regex_search(line.begin(), line.end(), match, reference)) {
            std::size_t tail = static_cast<std::size_t>(match.position(2) + match.length(2));
            const bool separated = tail < line.size() &&
                std::isspace(static_cast<unsigned char>(line[tail]));
            while (tail < line.size() && std::isspace(static_cast<unsigned char>(line[tail]))) ++tail;
            if (separated && tail < line.size() && line[tail] != '#') return {std::string(line), false};
        }
    }
    return replace_match(reference);
}

bool HasDeclaration(const std::vector<NamedScript>& files, RenameKind kind, std::string_view name) {
    const std::string source = RegexEscape(name);
    const std::regex expression(kind == RenameKind::SpeakerAlias
        ? "^\\s*(?:(?:define|default)\\s+)?" + source + "(?=\\s*=\\s*(?:renpy\\.)?Character\\b)"
        : "^\\s*label\\s+" + source + "(?=\\s*(?:\\([^)]*\\))?\\s*:)");
    for (const auto& file : files) {
        for (std::size_t start = 0; start <= file.content.size();) {
            const std::size_t newline = file.content.find('\n', start);
            std::string_view line(file.content.data() + start,
                                  (newline == std::string::npos ? file.content.size() : newline) - start);
            if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
            if (std::regex_search(line.begin(), line.end(), expression)) return true;
            if (newline == std::string::npos) break;
            start = newline + 1;
        }
    }
    return false;
}

}  // namespace

RenamePlan plan_symbol_rename(const std::vector<NamedScript>& files, RenameKind kind,
                              std::string_view from, std::string_view to) {
    RenamePlan plan;
    plan.files = files;
    const bool valid_from = kind == RenameKind::SpeakerAlias ? Identifier(from) : LabelName(from);
    const bool valid_to = kind == RenameKind::SpeakerAlias ? Identifier(to) : LabelName(to);
    if (!valid_from || !valid_to) {
        plan.error = kind == RenameKind::SpeakerAlias
            ? "Aliases must be valid Ren'Py identifiers."
            : "Labels must be valid Ren'Py label names.";
        return plan;
    }
    if (from == to) { plan.error = "Choose a different replacement name."; return plan; }
    if (HasDeclaration(files, kind, to)) {
        plan.error = "The replacement name is already declared in this project.";
        return plan;
    }
    for (std::size_t file_index = 0; file_index < plan.files.size(); ++file_index) {
        const std::string original = plan.files[file_index].content;
        std::string output;
        std::size_t line_number = 1;
        for (std::size_t start = 0; start <= original.size();) {
            const std::size_t newline = original.find('\n', start);
            const std::size_t end = newline == std::string::npos ? original.size() : newline;
            std::string_view body(original.data() + start, end - start);
            const bool carriage = !body.empty() && body.back() == '\r';
            if (carriage) body.remove_suffix(1);
            const auto renamed = RenameLine(body, kind, from, to);
            output += renamed.text;
            if (carriage) output.push_back('\r');
            if (newline != std::string::npos) output.push_back('\n');
            if (renamed.changed) plan.changes.push_back(
                {file_index, plan.files[file_index].name, line_number, std::string(body), renamed.text});
            if (newline == std::string::npos) break;
            start = newline + 1;
            ++line_number;
        }
        plan.files[file_index].content = std::move(output);
    }
    return plan;
}

}  // namespace say_count
