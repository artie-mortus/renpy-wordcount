#include "core/chat_program_parser.h"

#include "core/tokenizer.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <sstream>

namespace say_count {
namespace {

std::string Trim(std::string value) {
    const auto space = [](unsigned char c) { return std::isspace(c) != 0; };
    value.erase(value.begin(), std::find_if_not(value.begin(), value.end(), space));
    value.erase(std::find_if_not(value.rbegin(), value.rend(), space).base(), value.end());
    return value;
}

std::vector<std::string> Lines(std::string_view input) {
    std::vector<std::string> result;
    std::istringstream stream{std::string(input)};
    for (std::string line; std::getline(stream, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        result.push_back(std::move(line));
    }
    return result;
}

std::size_t Indent(std::string_view line) {
    std::size_t result = 0;
    for (char c : line) {
        if (c == ' ') ++result;
        else if (c == '\t') result += 4;
        else break;
    }
    return result;
}

std::string Unescape(std::string_view value) {
    std::string result;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (value[i] == '\\' && i + 1 < value.size()) {
            const char next = value[++i];
            result += next == 'n' ? '\n' : next;
        } else if (value[i] == '[' && i + 1 < value.size() && value[i + 1] == '[') {
            result += '['; ++i;
        } else result += value[i];
    }
    return result;
}

std::vector<std::string> SplitArguments(std::string_view input) {
    std::vector<std::string> result;
    std::size_t begin = 0, depth = 0;
    char quote = 0;
    bool escaped = false;
    for (std::size_t i = 0; i <= input.size(); ++i) {
        const char c = i == input.size() ? ',' : input[i];
        if (escaped) escaped = false;
        else if (c == '\\' && quote) escaped = true;
        else if ((c == '"' || c == '\'') && (!quote || quote == c)) quote = quote ? 0 : c;
        else if (!quote && (c == '[' || c == '(')) ++depth;
        else if (!quote && (c == ']' || c == ')') && depth) --depth;
        else if (!quote && !depth && c == ',') {
            result.push_back(Trim(std::string(input.substr(begin, i - begin))));
            begin = i + 1;
        }
    }
    return result;
}

std::optional<std::string> QuotedValue(std::string value) {
    value = Trim(std::move(value));
    if (value.size() < 2 || (value.front() != '"' && value.front() != '\'') ||
        value.back() != value.front()) return std::nullopt;
    return Unescape(std::string_view(value).substr(1, value.size() - 2));
}

bool Identifier(std::string_view value) {
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value.front())) || value.front() == '_')) return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char c) {
        return std::isalnum(c) || c == '_';
    });
}

std::optional<ChatCharacter> ParseCharacter(std::string_view line, std::size_t number) {
    std::string text = Trim(std::string(line));
    if (text.rfind("default ", 0) != 0 && text.rfind("define ", 0) != 0) return std::nullopt;
    const auto first_space = text.find(' ');
    const auto equals = text.find('=', first_space + 1);
    if (equals == std::string::npos) return std::nullopt;
    const std::string alias = Trim(text.substr(first_space + 1, equals - first_space - 1));
    if (!Identifier(alias)) return std::nullopt;
    const auto call = text.find("ChatCharacter", equals);
    const auto open = text.find('(', call);
    const auto close = text.rfind(')');
    if (call == std::string::npos || open == std::string::npos || close <= open) return std::nullopt;
    const auto arguments = SplitArguments(std::string_view(text).substr(open + 1, close - open - 1));
    if (arguments.empty()) return std::nullopt;
    const auto name = QuotedValue(arguments.front());
    if (!name) return std::nullopt;
    ChatCharacter result{alias, *name};
    result.source = {number, 1};
    for (std::size_t i = 1; i < arguments.size(); ++i) {
        const auto equal = arguments[i].find('=');
        if (equal == std::string::npos) continue;
        const auto key = Trim(arguments[i].substr(0, equal));
        const auto value = Trim(arguments[i].substr(equal + 1));
        if (key == "icon") { if (auto parsed = QuotedValue(value)) result.icon = *parsed; }
        else if (key == "name_color") { if (auto parsed = QuotedValue(value)) result.name_color = *parsed; }
        else if (key == "is_player") result.is_player = value == "True";
    }
    return result;
}

