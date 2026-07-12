#include "core/outline.h"
#include "core/parser.h"
#include "core/tokenizer.h"

#include <map>
#include <sstream>
#include <cctype>

namespace say_count {
namespace {
std::string first_identifier(std::string value) {
    const auto begin = value.find_first_not_of(" \t");
    if (begin == std::string::npos) return {};
    auto end = begin;
    if (value[end] == '.') ++end;
    while (end < value.size() && (std::isalnum(static_cast<unsigned char>(value[end])) || value[end] == '_' || value[end] == '.')) ++end;
    return value.substr(begin, end - begin);
}
std::string qualify(const std::string& name, const std::string& global) {
    return !name.empty() && name.front() == '.' && !global.empty() ? global + name : name;
}
}

std::vector<OutlineItem> build_outline(std::string_view script) {
    std::vector<OutlineItem> result;
    std::map<std::string, std::size_t> labels;
    std::string global;
    std::istringstream lines{std::string(script)};
    std::string raw;
    std::size_t number = 0;
    while (std::getline(lines, raw)) {
        ++number;
        const auto line = tokenize_line(raw, number);
        if (line.type == LineType::Label) {
            const auto display = first_identifier(line.subject);
            const auto name = qualify(display, global);
            if (!display.empty() && display.front() != '.') global = display;
            result.push_back({OutlineKind::Label, display, name, number, true, number});
            if (!labels.count(name)) labels[name] = number;
        } else if (line.type == LineType::Jump || line.type == LineType::Call) {
            const auto display = first_identifier(line.subject);
            if (display == "expression" || display == "screen") continue;
            result.push_back({line.type == LineType::Jump ? OutlineKind::Jump : OutlineKind::Call,
                              display, qualify(display, global), number, false, 0});
        } else if (!line.quoted.empty() && line.quoted.front().begin == 0 &&
                   !line.code.empty() && line.code.back() == ':') {
            result.push_back({OutlineKind::Choice, clean_renpy_text(line.quoted.front().text), {}, number, true, number});
        }
    }
    for (auto& item : result) {
        if (item.kind != OutlineKind::Jump && item.kind != OutlineKind::Call) continue;
        const auto found = labels.find(item.qualified_name);
        item.target_found = found != labels.end();
        item.target_line = item.target_found ? found->second : item.line_number;
    }
    return result;
}
}
