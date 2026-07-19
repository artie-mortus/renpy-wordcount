#pragma once

#include <string>

#include <wx/dialog.h>

class wxButton;
class wxStaticText;
class wxTextCtrl;

namespace say_count::ui {

class WriterDraftDialog final : public wxDialog {
public:
    WriterDraftDialog(wxWindow* parent, std::string script_path, std::string current_script);
    const std::string& generated_script() const { return generated_script_; }
    bool script_differs() const { return script_differs_; }

private:
    void RefreshPreview();
    bool SaveDraft();
    bool ConfirmClose();

    std::string script_path_;
    std::string current_script_;
    std::string generated_script_;
    bool script_differs_ = false;
    bool draft_dirty_ = false;
    wxTextCtrl* writing_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxStaticText* state_ = nullptr;
    wxButton* save_ = nullptr;
    wxButton* update_ = nullptr;
};

}  // namespace say_count::ui
