#include "ui/writer_draft_dialog.h"

#include <wx/button.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
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

    tabs_ = new wxNotebook(this, wxID_ANY); tabs_->SetName("Writing and generated script");
    auto* writing_page = new wxPanel(tabs_); auto* writing_layout = new wxBoxSizer(wxVERTICAL);
    const wxString formats[] = {"Visual novel dialogue", "Social media chat"};
    output_format_ = new wxRadioBox(writing_page, wxID_ANY, "Make this as", wxDefaultPosition,
                                    wxDefaultSize, 2, formats, 2, wxRA_SPECIFY_COLS);
    output_format_->SetName("Writing draft output format");
    writing_layout->Add(output_format_, 0, wxEXPAND | wxALL, 12);

    chat_options_ = new wxPanel(writing_page);
    chat_options_->SetName("Writing draft chat options");
    auto* chat_layout = new wxBoxSizer(wxHORIZONTAL);
    const wxString chat_styles[] = {"Discord-style channels", "Kik-style messenger"};
    chat_style_ = new wxRadioBox(chat_options_, wxID_ANY, "Chat look", wxDefaultPosition,
                                 wxDefaultSize, 2, chat_styles, 1, wxRA_SPECIFY_ROWS);
    chat_style_->SetName("Writing draft chat style");
    chat_layout->Add(chat_style_, 0, wxRIGHT, 20);
    auto* channel_layout = new wxBoxSizer(wxVERTICAL);
    chat_channel_label_ = new wxStaticText(chat_options_, wxID_ANY, "Chat room name");
    channel_layout->Add(chat_channel_label_, 0, wxBOTTOM, 6);
    chat_channel_ = new wxTextCtrl(chat_options_, wxID_ANY, "#general");
    chat_channel_->SetName("Writing draft chat channel");
    channel_layout->Add(chat_channel_, 0, wxEXPAND);
    channel_layout->Add(new wxStaticText(chat_options_, wxID_ANY,
        "Stage directions such as [Robo is typing], [fast], and Choice: work here too."),
        0, wxTOP, 8);
    chat_layout->Add(channel_layout, 1, wxEXPAND | wxTOP, 4);
    chat_options_->SetSizer(chat_layout);
    chat_options_->Hide();
    writing_layout->Add(chat_options_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    auto* writing_help = new wxStaticText(writing_page, wxID_ANY,
        "Use Name: dialogue or dialogue, said Name. Quotation marks are optional. "
        "Use :: scene name for headings. Your writing is saved beside the .rpy file.");
    writing_help->Wrap(820); writing_layout->Add(writing_help, 0, wxEXPAND | wxALL, 12);
    writing_ = new wxTextCtrl(writing_page, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_RICH2);
    writing_->SetName("Natural writing draft"); writing_->SetFont(style::BodyFont(12));
    writing_layout->Add(writing_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12); writing_page->SetSizer(writing_layout);
    auto* preview_page = new wxPanel(tabs_); auto* preview_layout = new wxBoxSizer(wxVERTICAL);
    auto* preview_help = new wxStaticText(preview_page, wxID_ANY,
        "This exact Ren'Py will replace the current script only when you click Update game script.");
    preview_layout->Add(preview_help, 0, wxEXPAND | wxALL, 12);
    preview_ = new wxTextCtrl(preview_page, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    preview_->SetName("Generated game script"); preview_->SetFont(style::UtilityFont(10));
    preview_layout->Add(preview_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12); preview_page->SetSizer(preview_layout);
    tabs_->AddPage(writing_page, "Writing", true); tabs_->AddPage(preview_page, "Generated script");
    layout->Add(tabs_, 1, wxEXPAND | wxLEFT | wxRIGHT, 20);

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
    output_format_->Bind(wxEVT_RADIOBOX, [this](wxCommandEvent&) {
        chat_options_->Show(generates_chat());
        tabs_->SetPageText(1, generates_chat() ? "Generated chat script" : "Generated script");
        chat_options_->GetParent()->Layout();
        Layout();
        RefreshPreview();
    });
    chat_style_->Bind(wxEVT_RADIOBOX, [this](wxCommandEvent&) {
        const bool messenger = chat_style_->GetSelection() == 1;
        chat_channel_label_->SetLabel(messenger ? "Who the player is chatting with" : "Chat room name");
        if (messenger && chat_channel_->GetValue() == "#general")
            chat_channel_->ChangeValue("direct-message");
        else if (!messenger && chat_channel_->GetValue().Find('#') == wxNOT_FOUND)
            chat_channel_->ChangeValue("#general");
        chat_options_->Layout();
        RefreshPreview();
    });
    chat_channel_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshPreview(); });
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
    WriterDraftRenderOptions options;
    if (generates_chat()) {
        options.output = WriterDraftOutput::SocialMediaChat;
        options.chat_channel = chat_channel_->GetValue().ToStdString(wxConvUTF8);
        options.chat_skin = chat_style_->GetSelection() == 1 ? "kik" : "discord";
    }
    generated_script_ = render_writer_draft(
        writing_->GetValue().ToStdString(wxConvUTF8), options);
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

bool WriterDraftDialog::generates_chat() const {
    return output_format_ && output_format_->GetSelection() == 1;
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
