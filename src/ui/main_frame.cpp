#include "ui/main_frame.h"

#include <algorithm>
#include <filesystem>
#include <string>

#include <wx/aboutdlg.h>
#include <wx/aui/dockart.h>
#include <wx/control.h>
#include <wx/dcbuffer.h>
#include <wx/display.h>
#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/icon.h>
#include <wx/datetime.h>
#include <wx/dirdlg.h>
#include <wx/evtloop.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stdpaths.h>
#include <wx/textctrl.h>
#include <wx/textdlg.h>
#include <wx/tokenzr.h>
#include <wx/choicdlg.h>
#include <wx/dataview.h>
#include <wx/stream.h>
#include <wx/progdlg.h>

#include "ui/editor_notebook.h"
#include "ui/speaker_stats_panel.h"
#include "ui/outline_panel.h"
#include "ui/diagnostics_panel.h"
#include "ui/conflict_dialog.h"
#include "ui/snapshot_dialog.h"
#include "ui/rename_dialog.h"
#include "ui/manuscript_dialog.h"
#include "ui/chat_conversion_dialog.h"
#include "ui/runtime_preset_dialog.h"
#include "ui/renpy_lint_panel.h"
#include "ui/asset_panel.h"
#include "ui/coverage_panel.h"
#include "ui/route_panel.h"
#include "ui/production_panel.h"
#include "ui/style.h"
#include "ui/palette_dialog.h"
#include "ui/git_dialog.h"
#include "ui/welcome_dialog.h"
#include "ui/story_element_dialog.h"
#include "ui/writer_draft_dialog.h"
#include "core/version.h"
#include "core/indent.h"
#include "core/navigator.h"
#include "core/workspace.h"
#include "core/chat_runtime.h"
#include "app/offline_prose_runner.h"

namespace say_count::ui {
namespace {

constexpr int kDefaultWidth = 1380;
constexpr int kDefaultHeight = 860;
enum MenuId {
    kToggleWrap = wxID_HIGHEST + 1,
    kToggleNvimMotions,
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
    kInsertStoryElement,
    kWriterDraft,
    kWriteManuscript,
    kConvertToChat,
    kConvertChatToDialogue,
    kInstallChatRuntime,
    kConfigureOfflineProseAi,
    kShortcutSheet,
    kManuscriptGuide,
    kChatGuide,
    kFocusMode,
    kConnectProject,
    kRecentProjectFirst,
    kRecentProjectLast = kRecentProjectFirst + 7,
    kReviewConflicts = kRecentProjectLast + 1,
    kSnapshotNow,
    kManageSnapshots,
    kImportProject,
    kExportProject,
    kGitRepository,
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
    kShowRoutes,
    kShowProduction,
    kFixIndents,
    kShowOutline,
    kShowSpeakerStats,
    kShowDiagnostics,
    kQuickOpen,
    kCommandPalette,
};

class CommandButton final : public wxControl {
public:
    CommandButton(wxWindow* parent, const wxString& label, bool primary = false)
        : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                    wxBORDER_NONE | wxWANTS_CHARS), label_(label), primary_(primary) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetFont(style::BodyFont(9, wxFONTWEIGHT_BOLD));
        const wxSize text = GetTextExtent(label_);
        SetMinSize(FromDIP(wxSize(text.x + 30, 36)));
        Bind(wxEVT_PAINT, &CommandButton::OnPaint, this);
        Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent&) { hovered_ = true; Refresh(); });
        Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent&) { hovered_ = false; pressed_ = false; Refresh(); });
        Bind(wxEVT_LEFT_DOWN, [this](wxMouseEvent&) { pressed_ = true; SetFocus(); CaptureMouse(); Refresh(); });
        Bind(wxEVT_LEFT_UP, [this](wxMouseEvent& event) {
            if (HasCapture()) ReleaseMouse();
            const bool activate = pressed_ && GetClientRect().Contains(event.GetPosition());
            pressed_ = false; Refresh();
            if (activate) SendClick();
        });
        Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event) {
            if (event.GetKeyCode() == WXK_SPACE || event.GetKeyCode() == WXK_RETURN) SendClick();
            else event.Skip();
        });
        Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) { Refresh(); event.Skip(); });
        Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) { Refresh(); event.Skip(); });
    }

    void SetCommandLabel(const wxString& label) {
        label_ = label;
        const wxSize size = GetTextExtent(label_);
        SetMinSize(FromDIP(wxSize(size.x + 30, 36)));
        Refresh();
    }

private:
    void SendClick() {
        wxCommandEvent event(wxEVT_BUTTON, GetId());
        event.SetEventObject(this);
        ProcessWindowEvent(event);
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        dc.SetBackground(wxBrush(style::Colors().ink));
        dc.Clear();
        wxColour fill = primary_ ? style::Colors().cue : wxColour("#243650");
        if (hovered_) fill = primary_ ? wxColour("#D6AB4B") : wxColour("#304660");
        if (pressed_) fill = primary_ ? wxColour("#B8892D") : wxColour("#1E2D43");
        dc.SetPen(wxPen(primary_ ? style::Colors().cue : wxColour("#526178"), 1));
        dc.SetBrush(wxBrush(fill));
        wxRect box = GetClientRect();
        box.Deflate(1);
        dc.DrawRoundedRectangle(box, FromDIP(6));
        if (HasFocus()) {
            wxRect focus = box;
            focus.Deflate(3);
            dc.SetPen(wxPen(primary_ ? style::Colors().ink : style::Colors().cue, 1, wxPENSTYLE_DOT));
            dc.SetBrush(*wxTRANSPARENT_BRUSH);
            dc.DrawRoundedRectangle(focus, FromDIP(4));
        }
        dc.SetFont(GetFont());
        dc.SetTextForeground(primary_ ? style::Colors().ink : style::Colors().white);
        const wxSize text = dc.GetTextExtent(label_);
        dc.DrawText(label_, (GetClientSize().x - text.x) / 2, (GetClientSize().y - text.y) / 2);
    }

    wxString label_;
    bool primary_ = false;
    bool hovered_ = false;
    bool pressed_ = false;
};

struct CommandCopy {
    const char* label;
    const char* help;
};

CommandCopy FriendlyCommandCopy(int id) {
    switch (id) {
        case kConnectProject: return {"Open a game...", "Open an existing Ren'Py game folder"};
        case kShowDiagnostics: return {"Problems", "Show problems found in the writing"};
        case kRunRenpyLint: return {"Check game for problems", "Run Ren'Py's complete project check"};
        case kSnapshotNow: return {"Save a version now", "Store a version of every open script"};
        case kManageSnapshots: return {"Version history...", "Preview and restore earlier local versions"};
        case kWriteManuscript: return {"Make game script from writing...", "Review and turn natural writing into Ren'Py script"};
        case kWriterDraft: return {"Writing draft...", "Write naturally in a persistent draft with a generated-script preview"};
        default: return {"", ""};
    }
}

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

void SetApplicationIcon(wxFrame& frame) {
    const wxFileName executable(wxStandardPaths::Get().GetExecutablePath());
    const wxFileName user_install_icon(
        executable.GetPath() + wxFILE_SEP_PATH + ".." + wxFILE_SEP_PATH + "share" +
        wxFILE_SEP_PATH + "icons" + wxFILE_SEP_PATH + "hicolor" + wxFILE_SEP_PATH +
        "512x512" + wxFILE_SEP_PATH + "apps" + wxFILE_SEP_PATH + "say-count.png");
    const wxString candidates[] = {
        user_install_icon.GetFullPath(),
        wxString::FromUTF8(SAY_COUNT_ICON_PATH),
        wxString::FromUTF8(SAY_COUNT_SOURCE_ICON_PATH),
    };
    for (const auto& path : candidates) {
        if (!wxFileName::FileExists(path)) continue;
        wxIcon icon;
        if (icon.LoadFile(path, wxBITMAP_TYPE_PNG)) {
            frame.SetIcon(icon);
            return;
        }
    }
}

}  // namespace

