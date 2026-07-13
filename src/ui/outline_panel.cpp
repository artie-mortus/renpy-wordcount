#include "ui/outline_panel.h"
#include "core/outline.h"
#include <wx/listctrl.h>
#include <wx/sizer.h>

namespace say_count::ui {
OutlinePanel::OutlinePanel(wxWindow* parent) : wxPanel(parent) {
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    list_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    list_->AppendColumn("Scene / flow", wxLIST_FORMAT_LEFT, 195);
    list_->AppendColumn("Words", wxLIST_FORMAT_RIGHT, 55);
    sizer->Add(list_, 1, wxEXPAND); SetSizer(sizer);
    list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event) {
        const auto row = static_cast<std::size_t>(event.GetIndex());
        if (row < jump_lines_.size() && jump_handler_) jump_handler_(jump_lines_[row]);
    });
}
void OutlinePanel::SetJumpHandler(std::function<void(std::size_t)> handler) { jump_handler_ = std::move(handler); }
void OutlinePanel::SetDocument(const wxString& source, const ScriptAnalysis& analysis) {
    list_->DeleteAllItems(); jump_lines_.clear();
    std::map<std::string, std::size_t> words;
    for (const auto& scene : analysis.scenes) words[scene.name] = scene.words;
    for (const auto& item : build_outline(source.ToStdString(wxConvUTF8))) {
        wxString text;
        switch (item.kind) {
        case OutlineKind::Label: text = wxString::FromUTF8(item.name); break;
        case OutlineKind::Choice: text = wxString::FromUTF8("◇ " + item.name); break;
        case OutlineKind::Jump: text = wxString::FromUTF8("→ jump " + item.name); break;
        case OutlineKind::Call: text = wxString::FromUTF8("⇢ call " + item.name); break;
        }
        const long row = list_->InsertItem(list_->GetItemCount(), text);
        if (item.kind == OutlineKind::Label && words[item.qualified_name])
            list_->SetItem(row, 1, wxString::Format("%zu", words[item.qualified_name]));
        if ((item.kind == OutlineKind::Jump || item.kind == OutlineKind::Call) && !item.target_found) {
            list_->SetItemTextColour(row, wxColour("#c62828"));
        }
        jump_lines_.push_back(item.kind == OutlineKind::Jump || item.kind == OutlineKind::Call ? item.target_line : item.line_number);
    }
}
}
