#pragma once

#include <wx/frame.h>

#include "app/settings.h"

namespace say_count::ui {

class EditorNotebook;

class MainFrame final : public wxFrame {
public:
    MainFrame();

private:
    void BuildMenus();
    void RestoreWindow();
    void OnAbout(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnCloseTab(wxCommandEvent& event);
    void OnNewTab(wxCommandEvent& event);
    void OnQuit(wxCommandEvent& event);

    app::Settings settings_;
    EditorNotebook* notebook_ = nullptr;
    wxRect normal_geometry_;
};

}  // namespace say_count::ui
