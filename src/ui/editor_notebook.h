#pragma once

#include <wx/aui/auibook.h>
#include <wx/string.h>
#include <wx/timer.h>

#include <functional>
#include <optional>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include "app/settings.h"
#include "core/parser.h"
#include "core/autocomplete.h"
#include "core/find_replace.h"
#include "core/diagnostics.h"
#include "core/editor_commands.h"
#include "core/project.h"
#include "core/workspace.h"
#include "core/manuscript.h"
#include "core/vim.h"

class wxStyledTextCtrl;
class wxStyledTextEvent;
class wxKeyEvent;
class wxPanel;

namespace say_count::ui {

struct FindStatus {
    bool valid = true;
    std::size_t current = 0;
    std::size_t total = 0;
};

struct DocumentState {
    wxString path;
    wxString name;
    bool dirty = false;
};

enum class ExternalFileResult { NotOpen, Unchanged, Reloaded, Dirty, Missing, Failed };
enum class ManuscriptConversionScope { None, AlreadyRenpy, Document, Selection };
struct ManuscriptEditorPreview {
    int start = 0;
    int end = 0;
    bool selection = false;
    std::string source;
    std::vector<ManuscriptLineReview> lines;
    ManuscriptConversion safe_conversion;
    ManuscriptConversion inclusive_conversion;
};
struct TextReplacementPreview {
    int start = 0;
    int end = 0;
    bool selection = false;
    std::string source;
};
struct ExternalFileUpdate {
    ExternalFileResult result = ExternalFileResult::NotOpen;
    std::string local_content;
    std::string disk_content;
};

class EditorNotebook final : public wxAuiNotebook {
public:
    using AnalysisHandler = std::function<void(const wxString&, const ScriptAnalysis&)>;
    using FindStatusHandler = std::function<void(const FindStatus&)>;
    using DiagnosticsHandler = std::function<void(const std::vector<Diagnostic>&)>;
    using DocumentStateHandler = std::function<void(const DocumentState&)>;
    using NvimModeHandler = std::function<void(bool enabled, const std::string& mode,
                                                const std::string& command_line)>;

    EditorNotebook(wxWindow* parent, const app::EditorSettings& settings,
                   AnalysisHandler analysis_handler);

