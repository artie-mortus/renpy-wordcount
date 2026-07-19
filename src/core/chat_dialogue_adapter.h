#pragma once

#include "core/chat_document.h"

namespace say_count {

// bridge_skin: when non-empty ("discord" or "kik"), the generated scene is
// wrapped in say_count_chat_begin/say_count_chat_end calls for that skin.
ChatConversion convert_manuscript_to_chat(
    std::string_view manuscript, std::string_view default_channel = "#general",
    const std::map<std::string, std::string>& existing_characters = {},
    std::string_view narrator_alias = {}, std::string_view narrator_name = {},
    std::string_view bridge_skin = {});
ChatConversion convert_chat_to_manuscript(
    std::string_view script, bool ordinary_renpy = false);

}  // namespace say_count
