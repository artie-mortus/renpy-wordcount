#pragma once

#include <wx/panel.h>

#include <functional>
#include <vector>

#include "core/diagnostics.h"

class wxDataViewListCtrl;
class wxDataViewEvent;
class wxButton;
class wxStaticText;

namespace say_count::ui {

class DiagnosticsPanel final : public wxPanel {
public:
    using JumpHandler = std::function<void(const Diagnostic&)>;
    using FixHandler = std::function<void()>;

    explicit DiagnosticsPanel(wxWindow* parent);
    void SetDiagnostics(const std::vector<Diagnostic>& diagnostics);
    void SetJumpHandler(JumpHandler handler);
    void SetFixHandler(FixHandler handler);

private:
    void OnActivated(wxDataViewEvent& event);

    wxDataViewListCtrl* list_ = nullptr;
    wxStaticText* fix_summary_ = nullptr;
    wxButton* fix_button_ = nullptr;
    std::vector<Diagnostic> diagnostics_;
    JumpHandler jump_handler_;
    FixHandler fix_handler_;
};

}  // namespace say_count::ui
