#include "ui/diagnostics_panel.h"

#include <wx/button.h>
#include <wx/dataview.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "ui/style.h"

namespace say_count::ui {
namespace {

const char* severity_name(DiagnosticSeverity severity) {
    switch (severity) {
        case DiagnosticSeverity::Error: return "Error";
        case DiagnosticSeverity::Warning: return "Warning";
        case DiagnosticSeverity::Notice: return "Notice";
    }
    return "Warning";
}

}  // namespace

DiagnosticsPanel::DiagnosticsPanel(wxWindow* parent) : wxPanel(parent) {
    auto* layout = new wxBoxSizer(wxVERTICAL);

    auto* repair_bar = new wxPanel(this);
    repair_bar->SetBackgroundColour(style::Colors().canvas);
    auto* repair_layout = new wxBoxSizer(wxHORIZONTAL);
    auto* repair_mark = new wxPanel(repair_bar, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(3, 24)));
    repair_mark->SetBackgroundColour(style::Colors().cue);
    fix_summary_ = new wxStaticText(repair_bar, wxID_ANY, "No problems found");
    fix_summary_->SetFont(style::BodyFont(9, wxFONTWEIGHT_MEDIUM));
    fix_button_ = new wxButton(repair_bar, wxID_ANY, "Fix basic errors");
    fix_button_->SetToolTip(
        "Automatically repair indentation, unmatched quotes, missing block colons, and empty blocks");
    style::StylePrimaryButton(fix_button_);
    fix_button_->Enable(false);
    fix_button_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (fix_handler_) fix_handler_();
    });
    repair_layout->Add(repair_mark, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, FromDIP(10));
    repair_layout->Add(fix_summary_, 1, wxALIGN_CENTER_VERTICAL);
    repair_layout->Add(fix_button_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, FromDIP(12));
    repair_bar->SetSizer(repair_layout);
    layout->Add(repair_bar, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, FromDIP(8));

    list_ = new wxDataViewListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxDV_ROW_LINES | wxDV_VERT_RULES);
    list_->AppendTextColumn("Severity", wxDATAVIEW_CELL_INERT, 85, wxALIGN_LEFT,
                            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    list_->AppendTextColumn("File", wxDATAVIEW_CELL_INERT, 160, wxALIGN_LEFT,
                            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    list_->AppendTextColumn("Line", wxDATAVIEW_CELL_INERT, 60, wxALIGN_RIGHT,
                            wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    list_->AppendTextColumn("Message", wxDATAVIEW_CELL_INERT, 620, wxALIGN_LEFT,
                            wxDATAVIEW_COL_RESIZABLE);
    list_->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &DiagnosticsPanel::OnActivated, this);
    layout->Add(list_, 1, wxEXPAND);
    SetSizer(layout);
}

void DiagnosticsPanel::SetDiagnostics(const std::vector<Diagnostic>& diagnostics) {
    diagnostics_ = diagnostics;
    const std::size_t fixable = count_basic_fixes(diagnostics_);
    if (diagnostics_.empty()) {
        fix_summary_->SetLabel("No problems found");
        fix_button_->SetLabel("No errors to fix");
    } else if (fixable == 0) {
        fix_summary_->SetLabel("These diagnostics need review");
        fix_button_->SetLabel("No automatic fixes");
    } else {
        fix_summary_->SetLabel(wxString::FromUTF8(
            std::to_string(fixable) + " automatic repair" + (fixable == 1 ? "" : "s") +
            " available"));
        fix_button_->SetLabel("Fix basic errors");
    }
    fix_button_->Enable(fixable > 0);
    list_->DeleteAllItems();
    for (std::size_t index = 0; index < diagnostics_.size(); ++index) {
        const auto& diagnostic = diagnostics_[index];
        wxVector<wxVariant> row;
        row.push_back(wxVariant(severity_name(diagnostic.severity)));
        row.push_back(wxVariant(wxString::FromUTF8(diagnostic.file)));
        row.push_back(wxVariant(std::to_string(diagnostic.line_number)));
        row.push_back(wxVariant(wxString::FromUTF8(diagnostic.message)));
        list_->AppendItem(row, static_cast<wxUIntPtr>(index + 1));
    }
}

void DiagnosticsPanel::SetJumpHandler(JumpHandler handler) {
    jump_handler_ = std::move(handler);
}

void DiagnosticsPanel::SetFixHandler(FixHandler handler) {
    fix_handler_ = std::move(handler);
}

void DiagnosticsPanel::OnActivated(wxDataViewEvent& event) {
    const wxUIntPtr data = list_->GetItemData(event.GetItem());
    if (data == 0 || data - 1 >= diagnostics_.size() || !jump_handler_) return;
    jump_handler_(diagnostics_[data - 1]);
}

}  // namespace say_count::ui
