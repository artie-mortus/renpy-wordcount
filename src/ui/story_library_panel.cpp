#include "ui/story_library_panel.h"

#include <algorithm>
#include <filesystem>

#include <wx/button.h>
#include <wx/filename.h>
#include <wx/image.h>
#include <wx/listbox.h>
#include <wx/mediactrl.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "core/script_builder.h"
#include "ui/style.h"

namespace say_count::ui {
namespace {

wxStaticText* Heading(wxWindow* parent, const wxString& text) {
    auto* label = new wxStaticText(parent, wxID_ANY, text);
    label->SetFont(style::BodyFont(13, wxFONTWEIGHT_BOLD));
    return label;
}

wxPanel* Page(wxNotebook* book) {
    auto* page = new wxPanel(book);
    page->SetBackgroundColour(style::Colors().paper);
    return page;
}

void AddIntro(wxWindow* page, wxBoxSizer* layout, const wxString& title, const wxString& copy) {
    layout->Add(Heading(page, title), 0, wxLEFT | wxRIGHT | wxTOP, 14);
    auto* help = new wxStaticText(page, wxID_ANY, copy);
    help->SetForegroundColour(style::Colors().ink_soft);
    help->Wrap(300);
    layout->Add(help, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 14);
}

wxString FriendlyAssetName(const ProjectAsset& asset) {
    std::string name = std::filesystem::path(asset.relative_path).stem().string();
    for (char& c : name) if (c == '_' || c == '-') c = ' ';
    if (name.rfind("bg ", 0) == 0) name.erase(0, 3);
    return wxString::FromUTF8(name + "  ·  " + asset.relative_path);
}

bool Matches(const wxString& query, const wxString& text) {
    return query.empty() || text.Lower().Contains(query);
}

}  // namespace

StoryLibraryPanel::StoryLibraryPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY) {
    SetBackgroundColour(style::Colors().paper);
    auto* root = new wxBoxSizer(wxVERTICAL);

    auto* search_row = new wxBoxSizer(wxVERTICAL);
    auto* search_label = new wxStaticText(this, wxID_ANY, "Find in this story");
    search_label->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    search_ = new wxTextCtrl(this, wxID_ANY);
    search_->SetName("Search story library");
    search_->SetHint("Search cast, images, music, or sounds");
    search_row->Add(search_label, 0, wxBOTTOM, 5);
    search_row->Add(search_, 0, wxEXPAND);
    root->Add(search_row, 0, wxEXPAND | wxALL, 14);

    auto* book = new wxNotebook(this, wxID_ANY);

    auto* characters = Page(book);
    auto* character_layout = new wxBoxSizer(wxVERTICAL);
    AddIntro(characters, character_layout, "Cast", "Choose who speaks, write their line, then add it at the cursor.");
    character_list_ = new wxListBox(characters, wxID_ANY);
    character_list_->SetName("Story library characters");
    character_layout->Add(character_list_, 1, wxEXPAND | wxALL, 14);
    character_help_ = new wxStaticText(characters, wxID_ANY, "No matching characters. Add one to grow the cast.");
    character_help_->Wrap(280);
    character_layout->Add(character_help_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);
    dialogue_ = new wxTextCtrl(characters, wxID_ANY, {}, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE);
    dialogue_->SetName("Character dialogue");
    dialogue_->SetHint("What do they say?");
    dialogue_->SetMinSize(wxSize(-1, 80));
    character_layout->Add(dialogue_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);
    auto* character_actions = new wxBoxSizer(wxHORIZONTAL);
    auto* add_character = new wxButton(characters, wxID_ANY, "Add character...");
    add_character->SetName("Add story library character");
    insert_dialogue_ = new wxButton(characters, wxID_ANY, "Add dialogue");
    insert_dialogue_->SetName("Add character dialogue");
    character_actions->Add(add_character);
    character_actions->AddStretchSpacer();
    character_actions->Add(insert_dialogue_);
    character_layout->Add(character_actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);
    characters->SetSizer(character_layout);
    book->AddPage(characters, "Cast", true);

    auto* places = Page(book);
    auto* place_layout = new wxBoxSizer(wxVERTICAL);
    AddIntro(places, place_layout, "Places & images", "Preview an image, then use it as the background or place it in the scene.");
    place_list_ = new wxListBox(places, wxID_ANY);
    place_list_->SetName("Story library places");
    place_layout->Add(place_list_, 1, wxEXPAND | wxALL, 14);
    place_preview_text_ = new wxStaticText(places, wxID_ANY, "No project images found.");
    place_preview_text_->SetName("Story library image preview text");
    place_preview_text_->SetForegroundColour(style::Colors().ink_soft);
    place_layout->Add(place_preview_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    place_preview_ = new wxStaticBitmap(places, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(260, 140));
    place_preview_->SetName("Story library image preview");
    place_layout->Add(place_preview_, 0, wxALIGN_CENTER | wxBOTTOM, 12);
    auto* place_actions = new wxBoxSizer(wxHORIZONTAL);
    set_background_ = new wxButton(places, wxID_ANY, "Set background");
    set_background_->SetName("Set story background");
    show_image_ = new wxButton(places, wxID_ANY, "Show in scene");
    show_image_->SetName("Show story image");
    place_actions->Add(set_background_);
    place_actions->AddStretchSpacer();
    place_actions->Add(show_image_);
    place_layout->Add(place_actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);
    places->SetSizer(place_layout);
    book->AddPage(places, "Images", false);

    auto* audio = Page(book);
    auto* audio_layout = new wxBoxSizer(wxVERTICAL);
    AddIntro(audio, audio_layout, "Music & sounds", "Preview audio already in this game, then add it at the cursor.");
    audio_list_ = new wxListBox(audio, wxID_ANY);
    audio_list_->SetName("Story library audio");
    audio_layout->Add(audio_list_, 1, wxEXPAND | wxALL, 14);
    audio_preview_text_ = new wxStaticText(audio, wxID_ANY, "No project audio found.");
    audio_preview_text_->SetName("Story library audio preview text");
    audio_preview_text_->SetForegroundColour(style::Colors().ink_soft);
    audio_layout->Add(audio_preview_text_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    audio_preview_ = new wxMediaCtrl(audio, wxID_ANY, wxString{}, wxDefaultPosition, wxSize(1, 1));
    audio_preview_->Hide();
    preview_audio_ = new wxButton(audio, wxID_ANY, "Preview audio");
    preview_audio_->SetName("Preview story audio");
    audio_layout->Add(preview_audio_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 14);
    auto* audio_actions = new wxBoxSizer(wxHORIZONTAL);
    play_music_ = new wxButton(audio, wxID_ANY, "Play as music");
    play_music_->SetName("Play story music");
    play_sound_ = new wxButton(audio, wxID_ANY, "Play as sound");
    play_sound_->SetName("Play story sound");
    audio_actions->Add(play_music_);
    audio_actions->AddStretchSpacer();
    audio_actions->Add(play_sound_);
    audio_layout->Add(audio_actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 14);
    audio->SetSizer(audio_layout);
    book->AddPage(audio, "Audio", false);

    root->Add(book, 1, wxEXPAND);
    SetSizer(root);

    search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshCharacters(); RefreshPlaces(); RefreshAudio(); });
    character_list_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { RefreshCharacterActions(); });
    dialogue_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshCharacterActions(); });
    insert_dialogue_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { InsertDialogue(); });
    add_character->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (add_character_handler_) add_character_handler_();
    });
    place_list_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { RefreshPlacePreview(); });
    audio_list_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { RefreshAudioPreview(); });
    preview_audio_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (audio_preview_->GetState() == wxMEDIASTATE_PLAYING) {
            audio_preview_->Stop(); preview_audio_->SetLabel("Preview audio");
        } else if (SelectedAudio() && audio_preview_->Play()) {
            preview_audio_->SetLabel("Stop preview");
        }
    });
    set_background_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { InsertAsset(AssetInsertAction::Scene); });
    show_image_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { InsertAsset(AssetInsertAction::Show); });
    play_music_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { InsertAsset(AssetInsertAction::PlayMusic); });
    play_sound_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { InsertAsset(AssetInsertAction::PlaySound); });
    RefreshCharacters(); RefreshPlaces(); RefreshAudio();
}

