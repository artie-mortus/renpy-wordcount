#pragma once

#include <wx/dialog.h>
#include <wx/string.h>

#include <vector>

class wxListCtrl;
class wxTextCtrl;

namespace say_count::ui {

struct PaletteEntry {
    wxString title;
    wxString detail;
    int id = 0;
};

class PaletteDialog final : public wxDialog {
public:
    PaletteDialog(wxWindow* parent, wxString title, wxString hint,
                  std::vector<PaletteEntry> entries);
    int SelectedId() const { return selected_id_; }

private:
    void RefreshResults();
    void Accept();
    void OnKeyDown(wxKeyEvent& event);

    std::vector<PaletteEntry> entries_;
    std::vector<std::size_t> visible_;
    wxTextCtrl* query_ = nullptr;
    wxListCtrl* results_ = nullptr;
    int selected_id_ = -1;
};

}  // namespace say_count::ui
