#pragma once

#include <functional>
#include <string>
#include <vector>

#include <wx/panel.h>

#include "core/assets.h"
#include "core/parser.h"

class wxButton;
class wxListBox;
class wxMediaCtrl;
class wxStaticBitmap;
class wxStaticText;
class wxTextCtrl;

namespace say_count::ui {

class StoryLibraryPanel final : public wxPanel {
public:
    explicit StoryLibraryPanel(wxWindow* parent);

    void SetLibrary(const ScriptAnalysis& analysis, std::vector<ProjectAsset> assets);
    void SetInsertHandler(std::function<void(const std::string&, const std::string&)> handler);
    void SetAddCharacterHandler(std::function<void()> handler);
    void SetIndentationProvider(std::function<std::string()> provider);

private:
    struct Character { std::string alias; std::string name; };

    void RefreshCharacters();
    void RefreshPlaces();
    void RefreshAudio();
    void RefreshCharacterActions();
    void RefreshPlacePreview();
    void RefreshAudioPreview();
    void InsertDialogue();
    void InsertAsset(AssetInsertAction action);
    const ProjectAsset* SelectedPlace() const;
    const ProjectAsset* SelectedAudio() const;

    std::vector<Character> characters_;
    std::vector<ProjectAsset> places_;
    std::vector<ProjectAsset> audio_;
    std::vector<std::size_t> visible_characters_;
    std::vector<std::size_t> visible_places_;
    std::vector<std::size_t> visible_audio_;
    wxTextCtrl* search_ = nullptr;
    wxListBox* character_list_ = nullptr;
    wxStaticText* character_help_ = nullptr;
    wxTextCtrl* dialogue_ = nullptr;
    wxButton* insert_dialogue_ = nullptr;
    wxListBox* place_list_ = nullptr;
    wxStaticText* place_preview_text_ = nullptr;
    wxStaticBitmap* place_preview_ = nullptr;
    wxButton* set_background_ = nullptr;
    wxButton* show_image_ = nullptr;
    wxListBox* audio_list_ = nullptr;
    wxStaticText* audio_preview_text_ = nullptr;
    wxMediaCtrl* audio_preview_ = nullptr;
    wxButton* preview_audio_ = nullptr;
    wxButton* play_music_ = nullptr;
    wxButton* play_sound_ = nullptr;
    std::function<void(const std::string&, const std::string&)> insert_handler_;
    std::function<void()> add_character_handler_;
    std::function<std::string()> indentation_provider_;
};

}  // namespace say_count::ui
