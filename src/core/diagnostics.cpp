#include "core/diagnostics.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <unordered_set>

#include "core/indent.h"
#include "core/tokenizer.h"

namespace say_count {
namespace {

struct LineRange {
    std::size_t position = 0;
    std::size_t length = 0;
    std::size_t column = 1;
};

struct SourceLine {
    std::string text;
    std::string ending;
};

struct LineFixes {
    bool indentation = false;
    bool quote = false;
    bool colon = false;
    bool empty_block = false;
};

bool should_diagnose_file(std::string_view name) {
    const auto slash = name.find_last_of("/\\");
    std::string base(name.substr(slash == std::string_view::npos ? 0 : slash + 1));
    std::transform(base.begin(), base.end(), base.begin(), [](unsigned char byte) {
        return static_cast<char>(std::tolower(byte));
    });
    static const std::unordered_set<std::string> support{
        "gui.rpy", "options.rpy", "screens.rpy", "say_count_runtime.rpy"};
    return support.count(base) == 0;
}

LineRange line_range(std::string_view source, std::size_t line_number) {
    if (line_number == 0) return {};
    std::size_t start = 0;
    for (std::size_t line = 1; line < line_number; ++line) {
        const auto newline = source.find('\n', start);
        if (newline == std::string_view::npos) return {source.size(), 0, 1};
        start = newline + 1;
    }
    const auto newline = source.find('\n', start);
    std::size_t end = newline == std::string_view::npos ? source.size() : newline;
    if (end > start && source[end - 1] == '\r') --end;
    std::size_t first = start;
    while (first < end && (source[first] == ' ' || source[first] == '\t')) ++first;
    return {first, std::max<std::size_t>(1, end - first), first - start + 1};
}

DiagnosticSeverity severity_for(std::string_view type) {
    if (type == "syntax" || type == "missing-label" || type == "quote" ||
        type == "duplicate-label") return DiagnosticSeverity::Error;
    if (type == "length") return DiagnosticSeverity::Notice;
    return DiagnosticSeverity::Warning;
}

std::size_t find_file(const std::vector<NamedScript>& scripts, std::string_view name) {
    const auto found = std::find_if(scripts.begin(), scripts.end(),
        [&](const auto& script) { return script.name == name; });
    return found == scripts.end() ? scripts.size() : static_cast<std::size_t>(found - scripts.begin());
}

void append_indentation_diagnostics(std::vector<Diagnostic>& diagnostics,
                                    const NamedScript& script, std::size_t file_index) {
    if (!should_diagnose_file(script.name)) return;
    std::size_t start = 0;
    std::size_t line_number = 1;
    while (start <= script.content.size()) {
        const auto newline = script.content.find('\n', start);
        const std::size_t end = newline == std::string::npos ? script.content.size() : newline;
        std::size_t prefix = start;
        std::size_t width = 0;
        bool has_tab = false;
        while (prefix < end && (script.content[prefix] == ' ' || script.content[prefix] == '\t')) {
            if (script.content[prefix] == '\t') { width += 4; has_tab = true; }
            else ++width;
            ++prefix;
        }
        const bool content = prefix < end && script.content[prefix] != '#' && script.content[prefix] != '\r';
        if (content && (has_tab || width % 4 != 0)) {
            const std::string message = has_tab
                ? "Suspicious indentation: use spaces instead of tabs."
                : "Suspicious indentation: use a multiple of four spaces.";
            diagnostics.push_back({"indentation", DiagnosticSeverity::Warning, file_index,
                                   script.name, line_number, 1, start,
                                   std::max<std::size_t>(1, prefix - start), message});
        }
        if (newline == std::string::npos) break;
        start = newline + 1;
        ++line_number;
    }
}

std::vector<SourceLine> split_lines(std::string_view source) {
    std::vector<SourceLine> lines;
    std::size_t start = 0;
    while (start < source.size()) {
        const auto newline = source.find('\n', start);
        if (newline == std::string_view::npos) {
            lines.push_back({std::string(source.substr(start)), {}});
            return lines;
        }
        std::size_t end = newline;
        std::string ending = "\n";
        if (end > start && source[end - 1] == '\r') {
            --end;
            ending = "\r\n";
        }
        lines.push_back({std::string(source.substr(start, end - start)), std::move(ending)});
        start = newline + 1;
    }
    if (source.empty() || source.back() == '\n') lines.push_back({{}, {}});
    return lines;
}

std::size_t leading_width(std::string_view line) {
    std::size_t width = 0;
    for (const char character : line) {
        if (character == ' ') ++width;
        else if (character == '\t') width += 4;
        else break;
    }
    return width;
}

bool append_trailing_colon(std::string& line) {
    const auto token = tokenize_line(line, 0);
    std::size_t code_end = line.size();
    if (!token.comment.empty()) {
        const auto comment = line.rfind(token.comment);
        if (comment != std::string::npos) code_end = comment;
    }
    while (code_end > 0 && std::isspace(static_cast<unsigned char>(line[code_end - 1])))
        --code_end;
    if (code_end == 0 || line[code_end - 1] == ':') return false;
    line.insert(code_end, 1, ':');
    return true;
}

bool close_single_line_quote(std::string& line) {
    if (line.find("\"\"\"") != std::string::npos) return false;
    const auto token = tokenize_line(line, 0);
    const auto open = std::find_if(token.quoted.begin(), token.quoted.end(),
        [](const auto& quote) { return !quote.closed; });
    if (open == token.quoted.end()) return false;
    std::size_t end = line.size();
    while (end > 0 && (line[end - 1] == ' ' || line[end - 1] == '\t')) --end;
    line.insert(end, 1, open->quote);
    return true;
}

bool fix_file(std::string& source, const std::vector<Diagnostic>& diagnostics,
              BasicFixResult& result) {
    std::map<std::size_t, LineFixes> fixes;
    for (const auto& diagnostic : diagnostics) {
        auto& line = fixes[diagnostic.line_number];
        if (diagnostic.type == "indentation") line.indentation = true;
        else if (diagnostic.type == "quote") line.quote = true;
        else if (diagnostic.type == "empty-block") line.empty_block = true;
        else if (diagnostic.type == "syntax" &&
                 diagnostic.message.find("missing a trailing colon") != std::string::npos)
            line.colon = true;
    }
    if (fixes.empty()) return false;

    auto lines = split_lines(source);
    const std::string default_ending = source.find("\r\n") != std::string::npos ? "\r\n" : "\n";
    std::string fixed;
    std::optional<std::size_t> triple_quote_indent;
    bool changed = false;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        auto line = std::move(lines[index]);
        const auto found = fixes.find(index + 1);
        if (found != fixes.end()) {
            const auto& requested = found->second;
            if (requested.indentation) {
                const auto preview = preview_indent_fix(line.text);
                if (!preview.changes.empty()) {
                    line.text = preview.fixed;
                    ++result.indentation_fixes;
                    changed = true;
                }
            }
            if (requested.colon && append_trailing_colon(line.text)) {
                ++result.colon_fixes;
                changed = true;
            }
            if (requested.quote && line.text.find("\"\"\"") == std::string::npos &&
                close_single_line_quote(line.text)) {
                ++result.quote_fixes;
                changed = true;
            }
        }

        fixed += line.text;
        fixed += line.ending;
        if (found == fixes.end()) continue;
        const auto& requested = found->second;
        if (requested.quote && line.text.find("\"\"\"") != std::string::npos) {
            triple_quote_indent = leading_width(line.text);
        }
        if (requested.empty_block) {
            if (line.ending.empty()) fixed += default_ending;
            fixed += std::string(leading_width(line.text) + 4, ' ') + "pass";
            if (!line.ending.empty() || index + 1 < lines.size()) fixed += default_ending;
            ++result.empty_block_fixes;
            changed = true;
        }
    }
    if (triple_quote_indent) {
        if (!fixed.empty() && fixed.back() != '\n') fixed += default_ending;
        fixed += std::string(*triple_quote_indent, ' ') + "\"\"\"";
        if (!source.empty() && source.back() == '\n') fixed += default_ending;
        ++result.quote_fixes;
        changed = true;
    }
    if (changed) source = std::move(fixed);
    return changed;
}

}  // namespace