void StoryLibraryPanel::SetLibrary(const ScriptAnalysis& analysis, std::vector<ProjectAsset> assets) {
    characters_.clear();
    for (const auto& [alias, name] : analysis.character_names) characters_.push_back({alias, name});
    std::sort(characters_.begin(), characters_.end(), [](const auto& a, const auto& b) { return a.name < b.name; });
    places_.clear(); audio_.clear();
    for (auto& asset : assets) {
        if (asset.kind == AssetKind::Image) places_.push_back(std::move(asset));
        else audio_.push_back(std::move(asset));
    }
    RefreshCharacters(); RefreshPlaces(); RefreshAudio();
}

void StoryLibraryPanel::SetInsertHandler(std::function<void(const std::string&, const std::string&)> handler) {
    insert_handler_ = std::move(handler);
}

void StoryLibraryPanel::SetAddCharacterHandler(std::function<void()> handler) {
    add_character_handler_ = std::move(handler);
}

void StoryLibraryPanel::SetIndentationProvider(std::function<std::string()> provider) {
    indentation_provider_ = std::move(provider);
}

void StoryLibraryPanel::RefreshCharacters() {
    const wxString query = search_->GetValue().Lower();
    visible_characters_.clear(); character_list_->Clear();
    for (std::size_t index = 0; index < characters_.size(); ++index) {
        const wxString label = wxString::FromUTF8(characters_[index].name + "  ·  " + characters_[index].alias);
        if (!Matches(query, label)) continue;
        visible_characters_.push_back(index); character_list_->Append(label);
    }
    if (!visible_characters_.empty()) character_list_->SetSelection(0);
    character_help_->Show(visible_characters_.empty());
    dialogue_->Show(!visible_characters_.empty());
    Layout(); RefreshCharacterActions();
}

