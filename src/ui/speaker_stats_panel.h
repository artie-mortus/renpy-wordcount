#pragma once

#include <map>
#include <string>
#include <functional>

#include <wx/scrolwin.h>
#include <wx/string.h>

#include "core/parser.h"
#include "core/project_bundle.h"

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
    std::pair<std::map<std::string, ProjectTarget>, std::map<std::string, ProjectTarget>>
        ExportTargets() const;
    ProjectTarget ExportProjectTarget() const;
    void ImportTargets(long project_words, long project_lines,
                       const std::map<std::string, ProjectTarget>& speakers,
                       const std::map<std::string, ProjectTarget>& scenes);

private:
    struct Targets { long words = 0; long lines = 0; };

    void Rebuild();
    void SaveTargets() const;
    void LoadTargets();
    Targets ReadTargets(wxTextCtrl* words, wxTextCtrl* lines) const;
    void RefreshCountedLines();
    void RefreshVersionComparison();

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
    wxTextCtrl* version_input_ = nullptr;
    wxTextCtrl* version_result_ = nullptr;
    wxString version_source_;
    std::function<void(std::size_t)> line_jump_handler_;
};

}  // namespace say_count::ui