std::optional<ChatEvent> ParseMessage(std::string_view line, std::size_t number,
                                      const std::string& inherited_channel,
                                      std::vector<ChatDiagnostic>* diagnostics) {
    const auto token = tokenize_line(line, number);
    if (token.type != LineType::Dialogue || token.quoted.empty()) return std::nullopt;
    std::string prefix = Trim(token.code.substr(0, token.quoted.front().begin));
    if (!Identifier(prefix)) return std::nullopt;
    ChatEvent event;
    event.kind = ChatEventKind::Message;
    event.speaker = prefix;
    event.text = Unescape(token.quoted.front().text);
    event.channel = inherited_channel;
    event.source = {number, Indent(line) + 1};
    const auto after_quote = token.quoted.front().end + 1;
    std::string suffix = Trim(token.code.substr(after_quote));
    if (suffix.empty()) return event;
    if (suffix.front() != '(' || suffix.back() != ')') {
        diagnostics->push_back({event.source, "Unsupported chat say suffix; line preserved unchanged.", false});
        ChatEvent passthrough;
        passthrough.kind = ChatEventKind::Passthrough;
        passthrough.original = std::string(line);
        passthrough.source = event.source;
        return passthrough;
    }
    for (const auto& argument : SplitArguments(std::string_view(suffix).substr(1, suffix.size() - 2))) {
        const auto equal = argument.find('=');
        if (equal == std::string::npos) {
            if (!argument.empty()) {
                event.extra_arguments.push_back(argument);
                diagnostics->push_back({event.source, "Unsupported chat argument: " + argument, false});
            }
            continue;
        }
        const auto key = Trim(argument.substr(0, equal));
        const auto value = Trim(argument.substr(equal + 1));
        if (key == "c") {
            if (auto parsed = QuotedValue(value)) { event.channel = *parsed; event.channel_explicit = true; }
        } else if (key == "ot") {
            std::string list = value;
            if (list.size() >= 2 && list.front() == '[' && list.back() == ']')
                list = list.substr(1, list.size() - 2);
            for (const auto& item : SplitArguments(list)) if (Identifier(item)) event.other_typers.push_back(item);
        } else if (key == "fastmode") {
            try {
                std::size_t consumed = 0;
                event.fast_mode = std::stod(value, &consumed);
                if (consumed != value.size() || !std::isfinite(event.fast_mode) || event.fast_mode < 0.0)
                    throw std::invalid_argument("fastmode");
                event.has_fast_mode = true;
            }
            catch (...) { diagnostics->push_back({event.source, "Invalid fastmode value.", false}); }
        } else {
            event.extra_arguments.push_back(argument);
            diagnostics->push_back({event.source, "Unsupported chat argument: " + key, false});
        }
    }
    return event;
}

