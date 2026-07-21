#include "ui/command_button.h"

#include "ui/style.h"

namespace say_count::ui {

CommandButton::CommandButton(wxWindow* parent, const wxString& label, bool primary)
    : wxButton(parent, wxID_ANY, label, wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT) {
    SetFont(style::BodyFont(9, wxFONTWEIGHT_BOLD));
    SetForegroundColour(primary ? style::Colors().ink : style::Colors().white);
    SetBackgroundColour(primary ? style::Colors().cue : wxColour("#243650"));
    SetMinSize(FromDIP(wxSize(GetTextExtent(label).x + 30, 36)));
    SetName(label);
    SetToolTip(label);
}

void CommandButton::SetCommandLabel(const wxString& label) {
    SetLabel(label);
    SetName(label);
    SetToolTip(label);
    SetMinSize(FromDIP(wxSize(GetTextExtent(label).x + 30, 36)));
}

}  // namespace say_count::ui
