#pragma once

#include <wx/dialog.h>

#include "core/renpy_runtime.h"

class wxChoice;
class wxTextCtrl;

namespace say_count::ui {

class RuntimePresetDialog final : public wxDialog {
public:
    RuntimePresetDialog(wxWindow* parent, RuntimePresetStore& store,
                        std::string selected_name, std::string selected_json);
    std::string selected_name() const { return selected_name_; }
    std::string selected_json() const { return selected_json_; }
private:
    void LoadSelection();
    bool ValidateEditor();
    void SavePreset();
    void DeletePreset();
    void RebuildChoices(const std::string& selection = {});

    RuntimePresetStore& store_;
    std::map<std::string, std::string> presets_;
    wxChoice* choices_ = nullptr;
    wxTextCtrl* name_ = nullptr;
    wxTextCtrl* json_ = nullptr;
    std::string selected_name_;
    std::string selected_json_;
};

}  // namespace say_count::ui
