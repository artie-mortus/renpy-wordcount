#pragma once

#include <wx/aui/auibook.h>
#include <wx/string.h>
#include <wx/timer.h>

#include <functional>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include "app/settings.h"
#include "core/parser.h"
#include "core/autocomplete.h"
#include "core/find_replace.h"

class wxStyledTextCtrl;
class wxStyledTextEvent;

namespace say_count::ui {

struct FindStatus {
    bool valid = true;
    std::size_t current = 0;
    std::size_t total = 0;
};

class EditorNotebook final : public wxAuiNotebook {
public:
    using AnalysisHandler = std::function<void(const wxString&, const ScriptAnalysis&)>;
    using FindStatusHandler = std::function<void(const FindStatus&)>;

    EditorNotebook(wxWindow* parent, const app::EditorSettings& settings,
                   AnalysisHandler analysis_handler);

    void NewTab();
    bool OpenFiles(const std::vector<wxString>& paths);
    bool SaveCurrent();
    bool SaveCurrentAs();
    void CloseCurrentTab();
    bool ConfirmCloseAll();
    void SetWordWrap(bool enabled);
    void SetFontSize(int size);
    void SetTheme(app::EditorTheme theme);
    void JumpToLine(std::size_t line_number);
    std::string SelectedText() const;
    FindStatus SetFindQuery(std::string query, FindOptions options);
    FindStatus FindNext(int direction);
    FindStatus FindNextAcrossFiles(int direction);
    bool ReplaceCurrent(std::string_view replacement, bool across_files = false);
    std::size_t ReplaceAll(std::string_view replacement);
    std::size_t ReplaceAllAcrossFiles(const std::vector<std::size_t>& selected_files,
                                      std::string_view replacement);
    std::vector<NamedScript> ProjectScripts() const;
    void SelectProjectMatch(const ProjectFindMatch& match);
    void ClearFind();
    void SetFindStatusHandler(FindStatusHandler handler);

private:
    wxStyledTextCtrl* EditorAt(size_t index) const;
    wxString FilePath(wxStyledTextCtrl* editor) const;
    void SetFilePath(wxStyledTextCtrl* editor, const wxString& path);
    bool SaveEditor(wxStyledTextCtrl* editor, const wxString& path);
    bool ConfirmDiscard(size_t index);
    void ConfigureEditor(wxStyledTextCtrl* editor);
    void UpdateTabLabel(wxStyledTextCtrl* editor);
    void OnPageClose(wxAuiNotebookEvent& event);
    void OnPageClosed(wxAuiNotebookEvent& event);
    void OnSavePointChanged(wxStyledTextEvent& event);
    void OnStyleNeeded(wxStyledTextEvent& event);
    void OnModified(wxStyledTextEvent& event);
    void OnCharAdded(wxStyledTextEvent& event);
    void OnAutoCompCompleted(wxStyledTextEvent& event);
    void OnPageChanged(wxAuiNotebookEvent& event);
    void OnAnalysisTimer(wxTimerEvent& event);
    void AnalyzeActive();
    void RefreshSpeakers(wxStyledTextCtrl* editor);
    void RefreshCompletionIndex();
    FindStatus RefreshFindHighlights();

    unsigned int next_untitled_number_ = 1;
    app::EditorSettings settings_;
    AnalysisHandler analysis_handler_;
    wxTimer analysis_timer_;
    std::unordered_map<wxStyledTextCtrl*, std::unordered_set<std::string>> speakers_;
    std::unordered_map<wxStyledTextCtrl*, CompletionResult> completions_;
    CompletionIndex completion_index_;
    std::string find_query_;
    FindOptions find_options_;
    FindStatusHandler find_status_handler_;
};

}  // namespace say_count::ui
