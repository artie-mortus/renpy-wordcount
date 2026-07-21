#include "ui/welcome_dialog.h"

#include <wx/button.h>
#include <wx/filepicker.h>
#include <wx/filename.h>
#include <wx/listbox.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/style.h"

namespace say_count::ui {

WelcomeDialog::WelcomeDialog(wxWindow* parent, const std::vector<wxString>& recent_games)
    : wxDialog(parent, wxID_ANY, "Home — Say Count", wxDefaultPosition, wxSize(760, 540),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), recent_games_(recent_games) {
    SetBackgroundColour(style::Colors().canvas);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* cue = new wxPanel(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 5)));
    cue->SetBackgroundColour(style::Colors().cue); layout->Add(cue, 0, wxEXPAND);
    auto* kicker = new wxStaticText(this, wxID_ANY, "TODAY'S CALL SHEET");
    kicker->SetFont(style::UtilityFont(9, wxFONTWEIGHT_BOLD)); kicker->SetForegroundColour(style::Colors().plum);
    layout->Add(kicker, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
    auto* heading = new wxStaticText(this, wxID_ANY, "What would you like to write?");
    heading->SetFont(style::BodyFont(23, wxFONTWEIGHT_BOLD));
    layout->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
    auto* detail = new wxStaticText(this, wxID_ANY,
        "Start with a complete game, continue one already on this computer, or work on a single script.");
    detail->SetForegroundColour(style::Colors().ink_soft); detail->Wrap(680);
    layout->Add(detail, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(28));

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    wxButton* primary_action = nullptr;
    auto add = [&](const wxString& label, WelcomeAction action, bool primary) {
        auto* button = new wxButton(this, wxID_ANY, label);
        button->SetName(label);
        if (primary) primary_action = button; else style::StyleSecondaryButton(button);
        button->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent&) { action_ = action; EndModal(wxID_OK); });
        actions->Add(button, 1, wxRIGHT, FromDIP(10));
    };
    add("Start a new story", WelcomeAction::NewStory, true);
    add("Open a game", WelcomeAction::OpenGame, false);
    add("Open a script", WelcomeAction::OpenScript, false);
    layout->Add(actions, 0, wxEXPAND | wxALL, FromDIP(28));

    auto* local_note = new wxStaticText(this, wxID_ANY,
        "Your scripts stay in the folder you choose. Say Count never replaces existing files without asking.");
    local_note->SetForegroundColour(style::Colors().ink_soft);
    local_note->Wrap(680);
    layout->Add(local_note, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(28));

    layout->Add(new wxStaticText(this, wxID_ANY, "Continue writing"), 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
    recent_ = new wxListBox(this, wxID_ANY); recent_->SetName("Recent games");
    for (const auto& path : recent_games_) recent_->Append(wxFileName(path).GetFullName() + "  —  " + path);
    recent_->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) {
        if (!recent_games_.empty() && recent_->GetSelection() != wxNOT_FOUND) {
            action_ = WelcomeAction::RecentGame; EndModal(wxID_OK);
        }
    });
    auto* recent_empty = new wxStaticText(this, wxID_ANY,
        "No recent stories yet. The next game you open will appear here.");
    recent_empty->SetName("Recent games empty state");
    recent_empty->SetForegroundColour(style::Colors().ink_soft);
    recent_empty->Show(recent_games_.empty());
    recent_->Show(!recent_games_.empty());
    layout->Add(recent_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(20));
    layout->Add(recent_empty, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(20));
    auto* footer = new wxBoxSizer(wxHORIZONTAL);
    auto* open_recent = new wxButton(this, wxID_ANY, "Open selected game");
    style::StyleSecondaryButton(open_recent);
    open_recent->SetName("Open selected game"); open_recent->Show(!recent_games_.empty());
    open_recent->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (recent_->GetSelection() == wxNOT_FOUND) return;
        action_ = WelcomeAction::RecentGame; EndModal(wxID_OK);
    });
    auto* close = new wxButton(this, wxID_CANCEL, "Not now");
    style::StyleSecondaryButton(close);
    footer->Add(open_recent); footer->AddStretchSpacer(); footer->Add(close);
    layout->Add(footer, 0, wxEXPAND | wxALL, FromDIP(20));
    SetSizer(layout);
    style::StylePrimaryButton(primary_action);
    primary_action->SetName("Start a new story");
    CentreOnParent();
}