MainFrame::MainFrame()
    : wxFrame(nullptr, wxID_ANY, "Say Count", wxDefaultPosition, wxSize(kDefaultWidth, kDefaultHeight)),
      manager_(this), snapshot_timer_(this), renpy_output_timer_(this), coverage_timer_(this) {
    SetApplicationIcon(*this);
    SetMinSize(wxSize(820, 560));
    editor_settings_ = settings_.LoadEditor();
    DetectRenpy();
    recent_projects_ = settings_.LoadRecentProjects();
    snapshot_store_ = std::make_unique<SnapshotStore>(
        (settings_.data_directory() + wxFILE_SEP_PATH + "snapshots").ToStdString(wxConvUTF8), 50);
    renpy_log_path_ = settings_.data_directory() + wxFILE_SEP_PATH + "renpy-launch.log";
    runtime_presets_ = std::make_unique<RuntimePresetStore>(
        (settings_.data_directory() + wxFILE_SEP_PATH + "runtime-presets.dat").ToStdString(wxConvUTF8));
    manual_coverage_store_ = std::make_unique<ManualCoverageStore>(
        (settings_.data_directory() + wxFILE_SEP_PATH + "manual-coverage.dat").ToStdString(wxConvUTF8));
    manual_coverage_projects_ = manual_coverage_store_->Load();
    BuildMenus();
    CreateStatusBar(2);
    const int status_widths[] = {-1, 112};
    GetStatusBar()->SetStatusWidths(2, status_widths);
    speaker_stats_ = new SpeakerStatsPanel(
        this, wxFileName(settings_.path()).GetPath() + wxFILE_SEP_PATH + "targets.ini");
    outline_ = new OutlinePanel(this);
    diagnostics_ = new DiagnosticsPanel(this);
    route_panel_ = new RoutePanel(this);
    production_panel_ = new ProductionPanel(
        this, settings_.data_directory().ToStdString(wxConvUTF8));
    notebook_ = new EditorNotebook(this, editor_settings_, [this](const wxString& source, const ScriptAnalysis& analysis) {
        analysis_ = analysis;
        // wxString::Format aborts on %zu; compose the text without varargs.
        const std::string text = std::to_string(analysis.total_words) + " dialogue words \xc2\xb7 " +
                                 std::to_string(analysis.dialogue_lines) + " dialogue lines \xc2\xb7 " +
                                 std::to_string(analysis.reading_minutes) + " min reading time";
        SetStatusText(wxString::FromUTF8(text));
        RefreshCueSummary();
        if (workspace_name_ && notebook_) {
            if (project_) {
                workspace_name_->SetLabel(wxFileName(wxString::FromUTF8(project_->root)).GetFullName());
            } else {
                const wxString path = notebook_->CurrentFilePath();
                workspace_name_->SetLabel(path.empty() ? notebook_->CurrentFileName()
                                                        : wxFileName(path).GetFullName());
            }
        }
        if (focus_count_) {
            focus_count_->SetLabel(wxString::FromUTF8(
                std::to_string(analysis.total_words) + " dialogue words"));
            if (focus_mode_) PositionFocusPill();
        }
        speaker_stats_->SetAnalysis(analysis);
        outline_->SetDocument(source, analysis);
        if (notebook_) RefreshRoutes();
        if (notebook_) RefreshProduction();
    });
    notebook_->SetNvimModeHandler([this](bool enabled, const std::string& mode,
                                         const std::string& command_line) {
        wxString label;
        if (enabled) {
            if (!mode.empty() && mode.front() == 'i') label = "VIM  INSERT";
            else if (mode == "v") label = "VIM  VISUAL";
            else if (mode == "V") label = "VIM  V-LINE";
            else if (mode == std::string(1, '\x16')) label = "VIM  V-BLOCK";
            else if (!mode.empty() && mode.front() == 'c') label = "VIM  COMMAND";
            else if (!mode.empty() && mode.front() == 'R') label = "VIM  REPLACE";
            else if (mode.rfind("no", 0) == 0) label = "VIM  OPERATOR";
            else label = "VIM  NORMAL";
        }
        SetStatusText(label, 1);
        if (!command_line.empty()) {
            SetStatusText(wxString::FromUTF8(command_line));
            nvim_command_active_ = true;
        } else if (nvim_command_active_) {
            SetStatusText(wxString::FromUTF8(
                std::to_string(analysis_.total_words) + " dialogue words · " +
                std::to_string(analysis_.dialogue_lines) + " dialogue lines · " +
                std::to_string(analysis_.reading_minutes) + " min reading time"));
            nvim_command_active_ = false;
        }
    });
    RefreshRoutes();
    RefreshProduction();
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
    route_panel_->SetJumpHandler([this](const std::string& file, std::size_t line) {
        const auto scripts = notebook_->ProjectScripts();
        for (std::size_t index = 0; index < scripts.size(); ++index) {
            if (scripts[index].name != file) continue;
            notebook_->SelectFileIndex(index);
            notebook_->JumpToLine(line);
            return;
        }
    });
    production_panel_->SetJumpHandler([this](const std::string& file, std::size_t line) {
        const auto scripts = notebook_->ProjectScripts();
        for (std::size_t index = 0; index < scripts.size(); ++index) {
            if (scripts[index].name != file) continue;
            notebook_->SelectFileIndex(index); notebook_->JumpToLine(line); return;
        }
        if (project_) {
            const auto path = std::filesystem::path(project_->scripts_root) / file;
            if (std::filesystem::exists(path))
                notebook_->OpenAndJump(wxString::FromUTF8(path.string()), line);
        }
    });
    production_panel_->SetSearchHandler([this](const std::string& term) {
        manager_.GetPane(find_bar_).Show(); manager_.Update();
        find_all_->SetValue(false); find_input_->SetValue(wxString::FromUTF8(term));
        notebook_->SetFindQuery(term, CurrentFindOptions()); find_input_->SetFocus();
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
        problem_count_ = diagnostics.size();
        fixable_problem_count_ = count_basic_fixes(diagnostics);
        RefreshCommandBarState();
    });
    diagnostics_->SetJumpHandler([this](const Diagnostic& diagnostic) {
        notebook_->SelectDiagnostic(diagnostic);
    });
    diagnostics_->SetFixHandler([this] { OnFixBasicErrors(); });
    notebook_->SetFindStatusHandler([this](const FindStatus& status) { UpdateFindStatus(status); });
    speaker_stats_->SetLineJumpHandler([this](std::size_t line) { notebook_->JumpToLine(line); });
    outline_->SetJumpHandler([this](std::size_t line) { notebook_->JumpToLine(line); });
    style::ApplyWorkspaceTheme(this);
    BuildCommandBar();
    notebook_->SetDocumentStateHandler([this](const DocumentState& state) {
        current_document_dirty_ = state.dirty;
        current_document_path_ = state.path;
        RefreshCommandBarState();
        RefreshCueSummary();
    });
    if (auto* status = GetStatusBar()) {
        status->SetBackgroundColour(style::Colors().ink);
        status->SetForegroundColour(style::Colors().white);
        status->SetFont(style::UtilityFont(8));
        status->SetMinHeight(26);
    }
    auto* dock_art = manager_.GetArtProvider();
    dock_art->SetMetric(wxAUI_DOCKART_SASH_SIZE, 2);
    dock_art->SetMetric(wxAUI_DOCKART_CAPTION_SIZE, 30);
    dock_art->SetMetric(wxAUI_DOCKART_PANE_BORDER_SIZE, 0);
    dock_art->SetMetric(wxAUI_DOCKART_GRADIENT_TYPE, wxAUI_GRADIENT_NONE);
    dock_art->SetColour(wxAUI_DOCKART_BACKGROUND_COLOUR, style::Colors().canvas);
    dock_art->SetColour(wxAUI_DOCKART_SASH_COLOUR, style::Colors().line);
    dock_art->SetColour(wxAUI_DOCKART_BORDER_COLOUR, style::Colors().line);
    dock_art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_COLOUR, style::Colors().ink);
    dock_art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_GRADIENT_COLOUR, style::Colors().ink);
    dock_art->SetColour(wxAUI_DOCKART_INACTIVE_CAPTION_TEXT_COLOUR, style::Colors().white);
    dock_art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_COLOUR, style::Colors().ink);
    dock_art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_GRADIENT_COLOUR, style::Colors().ink);
    dock_art->SetColour(wxAUI_DOCKART_ACTIVE_CAPTION_TEXT_COLOUR, style::Colors().white);
    dock_art->SetFont(wxAUI_DOCKART_CAPTION_FONT, style::BodyFont(9, wxFONTWEIGHT_BOLD));
    manager_.AddPane(command_bar_, wxAuiPaneInfo().Top().Layer(0).Row(0).Name("command-bar")
                         .CaptionVisible(false).PaneBorder(false).DockFixed(true).Resizable(false)
                         .BestSize(-1, 64).MinSize(-1, 64).CloseButton(false));
    manager_.AddPane(notebook_, wxAuiPaneInfo().CenterPane().Name("editor").PaneBorder(false));
    manager_.AddPane(find_bar_, wxAuiPaneInfo().Top().Name("find-replace").CaptionVisible(false)
                         .Layer(0).Row(1).PaneBorder(false).DockFixed(true).Resizable(false)
                         .BestSize(-1, 48).Hide());
    manager_.AddPane(find_results_, wxAuiPaneInfo().Bottom().Name("find-results").Caption("Find Results")
                         .BestSize(-1, 190).MinSize(300, 110).CloseButton(false).Hide());
    manager_.AddPane(diagnostics_, wxAuiPaneInfo().Bottom().Name("diagnostics").Caption("Problems")
                         .BestSize(-1, 190).MinSize(320, 110).CloseButton(true).MaximizeButton(true).Hide());
    manager_.AddPane(speaker_stats_, wxAuiPaneInfo().Right().Name("speaker-statistics")
                         .Caption("Speaker Statistics").BestSize(340, 500).MinSize(260, 180)
                         .CloseButton(true).MaximizeButton(true).Hide());
    manager_.AddPane(outline_, wxAuiPaneInfo().Left().Name("outline").Caption("Outline")
                         .BestSize(280, 500).MinSize(200, 160).CloseButton(true).MaximizeButton(true).Hide());
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
    manager_.AddPane(route_panel_, wxAuiPaneInfo().Right().Name("routes").Caption("Route Details")
                         .BestSize(380, 520).MinSize(280, 220).CloseButton(true).MaximizeButton(true)
                         .Hide());
    manager_.AddPane(production_panel_, wxAuiPaneInfo().Bottom().Name("production-desk")
                         .Caption("Production Desk").BestSize(-1, 420).MinSize(500, 260)
                         .CloseButton(true).MaximizeButton(true).Hide());
    manager_.Update();
    SetDropTarget(new ScriptDropTarget(notebook_));
    RestoreWindow();

    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    Bind(wxEVT_MENU, &MainFrame::OnQuit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnNewTab, this, wxID_NEW);
    Bind(wxEVT_MENU, &MainFrame::OnOpen, this, wxID_OPEN);
    Bind(wxEVT_MENU, &MainFrame::OnQuickOpen, this, kQuickOpen);
    Bind(wxEVT_MENU, &MainFrame::OnCommandPalette, this, kCommandPalette);
    Bind(wxEVT_MENU, &MainFrame::OnSave, this, wxID_SAVE);
    Bind(wxEVT_MENU, &MainFrame::OnSaveAs, this, wxID_SAVEAS);
    Bind(wxEVT_MENU, &MainFrame::OnCloseTab, this, wxID_CLOSE);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
    Bind(wxEVT_MENU, &MainFrame::OnToggleWrap, this, kToggleWrap);
    Bind(wxEVT_MENU, &MainFrame::OnToggleNvimMotions, this, kToggleNvimMotions);
    Bind(wxEVT_MENU, &MainFrame::OnFontSize, this, kFontIncrease, kFontReset);
    Bind(wxEVT_MENU, &MainFrame::OnTheme, this, kThemeSystem, kThemeDark);
    Bind(wxEVT_MENU, &MainFrame::OnExport, this, kExportCsv, kExportHtml);
    Bind(wxEVT_MENU, &MainFrame::OnOpenFind, this, wxID_FIND);
    Bind(wxEVT_MENU, &MainFrame::OnFindNext, this, kFindNext, kFindPrevious);
    Bind(wxEVT_CHAR_HOOK, &MainFrame::OnCharHook, this);
    Bind(wxEVT_MENU, &MainFrame::OnGoToLine, this, kGoToLine);
    Bind(wxEVT_MENU, &MainFrame::OnToggleComment, this, kToggleComment);
    Bind(wxEVT_MENU, &MainFrame::OnInsertStoryElement, this, kInsertStoryElement);
    Bind(wxEVT_MENU, &MainFrame::OnWriterDraft, this, kWriterDraft);
    Bind(wxEVT_MENU, &MainFrame::OnWriteManuscript, this, kWriteManuscript);
    Bind(wxEVT_MENU, &MainFrame::OnConvertChat, this, kConvertToChat, kConvertChatToDialogue);
    Bind(wxEVT_MENU, &MainFrame::OnInstallChatRuntime, this, kInstallChatRuntime);
    Bind(wxEVT_MENU, &MainFrame::OnConfigureOfflineProseAi, this, kConfigureOfflineProseAi);
    Bind(wxEVT_MENU, &MainFrame::OnShowShortcuts, this, kShortcutSheet);
    Bind(wxEVT_MENU, &MainFrame::OnShowManuscriptGuide, this, kManuscriptGuide);
    Bind(wxEVT_MENU, [this](wxCommandEvent&) { ShowChatGuide(this); }, kChatGuide);
    Bind(wxEVT_MENU, &MainFrame::OnToggleFocus, this, kFocusMode);
    Bind(wxEVT_MENU, &MainFrame::OnTogglePane, this, kShowOutline, kShowDiagnostics);
    Bind(wxEVT_AUI_PANE_CLOSE, &MainFrame::OnPaneClose, this);
    Bind(wxEVT_SIZE, &MainFrame::OnSize, this);
    Bind(wxEVT_MENU, &MainFrame::OnConnectProject, this, kConnectProject);
    Bind(wxEVT_MENU, &MainFrame::OnRecentProject, this, kRecentProjectFirst, kRecentProjectLast);
    Bind(wxEVT_FSWATCHER, &MainFrame::OnFileSystemEvent, this);
    Bind(wxEVT_MENU, &MainFrame::OnReviewConflicts, this, kReviewConflicts);
    Bind(wxEVT_MENU, &MainFrame::OnSnapshotNow, this, kSnapshotNow);
    Bind(wxEVT_MENU, &MainFrame::OnManageSnapshots, this, kManageSnapshots);
    Bind(wxEVT_MENU, &MainFrame::OnImportProject, this, kImportProject);
    Bind(wxEVT_MENU, &MainFrame::OnExportProject, this, kExportProject);
    Bind(wxEVT_MENU, &MainFrame::OnGitRepository, this, kGitRepository);
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
    Bind(wxEVT_MENU, &MainFrame::OnShowRoutes, this, kShowRoutes);
    Bind(wxEVT_MENU, &MainFrame::OnShowProduction, this, kShowProduction);
    Bind(wxEVT_MENU, &MainFrame::OnFixIndents, this, kFixIndents);
    Bind(wxEVT_TIMER, &MainFrame::OnCoverageTimer, this, coverage_timer_.GetId());
    Bind(wxEVT_TIMER, &MainFrame::OnRenpyOutputTimer, this, renpy_output_timer_.GetId());
    Bind(wxEVT_END_PROCESS, &MainFrame::OnRenpyEnded, this);
    Bind(wxEVT_TIMER, &MainFrame::OnSnapshotTimer, this, snapshot_timer_.GetId());
    RestoreWorkspace();
    CallAfter([this] { ShowWelcomeIfNeeded(); });
    snapshot_timer_.Start(10 * 60 * 1000);
    coverage_timer_.Start(750);
}

