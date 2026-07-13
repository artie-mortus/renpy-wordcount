#include "ui/main_frame.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <wx/aboutdlg.h>
#include <wx/display.h>
#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/datetime.h>
#include <wx/dirdlg.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/tokenzr.h>
#include <wx/choicdlg.h>
#include <wx/dataview.h>
#include <wx/stream.h>

#include "ui/editor_notebook.h"
#include "ui/speaker_stats_panel.h"
#include "ui/outline_panel.h"
#include "ui/diagnostics_panel.h"
#include "ui/conflict_dialog.h"
#include "ui/snapshot_dialog.h"
#include "ui/rename_dialog.h"
#include "ui/runtime_preset_dialog.h"
#include "ui/renpy_lint_panel.h"
#include "ui/asset_panel.h"
#include "ui/coverage_panel.h"
#include "core/version.h"

namespace say_count::ui {
namespace {

constexpr int kDefaultWidth = 1100;
constexpr int kDefaultHeight = 750;
enum MenuId {
    kToggleWrap = wxID_HIGHEST + 1,
    kFontIncrease,
    kFontDecrease,
    kFontReset,
    kThemeSystem,
    kThemeLight,
    kThemeDark,
    kExportCsv,
    kExportJson,
    kExportHtml,
    kFindNext,
    kFindPrevious,
    kFindReplace,
    kFindReplaceAll,
    kFindClose,
    kGoToLine,
    kToggleComment,
    kShortcutSheet,
    kFocusMode,
    kConnectProject,
    kRecentProjectFirst,
    kRecentProjectLast = kRecentProjectFirst + 7,
    kReviewConflicts = kRecentProjectLast + 1,
    kSnapshotNow,
    kManageSnapshots,
    kImportProject,
    kExportProject,
    kRenameSymbol,
    kConfigureRenpy,
    kRenpyStatus,
    kRunRenpy,
    kStopRenpy,
    kShowRenpyLog,
    kWarpRenpy,
    kDirectorRenpy,
    kRuntimePresets,
    kRunRenpyLint,
    kGenerateTranslations,
    kExportDialogue,
    kShowAssets,
    kShowCoverage,
};

class ScriptDropTarget final : public wxFileDropTarget {
public:
    explicit ScriptDropTarget(EditorNotebook* notebook) : notebook_(notebook) {}
    bool OnDropFiles(wxCoord, wxCoord, const wxArrayString& filenames) override {
        std::vector<wxString> scripts;
        for (const auto& filename : filenames) {
            if (wxFileName(filename).GetExt().CmpNoCase("rpy") == 0) scripts.push_back(filename);
        }
        return !scripts.empty() && notebook_->OpenFiles(scripts);
    }
private:
    EditorNotebook* notebook_;
};

bool IsGeometryVisible(const wxRect& rectangle) {
    for (unsigned int index = 0; index < wxDisplay::GetCount(); ++index) {
        if (wxDisplay(index).GetClientArea().Intersects(rectangle)) {
            return true;
        }
    }
    return false;
}

}  // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Say Count", wxDefaultPosition, wxSize(kDefaultWidth, kDefaultHeight)),
      manager_(this), snapshot_timer_(this), renpy_output_timer_(this), coverage_timer_(this) {
    SetMinSize(wxSize(400, 300));
    editor_settings_ = settings_.LoadEditor();
    DetectRenpy();
    recent_projects_ = settings_.LoadRecentProjects();
    snapshot_store_ = std::make_unique<SnapshotStore>(
        (settings_.data_directory() + wxFILE_SEP_PATH + "snapshots").ToStdString(), 50);
    renpy_log_path_ = settings_.data_directory() + wxFILE_SEP_PATH + "renpy-launch.log";
    runtime_presets_ = std::make_unique<RuntimePresetStore>(
        (settings_.data_directory() + wxFILE_SEP_PATH + "runtime-presets.dat").ToStdString());
    manual_coverage_store_ = std::make_unique<ManualCoverageStore>(
        (settings_.data_directory() + wxFILE_SEP_PATH + "manual-coverage.dat").ToStdString());
    manual_coverage_projects_ = manual_coverage_store_->Load();
    BuildMenus();
    CreateStatusBar();
    speaker_stats_ = new SpeakerStatsPanel(
        this, wxFileName(settings_.path()).GetPath() + wxFILE_SEP_PATH + "targets.ini");
    outline_ = new OutlinePanel(this);
    diagnostics_ = new DiagnosticsPanel(this);
    notebook_ = new EditorNotebook(this, editor_settings_, [this](const wxString& source, const ScriptAnalysis& analysis) {
        analysis_ = analysis;
        // wxString::Format aborts on %zu; compose the text without varargs.
        const std::string text = std::to_string(analysis.total_words) + " dialogue words \xc2\xb7 " +
                                 std::to_string(analysis.dialogue_lines) + " dialogue lines \xc2\xb7 " +
                                 std::to_string(analysis.reading_minutes) + " min reading time";
        SetStatusText(wxString::FromUTF8(text));
        if (focus_count_) {
            focus_count_->SetLabel(wxString::FromUTF8(
                std::to_string(analysis.total_words) + " dialogue words"));
            if (focus_mode_) PositionFocusPill();
        }
        speaker_stats_->SetAnalysis(analysis);
        outline_->SetDocument(source, analysis);
    });
    BuildFindBar();
    BuildFindResults();
    BuildFocusPill();
    renpy_log_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                                wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    if (wxFileName::FileExists(renpy_log_path_)) {
        wxFile log(renpy_log_path_);
        wxString text;
        if (log.IsOpened() && log.ReadAll(&text, wxConvUTF8))
            renpy_log_->SetValue(text.length() > 30000 ? text.Right(30000) : text);
    }
    renpy_lint_ = new RenpyLintPanel(this);
    renpy_tool_output_ = new wxTextCtrl(this, wxID_ANY, "No Ren'Py tool has run yet.",
                                        wxDefaultPosition, wxDefaultSize,
                                        wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    asset_panel_ = new AssetPanel(this);
    asset_panel_->SetInsertHandler([this](const std::string& statement) {
        notebook_->InsertAtCaret(statement);
        SetStatusText("Inserted Ren'Py asset statement");
    });
    coverage_panel_ = new CoveragePanel(this);
    coverage_panel_->SetManualHandler([this](const std::string& label, bool covered) {
        if (!project_) return;
        auto& labels = manual_coverage_projects_[project_->root];
        if (covered) labels.insert(label); else labels.erase(label);
        std::string error;
        if (!manual_coverage_store_->Save(manual_coverage_projects_, &error))
            wxMessageBox(wxString::FromUTF8(error), "Coverage save failed", wxOK | wxICON_ERROR, this);
        RefreshCoveragePanel();
    });
    coverage_panel_->SetClearHandler([this] {
        if (coverage_path_.empty()) return;
        wxFile file; file.Create(coverage_path_, true); file.Close();
        coverage_tail_.Reset(); playthrough_coverage_.clear(); RefreshCoveragePanel();
    });
    renpy_lint_->SetJumpHandler([this](const RenpyLintIssue& issue) {
        if (!project_) return;
        std::filesystem::path path(issue.file);
        if (!path.is_absolute()) {
            auto first = path.begin();
            path = first != path.end() && first->string() == "game"
                ? std::filesystem::path(project_->root) / path
                : std::filesystem::path(project_->scripts_root) / path;
        }
        std::error_code ec;
        const auto canonical = std::filesystem::weakly_canonical(path, ec);
        const auto relative = std::filesystem::relative(canonical, project_->scripts_root, ec);
        if (!ec && !relative.empty() && *relative.begin() != "..")
            notebook_->OpenAndJump(wxString::FromUTF8(canonical.string()), issue.line);
    });
    notebook_->SetDiagnosticsHandler([this](const std::vector<Diagnostic>& diagnostics) {
        diagnostics_->SetDiagnostics(diagnostics);
    });
    diagnostics_->SetJumpHandler([this](const Diagnostic& diagnostic) {
        notebook_->SelectDiagnostic(diagnostic);
    });
    notebook_->SetFindStatusHandler([this](const FindStatus& status) { UpdateFindStatus(status); });
    speaker_stats_->SetLineJumpHandler([this](std::size_t line) { notebook_->JumpToLine(line); });
    outline_->SetJumpHandler([this](std::size_t line) { notebook_->JumpToLine(line); });
    manager_.AddPane(notebook_, wxAuiPaneInfo().CenterPane().Name("editor"));
    manager_.AddPane(find_bar_, wxAuiPaneInfo().Top().Name("find-replace").CaptionVisible(false)
                         .PaneBorder(false).DockFixed(true).Resizable(false).BestSize(-1, 44).Hide());
    manager_.AddPane(find_results_, wxAuiPaneInfo().Bottom().Name("find-results").Caption("Find Results")
                         .BestSize(-1, 190).MinSize(300, 110).CloseButton(false).Hide());
    manager_.AddPane(diagnostics_, wxAuiPaneInfo().Bottom().Name("diagnostics").Caption("Diagnostics")
                         .BestSize(-1, 190).MinSize(320, 110).CloseButton(true).MaximizeButton(true));
    manager_.AddPane(speaker_stats_, wxAuiPaneInfo().Right().Name("speaker-statistics")
                         .Caption("Speaker Statistics").BestSize(320, 500).MinSize(240, 180)
                         .CloseButton(true).MaximizeButton(true));
    manager_.AddPane(outline_, wxAuiPaneInfo().Left().Name("outline").Caption("Outline")
                         .BestSize(280, 500).MinSize(200, 160).CloseButton(true).MaximizeButton(true));
    manager_.AddPane(renpy_log_, wxAuiPaneInfo().Bottom().Name("renpy-log").Caption("Ren'Py Log")
                         .BestSize(-1, 190).MinSize(320, 100).CloseButton(true).Hide());
    manager_.AddPane(renpy_lint_, wxAuiPaneInfo().Bottom().Name("renpy-lint").Caption("Official Ren'Py Lint")
                         .BestSize(-1, 210).MinSize(360, 120).CloseButton(true).Hide());
    manager_.AddPane(renpy_tool_output_, wxAuiPaneInfo().Bottom().Name("renpy-tool-output")
                         .Caption("Ren'Py Tool Output").BestSize(-1, 210).MinSize(360, 120)
                         .CloseButton(true).Hide());
    manager_.AddPane(asset_panel_, wxAuiPaneInfo().Left().Name("asset-browser").Caption("Project Assets")
                         .BestSize(330, 520).MinSize(260, 220).CloseButton(true).Hide());
    manager_.AddPane(coverage_panel_, wxAuiPaneInfo().Left().Name("coverage").Caption("Label Coverage")
                         .BestSize(360, 520).MinSize(280, 220).CloseButton(true).Hide());
    manager_.Update();
    SetDropTarget(new ScriptDropTarget(notebook_));
    RestoreWindow();

    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    Bind(wxEVT_MENU, &MainFrame::OnQuit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnNewTab, this, wxID_NEW);
    Bind(wxEVT_MENU, &MainFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &MainFrame::OnSaveAs, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &MainFrame::OnCloseTab, this, wxID_CLOSE);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &MainFrame::OnToggleWrap, this, kToggleWrap);
    Bind(wxEVT_MENU, &MainFrame::OnFontSize, this, kFontIncrease, kFontReset);
    Bind(wxEVT_MENU, &MainFrame::OnTheme, this, kThemeSystem, kThemeDark);
    Bind(wxEVT_MENU, &MainFrame::OnExport, this, kExportCsv, kExportHtml);
    Bind(wxEVT_MENU, &MainFrame::OnOpenFind, this, wxID_FIND);
    Bind(wxEVT_MENU, &MainFrame::OnFindNext, this, kFindNext, kFindPrevious);
    Bind(wxEVT_CHAR_HOOK, &MainFrame::OnCharHook, this);
    Bind(wxEVT_MENU, &MainFrame::OnGoToLine, this, kGoToLine);
    Bind(wxEVT_MENU, &MainFrame::OnToggleComment, this, kToggleComment);
    Bind(wxEVT_MENU, &MainFrame::OnShowShortcuts, this, kShortcutSheet);
    Bind(wxEVT_MENU, &MainFrame::OnToggleFocus, this, kFocusMode);
    Bind(wxEVT_SIZE, &MainFrame::OnSize, this);
    Bind(wxEVT_MENU, &MainFrame::OnConnectProject, this, kConnectProject);
    Bind(wxEVT_MENU, &MainFrame::OnRecentProject, this, kRecentProjectFirst, kRecentProjectLast);
    Bind(wxEVT_FSWATCHER, &MainFrame::OnFileSystemEvent, this);
    Bind(wxEVT_MENU, &MainFrame::OnReviewConflicts, this, kReviewConflicts);
    Bind(wxEVT_MENU, &MainFrame::OnSnapshotNow, this, kSnapshotNow);
    Bind(wxEVT_MENU, &MainFrame::OnManageSnapshots, this, kManageSnapshots);
    Bind(wxEVT_MENU, &MainFrame::OnImportProject, this, kImportProject);
    Bind(wxEVT_MENU, &MainFrame::OnExportProject, this, kExportProject);
    Bind(wxEVT_MENU, &MainFrame::OnRenameSymbol, this, kRenameSymbol);
    Bind(wxEVT_MENU, &MainFrame::OnConfigureRenpy, this, kConfigureRenpy);
    Bind(wxEVT_MENU, &MainFrame::OnRunRenpy, this, kRunRenpy);
    Bind(wxEVT_MENU, &MainFrame::OnStopRenpy, this, kStopRenpy);
    Bind(wxEVT_MENU, &MainFrame::OnShowRenpyLog, this, kShowRenpyLog);
    Bind(wxEVT_MENU, &MainFrame::OnWarpRenpy, this, kWarpRenpy);
    Bind(wxEVT_MENU, &MainFrame::OnDirectorRenpy, this, kDirectorRenpy);
    Bind(wxEVT_MENU, &MainFrame::OnRuntimePresets, this, kRuntimePresets);
    Bind(wxEVT_MENU, &MainFrame::OnRunRenpyLint, this, kRunRenpyLint);
    Bind(wxEVT_MENU, &MainFrame::OnGenerateTranslations, this, kGenerateTranslations);
    Bind(wxEVT_MENU, &MainFrame::OnExportDialogue, this, kExportDialogue);
    Bind(wxEVT_MENU, &MainFrame::OnShowAssets, this, kShowAssets);
    Bind(wxEVT_MENU, &MainFrame::OnShowCoverage, this, kShowCoverage);
    Bind(wxEVT_TIMER, &MainFrame::OnCoverageTimer, this, coverage_timer_.GetId());
    Bind(wxEVT_TIMER, &MainFrame::OnRenpyOutputTimer, this, renpy_output_timer_.GetId());
    Bind(wxEVT_END_PROCESS, &MainFrame::OnRenpyEnded, this);
    Bind(wxEVT_TIMER, &MainFrame::OnSnapshotTimer, this, snapshot_timer_.GetId());
    snapshot_timer_.Start(10 * 60 * 1000);
    coverage_timer_.Start(750);
}

MainFrame::~MainFrame() {
    coverage_timer_.Stop();
    renpy_output_timer_.Stop();
    if (renpy_process_) {
        renpy_process_->Detach();
        if (renpy_pid_) wxProcess::Kill(renpy_pid_, wxSIGTERM, wxKILL_CHILDREN);
        renpy_process_.reset();
    }
    manager_.UnInit();
}

void MainFrame::BuildMenus() {
    auto* file = new wxMenu();
    file->Append(wxID_NEW, "&New\tCtrl+N", "Create a new tab");
    file->Append(wxID_OPEN, "&Open...\tCtrl+O", "Open one or more Ren'Py scripts");
    file->Append(wxID_SAVE, "&Save\tCtrl+S", "Save the current script");
    file->Append(wxID_SAVEAS, "Save &As...\tCtrl+Shift+S", "Save the current script under a new name");
    file->Append(wxID_CLOSE, "&Close Tab\tCtrl+W", "Close the current tab");
    file->AppendSeparator();
    file->Append(kConnectProject, "Connect Project &Folder...", "Open every Ren'Py script in a project");
    recent_projects_menu_ = new wxMenu();
    file->AppendSubMenu(recent_projects_menu_, "Recent Projects");
    file->Append(kReviewConflicts, "Review External &Conflicts...");
    file->Append(kSnapshotNow, "&Snapshot Now", "Store a backup of every open script");
    file->Append(kManageSnapshots, "Manage Snapshots...", "Preview, compare, and restore snapshots");
    file->Append(kImportProject, "Import Say Count Project...", "Import a browser-app project bundle");
    RebuildRecentProjectsMenu();
    file->AppendSeparator();
    auto* export_menu = new wxMenu();
    export_menu->Append(kExportCsv, "Speaker statistics (CSV)...");
    export_menu->Append(kExportJson, "Full statistics (JSON)...");
    export_menu->Append(kExportHtml, "Standalone report (HTML)...");
    export_menu->AppendSeparator();
    export_menu->Append(kExportProject, "Complete Say Count project...");
    file->AppendSubMenu(export_menu, "Export &Statistics");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "&Quit\tCtrl+Q", "Quit Say Count");