wxString WelcomeDialog::selected_game() const {
    const int selection = recent_ ? recent_->GetSelection() : wxNOT_FOUND;
    return selection >= 0 && static_cast<std::size_t>(selection) < recent_games_.size()
        ? recent_games_[static_cast<std::size_t>(selection)] : wxString{};
}

std::optional<ProjectCreationPlan> ShowNewStoryDialog(wxWindow* parent, const wxString& initial_parent) {
    wxDialog dialog(parent, wxID_ANY, "Start a New Story", wxDefaultPosition, wxSize(640, 420));
    dialog.SetBackgroundColour(style::Colors().canvas);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* heading = new wxStaticText(&dialog, wxID_ANY, "Name the story and choose where to keep it.");
    heading->SetFont(style::BodyFont(14, wxFONTWEIGHT_BOLD));
    layout->Add(heading, 0, wxEXPAND | wxALL, 18);
    layout->Add(new wxStaticText(&dialog, wxID_ANY, "Story name"), 0, wxLEFT | wxRIGHT, 18);
    auto* name = new wxTextCtrl(&dialog, wxID_ANY); name->SetName("Story name");
    layout->Add(name, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 18);
    layout->Add(new wxStaticText(&dialog, wxID_ANY, "Save story in"), 0, wxLEFT | wxRIGHT, 18);
    auto* folder = new wxDirPickerCtrl(&dialog, wxID_ANY, initial_parent, "Choose a folder");
    folder->SetName("Story parent folder"); layout->Add(folder, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 18);
    layout->Add(new wxStaticText(&dialog, wxID_ANY, "Story folder"), 0, wxLEFT | wxRIGHT, 18);
    auto* destination = new wxStaticText(&dialog, wxID_ANY, {});
    destination->SetName("Story destination"); destination->SetForegroundColour(style::Colors().ink_soft);
    destination->SetFont(style::UtilityFont(9)); destination->Wrap(580);
    layout->Add(destination, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 18);
    auto* buttons = new wxStdDialogButtonSizer();
    auto* create = new wxButton(&dialog, wxID_OK, "Create story"); create->SetName("Create story");
    auto* cancel = new wxButton(&dialog, wxID_CANCEL, "Cancel"); style::StyleSecondaryButton(cancel);
    buttons->AddButton(create); buttons->AddButton(cancel); buttons->Realize();
    layout->Add(buttons, 0, wxEXPAND | wxALL, 18);
    auto refresh_destination = [&] {
        const std::string leaf = project_folder_name(name->GetValue().ToStdString(wxConvUTF8));
        const wxString parent_path = folder->GetPath();
        const bool valid = !leaf.empty() && wxFileName::DirExists(parent_path);
        destination->SetLabel(valid
            ? wxFileName(parent_path, wxString::FromUTF8(leaf)).GetFullPath()
            : "Choose a name and an existing folder.");
        create->Enable(valid);
        dialog.Layout();
    };
    name->Bind(wxEVT_TEXT, [&](wxCommandEvent&) { refresh_destination(); });
    folder->Bind(wxEVT_DIRPICKER_CHANGED, [&](wxFileDirPickerEvent&) { refresh_destination(); });
    dialog.SetSizer(layout); style::StylePrimaryButton(create);
    create->SetName("Create story"); refresh_destination(); dialog.CentreOnParent(); name->SetFocus();
    while (dialog.ShowModal() == wxID_OK) {
        ProjectCreationPlan plan;
        const auto result = plan_project_creation(folder->GetPath().ToStdString(wxConvUTF8),
                                                  name->GetValue().ToStdString(wxConvUTF8), &plan);
        if (result.success) return plan;
        wxMessageBox(wxString::FromUTF8(result.error), "Story cannot be created there",
                     wxOK | wxICON_WARNING, &dialog);
    }
    return std::nullopt;
}

}  // namespace say_count::ui
