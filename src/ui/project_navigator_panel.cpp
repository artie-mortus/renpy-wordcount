#include "ui/project_navigator_panel.h"

#include <algorithm>

#include <wx/button.h>
#include <wx/listbox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/style.h"

namespace say_count::ui {

ProjectNavigatorPanel::ProjectNavigatorPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {
    SetBackgroundColour(style::Colors().paper);
    auto* layout = new wxBoxSizer(wxVERTICAL);

    auto* kicker = new wxStaticText(this, wxID_ANY, "SCRIPT INDEX");
    kicker->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    kicker->SetForegroundColour(style::Colors().plum);
    layout->Add(kicker, 0, wxLEFT | wxRIGHT | wxTOP, 14);

    auto* heading = new wxStaticText(this, wxID_ANY, "Your story, by file");
    heading->SetFont(style::BodyFont(13, wxFONTWEIGHT_BOLD));
    layout->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, 14);

    summary_ = new wxStaticText(this, wxID_ANY, "Open a game to see its scripts.");
    summary_->SetName("Project script count");
    summary_->SetForegroundColour(style::Colors().ink_soft);
    layout->Add(summary_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);

    search_ = new wxTextCtrl(this, wxID_ANY);
    search_->SetName("Search project scripts");
    search_->SetHint("Find a script by folder or name");
    layout->Add(search_, 0, wxEXPAND | wxALL, 14);

    list_ = new wxListBox(this, wxID_ANY);
    list_->SetName("Project scripts");
    layout->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 14);

    empty_ = new wxStaticText(this, wxID_ANY, "No scripts match this search.");
    empty_->SetName("Project scripts empty state");
    empty_->SetForegroundColour(style::Colors().ink_soft);
    empty_->Hide();
    layout->Add(empty_, 1, wxEXPAND | wxLEFT | wxRIGHT, 14);

    open_ = new wxButton(this, wxID_ANY, "Open selected script");
    style::StylePrimaryButton(open_);
    open_->SetName("Open selected project script");
    layout->Add(open_, 0, wxALIGN_RIGHT | wxALL, 14);
    SetSizer(layout);

    search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshList(); });
    list_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) {
        open_->Enable(list_->GetSelection() != wxNOT_FOUND);
    });
    list_->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) { OpenSelection(); });
    open_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { OpenSelection(); });
    RefreshList();
}

void ProjectNavigatorPanel::SetProject(std::vector<ProjectScriptFile> scripts) {
    scripts_ = std::move(scripts);
    search_->Clear();
    summary_->SetLabel(scripts_.empty()
        ? "This game does not contain any .rpy scripts."
        : wxString::FromUTF8(std::to_string(scripts_.size()) +
                             " scripts \xc2\xb7 open only what you need"));
    RefreshList();
}

void ProjectNavigatorPanel::SetOpenHandler(OpenHandler handler) {
    open_handler_ = std::move(handler);
}

void ProjectNavigatorPanel::RefreshList() {
    const wxString query = search_->GetValue().Lower();
    visible_scripts_.clear();
    list_->Clear();
    for (std::size_t index = 0; index < scripts_.size(); ++index) {
        const wxString label = wxString::FromUTF8(scripts_[index].relative_path);
        if (!query.empty() && !label.Lower().Contains(query)) continue;
        visible_scripts_.push_back(index);
        list_->Append(label);
    }
    const bool has_matches = !visible_scripts_.empty();
    list_->Show(has_matches);
    empty_->Show(!has_matches && !scripts_.empty());
    if (has_matches) list_->SetSelection(0);
    open_->Enable(has_matches);
    Layout();
}

void ProjectNavigatorPanel::OpenSelection() {
    const int selection = list_->GetSelection();
    if (selection == wxNOT_FOUND ||
        static_cast<std::size_t>(selection) >= visible_scripts_.size() || !open_handler_) return;
    open_handler_(visible_scripts_[static_cast<std::size_t>(selection)]);
}

void ProjectNavigatorPanel::SelectPath(const wxString& absolute_path) {
    for (std::size_t visible = 0; visible < visible_scripts_.size(); ++visible) {
        const auto& script = scripts_[visible_scripts_[visible]];
        if (wxString::FromUTF8(script.absolute_path) != absolute_path) continue;
        list_->SetSelection(static_cast<int>(visible));
        list_->EnsureVisible(static_cast<int>(visible));
        return;
    }
}

}  // namespace say_count::ui
