#pragma once

#include <map>
#include <string>
#include <string_view>

namespace say_count {

struct OfflineProseRewrite {
    std::string manuscript;
    std::size_t rewritten_lines = 0;
    std::string error;

    explicit operator bool() const noexcept { return error.empty() && rewritten_lines > 0; }
};

// Builds a prompt containing prose/review lines only. Recognized Ren'Py code is
// deliberately excluded before the local model process is started.
std::string build_offline_prose_prompt(
    std::string_view manuscript,
    const std::map<std::string, std::string>& existing_characters = {});

// Accepts only SC|line|N|text and SC|line|D|speaker|text records. Everything
// else from the model is ignored, and records may address prose/review lines only.
OfflineProseRewrite apply_offline_prose_response(
    std::string_view manuscript, std::string_view response);

}  // namespace say_count
