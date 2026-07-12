#pragma once

#include <map>
#include <string>
#include <functional>

#include <wx/scrolwin.h>
#include <wx/string.h>

#include "core/parser.h"

class wxBoxSizer;
class wxTextCtrl;
class wxChoice;
class wxListCtrl;

namespace say_count::ui {

class SpeakerStatsPanel final : public wxScrolledWindow {
public:
    SpeakerStatsPanel(wxWindow* parent, wxString targets_path);
    void SetAnalysis(const ScriptAnalysis& analysis);
    void SetLineJumpHandler(std::function<void(std::size_t)> handler);

private:
    struct Targets { long words = 0; long lines = 0; };

    void Rebuild();
    void SaveTargets() const;
    void LoadTargets();
    Targets ReadTargets(wxTextCtrl* words, wxTextCtrl* lines) const;
    void RefreshCountedLines();

    wxString targets_path_;
    ScriptAnalysis analysis_;
    long project_words_ = 0;
    long project_lines_ = 0;
    std::map<std::string, Targets> speaker_targets_;
    std::map<std::string, Targets> scene_targets_;
    wxBoxSizer* content_ = nullptr;
    wxChoice* speaker_filter_ = nullptr;
    wxTextCtrl* text_filter_ = nullptr;
    wxTextCtrl* label_filter_ = nullptr;
    wxListCtrl* counted_lines_ = nullptr;
    std::function<void(std::size_t)> line_jump_handler_;
};

}  // namespace say_count::ui
