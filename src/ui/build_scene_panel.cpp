#include "ui/build_scene_panel.h"

#include <algorithm>
#include <set>

#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/tglbtn.h>

#include "ui/style.h"

namespace say_count::ui {
namespace {

struct FormCopy {
    const char* primary;
    const char* secondary;
    const char* details;
    const char* hint;
    const char* guidance;
};

FormCopy CopyFor(StoryElementKind kind) {
    switch (kind) {
        case StoryElementKind::Character:
            return {"Short character code", "Name players will see", "", "e",
                    "Creates a character definition at the top of this script."};
        case StoryElementKind::Dialogue:
            return {"Who speaks? (blank for narrator)", "What do they say?", "", "Eileen",
                    "Choose a character by name. Their short code is used in the script."};
        case StoryElementKind::Choice:
            return {"Question (optional)", "", "Choices — one per line", "What now?",
                    "Add at least two choices. Replace each generated pass with what happens next."};
        case StoryElementKind::Scene:
            return {"Background image", "", "", "bg kitchen",
                    "Replaces the stage with this background."};
        case StoryElementKind::Show:
            return {"Image", "Placement (optional)", "", "eileen happy",
                    "Shows a sprite or image. Placement can be left, center, or right."};
        case StoryElementKind::Hide:
            return {"Image tag", "", "", "eileen",
                    "Hides the displayed image with this tag."};
        case StoryElementKind::Transition:
            return {"Transition", "", "", "dissolve",
                    "Applies this transition after the preceding visual change."};
        case StoryElementKind::Music:
            return {"Music file", "", "", "audio/theme.ogg",
                    "Starts background music from the project."};
        case StoryElementKind::Sound:
            return {"Sound file", "", "", "audio/door.ogg",
                    "Plays a sound effect once."};
        case StoryElementKind::StopMusic:
            return {"Fade-out seconds (optional)", "", "", "0.5",
                    "Leave blank to stop the music immediately."};
        case StoryElementKind::Pause:
            return {"Seconds (optional)", "", "", "1.0",
                    "Leave blank to wait until the player continues."};
        case StoryElementKind::Jump:
            return {"Destination scene", "", "", "ending",
                    "Continues at another label without coming back."};
        case StoryElementKind::Call:
            return {"Scene to call", "", "", "aside",
                    "Plays another label, then comes back here when it returns."};
        case StoryElementKind::Return:
            return {"", "", "", "", "Returns to the scene that called this one."};
    }
    return {};
}

std::string StatusFor(StoryElementKind kind) {
    switch (kind) {
        case StoryElementKind::Character: return "Character added at the top of this script — Undo is available";
        case StoryElementKind::Dialogue: return "Dialogue added — Undo is available";
        case StoryElementKind::Choice: return "Player choice added — replace each pass with the next action";
        case StoryElementKind::Scene: return "Background cue added — Undo is available";
        case StoryElementKind::Show: return "Image cue added — Undo is available";
        case StoryElementKind::Hide: return "Hide cue added — Undo is available";
        case StoryElementKind::Transition: return "Transition added — Undo is available";
        case StoryElementKind::Music: return "Music cue added — Undo is available";
        case StoryElementKind::Sound: return "Sound cue added — Undo is available";
        case StoryElementKind::StopMusic: return "Stop-music cue added — Undo is available";
        case StoryElementKind::Pause: return "Pause added — Undo is available";
        case StoryElementKind::Jump: return "Jump added — Undo is available";
        case StoryElementKind::Call: return "Scene call added — Undo is available";
        case StoryElementKind::Return: return "Return added — Undo is available";
    }
    return "Story cue added — Undo is available";
}

}  // namespace

BuildScenePanel::BuildScenePanel(wxWindow* parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxTAB_TRAVERSAL) {
    SetBackgroundColour(style::Colors().canvas);
    SetScrollRate(0, FromDIP(10));
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* cue = new wxPanel(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 4)));
    cue->SetBackgroundColour(style::Colors().mint);
    root->Add(cue, 0, wxEXPAND);
    auto* kicker = new wxStaticText(this, wxID_ANY, "BUILD SCENE");
    kicker->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    kicker->SetForegroundColour(style::Colors().plum);
    root->Add(kicker, 0, wxLEFT | wxRIGHT | wxTOP, 14);
    auto* heading = new wxStaticText(this, wxID_ANY, "What happens next?");
    heading->SetFont(style::BodyFont(15, wxFONTWEIGHT_BOLD));
    root->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, 14);
    auto* intro = new wxStaticText(
        this, wxID_ANY, "Choose a cue, fill in plain-language fields, then add the preview at the cursor.");
    intro->SetForegroundColour(style::Colors().ink_soft);
    intro->Wrap(310);
    root->Add(intro, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 14);

    auto* cue_book = new wxNotebook(this, wxID_ANY, wxDefaultPosition, FromDIP(wxSize(-1, 132)));
    cue_book->SetName("Build scene cue categories");
    AddCueGroup(cue_book, "Text", {{StoryElementKind::Dialogue, "Someone speaks"},
                                 {StoryElementKind::Choice, "Player chooses"},
                                 {StoryElementKind::Character, "Add character"}});
    AddCueGroup(cue_book, "Stage", {{StoryElementKind::Scene, "Set background"},
                                 {StoryElementKind::Show, "Show image"},
                                 {StoryElementKind::Hide, "Hide image"},
                                 {StoryElementKind::Transition, "Transition"}});
    AddCueGroup(cue_book, "Sound", {{StoryElementKind::Music, "Play music"},
                                        {StoryElementKind::Sound, "Play sound"},
                                        {StoryElementKind::StopMusic, "Stop music"},
                                        {StoryElementKind::Pause, "Pause"}});
    AddCueGroup(cue_book, "Flow", {{StoryElementKind::Jump, "Jump to scene"},
                                {StoryElementKind::Call, "Call scene"},
                                {StoryElementKind::Return, "Return"}});
    root->Add(cue_book, 0, wxEXPAND | wxLEFT | wxRIGHT, 14);

    auto* form = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_SIMPLE);
    form->SetBackgroundColour(style::Colors().paper);
    auto* form_layout = new wxBoxSizer(wxVERTICAL);
    guidance_ = new wxStaticText(form, wxID_ANY, wxEmptyString);
    guidance_->SetForegroundColour(style::Colors().ink_soft);
    guidance_->Wrap(290);
    form_layout->Add(guidance_, 0, wxEXPAND | wxALL, 12);

    primary_label_ = new wxStaticText(form, wxID_ANY, wxEmptyString);
    primary_ = new wxComboBox(form, wxID_ANY);
    primary_->SetName("Build scene primary field");
    form_layout->Add(primary_label_, 0, wxLEFT | wxRIGHT, 12);
    form_layout->Add(primary_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    secondary_label_ = new wxStaticText(form, wxID_ANY, wxEmptyString);
    secondary_ = new wxTextCtrl(form, wxID_ANY, wxEmptyString, wxDefaultPosition,
                                FromDIP(wxSize(-1, 64)), wxTE_MULTILINE);
    secondary_->SetName("Build scene secondary field");
    form_layout->Add(secondary_label_, 0, wxLEFT | wxRIGHT, 12);
    form_layout->Add(secondary_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);

    details_label_ = new wxStaticText(form, wxID_ANY, wxEmptyString);
    details_ = new wxTextCtrl(form, wxID_ANY, wxEmptyString, wxDefaultPosition,
                              FromDIP(wxSize(-1, 90)), wxTE_MULTILINE);
    details_->SetName("Build scene details field");
    form_layout->Add(details_label_, 0, wxLEFT | wxRIGHT, 12);
    form_layout->Add(details_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 12);
    form->SetSizer(form_layout);
    root->Add(form, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);

    auto* preview_label = new wxStaticText(this, wxID_ANY, "SCRIPT CUE");
    preview_label->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    preview_label->SetForegroundColour(style::Colors().plum);
    root->Add(preview_label, 0, wxLEFT | wxRIGHT | wxTOP, 14);
    preview_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition,
                              FromDIP(wxSize(-1, 88)), wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    preview_->SetName("Build scene preview");
    preview_->SetFont(style::UtilityFont(9));
    preview_->SetBackgroundColour(style::Colors().ink);
    preview_->SetForegroundColour(style::Colors().white);
    root->Add(preview_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);

    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    auto* clear = new wxButton(this, wxID_ANY, "Clear");
    clear->SetName("Clear build scene cue");
    insert_ = new wxButton(this, wxID_ANY, "Add at cursor");
    style::StyleSecondaryButton(clear);
    style::StylePrimaryButton(insert_);
    insert_->SetName("Add build scene cue");
    actions->Add(clear);
    actions->AddStretchSpacer();
    actions->Add(insert_);
    root->Add(actions, 0, wxEXPAND | wxALL, 14);

    SetSizer(root);
    primary_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshForm(); });
    primary_->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent&) { RefreshForm(); });
    secondary_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshForm(); });
    details_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshForm(); });
    clear->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ClearFields(); });
    insert_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { InsertCue(); });
    SelectKind(StoryElementKind::Dialogue);
}

