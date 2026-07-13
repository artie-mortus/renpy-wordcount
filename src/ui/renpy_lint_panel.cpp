#include "ui/renpy_lint_panel.h"

#include <wx/dataview.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace say_count::ui {

RenpyLintPanel::RenpyLintPanel(wxWindow* parent) : wxPanel(parent) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    summary_ = new wxStaticText(this, wxID_ANY, "Official lint has not run yet.");
    layout->Add(summary_, 0, wxEXPAND | wxALL, 6);
    list_ = new wxDataViewListCtrl(this, wxID_ANY);
    list_->AppendTextColumn("File", wxDATAVIEW_CELL_INERT, 220, wxALIGN_LEFT);
    list_->AppendTextColumn("Line", wxDATAVIEW_CELL_INERT, 60, wxALIGN_LEFT);
    list_->AppendTextColumn("Official Ren'Py message", wxDATAVIEW_CELL_INERT, 560, wxALIGN_LEFT);
    layout->Add(list_, 1, wxEXPAND);
    SetSizer(layout);
    list_->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, [this](wxDataViewEvent& event) {
        const int row = list_->ItemToRow(event.GetItem());
        if (row >= 0 && static_cast<std::size_t>(row) < issues_.size() && jump_handler_)
            jump_handler_(issues_[static_cast<std::size_t>(row)]);
    });
}

void RenpyLintPanel::SetRunning() {
    issues_.clear(); list_->DeleteAllItems(); summary_->SetLabel("Running official Ren'Py lint...");
}

void RenpyLintPanel::SetResults(std::vector<RenpyLintIssue> issues, int exit_code, std::string output) {
    issues_ = std::move(issues); list_->DeleteAllItems();
    for (const auto& issue : issues_) {
        wxVector<wxVariant> row;
        row.push_back(wxVariant(wxString::FromUTF8(issue.file)));
        row.push_back(wxVariant(wxString::Format("%zu", issue.line)));
        row.push_back(wxVariant(wxString::FromUTF8(issue.message)));
        list_->AppendItem(row);
    }
    if (issues_.empty()) summary_->SetLabel(exit_code == 0 ? "Official lint completed with no located issues."
                                                            : "Lint failed without a source location; see Ren'Py Log.");
    else summary_->SetLabel(wxString::Format("%zu official lint issue%s (exit %d)", issues_.size(),
                                              issues_.size() == 1 ? "" : "s", exit_code));
    (void)output;
}

void RenpyLintPanel::SetJumpHandler(std::function<void(const RenpyLintIssue&)> handler) {
    jump_handler_ = std::move(handler);
}

}  // namespace say_count::ui
