#pragma once

#include <wx/frame.h>
#include <wx/aui/framemanager.h>
#include <wx/fswatcher.h>
#include <wx/timer.h>
#include <wx/process.h>

#include "app/settings.h"
#include "core/parser.h"
#include "core/find_replace.h"
#include "core/project.h"
#include "core/project_bundle.h"
#include "core/renpy.h"
#include "core/renpy_runtime.h"
#include "core/renpy_lint.h"
#include "core/assets.h"
#include "core/coverage.h"
#include "core/snapshots.h"

#include <optional>
#include <functional>
#include <memory>
#include <unordered_set>
#include <unordered_map>
#include <vector>

class wxPanel;
class wxTextCtrl;
class wxCheckBox;
class wxStaticText;
class wxDataViewListCtrl;
class wxDataViewEvent;
class wxMenu;
class wxControl;
class wxInfoBar;

namespace say_count::ui {

class EditorNotebook;
class SpeakerStatsPanel;
class OutlinePanel;
class DiagnosticsPanel;
class RenpyLintPanel;
class CoveragePanel;
class RoutePanel;
class ProductionPanel;
class StoryLibraryPanel;
class ProjectNavigatorPanel;
struct FindStatus;

class MainFrame final : public wxFrame {
public:
    MainFrame();
    ~MainFrame() override;
    bool OpenInitialFiles(const std::vector<wxString>& paths);

private:
    void BuildMenus();
    void BuildCommandBar();
    void RefreshCommandBarState();
    void RefreshCueSummary();
    void ShowNotice(const wxString& message, int flags);
    void HideNotice();
    void BuildFindBar();
    void BuildFindResults();
    void BuildFocusPill();
    void PositionFocusPill();
    void RebuildRecentProjectsMenu();
    bool ConnectProjectFolder(const wxString& selected_path);
    void StartProjectWatcher();
    void RefreshProjectDiscovery();
    void HandleExternalScriptChange(const wxString& path);
    void ReviewExternalConflict(const std::string& key);
    bool TakeSnapshot(bool automatic, std::string label = {});
    void RestoreWindow();
    void RestoreWorkspace();
    void ShowWelcomeIfNeeded();
    void ShowHome();
    void StartNewStory();
    wxString PreferredStoryParent() const;
    void RefreshStoryLibrary();
    void SaveWorkspace();
    void OnQuickOpen(wxCommandEvent& event);
    void OnCommandPalette(wxCommandEvent& event);
    void DispatchCommand(int id);
    void OnAbout(wxCommandEvent& event);
    void OnClose(wxCloseEvent& event);
    void OnCloseTab(wxCommandEvent& event);
    void OnNewTab(wxCommandEvent& event);
    void OnOpen(wxCommandEvent& event);
    void OnQuit(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnSaveAs(wxCommandEvent& event);
    void OnToggleWrap(wxCommandEvent& event);
    void OnToggleNvimMotions(wxCommandEvent& event);
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
    void OnWriteManuscript(wxCommandEvent& event);
    void OnWriterDraft(wxCommandEvent& event);
    void OnInsertStoryElement(wxCommandEvent& event);
    void OnConfigureOfflineProseAi(wxCommandEvent& event);
    void OnShowShortcuts(wxCommandEvent& event);
    void OnShowManuscriptGuide(wxCommandEvent& event);
    void OnConvertChat(wxCommandEvent& event);
    void OfferChatRuntimeInstall();
    void OnInstallChatRuntime(wxCommandEvent& event);
    void OnToggleFocus(wxCommandEvent& event);
    void OnTogglePane(wxCommandEvent& event);
    void OnPaneClose(wxAuiManagerEvent& event);
    void OnSize(wxSizeEvent& event);
    void OnConnectProject(wxCommandEvent& event);
    void OnRecentProject(wxCommandEvent& event);
    void OnFileSystemEvent(wxFileSystemWatcherEvent& event);
    void OnReviewConflicts(wxCommandEvent& event);
    void OnSnapshotNow(wxCommandEvent& event);
    void OnManageSnapshots(wxCommandEvent& event);
    void OnSnapshotTimer(wxTimerEvent& event);
    void OnImportProject(wxCommandEvent& event);
    void OnExportProject(wxCommandEvent& event);
    void OnGitRepository(wxCommandEvent& event);
    ProjectBundle BuildProjectBundle() const;
    bool ApplyProjectBundle(ProjectBundle bundle, const wxString& success_message);
    void OnRenameSymbol(wxCommandEvent& event);
    void OnConfigureRenpy(wxCommandEvent& event);
    void DetectRenpy();
    bool LaunchRenpy(const std::vector<wxString>& arguments,
                     const wxExecuteEnv* environment = nullptr,
                     std::function<void(int, std::string)> completion = {});
    void AppendRenpyLog(const wxString& text);
    void DrainRenpyOutput(bool flush = false);
    void OnRunRenpy(wxCommandEvent& event);
    void OnStopRenpy(wxCommandEvent& event);
    void OnShowRenpyLog(wxCommandEvent& event);
    void OnRenpyOutputTimer(wxTimerEvent& event);
    void OnRenpyEnded(wxProcessEvent& event);
    void OnWarpRenpy(wxCommandEvent& event);
    void OnDirectorRenpy(wxCommandEvent& event);
    void OnRuntimePresets(wxCommandEvent& event);
    bool LaunchRenpyWithRuntime(const std::vector<wxString>& arguments);
    void OnRunRenpyLint(wxCommandEvent& event);
    void OnGenerateTranslations(wxCommandEvent& event);
    void OnExportDialogue(wxCommandEvent& event);
    void RunLocalizationTool(bool dialogue);
    void OnShowAssets(wxCommandEvent& event);
    void OnShowCoverage(wxCommandEvent& event);
    void OnShowRoutes(wxCommandEvent& event);
    void RefreshRoutes();
    void OnShowProduction(wxCommandEvent& event);
    void RefreshProduction();
    void OnFixBasicErrors();
    void OnFixIndents(wxCommandEvent& event);
    void OnCoverageTimer(wxTimerEvent& event);
    void SetupCoverageProject();
    void RefreshCoveragePanel();
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
    wxPanel* command_bar_ = nullptr;
    wxInfoBar* notification_bar_ = nullptr;
    wxStaticText* workspace_name_ = nullptr;
    wxStaticText* cue_summary_ = nullptr;
    wxControl* open_game_button_ = nullptr;
    wxControl* save_button_ = nullptr;
    wxControl* write_button_ = nullptr;
    wxControl* history_button_ = nullptr;
    wxControl* problems_button_ = nullptr;
    wxControl* run_button_ = nullptr;
    wxTextCtrl* find_input_ = nullptr;
    wxTextCtrl* replace_input_ = nullptr;
    wxCheckBox* find_case_ = nullptr;
    wxCheckBox* find_regex_ = nullptr;
    wxCheckBox* find_word_ = nullptr;
    wxCheckBox* find_all_ = nullptr;
    wxStaticText* find_count_ = nullptr;
    wxDataViewListCtrl* find_results_ = nullptr;
    wxPanel* focus_pill_ = nullptr;
    wxStaticText* focus_count_ = nullptr;
    wxMenu* recent_projects_menu_ = nullptr;
    std::vector<ProjectFindMatch> project_matches_;
    wxAuiManager manager_;
    wxTimer snapshot_timer_;
    wxTimer renpy_output_timer_;
    wxTimer coverage_timer_;
    app::EditorSettings editor_settings_;
    wxRect normal_geometry_;
    ScriptAnalysis analysis_;
    bool current_document_dirty_ = false;
    wxString current_document_path_;
    wxString last_saved_time_;
    wxString last_history_time_;
    std::size_t problem_count_ = 0;
    std::size_t fixable_problem_count_ = 0;
    bool focus_mode_ = false;
    bool nvim_command_active_ = false;
    wxString focus_perspective_;
    std::optional<ProjectFolder> project_;
    std::vector<wxString> recent_projects_;
    std::unique_ptr<wxFileSystemWatcher> project_watcher_;
    std::unordered_map<std::string, ExternalConflict> external_conflicts_;
    bool conflict_review_open_ = false;
    bool close_confirm_open_ = false;
    std::unique_ptr<SnapshotStore> snapshot_store_;
    std::optional<ProjectBundle> imported_bundle_;
    bool count_menu_choices_ = false;
    std::optional<RenpySdk> renpy_sdk_;
    wxMenu* renpy_menu_ = nullptr;
    wxTextCtrl* renpy_log_ = nullptr;
    wxTextCtrl* renpy_tool_output_ = nullptr;
    RenpyLintPanel* renpy_lint_ = nullptr;
    StoryLibraryPanel* story_library_ = nullptr;
    ProjectNavigatorPanel* project_navigator_ = nullptr;
    std::vector<ProjectAsset> story_assets_;
    CoveragePanel* coverage_panel_ = nullptr;
    RoutePanel* route_panel_ = nullptr;
    ProductionPanel* production_panel_ = nullptr;
    std::unique_ptr<wxProcess> renpy_process_;
    long renpy_pid_ = 0;
    wxString renpy_log_path_;
    std::unique_ptr<RuntimePresetStore> runtime_presets_;
    std::string runtime_preset_name_;
    std::string runtime_state_json_ = "{}";
    std::string renpy_operation_output_;
    std::string renpy_display_pending_;
    std::function<void(int, std::string)> renpy_completion_;
    std::unique_ptr<ManualCoverageStore> manual_coverage_store_;
    std::map<std::string, std::set<std::string>> manual_coverage_projects_;
    std::vector<std::string> coverage_labels_;
    std::set<std::string> playthrough_coverage_;
    CoverageTail coverage_tail_;
    wxString coverage_path_;
};

}  // namespace say_count::ui
