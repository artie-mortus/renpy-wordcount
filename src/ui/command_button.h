#pragma once

#include <wx/control.h>

namespace say_count::ui {

class CommandButton final : public wxControl {
public:
    CommandButton(wxWindow* parent, const wxString& label, bool primary = false);
    void SetCommandLabel(const wxString& label);

protected:
    wxSize DoGetBestClientSize() const override;

private:
    void Activate();
    void OnPaint(wxPaintEvent& event);
    void OnLeftDown(wxMouseEvent& event);
    void OnLeftUp(wxMouseEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    void OnKeyUp(wxKeyEvent& event);

    bool primary_ = false;
    bool hovered_ = false;
    bool pressed_ = false;
};

}  // namespace say_count::ui
