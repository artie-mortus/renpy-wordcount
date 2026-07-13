#include "ui/snapshot_dialog.h"

#include <wx/button.h>
#include <wx/datetime.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace say_count::ui {
namespace {

wxString StatusText(SnapshotFileStatus status) {
    switch (status) {
        case SnapshotFileStatus::Changed: return "changed";
        case SnapshotFileStatus::Added: return "restored only";
        case SnapshotFileStatus::Removed: return "current only";
        case SnapshotFileStatus::Unchanged: return "unchanged";
    }
    return {};
}

wxString SnapshotName(const SnapshotMetadata& snapshot) {
    wxDateTime time(static_cast<time_t>(snapshot.id / 1000));
    const wxString label = snapshot.label.empty()
        ? wxString(snapshot.automatic ? "Automatic" : "Manual")
        : wxString::FromUTF8(snapshot.label);
    return wxString::Format("%s — %s — %zu words — %zu files",
                            time.FormatISOCombined(' '), label,
                            snapshot.word_count, snapshot.file_count);
}

}  // namespace

SnapshotDialog::SnapshotDialog(wxWindow* parent, SnapshotStore& store,
                               const std::vector<NamedScript>& current)
    : wxDialog(parent, wxID_ANY, "Snapshots", wxDefaultPosition, wxSize(820, 600),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      store_(store), current_(current), snapshots_(store.List()) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    layout->Add(new wxStaticText(this, wxID_ANY,
        "Select a snapshot to preview how it differs from the current editor state."),
        0, wxEXPAND | wxALL, 10);
    list_ = new wxListBox(this, wxID_ANY);
    for (const auto& snapshot : snapshots_) list_->Append(SnapshotName(snapshot));
    layout->Add(list_, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);
    preview_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    layout->Add(preview_, 1, wxEXPAND | wxALL, 10);
    auto* actions = new wxStdDialogButtonSizer();
    auto* restore = new wxButton(this, wxID_OK, "Restore Selected...");
    actions->AddButton(restore);
    actions->AddButton(new wxButton(this, wxID_CANCEL, "Close"));
    actions->Realize();
    layout->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizer(layout);
    restore->Enable(false);
    list_->Bind(wxEVT_LISTBOX, [this, restore](wxCommandEvent&) {
        ShowSelection();
        restore->Enable(selected_id_.has_value());
    });
    if (snapshots_.empty()) preview_->SetValue("No snapshots have been stored yet.");
    else { list_->SetSelection(0); ShowSelection(); restore->Enable(true); }
    CentreOnParent();
}

void SnapshotDialog::ShowSelection() {
    const int selection = list_->GetSelection();
    if (selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= snapshots_.size()) return;
    const auto& metadata = snapshots_[static_cast<std::size_t>(selection)];
    std::string error;
    const auto snapshot = store_.Load(metadata.id, &error);
    if (!snapshot) {
        selected_id_.reset();
        preview_->SetValue("Could not load snapshot: " + wxString::FromUTF8(error));
        return;
    }
    selected_id_ = metadata.id;
    const auto comparison = compare_snapshot(current_, *snapshot);
    const auto delta = static_cast<long long>(comparison.snapshot_words) -
                       static_cast<long long>(comparison.current_words);
    wxString text = wxString::Format(
        "Project: %s\nCurrent: %zu words   Snapshot: %zu words   Difference: %s%lld\n"
        "Files that would change: %zu\n\n",
        snapshot->metadata.project_root.empty() ? "(no connected folder)"
                                                : wxString::FromUTF8(snapshot->metadata.project_root),
        comparison.current_words, comparison.snapshot_words, delta >= 0 ? "+" : "", delta,
        comparison.changed_files);
    for (const auto& file : comparison.files) {
        text += wxString::Format("%-14s  %s  (%zu → %zu words)\n", StatusText(file.status),
                                wxString::FromUTF8(file.name), file.current_words,
                                file.snapshot_words);
    }
    preview_->SetValue(text);
}

}  // namespace say_count::ui
