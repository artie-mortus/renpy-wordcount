#pragma once

#include <wx/frame.h>
#include <wx/aui/framemanager.h>

#include "app/settings.h"
#include "core/parser.h"

namespace say_count::ui {

class EditorNotebook;
class SpeakerStatsPanel;
class OutlinePanel;

class MainFrame final : public wxFrame {
public:
    MainFrame();
    ~MainFrame() override;

private:
    void BuildMenus();
    void RestoreWindow();
    void OnAbout(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnCloseTab(wxCommandEvent& event);
    void OnNewTab(wxCommandEvent& event);
    void OnOpen(wxCommandEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnSaveAs(wxCommandEvent& event);
    void OnToggleWrap(wxCommandEvent& event);
    void OnFontSize(wxCommandEvent& event);
    void OnTheme(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);

    app::Settings settings_;
    EditorNotebook* notebook_ = nullptr;
    SpeakerStatsPanel* speaker_stats_ = nullptr;
    OutlinePanel* outline_ = nullptr;
    wxAuiManager manager_;
    app::EditorSettings editor_settings_;
    wxRect normal_geometry_;
    ScriptAnalysis analysis_;
};

}  // namespace say_count::ui
