#pragma once

#include "core/chat_document.h"

namespace say_count {

ChatDocument parse_chat_program(std::string_view script,
                                const ChatParseOptions& options = {});
bool valid_chat_alias(std::string_view value) noexcept;
bool semantically_equal(const ChatDocument& left, const ChatDocument& right) noexcept;

}  // namespace say_count
