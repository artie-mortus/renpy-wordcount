#include "ui/conflict_dialog.h"

#include <wx/button.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>

namespace say_count::ui {
namespace {

constexpr int kChangedMarker = 1;

void ConfigureVersion(wxStyledTextCtrl* editor, const std::string& text,
                      const std::vector<std::size_t>& changed_rows) {
    editor->SetText(wxString::FromUTF8(text));
    editor->SetReadOnly(true);
    editor->SetWrapMode(wxSTC_WRAP_NONE);
    editor->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    editor->SetMarginWidth(0, editor->TextWidth(wxSTC_STYLE_LINENUMBER, "00000") + 8);
    editor->MarkerDefine(kChangedMarker, wxSTC_MARK_BACKGROUND,
                         wxColour("#000000"), wxColour("#ffe0a3"));
    for (const auto row : changed_rows) editor->MarkerAdd(static_cast<int>(row), kChangedMarker);
}

}  // namespace

ConflictDialog::ConflictDialog(wxWindow* parent, const ExternalConflict& conflict)
    : wxDialog(parent, wxID_ANY, "External edit conflict — " +
               wxFileName(wxString::FromUTF8(conflict.path)).GetFullName(),
               wxDefaultPosition, wxSize(1050, 650), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    const auto rows = build_conflict_diff(conflict.local_content, conflict.disk_content);
    std::string local, disk;
    std::vector<std::size_t> changed_rows;
    for (std::size_t index = 0; index < rows.size(); ++index) {
        if (index) { local.push_back('\n'); disk.push_back('\n'); }
        local += rows[index].local_text;
        disk += rows[index].disk_text;
        if (rows[index].changed) changed_rows.push_back(index);
    }

    auto* layout = new wxBoxSizer(wxVERTICAL);
    layout->Add(new wxStaticText(this, wxID_ANY,
        "This file changed on disk while local edits were unsaved. Changed rows are highlighted."),
        0, wxEXPAND | wxALL, 10);
    auto* headings = new wxBoxSizer(wxHORIZONTAL);
    headings->Add(new wxStaticText(this, wxID_ANY, "Local unsaved version"), 1, wxLEFT, 8);
    headings->Add(new wxStaticText(this, wxID_ANY, "Disk version"), 1, wxLEFT, 8);
    layout->Add(headings, 0, wxEXPAND | wxLEFT | wxRIGHT, 8);
    auto* versions = new wxBoxSizer(wxHORIZONTAL);
    auto* local_editor = new wxStyledTextCtrl(this, wxID_ANY);
    auto* disk_editor = new wxStyledTextCtrl(this, wxID_ANY);
    ConfigureVersion(local_editor, local, changed_rows);
    ConfigureVersion(disk_editor, disk, changed_rows);
    versions->Add(local_editor, 1, wxEXPAND | wxALL, 6);
    versions->Add(disk_editor, 1, wxEXPAND | wxALL, 6);
    layout->Add(versions, 1, wxEXPAND);

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    auto* keep = new wxButton(this, wxID_ANY, "Keep Local");
    auto* use_disk = new wxButton(this, wxID_ANY, "Use Disk");
    auto* save = new wxButton(this, wxID_ANY, "Save Local Elsewhere, Then Use Disk…");
    auto* later = new wxButton(this, wxID_CANCEL, "Review Later");
    actions->AddStretchSpacer();
    actions->Add(keep, 0, wxRIGHT, 8);
    actions->Add(use_disk, 0, wxRIGHT, 8);
    actions->Add(save, 0, wxRIGHT, 8);
    actions->Add(later, 0);
    layout->Add(actions, 0, wxEXPAND | wxALL, 10);
    SetSizer(layout);

    keep->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        choice_ = ConflictDialogChoice::KeepLocal; EndModal(wxID_OK);
    });
    use_disk->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        choice_ = ConflictDialogChoice::UseDisk; EndModal(wxID_OK);
    });
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        choice_ = ConflictDialogChoice::SaveElsewhere; EndModal(wxID_OK);
    });
    CentreOnParent();
}

}  // namespace say_count::ui