std::vector<Diagnostic> diagnose_project(const std::vector<NamedScript>& scripts,
                                         DiagnosticsOptions options) {
    std::vector<Diagnostic> diagnostics;
    AnalysisOptions analysis_options;
    analysis_options.long_line_words = options.long_line_words;
    const auto analysis = analyze_project(scripts, analysis_options);
    diagnostics.reserve(analysis.warnings.size());
    for (const auto& warning : analysis.warnings) {
        const auto file_index = find_file(scripts, warning.file);
        if (file_index == scripts.size()) continue;
        const auto range = line_range(scripts[file_index].content, warning.line_number);
        diagnostics.push_back({warning.type, severity_for(warning.type), file_index, warning.file,
                               warning.line_number, range.column, range.position, range.length,
                               warning.message});
    }
    for (std::size_t index = 0; index < scripts.size(); ++index)
        append_indentation_diagnostics(diagnostics, scripts[index], index);
    std::stable_sort(diagnostics.begin(), diagnostics.end(), [](const auto& left, const auto& right) {
        if (left.file_index != right.file_index) return left.file_index < right.file_index;
        if (left.line_number != right.line_number) return left.line_number < right.line_number;
        return static_cast<int>(left.severity) < static_cast<int>(right.severity);
    });
    return diagnostics;
}

bool has_basic_fix(const Diagnostic& diagnostic) {
    return diagnostic.type == "indentation" || diagnostic.type == "quote" ||
           diagnostic.type == "empty-block" ||
           (diagnostic.type == "syntax" &&
            diagnostic.message.find("missing a trailing colon") != std::string::npos);
}

std::size_t count_basic_fixes(const std::vector<Diagnostic>& diagnostics) {
    return static_cast<std::size_t>(std::count_if(
        diagnostics.begin(), diagnostics.end(), has_basic_fix));
}

BasicFixResult fix_basic_diagnostics(const std::vector<NamedScript>& scripts,
                                     DiagnosticsOptions options) {
    BasicFixResult result;
    result.scripts = scripts;
    std::set<std::size_t> changed_files;
    // Some repairs reveal another deterministic issue. For example, adding a
    // missing block colon can reveal that the block is empty. Iterate to a
    // small fixed point so one button press handles both.
    for (int pass = 0; pass < 6; ++pass) {
        const auto diagnostics = diagnose_project(result.scripts, options);
        std::vector<std::vector<Diagnostic>> by_file(result.scripts.size());
        for (const auto& diagnostic : diagnostics) {
            if (diagnostic.file_index < by_file.size() && has_basic_fix(diagnostic))
                by_file[diagnostic.file_index].push_back(diagnostic);
        }
        bool changed = false;
        for (std::size_t index = 0; index < result.scripts.size(); ++index) {
            if (by_file[index].empty()) continue;
            if (fix_file(result.scripts[index].content, by_file[index], result)) {
                changed = true;
                changed_files.insert(index);
            }
        }
        if (!changed) break;
    }
    result.changed_files = changed_files.size();
    return result;
}

}  // namespace say_count