MainFrame::~MainFrame() {
    coverage_timer_.Stop();
    renpy_output_timer_.Stop();
    if (renpy_process_) {
        renpy_process_->Detach();
        if (renpy_pid_) wxProcess::Kill(renpy_pid_, wxSIGTERM, wxKILL_CHILDREN);
        // A detached wxProcess deletes itself on termination; deleting it here would double-free.
        renpy_process_.release();
    }
    manager_.UnInit();
}

bool MainFrame::OpenInitialFiles(const std::vector<wxString>& paths) {
    return !paths.empty() && notebook_->OpenFiles(paths);
}

void MainFrame::ShowWelcomeIfNeeded() {
    if (!notebook_ || editor_settings_.onboarding_version >= 1 || project_ ||
        notebook_->HasMeaningfulContent()) return;
    WelcomeDialog dialog(this, recent_projects_);
    const int result = dialog.ShowModal();
    editor_settings_.onboarding_version = 1;
    settings_.SaveEditor(editor_settings_);
    if (result != wxID_OK) return;
    wxCommandEvent command;
    switch (dialog.action()) {
        case WelcomeAction::NewStory: StartNewStory(); break;
        case WelcomeAction::OpenGame: OnConnectProject(command); break;
        case WelcomeAction::OpenScript: OnOpen(command); break;
        case WelcomeAction::RecentGame:
            if (!dialog.selected_game().empty()) ConnectProjectFolder(dialog.selected_game());
            break;
        case WelcomeAction::None: break;
    }
}

void MainFrame::StartNewStory() {
    const auto plan = ShowNewStoryDialog(this);
    if (!plan) return;
    wxString preview = "Create this story?\n\n" + wxString::FromUTF8(plan->root) + "\n\nFiles:";
    for (const auto& file : plan->files) preview += "\n• " + wxString::FromUTF8(file.relative_path);
    if (wxMessageBox(preview, "Create New Story", wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES)
        return;
    const auto result = apply_project_creation(*plan);
    if (!result.success) {
        wxMessageBox(wxString::FromUTF8(result.error), "Story was not created", wxOK | wxICON_ERROR, this);
        return;
    }
    if (ConnectProjectFolder(wxString::FromUTF8(result.root))) {
        SetStatusText("Story created — start by replacing the opening line");
    }
}

void MainFrame::BuildCommandBar() {
    const auto& colors = style::Colors();
    command_bar_ = new wxPanel(this);
    command_bar_->SetBackgroundColour(colors.ink);
    command_bar_->SetMinSize(wxSize(-1, 64));
    auto* layout = new wxBoxSizer(wxHORIZONTAL);

    auto* cue = new wxPanel(command_bar_, wxID_ANY, wxDefaultPosition, wxSize(4, 36));
    cue->SetBackgroundColour(colors.cue);
    layout->Add(cue, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 16);

    auto* identity = new wxBoxSizer(wxVERTICAL);
    auto* title = new wxStaticText(command_bar_, wxID_ANY, "Say Count");
    title->SetFont(style::BodyFont(13, wxFONTWEIGHT_BOLD));
    title->SetForegroundColour(colors.white);
    auto* edition = new wxStaticText(command_bar_, wxID_ANY, "REN'PY STORY DESK");
    edition->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    edition->SetForegroundColour(colors.mint);
    identity->Add(title);
    identity->Add(edition, 0, wxTOP, 2);
    layout->Add(identity, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 22);

    auto* divider = new wxPanel(command_bar_, wxID_ANY, wxDefaultPosition, wxSize(1, 34));
    divider->SetBackgroundColour(wxColour("#405068"));
    layout->Add(divider, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 20);

    auto* context = new wxBoxSizer(wxVERTICAL);
    const wxString initial_name = notebook_ ? notebook_->CurrentFileName() : wxString{};
    workspace_name_ = new wxStaticText(command_bar_, wxID_ANY,
                                        initial_name.empty() ? "Loose script" : initial_name);
    workspace_name_->SetFont(style::BodyFont(10, wxFONTWEIGHT_BOLD));
    workspace_name_->SetForegroundColour(colors.white);
    cue_summary_ = new wxStaticText(command_bar_, wxID_ANY, wxEmptyString);
    cue_summary_->SetFont(style::UtilityFont(8));
    cue_summary_->SetForegroundColour(wxColour("#AAB5C3"));
    context->Add(workspace_name_);
    context->Add(cue_summary_, 0, wxTOP, 3);
    layout->Add(context, 0, wxALIGN_CENTER_VERTICAL);
    layout->AddStretchSpacer();

    auto make_action = [&](const wxString& label, bool primary = false) {
        auto* button = new CommandButton(command_bar_, label, primary);
        layout->Add(button, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        return button;
    };
    open_game_button_ = make_action("Open a game");
    save_button_ = make_action("Save");
    write_button_ = make_action("Write naturally");
    history_button_ = make_action("Version history");
    problems_button_ = make_action("Problems");
    run_button_ = make_action("Preview game", true);
    layout->AddSpacer(8);

    open_game_button_->Bind(wxEVT_BUTTON, &MainFrame::OnConnectProject, this);
    save_button_->Bind(wxEVT_BUTTON, &MainFrame::OnSave, this);
    write_button_->Bind(wxEVT_BUTTON, &MainFrame::OnWriterDraft, this);
    history_button_->Bind(wxEVT_BUTTON, &MainFrame::OnManageSnapshots, this);
    problems_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (fixable_problem_count_ > 0) {
            OnFixBasicErrors();
        } else {
            manager_.GetPane("diagnostics").Show(true);
            if (GetMenuBar()) GetMenuBar()->Check(kShowDiagnostics, true);
            manager_.Update();
        }
    });
    run_button_->Bind(wxEVT_BUTTON, &MainFrame::OnRunRenpy, this);
    command_bar_->SetSizer(layout);
    RefreshCommandBarState();
    RefreshCueSummary();
}

