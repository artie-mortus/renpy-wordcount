#pragma once

#include "app/git_client.h"

#include <functional>
#include <optional>
#include <string>
#include <thread>

#include <wx/dialog.h>

class wxButton;
class wxCloseEvent;
class wxDataViewListCtrl;
class wxStaticText;
class wxThreadEvent;

namespace say_count::ui {

class GitDialog final : public wxDialog {
public:
    GitDialog(wxWindow* parent, std::string project_root,
              std::function<bool()> save_project);
    ~GitDialog() override;

    const std::optional<std::string>& selected_path() const { return selected_path_; }

private:
    enum class WorkKind { Status, OpenLocal, Initialize, SetRemote, Fetch, Pull, CommitPush, Clone };
    struct WorkResult {
        WorkKind kind = WorkKind::Status;
        app::GitSnapshot snapshot;
        std::string selected_path;
        std::string output;
        std::string error;
    };

    void RefreshStatus();
    void RefreshUi();
    void PopulateChanges();
    void SetBusy(bool busy, const wxString& status = {});
    void StartWork(WorkKind kind, std::function<WorkResult()> work,
                   const wxString& status);
    void OnOpenLocal(wxCommandEvent& event);
    void OnClone(wxCommandEvent& event);
    void OnInitialize(wxCommandEvent& event);
    void OnSetRemote(wxCommandEvent& event);
    void OnRefresh(wxCommandEvent& event);
    void OnFetch(wxCommandEvent& event);
    void OnPull(wxCommandEvent& event);
    void OnCommitPush(wxCommandEvent& event);
    void OnCloseButton(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);
    void OnWorkDone(wxThreadEvent& event);

    std::string project_root_;
    app::GitClient client_;
    std::function<bool()> save_project_;
    app::GitSnapshot snapshot_;
    std::optional<std::string> selected_path_;
    wxStaticText* repository_value_ = nullptr;
    wxStaticText* branch_value_ = nullptr;
    wxStaticText* remote_value_ = nullptr;
    wxStaticText* sync_value_ = nullptr;
    wxStaticText* commit_value_ = nullptr;
    wxStaticText* operation_status_ = nullptr;
    wxDataViewListCtrl* changes_ = nullptr;
    wxButton* open_local_ = nullptr;
    wxButton* clone_ = nullptr;
    wxButton* initialize_ = nullptr;
    wxButton* set_remote_ = nullptr;
    wxButton* refresh_ = nullptr;
    wxButton* fetch_ = nullptr;
    wxButton* pull_ = nullptr;
    wxButton* commit_push_ = nullptr;
    wxButton* close_ = nullptr;
    std::thread worker_;
    bool busy_ = false;
};

}  // namespace say_count::ui
