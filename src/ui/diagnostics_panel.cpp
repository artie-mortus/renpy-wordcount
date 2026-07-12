#include "ui/diagnostics_panel.h"

#include <wx/dataview.h>
#include <wx/sizer.h>

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

void DiagnosticsPanel::OnActivated(wxDataViewEvent& event) {
    const wxUIntPtr data = list_->GetItemData(event.GetItem());
    if (data == 0 || data - 1 >= diagnostics_.size() || !jump_handler_) return;
    jump_handler_(diagnostics_[data - 1]);
}

}  // namespace say_count::ui
