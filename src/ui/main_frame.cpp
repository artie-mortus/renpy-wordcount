#include "ui/main_frame.h"

#include <algorithm>
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
#include <wx/choicdlg.h>
#include <wx/dataview.h>

#include "ui/editor_notebook.h"
#include "ui/speaker_stats_panel.h"
#include "ui/outline_panel.h"
#include "ui/diagnostics_panel.h"
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
      manager_(this) {
    SetMinSize(wxSize(400, 300));
    editor_settings_ = settings_.LoadEditor();
    recent_projects_ = settings_.LoadRecentProjects();
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
}

MainFrame::~MainFrame() {
    manager_.UnInit();
}

void MainFrame::BuildMenus() {
    auto* file = new wxMenu();
    file->Append(wxID_NEW, "&New\tCtrl+N", "Create a new tab");
    file->Append(wxID_OPEN, "&Open…\tCtrl+O", "Open one or more Ren'Py scripts");
    file->Append(wxID_SAVE, "&Save\tCtrl+S", "Save the current script");
    file->Append(wxID_SAVEAS, "Save &As…\tCtrl+Shift+S", "Save the current script under a new name");
    file->Append(wxID_CLOSE, "&Close Tab\tCtrl+W", "Close the current tab");
    file->AppendSeparator();
    file->Append(kConnectProject, "Connect Project &Folder…", "Open every Ren'Py script in a project");
    recent_projects_menu_ = new wxMenu();
    file->AppendSubMenu(recent_projects_menu_, "Recent Projects");
    RebuildRecentProjectsMenu();
    file->AppendSeparator();
    auto* export_menu = new wxMenu();
    export_menu->Append(kExportCsv, "Speaker statistics (CSV)…");
    export_menu->Append(kExportJson, "Full statistics (JSON)…");
    export_menu->Append(kExportHtml, "Standalone report (HTML)…");
    file->AppendSubMenu(export_menu, "Export &Statistics");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "&Quit\tCtrl+Q", "Quit Say Count");

    auto* edit = new wxMenu();
    edit->Append(wxID_FIND, "&Find and Replace…\tCtrl+F");
    edit->Append(kFindNext, "Find &Next\tF3");
    edit->Append(kFindPrevious, "Find &Previous\tShift+F3");
    edit->AppendSeparator();
    edit->Append(kGoToLine, "&Go to Line…\tCtrl+G");
    edit->Append(kToggleComment, "Toggle &Comment\tCtrl+/");
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

    auto* menu_bar = new wxMenuBar();
    menu_bar->Append(file, "&File");
    menu_bar->Append(edit, "&Edit");
    menu_bar->Append(view, "&View");
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
    return true;
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

void MainFrame::OnSave(wxCommandEvent&) { notebook_->SaveCurrent(); }

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
