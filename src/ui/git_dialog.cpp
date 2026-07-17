#include "ui/git_dialog.h"

#include "ui/style.h"

#include <filesystem>
#include <memory>

#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/dirdlg.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>
#include <wx/thread.h>

namespace say_count::ui {
namespace {

wxDEFINE_EVENT(kGitWorkDone, wxThreadEvent);

wxStaticText* AddFact(wxWindow* parent, wxBoxSizer* layout, const wxString& label) {
    auto* stack = new wxBoxSizer(wxVERTICAL);
    auto* caption = new wxStaticText(parent, wxID_ANY, label);
    caption->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    caption->SetForegroundColour(style::Colors().ink_soft);
    auto* value = new wxStaticText(parent, wxID_ANY, "—");
    value->SetFont(style::BodyFont(10, wxFONTWEIGHT_BOLD));
    value->SetForegroundColour(style::Colors().ink);
    value->SetMinSize(parent->FromDIP(wxSize(130, 24)));
    stack->Add(caption);
    stack->Add(value, 0, wxTOP, parent->FromDIP(3));
    layout->Add(stack, 1, wxRIGHT, parent->FromDIP(16));
    return value;
}

}  // namespace

GitDialog::GitDialog(wxWindow* parent, std::string project_root,
                     std::function<bool()> save_project)
    : wxDialog(parent, wxID_ANY, "Git repository", wxDefaultPosition,
               FromDIP(wxSize(820, 610)), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      project_root_(std::move(project_root)), client_(project_root_),
      save_project_(std::move(save_project)) {
    SetMinSize(FromDIP(wxSize(680, 500)));
    const auto& colors = style::Colors();
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* masthead = new wxPanel(this);
    masthead->SetBackgroundColour(colors.ink);
    auto* masthead_layout = new wxBoxSizer(wxHORIZONTAL);
    auto* rule = new wxPanel(masthead, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(4, 48)));
    rule->SetBackgroundColour(colors.mint);
    masthead_layout->Add(rule, 0, wxRIGHT, FromDIP(14));
    auto* title_stack = new wxBoxSizer(wxVERTICAL);
    auto* eyebrow = new wxStaticText(masthead, wxID_ANY, "GIT  /  ANY REMOTE");
    eyebrow->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    eyebrow->SetForegroundColour(colors.mint);
    auto* title = new wxStaticText(masthead, wxID_ANY, "Repository desk");
    title->SetFont(style::BodyFont(18, wxFONTWEIGHT_BOLD));
    title->SetForegroundColour(colors.white);
    title_stack->Add(eyebrow);
    title_stack->Add(title, 0, wxTOP, FromDIP(2));
    masthead_layout->Add(title_stack, 1, wxALIGN_CENTER_VERTICAL);
    masthead->SetSizer(masthead_layout);
    root->Add(masthead, 0, wxEXPAND | wxALL, FromDIP(16));

    auto* content = new wxBoxSizer(wxVERTICAL);
    auto* explanation = new wxStaticText(this, wxID_ANY,
        "Use any Git host or server. Private access comes from your SSH agent or system Git "
        "credential helper; Say Count never stores repository passwords or tokens.");
    explanation->Wrap(FromDIP(740));
    explanation->SetForegroundColour(colors.ink_soft);
    content->Add(explanation, 0, wxEXPAND | wxBOTTOM, FromDIP(14));

    auto* setup = new wxBoxSizer(wxHORIZONTAL);
    open_local_ = new wxButton(this, wxID_ANY, "Open local repository...");
    clone_ = new wxButton(this, wxID_ANY, "Clone repository...");
    initialize_ = new wxButton(this, wxID_ANY, "Initialize");
    set_remote_ = new wxButton(this, wxID_ANY, "Set origin...");
    refresh_ = new wxButton(this, wxID_ANY, "Refresh");
    setup->Add(open_local_, 0, wxRIGHT, FromDIP(8));
    setup->Add(clone_, 0, wxRIGHT, FromDIP(8));
    setup->Add(initialize_, 0, wxRIGHT, FromDIP(8));
    setup->Add(set_remote_, 0, wxRIGHT, FromDIP(8));
    setup->AddStretchSpacer();
    setup->Add(refresh_);
    content->Add(setup, 0, wxEXPAND | wxBOTTOM, FromDIP(16));

    auto* facts_panel = new wxPanel(this);
    facts_panel->SetBackgroundColour(colors.paper);
    auto* facts = new wxBoxSizer(wxHORIZONTAL);
    repository_value_ = AddFact(facts_panel, facts, "REPOSITORY");
    branch_value_ = AddFact(facts_panel, facts, "BRANCH");
    remote_value_ = AddFact(facts_panel, facts, "REMOTE");
    sync_value_ = AddFact(facts_panel, facts, "SYNC");
    facts_panel->SetSizer(facts);
    content->Add(facts_panel, 0, wxEXPAND | wxBOTTOM, FromDIP(10));

    commit_value_ = new wxStaticText(this, wxID_ANY, "No commits yet");
    commit_value_->SetFont(style::UtilityFont(8));
    commit_value_->SetForegroundColour(colors.ink_soft);
    content->Add(commit_value_, 0, wxEXPAND | wxBOTTOM, FromDIP(8));

    changes_ = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxDV_ROW_LINES | wxDV_SINGLE);
    changes_->AppendTextColumn("State", wxDATAVIEW_CELL_INERT, FromDIP(160), wxALIGN_LEFT,
                               wxDATAVIEW_COL_RESIZABLE);
    changes_->AppendTextColumn("Repository file", wxDATAVIEW_CELL_INERT, FromDIP(560), wxALIGN_LEFT,
                               wxDATAVIEW_COL_RESIZABLE);
    content->Add(changes_, 1, wxEXPAND);

