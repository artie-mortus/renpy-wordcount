#include "ui/cloud_save_dialog.h"

#include "core/cloud_save.h"
#include "ui/style.h"

#include <algorithm>
#include <future>
#include <memory>

#include <wx/app.h>
#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/thread.h>
#include <wx/utils.h>

namespace say_count::ui {
namespace {

wxDEFINE_EVENT(kCloudWorkDone, wxThreadEvent);

wxString DisplayTime(std::string value) {
    std::replace(value.begin(), value.end(), 'T', ' ');
    if (!value.empty() && value.back() == 'Z') value.pop_back();
    const auto dot = value.find('.');
    if (dot != std::string::npos) value.erase(dot);
    return wxString::FromUTF8(value + " UTC");
}

wxString DisplaySize(std::uint64_t bytes) {
    if (bytes < 1024) return wxString::Format("%llu B", static_cast<unsigned long long>(bytes));
    if (bytes < 1024 * 1024)
        return wxString::Format("%.1f KB", static_cast<double>(bytes) / 1024.0);
    return wxString::Format("%.1f MB", static_cast<double>(bytes) / (1024.0 * 1024.0));
}

}  // namespace

CloudSaveDialog::CloudSaveDialog(wxWindow* parent, std::string data_directory,
                                 std::string backup_name, std::string backup_contents)
    : wxDialog(parent, wxID_ANY, "Google Drive cloud saves", wxDefaultPosition,
               FromDIP(wxSize(780, 560)), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      client_(std::move(data_directory)), backup_name_(std::move(backup_name)),
      backup_contents_(std::move(backup_contents)) {
    SetMinSize(FromDIP(wxSize(640, 440)));
    const auto& colors = style::Colors();
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* masthead = new wxPanel(this);
    masthead->SetBackgroundColour(colors.ink);
    auto* masthead_layout = new wxBoxSizer(wxHORIZONTAL);
    auto* rule = new wxPanel(masthead, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(4, 48)));
    rule->SetBackgroundColour(colors.mint);
    masthead_layout->Add(rule, 0, wxRIGHT, FromDIP(14));
    auto* title_stack = new wxBoxSizer(wxVERTICAL);
    auto* eyebrow = new wxStaticText(masthead, wxID_ANY, "GOOGLE DRIVE  /  PRIVATE APP STORAGE");
    eyebrow->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    eyebrow->SetForegroundColour(colors.mint);
    auto* title = new wxStaticText(masthead, wxID_ANY, "Cloud saves");
    title->SetFont(style::BodyFont(18, wxFONTWEIGHT_BOLD));
    title->SetForegroundColour(colors.white);
    title_stack->Add(eyebrow);
    title_stack->Add(title, 0, wxTOP, FromDIP(2));
    masthead_layout->Add(title_stack, 1, wxALIGN_CENTER_VERTICAL);
    masthead->SetSizer(masthead_layout);
    root->Add(masthead, 0, wxEXPAND | wxALL, FromDIP(16));

    auto* content = new wxBoxSizer(wxVERTICAL);
    auto* explanation = new wxStaticText(this, wxID_ANY,
        "Save the complete open project to your private Google Drive app folder. "
        "Cloud copies include scripts, targets, and project settings.");
    explanation->Wrap(FromDIP(700));
    explanation->SetForegroundColour(colors.ink_soft);
    content->Add(explanation, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

    auto* setup_row = new wxBoxSizer(wxHORIZONTAL);
    configure_ = new wxButton(this, wxID_ANY, "Import OAuth client...");
    connect_ = new wxButton(this, wxID_ANY, "Connect Google Drive");
    refresh_ = new wxButton(this, wxID_ANY, "Refresh");
    disconnect_ = new wxButton(this, wxID_ANY, "Disconnect");
    connection_status_ = new wxStaticText(this, wxID_ANY, "Not connected");
    connection_status_->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    connection_status_->SetForegroundColour(colors.ink_soft);
    connection_status_->SetMinSize(FromDIP(wxSize(120, 24)));
    setup_row->Add(configure_, 0, wxRIGHT, FromDIP(8));
    setup_row->Add(connect_, 0, wxRIGHT, FromDIP(8));
    setup_row->Add(refresh_, 0, wxRIGHT, FromDIP(8));
    setup_row->Add(connection_status_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(4));
    setup_row->AddStretchSpacer();
    setup_row->Add(disconnect_);
    content->Add(setup_row, 0, wxEXPAND | wxBOTTOM, FromDIP(12));

    list_ = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxDV_ROW_LINES | wxDV_SINGLE);
    list_->AppendTextColumn("Project", wxDATAVIEW_CELL_INERT, FromDIP(300), wxALIGN_LEFT,
                            wxDATAVIEW_COL_RESIZABLE);
    list_->AppendTextColumn("Last saved", wxDATAVIEW_CELL_INERT, FromDIP(220), wxALIGN_LEFT,
                            wxDATAVIEW_COL_RESIZABLE);
    list_->AppendTextColumn("Size", wxDATAVIEW_CELL_INERT, FromDIP(100), wxALIGN_RIGHT,
                            wxDATAVIEW_COL_RESIZABLE);
    content->Add(list_, 1, wxEXPAND);

    operation_status_ = new wxStaticText(this, wxID_ANY, "Cloud saves are ready when you connect.");
    operation_status_->SetFont(style::UtilityFont(8));
    operation_status_->SetForegroundColour(colors.ink_soft);
    content->Add(operation_status_, 0, wxEXPAND | wxTOP | wxBOTTOM, FromDIP(10));

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    save_ = new wxButton(this, wxID_ANY, "Save current project");
    restore_ = new wxButton(this, wxID_ANY, "Restore selected");
    close_ = new wxButton(this, wxID_CANCEL, "Close");
    style::StylePrimaryButton(save_);
    actions->Add(save_, 0, wxRIGHT, FromDIP(8));
    actions->Add(restore_, 0, wxRIGHT, FromDIP(8));
    actions->AddStretchSpacer();
    actions->Add(close_);
    content->Add(actions, 0, wxEXPAND);
    root->Add(content, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, FromDIP(20));
    SetSizer(root);

    configure_->Bind(wxEVT_BUTTON, &CloudSaveDialog::OnConfigure, this);
    connect_->Bind(wxEVT_BUTTON, &CloudSaveDialog::OnConnect, this);
    refresh_->Bind(wxEVT_BUTTON, &CloudSaveDialog::OnRefresh, this);
    save_->Bind(wxEVT_BUTTON, &CloudSaveDialog::OnSave, this);
    restore_->Bind(wxEVT_BUTTON, &CloudSaveDialog::OnRestore, this);
    disconnect_->Bind(wxEVT_BUTTON, &CloudSaveDialog::OnDisconnect, this);
    close_->Bind(wxEVT_BUTTON, &CloudSaveDialog::OnCloseButton, this);
    list_->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &CloudSaveDialog::OnSelection, this);
    Bind(wxEVT_CLOSE_WINDOW, &CloudSaveDialog::OnCloseWindow, this);
    Bind(kCloudWorkDone, &CloudSaveDialog::OnWorkDone, this);
    style::ApplyWorkspaceTheme(this);
    masthead->SetBackgroundColour(colors.ink);
    eyebrow->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    eyebrow->SetForegroundColour(colors.mint);
    title->SetFont(style::BodyFont(18, wxFONTWEIGHT_BOLD));
    title->SetForegroundColour(colors.white);
    connection_status_->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    connected_ = client_.configured() && client_.connected(&connection_error_);
    RefreshConnectionUi();
    if (connected_) CallAfter([this] { RefreshList(); });
}

