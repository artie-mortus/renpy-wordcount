#pragma once

#include <functional>
#include <string>
#include <utility>
#include <vector>

#include <wx/scrolwin.h>

#include "core/assets.h"
#include "core/parser.h"
#include "core/script_builder.h"

class wxButton;
class wxBoxSizer;
class wxComboBox;
class wxNotebook;
class wxStaticText;
class wxTextCtrl;
class wxToggleButton;

namespace say_count::ui {

class BuildScenePanel final : public wxScrolledWindow {
public:
    explicit BuildScenePanel(wxWindow* parent);

    void SetProject(const ScriptAnalysis& analysis, const std::vector<ProjectAsset>& assets);
    void SetInsertHandler(
        std::function<void(StoryElementKind, const std::string&, const std::string&)> handler);
    void SetIndentationProvider(std::function<std::string()> provider);
    void RefreshContext();

private:
    struct CueButton {
        StoryElementKind kind;
        wxToggleButton* button = nullptr;
    };

    void AddCueGroup(wxNotebook* book, const wxString& label,
                     const std::vector<std::pair<StoryElementKind, wxString>>& cues);
    void SelectKind(StoryElementKind kind);
    void RefreshForm();
    void RefreshSuggestions();
    void InsertCue();
    void ClearFields(bool keep_primary = false);
    std::vector<std::string> SuggestionsFor(StoryElementKind kind) const;

    StoryElementKind selected_kind_ = StoryElementKind::Dialogue;
    std::vector<CueButton> cue_buttons_;
    std::vector<std::string> characters_;
    std::vector<std::string> images_;
    std::vector<std::string> audio_;
    std::vector<std::string> labels_;
    wxStaticText* guidance_ = nullptr;
    wxStaticText* primary_label_ = nullptr;
    wxStaticText* secondary_label_ = nullptr;
    wxStaticText* details_label_ = nullptr;
    wxComboBox* primary_ = nullptr;
    wxTextCtrl* secondary_ = nullptr;
    wxTextCtrl* details_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxButton* insert_ = nullptr;
    std::function<void(StoryElementKind, const std::string&, const std::string&)> insert_handler_;
    std::function<std::string()> indentation_provider_;
};

}  // namespace say_count::ui