std::optional<ChatEvent> ParseChoice(const std::vector<std::string>& lines,
                                     std::size_t* index, std::string* channel,
                                     std::size_t maximum_nesting,
                                     std::vector<ChatDiagnostic>* diagnostics) {
    if (!index || *index >= lines.size()) return std::nullopt;
    const auto token = tokenize_line(lines[*index], *index + 1);
    if (token.type != LineType::Narration || token.quoted.empty() ||
        token.code.empty() || token.code.back() != ':') return std::nullopt;
    ChatEvent choice;
    choice.kind = ChatEventKind::Choice;
    choice.text = Unescape(token.quoted.front().text);
    choice.source = {*index + 1, Indent(lines[*index]) + 1};
    const auto suffix_begin = token.quoted.front().end + 1;
    const auto suffix = Trim(token.code.substr(suffix_begin, token.code.size() - suffix_begin - 1));
    const auto choice_indent = Indent(lines[*index]);
    if (!suffix.empty()) {
        if (suffix.front() != '(' || suffix.back() != ')') {
            diagnostics->push_back({choice.source, "Unsupported choice suffix; block preserved unchanged.", false});
            ChatEvent passthrough;
            passthrough.kind = ChatEventKind::Passthrough;
            passthrough.original = lines[*index];
            passthrough.source = choice.source;
            while (*index + 1 < lines.size() &&
                   (Trim(lines[*index + 1]).empty() || Indent(lines[*index + 1]) > choice_indent)) {
                ++*index;
                passthrough.original += '\n' + lines[*index];
            }
            return passthrough;
        }
        for (const auto& argument : SplitArguments(std::string_view(suffix).substr(1, suffix.size() - 2))) {
            if (argument.empty()) continue;
            const auto equal = argument.find('=');
            const auto key = equal == std::string::npos ? std::string{} : Trim(argument.substr(0, equal));
            if (key == "auto_send") {
                const auto value = Trim(argument.substr(equal + 1));
                if (value == "False") choice.auto_send = false;
                else if (value == "True") choice.auto_send = true;
                else diagnostics->push_back({choice.source, "Invalid auto_send value.", false});
            } else {
                choice.extra_arguments.push_back(argument);
                diagnostics->push_back({choice.source, "Unsupported choice argument: " + argument, false});
            }
        }
    }
    while (*index + 1 < lines.size() &&
           (Trim(lines[*index + 1]).empty() || Indent(lines[*index + 1]) > choice_indent)) {
        ++*index;
        if (Trim(lines[*index]).empty() || Trim(lines[*index]) == "pass") continue;
        const auto child_token = tokenize_line(lines[*index], *index + 1);
        if (child_token.type == LineType::Menu) {
            const auto menu_indent = Indent(lines[*index]);
            const std::string menu_line = lines[*index];
            const ChatSourceLocation menu_source{*index + 1, menu_indent + 1};
            std::vector<ChatEvent> menu_children;
            bool menu_has_choice = false;
            while (*index + 1 < lines.size() && Indent(lines[*index + 1]) > menu_indent) {
                ++*index;
                if (Indent(lines[*index]) / 4 > maximum_nesting) {
                    const ChatSourceLocation source{*index + 1, Indent(lines[*index]) + 1};
                    diagnostics->push_back({source, "Chat input exceeds the configured nesting limit.", false});
                    ChatEvent passthrough;
                    passthrough.kind = ChatEventKind::Passthrough;
                    passthrough.original = lines[*index];
                    passthrough.source = source;
                    menu_children.push_back(std::move(passthrough));
                    continue;
                }
                if (auto nested = ParseChoice(lines, index, channel, maximum_nesting, diagnostics)) {
                    if (nested->kind == ChatEventKind::Choice && !menu_has_choice) {
                        nested->menu_start = true;
                        menu_has_choice = true;
                    }
                    menu_children.push_back(std::move(*nested));
                } else if (!Trim(lines[*index]).empty()) {
                    ChatEvent passthrough;
                    passthrough.kind = ChatEventKind::Passthrough;
                    passthrough.original = lines[*index];
                    passthrough.source = {*index + 1, 1};
                    menu_children.push_back(std::move(passthrough));
                }
            }
            if (!menu_has_choice) {
                ChatEvent passthrough;
                passthrough.kind = ChatEventKind::Passthrough;
                passthrough.original = menu_line;
                passthrough.source = menu_source;
                choice.children.push_back(std::move(passthrough));
            }
            for (auto& child : menu_children) choice.children.push_back(std::move(child));
        } else if (auto child = ParseMessage(lines[*index], *index + 1, *channel, diagnostics)) {
            if (child->channel_explicit) *channel = child->channel;
            choice.children.push_back(std::move(*child));
        } else if (child_token.type == LineType::Narration && !child_token.quoted.empty()) {
            ChatEvent narration;
            narration.kind = ChatEventKind::Narration;
            narration.text = Unescape(child_token.quoted.front().text);
            narration.source = {*index + 1, Indent(lines[*index]) + 1};
            choice.children.push_back(std::move(narration));
        } else {
            ChatEvent passthrough;
            passthrough.kind = ChatEventKind::Passthrough;
            passthrough.original = lines[*index];
            passthrough.source = {*index + 1, 1};
            choice.children.push_back(std::move(passthrough));
        }
    }
    return choice;
}

}  // namespace

bool valid_chat_alias(std::string_view value) noexcept { return Identifier(value); }

