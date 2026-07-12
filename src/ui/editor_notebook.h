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

class wxStyledTextCtrl;
class wxStyledTextEvent;

namespace say_count::ui {

class EditorNotebook final : public wxAuiNotebook {
public:
    using AnalysisHandler = std::function<void(const wxString&, const ScriptAnalysis&)>;

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

    unsigned int next_untitled_number_ = 1;
    app::EditorSettings settings_;
    AnalysisHandler analysis_handler_;
    wxTimer analysis_timer_;
    std::unordered_map<wxStyledTextCtrl*, std::unordered_set<std::string>> speakers_;
    std::unordered_map<wxStyledTextCtrl*, CompletionResult> completions_;
    CompletionIndex completion_index_;
};

}  // namespace say_count::ui