void MainFrame::RefreshCommandBarState() {
    if (!command_bar_) return;
    const bool has_game = project_.has_value();
    if (open_game_button_) open_game_button_->Show(!has_game);
    if (history_button_) history_button_->Show(has_game || !current_document_path_.empty());
    if (problems_button_) {
        problems_button_->Show(problem_count_ > 0);
        auto* button = static_cast<CommandButton*>(problems_button_);
        button->SetCommandLabel(fixable_problem_count_ > 0
            ? "Fix " + std::to_string(fixable_problem_count_)
            : "Review " + std::to_string(problem_count_));
    }
    if (write_button_) write_button_->Show(problem_count_ == 0);
    if (run_button_) {
        run_button_->Show(has_game);
        run_button_->Enable(has_game && renpy_sdk_.has_value() && !renpy_process_);
    }
    if (save_button_) save_button_->Enable(current_document_dirty_ || current_document_path_.empty());
    command_bar_->Layout();
}

void MainFrame::RefreshCueSummary() {
    if (!cue_summary_) return;
    wxString state;
    if (current_document_dirty_) state = "UNSAVED CHANGES";
    else if (!last_saved_time_.empty()) state = "SAVED LOCALLY " + last_saved_time_;
    else if (!current_document_path_.empty()) state = "SAVED LOCALLY";
    else state = "NOT SAVED YET";
    cue_summary_->SetLabel(wxString::FromUTF8(std::to_string(analysis_.total_words) + " WORDS") +
                           "   /   " + state);
    command_bar_->Layout();
}

void MainFrame::BuildMenus() {
    auto* file = new wxMenu();
    file->Append(wxID_NEW, "&New\tCtrl+N", "Create a new tab");
    file->Append(wxID_OPEN, "&Open...\tCtrl+O", "Open one or more Ren'Py scripts");
    file->Append(kQuickOpen, "&Quick Open...\tCtrl+P", "Find a project file, label, or character");
    file->Append(wxID_SAVE, "&Save\tCtrl+S", "Save the current script");
    file->Append(wxID_SAVEAS, "Save &As...\tCtrl+Shift+S", "Save the current script under a new name");
    file->Append(wxID_CLOSE, "&Close Tab\tCtrl+W", "Close the current tab");
    file->AppendSeparator();
    const auto open_game = FriendlyCommandCopy(kConnectProject);
    file->Append(kConnectProject, open_game.label, open_game.help);
    recent_projects_menu_ = new wxMenu();
    file->AppendSubMenu(recent_projects_menu_, "Recent Projects");
    file->Append(kReviewConflicts, "Review External &Conflicts...");
    const auto save_version = FriendlyCommandCopy(kSnapshotNow);
    const auto version_history = FriendlyCommandCopy(kManageSnapshots);
    file->Append(kSnapshotNow, save_version.label, save_version.help);
    file->Append(kManageSnapshots, version_history.label, version_history.help);
    file->Append(kGitRepository, "Git &Repository...", "Clone, sync, commit, and push with any Git remote");
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
    edit->Append(kCommandPalette, "Command &Palette...\tCtrl+Shift+P");
    edit->AppendSeparator();
    edit->Append(wxID_FIND, "&Find and Replace...\tCtrl+F");
    edit->Append(kFindNext, "Find &Next\tF3");
    edit->Append(kFindPrevious, "Find &Previous\tShift+F3");
    edit->AppendSeparator();
    edit->Append(kGoToLine, "&Go to Line...\tCtrl+G");
    edit->Append(kToggleComment, "Toggle &Comment\tCtrl+/");
    auto* insert = new wxMenu();
    insert->Append(kInsertStoryElement, "Character, dialogue, choice, or direction...",
                   "Build a story element and preview the generated script");
    insert->Append(kShowAssets, "Image or audio from this game...",
                   "Browse project assets and insert an image, scene, music, or sound instruction");
    edit->AppendSubMenu(insert, "&Insert Into Story");
    edit->AppendCheckItem(kToggleNvimMotions, "&Vim Motions (Built-in)",
                          "Use built-in modal editing without an external process");
    edit->Check(kToggleNvimMotions, editor_settings_.nvim_motions);
    const auto natural_writing = FriendlyCommandCopy(kWriteManuscript);
    edit->Append(kWriterDraft, FriendlyCommandCopy(kWriterDraft).label,
                 FriendlyCommandCopy(kWriterDraft).help);
    edit->Append(kWriteManuscript, natural_writing.label, natural_writing.help);
    edit->Append(kConvertToChat, "Turn Writing Into a &Chat Scene...",
                 "Turn the selected writing (or the whole tab) into a phone/messenger scene");
    edit->Append(kConvertChatToDialogue, "Turn Chat Scene Back Into &Dialogue...",
                 "Turn a chat scene (selected, or the whole tab) back into normal writing");
    edit->Append(kInstallChatRuntime, "Install/Update Chat &App Files...",
                 "Add the chat app to your game project; your own files are never overwritten");
    edit->Append(kConfigureOfflineProseAi, "Configure &Offline Prose AI...",
                 "Optionally improve prose interpretation with a network-blocked local model");
    edit->Append(kFixIndents, "Fix &Indents...", "Preview and repair indentation in the active file");
    edit->Append(kRenameSymbol, "Rename Ren'Py Symbol...", "Preview and safely rename an alias or label project-wide");
    auto* view = new wxMenu();
    view->AppendCheckItem(kToggleWrap, "Word &Wrap", "Soft-wrap long lines");
    view->Check(kToggleWrap, editor_settings_.word_wrap);
    view->AppendCheckItem(kFocusMode, "&Focus Mode\tCtrl+Shift+F", "Hide nonessential panels");
    view->AppendSeparator();
    view->AppendCheckItem(kShowOutline, "Outline panel");
    view->AppendCheckItem(kShowSpeakerStats, "Speaker statistics panel");
    view->AppendCheckItem(kShowDiagnostics, FriendlyCommandCopy(kShowDiagnostics).label);
    view->Append(kShowRoutes, "Route &Details...", "Show route summaries and paths");
    view->Append(kShowProduction, "&Production Desk...", "Show prose and production tools");
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
    help->Append(kManuscriptGuide, "Prose Writing &Guide...",
                 "Learn the supported manuscript formats and conversion rules");
    help->Append(kChatGuide, "Chat &Format Guide...",
                 "Write social-media chat scenes and mix them with regular VN dialogue");
    help->Append(kShortcutSheet, "Keyboard &Shortcuts\tCtrl+K");
    help->AppendSeparator();
    help->Append(wxID_ABOUT, "&About", "About Say Count");
    renpy_menu_ = new wxMenu();
    renpy_menu_->Append(kRunRenpy, "Run Project\tF6");
    renpy_menu_->Append(kWarpRenpy, "Run from Caret\tF7");
    renpy_menu_->Append(kDirectorRenpy, "Interactive Director");
    renpy_menu_->Append(kRuntimePresets, "Runtime State Presets...");
    renpy_menu_->Append(kRunRenpyLint, FriendlyCommandCopy(kRunRenpyLint).label);
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
    auto discovered = discover_project_folder(selected_path.ToStdString(wxConvUTF8), &error);
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
    RefreshRoutes();
    RefreshProduction();
    external_conflicts_.clear();
    std::vector<std::string> recent;
    recent.reserve(recent_projects_.size());
    for (const auto& path : recent_projects_) recent.push_back(path.ToStdString(wxConvUTF8));
    recent = update_recent_projects(recent, project_->root, 8);
    recent_projects_.clear();
    for (const auto& path : recent) recent_projects_.push_back(wxString::FromUTF8(path));
    settings_.SaveRecentProjects(recent_projects_);
    RebuildRecentProjectsMenu();
    const wxFileName root(wxString::FromUTF8(project_->root));
    SetTitle("Say Count — " + root.GetFullName());
    if (workspace_name_) workspace_name_->SetLabel(root.GetFullName());
    RefreshCommandBarState();
    SetStatusText(wxString::FromUTF8("Connected " + project_->root + " — " +
        std::to_string(project_->scripts.size()) + " script" +
        (project_->scripts.size() == 1 ? "" : "s")));
    StartProjectWatcher();
    return true;
}