    auto* edit = new wxMenu();
    edit->Append(wxID_FIND, "&Find and Replace...\tCtrl+F");
    edit->Append(kFindNext, "Find &Next\tF3");
    edit->Append(kFindPrevious, "Find &Previous\tShift+F3");
    edit->AppendSeparator();
    edit->Append(kGoToLine, "&Go to Line...\tCtrl+G");
    edit->Append(kToggleComment, "Toggle &Comment\tCtrl+/");
    edit->Append(kRenameSymbol, "Rename Ren'Py Symbol...", "Preview and safely rename an alias or label project-wide");
    auto* view = new wxMenu();
    view->AppendCheckItem(kToggleWrap, "Word &Wrap", "Soft-wrap long lines");
    view->Check(kToggleWrap, editor_settings_.word_wrap);
    view->AppendCheckItem(kFocusMode, "&Focus Mode\tCtrl+Shift+F", "Hide nonessential panels");
    view->AppendSeparator();
    view->Append(kFontIncrease, "Increase Font Size\tCtrl+=");
    view->Append(kFontDecrease, "Decrease Font Size\tCtrl+-");
    view->Append(kFontReset, "Reset Font Size\tCtrl+0");
    auto* theme = new wxMenu();
    theme->AppendRadioItem(kThemeSystem, "System");
    theme->AppendRadioItem(kThemeLight, "Light");
    theme->AppendRadioItem(kThemeDark, "Dark");
    theme->Check(kThemeSystem + static_cast<int>(editor_settings_.theme), true);
    view->AppendSubMenu(theme, "Theme");

