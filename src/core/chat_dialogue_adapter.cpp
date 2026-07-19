#include "core/chat_dialogue_adapter.h"

#include "core/chat_program_formatter.h"
#include "core/chat_program_parser.h"
#include "core/manuscript.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <iomanip>
#include <optional>
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

std::string Trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && std::isspace(static_cast<unsigned char>(text[begin]))) ++begin;
    while (end > begin && std::isspace(static_cast<unsigned char>(text[end - 1]))) --end;
    return std::string(text.substr(begin, end - begin));
}

std::string Lower(std::string_view text) {
    std::string result(text);
    std::transform(result.begin(), result.end(), result.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return result;
}

bool StartsWith(const std::string& text, std::string_view prefix) {
    return text.size() >= prefix.size() && text.compare(0, prefix.size(), prefix) == 0;
}

bool EndsWith(const std::string& text, std::string_view suffix) {
    return text.size() >= suffix.size() &&
           text.compare(text.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::size_t IndentOf(std::string_view line) {
    std::size_t indent = 0;
    for (char c : line) {
        if (c == ' ') ++indent;
        else if (c == '\t') indent += 4;
        else break;
    }
    return indent;
}

std::string FormatNumber(double value) {
    std::ostringstream number;
    number << std::setprecision(12) << value;
    return number.str();
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

// --- Natural-language stage directions -------------------------------------
//
// Writers describe chat mechanics in prose with screenplay-style stage
// directions on their own line; the converter turns them into chat_program
// arguments so no code has to be written by hand:
//   [in #announcements]  [to Eileen]      -> channel / thread switch
//   [Robo is typing]  [Robo and Mel are typing]
//                                         -> ot= on the next message
//   [fast]  [fast 0.25]  [normal speed]   -> fastmode= until reset
//   [I am Eileen]                         -> is_player=True on that character
// A "Choice:" line starts a menu; each "- option" below it is one reply, and
// indented lines under an option are the branch that follows it.

enum class DirectiveKind { Channel, Typing, Fast, NormalSpeed, Player };

struct Directive {
    DirectiveKind kind = DirectiveKind::Channel;
    std::string value;
    std::vector<std::string> names;
    double fast_mode = 0.1;
};

std::vector<std::string> SplitNames(const std::string& list) {
    std::vector<std::string> names;
    std::string remaining = list;
    for (;;) {
        std::size_t cut = remaining.find(',');
        const std::size_t and_cut = Lower(remaining).find(" and ");
        if (and_cut != std::string::npos && (cut == std::string::npos || and_cut < cut)) {
            const auto name = Trim(remaining.substr(0, and_cut));
            if (!name.empty()) names.push_back(name);
            remaining = remaining.substr(and_cut + 5);
            continue;
        }
        if (cut == std::string::npos) break;
        const auto name = Trim(remaining.substr(0, cut));
        if (!name.empty()) names.push_back(name);
        remaining = remaining.substr(cut + 1);
    }
    const auto last = Trim(remaining);
    if (!last.empty()) names.push_back(last);
    return names;
}

std::optional<Directive> ParseDirective(const std::string& trimmed_line) {
    if (trimmed_line.size() < 3 || trimmed_line.front() != '[' || trimmed_line.back() != ']')
        return std::nullopt;
    const std::string inside = Trim(trimmed_line.substr(1, trimmed_line.size() - 2));
    if (inside.empty()) return std::nullopt;
    const std::string lower = Lower(inside);
    Directive directive;
    for (const char* prefix : {"in ", "channel ", "room ", "to ", "chat with ", "switch to "}) {
        if (StartsWith(lower, prefix)) {
            directive.kind = DirectiveKind::Channel;
            directive.value = Trim(inside.substr(std::string_view(prefix).size()));
            if (!directive.value.empty()) return directive;
        }
    }
    if (inside.front() == '#') {
        directive.kind = DirectiveKind::Channel;
        directive.value = inside;
        return directive;
    }
    for (const char* suffix : {" is typing", " are typing"}) {
        if (EndsWith(lower, suffix)) {
            directive.kind = DirectiveKind::Typing;
            directive.names = SplitNames(
                Trim(inside.substr(0, inside.size() - std::string_view(suffix).size())));
            if (!directive.names.empty()) return directive;
        }
    }
    if (lower == "normal" || lower == "normal speed" || lower == "normal pace") {
        directive.kind = DirectiveKind::NormalSpeed;
        return directive;
    }
    if (lower == "fast" || lower == "quickly") {
        directive.kind = DirectiveKind::Fast;
        directive.fast_mode = 0.1;
        return directive;
    }
    if (lower == "very fast") {
        directive.kind = DirectiveKind::Fast;
        directive.fast_mode = 0.05;
        return directive;
    }
    if (StartsWith(lower, "fast ")) {
        const std::string number = Trim(inside.substr(5));
        char* end = nullptr;
        const double value = std::strtod(number.c_str(), &end);
        if (end && *end == '\0' && value >= 0.0) {
            directive.kind = DirectiveKind::Fast;
            directive.fast_mode = value;
            return directive;
        }
        return std::nullopt;
    }
    for (const char* prefix : {"i am ", "player is ", "you are "}) {
        if (StartsWith(lower, prefix)) {
            directive.kind = DirectiveKind::Player;
            directive.value = Trim(inside.substr(std::string_view(prefix).size()));
            if (!directive.value.empty()) return directive;
        }
    }
    return std::nullopt;
}

bool IsChoiceHeader(const std::string& trimmed_line) {
    const std::string lower = Lower(trimmed_line);
    return lower == "choice:" || lower == "choices:" || lower == "menu:" || lower == "options:";
}

bool IsOptionLine(const std::string& trimmed_line) {
    return trimmed_line.size() > 2 &&
           (trimmed_line[0] == '-' || trimmed_line[0] == '*' || trimmed_line[0] == '>') &&
           trimmed_line[1] == ' ';
}

std::string OptionText(const std::string& trimmed_line) {
    std::string text = Trim(trimmed_line.substr(2));
    if (text.size() >= 2 && text.front() == '"' && text.back() == '"')
        text = text.substr(1, text.size() - 2);
    return text;
}

struct ConversionState {
    std::string channel;
    std::vector<std::string> pending_typer_names;
    double fast_mode = -1.0;
    std::string player_name;
    bool use_narrator = false;
    std::string narrator_alias;
};

class NaturalChatBuilder {
public:
    NaturalChatBuilder(ChatConversion* result, std::map<std::string, std::string> known,
                       std::string default_channel)
        : result_(result), known_(std::move(known)),
          default_channel_(std::move(default_channel)) {}

    std::string ResolveTyperAlias(const std::string& name) {
        const std::string lower = Lower(name);
        for (const auto& [alias, known_name] : known_)
            if (Lower(known_name) == lower || alias == lower) return alias;
        std::string alias;
        for (char c : lower) {
            if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') alias += c;
        }
        while (!alias.empty() && std::isdigit(static_cast<unsigned char>(alias.front())))
            alias.erase(alias.begin());
        if (alias.empty() || !valid_chat_alias(alias)) alias = "chat_character";
        while (known_.count(alias)) alias += '_';
        known_[alias] = name;
        result_->document.characters.push_back({alias, name});
        return alias;
    }

    std::vector<ChatEvent> ConvertSegment(const std::string& text, ConversionState* state) {
        std::vector<ChatEvent> events;
        if (Trim(text).empty()) return events;
        ManuscriptOptions options;
        options.label = "chat_scene";
        options.existing_characters = known_;
        const auto renpy = convert_manuscript_to_renpy(text, options);
        for (const auto& character : renpy.characters) {
            if (known_.count(character.alias)) continue;
            known_[character.alias] = character.name;
            result_->document.characters.push_back({character.alias, character.name});
        }
        const auto parsed = parse_chat_program(
            renpy.script, ChatParseOptions{default_channel_});
        for (const auto& parsed_event : parsed.events) {
            if (parsed_event.kind != ChatEventKind::Message &&
                parsed_event.kind != ChatEventKind::Narration)
                continue;
            ChatEvent event = parsed_event;
            if (event.kind == ChatEventKind::Narration && state->use_narrator) {
                event.kind = ChatEventKind::Message;
                event.speaker = state->narrator_alias;
            }
            if (event.kind == ChatEventKind::Message) {
                event.channel = state->channel;
                event.channel_explicit = false;
                if (state->fast_mode >= 0.0) {
                    event.has_fast_mode = true;
                    event.fast_mode = state->fast_mode;
                }
                if (!state->pending_typer_names.empty()) {
                    for (const auto& name : state->pending_typer_names)
                        event.other_typers.push_back(ResolveTyperAlias(name));
                    state->pending_typer_names.clear();
                }
            }
            events.push_back(std::move(event));
        }
        return events;
    }

    const std::map<std::string, std::string>& known() const noexcept { return known_; }

private:
    ChatConversion* result_;
    std::map<std::string, std::string> known_;
    std::string default_channel_;
};

std::vector<std::string> SplitLines(std::string_view text) {
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') { lines.push_back(current); current.clear(); }
        else if (c != '\r') current += c;
    }
    if (!current.empty()) lines.push_back(current);
    return lines;
}

void RenderManuscript(const ChatDocument& document, const std::vector<ChatEvent>& events,
                      std::size_t indent, bool renpy, std::ostringstream* output,
                      std::vector<ChatDiagnostic>* losses, std::string* last_channel,
                      double* fast_state) {
    const std::string pad(indent, ' ');
    for (std::size_t index = 0; index < events.size(); ++index) {
        const auto& event = events[index];
        if (event.kind == ChatEventKind::Message) {
            if (renpy) {
                *output << pad << event.speaker << " \"" << Escape(event.text) << "\"\n";
                if (event.channel_explicit)
                    losses->push_back({event.source, "Channel " + event.channel + " is not represented in regular dialogue.", true});
                if (!event.other_typers.empty())
                    losses->push_back({event.source, "Other-typer metadata is not represented in regular dialogue.", true});
                if (event.has_fast_mode)
                    losses->push_back({event.source, "fastmode is not represented in regular dialogue.", true});
            } else {
                // Chat-only mechanics become stage directions so the writer can
                // edit them as text and convert back without loss.
                if (!event.channel.empty() &&
                    (event.channel_explicit || event.channel != *last_channel)) {
                    *output << pad << "[in " << event.channel << "]\n";
                    *last_channel = event.channel;
                }
                if (!event.other_typers.empty()) {
                    *output << pad << '[';
                    for (std::size_t i = 0; i < event.other_typers.size(); ++i) {
                        if (i) *output << (i + 1 == event.other_typers.size() ? " and " : ", ");
                        *output << CharacterName(document, event.other_typers[i]);
                    }
                    *output << (event.other_typers.size() == 1 ? " is typing]\n" : " are typing]\n");
                }
                if (event.has_fast_mode && event.fast_mode != *fast_state) {
                    if (event.fast_mode == 0.1) *output << pad << "[fast]\n";
                    else *output << pad << "[fast " << FormatNumber(event.fast_mode) << "]\n";
                    *fast_state = event.fast_mode;
                } else if (!event.has_fast_mode && *fast_state >= 0.0) {
                    *output << pad << "[normal speed]\n";
                    *fast_state = -1.0;
                }
                *output << pad << CharacterName(document, event.speaker) << ": " << event.text << '\n';
            }
            for (const auto& argument : event.extra_arguments)
                losses->push_back({event.source, "Argument " + argument + " is not represented in regular dialogue.", true});
        } else if (event.kind == ChatEventKind::Narration) {
            *output << pad << (renpy ? "\"" + Escape(event.text) + "\"" : event.text) << '\n';
        } else if (event.kind == ChatEventKind::Choice) {
            *output << pad << (renpy ? "menu:\n" : "Choice:\n");
            bool first_choice = true;
            while (index < events.size() && events[index].kind == ChatEventKind::Choice &&
                   (first_choice || !events[index].menu_start)) {
                const auto& choice = events[index];
                if (renpy) {
                    *output << pad << "    \"" << Escape(choice.text) << "\":\n";
                    if (choice.children.empty()) *output << pad << "        pass\n";
                    else RenderManuscript(document, choice.children, indent + 8, true, output,
                                          losses, last_channel, fast_state);
                } else {
                    *output << pad << "- " << choice.text << '\n';
                    RenderManuscript(document, choice.children, indent + 4, false, output,
                                     losses, last_channel, fast_state);
                }
                if (!choice.auto_send)
                    losses->push_back({choice.source, "auto_send=False is not represented in regular dialogue.", true});
                for (const auto& argument : choice.extra_arguments)
                    losses->push_back({choice.source, "Argument " + argument + " is not represented in regular dialogue.", true});
                first_choice = false;
                ++index;
            }
            --index;
        } else if (event.kind == ChatEventKind::Passthrough) {
            *output << event.original << '\n';
        }
    }
}

}  // namespace

ChatConversion convert_manuscript_to_chat(
    std::string_view manuscript, std::string_view default_channel,
    const std::map<std::string, std::string>& existing_characters,
    std::string_view narrator_alias, std::string_view narrator_name,
    std::string_view bridge_skin) {
    ChatConversion result;
    result.document.default_channel = std::string(default_channel);

    ConversionState state;
    state.channel = std::string(default_channel);
    state.use_narrator = !narrator_alias.empty() && valid_chat_alias(narrator_alias);
    state.narrator_alias = std::string(narrator_alias);
    if (!narrator_alias.empty() && !state.use_narrator)
        result.document.diagnostics.push_back({{}, "Narrator alias must be a Ren'Py identifier.", false});

    NaturalChatBuilder builder(&result, existing_characters, std::string(default_channel));
    const auto lines = SplitLines(manuscript);
    std::string segment;
    const auto flush = [&]() {
        auto events = builder.ConvertSegment(segment, &state);
        for (auto& event : events) result.document.events.push_back(std::move(event));
        segment.clear();
    };

    for (std::size_t index = 0; index < lines.size(); ++index) {
        const std::string trimmed = Trim(lines[index]);
        if (const auto directive = ParseDirective(trimmed)) {
            flush();
            switch (directive->kind) {
                case DirectiveKind::Channel: state.channel = directive->value; break;
                case DirectiveKind::Typing: state.pending_typer_names = directive->names; break;
                case DirectiveKind::Fast: state.fast_mode = directive->fast_mode; break;
                case DirectiveKind::NormalSpeed: state.fast_mode = -1.0; break;
                case DirectiveKind::Player: state.player_name = directive->value; break;
            }
            continue;
        }
        if (!trimmed.empty() && trimmed.front() == '[' && trimmed.back() == ']') {
            result.document.diagnostics.push_back({{index + 1, 0},
                "Stage direction " + trimmed + " was not understood; kept as narration.", false});
        }
        if (IsChoiceHeader(trimmed)) {
            flush();
            bool first_option = true;
            while (index + 1 < lines.size()) {
                const std::string option_trimmed = Trim(lines[index + 1]);
                if (option_trimmed.empty()) {
                    // A blank line ends the menu unless another option follows.
                    if (index + 2 < lines.size() && IsOptionLine(Trim(lines[index + 2]))) {
                        ++index;
                        continue;
                    }
                    break;
                }
                if (!IsOptionLine(option_trimmed)) break;
                ++index;
                const std::size_t option_indent = IndentOf(lines[index]);
                ChatEvent choice;
                choice.kind = ChatEventKind::Choice;
                choice.text = OptionText(option_trimmed);
                choice.menu_start = first_option;
                first_option = false;
                std::string branch;
                std::size_t branch_indent = std::string::npos;
                while (index + 1 < lines.size()) {
                    const std::string& child = lines[index + 1];
                    const std::string child_trimmed = Trim(child);
                    if (child_trimmed.empty()) break;
                    if (IndentOf(child) <= option_indent) break;
                    if (IsChoiceHeader(child_trimmed))
                        result.document.diagnostics.push_back({{index + 2, 0},
                            "Choices inside a choice branch are not supported in prose form; "
                            "the line was kept as narration.", false});
                    branch_indent = std::min(branch_indent, IndentOf(child));
                    branch += child + '\n';
                    ++index;
                }
                if (!branch.empty()) {
                    std::string dedented;
                    for (const auto& child : SplitLines(branch))
                        dedented += (child.size() > branch_indent ? child.substr(branch_indent)
                                                                  : Trim(child)) + '\n';
                    choice.children = builder.ConvertSegment(dedented, &state);
                }
                result.document.events.push_back(std::move(choice));
            }
            continue;
        }
        segment += lines[index] + '\n';
    }
    flush();

    // Mark the player character so messenger skins can align their bubbles.
    bool player_marked = false;
    for (auto& character : result.document.characters) {
        const std::string lower = Lower(character.name);
        const bool implicit = lower == "me" || lower == "i";
        const bool requested = !state.player_name.empty() &&
            lower == Lower(state.player_name);
        if (implicit || requested) {
            character.is_player = true;
            player_marked = player_marked || requested;
        }
    }
    if (!state.player_name.empty() && !player_marked) {
        result.document.diagnostics.push_back({{},
            "Player character \"" + state.player_name + "\" was not found among the "
            "characters created here. If it is defined elsewhere in the project, add "
            "is_player=True to its ChatCharacter definition.", false});
    }

    if (state.use_narrator) {
        const auto exists = std::find_if(result.document.characters.begin(),
            result.document.characters.end(),
            [&](const auto& character) { return character.alias == narrator_alias; });
        const auto known = builder.known().find(std::string(narrator_alias));
        if (exists == result.document.characters.end() && known == builder.known().end())
            result.document.characters.push_back({std::string(narrator_alias),
                narrator_name.empty() ? "Narrator" : std::string(narrator_name)});
    }
    result.text = format_chat_program(result.document, "chat_scene", true, bridge_skin);
    CountEvents(result.document.events, &result);
    return result;
}

ChatConversion convert_chat_to_manuscript(std::string_view script, bool ordinary_renpy) {
    ChatConversion result;
    result.document = parse_chat_program(script);
    std::ostringstream output;
    for (const auto& character : result.document.characters) {
        if (character.is_player) {
            if (ordinary_renpy)
                result.losses.push_back({character.source, "Player ownership for " + character.name + " is not represented in regular dialogue.", true});
            else
                output << "[I am " << character.name << "]\n";
        }
        if (!character.icon.empty())
            result.losses.push_back({character.source, "Icon metadata for " + character.name + " is not represented in regular dialogue.", true});
        if (character.name_color != "#FFF")
            result.losses.push_back({character.source, "Name color for " + character.name + " is not represented in regular dialogue.", true});
    }
    std::string last_channel = result.document.default_channel;
    double fast_state = -1.0;
    RenderManuscript(result.document, result.document.events, ordinary_renpy ? 4 : 0,
                     ordinary_renpy, &output, &result.losses, &last_channel, &fast_state);
    result.text = output.str();
    CountEvents(result.document.events, &result);
    return result;
}

}  // namespace say_count
