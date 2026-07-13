#pragma once

#include <wx/dialog.h>

#include "core/snapshots.h"

#include <optional>

class wxListBox;
class wxTextCtrl;

namespace say_count::ui {

class SnapshotDialog final : public wxDialog {
public:
    SnapshotDialog(wxWindow* parent, SnapshotStore& store,
                   const std::vector<NamedScript>& current);
    std::optional<std::uint64_t> selected_id() const { return selected_id_; }

private:
    void ShowSelection();

    SnapshotStore& store_;
    std::vector<NamedScript> current_;
    std::vector<SnapshotMetadata> snapshots_;
    wxListBox* list_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    std::optional<std::uint64_t> selected_id_;
};

}  // namespace say_count::ui