ChatDocument parse_chat_program(std::string_view script, const ChatParseOptions& options) {
    ChatDocument result;
    result.default_channel = options.default_channel;
    if (script.size() > options.maximum_input_bytes) {
        result.diagnostics.push_back({{}, "Chat input exceeds the configured size limit.", false});
        return result;
    }
    const auto lines = Lines(script);
    std::string channel = options.default_channel;
    // A plain "menu:" header is regenerated by the formatter, so it is held
    // back and only re-emitted verbatim if no choice claims it.
    std::optional<ChatEvent> pending_menu;
    const auto commit = [&](ChatEvent&& event) {
        if (pending_menu) {
            if (event.kind == ChatEventKind::Choice) event.menu_start = true;
            else result.events.push_back(std::move(*pending_menu));
            pending_menu.reset();
        }
        result.events.push_back(std::move(event));
    };
    const auto make_passthrough = [](std::string original, ChatSourceLocation source) {
        ChatEvent passthrough;
        passthrough.kind = ChatEventKind::Passthrough;
        passthrough.original = std::move(original);
        passthrough.source = source;
        return passthrough;
    };
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (Indent(lines[i]) / 4 > options.maximum_nesting) {
            const ChatSourceLocation source{i + 1, Indent(lines[i]) + 1};
            result.diagnostics.push_back({source, "Chat input exceeds the configured nesting limit.", false});
            commit(make_passthrough(lines[i], source));
            continue;
        }
        if (auto character = ParseCharacter(lines[i], i + 1)) {
            result.characters.push_back(std::move(*character));
            continue;
        }
        if (auto message = ParseMessage(lines[i], i + 1, channel, &result.diagnostics)) {
            if (message->channel_explicit) channel = message->channel;
            commit(std::move(*message));
            continue;
        }
        const auto token = tokenize_line(lines[i], i + 1);
        if (token.type == LineType::Blank || token.type == LineType::Comment) {
            result.events.push_back(make_passthrough(lines[i], {i + 1, 1}));
            continue;
        }
        if (token.type == LineType::Menu) {
            const auto menu_indent = Indent(lines[i]);
            if (Trim(lines[i]) == "menu:") {
                if (pending_menu) result.events.push_back(std::move(*pending_menu));
                pending_menu = make_passthrough(lines[i], {i + 1, menu_indent + 1});
            } else {
                const ChatSourceLocation source{i + 1, menu_indent + 1};
                result.diagnostics.push_back({source, "Unsupported menu header; block preserved unchanged.", false});
                auto passthrough = make_passthrough(lines[i], source);
                while (i + 1 < lines.size() &&
                       (Trim(lines[i + 1]).empty() || Indent(lines[i + 1]) > menu_indent)) {
                    ++i;
                    passthrough.original += '\n' + lines[i];
                }
                commit(std::move(passthrough));
            }
            continue;
        }
        if (token.type == LineType::Label) {
            commit(make_passthrough(lines[i], {i + 1, 1}));
            continue;
        }
        if (auto choice = ParseChoice(lines, &i, &channel, options.maximum_nesting,
                                      &result.diagnostics)) {
            commit(std::move(*choice));
            continue;
        }
        if (token.type == LineType::Narration && !token.quoted.empty()) {
            ChatEvent narration;
            narration.kind = ChatEventKind::Narration;
            narration.text = Unescape(token.quoted.front().text);
            narration.source = {i + 1, Indent(lines[i]) + 1};
            commit(std::move(narration));
            continue;
        }
        commit(make_passthrough(lines[i], {i + 1, 1}));
    }
    if (pending_menu) result.events.push_back(std::move(*pending_menu));
    return result;
}

bool semantically_equal(const ChatDocument& left, const ChatDocument& right) noexcept {
    const auto locations_equal = [](const ChatSourceLocation&, const ChatSourceLocation&) { return true; };
    const auto characters_equal = [&](const ChatCharacter& a, const ChatCharacter& b) {
        return a.alias == b.alias && a.name == b.name && a.icon == b.icon &&
               a.name_color == b.name_color && a.is_player == b.is_player &&
               locations_equal(a.source, b.source);
    };
    // Whitespace-only passthrough events carry no meaning.
    const auto meaningful = [](const std::vector<ChatEvent>& events) {
        std::vector<const ChatEvent*> result;
        for (const auto& event : events)
            if (event.kind != ChatEventKind::Passthrough || !Trim(event.original).empty())
                result.push_back(&event);
        return result;
    };
    const auto events_equal = [&](const auto& self, const ChatEvent& a, const ChatEvent& b) -> bool {
        if (a.kind != b.kind || a.speaker != b.speaker || a.text != b.text ||
            a.channel != b.channel || a.channel_explicit != b.channel_explicit ||
            a.menu_start != b.menu_start ||
            a.other_typers != b.other_typers || a.extra_arguments != b.extra_arguments ||
            a.fast_mode != b.fast_mode || a.has_fast_mode != b.has_fast_mode ||
            a.auto_send != b.auto_send || a.original != b.original) return false;
        const auto a_children = meaningful(a.children);
        const auto b_children = meaningful(b.children);
        if (a_children.size() != b_children.size()) return false;
        for (std::size_t i = 0; i < a_children.size(); ++i)
            if (!self(self, *a_children[i], *b_children[i])) return false;
        return true;
    };
    const auto left_events = meaningful(left.events);
    const auto right_events = meaningful(right.events);
    if (left.default_channel != right.default_channel ||
        left.characters.size() != right.characters.size() ||
        left_events.size() != right_events.size()) return false;
    for (std::size_t i = 0; i < left.characters.size(); ++i)
        if (!characters_equal(left.characters[i], right.characters[i])) return false;
    for (std::size_t i = 0; i < left_events.size(); ++i)
        if (!events_equal(events_equal, *left_events[i], *right_events[i])) return false;
    return true;
}

}  // namespace say_count
