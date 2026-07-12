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
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/editor_notebook.h"
#include "ui/speaker_stats_panel.h"
#include "ui/outline_panel.h"
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
    BuildMenus();
    CreateStatusBar();
    speaker_stats_ = new SpeakerStatsPanel(
        this, wxFileName(settings_.path()).GetPath() + wxFILE_SEP_PATH + "targets.ini");
    outline_ = new OutlinePanel(this);
    notebook_ = new EditorNotebook(this, editor_settings_, [this](const wxString& source, const ScriptAnalysis& analysis) {
        analysis_ = analysis;
        // wxString::Format aborts on %zu; compose the text without varargs.
        const std::string text = std::to_string(analysis.total_words) + " dialogue words \xc2\xb7 " +
                                 std::to_string(analysis.dialogue_lines) + " dialogue lines \xc2\xb7 " +
                                 std::to_string(analysis.reading_minutes) + " min reading time";
        SetStatusText(wxString::FromUTF8(text));
        speaker_stats_->SetAnalysis(analysis);
        outline_->SetDocument(source, analysis);
    });
    BuildFindBar();
    notebook_->SetFindStatusHandler([this](const FindStatus& status) { UpdateFindStatus(status); });
    speaker_stats_->SetLineJumpHandler([this](std::size_t line) { notebook_->JumpToLine(line); });
    outline_->SetJumpHandler([this](std::size_t line) { notebook_->JumpToLine(line); });
    manager_.AddPane(notebook_, wxAuiPaneInfo().CenterPane().Name("editor"));
    manager_.AddPane(find_bar_, wxAuiPaneInfo().Top().Name("find-replace").CaptionVisible(false)
                         .PaneBorder(false).DockFixed(true).Resizable(false).BestSize(-1, 44).Hide());
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
    auto* view = new wxMenu();
    view->AppendCheckItem(kToggleWrap, "Word &Wrap", "Soft-wrap long lines");
    view->Check(kToggleWrap, editor_settings_.word_wrap);
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

    auto* menu_bar = new wxMenuBar();
    menu_bar->Append(file, "&File");
    menu_bar->Append(edit, "&Edit");
    menu_bar->Append(view, "&View");
    menu_bar->Append(help, "&Help");
    SetMenuBar(menu_bar);
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
    layout->Add(close, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 4);
    find_bar_->SetSizer(layout);

    find_input_->Bind(wxEVT_TEXT, &MainFrame::OnFindChanged, this);
    find_case_->Bind(wxEVT_CHECKBOX, &MainFrame::OnFindChanged, this);
    find_regex_->Bind(wxEVT_CHECKBOX, &MainFrame::OnFindChanged, this);
    find_word_->Bind(wxEVT_CHECKBOX, &MainFrame::OnFindChanged, this);
    previous->Bind(wxEVT_BUTTON, &MainFrame::OnFindNext, this);
    next->Bind(wxEVT_BUTTON, &MainFrame::OnFindNext, this);
    replace->Bind(wxEVT_BUTTON, &MainFrame::OnReplace, this);
    replace_all->Bind(wxEVT_BUTTON, &MainFrame::OnReplaceAll, this);
    close->Bind(wxEVT_BUTTON, &MainFrame::OnCloseFind, this);
    find_input_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) {
        notebook_->FindNext(wxGetKeyState(WXK_SHIFT) ? -1 : 1);
    });
    replace_input_->Bind(wxEVT_TEXT_ENTER, &MainFrame::OnReplace, this);
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
    notebook_->FindNext(event.GetId() == kFindPrevious ? -1 : 1);
}

void MainFrame::OnFindChanged(wxCommandEvent&) {
    notebook_->SetFindQuery(find_input_->GetValue().ToStdString(), CurrentFindOptions());
}

void MainFrame::OnReplace(wxCommandEvent&) {
    notebook_->ReplaceCurrent(replace_input_->GetValue().ToStdString());
}

void MainFrame::OnReplaceAll(wxCommandEvent&) {
    const auto count = notebook_->ReplaceAll(replace_input_->GetValue().ToStdString());
    SetStatusText(count == 0 ? "No matches to replace"
                             : wxString::FromUTF8("Replaced " + std::to_string(count) + " match" +
                                                  (count == 1 ? "" : "es")));
}

void MainFrame::OnCloseFind(wxCommandEvent&) {
    notebook_->ClearFind();
    manager_.GetPane(find_bar_).Hide();
    manager_.Update();
    notebook_->SetFocus();
}

void MainFrame::OnCharHook(wxKeyEvent& event) {
    if (event.GetKeyCode() == WXK_ESCAPE && find_bar_ && manager_.GetPane(find_bar_).IsShown()) {
        wxCommandEvent close;
        OnCloseFind(close);
        return;
    }
    event.Skip();
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
