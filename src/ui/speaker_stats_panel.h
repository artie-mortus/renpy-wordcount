#pragma once

#include <map>
#include <string>

#include <wx/scrolwin.h>
#include <wx/string.h>

#include "core/parser.h"

class wxBoxSizer;
class wxTextCtrl;

namespace say_count::ui {

class SpeakerStatsPanel final : public wxScrolledWindow {
public:
    SpeakerStatsPanel(wxWindow* parent, wxString targets_path);
    void SetAnalysis(const ScriptAnalysis& analysis);

private:
    struct Targets { long words = 0; long lines = 0; };

    void Rebuild();
    void SaveTargets() const;
    void LoadTargets();
    Targets ReadTargets(wxTextCtrl* words, wxTextCtrl* lines) const;

    wxString targets_path_;
    ScriptAnalysis analysis_;
    long project_words_ = 0;
    long project_lines_ = 0;
    std::map<std::string, Targets> speaker_targets_;
    std::map<std::string, Targets> scene_targets_;
    wxBoxSizer* content_ = nullptr;
};

}  // namespace say_count::ui
