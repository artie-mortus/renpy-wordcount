#pragma once

#include <wx/panel.h>

#include <functional>
#include <vector>

#include "core/diagnostics.h"

class wxDataViewListCtrl;
class wxDataViewEvent;

namespace say_count::ui {

class DiagnosticsPanel final : public wxPanel {
public:
    using JumpHandler = std::function<void(const Diagnostic&)>;

    explicit DiagnosticsPanel(wxWindow* parent);
    void SetDiagnostics(const std::vector<Diagnostic>& diagnostics);
    void SetJumpHandler(JumpHandler handler);

private:
    void OnActivated(wxDataViewEvent& event);

    wxDataViewListCtrl* list_ = nullptr;
    std::vector<Diagnostic> diagnostics_;
    JumpHandler jump_handler_;
};

}  // namespace say_count::ui
