#pragma once

#include "core/chat_document.h"

namespace say_count {

// bridge_skin: when non-empty ("discord" or "kik"), the emitted scene is
// wrapped in say_count_chat_begin/say_count_chat_end calls for that skin.
std::string format_chat_program(const ChatDocument& document,
                                std::string_view label = "chat_scene",
                                bool include_definitions = true,
                                std::string_view bridge_skin = {});

}  // namespace say_count
