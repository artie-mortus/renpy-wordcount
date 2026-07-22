#include "ui/story_element_dialog.h"

#include <algorithm>

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/style.h"

namespace say_count::ui {
namespace {

struct FormCopy { const char* primary; const char* secondary; const char* details; const char* guidance; };

FormCopy CopyFor(StoryElementKind kind) {
    switch (kind) {
        case StoryElementKind::Character: return {"Short character code", "Name players will see", "", "Example: e and Eileen. Add this near the top of a definitions script."};
        case StoryElementKind::Dialogue: return {"Character code (blank for narrator)", "What they say", "", "The line will be placed at the cursor."};
        case StoryElementKind::Choice: return {"Question shown to the player (optional)", "", "Choices — one per line", "Each choice starts with a safe placeholder. Replace pass with what happens next."};
        case StoryElementKind::Scene: return {"Background image name", "", "", "Use the Ren'Py image name, such as bg kitchen."};
        case StoryElementKind::Music: return {"Music file path", "", "", "Example: audio/theme.ogg"};
        case StoryElementKind::Sound: return {"Sound file path", "", "", "Example: audio/door-knock.ogg"};
        case StoryElementKind::Jump: return {"Scene name", "", "", "The destination scene must already exist."};
        case StoryElementKind::Show: return {"Image name", "Placement (optional)", "", "Example: eileen happy at left."};
        case StoryElementKind::Hide: return {"Image tag", "", "", "Hide every displayed image with this tag."};
        case StoryElementKind::StopMusic: return {"Fade-out seconds (optional)", "", "", "Leave blank to stop immediately."};
        case StoryElementKind::Pause: return {"Seconds (optional)", "", "", "Leave blank to wait for the player."};
        case StoryElementKind::Call: return {"Scene name", "", "", "Play another scene, then come back here."};
        case StoryElementKind::Return: return {"", "", "", "Return to the scene that called this one."};
        case StoryElementKind::Transition: return {"Transition name", "", "", "Common choices are dissolve and fade."};
    }
    return {};
}

}  // namespace

StoryElementDialog::StoryElementDialog(wxWindow* parent, const wxString& indentation,
                                       std::optional<StoryElementKind> fixed_kind)
    : wxDialog(parent, wxID_ANY, "Insert Into the Story", wxDefaultPosition, wxSize(760, 680),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), indentation_(indentation),
      fixed_kind_(fixed_kind) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* cue = new wxPanel(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 4)));
    cue->SetBackgroundColour(style::Colors().cue); layout->Add(cue, 0, wxEXPAND);
    auto* kicker = new wxStaticText(this, wxID_ANY, "STORY ELEMENT");
    kicker->SetFont(style::UtilityFont(9, wxFONTWEIGHT_BOLD)); kicker->SetForegroundColour(style::Colors().plum);
    layout->Add(kicker, 0, wxLEFT | wxRIGHT | wxTOP, 20);
    auto* heading = new wxStaticText(this, wxID_ANY,
        fixed_kind_ == StoryElementKind::Character ? "Add a character" : "Add something without writing code");
    heading->SetFont(style::BodyFont(17, wxFONTWEIGHT_BOLD)); layout->Add(heading, 0, wxALL, 20);

    kind_ = new wxChoice(this, wxID_ANY);
    for (const auto* label : {"Character", "Dialogue or narration", "Player choice", "Background scene",
                              "Music", "Sound effect", "Continue at another scene", "Show image",
                              "Hide image", "Stop music", "Pause", "Call another scene", "Return",
                              "Transition"}) kind_->Append(label);
    kind_->SetSelection(fixed_kind_ ? static_cast<int>(*fixed_kind_) : 0); kind_->SetName("Story element type");
    layout->Add(kind_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);
    kind_->Show(!fixed_kind_.has_value());
    guidance_ = new wxStaticText(this, wxID_ANY, wxEmptyString); guidance_->SetForegroundColour(style::Colors().ink_soft);
    layout->Add(guidance_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);

    auto add_field = [&](wxStaticText** label, wxTextCtrl** field, const char* name, long flags = 0) {
        *label = new wxStaticText(this, wxID_ANY, wxEmptyString); layout->Add(*label, 0, wxLEFT | wxRIGHT, 20);
        *field = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, flags);
        (*field)->SetName(name); layout->Add(*field, flags ? 1 : 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);
    };
    add_field(&primary_label_, &primary_, "Story element primary field");
    add_field(&secondary_label_, &secondary_, "Story element secondary field");
    add_field(&details_label_, &details_, "Story element choices", wxTE_MULTILINE);

    auto* preview_label = new wxStaticText(this, wxID_ANY, "What will be inserted");
    preview_label->SetFont(style::BodyFont(10, wxFONTWEIGHT_BOLD)); layout->Add(preview_label, 0, wxLEFT | wxRIGHT, 20);
    preview_ = new wxTextCtrl(this, wxID_ANY, {}, wxDefaultPosition, FromDIP(wxSize(-1, 120)),
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    preview_->SetName("Story element preview"); layout->Add(preview_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 20);
    auto* actions = new wxStdDialogButtonSizer();
    insert_ = new wxButton(this, wxID_OK, "Insert into story"); insert_->SetName("Insert into story");
    actions->AddButton(insert_); actions->AddButton(new wxButton(this, wxID_CANCEL, "Cancel")); actions->Realize();
    layout->Add(actions, 0, wxEXPAND | wxALL, 20); SetSizer(layout); CentreOnParent();
    kind_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { RefreshForm(); });
    for (auto* field : {primary_, secondary_, details_})
        field->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshForm(); });
    RefreshForm();
}

StoryElementKind StoryElementDialog::kind() const {
    return fixed_kind_.value_or(static_cast<StoryElementKind>(std::max(0, kind_->GetSelection())));
}

void StoryElementDialog::RefreshForm() {
    const auto copy = CopyFor(kind());
    auto configure = [this](wxStaticText* label, wxTextCtrl* field, const char* text) {
        const bool visible = text && *text; label->SetLabel(visible ? text : ""); label->Show(visible); field->Show(visible);
    };
    configure(primary_label_, primary_, copy.primary);
    configure(secondary_label_, secondary_, copy.secondary);
    configure(details_label_, details_, copy.details);
    guidance_->SetLabel(copy.guidance);
    const auto result = build_story_element({kind(), primary_->GetValue().ToStdString(wxConvUTF8),
        secondary_->GetValue().ToStdString(wxConvUTF8), details_->GetValue().ToStdString(wxConvUTF8),
        indentation_.ToStdString(wxConvUTF8)});
    generated_text_ = result.valid ? result.text : std::string{};
    preview_->SetValue(result.valid ? wxString::FromUTF8(result.text)
                                    : "Complete the fields above · " + wxString::FromUTF8(result.error));
    insert_->Enable(result.valid);
    Layout();
}

}  // namespace say_count::ui