    operation_status_ = new wxStaticText(this, wxID_ANY, "Reading repository status...");
    operation_status_->SetFont(style::UtilityFont(8));
    operation_status_->SetForegroundColour(colors.ink_soft);
    content->Add(operation_status_, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(10));

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    fetch_ = new wxButton(this, wxID_ANY, "Fetch");
    pull_ = new wxButton(this, wxID_ANY, "Pull changes");
    commit_push_ = new wxButton(this, wxID_ANY, "Commit & push");
    close_ = new wxButton(this, wxID_CANCEL, "Close");
    style::StylePrimaryButton(commit_push_);
    actions->Add(fetch_, 0, wxRIGHT, FromDIP(8));
    actions->Add(pull_, 0, wxRIGHT, FromDIP(8));
    actions->Add(commit_push_, 0, wxRIGHT, FromDIP(8));
    actions->AddStretchSpacer();
    actions->Add(close_);
    content->Add(actions, 0, wxEXPAND);
    root->Add(content, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    SetSizer(root);

    open_local_->Bind(wxEVT_BUTTON, &GitDialog::OnOpenLocal, this);
    clone_->Bind(wxEVT_BUTTON, &GitDialog::OnClone, this);
    initialize_->Bind(wxEVT_BUTTON, &GitDialog::OnInitialize, this);
    set_remote_->Bind(wxEVT_BUTTON, &GitDialog::OnSetRemote, this);
    refresh_->Bind(wxEVT_BUTTON, &GitDialog::OnRefresh, this);
    fetch_->Bind(wxEVT_BUTTON, &GitDialog::OnFetch, this);
    pull_->Bind(wxEVT_BUTTON, &GitDialog::OnPull, this);
    commit_push_->Bind(wxEVT_BUTTON, &GitDialog::OnCommitPush, this);
    close_->Bind(wxEVT_BUTTON, &GitDialog::OnCloseButton, this);
    Bind(wxEVT_CLOSE_WINDOW, &GitDialog::OnCloseWindow, this);
    Bind(kGitWorkDone, &GitDialog::OnWorkDone, this);
    style::ApplyWorkspaceTheme(this);
    masthead->SetBackgroundColour(colors.ink);
    eyebrow->SetForegroundColour(colors.mint);
    title->SetForegroundColour(colors.white);
    facts_panel->SetBackgroundColour(colors.paper);
    RefreshUi();
    if (!project_root_.empty()) CallAfter([this] { RefreshStatus(); });
}

GitDialog::~GitDialog() {
    if (worker_.joinable()) worker_.join();
}

void GitDialog::SetBusy(bool busy, const wxString& status) {
    busy_ = busy;
    if (!status.empty()) operation_status_->SetLabel(status);
    close_->Enable(!busy);
    changes_->Enable(!busy);
    RefreshUi();
}

void GitDialog::StartWork(WorkKind kind, std::function<WorkResult()> work,
                          const wxString& status) {
    if (busy_) return;
    if (worker_.joinable()) worker_.join();
    SetBusy(true, status);
    worker_ = std::thread([this, kind, work = std::move(work)]() mutable {
        WorkResult result = work();
        result.kind = kind;
        auto* event = new wxThreadEvent(kGitWorkDone);
        event->SetPayload(std::make_shared<WorkResult>(std::move(result)));
        wxQueueEvent(this, event);
    });
}

void GitDialog::RefreshStatus() {
    StartWork(WorkKind::Status, [this] {
        WorkResult result;
        auto status = client_.Status();
        if (status) result.snapshot = std::move(status.value);
        else result.error = std::move(status.error);
        return result;
    }, "Reading branch, remote, and working tree...");
}

void GitDialog::PopulateChanges() {
    changes_->DeleteAllItems();
    for (const auto& change : snapshot_.status.changes) {
        wxVector<wxVariant> row;
        row.push_back(wxVariant(wxString::FromUTF8(change.state)));
        row.push_back(wxVariant(wxString::FromUTF8(change.path)));
        changes_->AppendItem(row);
    }
}

void GitDialog::RefreshUi() {
    const bool has_project = !project_root_.empty();
    const bool repository = has_project && snapshot_.is_repository;
    const bool remote = repository && !snapshot_.remote_name.empty();
    const bool clean = snapshot_.status.changes.empty();
    open_local_->Enable(!busy_);
    clone_->Enable(!busy_);
    initialize_->Enable(has_project && !repository && !busy_);
    set_remote_->Enable(repository && !busy_);
    refresh_->Enable(has_project && !busy_);
    fetch_->Enable(remote && !busy_);
    pull_->Enable(remote && clean && !snapshot_.status.upstream.empty() && !busy_);
    commit_push_->Enable(remote && !busy_);
    commit_push_->SetLabel(clean ? "Push" : "Commit & push");
    commit_push_->SetBackgroundColour(remote && !busy_ ? style::Colors().cue : wxColour("#E2E7EB"));

    if (!has_project) {
        repository_value_->SetLabel("No project connected");
        branch_value_->SetLabel("—");
        remote_value_->SetLabel("—");
        sync_value_->SetLabel("Open or clone");
        commit_value_->SetLabel("Open a repository already on this PC, or clone one into a new folder.");
        if (!busy_) operation_status_->SetLabel("Choose Open local repository or Clone repository to begin.");
    } else if (!repository) {
        repository_value_->SetLabel("Not initialized");
        branch_value_->SetLabel("—");
        remote_value_->SetLabel("—");
        sync_value_->SetLabel("Local only");
        commit_value_->SetLabel(wxString::FromUTF8(project_root_));
        if (!busy_) operation_status_->SetLabel("Initialize this project, or clone another repository.");
    } else {
        repository_value_->SetLabel(wxString::FromUTF8(snapshot_.repository_root));
        branch_value_->SetLabel(wxString::FromUTF8(snapshot_.status.branch.empty()
            ? "Detached HEAD" : snapshot_.status.branch));
        remote_value_->SetLabel(remote
            ? wxString::FromUTF8(snapshot_.remote_name + "  ·  " + snapshot_.remote_url)
            : "Not set");
        if (snapshot_.status.upstream.empty()) sync_value_->SetLabel("Not tracking");
        else if (snapshot_.status.ahead == 0 && snapshot_.status.behind == 0) sync_value_->SetLabel("Up to date");
        else sync_value_->SetLabel(wxString::Format("%zu ahead  ·  %zu behind",
            snapshot_.status.ahead, snapshot_.status.behind));
        commit_value_->SetLabel(snapshot_.last_commit.empty()
            ? "No commits yet" : "LATEST  /  " + wxString::FromUTF8(snapshot_.last_commit));
    }
    Layout();
    commit_push_->Refresh();
}

void GitDialog::OnOpenLocal(wxCommandEvent&) {
    wxDirDialog dialog(this, "Choose a local Git repository or its project folder",
                       wxEmptyString, wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    const std::string path = dialog.GetPath().ToStdString(wxConvUTF8);
    StartWork(WorkKind::OpenLocal, [path] {
        WorkResult result;
        auto status = app::GitClient(path).Status();
        if (!status) result.error = std::move(status.error);
        else if (!status.value.is_repository)
            result.error = "The selected folder is not inside a Git repository.";
        else
            result.selected_path = path;
        return result;
    }, "Checking the local repository and its configured remote...");
}

void GitDialog::OnClone(wxCommandEvent&) {
    wxTextEntryDialog remote_dialog(this,
        "Enter any Git URL or repository path. SSH is recommended for private repositories.",
        "Clone Git repository", "");
    if (remote_dialog.ShowModal() != wxID_OK) return;
    const std::string remote = remote_dialog.GetValue().ToStdString(wxConvUTF8);
    std::string error;
    if (!valid_git_remote(remote, &error)) {
        wxMessageBox(wxString::FromUTF8(error), "Invalid Git remote", wxOK | wxICON_ERROR, this);
        return;
    }
    wxDirDialog parent_dialog(this, "Choose the parent folder for the clone", wxEmptyString,
                              wxDD_DEFAULT_STYLE | wxDD_DIR_MUST_EXIST);
    if (parent_dialog.ShowModal() != wxID_OK) return;
    const std::string suggested = git_repository_name(remote);
    wxTextEntryDialog name_dialog(this, "Name the new repository folder:",
                                  "Clone destination", wxString::FromUTF8(suggested));
    if (name_dialog.ShowModal() != wxID_OK) return;
    const std::string name = name_dialog.GetValue().ToStdString(wxConvUTF8);
    if (name.empty() || name == "." || name == ".." || name.find('/') != std::string::npos ||
        name.find('\\') != std::string::npos) {
        wxMessageBox("Enter one folder name without slashes.", "Invalid clone destination",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    const std::string destination = (std::filesystem::path(
        parent_dialog.GetPath().ToStdString(wxConvUTF8)) / name).string();
    StartWork(WorkKind::Clone, [remote, destination] {
        WorkResult result;
        auto cloned = app::GitClient::Clone(remote, destination);
        if (cloned) { result.selected_path = std::move(cloned.value); result.output = std::move(cloned.output); }
        else result.error = std::move(cloned.error);
        return result;
    }, "Cloning repository. Your Git credential helper may ask you to sign in...");
}

void GitDialog::OnInitialize(wxCommandEvent&) {
    StartWork(WorkKind::Initialize, [this] {
        WorkResult result;
        auto initialized = client_.Initialize();
        if (initialized) result.output = std::move(initialized.output);
        else result.error = std::move(initialized.error);
        return result;
    }, "Initializing Git repository...");
}

void GitDialog::OnSetRemote(wxCommandEvent&) {
    wxTextEntryDialog dialog(this,
        "Enter an HTTPS, SSH, file, or local Git repository address:",
        "Set origin remote", wxString::FromUTF8(snapshot_.remote_url));
    if (dialog.ShowModal() != wxID_OK) return;
    const std::string remote = dialog.GetValue().ToStdString(wxConvUTF8);
    std::string error;
    if (!valid_git_remote(remote, &error)) {
        wxMessageBox(wxString::FromUTF8(error), "Invalid Git remote", wxOK | wxICON_ERROR, this);
        return;
    }
    StartWork(WorkKind::SetRemote, [this, remote] {
        WorkResult result;
        auto configured = client_.SetRemote(remote);
        if (configured) result.output = std::move(configured.output);
        else result.error = std::move(configured.error);
        return result;
    }, "Updating origin remote...");
}

void GitDialog::OnRefresh(wxCommandEvent&) { RefreshStatus(); }

void GitDialog::OnFetch(wxCommandEvent&) {
    StartWork(WorkKind::Fetch, [this] {
        WorkResult result;
        auto fetched = client_.Fetch();
        if (fetched) result.output = std::move(fetched.output);
        else result.error = std::move(fetched.error);
        return result;
    }, "Fetching from every configured remote...");
}

void GitDialog::OnPull(wxCommandEvent&) {
    if (save_project_ && !save_project_()) return;
    StartWork(WorkKind::Pull, [this] {
        WorkResult result;
        auto pulled = client_.PullFastForward();
        if (pulled) result.output = std::move(pulled.output);
        else result.error = std::move(pulled.error);
        return result;
    }, "Pulling a fast-forward update...");
}

void GitDialog::OnCommitPush(wxCommandEvent&) {
    if (save_project_ && !save_project_()) return;
    std::string message;
    if (!snapshot_.status.changes.empty()) {
        wxTextEntryDialog dialog(this, "Describe this repository revision:",
                                 "Commit all repository changes", "Update story project");
        if (dialog.ShowModal() != wxID_OK) return;
        message = dialog.GetValue().ToStdString(wxConvUTF8);
        if (message.empty()) {
            wxMessageBox("Enter a commit message.", "Commit message required",
                         wxOK | wxICON_ERROR, this);
            return;
        }
    }
    StartWork(WorkKind::CommitPush, [this, message] {
        WorkResult result;
        auto pushed = client_.CommitAndPush(message);
        if (pushed) result.output = std::move(pushed.output);
        else result.error = std::move(pushed.error);
        return result;
    }, snapshot_.status.changes.empty() ? "Pushing current branch..." : "Committing and pushing all repository changes...");
}

void GitDialog::OnCloseButton(wxCommandEvent&) {
    if (!busy_) EndModal(wxID_CANCEL);
}

void GitDialog::OnCloseWindow(wxCloseEvent& event) {
    if (busy_) { event.Veto(); return; }
    EndModal(wxID_CANCEL);
}

void GitDialog::OnWorkDone(wxThreadEvent& event) {
    if (worker_.joinable()) worker_.join();
    const auto result = event.GetPayload<std::shared_ptr<WorkResult>>();
    SetBusy(false);
    if (!result->error.empty()) {
        operation_status_->SetLabel(wxString::FromUTF8(result->error));
        operation_status_->SetForegroundColour(wxColour("#A33A3A"));
        wxMessageBox(wxString::FromUTF8(result->error), "Git operation failed",
                     wxOK | wxICON_ERROR, this);
        RefreshUi();
        return;
    }
    operation_status_->SetForegroundColour(style::Colors().ink_soft);
    if (result->kind == WorkKind::Status) {
        snapshot_ = std::move(result->snapshot);
        PopulateChanges();
        operation_status_->SetLabel(snapshot_.status.changes.empty()
            ? "Working tree clean."
            : wxString::Format("%zu changed repository file%s.", snapshot_.status.changes.size(),
                               snapshot_.status.changes.size() == 1 ? "" : "s"));
        RefreshUi();
    } else if (result->kind == WorkKind::Clone || result->kind == WorkKind::OpenLocal) {
        selected_path_ = std::move(result->selected_path);
        EndModal(wxID_OK);
    } else {
        switch (result->kind) {
            case WorkKind::Initialize: operation_status_->SetLabel("Repository initialized."); break;
            case WorkKind::SetRemote: operation_status_->SetLabel("Origin remote updated."); break;
            case WorkKind::Fetch: operation_status_->SetLabel("Remote references fetched."); break;
            case WorkKind::Pull: operation_status_->SetLabel("Fast-forward update pulled."); break;
            case WorkKind::CommitPush: operation_status_->SetLabel("Current branch pushed."); break;
            default: break;
        }
        RefreshStatus();
    }
}

}  // namespace say_count::ui
