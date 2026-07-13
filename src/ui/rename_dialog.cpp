#include "ui/rename_dialog.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace say_count::ui {

RenameDialog::RenameDialog(wxWindow* parent, const std::vector<NamedScript>& files)
    : wxDialog(parent, wxID_ANY, "Rename Ren'Py Symbol", wxDefaultPosition, wxSize(850, 600),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), files_(files) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* fields = new wxFlexGridSizer(3, 2, 8, 10);
    fields->AddGrowableCol(1, 1);
    fields->Add(new wxStaticText(this, wxID_ANY, "Symbol type"), 0, wxALIGN_CENTER_VERTICAL);
    kind_ = new wxChoice(this, wxID_ANY);
    kind_->Append("Speaker alias");
    kind_->Append("Label");
    kind_->SetSelection(0);
    fields->Add(kind_, 1, wxEXPAND);
    fields->Add(new wxStaticText(this, wxID_ANY, "Current name"), 0, wxALIGN_CENTER_VERTICAL);
    from_ = new wxTextCtrl(this, wxID_ANY);
    fields->Add(from_, 1, wxEXPAND);
    fields->Add(new wxStaticText(this, wxID_ANY, "New name"), 0, wxALIGN_CENTER_VERTICAL);
    to_ = new wxTextCtrl(this, wxID_ANY);
    fields->Add(to_, 1, wxEXPAND);
    layout->Add(fields, 0, wxEXPAND | wxALL, 12);
    auto* preview_button = new wxButton(this, wxID_ANY, "Preview Changes");
    layout->Add(preview_button, 0, wxLEFT | wxRIGHT | wxBOTTOM, 12);
    preview_ = new wxTextCtrl(this, wxID_ANY, "Enter both names, then preview the supported changes.",
                              wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    layout->Add(preview_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    auto* actions = new wxStdDialogButtonSizer();
    apply_ = new wxButton(this, wxID_OK, "Apply Rename");
    apply_->Enable(false);
    actions->AddButton(apply_);
    actions->AddButton(new wxButton(this, wxID_CANCEL, "Cancel"));
    actions->Realize();
    layout->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    SetSizer(layout);
    preview_button->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Preview(); });
    kind_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { Invalidate(); });
    from_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { Invalidate(); });
    to_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { Invalidate(); });
    CentreOnParent();
}

std::string RenameDialog::original_name() const {
    return from_->GetValue().Strip(wxString::both).ToStdString();
}

void RenameDialog::Invalidate() {
    plan_.reset();
    apply_->Enable(false);
}

void RenameDialog::Preview() {
    const auto kind = kind_->GetSelection() == 0 ? RenameKind::SpeakerAlias : RenameKind::Label;
    const std::string from = original_name();
    const std::string to = to_->GetValue().Strip(wxString::both).ToStdString();
    RenamePlan candidate = plan_symbol_rename(files_, kind, from, to);
    if (!candidate.error.empty()) {
        preview_->SetValue(wxString::FromUTF8(candidate.error));
        plan_.reset();
        apply_->Enable(false);
        return;
    }
    if (candidate.changes.empty()) {
        preview_->SetValue(
            "No supported declarations, dialogue aliases, or static jump/call targets match this name.");
        plan_.reset();
        apply_->Enable(false);
        return;
    }
    wxString text = wxString::Format("%zu line%s will change:\n\n", candidate.changes.size(),
                                     candidate.changes.size() == 1 ? "" : "s");
    for (const auto& change : candidate.changes) {
        text += wxString::Format("%s:%zu\n  %s\n→ %s\n\n", wxString::FromUTF8(change.file),
                                change.line, wxString::FromUTF8(change.before),
                                wxString::FromUTF8(change.after));
    }
    preview_->SetValue(text);
    plan_ = std::move(candidate);
    apply_->Enable(true);
}

}  // namespace say_count::ui
