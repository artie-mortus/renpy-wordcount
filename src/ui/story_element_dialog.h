#pragma once

#include <wx/dialog.h>

#include "core/script_builder.h"

class wxButton;
class wxChoice;
class wxStaticText;
class wxTextCtrl;

namespace say_count::ui {

class StoryElementDialog final : public wxDialog {
public:
    StoryElementDialog(wxWindow* parent, const wxString& indentation);
    const std::string& generated_text() const { return generated_text_; }

private:
    void RefreshForm();
    StoryElementKind kind() const;

    wxChoice* kind_ = nullptr;
    wxStaticText* primary_label_ = nullptr;
    wxStaticText* secondary_label_ = nullptr;
    wxStaticText* details_label_ = nullptr;
    wxStaticText* guidance_ = nullptr;
    wxTextCtrl* primary_ = nullptr;
    wxTextCtrl* secondary_ = nullptr;
    wxTextCtrl* details_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxButton* insert_ = nullptr;
    wxString indentation_;
    std::string generated_text_;
};

}  // namespace say_count::ui
