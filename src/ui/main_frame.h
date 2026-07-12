#pragma once

#include <wx/frame.h>
#include <wx/aui/framemanager.h>

#include "app/settings.h"
#include "core/parser.h"
#include "core/find_replace.h"

class wxPanel;
class wxTextCtrl;
class wxCheckBox;
class wxStaticText;
class wxDataViewListCtrl;
class wxDataViewEvent;

namespace say_count::ui {

class EditorNotebook;
class SpeakerStatsPanel;
class OutlinePanel;
class DiagnosticsPanel;
struct FindStatus;

class MainFrame final : public wxFrame {
public:
    MainFrame();
    ~MainFrame() override;

private:
    void BuildMenus();
    void BuildFindBar();
    void BuildFindResults();
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
    void OnOpenFind(wxCommandEvent& event);
    void OnFindNext(wxCommandEvent& event);
    void OnFindChanged(wxCommandEvent& event);
    void OnReplace(wxCommandEvent& event);
    void OnReplaceAll(wxCommandEvent& event);
    void OnCloseFind(wxCommandEvent& event);
    void OnCharHook(wxKeyEvent& event);
    void OnGoToLine(wxCommandEvent& event);
    void OnToggleComment(wxCommandEvent& event);
    void OnShowShortcuts(wxCommandEvent& event);
    void OnFindResultActivated(wxDataViewEvent& event);
    FindOptions CurrentFindOptions() const;
    void UpdateFindStatus(const FindStatus& status);
    void RefreshProjectFindResults(bool valid);

    app::Settings settings_;
    EditorNotebook* notebook_ = nullptr;
    SpeakerStatsPanel* speaker_stats_ = nullptr;
    OutlinePanel* outline_ = nullptr;
    DiagnosticsPanel* diagnostics_ = nullptr;
    wxPanel* find_bar_ = nullptr;
    wxTextCtrl* find_input_ = nullptr;
    wxTextCtrl* replace_input_ = nullptr;
    wxCheckBox* find_case_ = nullptr;
    wxCheckBox* find_regex_ = nullptr;
    wxCheckBox* find_word_ = nullptr;
    wxCheckBox* find_all_ = nullptr;
    wxStaticText* find_count_ = nullptr;
    wxDataViewListCtrl* find_results_ = nullptr;
    std::vector<ProjectFindMatch> project_matches_;
    wxAuiManager manager_;
    app::EditorSettings editor_settings_;
    wxRect normal_geometry_;
    ScriptAnalysis analysis_;
};

}  // namespace say_count::ui
