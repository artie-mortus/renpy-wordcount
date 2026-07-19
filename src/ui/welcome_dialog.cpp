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
    : wxDialog(parent, wxID_ANY, "Welcome to Say Count", wxDefaultPosition, wxSize(760, 600),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), recent_games_(recent_games) {
    SetBackgroundColour(style::Colors().canvas);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* cue = new wxPanel(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 5)));
    cue->SetBackgroundColour(style::Colors().cue); layout->Add(cue, 0, wxEXPAND);
    auto* kicker = new wxStaticText(this, wxID_ANY, "PLACES, EVERYONE");
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
    auto add = [&](const wxString& label, WelcomeAction action, bool primary) {
        auto* button = new wxButton(this, wxID_ANY, label); button->SetName(label);
        if (primary) style::StylePrimaryButton(button);
        button->Bind(wxEVT_BUTTON, [this, action](wxCommandEvent&) { action_ = action; EndModal(wxID_OK); });
        actions->Add(button, 1, wxRIGHT, FromDIP(10));
    };
    add("Start a new story", WelcomeAction::NewStory, true);
    add("Open a game", WelcomeAction::OpenGame, false);
    add("Open a script", WelcomeAction::OpenScript, false);
    layout->Add(actions, 0, wxEXPAND | wxALL, FromDIP(28));

    auto* steps = new wxPanel(this); steps->SetBackgroundColour(style::Colors().ink);
    auto* step_layout = new wxBoxSizer(wxHORIZONTAL);
    for (const auto& step : {"1  OPEN OR CREATE", "2  WRITE DIALOGUE", "3  PREVIEW GAME"}) {
        auto* label = new wxStaticText(steps, wxID_ANY, step);
        label->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD)); label->SetForegroundColour(style::Colors().white);
        step_layout->Add(label, 1, wxALL, FromDIP(14));
    }
    steps->SetSizer(step_layout); layout->Add(steps, 0, wxEXPAND | wxLEFT | wxRIGHT, FromDIP(28));

    layout->Add(new wxStaticText(this, wxID_ANY, "Continue writing"), 0, wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
    recent_ = new wxListBox(this, wxID_ANY); recent_->SetName("Recent games");
    for (const auto& path : recent_games_) recent_->Append(wxFileName(path).GetFullName() + "  —  " + path);
    recent_->Enable(!recent_games_.empty());
    if (recent_games_.empty()) recent_->Append("No recent games yet");
    recent_->Bind(wxEVT_LISTBOX_DCLICK, [this](wxCommandEvent&) {
        if (!recent_games_.empty() && recent_->GetSelection() != wxNOT_FOUND) {
            action_ = WelcomeAction::RecentGame; EndModal(wxID_OK);
        }
    });
    layout->Add(recent_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, FromDIP(28));
    auto* footer = new wxBoxSizer(wxHORIZONTAL);
    auto* open_recent = new wxButton(this, wxID_ANY, "Open selected game");
    open_recent->SetName("Open selected game"); open_recent->Enable(!recent_games_.empty());
    open_recent->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (recent_->GetSelection() == wxNOT_FOUND) return;
        action_ = WelcomeAction::RecentGame; EndModal(wxID_OK);
    });
    auto* close = new wxButton(this, wxID_CANCEL, "Not now");
    footer->Add(open_recent); footer->AddStretchSpacer(); footer->Add(close);
    layout->Add(footer, 0, wxEXPAND | wxALL, FromDIP(20));
    SetSizer(layout); CentreOnParent();
}

wxString WelcomeDialog::selected_game() const {
    const int selection = recent_ ? recent_->GetSelection() : wxNOT_FOUND;
    return selection >= 0 && static_cast<std::size_t>(selection) < recent_games_.size()
        ? recent_games_[static_cast<std::size_t>(selection)] : wxString{};
}

std::optional<ProjectCreationPlan> ShowNewStoryDialog(wxWindow* parent) {
    wxDialog dialog(parent, wxID_ANY, "Start a New Story", wxDefaultPosition, wxSize(620, 340));
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* heading = new wxStaticText(&dialog, wxID_ANY, "Name the story and choose where to keep it.");
    heading->SetFont(style::BodyFont(14, wxFONTWEIGHT_BOLD));
    layout->Add(heading, 0, wxEXPAND | wxALL, 18);
    layout->Add(new wxStaticText(&dialog, wxID_ANY, "Story name"), 0, wxLEFT | wxRIGHT, 18);
    auto* name = new wxTextCtrl(&dialog, wxID_ANY); name->SetName("Story name");
    layout->Add(name, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 18);
    layout->Add(new wxStaticText(&dialog, wxID_ANY, "Create inside"), 0, wxLEFT | wxRIGHT, 18);
    auto* folder = new wxDirPickerCtrl(&dialog, wxID_ANY, wxEmptyString, "Choose a folder");
    folder->SetName("Story parent folder"); layout->Add(folder, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 18);
    auto* buttons = dialog.CreateStdDialogButtonSizer(wxOK | wxCANCEL);
    layout->Add(buttons, 0, wxEXPAND | wxALL, 18); dialog.SetSizer(layout); dialog.CentreOnParent(); name->SetFocus();
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
