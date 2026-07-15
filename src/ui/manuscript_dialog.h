#pragma once

#include <wx/dialog.h>

#include "core/manuscript.h"

class wxCheckBox;
class wxStaticText;
class wxTextCtrl;

namespace say_count::ui {

void ShowManuscriptGuide(wxWindow* parent);

class ManuscriptDialog final : public wxDialog {
public:
    explicit ManuscriptDialog(wxWindow* parent);
    const ManuscriptConversion& conversion() const noexcept { return conversion_; }

private:
    void RefreshPreview();
    void Convert(wxCommandEvent& event);

    wxTextCtrl* manuscript_ = nullptr;
    wxTextCtrl* label_ = nullptr;
    wxCheckBox* definitions_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxStaticText* summary_ = nullptr;
    ManuscriptConversion conversion_;
};

}  // namespace say_count::ui
