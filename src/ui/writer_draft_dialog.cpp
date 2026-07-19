#include "ui/writer_draft_dialog.h"

#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "core/writer_draft.h"
#include "ui/style.h"

namespace say_count::ui {

WriterDraftDialog::WriterDraftDialog(wxWindow* parent, std::string script_path, std::string current_script)
    : wxDialog(parent, wxID_ANY, "Writing Draft", wxDefaultPosition, wxSize(940, 760),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      script_path_(std::move(script_path)), current_script_(std::move(current_script)) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* cue = new wxPanel(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 4)));
    cue->SetBackgroundColour(style::Colors().cue); layout->Add(cue, 0, wxEXPAND);
    auto* kicker = new wxStaticText(this, wxID_ANY, "WRITING SOURCE");
    kicker->SetFont(style::UtilityFont(9, wxFONTWEIGHT_BOLD)); kicker->SetForegroundColour(style::Colors().plum);
    layout->Add(kicker, 0, wxLEFT | wxRIGHT | wxTOP, 20);
    auto* heading = new wxStaticText(this, wxID_ANY, "Write naturally. Update the game script when you choose.");
    heading->SetFont(style::BodyFont(17, wxFONTWEIGHT_BOLD)); layout->Add(heading, 0, wxALL, 20);
    state_ = new wxStaticText(this, wxID_ANY, wxEmptyString); state_->SetName("Writing draft state");
    state_->SetFont(style::BodyFont(10, wxFONTWEIGHT_MEDIUM));
    layout->Add(state_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);

    auto* tabs = new wxNotebook(this, wxID_ANY); tabs->SetName("Writing and generated script");
    auto* writing_page = new wxPanel(tabs); auto* writing_layout = new wxBoxSizer(wxVERTICAL);
    auto* writing_help = new wxStaticText(writing_page, wxID_ANY,
        "Use Name: dialogue, ordinary narration, and :: scene name headings. Your writing is saved beside the .rpy file.");
    writing_help->Wrap(820); writing_layout->Add(writing_help, 0, wxEXPAND | wxALL, 12);
    writing_ = new wxTextCtrl(writing_page, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_RICH2);
    writing_->SetName("Natural writing draft"); writing_->SetFont(style::BodyFont(12));
    writing_layout->Add(writing_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12); writing_page->SetSizer(writing_layout);
    auto* preview_page = new wxPanel(tabs); auto* preview_layout = new wxBoxSizer(wxVERTICAL);
    auto* preview_help = new wxStaticText(preview_page, wxID_ANY,
        "This exact Ren'Py will replace the current script only when you click Update game script.");
    preview_layout->Add(preview_help, 0, wxEXPAND | wxALL, 12);
    preview_ = new wxTextCtrl(preview_page, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    preview_->SetName("Generated game script"); preview_->SetFont(style::UtilityFont(10));
    preview_layout->Add(preview_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12); preview_page->SetSizer(preview_layout);
    tabs->AddPage(writing_page, "Writing", true); tabs->AddPage(preview_page, "Generated script");
    layout->Add(tabs, 1, wxEXPAND | wxLEFT | wxRIGHT, 20);

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    save_ = new wxButton(this, wxID_APPLY, "Save writing draft"); save_->SetName("Save writing draft");
    update_ = new wxButton(this, wxID_OK, "Update game script...");
    style::StylePrimaryButton(update_); update_->SetName("Update game script");
    auto* close = new wxButton(this, wxID_CANCEL, "Close");
    actions->Add(save_); actions->AddStretchSpacer(); actions->Add(update_, 0, wxRIGHT, 8); actions->Add(close);
    layout->Add(actions, 0, wxEXPAND | wxALL, 20); SetSizer(layout); CentreOnParent();

    std::string error;
    if (const auto draft = load_writer_draft(script_path_, &error)) writing_->ChangeValue(wxString::FromUTF8(*draft));
    else if (!error.empty()) wxMessageBox(wxString::FromUTF8(error), "Writing draft unavailable", wxOK | wxICON_WARNING, this);
    writing_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { draft_dirty_ = true; RefreshPreview(); });
    save_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SaveDraft(); });
    update_->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event) {
        if (!SaveDraft()) return;
        event.Skip();
    });
    close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { if (ConfirmClose()) EndModal(wxID_CANCEL); });
    Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event) {
        if (ConfirmClose()) event.Skip(); else event.Veto();
    });
    RefreshPreview(); writing_->SetFocus();
}

void WriterDraftDialog::RefreshPreview() {
    generated_script_ = render_writer_draft(writing_->GetValue().ToStdString(wxConvUTF8));
    preview_->ChangeValue(wxString::FromUTF8(generated_script_));
    script_differs_ = !generated_script_.empty() && generated_script_ != current_script_;
    if (generated_script_.empty()) {
        state_->SetLabel("Start writing to create a game-script preview.");
        state_->SetForegroundColour(style::Colors().ink_soft);
    } else if (script_differs_) {
        state_->SetLabel("The current game script differs. Updating will replace its manual code edits after confirmation.");
        state_->SetForegroundColour(wxColour("#9A3E2D"));
    } else {
        state_->SetLabel("Writing draft and game script match.");
        state_->SetForegroundColour(wxColour("#245C43"));
    }
    save_->Enable(draft_dirty_);
    update_->Enable(!generated_script_.empty() && script_differs_);
}

bool WriterDraftDialog::SaveDraft() {
    if (!draft_dirty_) return true;
    std::string error;
    if (!save_writer_draft(script_path_, writing_->GetValue().ToStdString(wxConvUTF8), &error)) {
        wxMessageBox(wxString::FromUTF8(error), "Writing draft not saved", wxOK | wxICON_ERROR, this);
        return false;
    }
    draft_dirty_ = false; save_->Enable(false); state_->SetLabel("Writing draft saved locally. The game script has not changed.");
    return true;
}

bool WriterDraftDialog::ConfirmClose() {
    if (!draft_dirty_) return true;
    wxMessageDialog prompt(this, "Save the writing draft before closing?\n\nThe game script will not be changed.",
                           "Unsaved writing draft", wxYES_NO | wxCANCEL | wxICON_QUESTION);
    prompt.SetYesNoCancelLabels("Save draft", "Discard draft changes", "Keep writing");
    const int result = prompt.ShowModal();
    if (result == wxID_CANCEL) return false;
    return result == wxID_NO || SaveDraft();
}

}  // namespace say_count::ui