    auto* help = new wxMenu();
    help->Append(wxID_ABOUT, "&About", "About Say Count");
    help->Append(kShortcutSheet, "Keyboard &Shortcuts\tCtrl+K");
    renpy_menu_ = new wxMenu();
    renpy_menu_->Append(kRunRenpy, "Run Project\tF6");
    renpy_menu_->Append(kWarpRenpy, "Run from Caret\tF7");
    renpy_menu_->Append(kDirectorRenpy, "Interactive Director");
    renpy_menu_->Append(kRuntimePresets, "Runtime State Presets...");
    renpy_menu_->Append(kRunRenpyLint, "Run Official Lint");
    renpy_menu_->Append(kGenerateTranslations, "Generate Translations...");
    renpy_menu_->Append(kExportDialogue, "Export Dialogue...");
    renpy_menu_->Append(kShowAssets, "Browse Project Assets...");
    renpy_menu_->Append(kShowCoverage, "Label Coverage...");
    renpy_menu_->Append(kStopRenpy, "Stop Running Project\tShift+F6");
    renpy_menu_->Append(kShowRenpyLog, "Show Launch Log");
    renpy_menu_->AppendSeparator();
    renpy_menu_->Append(kConfigureRenpy, "Configure SDK Executable...");
    auto* sdk_status = renpy_menu_->Append(kRenpyStatus, renpy_sdk_
        ? "SDK: " + wxString::FromUTF8(renpy_sdk_->version.empty() ? "detected" : renpy_sdk_->version)
        : "SDK: not found");
    sdk_status->Enable(false);

    auto* menu_bar = new wxMenuBar();
    menu_bar->Append(file, "&File");
    menu_bar->Append(edit, "&Edit");
    menu_bar->Append(view, "&View");
    menu_bar->Append(renpy_menu_, "&Ren'Py");
    menu_bar->Append(help, "&Help");
    SetMenuBar(menu_bar);
}

void MainFrame::RebuildRecentProjectsMenu() {
    if (!recent_projects_menu_) return;
    while (recent_projects_menu_->GetMenuItemCount() != 0)
        recent_projects_menu_->Destroy(recent_projects_menu_->FindItemByPosition(0));
    if (recent_projects_.empty()) {
        auto* item = recent_projects_menu_->Append(wxID_ANY, "(None)");
        item->Enable(false);
        return;
    }
    const std::size_t count = std::min<std::size_t>(recent_projects_.size(), 8);
    for (std::size_t index = 0; index < count; ++index)
        recent_projects_menu_->Append(kRecentProjectFirst + static_cast<int>(index), recent_projects_[index]);
}

bool MainFrame::ConnectProjectFolder(const wxString& selected_path) {
    std::string error;
    auto discovered = discover_project_folder(selected_path.ToStdString(), &error);
    if (!discovered) {
        wxMessageBox(wxString::FromUTF8(error), "Project connection failed",
                     wxOK | wxICON_ERROR, this);
        return false;
    }
    std::vector<wxString> paths;
    paths.reserve(discovered->scripts.size());
    for (const auto& script : discovered->scripts) paths.push_back(wxString::FromUTF8(script.absolute_path));
    if (!notebook_->OpenProjectFiles(paths)) return false;
    project_ = std::move(discovered);
    asset_panel_->SetAssets(discover_project_assets(project_->scripts_root));
    SetupCoverageProject();
    external_conflicts_.clear();
    std::vector<std::string> recent;
    recent.reserve(recent_projects_.size());
    for (const auto& path : recent_projects_) recent.push_back(path.ToStdString());
    recent = update_recent_projects(recent, project_->root, 8);
    recent_projects_.clear();
    for (const auto& path : recent) recent_projects_.push_back(wxString::FromUTF8(path));
    settings_.SaveRecentProjects(recent_projects_);
    RebuildRecentProjectsMenu();
    const wxFileName root(wxString::FromUTF8(project_->root));
    SetTitle("Say Count — " + root.GetFullName());
    SetStatusText(wxString::FromUTF8("Connected " + project_->root + " — " +
        std::to_string(project_->scripts.size()) + " script" +
        (project_->scripts.size() == 1 ? "" : "s")));
    StartProjectWatcher();
    return true;
}

void MainFrame::StartProjectWatcher() {
    if (!project_) return;
    if (!project_watcher_) {
        project_watcher_ = std::make_unique<wxFileSystemWatcher>();
        project_watcher_->SetOwner(this);
    } else {
        project_watcher_->RemoveAll();
    }
    const wxFileName scripts_root(wxString::FromUTF8(project_->scripts_root));
    if (!project_watcher_->AddTree(scripts_root, wxFSW_EVENT_CREATE | wxFSW_EVENT_DELETE |
        wxFSW_EVENT_RENAME | wxFSW_EVENT_MODIFY | wxFSW_EVENT_WARNING | wxFSW_EVENT_ERROR)) {
        SetStatusText("Project connected, but external file watching could not start");
    }
}

void MainFrame::RefreshProjectDiscovery() {
    if (!project_) return;
    auto refreshed = discover_project_folder(project_->root);
    if (!refreshed) return;
    std::vector<wxString> paths;
    paths.reserve(refreshed->scripts.size());
    for (const auto& script : refreshed->scripts) paths.push_back(wxString::FromUTF8(script.absolute_path));
    notebook_->OpenProjectFiles(paths);
    project_ = std::move(refreshed);
    asset_panel_->SetAssets(discover_project_assets(project_->scripts_root));
    coverage_labels_ = collect_project_labels(notebook_->ProjectScripts());
    RefreshCoveragePanel();
}

void MainFrame::HandleExternalScriptChange(const wxString& path) {
    const auto update = notebook_->ReloadExternalFile(path);
    const auto result = update.result;
    const wxFileName file(path);
    const std::string key = file.GetFullPath().ToStdString();
    if (result == ExternalFileResult::Reloaded) {
        external_conflicts_.erase(key);
        SetStatusText("Reloaded external changes to " + file.GetFullName());
    } else if (result == ExternalFileResult::Dirty) {
        const bool first = external_conflicts_.count(key) == 0;
        external_conflicts_.insert_or_assign(key, ExternalConflict{key, update.local_content,
                                                                   update.disk_content});
        if (first) wxBell();
        SetStatusText(file.GetFullName() + " changed externally; local unsaved edits were kept");
        if (first) CallAfter([this, key] { ReviewExternalConflict(key); });
    } else if (result == ExternalFileResult::NotOpen && wxFileName::FileExists(path)) {
        notebook_->OpenFiles({path});
    } else if (result == ExternalFileResult::Unchanged) {
        external_conflicts_.erase(key);
    } else if (result == ExternalFileResult::Failed) {
        SetStatusText("Could not read external changes to " + file.GetFullName());
    }
}