void StoryLibraryPanel::RefreshPlaces() {
    const wxString query = search_->GetValue().Lower();
    visible_places_.clear(); place_list_->Clear();
    for (std::size_t index = 0; index < places_.size(); ++index) {
        const wxString label = FriendlyAssetName(places_[index]);
        if (!Matches(query, label)) continue;
        visible_places_.push_back(index); place_list_->Append(label);
    }
    if (!visible_places_.empty()) place_list_->SetSelection(0);
    RefreshPlacePreview();
}

void StoryLibraryPanel::RefreshAudio() {
    const wxString query = search_->GetValue().Lower();
    visible_audio_.clear(); audio_list_->Clear();
    for (std::size_t index = 0; index < audio_.size(); ++index) {
        const wxString label = FriendlyAssetName(audio_[index]);
        if (!Matches(query, label)) continue;
        visible_audio_.push_back(index); audio_list_->Append(label);
    }
    if (!visible_audio_.empty()) audio_list_->SetSelection(0);
    RefreshAudioPreview();
}

void StoryLibraryPanel::RefreshCharacterActions() {
    insert_dialogue_->Enable(character_list_->GetSelection() != wxNOT_FOUND &&
                             !dialogue_->GetValue().Strip(wxString::both).empty());
}

const ProjectAsset* StoryLibraryPanel::SelectedPlace() const {
    const int selected = place_list_->GetSelection();
    return selected == wxNOT_FOUND || static_cast<std::size_t>(selected) >= visible_places_.size()
        ? nullptr : &places_[visible_places_[static_cast<std::size_t>(selected)]];
}

const ProjectAsset* StoryLibraryPanel::SelectedAudio() const {
    const int selected = audio_list_->GetSelection();
    return selected == wxNOT_FOUND || static_cast<std::size_t>(selected) >= visible_audio_.size()
        ? nullptr : &audio_[visible_audio_[static_cast<std::size_t>(selected)]];
}

