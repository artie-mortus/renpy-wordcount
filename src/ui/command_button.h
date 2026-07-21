#pragma once

#include <wx/button.h>

namespace say_count::ui {

class CommandButton final : public wxButton {
public:
    CommandButton(wxWindow* parent, const wxString& label, bool primary = false);
    void SetCommandLabel(const wxString& label);
};

}  // namespace say_count::ui
