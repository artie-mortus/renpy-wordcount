#pragma once

#include <wx/colour.h>
#include <wx/font.h>

class wxButton;
class wxWindow;

namespace say_count::ui::style {

struct Palette {
    wxColour ink;
    wxColour ink_soft;
    wxColour paper;
    wxColour canvas;
    wxColour line;
    wxColour plum;
    wxColour cue;
    wxColour mint;
    wxColour white;
};

const Palette& Colors();
wxFont BodyFont(int size = 10, wxFontWeight weight = wxFONTWEIGHT_NORMAL);
wxFont UtilityFont(int size = 9, wxFontWeight weight = wxFONTWEIGHT_NORMAL);
void ApplyWorkspaceTheme(wxWindow* root);
void StylePrimaryButton(wxButton* button);

}  // namespace say_count::ui::style
