#include "core/writer_draft.h"

#include <filesystem>
#include <fstream>
#include <sstream>

#include "core/manuscript.h"

namespace say_count {

std::string writer_draft_path(std::string_view script_path) {
    return std::string(script_path) + ".saycount.txt";
}

std::optional<std::string> load_writer_draft(const std::string& script_path, std::string* error) {
    if (error) error->clear();
    const std::string path = writer_draft_path(script_path);
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (!std::filesystem::exists(path)) return std::nullopt;
        if (error) *error = "The writing draft could not be read.";
        return std::nullopt;
    }
    std::ostringstream contents; contents << input.rdbuf();
    if (!input.good() && !input.eof()) {
        if (error) *error = "The writing draft could not be read completely.";
        return std::nullopt;
    }
    return contents.str();
}

bool save_writer_draft(const std::string& script_path, std::string_view writing, std::string* error) {
    if (error) error->clear();
    const std::filesystem::path path(writer_draft_path(script_path));
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);
    if (ec) { if (error) *error = "The writing draft folder could not be created."; return false; }
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    output.write(writing.data(), static_cast<std::streamsize>(writing.size())); output.close();
    if (!output) {
        std::filesystem::remove(temporary, ec);
        if (error) *error = "The writing draft could not be saved.";
        return false;
    }
    std::filesystem::rename(temporary, path, ec);
    if (!ec) return true;
    std::filesystem::remove(temporary, ec);
    if (error) *error = "The writing draft could not replace its previous version; the earlier draft was kept.";
    return false;
}

std::string render_writer_draft(std::string_view writing) {
    if (writing.find_first_not_of(" \t\r\n") == std::string_view::npos) return {};
    return convert_manuscript_to_renpy(writing).script;
}

WriterDraftState compare_writer_draft(std::string_view writing, std::string_view current_script) {
    if (writing.empty()) return WriterDraftState::NoDraft;
    return render_writer_draft(writing) == current_script ? WriterDraftState::InSync
                                                          : WriterDraftState::ScriptDiffers;
}

}  // namespace say_count
