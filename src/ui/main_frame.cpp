#include "ui/main_frame.h"

#include <wx/aboutdlg.h>
#include <wx/display.h>
#include <wx/menu.h>

#include "ui/editor_notebook.h"

namespace say_count::ui {
namespace {

constexpr int kDefaultWidth = 1100;
constexpr int kDefaultHeight = 750;

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
    BuildMenus();
    notebook_ = new EditorNotebook(this);
    CreateStatusBar();
    SetStatusText("Ready");
    RestoreWindow();

    Bind(wxEVT_CLOSE_WINDOW, &MainFrame::OnClose, this);
    Bind(wxEVT_MENU, &MainFrame::OnQuit, this, wxID_EXIT);
    Bind(wxEVT_MENU, &MainFrame::OnNewTab, this, wxID_NEW);
    Bind(wxEVT_MENU, &MainFrame::OnCloseTab, this, wxID_CLOSE);
    Bind(wxEVT_MENU, &MainFrame::OnAbout, this, wxID_ABOUT);
}

void MainFrame::BuildMenus() {
    auto* file = new wxMenu();
    file->Append(wxID_NEW, "&New\tCtrl+N", "Create a new tab");
    file->Append(wxID_CLOSE, "&Close Tab\tCtrl+W", "Close the current tab");
    file->AppendSeparator();
    file->Append(wxID_EXIT, "&Quit\tCtrl+Q", "Quit Say Count");

    auto* edit = new wxMenu();
    auto* view = new wxMenu();

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