void BuildScenePanel::AddCueGroup(
    wxNotebook* book, const wxString& label,
    const std::vector<std::pair<StoryElementKind, wxString>>& cues) {
    auto* page = new wxPanel(book);
    page->SetBackgroundColour(style::Colors().paper);
    auto* grid = new wxFlexGridSizer(2, FromDIP(6), FromDIP(6));
    grid->AddGrowableCol(0, 1);
    grid->AddGrowableCol(1, 1);
    for (const auto& [kind, text] : cues) {
        auto* button = new wxToggleButton(page, wxID_ANY, text);
        button->SetName("Build scene cue " + text);
        button->SetFont(style::BodyFont(9, wxFONTWEIGHT_MEDIUM));
        button->SetMinSize(FromDIP(wxSize(120, 34)));
        button->Bind(wxEVT_TOGGLEBUTTON, [this, kind](wxCommandEvent&) { SelectKind(kind); });
        cue_buttons_.push_back({kind, button});
        grid->Add(button, 1, wxEXPAND);
    }
    if (cues.size() % 2 != 0) grid->AddSpacer(1);
    auto* page_layout = new wxBoxSizer(wxVERTICAL);
    page_layout->Add(grid, 1, wxEXPAND | wxALL, 8);
    page->SetSizer(page_layout);
    book->AddPage(page, label, book->GetPageCount() == 0);
}

