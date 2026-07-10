#pragma once

#include <wx/frame.h>

#include "app/settings.h"

namespace say_count::ui {

class MainFrame final : public wxFrame {
public:
    MainFrame();

private:
    void BuildMenus();
    void RestoreWindow();
    void OnAbout(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnQuit(wxCommandEvent& event);

    app::Settings settings_;
    wxRect normal_geometry_;
};

}  // namespace say_count::ui
