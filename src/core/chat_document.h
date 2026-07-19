#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace say_count {

struct ChatSourceLocation {
    std::size_t line = 0;
    std::size_t column = 0;
};

struct ChatCharacter {
    std::string alias;
    std::string name;
    std::string icon;
    std::string name_color = "#FFF";
    bool is_player = false;
    ChatSourceLocation source;
};

enum class ChatEventKind { Message, Choice, Narration, Passthrough };

struct ChatEvent {
    ChatEventKind kind = ChatEventKind::Passthrough;
    std::string speaker;
    std::string text;
    std::string channel;
    bool channel_explicit = false;
    bool menu_start = false;
    std::vector<std::string> other_typers;
    std::vector<std::string> extra_arguments;
    double fast_mode = -1.0;
    bool has_fast_mode = false;
    bool auto_send = true;
    std::vector<ChatEvent> children;
    std::string original;
    ChatSourceLocation source;
};

struct ChatDiagnostic {
    ChatSourceLocation source;
    std::string message;
    bool metadata_loss = false;
};

struct ChatDocument {
    std::vector<ChatCharacter> characters;
    std::vector<ChatEvent> events;
    std::vector<ChatDiagnostic> diagnostics;
    std::string default_channel;
};

struct ChatParseOptions {
    std::string default_channel;
    std::size_t maximum_input_bytes = 2 * 1024 * 1024;
    std::size_t maximum_nesting = 32;
};

struct ChatConversion {
    std::string text;
    ChatDocument document;
    std::size_t messages = 0;
    std::size_t narration = 0;
    std::size_t choices = 0;
    std::vector<ChatDiagnostic> losses;
};

}  // namespace say_count
