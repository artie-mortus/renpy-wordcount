#pragma once

#include <wx/dialog.h>

#include "core/rename.h"

#include <optional>

class wxButton;
class wxChoice;
class wxTextCtrl;

namespace say_count::ui {

class RenameDialog final : public wxDialog {
public:
    RenameDialog(wxWindow* parent, const std::vector<NamedScript>& files);
    const std::optional<RenamePlan>& plan() const { return plan_; }
    std::string original_name() const;

private:
    void Preview();
    void Invalidate();

    std::vector<NamedScript> files_;
    wxChoice* kind_ = nullptr;
    wxTextCtrl* from_ = nullptr;
    wxTextCtrl* to_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxButton* apply_ = nullptr;
    std::optional<RenamePlan> plan_;
};

}  // namespace say_count::ui
