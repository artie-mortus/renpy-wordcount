#pragma once

#include <wx/panel.h>

#include "core/renpy_lint.h"

#include <functional>

class wxDataViewListCtrl;
class wxStaticText;

namespace say_count::ui {

class RenpyLintPanel final : public wxPanel {
public:
    explicit RenpyLintPanel(wxWindow* parent);
    void SetRunning();
    void SetResults(std::vector<RenpyLintIssue> issues, int exit_code, std::string output);
    void SetJumpHandler(std::function<void(const RenpyLintIssue&)> handler);
private:
    wxStaticText* summary_ = nullptr;
    wxDataViewListCtrl* list_ = nullptr;
    std::vector<RenpyLintIssue> issues_;
    std::function<void(const RenpyLintIssue&)> jump_handler_;
};

}  // namespace say_count::ui
