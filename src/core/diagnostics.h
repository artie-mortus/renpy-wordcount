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

std::vector<Diagnostic> diagnose_project(const std::vector<NamedScript>& scripts,
                                         DiagnosticsOptions options = {});

}  // namespace say_count
