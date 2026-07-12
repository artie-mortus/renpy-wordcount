#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {
enum class OutlineKind { Label, Choice, Jump, Call };
struct OutlineItem {
    OutlineKind kind;
    std::string name;
    std::string qualified_name;
    std::size_t line_number = 0;
    bool target_found = true;
    std::size_t target_line = 0;
};
std::vector<OutlineItem> build_outline(std::string_view script);
}
