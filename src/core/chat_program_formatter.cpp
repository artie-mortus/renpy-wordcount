#include "core/chat_program_formatter.h"

#include <iomanip>
#include <sstream>
#include <vector>

namespace say_count {
namespace {

std::string Escape(std::string_view value) {
    std::string result;
    for (char c : value) {
        if (c == '\n') { result += "\\n"; continue; }
        if (c == '\\' || c == '"') result += '\\';
        if (c == '[') result += '[';
        result += c;
    }
    return result;
}

void FormatEvents(const std::vector<ChatEvent>& events, std::size_t indent,
                  std::string* last_channel, std::ostringstream* output) {
    const std::string pad(indent, ' ');
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        if (event.kind == ChatEventKind::Passthrough) { *output << event.original << '\n'; continue; }
        if (event.kind == ChatEventKind::Narration) {
            *output << pad << "\"" << Escape(event.text) << "\"\n";
            continue;
        }
        if (event.kind == ChatEventKind::Choice) {
            *output << pad << "menu:\n";
            bool first_choice = true;
            while (index < events.size() && events[index].kind == ChatEventKind::Choice &&
                   (first_choice || !events[index].menu_start)) {
                const auto& choice = events[index];
                *output << pad << "    \"" << Escape(choice.text) << "\"";
                std::vector<std::string> choice_arguments;
                if (!choice.auto_send) choice_arguments.push_back("auto_send=False");
                choice_arguments.insert(choice_arguments.end(),
                                        choice.extra_arguments.begin(), choice.extra_arguments.end());
                if (!choice_arguments.empty()) {
                    *output << " (";
                    for (std::size_t i = 0; i < choice_arguments.size(); ++i) {
                        if (i) *output << ", ";
                        *output << choice_arguments[i];
                    }
                    *output << ')';
                }
                *output << ":\n";
                if (choice.children.empty()) *output << pad << "        pass\n";
                else FormatEvents(choice.children, indent + 8, last_channel, output);
                first_choice = false;
                ++index;
            }
            --index;
            continue;
        }
        *output << pad << event.speaker << " \"" << Escape(event.text) << "\"";
        std::vector<std::string> arguments;
        if (!event.channel.empty() && (event.channel_explicit || event.channel != *last_channel))
            arguments.push_back("c=\"" + Escape(event.channel) + "\"");
        if (!event.other_typers.empty()) {
            std::string value = event.other_typers.size() == 1 ? event.other_typers.front() : "[";
            if (event.other_typers.size() > 1) {
                for (std::size_t i = 0; i < event.other_typers.size(); ++i) {
                    if (i) value += ", ";
                    value += event.other_typers[i];
                }
                value += ']';
            }
            arguments.push_back("ot=" + value);
        }
        if (event.has_fast_mode) {
            std::ostringstream number;
            number << std::setprecision(12) << event.fast_mode;
            arguments.push_back("fastmode=" + number.str());
        }
        arguments.insert(arguments.end(), event.extra_arguments.begin(), event.extra_arguments.end());
        if (!arguments.empty()) {
            *output << " (";
            for (std::size_t i = 0; i < arguments.size(); ++i) {
                if (i) *output << ", ";
                *output << arguments[i];
            }
            *output << ')';
        }
        *output << '\n';
        if (!event.channel.empty()) *last_channel = event.channel;
    }
}

}  // namespace

std::string format_chat_program(const ChatDocument& document, std::string_view label,
                                bool include_definitions) {
    std::ostringstream output;
    if (include_definitions) {
        for (const auto& character : document.characters) {
            output << "default " << character.alias << " = ChatCharacter(\""
                   << Escape(character.name) << "\"";
            if (!character.icon.empty()) output << ", icon=\"" << Escape(character.icon) << "\"";
            if (character.name_color != "#FFF")
                output << ", name_color=\"" << Escape(character.name_color) << "\"";
            if (character.is_player) output << ", is_player=True";
            output << ")\n";
        }
        if (!document.characters.empty()) output << '\n';
    }
    if (!label.empty()) output << "label " << label << ":\n";
    std::string last_channel = document.default_channel;
    FormatEvents(document.events, label.empty() ? 0 : 4, &last_channel, &output);
    return output.str();
}

}  // namespace say_count