CloudSaveDialog::~CloudSaveDialog() {
    cancelled_.store(true);
    if (worker_.joinable()) worker_.join();
}

void CloudSaveDialog::RefreshConnectionUi() {
    const bool configured = client_.configured();
    if (!configured) {
        connection_status_->SetLabel("SETUP NEEDED");
        connection_status_->SetForegroundColour(style::Colors().cue);
        operation_status_->SetLabel("Import a Desktop app OAuth JSON file from Google Cloud to begin.");
    } else if (connected_) {
        connection_status_->SetLabel("CONNECTED");
        connection_status_->SetForegroundColour(style::Colors().mint);
    } else {
        connection_status_->SetLabel("NOT CONNECTED");
        connection_status_->SetForegroundColour(wxColour("#AAB5C3"));
        if (!connection_error_.empty()) operation_status_->SetLabel(wxString::FromUTF8(connection_error_));
    }
    configure_->Enable(!busy_);
    connect_->Enable(configured && !connected_ && !busy_);
    refresh_->Enable(connected_ && !busy_);
    const bool can_save = connected_ && !busy_ && !backup_contents_.empty();
    save_->Enable(can_save);
    save_->SetBackgroundColour(can_save ? style::Colors().cue : wxColour("#E2E7EB"));
    restore_->Enable(connected_ && !busy_ && SelectedRow() >= 0);
    disconnect_->Enable(connected_ && !busy_);
    connection_status_->Show();
    connection_status_->GetParent()->Layout();
    Layout();
    save_->Refresh();
}

