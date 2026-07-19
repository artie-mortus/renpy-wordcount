#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace say_count {

enum class WriterDraftState { NoDraft, InSync, ScriptDiffers };

std::string writer_draft_path(std::string_view script_path);
std::optional<std::string> load_writer_draft(const std::string& script_path,
                                             std::string* error = nullptr);
bool save_writer_draft(const std::string& script_path, std::string_view writing,
                       std::string* error = nullptr);
std::string render_writer_draft(std::string_view writing);
WriterDraftState compare_writer_draft(std::string_view writing, std::string_view current_script);

}  // namespace say_count