void BuildScenePanel::SetProject(const ScriptAnalysis& analysis,
                                 const std::vector<ProjectAsset>& assets) {
    characters_.clear();
    for (const auto& [alias, name] : analysis.character_names) {
        characters_.push_back({name, alias});
    }
    labels_.clear();
    for (const auto& scene : analysis.scenes)
        if (!scene.name.empty()) labels_.push_back(scene.name);
    images_.clear();
    audio_.clear();
    for (const auto& asset : assets) {
        if (asset.kind == AssetKind::Audio) {
            audio_.push_back(asset.relative_path);
            continue;
        }
        std::string statement = asset_insert_statement(asset, AssetInsertAction::Show);
        if (statement.rfind("show ", 0) == 0) images_.push_back(statement.substr(5));
    }
    auto unique = [](auto& values) {
        std::sort(values.begin(), values.end());
        values.erase(std::unique(values.begin(), values.end()), values.end());
    };
    unique(characters_); unique(labels_); unique(images_); unique(audio_);
    RefreshSuggestions();
    RefreshForm();
}

void BuildScenePanel::SetInsertHandler(
    std::function<void(StoryElementKind, const std::string&, const std::string&)> handler) {
    insert_handler_ = std::move(handler);
}

void BuildScenePanel::SetIndentationProvider(std::function<std::string()> provider) {
    indentation_provider_ = std::move(provider);
    RefreshForm();
}

void BuildScenePanel::RefreshContext() { RefreshForm(); }

void BuildScenePanel::SelectKind(StoryElementKind kind) {
    selected_kind_ = kind;
    for (auto& cue : cue_buttons_) {
        const bool selected = cue.kind == kind;
        cue.button->SetValue(selected);
        cue.button->SetBackgroundColour(selected ? style::Colors().cue : wxColour("#E2E7EB"));
        cue.button->SetForegroundColour(style::Colors().ink);
    }
    ClearFields();
    RefreshSuggestions();
    RefreshForm();
}