void StoryLibraryPanel::RefreshPlacePreview() {
    place_preview_->SetBitmap(wxNullBitmap); place_preview_->Hide();
    const auto* asset = SelectedPlace();
    set_background_->Enable(asset != nullptr); show_image_->Enable(asset != nullptr);
    if (!asset) { place_preview_text_->SetLabel("No matching project images."); Layout(); return; }
    place_preview_text_->SetLabel(wxString::FromUTF8(asset->relative_path));
    const wxString absolute_path = wxString::FromUTF8(asset->absolute_path);
    if (!wxFileName::FileExists(absolute_path)) {
        place_preview_text_->SetLabel("Preview unavailable · the image file has moved or was deleted");
        Layout(); return;
    }
    wxImage loaded(absolute_path);
    if (loaded.IsOk()) {
        const double scale = std::min(260.0 / loaded.GetWidth(), 140.0 / loaded.GetHeight());
        if (scale < 1.0) loaded.Rescale(static_cast<int>(loaded.GetWidth() * scale),
                                        static_cast<int>(loaded.GetHeight() * scale), wxIMAGE_QUALITY_HIGH);
        place_preview_->SetBitmap(wxBitmap(loaded)); place_preview_->Show();
    } else {
        place_preview_text_->SetLabel("Preview unavailable · " + wxString::FromUTF8(asset->relative_path));
    }
    Layout();
}

void StoryLibraryPanel::RefreshAudioPreview() {
    audio_preview_->Stop(); preview_audio_->SetLabel("Preview audio");
    const auto* asset = SelectedAudio();
    play_music_->Enable(asset != nullptr); play_sound_->Enable(asset != nullptr);
    if (!asset) {
        audio_preview_text_->SetLabel("No matching project audio."); preview_audio_->Enable(false); Layout(); return;
    }
    audio_preview_text_->SetLabel(wxString::FromUTF8(asset->relative_path));
    const wxString absolute_path = wxString::FromUTF8(asset->absolute_path);
    const bool loaded = wxFileName::FileExists(absolute_path) && audio_preview_->Load(absolute_path);
    preview_audio_->Enable(loaded);
    if (!loaded) audio_preview_text_->SetLabel("Preview unavailable · the audio file has moved or was deleted");
    Layout();
}

void StoryLibraryPanel::InsertDialogue() {
    const int selected = character_list_->GetSelection();
    if (selected == wxNOT_FOUND || static_cast<std::size_t>(selected) >= visible_characters_.size()) return;
    const auto& character = characters_[visible_characters_[static_cast<std::size_t>(selected)]];
    const std::string indentation = indentation_provider_ ? indentation_provider_() : std::string{};
    const auto result = build_story_element({StoryElementKind::Dialogue, character.alias,
        dialogue_->GetValue().ToStdString(wxConvUTF8), {}, indentation});
    if (!result.valid || !insert_handler_) return;
    insert_handler_(result.text, "Dialogue added — Undo is available");
    dialogue_->Clear(); dialogue_->SetFocus();
}

void StoryLibraryPanel::InsertAsset(AssetInsertAction action) {
    const ProjectAsset* asset = action == AssetInsertAction::Scene || action == AssetInsertAction::Show
        ? SelectedPlace() : SelectedAudio();
    if (!asset || !insert_handler_) return;
    const std::string indentation = indentation_provider_ ? indentation_provider_() : std::string{};
    const std::string text = indentation + asset_insert_statement(*asset, action) + "\n";
    const std::string message = action == AssetInsertAction::Scene ? "Background added — Undo is available" :
        action == AssetInsertAction::Show ? "Image added — Undo is available" :
        action == AssetInsertAction::PlayMusic ? "Music added — Undo is available" : "Sound added — Undo is available";
    insert_handler_(text, message);
}

}  // namespace say_count::ui
