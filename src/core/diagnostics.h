#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "core/parser.h"

namespace say_count {

enum class DiagnosticSeverity { Error, Warning, Notice };

struct Diagnostic {
    std::string type;
    DiagnosticSeverity severity = DiagnosticSeverity::Warning;
    std::size_t file_index = 0;
    std::string file;
    std::size_t line_number = 0;
    std::size_t column = 0;
    std::size_t position = 0;
    std::size_t length = 0;
    std::string message;
};

struct DiagnosticsOptions {
    std::size_t long_line_words = 35;
};

struct BasicFixResult {
    std::vector<NamedScript> scripts;
    std::size_t changed_files = 0;
    std::size_t indentation_fixes = 0;
    std::size_t quote_fixes = 0;
    std::size_t colon_fixes = 0;
    std::size_t empty_block_fixes = 0;

    std::size_t total_fixes() const {
        return indentation_fixes + quote_fixes + colon_fixes + empty_block_fixes;
    }
};

std::vector<Diagnostic> diagnose_project(const std::vector<NamedScript>& scripts,
                                         DiagnosticsOptions options = {});
bool has_basic_fix(const Diagnostic& diagnostic);
std::size_t count_basic_fixes(const std::vector<Diagnostic>& diagnostics);
BasicFixResult fix_basic_diagnostics(const std::vector<NamedScript>& scripts,
                                     DiagnosticsOptions options = {});

}  // namespace say_count