void CloudSaveDialog::SetBusy(bool busy, const wxString& status) {
    busy_ = busy;
    if (!status.empty()) operation_status_->SetLabel(status);
    close_->SetLabel(busy ? "Cancel" : "Close");
    list_->Enable(!busy);
    RefreshConnectionUi();
}

void CloudSaveDialog::StartWork(WorkKind kind, std::function<WorkResult()> work,
                                const wxString& status) {
    if (busy_) return;
    if (worker_.joinable()) worker_.join();
    cancelled_.store(false);
    SetBusy(true, status);
    worker_ = std::thread([this, kind, work = std::move(work)]() mutable {
        WorkResult result = work();
        result.kind = kind;
        auto* event = new wxThreadEvent(kCloudWorkDone);
        event->SetPayload(std::make_shared<WorkResult>(std::move(result)));
        wxQueueEvent(this, event);
    });
}

void CloudSaveDialog::RefreshList() {
    StartWork(WorkKind::List, [this] {
        WorkResult result;
        auto listed = client_.ListBackups();
        if (listed) result.entries = std::move(listed.value);
        else result.error = std::move(listed.error);
        return result;
    }, "Reading private app backups from Google Drive...");
}

void CloudSaveDialog::PopulateList(std::vector<CloudSaveEntry> entries) {
    entries_ = std::move(entries);
    list_->DeleteAllItems();
    for (const auto& entry : entries_) {
        wxVector<wxVariant> row;
        row.push_back(wxVariant(wxString::FromUTF8(cloud_save_display_name(entry.name))));
        row.push_back(wxVariant(DisplayTime(entry.modified_time)));
        row.push_back(wxVariant(DisplaySize(entry.size)));
        list_->AppendItem(row);
    }
    operation_status_->SetLabel(entries_.empty()
        ? "No cloud saves yet. Save the current project to create the first one."
        : wxString::Format("%zu cloud save%s", entries_.size(), entries_.size() == 1 ? "" : "s"));
}

