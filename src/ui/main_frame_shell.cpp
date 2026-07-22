#include "ui/main_frame.h"

#include <algorithm>
#include <filesystem>

#include <wx/infobar.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/stattext.h>

#include "ui/command_button.h"
#include "ui/command_model.h"
#include "ui/editor_notebook.h"
#include "ui/style.h"

namespace say_count::ui {

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
    save_button_ = make_action(CommandFor(wxID_SAVE).label);
    write_button_ = make_action("Write naturally");
    history_button_ = make_action("Version history");
    problems_button_ = make_action(CommandFor(kShowDiagnostics).label);
    run_button_ = make_action("Preview line 1", true);
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
    run_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event) {
        if (CanPreviewFromCaret()) OnWarpRenpy(event);
        else OnRunRenpy(event);
    });
    command_bar_->SetSizer(layout);
    RefreshCommandBarState();
    RefreshCueSummary();
}

void MainFrame::RefreshCommandBarState() {
    if (!command_bar_) return;
    ShellContext context{project_.has_value(), current_document_dirty_,
        !current_document_path_.empty(), problem_count_, fixable_problem_count_,
        renpy_sdk_.has_value(), renpy_process_ != nullptr};
    context.can_preview_from_line = CanPreviewFromCaret();
    context.current_line = current_document_line_;
    const auto state = DeriveCommandBarState(context);
    const int width = GetClientSize().x;
    const bool compact = width < FromDIP(1080);
    const bool narrow = width < FromDIP(900);
    const bool very_narrow = width < FromDIP(720);
    if (cue_summary_) cue_summary_->Show(!narrow);
    if (workspace_name_) workspace_name_->Show(!very_narrow);
    if (open_game_button_) open_game_button_->Show(state.show_open_game);
    if (history_button_) history_button_->Show(state.show_history && !compact);
    if (problems_button_) {
        problems_button_->Show(state.show_problems);
        auto* button = static_cast<CommandButton*>(problems_button_);
        button->SetCommandLabel(wxString::FromUTF8(state.problem_label));
    }
    if (write_button_) write_button_->Show(state.show_write);
    if (run_button_) {
        run_button_->Show(state.show_preview);
        run_button_->Enable(state.enable_preview);
        static_cast<CommandButton*>(run_button_)->SetCommandLabel(
            wxString::FromUTF8(state.preview_label));
        run_button_->SetToolTip(state.preview_from_line
            ? "Save all scripts and preview from the current line (F7)"
            : "Save all scripts and preview the game from the beginning (F6)");
    }
    if (save_button_) {
        save_button_->Show(!compact || state.enable_save);
        save_button_->Enable(state.enable_save);
    }
    command_bar_->Layout();
}

bool MainFrame::CanPreviewFromCaret() const {
    if (!project_ || !renpy_sdk_ || !renpy_capabilities(renpy_sdk_->version).warp ||
        current_document_path_.empty()) return false;
    std::error_code ec;
    const auto relative = std::filesystem::relative(
        current_document_path_.ToStdString(wxConvUTF8), project_->scripts_root, ec);
    return !ec && !relative.empty() && *relative.begin() != "..";
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

void MainFrame::ShowNotice(const wxString& message, int flags) {
    if (!notification_bar_) {
        SetStatusText(message);
        return;
    }
    notification_bar_->ShowMessage(message, flags);
    manager_.GetPane("notification-bar").Show(true);
    manager_.Update();
}

void MainFrame::HideNotice() {
    if (!notification_bar_) return;
    notification_bar_->Dismiss();
    manager_.GetPane("notification-bar").Hide();
    manager_.Update();
    if (notebook_) notebook_->SetFocus();
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
    file->Append(kShowHome, "&Home...\tCtrl+Shift+H", CommandFor(kShowHome).help);
    const auto& open_game = CommandFor(kConnectProject);
    file->Append(kConnectProject, open_game.label, open_game.help);
    recent_projects_menu_ = new wxMenu();
    file->AppendSubMenu(recent_projects_menu_, "Recent Projects");
    file->Append(kReviewConflicts, "Review External &Conflicts...");
    const auto& save_version = CommandFor(kSnapshotNow);
    const auto& version_history = CommandFor(kManageSnapshots);
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
    insert->Append(kShowAssets, "From the Story Library...", CommandFor(kShowAssets).help);
    edit->AppendSubMenu(insert, "&Insert Into Story");
    edit->AppendCheckItem(kToggleNvimMotions, "&Vim Motions (Built-in)",
                          "Use built-in modal editing without an external process");
    edit->Check(kToggleNvimMotions, editor_settings_.nvim_motions);
    const auto& natural_writing = CommandFor(kWriteManuscript);
    edit->Append(kWriterDraft, CommandFor(kWriterDraft).label,
                 CommandFor(kWriterDraft).help);
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
    view->AppendCheckItem(kShowProjectNavigator, "Script index");
    view->AppendCheckItem(kShowSpeakerStats, "Speaker statistics panel");
    view->AppendCheckItem(kShowBuildScene, "Build Scene shelf");
    view->Check(kShowBuildScene, true);
    view->AppendCheckItem(kShowStoryLibrary, "Story library");
    view->AppendCheckItem(kShowDiagnostics, CommandFor(kShowDiagnostics).label);
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
    renpy_menu_->Append(kWarpRenpy, "Preview from Caret\tF7");
    renpy_menu_->Append(kDirectorRenpy, "Interactive Director");
    renpy_menu_->Append(kRuntimePresets, "Runtime State Presets...");
    renpy_menu_->Append(kRunRenpyLint, CommandFor(kRunRenpyLint).label);
    renpy_menu_->Append(kGenerateTranslations, "Generate Translations...");
    renpy_menu_->Append(kExportDialogue, "Export Dialogue...");
    renpy_menu_->Append(kShowAssets, "Story Library...");
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


}  // namespace say_count::ui
