#include "core/chat_dialogue_adapter.h"

#include "core/chat_program_formatter.h"
#include "core/chat_program_parser.h"
#include "core/manuscript.h"

#include <algorithm>
#include <sstream>

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

void CountEvents(const std::vector<ChatEvent>& events, ChatConversion* conversion) {
    for (const auto& event : events) {
        if (event.kind == ChatEventKind::Message) ++conversion->messages;
        else if (event.kind == ChatEventKind::Narration) ++conversion->narration;
        else if (event.kind == ChatEventKind::Choice) ++conversion->choices;
        CountEvents(event.children, conversion);
    }
}

std::string CharacterName(const ChatDocument& document, const std::string& alias) {
    const auto found = std::find_if(document.characters.begin(), document.characters.end(),
        [&](const auto& character) { return character.alias == alias; });
    return found == document.characters.end() ? alias : found->name;
}

void RenderManuscript(const ChatDocument& document, const std::vector<ChatEvent>& events,
                      std::size_t indent, bool renpy, std::ostringstream* output,
                      std::vector<ChatDiagnostic>* losses) {
    const std::string pad(indent, ' ');
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        if (event.kind == ChatEventKind::Message) {
            if (renpy) *output << pad << event.speaker << " \"" << Escape(event.text) << "\"\n";
            else *output << pad << CharacterName(document, event.speaker) << ": " << event.text << '\n';
            if (event.channel_explicit)
                losses->push_back({event.source, "Channel " + event.channel + " is not represented in regular dialogue.", true});
            if (!event.other_typers.empty())
                losses->push_back({event.source, "Other-typer metadata is not represented in regular dialogue.", true});
            if (event.has_fast_mode)
                losses->push_back({event.source, "fastmode is not represented in regular dialogue.", true});
            for (const auto& argument : event.extra_arguments)
                losses->push_back({event.source, "Argument " + argument + " is not represented in regular dialogue.", true});
        } else if (event.kind == ChatEventKind::Narration) {
            *output << pad << (renpy ? "\"" + Escape(event.text) + "\"" : event.text) << '\n';
        } else if (event.kind == ChatEventKind::Choice) {
            if (renpy) {
                *output << pad << "menu:\n";
                bool first_choice = true;
                while (index < events.size() && events[index].kind == ChatEventKind::Choice &&
                       (first_choice || !events[index].menu_start)) {
                    const auto& choice = events[index];
                    *output << pad << "    \"" << Escape(choice.text) << "\":\n";
                    if (choice.children.empty()) *output << pad << "        pass\n";
                    else RenderManuscript(document, choice.children, indent + 8, true, output, losses);
                    if (!choice.auto_send)
                        losses->push_back({choice.source, "auto_send=False is not represented in regular dialogue.", true});
                    for (const auto& argument : choice.extra_arguments)
                        losses->push_back({choice.source, "Argument " + argument + " is not represented in regular dialogue.", true});
                    first_choice = false;
                    ++index;
                }
                --index;
            } else {
                *output << pad << ":: choice " << event.text << '\n';
                RenderManuscript(document, event.children, indent, false, output, losses);
                if (!event.auto_send)
                    losses->push_back({event.source, "auto_send=False is not represented in regular dialogue.", true});
            }
        } else if (event.kind == ChatEventKind::Passthrough) {
            *output << event.original << '\n';
        }
    }
}

}  // namespace

ChatConversion convert_manuscript_to_chat(
    std::string_view manuscript, std::string_view default_channel,
    const std::map<std::string, std::string>& existing_characters,
    std::string_view narrator_alias, std::string_view narrator_name) {
    ManuscriptOptions options;
    options.label = "chat_scene";
    options.existing_characters = existing_characters;
    const auto renpy = convert_manuscript_to_renpy(manuscript, options);
    ChatConversion result;
    result.document.default_channel = std::string(default_channel);
    for (const auto& character : renpy.characters)
        result.document.characters.push_back({character.alias, character.name});
    const auto parsed = parse_chat_program(
        renpy.script, ChatParseOptions{std::string(default_channel)});
    for (const auto& event : parsed.events)
        if (event.kind == ChatEventKind::Message || event.kind == ChatEventKind::Narration)
            result.document.events.push_back(event);
    const bool use_narrator = !narrator_alias.empty() && valid_chat_alias(narrator_alias);
    if (!narrator_alias.empty() && !use_narrator)
        result.document.diagnostics.push_back({{}, "Narrator alias must be a Ren'Py identifier.", false});
    for (auto& event : result.document.events) {
        if (event.kind == ChatEventKind::Message) event.channel = std::string(default_channel);
        else if (event.kind == ChatEventKind::Narration && use_narrator) {
            event.kind = ChatEventKind::Message;
            event.speaker = std::string(narrator_alias);
            event.channel = std::string(default_channel);
        }
    }
    if (use_narrator) {
        const auto exists = std::find_if(result.document.characters.begin(), result.document.characters.end(),
            [&](const auto& character) { return character.alias == narrator_alias; });
        if (exists == result.document.characters.end())
            result.document.characters.push_back({std::string(narrator_alias),
                narrator_name.empty() ? "Narrator" : std::string(narrator_name)});
    }
    result.text = format_chat_program(result.document);
    CountEvents(result.document.events, &result);
    return result;
}

ChatConversion convert_chat_to_manuscript(std::string_view script, bool ordinary_renpy) {
    ChatConversion result;
    result.document = parse_chat_program(script);
    for (const auto& character : result.document.characters) {
        if (character.is_player)
            result.losses.push_back({character.source, "Player ownership for " + character.name + " is not represented in regular dialogue.", true});
        if (!character.icon.empty())
            result.losses.push_back({character.source, "Icon metadata for " + character.name + " is not represented in regular dialogue.", true});
        if (character.name_color != "#FFF")
            result.losses.push_back({character.source, "Name color for " + character.name + " is not represented in regular dialogue.", true});
    }
    std::ostringstream output;
    RenderManuscript(result.document, result.document.events, ordinary_renpy ? 4 : 0,
                     ordinary_renpy, &output, &result.losses);
    result.text = output.str();
    CountEvents(result.document.events, &result);
    return result;
}

}  // namespace say_count
