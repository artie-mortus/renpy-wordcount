#include "core/script_builder.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <vector>

namespace say_count {
namespace {

std::string trim(std::string value) {
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) value.erase(value.begin());
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    return value;
}

bool identifier(const std::string& value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_')) return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char c) { return std::isalnum(c) || c == '_'; });
}

bool label_identifier(const std::string& value) {
    if (value.empty()) return false;
    const std::size_t start = value.front() == '.' ? 1 : 0;
    if (start == value.size()) return false;
    std::size_t part_start = start;
    while (part_start < value.size()) {
        const std::size_t dot = value.find('.', part_start);
        const std::string part = value.substr(
            part_start, dot == std::string::npos ? value.size() - part_start : dot - part_start);
        if (!identifier(part)) return false;
        if (dot == std::string::npos) return true;
        part_start = dot + 1;
    }
    return false;
}

bool image_name(const std::string& value) {
    if (value.empty() || value.find_first_of("\r\n") != std::string::npos) return false;
    std::istringstream input(value);
    std::string word;
    bool found = false;
    while (input >> word) {
        if (!identifier(word)) return false;
        found = true;
    }
    return found;
}

bool duration(const std::string& value) {
    if (value.empty()) return true;
    bool digit = false;
    bool dot = false;
    for (const char c : value) {
        if (std::isdigit(static_cast<unsigned char>(c))) { digit = true; continue; }
        if (c == '.' && !dot) { dot = true; continue; }
        return false;
    }
    return digit;
}

std::string quote(std::string value) {
    std::string result = "\"";
    for (const char c : value) {
        if (c == '\\' || c == '"') result.push_back('\\');
        if (c == '\n' || c == '\r') result += "\\n"; else result.push_back(c);
    }
    return result + "\"";
}

std::vector<std::string> lines(const std::string& source) {
    std::vector<std::string> result;
    std::istringstream input(source); std::string line;
    while (std::getline(input, line)) if (!(line = trim(line)).empty()) result.push_back(line);
    return result;
}

std::string at_line_start(std::string text, const std::string& indent) {
    std::string result = indent;
    for (char c : text) {
        result.push_back(c);
        if (c == '\n') result += indent;
    }
    if (result.size() >= indent.size() && result.substr(result.size() - indent.size()) == indent)
        result.erase(result.size() - indent.size());
    return result;
}

}  // namespace

StoryElementResult build_story_element(const StoryElementRequest& request) {
    const std::string primary = trim(request.primary);
    const std::string secondary = trim(request.secondary);
    std::string text;
    switch (request.kind) {
        case StoryElementKind::Character:
            if (!identifier(primary)) return {false, {}, "Use a short character code made from letters, numbers, or underscores.", {}};
            if (secondary.empty()) return {false, {}, "Enter the character name players will see.", {}};
            text = "define " + primary + " = Character(" + quote(secondary) + ")\n";
            return {true, text, {}, "Character definitions usually belong near the top of a definitions script."};
        case StoryElementKind::Dialogue:
            if (secondary.empty()) return {false, {}, "Enter what the character says.", {}};
            if (!primary.empty() && !identifier(primary)) return {false, {}, "Use a valid character code, or leave it blank for narration.", {}};
            text = (primary.empty() ? "narrator " : primary + " ") + quote(secondary) + "\n";
            break;
        case StoryElementKind::Choice: {
            const auto options = lines(request.details);
            if (options.size() < 2) return {false, {}, "Enter at least two choices, one per line.", {}};
            text = "menu:\n";
            if (!primary.empty()) text += "    " + quote(primary) + "\n";
            for (const auto& option : options) text += "    " + quote(option) + ":\n        pass\n";
            break;
        }
        case StoryElementKind::Scene:
            if (!image_name(primary)) return {false, {}, "Enter an image name, such as bg kitchen.", {}};
            text = "scene " + primary + "\n"; break;
        case StoryElementKind::Music:
        case StoryElementKind::Sound:
            if (primary.empty()) return {false, {}, "Enter an audio file path.", {}};
            text = std::string("play ") + (request.kind == StoryElementKind::Music ? "music " : "sound ") + quote(primary) + "\n";
            break;
        case StoryElementKind::Jump:
            if (!label_identifier(primary)) return {false, {}, "Enter the exact scene name to continue at.", {}};
            text = "jump " + primary + "\n"; break;
        case StoryElementKind::Show:
            if (!image_name(primary)) return {false, {}, "Enter an image name, such as eileen happy.", {}};
            if (!secondary.empty() && !identifier(secondary))
                return {false, {}, "Use one placement name, such as left or center.", {}};
            text = "show " + primary + (secondary.empty() ? "" : " at " + secondary) + "\n";
            break;
        case StoryElementKind::Hide:
            if (!image_name(primary)) return {false, {}, "Enter the image tag to hide, such as eileen.", {}};
            text = "hide " + primary + "\n"; break;
        case StoryElementKind::StopMusic:
            if (!duration(primary)) return {false, {}, "Use seconds such as 0.5, or leave this blank.", {}};
            text = "stop music" + (primary.empty() ? "" : " fadeout " + primary) + "\n";
            break;
        case StoryElementKind::Pause:
            if (!duration(primary)) return {false, {}, "Use seconds such as 1.0, or leave this blank.", {}};
            text = "pause" + (primary.empty() ? "" : " " + primary) + "\n";
            break;
        case StoryElementKind::Call:
            if (!label_identifier(primary)) return {false, {}, "Enter the exact scene name to call.", {}};
            text = "call " + primary + "\n"; break;
        case StoryElementKind::Return:
            text = "return\n"; break;
        case StoryElementKind::Transition:
            if (!identifier(primary)) return {false, {}, "Choose a transition such as dissolve or fade.", {}};
            text = "with " + primary + "\n"; break;
    }
    return {true, at_line_start(text, request.indentation), {}, {}};
}

}  // namespace say_count