std::vector<std::string> BuildScenePanel::SuggestionsFor(StoryElementKind kind) const {
    switch (kind) {
        case StoryElementKind::Dialogue: {
            std::vector<std::string> names;
            names.reserve(characters_.size());
            for (const auto& [name, alias] : characters_) {
                (void)alias;
                names.push_back(name);
            }
            return names;
        }
        case StoryElementKind::Scene:
        case StoryElementKind::Show:
        case StoryElementKind::Hide: return images_;
        case StoryElementKind::Music:
        case StoryElementKind::Sound: return audio_;
        case StoryElementKind::Jump:
        case StoryElementKind::Call: return labels_;
        case StoryElementKind::Transition: return {"dissolve", "fade", "pixellate", "move"};
        case StoryElementKind::Pause: return {"0.5", "1.0", "2.0"};
        case StoryElementKind::StopMusic: return {"0.5", "1.0", "2.0"};
        default: return {};
    }
}

std::string BuildScenePanel::PrimaryValue() const {
    const std::string value = primary_->GetValue().ToStdString(wxConvUTF8);
    if (selected_kind_ != StoryElementKind::Dialogue) return value;

    const int selection = primary_->GetSelection();
    if (selection != wxNOT_FOUND && static_cast<std::size_t>(selection) < characters_.size())
        return characters_[selection].second;

    // SetValue() and some platform-native combo boxes can clear the selection
    // even when their text exactly matches an item. Resolve an unambiguous
    // display name here while continuing to accept manually entered aliases.
    const auto matching_name = std::find_if(characters_.begin(), characters_.end(),
        [&](const auto& character) { return character.first == value; });
    if (matching_name != characters_.end() &&
        std::find_if(std::next(matching_name), characters_.end(),
            [&](const auto& character) { return character.first == value; }) == characters_.end())
        return matching_name->second;
    return value;
}

void BuildScenePanel::RefreshSuggestions() {
    const wxString current = primary_->GetValue();
    primary_->Freeze();
    primary_->Clear();
    for (const auto& value : SuggestionsFor(selected_kind_))
        primary_->Append(wxString::FromUTF8(value));
    primary_->SetValue(current);
    primary_->Thaw();
}

void BuildScenePanel::RefreshForm() {
    const auto copy = CopyFor(selected_kind_);
    auto configure = [](wxStaticText* label, wxWindow* field, const char* text) {
        const bool visible = text && *text;
        label->SetLabel(visible ? wxString::FromUTF8(text) : wxString{});
        label->Show(visible);
        field->Show(visible);
    };
    configure(primary_label_, primary_, copy.primary);
    configure(secondary_label_, secondary_, copy.secondary);
    configure(details_label_, details_, copy.details);
    primary_->SetHint(wxString::FromUTF8(copy.hint));
    guidance_->SetLabel(wxString::FromUTF8(copy.guidance));
    const std::string indentation = indentation_provider_ ? indentation_provider_() : std::string{};
    const auto result = build_story_element({
        selected_kind_, PrimaryValue(),
        secondary_->GetValue().ToStdString(wxConvUTF8),
        details_->GetValue().ToStdString(wxConvUTF8), indentation});
    preview_->SetForegroundColour(result.valid ? style::Colors().white : style::Colors().mint);
    preview_->SetValue(result.valid ? wxString::FromUTF8(result.text)
                                    : "Finish the cue · " + wxString::FromUTF8(result.error));
    insert_->Enable(result.valid);
    insert_->SetBackgroundColour(result.valid ? style::Colors().cue : wxColour("#CBD2D8"));
    insert_->SetForegroundColour(result.valid ? style::Colors().ink : style::Colors().ink_soft);
    Layout();
    FitInside();
}

void BuildScenePanel::ClearFields(bool keep_primary) {
    if (!keep_primary) primary_->SetValue(wxEmptyString);
    secondary_->Clear();
    details_->Clear();
    RefreshForm();
    if (primary_->IsShown()) primary_->SetFocus();
    else insert_->SetFocus();
}

void BuildScenePanel::InsertCue() {
    const std::string indentation = indentation_provider_ ? indentation_provider_() : std::string{};
    const auto result = build_story_element({
        selected_kind_, PrimaryValue(),
        secondary_->GetValue().ToStdString(wxConvUTF8),
        details_->GetValue().ToStdString(wxConvUTF8), indentation});
    if (!result.valid || !insert_handler_) return;
    insert_handler_(selected_kind_, result.text, StatusFor(selected_kind_));
    ClearFields(selected_kind_ == StoryElementKind::Dialogue);
}

}  // namespace say_count::ui
