#include "ui/palette_dialog.h"

#include "core/navigator.h"
#include "ui/style.h"

#include <algorithm>
#include <string>

#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace say_count::ui {

PaletteDialog::PaletteDialog(wxWindow* parent, wxString title, wxString hint,
                             std::vector<PaletteEntry> entries)
    : wxDialog(parent, wxID_ANY, std::move(title), wxDefaultPosition, wxSize(720, 480),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), entries_(std::move(entries)) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    query_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize,
                            wxTE_PROCESS_ENTER);
    query_->SetHint(hint);
    query_->SetFont(style::BodyFont(13));
    results_ = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                              wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_HEADER);
    results_->AppendColumn("Action", wxLIST_FORMAT_LEFT, 340);
    results_->AppendColumn("Context", wxLIST_FORMAT_LEFT, 330);
    auto* help = new wxStaticText(this, wxID_ANY,
        "Type to filter  ·  ↑↓ navigate  ·  Enter open  ·  Esc close");
    help->SetFont(style::UtilityFont(8));
    help->SetForegroundColour(style::Colors().ink_soft);
    layout->Add(query_, 0, wxEXPAND | wxALL, 16);
    layout->Add(results_, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);
    layout->Add(help, 0, wxALL, 16);
    SetSizer(layout);
    style::ApplyWorkspaceTheme(this);
    query_->SetFont(style::BodyFont(13));
    help->SetFont(style::UtilityFont(8));
    help->SetForegroundColour(style::Colors().ink_soft);
    query_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshResults(); });
    query_->Bind(wxEVT_TEXT_ENTER, [this](wxCommandEvent&) { Accept(); });
    query_->Bind(wxEVT_KEY_DOWN, &PaletteDialog::OnKeyDown, this);
    results_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { Accept(); });
    RefreshResults();
    query_->SetFocus();
    CentreOnParent();
}

void PaletteDialog::RefreshResults() {
    struct Match { int score; std::size_t index; };
    std::vector<Match> matches;
    const std::string query = query_->GetValue().ToStdString(wxConvUTF8);
    for (std::size_t index = 0; index < entries_.size(); ++index) {
        const std::string candidate = (entries_[index].title + " " + entries_[index].detail)
                                          .ToStdString(wxConvUTF8);
        const int score = fuzzy_score(query, candidate);
        if (score >= 0) matches.push_back({score, index});
    }
    std::stable_sort(matches.begin(), matches.end(), [](const auto& left, const auto& right) {
        return left.score > right.score;
    });
    results_->Freeze();
    results_->DeleteAllItems();
    visible_.clear();
    for (std::size_t row = 0; row < std::min<std::size_t>(matches.size(), 100); ++row) {
        const auto index = matches[row].index;
        visible_.push_back(index);
        const long inserted = results_->InsertItem(static_cast<long>(row), entries_[index].title);
        results_->SetItem(inserted, 1, entries_[index].detail);
    }
    if (!visible_.empty()) results_->SetItemState(0, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    results_->Thaw();
}

void PaletteDialog::Accept() {
    const long row = results_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0 || static_cast<std::size_t>(row) >= visible_.size()) return;
    selected_id_ = entries_[visible_[static_cast<std::size_t>(row)]].id;
    EndModal(wxID_OK);
}

void PaletteDialog::OnKeyDown(wxKeyEvent& event) {
    if (event.GetKeyCode() == WXK_ESCAPE) { EndModal(wxID_CANCEL); return; }
    if (event.GetKeyCode() != WXK_UP && event.GetKeyCode() != WXK_DOWN) {
        event.Skip(); return;
    }
    long row = results_->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (row < 0) row = 0;
    else row = std::clamp<long>(row + (event.GetKeyCode() == WXK_DOWN ? 1 : -1),
                                0, std::max<long>(0, results_->GetItemCount() - 1));
    results_->SetItemState(-1, 0, wxLIST_STATE_SELECTED);
    results_->SetItemState(row, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
    results_->EnsureVisible(row);
}

}  // namespace say_count::ui
