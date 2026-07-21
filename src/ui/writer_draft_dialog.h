#pragma once

#include <string>

#include <wx/dialog.h>

class wxButton;
class wxNotebook;
class wxPanel;
class wxRadioBox;
class wxStaticText;
class wxTextCtrl;

namespace say_count::ui {

class WriterDraftDialog final : public wxDialog {
public:
    WriterDraftDialog(wxWindow* parent, std::string script_path, std::string current_script);
    const std::string& generated_script() const { return generated_script_; }
    bool script_differs() const { return script_differs_; }
    bool generates_chat() const;

private:
    void RefreshPreview();
    void UpdateWritingGuidance();
    void InsertWritingSnippet(const wxString& snippet, long selection_start,
                              long selection_length);
    bool SaveDraft();
    bool ConfirmClose();

    std::string script_path_;
    std::string current_script_;
    std::string generated_script_;
    bool script_differs_ = false;
    bool draft_dirty_ = false;
    wxTextCtrl* writing_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxNotebook* tabs_ = nullptr;
    wxRadioBox* output_format_ = nullptr;
    wxPanel* chat_options_ = nullptr;
    wxRadioBox* chat_style_ = nullptr;
    wxStaticText* chat_channel_label_ = nullptr;
    wxTextCtrl* chat_channel_ = nullptr;
    wxStaticText* writing_help_ = nullptr;
    wxButton* switch_chat_ = nullptr;
    wxStaticText* state_ = nullptr;
    wxButton* save_ = nullptr;
    wxButton* update_ = nullptr;
};

}  // namespace say_count::ui
