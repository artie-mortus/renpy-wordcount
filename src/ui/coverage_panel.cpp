#include "ui/coverage_panel.h"

#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

namespace say_count::ui {

CoveragePanel::CoveragePanel(wxWindow* parent) : wxPanel(parent) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    summary_ = new wxStaticText(this, wxID_ANY, "Connect a project to track label coverage.");
    layout->Add(summary_, 0, wxEXPAND | wxALL, 6);
    list_ = new wxDataViewListCtrl(this, wxID_ANY);
    list_->AppendTextColumn("Label", wxDATAVIEW_CELL_INERT, 260, wxALIGN_LEFT);
    list_->AppendTextColumn("Manual", wxDATAVIEW_CELL_INERT, 80, wxALIGN_CENTER);
    list_->AppendTextColumn("Playthrough", wxDATAVIEW_CELL_INERT, 100, wxALIGN_CENTER);
    layout->Add(list_, 1, wxEXPAND);
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    toggle_ = new wxButton(this, wxID_ANY, "Mark Manual");
    auto* clear = new wxButton(this, wxID_ANY, "Clear Playthrough");
    actions->Add(toggle_, 0, wxRIGHT, 6); actions->Add(clear);
    layout->Add(actions, 0, wxALL, 6); SetSizer(layout);
    list_->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, [this](wxDataViewEvent&) {
        const auto label = SelectedLabel(); toggle_->Enable(!label.empty());
        toggle_->SetLabel(manual_.count(label) ? "Unmark Manual" : "Mark Manual");
    });
    toggle_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        const auto label = SelectedLabel(); if (!label.empty() && manual_handler_) manual_handler_(label, manual_.count(label) == 0);
    });
    clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { if (clear_handler_) clear_handler_(); });
    toggle_->Enable(false);
}

void CoveragePanel::SetCoverage(std::vector<std::string> labels, std::set<std::string> manual,
                                std::set<std::string> playthrough) {
    labels_ = std::move(labels); manual_ = std::move(manual); playthrough_ = std::move(playthrough); Rebuild();
}

void CoveragePanel::SetManualHandler(std::function<void(const std::string&, bool)> handler) { manual_handler_ = std::move(handler); }
void CoveragePanel::SetClearHandler(std::function<void()> handler) { clear_handler_ = std::move(handler); }

void CoveragePanel::Rebuild() {
    const auto previous = SelectedLabel(); list_->DeleteAllItems(); std::size_t covered = 0;
    for (const auto& label : labels_) {
        const bool manual = manual_.count(label); const bool played = playthrough_.count(label);
        if (manual || played) ++covered;
        wxVector<wxVariant> row; row.push_back(wxVariant(wxString::FromUTF8(label)));
        row.push_back(wxVariant(manual ? "Yes" : "")); row.push_back(wxVariant(played ? "Yes" : "")); list_->AppendItem(row);
    }
    summary_->SetLabel(wxString::Format("%zu of %zu labels covered (%zu manual, %zu playthrough)",
                                        covered, labels_.size(), manual_.size(), playthrough_.size()));
    if (!previous.empty()) for (unsigned row = 0; row < list_->GetItemCount(); ++row)
        if (list_->GetTextValue(row, 0).ToStdString() == previous) { list_->SelectRow(row); break; }
    toggle_->Enable(!SelectedLabel().empty());
}

std::string CoveragePanel::SelectedLabel() const {
    const int row = list_->ItemToRow(list_->GetSelection());
    return row < 0 ? std::string{} : list_->GetTextValue(static_cast<unsigned>(row), 0).ToStdString();
}

}  // namespace say_count::ui