void CloudSaveDialog::OnConfigure(wxCommandEvent&) {
    wxFileDialog dialog(this, "Import Google desktop OAuth client", {}, {},
        "Google OAuth client JSON (*.json)|*.json", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (dialog.ShowModal() != wxID_OK) return;
    wxFile file(dialog.GetPath());
    wxString contents;
    if (!file.IsOpened() || !file.ReadAll(&contents, wxConvUTF8)) {
        wxMessageBox("Could not read the OAuth client file.", "Google Drive setup failed",
                     wxOK | wxICON_ERROR, this);
        return;
    }
    std::string error;
    if (!client_.ImportClientConfiguration(contents.ToStdString(wxConvUTF8), &error)) {
        wxMessageBox(wxString::FromUTF8(error), "Google Drive setup failed", wxOK | wxICON_ERROR, this);
        return;
    }
    connection_error_.clear();
    connected_ = client_.connected(&connection_error_);
    operation_status_->SetLabel("OAuth client installed. Connect Google Drive to authorize Say Count.");
    RefreshConnectionUi();
}

void CloudSaveDialog::OnConnect(wxCommandEvent&) {
    StartWork(WorkKind::Connect, [this] {
        WorkResult result;
        auto connected = client_.Connect([](const std::string& url) {
            auto promise = std::make_shared<std::promise<bool>>();
            auto future = promise->get_future();
            wxTheApp->CallAfter([promise, url] {
                promise->set_value(wxLaunchDefaultBrowser(wxString::FromUTF8(url)));
            });
            return future.get();
        }, &cancelled_);
        if (!connected) result.error = std::move(connected.error);
        return result;
    }, "Waiting for Google sign-in in your browser...");
}

void CloudSaveDialog::OnRefresh(wxCommandEvent&) { RefreshList(); }

void CloudSaveDialog::OnSave(wxCommandEvent&) {
    std::optional<std::string> existing;
    const auto found = std::find_if(entries_.begin(), entries_.end(), [this](const auto& entry) {
        return entry.name == backup_name_;
    });
    if (found != entries_.end()) existing = found->id;
    StartWork(WorkKind::Save, [this, existing] {
        WorkResult result;
        auto saved = client_.UploadBackup(backup_name_, backup_contents_, existing);
        if (saved) result.entry = std::move(saved.value);
        else result.error = std::move(saved.error);
        return result;
    }, existing ? "Updating this project's cloud save..." : "Creating this project's cloud save...");
}

long CloudSaveDialog::SelectedRow() const {
    if (!list_) return -1;
    const wxDataViewItem selection = list_->GetSelection();
    return selection.IsOk() ? list_->ItemToRow(selection) : -1;
}

void CloudSaveDialog::OnRestore(wxCommandEvent&) {
    const long row = SelectedRow();
    if (row < 0 || static_cast<std::size_t>(row) >= entries_.size()) return;
    const CloudSaveEntry selected = entries_[static_cast<std::size_t>(row)];
    StartWork(WorkKind::Restore, [this, selected] {
        WorkResult result;
        result.name = cloud_save_display_name(selected.name);
        auto downloaded = client_.DownloadBackup(selected.id);
        if (downloaded) result.contents = std::move(downloaded.value);
        else result.error = std::move(downloaded.error);
        return result;
    }, "Downloading the selected cloud save...");
}

void CloudSaveDialog::OnDisconnect(wxCommandEvent&) {
    StartWork(WorkKind::Disconnect, [this] {
        WorkResult result;
        auto disconnected = client_.Disconnect();
        if (!disconnected) result.error = std::move(disconnected.error);
        return result;
    }, "Disconnecting Google Drive...");
}

void CloudSaveDialog::OnSelection(wxDataViewEvent&) { RefreshConnectionUi(); }

void CloudSaveDialog::OnCloseButton(wxCommandEvent&) {
    if (busy_) {
        cancelled_.store(true);
        operation_status_->SetLabel("Cancelling...");
        close_->Enable(false);
        return;
    }
    EndModal(wxID_CANCEL);
}

void CloudSaveDialog::OnCloseWindow(wxCloseEvent& event) {
    if (busy_) {
        cancelled_.store(true);
        operation_status_->SetLabel("Cancelling...");
        close_->Enable(false);
        event.Veto();
        return;
    }
    EndModal(wxID_CANCEL);
}

void CloudSaveDialog::OnWorkDone(wxThreadEvent& event) {
    if (worker_.joinable()) worker_.join();
    const auto result = event.GetPayload<std::shared_ptr<WorkResult>>();
    SetBusy(false);
    close_->Enable(true);
    if (!result->error.empty()) {
        operation_status_->SetLabel(wxString::FromUTF8(result->error));
        operation_status_->SetForegroundColour(wxColour("#A33A3A"));
        wxMessageBox(wxString::FromUTF8(result->error), "Google Drive", wxOK | wxICON_ERROR, this);
        RefreshConnectionUi();
        return;
    }
    operation_status_->SetForegroundColour(style::Colors().ink_soft);
    if (result->kind == WorkKind::Connect) {
        connected_ = true;
        connection_error_.clear();
        operation_status_->SetLabel("Google Drive connected.");
        RefreshConnectionUi();
        RefreshList();
    } else if (result->kind == WorkKind::List) {
        PopulateList(std::move(result->entries));
        RefreshConnectionUi();
    } else if (result->kind == WorkKind::Save) {
        operation_status_->SetLabel("Cloud save updated.");
        RefreshList();
    } else if (result->kind == WorkKind::Restore) {
        restored_bundle_ = std::move(result->contents);
        restored_name_ = std::move(result->name);
        EndModal(wxID_OK);
    } else if (result->kind == WorkKind::Disconnect) {
        connected_ = false;
        connection_error_.clear();
        entries_.clear();
        list_->DeleteAllItems();
        operation_status_->SetLabel("Google Drive disconnected. Cloud copies remain in Drive.");
        RefreshConnectionUi();
    }
}

}  // namespace say_count::ui
