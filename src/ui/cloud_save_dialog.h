#pragma once

#include "app/google_drive.h"

#include <atomic>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <wx/dialog.h>

class wxButton;
class wxCloseEvent;
class wxDataViewEvent;
class wxDataViewListCtrl;
class wxStaticText;
class wxThreadEvent;

namespace say_count::ui {

class CloudSaveDialog final : public wxDialog {
public:
    CloudSaveDialog(wxWindow* parent, std::string data_directory, std::string backup_name,
                    std::string backup_contents);
    ~CloudSaveDialog() override;

    const std::optional<std::string>& restored_bundle() const { return restored_bundle_; }
    const std::string& restored_name() const { return restored_name_; }

private:
    enum class WorkKind { Connect, List, Save, Restore, Disconnect };
    struct WorkResult {
        WorkKind kind = WorkKind::List;
        std::vector<CloudSaveEntry> entries;
        std::optional<CloudSaveEntry> entry;
        std::string contents;
        std::string name;
        std::string error;
    };

    void RefreshConnectionUi();
    void RefreshList();
    void PopulateList(std::vector<CloudSaveEntry> entries);
    void SetBusy(bool busy, const wxString& status = {});
    void StartWork(WorkKind kind, std::function<WorkResult()> work, const wxString& status);
    void OnConfigure(wxCommandEvent& event);
    void OnConnect(wxCommandEvent& event);
    void OnRefresh(wxCommandEvent& event);
    void OnSave(wxCommandEvent& event);
    void OnRestore(wxCommandEvent& event);
    void OnDisconnect(wxCommandEvent& event);
    void OnSelection(wxDataViewEvent& event);
    void OnCloseButton(wxCommandEvent& event);
    void OnCloseWindow(wxCloseEvent& event);
    void OnWorkDone(wxThreadEvent& event);
    long SelectedRow() const;

    app::GoogleDriveClient client_;
    std::string backup_name_;
    std::string backup_contents_;
    std::vector<CloudSaveEntry> entries_;
    std::optional<std::string> restored_bundle_;
    std::string restored_name_;
    wxStaticText* connection_status_ = nullptr;
    wxStaticText* operation_status_ = nullptr;
    wxDataViewListCtrl* list_ = nullptr;
    wxButton* configure_ = nullptr;
    wxButton* connect_ = nullptr;
    wxButton* refresh_ = nullptr;
    wxButton* save_ = nullptr;
    wxButton* restore_ = nullptr;
    wxButton* disconnect_ = nullptr;
    wxButton* close_ = nullptr;
    std::thread worker_;
    std::atomic_bool cancelled_{false};
    bool connected_ = false;
    std::string connection_error_;
    bool busy_ = false;
};

}  // namespace say_count::ui