    void NewTab();
    bool OpenFiles(const std::vector<wxString>& paths, bool focus_existing = true);
    bool OpenProjectFiles(const std::vector<wxString>& paths);
    bool SaveCurrent();
    bool SaveCurrentAs();
    void CloseCurrentTab();
    bool ConfirmCloseAll();
    void SetWordWrap(bool enabled);
    void SetFontSize(int size);
    void SetTheme(app::EditorTheme theme);
    void SetNvimMotions(bool enabled);
    void SetNvimModeHandler(NvimModeHandler handler);
    bool ExitNvimMode();
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
    bool RestoreProjectScripts(const std::vector<NamedScript>& scripts);
    std::size_t CurrentFileIndex() const;
    void SelectFileIndex(std::size_t index);
    void SetCountMenuChoices(bool enabled);
    wxString CurrentFilePath() const;
    wxString CurrentFileName() const;
    std::string CurrentText() const;
    bool HasMeaningfulContent() const;
    std::size_t CurrentLine() const;
    wxString CurrentIndentation() const;
    bool SaveAll();
    bool OpenAndJump(const wxString& path, std::size_t line);
    void InsertAtCaret(std::string_view text);
    void InsertStoryElement(std::string_view text);
    void InsertTopLevelDefinition(std::string_view text);
    std::optional<ManuscriptEditorPreview> PrepareManuscriptConversion() const;
    std::optional<TextReplacementPreview> PrepareTextReplacement() const;
    bool ApplyTextReplacement(const TextReplacementPreview& preview, std::string_view replacement);
    bool ReplaceCurrentDocument(std::string_view replacement);
    bool PrepareOfflineAiConversion(ManuscriptEditorPreview* preview,
                                    std::string_view rewritten_manuscript) const;
    bool ApplyManuscriptConversion(const ManuscriptEditorPreview& preview,
                                   bool include_uncertain_lines);
    void SelectProjectMatch(const ProjectFindMatch& match);
    void ClearFind();
    void SetFindStatusHandler(FindStatusHandler handler);
    void SetDiagnosticsHandler(DiagnosticsHandler handler);
    void SetDocumentStateHandler(DocumentStateHandler handler);
    void SelectDiagnostic(const Diagnostic& diagnostic);
    void ToggleComments();
    ExternalFileUpdate ReloadExternalFile(const wxString& path);
    bool ApplyExternalVersion(const wxString& path, std::string_view content, bool mark_clean);
    std::vector<WorkspaceFileState> CaptureWorkspaceFiles() const;
    void RestoreWorkspaceViews(const std::vector<WorkspaceFileState>& files,
                               std::string_view active_file);

private:
    wxStyledTextCtrl* EditorAt(size_t index) const;
    wxString FilePath(wxStyledTextCtrl* editor) const;
    void SetFilePath(wxStyledTextCtrl* editor, const wxString& path);
    bool SaveEditor(wxStyledTextCtrl* editor, const wxString& path);
    bool ConfirmDiscard(size_t index);
    void ConfigureEditor(wxStyledTextCtrl* editor);
    void CreateEmptyHint(wxStyledTextCtrl* editor);
    void UpdateEmptyHint(wxStyledTextCtrl* editor);
    void PositionEmptyHint(wxStyledTextCtrl* editor);
    void UpdateTabLabel(wxStyledTextCtrl* editor);
    void OnPageClose(wxAuiNotebookEvent& event);
    void OnPageClosed(wxAuiNotebookEvent& event);
    void OnSavePointChanged(wxStyledTextEvent& event);
    void OnStyleNeeded(wxStyledTextEvent& event);
    void OnModified(wxStyledTextEvent& event);
    void OnCharAdded(wxStyledTextEvent& event);
    void OnAutoCompCompleted(wxStyledTextEvent& event);
    void OnDwellStart(wxStyledTextEvent& event);
    void OnDwellEnd(wxStyledTextEvent& event);
    void OnKeyDown(wxKeyEvent& event);
    bool HandleNvimKey(wxStyledTextCtrl* editor, wxKeyEvent& event);
    void ApplyNvimState(wxStyledTextCtrl* editor, const VimState& state);
    void NotifyNvimMode();
    void OnPageChanged(wxAuiNotebookEvent& event);
    void OnAnalysisTimer(wxTimerEvent& event);
    void OnCompletionTimer(wxTimerEvent& event);
    void AnalyzeActive();
    bool RefreshSpeakers(wxStyledTextCtrl* editor);
    void DropEditorState(wxStyledTextCtrl* editor);
    void RefreshCompletionIndex();
    void RefreshCompletionDocument(wxStyledTextCtrl* editor);
    void MergeCompletionIndex();
    FindStatus RefreshFindHighlights();
    void RefreshDiagnostics();
    void ApplyDiagnostics();
    void ApplyCommandResult(wxStyledTextCtrl* editor, const EditorCommandResult& result);
    void NotifyDocumentState();

    unsigned int next_untitled_number_ = 1;
    app::EditorSettings settings_;
    AnalysisHandler analysis_handler_;
    wxTimer analysis_timer_;
    wxTimer completion_timer_;
    std::unordered_map<wxStyledTextCtrl*, std::unordered_set<std::string>> speakers_;
    std::unordered_map<wxStyledTextCtrl*, std::unordered_map<int, std::string>> speaker_lines_;
    std::unordered_map<wxStyledTextCtrl*, CompletionResult> completions_;
    std::unordered_map<wxStyledTextCtrl*, wxPanel*> empty_hints_;
    CompletionIndex completion_index_;
    std::unordered_map<wxStyledTextCtrl*, CompletionIndex> completion_indexes_;
    std::unordered_set<wxStyledTextCtrl*> pending_completion_editors_;
    std::string find_query_;
    FindOptions find_options_;
    FindStatusHandler find_status_handler_;
    std::vector<Diagnostic> diagnostics_;
    DiagnosticsHandler diagnostics_handler_;
    DocumentStateHandler document_state_handler_;
    NvimModeHandler nvim_mode_handler_;
    std::unordered_map<wxStyledTextCtrl*, VimEmulator> vim_emulators_;
    std::unordered_map<wxStyledTextCtrl*, std::string> nvim_modes_;
    std::string nvim_command_line_;
    bool opening_files_ = false;
    bool count_menu_choices_ = false;
};

}  // namespace say_count::ui
