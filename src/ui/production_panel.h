#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include <wx/panel.h>

#include "core/accessibility.h"
#include "core/continuity.h"
#include "core/parser.h"
#include "core/prose.h"
#include "core/translation.h"
#include "core/voice.h"

class wxCheckBox;
class wxChoice;
class wxListCtrl;
class wxNotebook;
class wxStaticText;
class wxTextCtrl;

namespace say_count::ui {

class ProductionPanel final : public wxPanel {
public:
    using JumpHandler = std::function<void(const std::string&, std::size_t)>;
    using SearchHandler = std::function<void(const std::string&)>;

    ProductionPanel(wxWindow* parent, std::string data_directory);
    void SetProject(std::vector<NamedScript> scripts, ScriptAnalysis analysis,
                    std::string game_directory, std::string active_file,
                    std::size_t active_line);
    void SetJumpHandler(JumpHandler handler) { jump_handler_ = std::move(handler); }
    void SetSearchHandler(SearchHandler handler) { search_handler_ = std::move(handler); }

private:
    wxPanel* BuildProse();
    wxPanel* BuildVoice();
    wxPanel* BuildTranslations();
    wxPanel* BuildContinuity();
    wxPanel* BuildAccessibility();
    void RefreshProse();
    void RefreshVoice();
    void RefreshTranslations();
    void RefreshContinuity();
    void RefreshAccessibility();
    void SaveVoiceSelection();
    void ExportVoice(bool html);

    std::vector<NamedScript> scripts_;
    ScriptAnalysis analysis_;
    std::string game_directory_;
    std::string active_file_;
    std::size_t active_line_ = 0;
    JumpHandler jump_handler_;
    SearchHandler search_handler_;
    VoiceStore voice_store_;
    ContinuityStore continuity_store_;
    AccessibilityAcknowledgementStore accessibility_store_;
    std::map<std::string, VoiceEntry> voice_entries_;
    std::vector<VoiceLine> voice_rows_;
    std::vector<ContinuityNote> continuity_notes_;
    std::vector<ContinuityNote> visible_notes_;
    std::set<std::string> acknowledged_;
    std::vector<AccessibilityIssue> accessibility_issues_;
    TranslationDashboard translation_;
    std::vector<ProseFrequency> prose_terms_;

    wxNotebook* notebook_ = nullptr;
    wxStaticText* prose_summary_ = nullptr;
    wxChoice* prose_speaker_ = nullptr;
    wxTextCtrl* prose_exclusions_ = nullptr;
    wxListCtrl* prose_list_ = nullptr;
    wxStaticText* voice_summary_ = nullptr;
    wxListCtrl* voice_list_ = nullptr;
    wxChoice* voice_status_ = nullptr;
    wxTextCtrl* voice_file_ = nullptr;
    wxTextCtrl* voice_notes_ = nullptr;
    wxChoice* voice_export_speaker_ = nullptr;
    wxCheckBox* voice_source_context_ = nullptr;
    wxTextCtrl* translation_language_ = nullptr;
    wxStaticText* translation_summary_ = nullptr;
    wxListCtrl* translation_list_ = nullptr;
    wxChoice* continuity_kind_ = nullptr;
    wxTextCtrl* continuity_subject_ = nullptr;
    wxTextCtrl* continuity_note_ = nullptr;
    wxCheckBox* continuity_attach_ = nullptr;
    wxTextCtrl* continuity_search_ = nullptr;
    wxChoice* continuity_filter_kind_ = nullptr;
    wxListCtrl* continuity_list_ = nullptr;
    wxListCtrl* accessibility_list_ = nullptr;
};

}  // namespace say_count::ui
