#include "core/renpy_lint.h"

#include <cctype>
#include <regex>

namespace say_count {

std::vector<RenpyLintIssue> parse_renpy_lint(std::string_view output) {
    std::vector<RenpyLintIssue> issues;
    const std::regex compact(R"(^\s*(.+?\.rpy):(\d+)(?::|\s)+(.*)$)", std::regex::icase);
    const std::regex traceback(R"(^\s*File\s+["'](.+?\.rpy)["'],\s*line\s+(\d+)\s*:?(.*)$)",
                               std::regex::icase);
    std::size_t start = 0;
    while (start <= output.size()) {
        const auto newline = output.find('\n', start);
        auto line = output.substr(start, newline == std::string_view::npos ? output.size() - start
                                                                           : newline - start);
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        std::match_results<std::string_view::const_iterator> match;
        if (std::regex_match(line.begin(), line.end(), match, compact) ||
            std::regex_match(line.begin(), line.end(), match, traceback)) {
            RenpyLintIssue issue;
            issue.file = match[1].str();
            try { issue.line = std::stoull(match[2].str()); } catch (...) { issue.line = 0; }
            issue.message = match[3].str();
            while (!issue.message.empty() && std::isspace(static_cast<unsigned char>(issue.message.front())))
                issue.message.erase(issue.message.begin());
            if (issue.message.empty()) issue.message = "Ren'Py lint issue";
            issues.push_back(std::move(issue));
        }
        if (newline == std::string_view::npos) break;
        start = newline + 1;
    }
    return issues;
}

}  // namespace say_count