void MainFrame::ReviewExternalConflict(const std::string& key) {
    if (conflict_review_open_) return;
    const auto found = external_conflicts_.find(key);
    if (found == external_conflicts_.end()) return;
    const ExternalConflict conflict = found->second;
    conflict_review_open_ = true;
    ConflictDialog dialog(this, conflict);
    const int modal = dialog.ShowModal();
    conflict_review_open_ = false;
    if (modal != wxID_OK || dialog.choice() == ConflictDialogChoice::Cancel) return;

    const auto current = external_conflicts_.find(key);
    if (current == external_conflicts_.end()) return;
    if (current->second.disk_content != conflict.disk_content) {
        wxMessageBox("The disk file changed again during review. The comparison will be refreshed.",
                     "Conflict changed", wxOK | wxICON_WARNING, this);
        CallAfter([this, key] { ReviewExternalConflict(key); });
        return;
    }

    if (dialog.choice() == ConflictDialogChoice::KeepLocal) {
        external_conflicts_.erase(key);
        SetStatusText("Keeping local unsaved version of " + wxFileName(wxString::FromUTF8(key)).GetFullName());
        return;
    }
    if (dialog.choice() == ConflictDialogChoice::SaveElsewhere) {
        const wxFileName original(wxString::FromUTF8(conflict.path));
        const wxString suggested = original.GetName() + "-local." + original.GetExt();
        wxFileDialog save(this, "Save local version before using disk", original.GetPath(), suggested,
                          "Ren'Py scripts (*.rpy)|*.rpy|All files (*.*)|*.*",
                          wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (save.ShowModal() != wxID_OK) return;
        if (wxFileName(save.GetPath()).GetAbsolutePath() == original.GetAbsolutePath()) {
            wxMessageBox("Choose a different path so the external disk revision is not overwritten.",
                         "Different path required", wxOK | wxICON_WARNING, this);
            return;
        }
        wxFile output;
        if (!output.Create(save.GetPath(), true) ||
            !output.Write(wxString::FromUTF8(conflict.local_content), wxConvUTF8) || !output.Close()) {
            wxMessageBox("The local copy could not be saved. The conflict remains unresolved.",
                         "Save failed", wxOK | wxICON_ERROR, this);
            return;
        }
    } else if (!TakeSnapshot(false, "Before using disk version of " +
                             wxFileName(wxString::FromUTF8(conflict.path)).GetFullName().ToStdString())) {
        return;
    }
    if (notebook_->ApplyExternalVersion(wxString::FromUTF8(conflict.path), conflict.disk_content, true)) {
        external_conflicts_.erase(key);
        SetStatusText("Using external disk version of " +
                      wxFileName(wxString::FromUTF8(conflict.path)).GetFullName());
    }
}

void MainFrame::OnReviewConflicts(wxCommandEvent&) {
    if (external_conflicts_.empty()) {
        SetStatusText("No external conflicts are pending");
        return;
    }
    ReviewExternalConflict(external_conflicts_.begin()->first);
}

bool MainFrame::TakeSnapshot(bool automatic, std::string label) {
    if (!snapshot_store_) return false;
    const auto files = notebook_->ProjectScripts();
    if (automatic && std::none_of(files.begin(), files.end(), [](const auto& file) {
        return file.content.find_first_not_of(" \t\r\n") != std::string::npos;
    })) return true;
    const auto words = analyze_project(files, {count_menu_choices_}).total_words;
    if (label.empty()) label = automatic ? "Automatic snapshot" : "Manual snapshot";
    const auto result = snapshot_store_->Create(files, std::move(label), automatic,
                                                project_ ? project_->root : std::string{}, words);
    if (!result.success) {
        if (!automatic) wxMessageBox(wxString::FromUTF8(result.error), "Snapshot failed",
                                     wxOK | wxICON_ERROR, this);
        return false;
    }
    if (!automatic) SetStatusText(result.created ? "Snapshot stored" : "No changes since the last snapshot");
    return true;
}

void MainFrame::OnSnapshotNow(wxCommandEvent&) {
    TakeSnapshot(false);
}

void MainFrame::OnManageSnapshots(wxCommandEvent&) {
    if (!snapshot_store_) return;
    SnapshotDialog dialog(this, *snapshot_store_, notebook_->ProjectScripts());
    if (dialog.ShowModal() != wxID_OK || !dialog.selected_id()) return;
    std::string error;
    const auto snapshot = snapshot_store_->Load(*dialog.selected_id(), &error);
    if (!snapshot) {
        wxMessageBox("Could not load the selected snapshot: " + wxString::FromUTF8(error),
                     "Restore failed", wxOK | wxICON_ERROR, this);
        return;
    }
    if (wxMessageBox(
            "Replace the complete editor tab set with this snapshot?\n\n"
            "The current editor state will be snapshotted first. Restored files remain unsaved.",
            "Confirm snapshot restore", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES)
        return;
    if (!TakeSnapshot(false, "Before snapshot restore")) return;
    if (!notebook_->RestoreProjectScripts(snapshot->files)) {
        wxMessageBox("The snapshot did not contain any restorable scripts.", "Restore failed",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    SetStatusText("Snapshot restored — changes are unsaved");
}

void MainFrame::OnSnapshotTimer(wxTimerEvent&) {
    TakeSnapshot(true);
}

void MainFrame::OnImportProject(wxCommandEvent&) {
    wxFileDialog dialog(this, "Import Say Count project", wxEmptyString, wxEmptyString,
                        "Say Count projects (*.saycount.json;*.json)|*.saycount.json;*.json|JSON files (*.json)|*.json",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    wxFile file(dialog.GetPath());
    wxString text;
    if (!file.IsOpened() || !file.ReadAll(&text, wxConvUTF8)) {
        wxMessageBox("Could not read the selected project.", "Import failed", wxOK | wxICON_ERROR, this);
        return;
    }
    auto parsed = parse_project_bundle(text.ToStdString());
    if (!parsed.bundle) {
        wxMessageBox("Project import failed: " + wxString::FromUTF8(parsed.error), "Import failed",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    if (!TakeSnapshot(false, "Before project import")) return;
    ProjectBundle bundle = std::move(*parsed.bundle);
    if (!notebook_->RestoreProjectScripts(bundle.files)) return;
    notebook_->SelectFileIndex(bundle.active_file);
    count_menu_choices_ = bundle.settings.count_menu_choices;
    notebook_->SetCountMenuChoices(count_menu_choices_);
    editor_settings_.theme = bundle.settings.theme == "dark" ? app::EditorTheme::Dark
                                                               : app::EditorTheme::Light;
    notebook_->SetTheme(editor_settings_.theme);
    settings_.SaveEditor(editor_settings_);
    auto positive = [](const std::string& value) {
        try { return std::max(0L, std::stol(value)); } catch (...) { return 0L; }
    };
    speaker_stats_->ImportTargets(positive(bundle.settings.target),
                                  positive(bundle.settings.line_target),
                                  bundle.speaker_targets, bundle.scene_targets);
    imported_bundle_ = std::move(bundle);
    SetStatusText(wxString::Format("Imported project with %zu files", notebook_->ProjectScripts().size()));
}

void MainFrame::OnExportProject(wxCommandEvent&) {
    wxFileDialog dialog(this, "Export Say Count project", wxEmptyString,
                        "say-count-project.saycount.json",
                        "Say Count projects (*.saycount.json)|*.saycount.json|JSON files (*.json)|*.json",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    ProjectBundle bundle = imported_bundle_.value_or(ProjectBundle{});
    bundle.files = notebook_->ProjectScripts();
    bundle.active_file = notebook_->CurrentFileIndex();
    bundle.exported_at = wxDateTime::UNow().FormatISOCombined('T').ToStdString() + "Z";
    const auto [speakers, scenes] = speaker_stats_->ExportTargets();
    bundle.speaker_targets = speakers;
    bundle.scene_targets = scenes;
    const auto project_target = speaker_stats_->ExportProjectTarget();
    bundle.settings.target = project_target.words > 0 ? std::to_string(project_target.words) : "";
    bundle.settings.line_target = project_target.lines > 0 ? std::to_string(project_target.lines) : "";
    bundle.settings.count_menu_choices = count_menu_choices_;
    bundle.settings.theme = editor_settings_.theme == app::EditorTheme::Dark ? "dark" : "light";
    wxFile file;
    const std::string json = project_bundle_json(bundle);
    if (!file.Create(dialog.GetPath(), true) || !file.Write(wxString::FromUTF8(json), wxConvUTF8) ||
        !file.Close()) {
        wxMessageBox("Could not write the project bundle.", "Export failed", wxOK | wxICON_ERROR, this);
        return;
    }
    imported_bundle_ = bundle;
    SetStatusText("Project exported");
}

void MainFrame::OnRenameSymbol(wxCommandEvent&) {
    RenameDialog dialog(this, notebook_->ProjectScripts());
    if (dialog.ShowModal() != wxID_OK || !dialog.plan()) return;
    const std::size_t active = notebook_->CurrentFileIndex();
    if (!TakeSnapshot(false, "Before renaming " + dialog.original_name())) return;
    const std::size_t changes = dialog.plan()->changes.size();
    if (!notebook_->RestoreProjectScripts(dialog.plan()->files)) return;
    notebook_->SelectFileIndex(active);
    SetStatusText(wxString::Format("Renamed symbol in %zu line%s — changes are unsaved", changes,
                                   changes == 1 ? "" : "s"));
}

void MainFrame::DetectRenpy() {
    wxString environment, path_value;
    wxGetEnv("RENPY_EXECUTABLE", &environment);
    wxGetEnv("PATH", &path_value);
    std::vector<std::string> paths;
#ifdef _WIN32
    wxStringTokenizer tokenizer(path_value, ";");
#else
    wxStringTokenizer tokenizer(path_value, ":");
#endif
    while (tokenizer.HasMoreTokens()) paths.push_back(tokenizer.GetNextToken().ToStdString());
    renpy_sdk_ = detect_renpy_sdk({editor_settings_.renpy_sdk_path.ToStdString(),
                                   environment.ToStdString(), std::move(paths),
                                   wxGetHomeDir().ToStdString()});
}

void MainFrame::OnConfigureRenpy(wxCommandEvent&) {
    wxFileDialog dialog(this, "Choose the Ren'Py SDK executable", wxEmptyString, wxEmptyString,
                        "Ren'Py launcher (renpy;renpy.sh;renpy.exe)|renpy;renpy.sh;renpy.exe|All files|*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    if (!is_renpy_executable(dialog.GetPath().ToStdString())) {
        wxMessageBox("The selected file is not executable.", "Invalid Ren'Py SDK",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    editor_settings_.renpy_sdk_path = wxFileName(dialog.GetPath()).GetAbsolutePath();
    if (!settings_.SaveEditor(editor_settings_)) {
        wxMessageBox("Could not save the SDK setting.", "Settings failed", wxOK | wxICON_ERROR, this);
        return;
    }
    DetectRenpy();
    wxArrayString output, errors;
    if (renpy_sdk_) {
        wxExecute("\"" + wxString::FromUTF8(renpy_sdk_->executable) + "\" --version",
                  output, errors, wxEXEC_SYNC);
        wxString combined;
        for (const auto& line : output) combined += line + "\n";
        for (const auto& line : errors) combined += line + "\n";
        const std::string probed = parse_renpy_version(combined.ToStdString());
        if (!probed.empty()) renpy_sdk_->version = probed;
    }
    if (auto* item = renpy_menu_->FindItem(kRenpyStatus))
        item->SetItemLabel(renpy_sdk_ ? "SDK: " + wxString::FromUTF8(
            renpy_sdk_->version.empty() ? "detected" : renpy_sdk_->version) : "SDK: not found");
    SetStatusText(renpy_sdk_ ? "Ren'Py SDK configured" : "Ren'Py SDK not found");
}

bool MainFrame::LaunchRenpy(const std::vector<wxString>& arguments,
                            const wxExecuteEnv* environment,
                            std::function<void(int, std::string)> completion) {
    if (renpy_pid_ != 0) {
        wxMessageBox("A Ren'Py operation is already running.", "Ren'Py busy",
                     wxOK | wxICON_INFORMATION, this);
        return false;
    }
    if (!renpy_sdk_) {
        wxMessageBox("Ren'Py was not found. Configure the SDK executable first.", "Ren'Py unavailable",
                     wxOK | wxICON_ERROR, this);
        return false;
    }
    if (!project_) {
        wxMessageBox("Connect a Ren'Py project folder first.", "No project",
                     wxOK | wxICON_ERROR, this);
        return false;
    }
    wxArrayString command;
    command.Add(wxString::FromUTF8(renpy_sdk_->executable));
    command.Add(wxString::FromUTF8(project_->root));
    for (const auto& argument : arguments) command.Add(argument);
    wxString display = "Launching:";
    for (const auto& argument : command) display += " \"" + argument + "\"";
    AppendRenpyLog("\n" + wxDateTime::Now().FormatISOCombined(' ') + " " + display + "\n");
    renpy_process_ = std::make_unique<wxProcess>(this);
    renpy_process_->Redirect();
    std::vector<wxCharBuffer> buffers;
    buffers.reserve(command.size());
    for (const auto& argument : command) buffers.push_back(argument.utf8_str());
    std::vector<const char*> argv;
    argv.reserve(buffers.size() + 1);
    for (const auto& buffer : buffers) argv.push_back(buffer.data());
    argv.push_back(nullptr);
    renpy_pid_ = wxExecute(argv.data(), wxEXEC_ASYNC, renpy_process_.get(), environment);
    if (renpy_pid_ <= 0) {
        AppendRenpyLog("Launch failed before the process started.\n");
        renpy_process_.reset();
        renpy_pid_ = 0;
        SetStatusText("Ren'Py launch failed");
        return false;
    }
    renpy_operation_output_.clear();
    renpy_completion_ = std::move(completion);
    renpy_output_timer_.Start(100);
    SetStatusText("Ren'Py running");
    return true;
}

void MainFrame::AppendRenpyLog(const wxString& text) {
    if (renpy_log_) {
        renpy_log_->AppendText(text);
        if (renpy_log_->GetLastPosition() > 30000)
            renpy_log_->Remove(0, renpy_log_->GetLastPosition() - 30000);
    }
    const wxString directory = wxFileName(renpy_log_path_).GetPath();
    if (!wxFileName::DirExists(directory)) wxFileName::Mkdir(directory, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL);
    wxFile file;
    if (file.Open(renpy_log_path_, wxFile::write_append) ||
        (!wxFileName::FileExists(renpy_log_path_) && file.Create(renpy_log_path_)))
        file.Write(text, wxConvUTF8);
}

void MainFrame::DrainRenpyOutput() {
    if (!renpy_process_) return;
    auto drain = [this](wxInputStream* stream) {
        if (!stream) return;
        char buffer[4096];
        while (stream->CanRead()) {
            stream->Read(buffer, sizeof(buffer));
            const std::size_t count = stream->LastRead();
            if (!count) break;
            const wxString text = wxString::FromUTF8(buffer, count);
            renpy_operation_output_.append(buffer, count);
            AppendRenpyLog(text);
        }
    };
    if (renpy_process_->IsInputAvailable()) drain(renpy_process_->GetInputStream());
    if (renpy_process_->IsErrorAvailable()) drain(renpy_process_->GetErrorStream());
}

void MainFrame::OnRunRenpy(wxCommandEvent&) { LaunchRenpyWithRuntime({}); }

void MainFrame::OnStopRenpy(wxCommandEvent&) {
    if (!renpy_pid_) { SetStatusText("No Ren'Py process is running"); return; }
    const auto result = wxProcess::Kill(renpy_pid_, wxSIGTERM, wxKILL_CHILDREN);
    AppendRenpyLog(result == wxKILL_OK ? "Stop requested.\n" : "Could not stop the process.\n");
}

void MainFrame::OnShowRenpyLog(wxCommandEvent&) {
    auto& pane = manager_.GetPane("renpy-log");
    pane.Show(true);
    manager_.Update();
}

void MainFrame::OnRenpyOutputTimer(wxTimerEvent&) { DrainRenpyOutput(); }

void MainFrame::OnRenpyEnded(wxProcessEvent& event) {
    if (!renpy_process_ || event.GetPid() != renpy_pid_) return;
    DrainRenpyOutput();
    renpy_output_timer_.Stop();
    const int code = event.GetExitCode();
    AppendRenpyLog(wxString::Format("Process exited with code %d.\n", code));
    renpy_pid_ = 0;
    renpy_process_.reset();
    auto completion = std::move(renpy_completion_);
    std::string output = std::move(renpy_operation_output_);
    SetStatusText(code == 0 ? "Ren'Py exited" : "Ren'Py failed — see launch log");
    if (completion) completion(code, std::move(output));
}

void MainFrame::OnWarpRenpy(wxCommandEvent&) {
    if (!renpy_sdk_) { LaunchRenpy({}); return; }
    if (!renpy_capabilities(renpy_sdk_->version).warp) {
        wxMessageBox("This Ren'Py SDK version does not support line warping.", "Warp unsupported",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    if (!project_) { LaunchRenpy({}); return; }
    const wxString path = notebook_->CurrentFilePath();
    if (path.empty()) {
        wxMessageBox("Save the active script inside the connected project before warping.",
                     "Warp unavailable", wxOK | wxICON_ERROR, this);
        return;
    }
    std::error_code ec;
    const auto relative = std::filesystem::relative(path.ToStdString(), project_->scripts_root, ec);
    if (ec || relative.empty() || *relative.begin() == "..") {
        wxMessageBox("The active script is outside the connected project's game folder.",
                     "Warp unavailable", wxOK | wxICON_ERROR, this);
        return;
    }
    if (!notebook_->SaveAll()) return;
    const std::string target = relative.generic_string() + ":" + std::to_string(notebook_->CurrentLine());
    LaunchRenpyWithRuntime({"--warp", wxString::FromUTF8(target)});
}

void MainFrame::OnDirectorRenpy(wxCommandEvent&) {
    if (!renpy_sdk_) { LaunchRenpy({}); return; }
    if (!renpy_capabilities(renpy_sdk_->version).director) {
        wxMessageBox("This Ren'Py SDK version does not support Interactive Director.",
                     "Director unsupported", wxOK | wxICON_ERROR, this);
        return;
    }
    if (!notebook_->SaveAll()) return;
    LaunchRenpyWithRuntime({"director"});
}

bool MainFrame::LaunchRenpyWithRuntime(const std::vector<wxString>& arguments) {
    if (!project_) { return LaunchRenpy(arguments); }
    const auto validation = validate_runtime_state(runtime_state_json_);
    if (!validation.valid) {
        wxMessageBox(wxString::FromUTF8(validation.error), "Invalid runtime state", wxOK | wxICON_ERROR, this);
        return false;
    }
    std::string error;
    if (!write_renpy_runtime(project_->scripts_root, &error)) {
        wxMessageBox(wxString::FromUTF8(error), "Runtime helper not written", wxOK | wxICON_ERROR, this);
        return false;
    }
    wxExecuteEnv environment;
    wxGetEnvMap(&environment.env);
    environment.cwd = wxString::FromUTF8(project_->root);
    environment.env["SAY_COUNT_STATE"] = wxString::FromUTF8(runtime_state_json_);
    if (coverage_path_.empty()) SetupCoverageProject();
    environment.env["SAY_COUNT_COVERAGE"] = coverage_path_;
    return LaunchRenpy(arguments, &environment);
}

void MainFrame::OnRuntimePresets(wxCommandEvent&) {
    RuntimePresetDialog dialog(this, *runtime_presets_, runtime_preset_name_, runtime_state_json_);
    if (dialog.ShowModal() != wxID_OK) return;
    runtime_preset_name_ = dialog.selected_name();
    runtime_state_json_ = dialog.selected_json();
    SetStatusText(runtime_preset_name_.empty() ? "Runtime state cleared"
                                               : "Runtime preset selected: " + wxString::FromUTF8(runtime_preset_name_));
}

void MainFrame::OnRunRenpyLint(wxCommandEvent&) {
    if (!notebook_->SaveAll()) return;
    renpy_lint_->SetRunning();
    manager_.GetPane("renpy-lint").Show(true);
    manager_.Update();
    if (!LaunchRenpy({"lint", "--error-code"}, nullptr,
        [this](int code, std::string output) {
            auto issues = parse_renpy_lint(output);
            renpy_lint_->SetResults(std::move(issues), code, std::move(output));
            SetStatusText(code == 0 ? "Official Ren'Py lint completed"
                                    : "Official Ren'Py lint found problems");
        })) {
        renpy_lint_->SetResults({}, -1, {});
    }
}

void MainFrame::OnGenerateTranslations(wxCommandEvent&) { RunLocalizationTool(false); }

void MainFrame::OnExportDialogue(wxCommandEvent&) { RunLocalizationTool(true); }

void MainFrame::RunLocalizationTool(bool dialogue) {
    wxTextEntryDialog prompt(this,
        dialogue ? "Language for dialogue export (for example, english or pt_br):"
                 : "Language to generate (for example, spanish or pt_br):",
        dialogue ? "Export Ren'Py dialogue" : "Generate Ren'Py translations");
    if (prompt.ShowModal() != wxID_OK) return;
    const std::string language = prompt.GetValue().Strip(wxString::both).ToStdString();
    if (!valid_renpy_language(language)) {
        wxMessageBox("Enter a language containing only letters, numbers, and underscores.",
                     "Invalid language", wxOK | wxICON_ERROR, this);
        return;
    }
    if (!notebook_->SaveAll()) return;
    const wxString action = dialogue ? "Exporting dialogue" : "Generating translations";
    renpy_tool_output_->SetValue(action + " for " + wxString::FromUTF8(language) + "...\n");
    manager_.GetPane("renpy-tool-output").Show(true);
    manager_.Update();
    std::vector<wxString> arguments;
    if (dialogue) arguments = {"dialogue", wxString::FromUTF8(language), "--text", "--strings"};
    else arguments = {"translate", wxString::FromUTF8(language)};
    if (!LaunchRenpy(arguments, nullptr,
        [this, dialogue, language](int code, std::string output) {
            const std::string heading = std::string(dialogue ? "Dialogue export" : "Translation generation") +
                " for " + language + " exited with code " + std::to_string(code) + ".\n\n";
            if (output.size() > 30000) output.erase(0, output.size() - 30000);
            renpy_tool_output_->SetValue(wxString::FromUTF8(heading +
                (output.empty() ? "Command completed without output." : output)));
            SetStatusText(code == 0 ? (dialogue ? "Dialogue export completed" : "Translations generated")
                                    : "Ren'Py localization command failed");
        })) {
        renpy_tool_output_->AppendText("Could not start the command.\n");
    }
}

void MainFrame::OnShowAssets(wxCommandEvent&) {
    if (!project_) {
        wxMessageBox("Connect a Ren'Py project folder first.", "No project", wxOK | wxICON_ERROR, this);
        return;
    }
    asset_panel_->SetAssets(discover_project_assets(project_->scripts_root));
    manager_.GetPane("asset-browser").Show(true);
    manager_.Update();
}

void MainFrame::SetupCoverageProject() {
    if (!project_) return;
    coverage_path_ = settings_.data_directory() + wxFILE_SEP_PATH +
        wxString::FromUTF8(coverage_file_name(project_->root));
    coverage_tail_.Reset();
    playthrough_coverage_.clear();
    coverage_labels_ = collect_project_labels(notebook_->ProjectScripts());
    for (const auto& label : coverage_tail_.Read(coverage_path_.ToStdString()))
        playthrough_coverage_.insert(label);
    RefreshCoveragePanel();
}

void MainFrame::RefreshCoveragePanel() {
    if (!project_ || !coverage_panel_) return;
    std::set<std::string> visible_manual;
    std::set<std::string> visible_playthrough;
    for (const auto& label : coverage_labels_) {
        if (manual_coverage_projects_[project_->root].count(label)) visible_manual.insert(label);
        if (playthrough_coverage_.count(label)) visible_playthrough.insert(label);
    }
    coverage_panel_->SetCoverage(coverage_labels_, std::move(visible_manual),
                                 std::move(visible_playthrough));
}

void MainFrame::OnCoverageTimer(wxTimerEvent&) {
    if (!project_ || coverage_path_.empty()) return;
    const auto labels = coverage_tail_.Read(coverage_path_.ToStdString());
    if (labels.empty()) return;
    playthrough_coverage_.insert(labels.begin(), labels.end());
    RefreshCoveragePanel();
}

void MainFrame::OnShowCoverage(wxCommandEvent&) {
    if (!project_) {
        wxMessageBox("Connect a Ren'Py project folder first.", "No project", wxOK | wxICON_ERROR, this);
        return;
    }
    RefreshCoveragePanel();
    manager_.GetPane("coverage").Show(true);
    manager_.Update();
}

void MainFrame::OnFileSystemEvent(wxFileSystemWatcherEvent& event) {
    if (!project_) return;
    if (event.IsError()) {
        SetStatusText("Project watcher warning: " + event.GetErrorDescription());
        return;
    }
    const int change = event.GetChangeType();
    const wxFileName path = event.GetPath();
    const wxFileName new_path = event.GetNewPath();
    const bool script = path.GetExt().CmpNoCase("rpy") == 0;
    const bool new_script = new_path.GetExt().CmpNoCase("rpy") == 0;

    if ((change & wxFSW_EVENT_RENAME) != 0) {
        if (new_script && wxFileName::FileExists(new_path.GetFullPath())) {
            HandleExternalScriptChange(new_path.GetFullPath());
        }
        RefreshProjectDiscovery();
        StartProjectWatcher();
        return;
    }
    if ((change & wxFSW_EVENT_CREATE) != 0) {
        if (script && wxFileName::FileExists(path.GetFullPath()))
            HandleExternalScriptChange(path.GetFullPath());
        RefreshProjectDiscovery();
        if (wxFileName::DirExists(path.GetFullPath())) StartProjectWatcher();
        return;
    }
    if ((change & wxFSW_EVENT_DELETE) != 0) {
        if (script) SetStatusText(path.GetFullName() + " was deleted externally; its open tab was preserved");
        RefreshProjectDiscovery();
        return;
    }
    if (!script || (change & wxFSW_EVENT_MODIFY) == 0) return;

    HandleExternalScriptChange(path.GetFullPath());
}

void MainFrame::OnConnectProject(wxCommandEvent&) {
    wxDirDialog dialog(this, "Choose a Ren'Py project or game folder", wxEmptyString,
                       wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() == wxID_OK) ConnectProjectFolder(dialog.GetPath());
}

void MainFrame::OnRecentProject(wxCommandEvent& event) {
    const int index = event.GetId() - kRecentProjectFirst;
    if (index >= 0 && static_cast<std::size_t>(index) < recent_projects_.size())
        ConnectProjectFolder(recent_projects_[static_cast<std::size_t>(index)]);
}

void MainFrame::BuildFindBar() {
    find_bar_ = new wxPanel(this);
    auto* layout = new wxBoxSizer(wxHORIZONTAL);
    find_input_ = new wxTextCtrl(find_bar_, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                 wxSize(180, -1), wxTE_PROCESS_ENTER);
    find_input_->SetHint("Find");
    replace_input_ = new wxTextCtrl(find_bar_, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                    wxSize(180, -1), wxTE_PROCESS_ENTER);
    replace_input_->SetHint("Replace");
    find_count_ = new wxStaticText(find_bar_, wxID_ANY, "0/0", wxDefaultPosition, wxSize(52, -1),
                                   wxALIGN_CENTER_HORIZONTAL);
    auto* previous = new wxButton(find_bar_, kFindPrevious, "↑", wxDefaultPosition, wxDefaultSize,
                                  wxBU_EXACTFIT);
    auto* next = new wxButton(find_bar_, kFindNext, "↓", wxDefaultPosition, wxDefaultSize,
                              wxBU_EXACTFIT);
    auto* replace = new wxButton(find_bar_, kFindReplace, "Replace");
    auto* replace_all = new wxButton(find_bar_, kFindReplaceAll, "Replace all");
    find_case_ = new wxCheckBox(find_bar_, wxID_ANY, "Aa");
    find_case_->SetToolTip("Match case");
    find_regex_ = new wxCheckBox(find_bar_, wxID_ANY, ".*");
    find_regex_->SetToolTip("Regular expression");
    find_word_ = new wxCheckBox(find_bar_, wxID_ANY, "Whole word");
    find_all_ = new wxCheckBox(find_bar_, wxID_ANY, "All files");
    auto* close = new wxButton(find_bar_, kFindClose, "×", wxDefaultPosition, wxDefaultSize,
                               wxBU_EXACTFIT);

    layout->Add(find_input_, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    layout->Add(replace_input_, 1, wxALIGN_CENTER_VERTICAL | wxALL, 4);
    layout->Add(find_count_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 4);
    for (auto* button : {previous, next, replace, replace_all})
        layout->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    layout->Add(find_case_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    layout->Add(find_regex_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    layout->Add(find_word_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    layout->Add(find_all_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);
    layout->Add(close, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    find_bar_->SetSizer(layout);

    find_input_->Bind(wxEVT_TEXT, &MainFrame::OnFindChanged, this);
    find_case_->Bind(wxEVT_CHECKBOX, &MainFrame::OnFindChanged, this);
    find_regex_->Bind(wxEVT_CHECKBOX, &MainFrame::OnFindChanged, this);
    find_word_->Bind(wxEVT_CHECKBOX, &MainFrame::OnFindChanged, this);
    find_all_->Bind(wxEVT_CHECKBOX, &MainFrame::OnFindChanged, this);
    previous->Bind(wxEVT_BUTTON, &MainFrame::OnFindNext, this);
    next->Bind(wxEVT_BUTTON, &MainFrame::OnFindNext, this);
    replace->Bind(wxEVT_BUTTON, &MainFrame::OnReplace, this);
    replace_all->Bind(wxEVT_BUTTON, &MainFrame::OnReplaceAll, this);
    close->Bind(wxEVT_BUTTON, &MainFrame::OnCloseFind, this);
    find_input_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
        const int direction = wxGetKeyState(WXK_SHIFT) ? -1 : 1;
        if (find_all_->GetValue()) notebook_->FindNextAcrossFiles(direction);
        else notebook_->FindNext(direction);
    });
    replace_input_->Bind(wxEVT_TEXT_ENTER, &MainFrame::OnReplace, this);
}

void MainFrame::BuildFindResults() {
    find_results_ = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                           wxDV_ROW_LINES | wxDV_VERT_RULES);
    find_results_->AppendTextColumn("File", wxDATAVIEW_CELL_INERT, 180, wxALIGN_LEFT,
                                    wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    find_results_->AppendTextColumn("Line", wxDATAVIEW_CELL_INERT, 65, wxALIGN_RIGHT,
                                    wxDATAVIEW_COL_RESIZABLE);
    find_results_->AppendTextColumn("Preview", wxDATAVIEW_CELL_INERT, 600, wxALIGN_LEFT,
                                    wxDATAVIEW_COL_RESIZABLE);
    find_results_->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &MainFrame::OnFindResultActivated, this);
}

void MainFrame::BuildFocusPill() {
    focus_pill_ = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
    focus_pill_->SetBackgroundColour(wxColour("#fff3c4"));
    auto* layout = new wxBoxSizer(wxHORIZONTAL);
    focus_count_ = new wxStaticText(focus_pill_, wxID_ANY,
        wxString::FromUTF8(std::to_string(analysis_.total_words) + " dialogue words"));
    focus_count_->SetForegroundColour(wxColour("#6f4d00"));
    focus_count_->SetFont(focus_count_->GetFont().Bold());
    layout->Add(focus_count_, 0, wxALL, 9);
    focus_pill_->SetSizerAndFit(layout);
    focus_pill_->Hide();
}

void MainFrame::PositionFocusPill() {
    if (!focus_pill_ || !focus_pill_->IsShown()) return;
    focus_pill_->Fit();
    const wxSize client = GetClientSize();
    const wxSize pill = focus_pill_->GetSize();
    focus_pill_->SetPosition(wxPoint(std::max(8, client.x - pill.x - 16),
                                     std::max(8, client.y - pill.y - 16)));
    focus_pill_->Raise();
}

FindOptions MainFrame::CurrentFindOptions() const {
    return {find_case_->GetValue(), find_regex_->GetValue(), find_word_->GetValue()};
}

void MainFrame::UpdateFindStatus(const FindStatus& status) {
    if (!find_count_) return;
    const std::string count = status.valid
        ? std::to_string(status.current) + "/" + std::to_string(status.total)
        : "Invalid";
    find_count_->SetLabel(wxString::FromUTF8(count));
    find_input_->SetBackgroundColour(status.valid ? wxNullColour : wxColour("#ffd8d8"));
    find_input_->Refresh();
    RefreshProjectFindResults(status.valid);
}

void MainFrame::RefreshProjectFindResults(bool valid) {
    if (!find_results_) return;
    find_results_->DeleteAllItems();
    project_matches_.clear();
    auto& pane = manager_.GetPane(find_results_);
    const std::string query = find_input_->GetValue().ToStdString();
    if (!find_all_->GetValue() || query.empty() || !valid) {
        if (pane.IsShown()) { pane.Hide(); manager_.Update(); }
        return;
    }

    const auto found = find_project_matches(notebook_->ProjectScripts(), query, CurrentFindOptions());
    if (!found.valid) {
        if (pane.IsShown()) { pane.Hide(); manager_.Update(); }
        return;
    }
    project_matches_ = found.matches;
    for (const auto& match : project_matches_) {
        wxVector<wxVariant> row;
        row.push_back(wxVariant(wxString::FromUTF8(match.file_name)));
        row.push_back(wxVariant(std::to_string(match.line_number)));
        row.push_back(wxVariant(wxString::FromUTF8(match.preview)));
        find_results_->AppendItem(row, static_cast<wxUIntPtr>(&match - project_matches_.data() + 1));
    }
    find_count_->SetLabel(wxString::FromUTF8(std::to_string(project_matches_.size()) + " matches"));
    if (!pane.IsShown()) { pane.Show(); manager_.Update(); }
}

void MainFrame::OnOpenFind(wxCommandEvent&) {
    manager_.GetPane(find_bar_).Show();
    manager_.Update();
    const std::string selected = notebook_->SelectedText();
    if (!selected.empty() && selected.find('\n') == std::string::npos && selected.find('\r') == std::string::npos)
        find_input_->SetValue(wxString::FromUTF8(selected));
    find_input_->SetFocus();
    find_input_->SelectAll();
    notebook_->SetFindQuery(find_input_->GetValue().ToStdString(), CurrentFindOptions());
}

void MainFrame::OnFindNext(wxCommandEvent& event) {
    if (!manager_.GetPane(find_bar_).IsShown()) {
        OnOpenFind(event);
        return;
    }
    const int direction = event.GetId() == kFindPrevious ? -1 : 1;
    if (find_all_->GetValue()) notebook_->FindNextAcrossFiles(direction);
    else notebook_->FindNext(direction);
}

void MainFrame::OnFindChanged(wxCommandEvent&) {
    notebook_->SetFindQuery(find_input_->GetValue().ToStdString(), CurrentFindOptions());
}

void MainFrame::OnReplace(wxCommandEvent&) {
    notebook_->ReplaceCurrent(replace_input_->GetValue().ToStdString(), find_all_->GetValue());
}

void MainFrame::OnReplaceAll(wxCommandEvent&) {
    std::size_t count = 0;
    if (!find_all_->GetValue()) {
        count = notebook_->ReplaceAll(replace_input_->GetValue().ToStdString());
    } else {
        const auto scripts = notebook_->ProjectScripts();
        const auto preview = preview_project_replacement(
            scripts, find_input_->GetValue().ToStdString(), CurrentFindOptions());
        if (preview.empty()) {
            SetStatusText("No matches to replace");
            return;
        }
        wxArrayString choices;
        wxArrayInt defaults;
        for (std::size_t index = 0; index < preview.size(); ++index) {
            choices.Add(wxString::FromUTF8(preview[index].file_name + " — " +
                                           std::to_string(preview[index].count) + " match" +
                                           (preview[index].count == 1 ? "" : "es")));
            defaults.Add(static_cast<int>(index));
        }
        wxMultiChoiceDialog selection(this, "Select the files to change. Unchecked files remain untouched.",
                                      "Replace across files", choices);
        selection.SetSelections(defaults);
        if (selection.ShowModal() != wxID_OK) return;
        const wxArrayInt chosen = selection.GetSelections();
        if (chosen.empty()) return;
        std::vector<std::size_t> selected_files;
        std::size_t selected_matches = 0;
        for (const int choice : chosen) {
            selected_files.push_back(preview[static_cast<std::size_t>(choice)].file_index);
            selected_matches += preview[static_cast<std::size_t>(choice)].count;
        }
        const std::string question = "Replace " + std::to_string(selected_matches) + " match" +
            (selected_matches == 1 ? "" : "es") + " across " +
            std::to_string(selected_files.size()) + " selected file" +
            (selected_files.size() == 1 ? "?" : "s?");
        if (wxMessageBox(wxString::FromUTF8(question), "Confirm project replacement",
                         wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) return;
        count = notebook_->ReplaceAllAcrossFiles(selected_files,
                                                 replace_input_->GetValue().ToStdString());
    }
    SetStatusText(count == 0 ? "No matches to replace"
                             : wxString::FromUTF8("Replaced " + std::to_string(count) + " match" +
                                                  (count == 1 ? "" : "es")));
}

void MainFrame::OnCloseFind(wxCommandEvent&) {
    notebook_->ClearFind();
    manager_.GetPane(find_bar_).Hide();
    manager_.GetPane(find_results_).Hide();
    manager_.Update();
    notebook_->SetFocus();
}

void MainFrame::OnFindResultActivated(wxDataViewEvent& event) {
    const wxUIntPtr data = find_results_->GetItemData(event.GetItem());
    if (data == 0 || data - 1 >= project_matches_.size()) return;
    const auto match = project_matches_[data - 1];
    notebook_->SelectProjectMatch(match);
}

void MainFrame::OnCharHook(wxKeyEvent& event) {
    if (event.GetKeyCode() == WXK_ESCAPE && find_bar_ && manager_.GetPane(find_bar_).IsShown()) {
        wxCommandEvent close;
        OnCloseFind(close);
        return;
    }
    event.Skip();
}

void MainFrame::OnGoToLine(wxCommandEvent&) {
    wxTextEntryDialog dialog(this, "Enter a line number:", "Go to Line");
    if (dialog.ShowModal() != wxID_OK) return;
    long line = 1;
    if (!dialog.GetValue().ToLong(&line)) line = 1;
    notebook_->JumpToLine(static_cast<std::size_t>(std::max<long>(1, line)));
}

void MainFrame::OnToggleComment(wxCommandEvent&) {
    notebook_->ToggleComments();
}

void MainFrame::OnShowShortcuts(wxCommandEvent&) {
    const wxString shortcuts =
        "Ctrl+F                 Find and replace\n"
        "F3 / Shift+F3         Next / previous match\n"
        "Ctrl+G                 Go to line\n"
        "Ctrl+S                 Save\n"
        "Ctrl+/                 Toggle comment\n"
        "Alt+Up / Alt+Down      Move selected lines\n"
        "Shift+Alt+Up/Down      Duplicate selected lines\n"
        "Ctrl+Up / Ctrl+Down    Previous / next label\n"
        "Ctrl+PageUp/PageDown   Switch tabs\n"
        "Ctrl+= / - / 0         Editor font size\n"
        "Ctrl+K                 This shortcut sheet\n"
        "Ctrl+Shift+F           Focus mode\n"
        "Quotes, (, [           Auto-close or wrap selection\n"
        "Esc                    Close find";
    wxMessageBox(shortcuts, "Keyboard Shortcuts", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnToggleFocus(wxCommandEvent& event) {
    const bool enable = event.IsChecked();
    if (enable == focus_mode_) return;
    if (enable) {
        focus_perspective_ = manager_.SavePerspective();
        auto& panes = manager_.GetAllPanes();
        for (std::size_t index = 0; index < panes.GetCount(); ++index) {
            if (panes.Item(index).name != "editor") panes.Item(index).Hide();
        }
        focus_mode_ = true;
        manager_.Update();
        focus_pill_->Show();
        PositionFocusPill();
    } else {
        focus_pill_->Hide();
        focus_mode_ = false;
        if (!focus_perspective_.empty()) manager_.LoadPerspective(focus_perspective_, true);
        manager_.Update();
        notebook_->SetFocus();
    }
}

void MainFrame::OnSize(wxSizeEvent& event) {
    event.Skip();
    if (focus_mode_) PositionFocusPill();
}

void MainFrame::RestoreWindow() {
    const auto window = settings_.LoadWindow();
    if (!window) {
        Centre();
        normal_geometry_ = GetRect();
        return;
    }

    const wxRect restored(window->position, window->size);
    if (IsGeometryVisible(restored)) {
        SetSize(restored);
    } else {
        SetSize(window->size);
        Centre();
    }
    normal_geometry_ = GetRect();
    if (window->maximized) {
        Maximize();
    }
}

void MainFrame::OnQuit(wxCommandEvent&) {
    Close();
}

void MainFrame::OnNewTab(wxCommandEvent&) {
    notebook_->NewTab();
}

void MainFrame::OnOpen(wxCommandEvent&) {
    wxFileDialog dialog(this, "Open Ren'Py scripts", wxEmptyString, wxEmptyString,
                        "Ren'Py scripts (*.rpy)|*.rpy", wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
    if (dialog.ShowModal() != wxID_OK) return;
    wxArrayString paths;
    dialog.GetPaths(paths);
    notebook_->OpenFiles({paths.begin(), paths.end()});
}

void MainFrame::OnSave(wxCommandEvent&) {
    if (notebook_->SaveCurrent()) TakeSnapshot(false, "Manual save");
}

void MainFrame::OnSaveAs(wxCommandEvent&) { notebook_->SaveCurrentAs(); }

void MainFrame::OnExport(wxCommandEvent& event) {
    wxString filename, wildcard, contents;
    if (event.GetId() == kExportCsv) {
        filename = "say-count-speakers.csv"; wildcard = "CSV files (*.csv)|*.csv";
        contents = wxString::FromUTF8(statistics_csv(analysis_));
    } else if (event.GetId() == kExportJson) {
        filename = "say-count-stats.json"; wildcard = "JSON files (*.json)|*.json";
        contents = wxString::FromUTF8(statistics_json(analysis_));
    } else {
        filename = "say-count-report.html"; wildcard = "HTML files (*.html)|*.html";
        const auto generated = wxDateTime::Now().FormatISOCombined(' ').ToStdString();
        contents = wxString::FromUTF8(statistics_html(analysis_, "Say Count report", generated));
    }
    wxFileDialog dialog(this, "Export statistics", wxEmptyString, filename, wildcard,
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    wxFile output(dialog.GetPath(), wxFile::write);
    if (!output.IsOpened() || !output.Write(contents, wxConvUTF8)) {
        wxMessageBox("Could not write the export file.", "Export failed", wxOK | wxICON_ERROR, this);
        return;
    }
    SetStatusText("Exported " + dialog.GetPath());
}

void MainFrame::OnToggleWrap(wxCommandEvent& event) {
    editor_settings_.word_wrap = event.IsChecked();
    notebook_->SetWordWrap(editor_settings_.word_wrap);
    settings_.SaveEditor(editor_settings_);
}

void MainFrame::OnFontSize(wxCommandEvent& event) {
    if (event.GetId() == kFontIncrease) ++editor_settings_.font_size;
    else if (event.GetId() == kFontDecrease) --editor_settings_.font_size;
    else editor_settings_.font_size = 16;
    editor_settings_.font_size = std::clamp(editor_settings_.font_size, 10, 32);
    notebook_->SetFontSize(editor_settings_.font_size);
    settings_.SaveEditor(editor_settings_);
}

void MainFrame::OnTheme(wxCommandEvent& event) {
    editor_settings_.theme = static_cast<app::EditorTheme>(event.GetId() - kThemeSystem);
    notebook_->SetTheme(editor_settings_.theme);
    settings_.SaveEditor(editor_settings_);
}

void MainFrame::OnCloseTab(wxCommandEvent&) {
    notebook_->CloseCurrentTab();
}

void MainFrame::OnAbout(wxCommandEvent&) {
    wxAboutDialogInfo info;
    info.SetName("Say Count");
    info.SetVersion("0.1.0");
    info.SetDescription("A native Ren'Py writing and word-counting application.");
    wxAboutBox(info, this);
}

void MainFrame::OnClose(wxCloseEvent& event) {
    if (event.CanVeto() && !notebook_->ConfirmCloseAll()) {
        event.Veto();
        return;
    }
    const bool maximized = IsMaximized();
    if (!maximized && !IsIconized()) {
        normal_geometry_ = GetRect();
    }
    if (normal_geometry_.GetWidth() > 0 && normal_geometry_.GetHeight() > 0) {
        settings_.SaveWindow({normal_geometry_.GetPosition(), normal_geometry_.GetSize(), maximized});
    }
    event.Skip();
}

}  // namespace say_count::ui
