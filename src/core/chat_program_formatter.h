#pragma once

#include "core/chat_document.h"

namespace say_count {

std::string format_chat_program(const ChatDocument& document,
                                std::string_view label = "chat_scene",
                                bool include_definitions = true);

}  // namespace say_count
