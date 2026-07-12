#include "core/diagnostics.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_set>

namespace say_count {
namespace {

struct LineRange {
    std::size_t position = 0;
    std::size_t length = 0;
    std::size_t column = 1;
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

}  // namespace say_count
