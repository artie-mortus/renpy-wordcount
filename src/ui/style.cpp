#include "ui/style.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dataview.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <wx/textctrl.h>
#include <wx/window.h>

namespace say_count::ui::style {
namespace {

const Palette kPalette{
    wxColour("#17243A"),  // ink
    wxColour("#4C5A6D"),  // ink soft
    wxColour("#FCFBF8"),  // paper
    wxColour("#EEF1F3"),  // canvas
    wxColour("#D8DEE4"),  // line
    wxColour("#76546E"),  // plum
    wxColour("#C69A3A"),  // cue
    wxColour("#6FC6B4"),  // mint
    wxColour("#FFFFFF"),  // white
};

void Apply(wxWindow* window) {
    if (!window || dynamic_cast<wxStyledTextCtrl*>(window)) return;

    const bool build_scene_preview = window->GetName() == "Build scene preview";

    window->SetFont(BodyFont());
    window->SetForegroundColour(kPalette.ink);

    if (dynamic_cast<wxPanel*>(window) || dynamic_cast<wxScrolledWindow*>(window) ||
        dynamic_cast<wxNotebook*>(window)) {
        window->SetBackgroundColour(kPalette.canvas);
    }
    if ((!build_scene_preview && dynamic_cast<wxTextCtrl*>(window)) || dynamic_cast<wxChoice*>(window) ||
        dynamic_cast<wxListCtrl*>(window) || dynamic_cast<wxDataViewCtrl*>(window)) {
        window->SetBackgroundColour(kPalette.white);
    }
    if (build_scene_preview) {
        window->SetBackgroundColour(kPalette.ink);
        window->SetForegroundColour(kPalette.white);
        window->SetFont(UtilityFont(9));
    }
    if (dynamic_cast<wxTextCtrl*>(window) || dynamic_cast<wxChoice*>(window)) {
        window->SetMinSize(wxSize(-1, 30));
    }
    if (auto* check = dynamic_cast<wxCheckBox*>(window)) {
        check->SetMinSize(wxSize(-1, 24));
    }
    if (auto* button = dynamic_cast<wxButton*>(window)) {
        button->SetBackgroundColour(wxColour("#E2E7EB"));
        button->SetForegroundColour(kPalette.ink);
        button->SetFont(BodyFont(9, wxFONTWEIGHT_MEDIUM));
        button->SetMinSize(wxSize(-1, 32));
        if (button->GetName() == "say-count-primary" ||
            button->GetName() == "Add build scene cue") {
            button->SetBackgroundColour(kPalette.cue);
            button->SetFont(BodyFont(9, wxFONTWEIGHT_BOLD));
        }
    }
    if (auto* label = dynamic_cast<wxStaticText*>(window)) {
        label->SetForegroundColour(kPalette.ink);
    }
    if (auto* list = dynamic_cast<wxListCtrl*>(window)) {
        list->SetTextColour(kPalette.ink);
    }

    for (auto* child : window->GetChildren()) Apply(child);
}

}  // namespace

const Palette& Colors() { return kPalette; }

wxFont BodyFont(int size, wxFontWeight weight) {
    wxFont font(wxFontInfo(size).Family(wxFONTFAMILY_SWISS).FaceName("DejaVu Sans"));
    font.SetWeight(weight);
    return font;
}

wxFont UtilityFont(int size, wxFontWeight weight) {
    wxFont font(wxFontInfo(size).Family(wxFONTFAMILY_TELETYPE).FaceName("DejaVu Sans Mono"));
    font.SetWeight(weight);
    return font;
}

void ApplyWorkspaceTheme(wxWindow* root) {
    Apply(root);
    if (root) root->Refresh();
}

void StylePrimaryButton(wxButton* button) {
    if (!button) return;
    button->SetName("say-count-primary");
    button->SetBackgroundColour(kPalette.cue);
    button->SetForegroundColour(kPalette.ink);
    button->SetFont(BodyFont(9, wxFONTWEIGHT_BOLD));
    button->SetMinSize(wxSize(-1, 34));
}

void StyleSecondaryButton(wxButton* button) {
    if (!button) return;
    button->SetBackgroundColour(wxColour("#E2E7EB"));
    button->SetForegroundColour(kPalette.ink);
    button->SetFont(BodyFont(9, wxFONTWEIGHT_MEDIUM));
    button->SetMinSize(wxSize(-1, 34));
}

}  // namespace say_count::ui::style
