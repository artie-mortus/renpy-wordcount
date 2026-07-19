#pragma once

#include <optional>
#include <vector>

#include <wx/dialog.h>
#include <wx/string.h>

#include "core/project_creation.h"

class wxListBox;

namespace say_count::ui {

enum class WelcomeAction { None, NewStory, OpenGame, OpenScript, RecentGame };

class WelcomeDialog final : public wxDialog {
public:
    WelcomeDialog(wxWindow* parent, const std::vector<wxString>& recent_games);
    WelcomeAction action() const { return action_; }
    wxString selected_game() const;

private:
    WelcomeAction action_ = WelcomeAction::None;
    wxListBox* recent_ = nullptr;
    std::vector<wxString> recent_games_;
};

std::optional<ProjectCreationPlan> ShowNewStoryDialog(wxWindow* parent);

}  // namespace say_count::ui