void MainFrame::StartProjectWatcher() {
    if (!project_) return;
    // Restoring a workspace happens while MainFrame is still being constructed from
    // wxApp::OnInit(), before wxEntry starts the GUI event loop. The Unix watcher
    // requires an active loop when it initializes its inotify descriptor.
    if (!wxEventLoopBase::GetActive()) {
        CallAfter([this] { StartProjectWatcher(); });
        return;
    }
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
    RefreshRoutes();
    RefreshProduction();
}

void MainFrame::HandleExternalScriptChange(const wxString& path) {
    const auto update = notebook_->ReloadExternalFile(path);
    const auto result = update.result;
    const wxFileName file(path);
    const std::string key = file.GetFullPath().ToStdString(wxConvUTF8);
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
                             wxFileName(wxString::FromUTF8(conflict.path)).GetFullName().ToStdString(wxConvUTF8))) {
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
    if (result.created) {
        last_history_time_ = wxDateTime::Now().Format("%H:%M");
        if (!current_document_dirty_) RefreshCueSummary();
    }
    if (!automatic) SetStatusText(result.created ? "Version saved to local history"
                                                  : "No changes since the last saved version");
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
    auto parsed = parse_project_bundle(text.ToStdString(wxConvUTF8));
    if (!parsed.bundle) {
        wxMessageBox("Project import failed: " + wxString::FromUTF8(parsed.error), "Import failed",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    if (!TakeSnapshot(false, "Before project import")) return;
    const std::size_t imported_count = parsed.bundle->files.size();
    ApplyProjectBundle(std::move(*parsed.bundle),
        wxString::Format("Imported project with %zu files", imported_count));
}

ProjectBundle MainFrame::BuildProjectBundle() const {
    ProjectBundle bundle = imported_bundle_.value_or(ProjectBundle{});
    bundle.files = notebook_->ProjectScripts();
    bundle.active_file = notebook_->CurrentFileIndex();
    bundle.exported_at = wxDateTime::UNow().FormatISOCombined('T').ToStdString(wxConvUTF8) + "Z";
    const auto [speakers, scenes] = speaker_stats_->ExportTargets();
    bundle.speaker_targets = speakers;
    bundle.scene_targets = scenes;
    const auto project_target = speaker_stats_->ExportProjectTarget();
    bundle.settings.target = project_target.words > 0 ? std::to_string(project_target.words) : "";
    bundle.settings.line_target = project_target.lines > 0 ? std::to_string(project_target.lines) : "";
    bundle.settings.count_menu_choices = count_menu_choices_;
    bundle.settings.theme = editor_settings_.theme == app::EditorTheme::Dark ? "dark" : "light";
    return bundle;
}

bool MainFrame::ApplyProjectBundle(ProjectBundle bundle, const wxString& success_message) {
    if (!notebook_->RestoreProjectScripts(bundle.files)) return false;
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
    SetStatusText(success_message);
    return true;
}

void MainFrame::OnExportProject(wxCommandEvent&) {
    wxFileDialog dialog(this, "Export Say Count project", wxEmptyString,
                        "say-count-project.saycount.json",
                        "Say Count projects (*.saycount.json)|*.saycount.json|JSON files (*.json)|*.json",
                        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return;
    ProjectBundle bundle = BuildProjectBundle();
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

void MainFrame::OnGitRepository(wxCommandEvent&) {
    GitDialog dialog(this, project_ ? project_->root : std::string{},
                     [this] { return notebook_->SaveAll(); });
    const int result = dialog.ShowModal();
    if (result == wxID_OK && dialog.selected_path()) {
        ConnectProjectFolder(wxString::FromUTF8(*dialog.selected_path()));
        return;
    }
    if (project_) RefreshProjectDiscovery();
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
    while (tokenizer.HasMoreTokens()) paths.push_back(tokenizer.GetNextToken().ToStdString(wxConvUTF8));
    renpy_sdk_ = detect_renpy_sdk({editor_settings_.renpy_sdk_path.ToStdString(wxConvUTF8),
                                   environment.ToStdString(wxConvUTF8), std::move(paths),
                                   wxGetHomeDir().ToStdString(wxConvUTF8)});
}

void MainFrame::OnConfigureRenpy(wxCommandEvent&) {
    wxFileDialog dialog(this, "Choose the Ren'Py SDK executable", wxEmptyString, wxEmptyString,
                        "Ren'Py launcher (renpy;renpy.sh;renpy.exe)|renpy;renpy.sh;renpy.exe|All files|*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    if (!is_renpy_executable(dialog.GetPath().ToStdString(wxConvUTF8))) {
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
        const std::string probed = parse_renpy_version(combined.ToStdString(wxConvUTF8));
        if (!probed.empty()) renpy_sdk_->version = probed;
    }
    if (auto* item = renpy_menu_->FindItem(kRenpyStatus))
        item->SetItemLabel(renpy_sdk_ ? "SDK: " + wxString::FromUTF8(
            renpy_sdk_->version.empty() ? "detected" : renpy_sdk_->version) : "SDK: not found");
    SetStatusText(renpy_sdk_ ? "Ren'Py SDK configured" : "Ren'Py SDK not found");
    RefreshCommandBarState();
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
    renpy_display_pending_.clear();
    renpy_completion_ = std::move(completion);
    renpy_output_timer_.Start(100);
    SetStatusText("Ren'Py running");
    RefreshCommandBarState();
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

void MainFrame::DrainRenpyOutput(bool flush) {
    if (renpy_process_) {
        auto drain = [this](wxInputStream* stream) {
            if (!stream) return;
            char buffer[4096];
            while (stream->CanRead()) {
                stream->Read(buffer, sizeof(buffer));
                const std::size_t count = stream->LastRead();
                if (!count) break;
                renpy_operation_output_.append(buffer, count);
                renpy_display_pending_.append(buffer, count);
            }
        };
        if (renpy_process_->IsInputAvailable()) drain(renpy_process_->GetInputStream());
        if (renpy_process_->IsErrorAvailable()) drain(renpy_process_->GetErrorStream());
    }
    if (renpy_display_pending_.empty()) return;
    // A read can end mid code point; hold the incomplete trailing sequence for the next drain.
    std::size_t take = renpy_display_pending_.size();
    if (!flush) {
        for (std::size_t back = 1; back <= 3 && back <= renpy_display_pending_.size(); ++back) {
            const auto c = static_cast<unsigned char>(renpy_display_pending_[renpy_display_pending_.size() - back]);
            if ((c & 0xc0) == 0x80) continue;
            const std::size_t need = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 1;
            if (need > back) take = renpy_display_pending_.size() - back;
            break;
        }
    }
    if (take == 0) return;
    wxString text = wxString::FromUTF8(renpy_display_pending_.data(), take);
    if (text.empty()) text = wxString::From8BitData(renpy_display_pending_.data(), take);
    renpy_display_pending_.erase(0, take);
    if (!text.empty()) AppendRenpyLog(text);
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
    DrainRenpyOutput(true);
    renpy_output_timer_.Stop();
    const int code = event.GetExitCode();
    AppendRenpyLog(wxString::Format("Process exited with code %d.\n", code));
    renpy_pid_ = 0;
    renpy_process_.reset();
    RefreshCommandBarState();
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
    const auto relative = std::filesystem::relative(path.ToStdString(wxConvUTF8), project_->scripts_root, ec);
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
    const std::string language = prompt.GetValue().Strip(wxString::both).ToStdString(wxConvUTF8);
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
    for (const auto& label : coverage_tail_.Read(coverage_path_.ToStdString(wxConvUTF8)))
        playthrough_coverage_.insert(label);
    RefreshCoveragePanel();
}

void MainFrame::RefreshCoveragePanel() {
    if (!project_ || !coverage_panel_) return;
    std::set<std::string> visible_manual;
    std::set<std::string> visible_playthrough;
    const auto manual = manual_coverage_projects_.find(project_->root);
    for (const auto& label : coverage_labels_) {
        if (manual != manual_coverage_projects_.end() && manual->second.count(label)) visible_manual.insert(label);
        if (playthrough_coverage_.count(label)) visible_playthrough.insert(label);
    }
    coverage_panel_->SetCoverage(coverage_labels_, std::move(visible_manual),
                                 std::move(visible_playthrough));
}

void MainFrame::OnCoverageTimer(wxTimerEvent&) {
    if (!project_ || coverage_path_.empty()) return;
    const auto labels = coverage_tail_.Read(coverage_path_.ToStdString(wxConvUTF8));
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

void MainFrame::RefreshRoutes() {
    if (!notebook_ || !route_panel_) return;
    // Route computation walks every script and enumerates paths; the analysis
    // callback invokes this per edit, so skip the work while the pane is hidden.
    // OnShowRoutes recomputes when the pane reopens.
    const auto& pane = manager_.GetPane("routes");
    if (pane.IsOk() && !pane.IsShown()) return;
    const auto scripts = notebook_->ProjectScripts();
    const auto project_analysis = analyze_project(scripts, {count_menu_choices_});
    route_panel_->SetReport(compute_routes(project_analysis, scripts));
}

void MainFrame::OnShowRoutes(wxCommandEvent&) {
    manager_.GetPane("routes").Show(true);
    manager_.Update();
    RefreshRoutes();
}

void MainFrame::RefreshProduction() {
    if (!notebook_ || !production_panel_) return;
    // Production analysis includes prose, voice, continuity, and accessibility
    // passes across the whole project. Build it only when its pane is visible.
    const auto& pane = manager_.GetPane("production-desk");
    if (pane.IsOk() && !pane.IsShown()) return;
    const auto scripts = notebook_->ProjectScripts();
    const std::size_t active = notebook_->CurrentFileIndex();
    production_panel_->SetProject(
        scripts, analyze_project(scripts, {count_menu_choices_}),
        project_ ? project_->scripts_root : std::string{},
        active < scripts.size() ? scripts[active].name : std::string{}, notebook_->CurrentLine());
}

void MainFrame::OnShowProduction(wxCommandEvent&) {
    manager_.GetPane("production-desk").Show(true);
    manager_.Update();
    RefreshProduction();
}

void MainFrame::OnFixBasicErrors() {
    const auto fixed = fix_basic_diagnostics(notebook_->ProjectScripts());
    if (fixed.total_fixes() == 0) {
        SetStatusText("No automatic repairs are available");
        return;
    }
    const wxString preview = wxString::FromUTF8(
        "Fix " + std::to_string(fixed.total_fixes()) + " problem" +
        (fixed.total_fixes() == 1 ? "" : "s") + " in " +
        std::to_string(fixed.changed_files) + " file" +
        (fixed.changed_files == 1 ? "" : "s") + "?\n\n" +
        std::to_string(fixed.indentation_fixes) + " indentation\n" +
        std::to_string(fixed.quote_fixes) + " unfinished quote\n" +
        std::to_string(fixed.colon_fixes) + " missing colon\n" +
        std::to_string(fixed.empty_block_fixes) + " empty section\n\n" +
        "A local version will be saved first. The edit can also be undone.");
    if (wxMessageBox(preview, "Review Automatic Fixes",
                     wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) return;
    const std::size_t active = notebook_->CurrentFileIndex();
    if (!TakeSnapshot(false, "Before fixing basic diagnostics")) return;
    if (!notebook_->RestoreProjectScripts(fixed.scripts)) return;
    notebook_->SelectFileIndex(active);
    const std::string status = "Fixed " + std::to_string(fixed.total_fixes()) +
        " basic error" + (fixed.total_fixes() == 1 ? "" : "s") + " in " +
        std::to_string(fixed.changed_files) + " file" + (fixed.changed_files == 1 ? "" : "s") +
        " — changes are unsaved";
    SetStatusText(wxString::FromUTF8(status));
}

void MainFrame::OnFixIndents(wxCommandEvent&) {
    auto scripts = notebook_->ProjectScripts();
    const std::size_t active = notebook_->CurrentFileIndex();
    if (active >= scripts.size()) return;
    const auto preview = preview_indent_fix(scripts[active].content);
    if (preview.changes.empty()) { SetStatusText("Indentation already clean"); return; }
    std::string message = std::to_string(preview.changes.size()) +
        " line(s) will change. A snapshot will be created first.\n\n";
    for (std::size_t index = 0; index < std::min<std::size_t>(preview.changes.size(), 10); ++index) {
        const auto& change = preview.changes[index];
        message += "Line " + std::to_string(change.line) + ": " +
                   std::to_string(change.before_width) + " → " +
                   std::to_string(change.after_width) + " spaces\n";
    }
    if (wxMessageBox(wxString::FromUTF8(message), "Fix indentation preview",
                     wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) return;
    if (!TakeSnapshot(false, "Before fixing indentation in " + scripts[active].name)) return;
    scripts[active].content = preview.fixed;
    if (notebook_->RestoreProjectScripts(scripts)) {
        notebook_->SelectFileIndex(active); SetStatusText("Indentation fixed");
    }
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
    const std::string query = find_input_->GetValue().ToStdString(wxConvUTF8);
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
    notebook_->SetFindQuery(find_input_->GetValue().ToStdString(wxConvUTF8), CurrentFindOptions());
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
    notebook_->SetFindQuery(find_input_->GetValue().ToStdString(wxConvUTF8), CurrentFindOptions());
}

void MainFrame::OnReplace(wxCommandEvent&) {
    notebook_->ReplaceCurrent(replace_input_->GetValue().ToStdString(wxConvUTF8), find_all_->GetValue());
}

void MainFrame::OnReplaceAll(wxCommandEvent&) {
    std::size_t count = 0;
    if (!find_all_->GetValue()) {
        count = notebook_->ReplaceAll(replace_input_->GetValue().ToStdString(wxConvUTF8));
    } else {
        const auto scripts = notebook_->ProjectScripts();
        const auto preview = preview_project_replacement(
            scripts, find_input_->GetValue().ToStdString(wxConvUTF8), CurrentFindOptions());
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
                                                 replace_input_->GetValue().ToStdString(wxConvUTF8));
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
    if (event.GetKeyCode() == WXK_ESCAPE && notebook_ && notebook_->ExitNvimMode()) return;
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

void MainFrame::OnInsertStoryElement(wxCommandEvent&) {
    StoryElementDialog dialog(this, notebook_->CurrentIndentation());
    if (dialog.ShowModal() != wxID_OK || dialog.generated_text().empty()) return;
    notebook_->InsertStoryElement(dialog.generated_text());
    SetStatusText("Inserted story element — undo is available");
}

void MainFrame::OnWriterDraft(wxCommandEvent&) {
    if (notebook_->CurrentFilePath().empty()) {
        if (!notebook_->SaveCurrent()) return;
        last_saved_time_ = wxDateTime::Now().Format("%H:%M");
        RefreshCueSummary();
    }
    const std::string path = notebook_->CurrentFilePath().ToStdString(wxConvUTF8);
    WriterDraftDialog dialog(this, path, notebook_->CurrentText());
    if (dialog.ShowModal() != wxID_OK || dialog.generated_script().empty()) return;
    const wxString warning =
        "Replace the current game script with the generated preview?\n\n"
        "The natural-writing draft is already saved separately. A local version of the current script "
        "will be saved first, and Undo remains available.";
    if (wxMessageBox(warning, "Update Game Script",
                     wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) return;
    if (!TakeSnapshot(false, "Before updating script from writing draft")) return;
    if (!notebook_->ReplaceCurrentDocument(dialog.generated_script())) return;
    SetStatusText("Game script updated from writing draft — changes are unsaved and Undo is available");
}

void MainFrame::OnShowShortcuts(wxCommandEvent&) {
    const wxString shortcuts =
        "Ctrl+F                 Find and replace\n"
        "Ctrl+P                 Quick Open files, labels, characters\n"
        "Ctrl+Shift+P           Command Palette\n"
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
        "Vim mode               Motions, counts, operators, visual mode, search, and commands\n"
        "Quotes, (, [           Auto-close or wrap selection\n"
        "Esc                    Close find";
    wxMessageBox(shortcuts, "Keyboard Shortcuts", wxOK | wxICON_INFORMATION, this);
}

void MainFrame::OnShowManuscriptGuide(wxCommandEvent&) {
    ShowManuscriptGuide(this);
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

void MainFrame::OnTogglePane(wxCommandEvent& event) {
    const char* pane = event.GetId() == kShowOutline ? "outline" :
                       event.GetId() == kShowSpeakerStats ? "speaker-statistics" : "diagnostics";
    manager_.GetPane(pane).Show(event.IsChecked());
    manager_.Update();
}

void MainFrame::OnPaneClose(wxAuiManagerEvent& event) {
    if (event.GetPane()) {
        const wxString& name = event.GetPane()->name;
        int id = wxID_NONE;
        if (name == "outline") id = kShowOutline;
        else if (name == "speaker-statistics") id = kShowSpeakerStats;
        else if (name == "diagnostics") id = kShowDiagnostics;
        if (id != wxID_NONE && GetMenuBar()) GetMenuBar()->Check(id, false);
    }
    event.Skip();
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

void MainFrame::RestoreWorkspace() {
    WorkspaceStore store((settings_.data_directory() + wxFILE_SEP_PATH + "workspace.dat")
                             .ToStdString(wxConvUTF8));
    const auto state = store.Load();
    if (!state) return;
    if (!state->project_root.empty() && std::filesystem::exists(state->project_root))
        ConnectProjectFolder(wxString::FromUTF8(state->project_root));
    std::vector<wxString> paths;
    for (const auto& file : state->files)
        if (wxFileName::FileExists(wxString::FromUTF8(file.path)))
            paths.push_back(wxString::FromUTF8(file.path));
    if (!paths.empty()) notebook_->OpenFiles(paths, false);
    notebook_->RestoreWorkspaceViews(state->files, state->active_file);
    if (!state->perspective.empty()) {
        manager_.LoadPerspective(wxString::FromUTF8(state->perspective), true);
        manager_.GetPane("command-bar").Show(true);
        manager_.Update();
    }
    RefreshRoutes();
    RefreshProduction();
    if (GetMenuBar()) {
        GetMenuBar()->Check(kShowOutline, manager_.GetPane("outline").IsShown());
        GetMenuBar()->Check(kShowSpeakerStats, manager_.GetPane("speaker-statistics").IsShown());
        GetMenuBar()->Check(kShowDiagnostics, manager_.GetPane("diagnostics").IsShown());
    }
}

void MainFrame::SaveWorkspace() {
    WorkspaceState state;
    if (project_) state.project_root = project_->root;
    state.files = notebook_->CaptureWorkspaceFiles();
    state.active_file = notebook_->CurrentFilePath().ToStdString(wxConvUTF8);
    state.perspective = (focus_mode_ ? focus_perspective_ : manager_.SavePerspective())
                            .ToStdString(wxConvUTF8);
    WorkspaceStore store((settings_.data_directory() + wxFILE_SEP_PATH + "workspace.dat")
                             .ToStdString(wxConvUTF8));
    store.Save(state);
}

void MainFrame::OnQuickOpen(wxCommandEvent&) {
    const auto navigation = build_navigation_index(notebook_->ProjectScripts());
    std::vector<PaletteEntry> entries;
    entries.reserve(navigation.size());
    for (std::size_t index = 0; index < navigation.size(); ++index)
        entries.push_back({wxString::FromUTF8(navigation[index].title),
                           wxString::FromUTF8(navigation[index].detail),
                           static_cast<int>(index)});
    PaletteDialog dialog(this, "Quick Open", "Search files, labels, and characters", std::move(entries));
    if (dialog.ShowModal() != wxID_OK || dialog.SelectedId() < 0) return;
    const auto& selected = navigation[static_cast<std::size_t>(dialog.SelectedId())];
    notebook_->SelectFileIndex(selected.file_index);
    if (selected.kind != say_count::NavigationKind::File) notebook_->JumpToLine(selected.line);
    else notebook_->SetFocus();
}

void MainFrame::DispatchCommand(int id) {
    wxCommandEvent command(wxEVT_MENU, id);
    command.SetEventObject(this);
    if (id == kToggleWrap || id == kToggleNvimMotions || id == kFocusMode || id == kShowOutline ||
        id == kShowSpeakerStats || id == kShowDiagnostics) {
        const bool checked = id == kToggleWrap ? !editor_settings_.word_wrap :
            id == kToggleNvimMotions ? !editor_settings_.nvim_motions :
            id == kFocusMode ? !focus_mode_ :
            id == kShowOutline ? !manager_.GetPane("outline").IsShown() :
            id == kShowSpeakerStats ? !manager_.GetPane("speaker-statistics").IsShown() :
                                      !manager_.GetPane("diagnostics").IsShown();
        command.SetInt(checked ? 1 : 0);
        if (GetMenuBar()) GetMenuBar()->Check(id, checked);
    }
    if (id >= kThemeSystem && id <= kThemeDark && GetMenuBar()) GetMenuBar()->Check(id, true);
    ProcessWindowEvent(command);
}

void MainFrame::OnCommandPalette(wxCommandEvent&) {
    const std::vector<PaletteEntry> commands{
        {"New script", "Ctrl+N · File", wxID_NEW}, {"Open scripts", "Ctrl+O · File", wxID_OPEN},
        {"Quick Open", "Ctrl+P · Navigation", kQuickOpen}, {"Save", "Ctrl+S · File", wxID_SAVE},
        {"Save As", "Ctrl+Shift+S · File", wxID_SAVEAS}, {"Close tab", "Ctrl+W · File", wxID_CLOSE},
        {FriendlyCommandCopy(kConnectProject).label, "Game", kConnectProject},
        {FriendlyCommandCopy(kSnapshotNow).label, "Local history · Game", kSnapshotNow},
        {FriendlyCommandCopy(kManageSnapshots).label, "Local history · Game", kManageSnapshots},
        {"Review external conflicts", "Project", kReviewConflicts},
        {"Git repository", "Clone, sync, commit, and push · Project", kGitRepository},
        {"Import Say Count project", "File", kImportProject}, {"Export complete project", "File", kExportProject},
        {"Export speaker statistics", "CSV · File", kExportCsv}, {"Export full statistics", "JSON · File", kExportJson},
        {"Export standalone report", "HTML · File", kExportHtml}, {"Find and replace", "Ctrl+F · Edit", wxID_FIND},
        {"Go to line", "Ctrl+G · Navigation", kGoToLine}, {"Toggle comment", "Ctrl+/ · Edit", kToggleComment},
        {"Insert into story", "Character, dialogue, choice, scene, audio, or jump · Writing", kInsertStoryElement},
        {FriendlyCommandCopy(kWriterDraft).label, "Persistent natural-writing source · Writing", kWriterDraft},
        {"Toggle built-in Vim motions", "Normal/insert modes · Edit", kToggleNvimMotions},
        {FriendlyCommandCopy(kWriteManuscript).label, "Writing", kWriteManuscript},
        {"Turn writing into a chat scene", "Edit", kConvertToChat},
        {"Turn chat scene back into dialogue", "Edit", kConvertChatToDialogue},
        {"Install/update chat app files", "Edit", kInstallChatRuntime},
        {"Fix indents", "Edit", kFixIndents}, {"Rename Ren'Py symbol", "Project", kRenameSymbol},
        {"Toggle word wrap", "View", kToggleWrap}, {"Toggle focus mode", "Ctrl+Shift+F · View", kFocusMode},
        {"Toggle outline", "View", kShowOutline}, {"Toggle speaker statistics", "View", kShowSpeakerStats},
        {"Toggle problems", "View", kShowDiagnostics}, {"Route details", "View", kShowRoutes},
        {"Production Desk", "View", kShowProduction}, {"Increase editor font", "Ctrl+= · View", kFontIncrease},
        {"Decrease editor font", "Ctrl+- · View", kFontDecrease}, {"Reset editor font", "Ctrl+0 · View", kFontReset},
        {"Use system theme", "View", kThemeSystem}, {"Use light theme", "View", kThemeLight},
        {"Use dark theme", "View", kThemeDark},
        {"Run project", "F6 · Ren'Py", kRunRenpy}, {"Run from caret", "F7 · Ren'Py", kWarpRenpy},
        {"Interactive Director", "Ren'Py", kDirectorRenpy}, {"Runtime state presets", "Ren'Py", kRuntimePresets},
        {FriendlyCommandCopy(kRunRenpyLint).label, "Ren'Py", kRunRenpyLint},
        {"Generate translations", "Ren'Py", kGenerateTranslations},
        {"Export dialogue", "Ren'Py", kExportDialogue}, {"Browse project assets", "Ren'Py", kShowAssets},
        {"Label coverage", "Ren'Py", kShowCoverage}, {"Stop running project", "Shift+F6 · Ren'Py", kStopRenpy},
        {"Show launch log", "Ren'Py", kShowRenpyLog}, {"Configure Ren'Py SDK", "Ren'Py", kConfigureRenpy},
        {"Prose writing guide", "Help", kManuscriptGuide},
        {"Chat format guide", "Help", kChatGuide},
        {"Keyboard shortcuts", "Ctrl+K · Help", kShortcutSheet}, {"About Say Count", "Help", wxID_ABOUT}
    };
    PaletteDialog dialog(this, "Command Palette", "Type an action", commands);
    if (dialog.ShowModal() == wxID_OK && dialog.SelectedId() >= 0)
        DispatchCommand(dialog.SelectedId());
}

void MainFrame::OnQuit(wxCommandEvent&) {
    Close();
}

void MainFrame::OnNewTab(wxCommandEvent&) {
    notebook_->NewTab();
}

void MainFrame::OnWriteManuscript(wxCommandEvent&) {
    auto preview = notebook_->PrepareManuscriptConversion();
    if (!preview) {
        wxMessageBox("Type prose in the active script editor, then choose Convert prose.\n\n"
                     "To convert only part of a tab, select that text first.",
                     "Nothing to convert", wxOK | wxICON_INFORMATION, this);
        return;
    }
    if (preview->inclusive_conversion.script == preview->source) {
        SetStatusText("Already looks like Ren'Py — nothing changed");
        return;
    }
    bool offline_ai_used = false;
    if (editor_settings_.offline_prose_ai) {
        wxProgressDialog progress("Offline prose AI",
            "Interpreting prose locally · network access is blocked", 100, this,
            wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_ELAPSED_TIME | wxPD_SMOOTH);
        const auto characters = analyze_project(notebook_->ProjectScripts()).character_names;
        const auto local = app::run_offline_prose_model(
            preview->source, characters,
            editor_settings_.offline_ai_runner_path.ToStdString(wxConvUTF8),
            editor_settings_.offline_ai_model_path.ToStdString(wxConvUTF8),
            [&progress] {
                bool keep_going = true;
                progress.Pulse("Interpreting prose locally · network access is blocked", &keep_going);
                return keep_going;
            });
        if (local.cancelled) {
            SetStatusText("Offline prose conversion cancelled — nothing changed");
            return;
        }
        if (!local.error.empty()) {
            wxMessageBox(wxString::FromUTF8(local.error) +
                         "\n\nThe rule-based converter will be used instead.",
                         "Offline AI unavailable", wxOK | wxICON_WARNING, this);
        } else {
            offline_ai_used = notebook_->PrepareOfflineAiConversion(
                &*preview, local.rewrite.manuscript);
        }
    }
    ManuscriptReviewDialog dialog(this, preview->lines, preview->safe_conversion,
                                  preview->inclusive_conversion, preview->source, offline_ai_used);
    if (dialog.ShowModal() != wxID_OK) return;
    const bool include_uncertain = dialog.include_uncertain_lines();
    const auto& conversion = dialog.selected_conversion();
    if (!notebook_->ApplyManuscriptConversion(*preview, include_uncertain)) {
        SetStatusText("Prose conversion was not applied");
        return;
    }
    const std::size_t pending = include_uncertain ? 0 : dialog.uncertain_line_count();
    const std::string summary = "Converted " + std::to_string(dialog.converted_line_count()) +
        " prose · preserved " + std::to_string(dialog.preserved_line_count()) +
        " code · reused " + std::to_string(conversion.reused_aliases.size()) +
        (conversion.reused_aliases.size() == 1 ? " character · created " : " characters · created ") +
        std::to_string(conversion.characters.size()) +
        (conversion.characters.size() == 1 ? " character · " : " characters · ") +
        std::to_string(pending) + (pending == 1 ? " needs review" : " need review") +
        " · Undo restores it";
    SetStatusText(wxString::FromUTF8(summary));
}

void MainFrame::OnConvertChat(wxCommandEvent& event) {
    auto preview = notebook_->PrepareTextReplacement();
    if (!preview) {
        wxMessageBox("Select text or type into the active editor first.", "Nothing to convert",
                     wxOK | wxICON_INFORMATION, this);
        return;
    }
    const bool to_chat = event.GetId() == kConvertToChat;
    const auto characters = analyze_project(notebook_->ProjectScripts()).character_names;
    ChatConversionDialog dialog(this, preview->source, to_chat, characters);
    if (dialog.ShowModal() != wxID_OK) return;
    const auto& conversion = dialog.conversion();
    if (!notebook_->ApplyTextReplacement(*preview, conversion.text)) {
        SetStatusText("Chat conversion was not applied");
        return;
    }
    SetStatusText(wxString::Format("Converted %zu messages · %zu metadata losses · Undo restores it",
                                  conversion.messages, conversion.losses.size()));
    if (to_chat && project_) {
        std::error_code probe;
        const auto runtime = std::filesystem::path(project_->scripts_root) /
            "vendor" / "chat_program" / "chat_program.rpy";
        if (!std::filesystem::exists(runtime, probe) &&
            wxMessageBox("Your chat scene is in the script, but the chat app itself is not "
                         "installed in this project yet, so the scene cannot play in the game.\n\n"
                         "Install the chat app files now? Your writing will not be touched.",
                         "Install the chat app?", wxYES_NO | wxICON_QUESTION, this) == wxYES) {
            wxCommandEvent install;
            OnInstallChatRuntime(install);
        }
    }
}

void MainFrame::OnInstallChatRuntime(wxCommandEvent&) {
    if (!project_) {
        wxMessageBox("Connect a Ren'Py project folder before installing the chat runtime.",
                     "Project required", wxOK | wxICON_INFORMATION, this);
        return;
    }
    const wxFileName executable(wxStandardPaths::Get().GetExecutablePath());
    const auto user_install_resources =
        std::filesystem::path(executable.GetPath().ToStdString(wxConvUTF8)) /
        ".." / "share" / "say-count" / "chat_program";
    std::string resource_directory = SAY_COUNT_CHAT_RESOURCE_PATH;
    std::error_code probe;
    if (std::filesystem::is_directory(user_install_resources, probe))
        resource_directory = user_install_resources.lexically_normal().string();
    else if (std::filesystem::is_directory(SAY_COUNT_CHAT_INSTALLED_PATH, probe))
        resource_directory = SAY_COUNT_CHAT_INSTALLED_PATH;
    auto inspection = inspect_chat_runtime(resource_directory, project_->scripts_root);
    if (!inspection.error.empty()) {
        wxMessageBox(wxString::FromUTF8(inspection.error), "Install failed",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    std::vector<std::string> changed;
    std::vector<std::string> protected_files;
    bool exported_upgrade = false;
    for (const auto& file : inspection.files) {
        if (file.state == ChatRuntimeFileState::Modified) protected_files.push_back(file.name);
        else if (file.state == ChatRuntimeFileState::NewFile) changed.push_back(file.name);
    }
    if (!protected_files.empty()) {
        std::string message = "These installed files contain local changes:\n\n";
        for (const auto& name : protected_files) message += "• " + name + "\n";
        message += "\nNothing will be overwritten. Export the pinned runtime beside the project "
                   "instead? The export will not be loaded until you merge it manually.";
        if (wxMessageBox(wxString::FromUTF8(message), "Local chat runtime protected",
                         wxYES_NO | wxNO_DEFAULT | wxICON_WARNING, this) != wxYES) return;
        const auto export_root = std::filesystem::path(project_->root) /
            "say-count-chat-runtime-85ee08d";
        inspection = inspect_chat_runtime(resource_directory, export_root.string());
        exported_upgrade = true;
        if (!inspection.error.empty() || inspection.has_modified_files()) {
            wxMessageBox("The upgrade-export destination is unavailable or also customized.",
                         "Install stopped", wxOK | wxICON_WARNING, this);
            return;
        }
        changed.clear();
        for (const auto& file : inspection.files)
            if (file.state == ChatRuntimeFileState::NewFile) changed.push_back(file.name);
    }
    if (changed.empty()) {
        SetStatusText("Pinned chat_program runtime is already installed and identical");
        return;
    }
    std::string preview = "Install pinned chat_program commit 85ee08d into:\n" +
        inspection.destination_directory + "\n\nNew files:\n";
    for (const auto& name : changed) preview += "• " + name + "\n";
    preview += "\nExisting files will not be overwritten.";
    if (wxMessageBox(wxString::FromUTF8(preview), "Install Chat Runtime",
                     wxYES_NO | wxNO_DEFAULT | wxICON_QUESTION, this) != wxYES) return;
    const auto installed = install_chat_runtime(inspection);
    if (!installed.error.empty()) {
        wxMessageBox(wxString::FromUTF8(installed.error), "Install failed",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    RefreshProjectDiscovery();
    SetStatusText(exported_upgrade
        ? "Exported protected chat runtime upgrade outside game; merge it manually"
        : "Installed pinned chat_program runtime with license and provenance");
}

void MainFrame::OnConfigureOfflineProseAi(wxCommandEvent&) {
    if (!ConfigureOfflineProseAi(this, &editor_settings_)) return;
    if (!settings_.SaveEditor(editor_settings_)) {
        wxMessageBox("Could not save the offline AI setting.", "Settings failed",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    SetStatusText(editor_settings_.offline_prose_ai
        ? "Offline prose AI enabled · model runs locally with network blocked"
        : "Offline prose AI disabled · using the rule-based converter");
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
    if (!notebook_->SaveCurrent()) return;
    last_saved_time_ = wxDateTime::Now().Format("%H:%M");
    RefreshCueSummary();
    TakeSnapshot(false, "Manual save");
}

void MainFrame::OnSaveAs(wxCommandEvent&) {
    if (!notebook_->SaveCurrentAs()) return;
    last_saved_time_ = wxDateTime::Now().Format("%H:%M");
    RefreshCueSummary();
    TakeSnapshot(false, "Manual save");
}

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
        const auto generated = wxDateTime::Now().FormatISOCombined(' ').ToStdString(wxConvUTF8);
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

void MainFrame::OnToggleNvimMotions(wxCommandEvent& event) {
    editor_settings_.nvim_motions = event.IsChecked();
    notebook_->SetNvimMotions(editor_settings_.nvim_motions);
    settings_.SaveEditor(editor_settings_);
    SetStatusText(editor_settings_.nvim_motions
        ? "Built-in Vim motions enabled · NORMAL mode"
        : "Built-in Vim motions disabled");
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
    // A second close request while the discard dialog is open must not re-enter
    // ConfirmCloseAll or destroy the frame under the running modal.
    if (close_confirm_open_) {
        if (event.CanVeto()) { event.Veto(); return; }
    } else if (event.CanVeto()) {
        close_confirm_open_ = true;
        const bool proceed = notebook_->ConfirmCloseAll();
        close_confirm_open_ = false;
        if (!proceed) {
            event.Veto();
            return;
        }
    }
    const bool maximized = IsMaximized();
    if (!maximized && !IsIconized()) {
        normal_geometry_ = GetRect();
    }
    if (normal_geometry_.GetWidth() > 0 && normal_geometry_.GetHeight() > 0) {
        settings_.SaveWindow({normal_geometry_.GetPosition(), normal_geometry_.GetSize(), maximized});
    }
    SaveWorkspace();
    event.Skip();
}

}  // namespace say_count::ui
