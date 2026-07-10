#include "ui/main_frame.h"

#include <algorithm>

#include <wx/aboutdlg.h>
#include <wx/display.h>
#include <wx/dnd.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/menu.h>

#include "ui/editor_notebook.h"

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
    : wxFrame(nullptr, wxID_ANY, "Say Count", wxDefaultPosition, wxSize(kDefaultWidth, kDefaultHeight)) {
    SetMinSize(wxSize(400, 300));
    editor_settings_ = settings_.LoadEditor();
    BuildMenus();
    notebook_ = new EditorNotebook(this, editor_settings_);
    SetDropTarget(new ScriptDropTarget(notebook_));
    CreateStatusBar();
    SetStatusText("Ready");
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
}

void MainFrame::BuildMenus() {
    auto* file = new wxMenu();
    file->Append(wxID_NEW, "&New\tCtrl+N", "Create a new tab");
    file->Append(wxID_OPEN, "&Open…\tCtrl+O", "Open one or more Ren'Py scripts");
    file->Append(wxID_SAVE, "&Save\tCtrl+S", "Save the current script");
    file->Append(wxID_SAVEAS, "Save &As…\tCtrl+Shift+S", "Save the current script under a new name");
    file->Append(wxID_CLOSE, "&Close Tab\tCtrl+W", "Close the current tab");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "&Quit\tCtrl+Q", "Quit Say Count");

    auto* edit = new wxMenu();
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
