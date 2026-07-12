#pragma once
#include <functional>
#include <wx/panel.h>
#include "core/parser.h"

class wxListCtrl;
namespace say_count::ui {
class OutlinePanel final : public wxPanel {
public:
    explicit OutlinePanel(wxWindow* parent);
    void SetDocument(const wxString& source, const ScriptAnalysis& analysis);
    void SetJumpHandler(std::function<void(std::size_t)> handler);
private:
    wxListCtrl* list_ = nullptr;
    std::vector<std::size_t> jump_lines_;
    std::function<void(std::size_t)> jump_handler_;
};
}
