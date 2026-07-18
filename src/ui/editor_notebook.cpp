#include "ui/editor_notebook.h"
#include "core/parser.h"
#include "core/tokenizer.h"
#include "core/autocomplete.h"
#include "core/editor_commands.h"
#include "core/indent.h"
#include "core/manuscript.h"
#include "ui/style.h"

#include <algorithm>

#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <wx/settings.h>
#include <wx/aui/tabart.h>

namespace say_count::ui {
namespace {

constexpr const char* kFilePathProperty = "say-count-file-path";
constexpr int kAnalysisDelayMs = 120;
constexpr int kCompletionDelayMs = 300;
constexpr int kFindIndicator = 8;
constexpr int kDiagnosticErrorIndicator = 9;
constexpr int kDiagnosticWarningIndicator = 10;
constexpr int kDiagnosticNoticeIndicator = 11;
constexpr int kDiagnosticErrorMarker = 20;
constexpr int kDiagnosticWarningMarker = 21;
constexpr int kDiagnosticNoticeMarker = 22;
constexpr std::size_t kMaxHighlightedMatches = 2000;
enum EditorStyle { kDefault, kComment, kKeyword, kLabel, kSpeaker, kString, kPython, kStatement };

wxString NormalizeTabs(wxString text) {
    text.Replace("\t", "    ");
    return text;
}

}  // namespace

EditorNotebook::EditorNotebook(wxWindow* parent, const app::EditorSettings& settings,
                               AnalysisHandler analysis_handler)
    : wxAuiNotebook(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                    wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ALL_TABS),
      settings_(settings), analysis_handler_(std::move(analysis_handler)), analysis_timer_(this),
      completion_timer_(this) {
    auto* tabs = new wxAuiDefaultTabArt();
    tabs->SetColour(style::Colors().canvas);
    tabs->SetActiveColour(style::Colors().paper);
    SetArtProvider(tabs);
    SetTabCtrlHeight(36);
    Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &EditorNotebook::OnPageClose, this);
    Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSED, &EditorNotebook::OnPageClosed, this);
    Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, &EditorNotebook::OnPageChanged, this);
    Bind(wxEVT_TIMER, &EditorNotebook::OnAnalysisTimer, this, analysis_timer_.GetId());
    Bind(wxEVT_TIMER, &EditorNotebook::OnCompletionTimer, this, completion_timer_.GetId());
    NewTab();
}

void EditorNotebook::NewTab() {
    auto* editor = new wxStyledTextCtrl(this, wxID_ANY);
    ConfigureEditor(editor);
    const wxString title = wxString::Format("scene-%u.rpy", next_untitled_number_++);
    editor->SetName(title);
    editor->SetSavePoint();
    RefreshSpeakers(editor);
    editor->Bind(wxEVT_STC_SAVEPOINTLEFT, &EditorNotebook::OnSavePointChanged, this);
    editor->Bind(wxEVT_STC_SAVEPOINTREACHED, &EditorNotebook::OnSavePointChanged, this);
    AddPage(editor, title, true);
    RefreshCompletionDocument(editor);
    editor->SetFocus();
    AnalyzeActive();
}

wxString EditorNotebook::FilePath(wxStyledTextCtrl* editor) const {
    return editor ? editor->GetProperty(kFilePathProperty) : wxString{};
}

void EditorNotebook::SetFilePath(wxStyledTextCtrl* editor, const wxString& path) {
    editor->SetProperty(kFilePathProperty, path);
    editor->SetName(wxFileName(path).GetFullName());
    UpdateTabLabel(editor);
}

bool EditorNotebook::OpenFiles(const std::vector<wxString>& paths, bool focus_existing) {
    bool opened = false;
    for (const auto& path : paths) {
        const wxString absolute_path = wxFileName(path).GetAbsolutePath();
        int existing = wxNOT_FOUND;
        for (size_t index = 0; index < GetPageCount(); ++index) {
            if (FilePath(EditorAt(index)) == absolute_path) {
                existing = static_cast<int>(index);
                break;
            }
        }
        if (existing != wxNOT_FOUND) {
            if (focus_existing) SetSelection(static_cast<size_t>(existing));
            opened = true;
            continue;
        }

        wxFile file(absolute_path);
        wxString content;
        if (!file.IsOpened() || !file.ReadAll(&content, wxConvUTF8)) {
            wxMessageBox(wxString::Format("Could not read %s.", absolute_path), "Open failed",
                         wxOK | wxICON_ERROR, this);
            continue;
        }

        auto* editor = new wxStyledTextCtrl(this, wxID_ANY);
        ConfigureEditor(editor);
        editor->SetText(NormalizeTabs(content));
        RefreshSpeakers(editor);
        SetFilePath(editor, absolute_path);
        editor->SetSavePoint();
        editor->Bind(wxEVT_STC_SAVEPOINTLEFT, &EditorNotebook::OnSavePointChanged, this);
        editor->Bind(wxEVT_STC_SAVEPOINTREACHED, &EditorNotebook::OnSavePointChanged, this);
        AddPage(editor, editor->GetName(), true);
        RefreshCompletionDocument(editor);
        editor->SetFocus();
        AnalyzeActive();
        opened = true;
    }
    if (opened && GetPageCount() > 1) {
        for (size_t index = GetPageCount(); index-- > 0;) {
            auto* editor = EditorAt(index);
            if (editor && FilePath(editor).empty() && !editor->GetModify() && editor->GetText().empty()) {
                DropEditorState(editor);
                DeletePage(index);
            }
        }
        MergeCompletionIndex();
        AnalyzeActive();
    }
    return opened;
}

bool EditorNotebook::OpenProjectFiles(const std::vector<wxString>& paths) {
    if (paths.empty()) return true;
    // Project refresh must not steal focus from the tab the user is editing.
    const bool opened = OpenFiles(paths, false);
    if (!opened) return false;
    bool removed = false;
    for (size_t index = GetPageCount(); index-- > 0;) {
        auto* editor = EditorAt(index);
        if (GetPageCount() > 1 && editor && FilePath(editor).empty() && !editor->GetModify() &&
            editor->GetText().empty()) {
            DropEditorState(editor);
            DeletePage(index);
            removed = true;
        }
    }
    if (removed) {
        MergeCompletionIndex();
        AnalyzeActive();
    }
    return true;
}

ExternalFileUpdate EditorNotebook::ReloadExternalFile(const wxString& path) {
    const wxString absolute_path = wxFileName(path).GetAbsolutePath();
    wxStyledTextCtrl* editor = nullptr;
    for (size_t index = 0; index < GetPageCount(); ++index) {
        auto* candidate = EditorAt(index);
        if (candidate && FilePath(candidate) == absolute_path) { editor = candidate; break; }
    }
    if (!editor) return {ExternalFileResult::NotOpen, {}, {}};
    const std::string local = editor->GetText().ToStdString(wxConvUTF8);
    if (!wxFileName::FileExists(absolute_path)) return {ExternalFileResult::Missing, local, {}};
    wxFile file(absolute_path);
    wxString content;
    if (!file.IsOpened() || !file.ReadAll(&content, wxConvUTF8))
        return {ExternalFileResult::Failed, local, {}};
    content = NormalizeTabs(content);
    const std::string disk = content.ToStdString(wxConvUTF8);
    const auto decision = classify_external_change(local, disk, editor->GetModify());
    if (decision == ExternalChangeDecision::Unchanged)
        return {ExternalFileResult::Unchanged, local, disk};
    if (decision == ExternalChangeDecision::Conflict)
        return {ExternalFileResult::Dirty, local, disk};

    const int selection_start = editor->GetSelectionStart();
    const int selection_end = editor->GetSelectionEnd();
    const int first_visible = editor->GetFirstVisibleLine();
    editor->SetText(content);
    editor->SetSelection(std::min(selection_start, editor->GetTextLength()),
                         std::min(selection_end, editor->GetTextLength()));
    editor->SetFirstVisibleLine(std::min(first_visible, editor->GetLineCount() - 1));
    editor->SetSavePoint();
    RefreshSpeakers(editor);
    editor->Colourise(0, -1);
    RefreshCompletionDocument(editor);
    analysis_timer_.StartOnce(kAnalysisDelayMs);
    return {ExternalFileResult::Reloaded, local, disk};
}

bool EditorNotebook::ApplyExternalVersion(const wxString& path, std::string_view content,
                                          bool mark_clean) {
    const wxString absolute_path = wxFileName(path).GetAbsolutePath();
    wxStyledTextCtrl* editor = nullptr;
    for (size_t index = 0; index < GetPageCount(); ++index) {
        auto* candidate = EditorAt(index);
        if (candidate && FilePath(candidate) == absolute_path) { editor = candidate; break; }
    }
    if (!editor) return false;
    const int selection_start = editor->GetSelectionStart();
    const int selection_end = editor->GetSelectionEnd();
    const int first_visible = editor->GetFirstVisibleLine();
    editor->SetText(NormalizeTabs(wxString::FromUTF8(content.data(), content.size())));
    editor->SetSelection(std::min(selection_start, editor->GetTextLength()),
                         std::min(selection_end, editor->GetTextLength()));
    editor->SetFirstVisibleLine(std::min(first_visible, editor->GetLineCount() - 1));
    if (mark_clean) editor->SetSavePoint();
    RefreshSpeakers(editor);
    editor->Colourise(0, -1);
    RefreshCompletionDocument(editor);
    analysis_timer_.StartOnce(kAnalysisDelayMs);
    return true;
}

void EditorNotebook::ConfigureEditor(wxStyledTextCtrl* editor) {
    nvim_normal_modes_.try_emplace(editor, settings_.nvim_motions);
    editor->SetLexer(wxSTC_LEX_CONTAINER);
    editor->Unbind(wxEVT_STC_STYLENEEDED, &EditorNotebook::OnStyleNeeded, this);
    editor->Bind(wxEVT_STC_STYLENEEDED, &EditorNotebook::OnStyleNeeded, this);
    editor->Unbind(wxEVT_STC_MODIFIED, &EditorNotebook::OnModified, this);
    editor->Bind(wxEVT_STC_MODIFIED, &EditorNotebook::OnModified, this);
    editor->Unbind(wxEVT_STC_CHARADDED, &EditorNotebook::OnCharAdded, this);
    editor->Bind(wxEVT_STC_CHARADDED, &EditorNotebook::OnCharAdded, this);
    editor->Unbind(wxEVT_STC_AUTOCOMP_COMPLETED, &EditorNotebook::OnAutoCompCompleted, this);
    editor->Bind(wxEVT_STC_AUTOCOMP_COMPLETED, &EditorNotebook::OnAutoCompCompleted, this);
    editor->Unbind(wxEVT_STC_DWELLSTART, &EditorNotebook::OnDwellStart, this);
    editor->Bind(wxEVT_STC_DWELLSTART, &EditorNotebook::OnDwellStart, this);
    editor->Unbind(wxEVT_STC_DWELLEND, &EditorNotebook::OnDwellEnd, this);
    editor->Bind(wxEVT_STC_DWELLEND, &EditorNotebook::OnDwellEnd, this);
    editor->Unbind(wxEVT_KEY_DOWN, &EditorNotebook::OnKeyDown, this);
    editor->Bind(wxEVT_KEY_DOWN, &EditorNotebook::OnKeyDown, this);
    editor->SetMouseDwellTime(350);
    editor->AutoCompSetIgnoreCase(true);
    editor->AutoCompSetMaxHeight(8);
    editor->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    editor->SetMarginWidth(0, editor->TextWidth(wxSTC_STYLE_LINENUMBER, "00000") + 16);
    editor->SetMarginType(1, wxSTC_MARGIN_SYMBOL);
    editor->SetMarginMask(1, (1 << kDiagnosticErrorMarker) |
                             (1 << kDiagnosticWarningMarker) |
                             (1 << kDiagnosticNoticeMarker));
    editor->SetMarginWidth(1, 18);
    editor->SetCaretLineVisible(true);
    editor->SetCaretLineVisibleAlways(true);
    editor->SetCaretWidth(2);
    editor->SetCaretStyle(settings_.nvim_motions && nvim_normal_modes_[editor]
        ? wxSTC_CARETSTYLE_BLOCK : wxSTC_CARETSTYLE_LINE);
    editor->SetExtraAscent(3);
    editor->SetExtraDescent(3);
    editor->SetMarginLeft(12);
    editor->SetMarginRight(20);
    editor->SetIndentationGuides(wxSTC_IV_LOOKBOTH);
    editor->SetWrapMode(settings_.word_wrap ? wxSTC_WRAP_WORD : wxSTC_WRAP_NONE);
    const bool dark = settings_.theme == app::EditorTheme::Dark ||
                      (settings_.theme == app::EditorTheme::System && wxSystemSettings::GetAppearance().IsDark());
    const wxColour foreground(dark ? "#E8ECF1" : "#243247");
    const wxColour background(dark ? "#111B2B" : "#FCFBF8");
    const wxColour margin(dark ? "#17243A" : "#EEF1F3");
    editor->StyleSetFont(wxSTC_STYLE_DEFAULT,
                         wxFont(wxFontInfo(settings_.font_size).Family(wxFONTFAMILY_TELETYPE)
                                .FaceName("DejaVu Sans Mono")));
    editor->StyleSetForeground(wxSTC_STYLE_DEFAULT, foreground);
    editor->StyleSetBackground(wxSTC_STYLE_DEFAULT, background);
    editor->StyleClearAll();
    editor->StyleSetForeground(kComment, dark ? wxColour("#7F91A8") : wxColour("#788494"));
    editor->StyleSetItalic(kComment, true);
    editor->StyleSetForeground(kKeyword, dark ? wxColour("#E69A83") : wxColour("#AB4F42"));
    editor->StyleSetBold(kKeyword, true);
    editor->StyleSetForeground(kLabel, dark ? wxColour("#E3BE68") : wxColour("#8A6518"));
    editor->StyleSetBold(kLabel, true);
    editor->StyleSetForeground(kSpeaker, dark ? wxColour("#6FC6B4") : wxColour("#28736A"));
    editor->StyleSetBold(kSpeaker, true);
    editor->StyleSetForeground(kString, dark ? wxColour("#C29AB8") : wxColour("#76546E"));
    editor->StyleSetForeground(kPython, dark ? wxColour("#B7A6E8") : wxColour("#6E58A5"));
    editor->StyleSetForeground(kStatement, dark ? wxColour("#86B4E0") : wxColour("#356A9C"));
    editor->StyleSetFont(wxSTC_STYLE_LINENUMBER, style::UtilityFont(std::max(8, settings_.font_size - 3)));
    editor->StyleSetForeground(wxSTC_STYLE_LINENUMBER, dark ? wxColour("#8190A4") : wxColour("#7A8796"));
    editor->StyleSetBackground(wxSTC_STYLE_LINENUMBER, margin);
    editor->SetCaretForeground(foreground);
    editor->SetCaretLineBackground(dark ? wxColour("#1C2C43") : wxColour("#F3F0E7"));
    editor->SetSelBackground(true, dark ? wxColour("#415979") : wxColour("#DCE8F3"));
    editor->IndicatorSetStyle(kFindIndicator, wxSTC_INDIC_ROUNDBOX);
    editor->IndicatorSetForeground(kFindIndicator, dark ? wxColour("#f2ca72") : wxColour("#d49a13"));
    editor->IndicatorSetAlpha(kFindIndicator, 80);
    editor->IndicatorSetOutlineAlpha(kFindIndicator, 180);
    editor->IndicatorSetStyle(kDiagnosticErrorIndicator, wxSTC_INDIC_SQUIGGLE);
    editor->IndicatorSetForeground(kDiagnosticErrorIndicator, wxColour("#d64545"));
    editor->IndicatorSetStyle(kDiagnosticWarningIndicator, wxSTC_INDIC_SQUIGGLE);
    editor->IndicatorSetForeground(kDiagnosticWarningIndicator, wxColour("#d49a13"));
    editor->IndicatorSetStyle(kDiagnosticNoticeIndicator, wxSTC_INDIC_SQUIGGLE);
    editor->IndicatorSetForeground(kDiagnosticNoticeIndicator, wxColour("#4186c9"));
    editor->MarkerDefine(kDiagnosticErrorMarker, wxSTC_MARK_CIRCLE, wxColour("#ffffff"), wxColour("#d64545"));
    editor->MarkerDefine(kDiagnosticWarningMarker, wxSTC_MARK_CIRCLE, wxColour("#ffffff"), wxColour("#d49a13"));
    editor->MarkerDefine(kDiagnosticNoticeMarker, wxSTC_MARK_CIRCLE, wxColour("#ffffff"), wxColour("#4186c9"));
    editor->Colourise(0, -1);
    CreateEmptyHint(editor);
    UpdateEmptyHint(editor);
}

void EditorNotebook::CreateEmptyHint(wxStyledTextCtrl* editor) {
    if (!editor || empty_hints_.count(editor)) return;
    auto* hint = new wxPanel(editor, wxID_ANY, wxDefaultPosition, editor->FromDIP(wxSize(480, 230)));
    hint->SetName("empty-script-hint");
    auto* layout = new wxBoxSizer(wxVERTICAL);

    auto* kicker = new wxStaticText(hint, wxID_ANY, "NEW SCENE");
    kicker->SetName("empty-kicker");
    kicker->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    auto* heading = new wxStaticText(hint, wxID_ANY, "Give the scene a place to begin.");
    heading->SetFont(style::BodyFont(18, wxFONTWEIGHT_BOLD));
    auto* detail = new wxStaticText(
        hint, wxID_ANY,
        "Start typing, open an existing .rpy script, or connect a project folder.");
    detail->SetFont(style::BodyFont(10));
    auto* example = new wxStaticText(
        hint, wxID_ANY, "label start:\n    narrator \"The story starts here.\"");
    example->SetName("empty-code");
    example->SetFont(style::UtilityFont(10));
    auto* shortcuts = new wxStaticText(
        hint, wxID_ANY, "CTRL+O  OPEN SCRIPT     CTRL+N  NEW TAB     CTRL+K  SHORTCUTS");
    shortcuts->SetName("empty-shortcuts");
    shortcuts->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));

    layout->Add(kicker, 0, wxBOTTOM, 8);
    layout->Add(heading, 0, wxBOTTOM, 8);
    layout->Add(detail, 0, wxBOTTOM, 20);
    layout->Add(example, 0, wxBOTTOM, 20);
    layout->Add(shortcuts);
    hint->SetSizer(layout);
    empty_hints_[editor] = hint;
    editor->Bind(wxEVT_SIZE, [this, editor](wxSizeEvent& event) {
        PositionEmptyHint(editor);
        event.Skip();
    });
    PositionEmptyHint(editor);
}

void EditorNotebook::UpdateEmptyHint(wxStyledTextCtrl* editor) {
    const auto found = empty_hints_.find(editor);
    if (found == empty_hints_.end() || !found->second) return;
    auto* hint = found->second;
    const bool empty = editor->GetText().Strip(wxString::both).empty();
    hint->Show(empty);
    if (!empty) return;
    const bool dark = settings_.theme == app::EditorTheme::Dark ||
                      (settings_.theme == app::EditorTheme::System &&
                       wxSystemSettings::GetAppearance().IsDark());
    const wxColour background(dark ? "#111B2B" : "#FCFBF8");
    hint->SetBackgroundColour(background);
    for (auto* child : hint->GetChildren()) {
        child->SetBackgroundColour(background);
        const wxString& name = child->GetName();
        child->SetForegroundColour(name == "empty-kicker" ? style::Colors().cue :
            name == "empty-code" ? (dark ? wxColour("#C29AB8") : style::Colors().plum) :
            name == "empty-shortcuts" ? (dark ? wxColour("#8190A4") : style::Colors().ink_soft) :
            (dark ? wxColour("#E8ECF1") : style::Colors().ink));
    }
    hint->Raise();
    PositionEmptyHint(editor);
}

void EditorNotebook::PositionEmptyHint(wxStyledTextCtrl* editor) {
    const auto found = empty_hints_.find(editor);
    if (found == empty_hints_.end() || !found->second) return;
    auto* hint = found->second;
    const wxSize size = hint->GetSize();
    const wxSize client = editor->GetClientSize();
    hint->SetPosition(wxPoint(std::max(editor->FromDIP(72), (client.x - size.x) / 2),
                              std::max(editor->FromDIP(72), (client.y - size.y) / 3)));
}

bool EditorNotebook::RefreshSpeakers(wxStyledTextCtrl* editor) {
    std::unordered_map<int, std::string> declarations;
    std::unordered_set<std::string> aliases;
    for (int line = 0; line < editor->GetLineCount(); ++line) {
        const auto alias = character_alias_on_line(
            editor->GetLine(line).ToStdString(wxConvUTF8));
        if (!alias) continue;
        declarations.emplace(line, *alias);
        aliases.insert(*alias);
    }
    const bool changed = speakers_[editor] != aliases;
    speaker_lines_[editor] = std::move(declarations);
    speakers_[editor] = std::move(aliases);
    return changed;
}

void EditorNotebook::DropEditorState(wxStyledTextCtrl* editor) {
    speakers_.erase(editor);
    speaker_lines_.erase(editor);
    completion_indexes_.erase(editor);
    pending_completion_editors_.erase(editor);
    completions_.erase(editor);
    empty_hints_.erase(editor);
    nvim_normal_modes_.erase(editor);
    nvim_pending_g_.erase(editor);
}

void EditorNotebook::RefreshCompletionIndex() {
    completion_indexes_.clear();
    for (size_t index = 0; index < GetPageCount(); ++index) {
        auto* editor = EditorAt(index);
        if (!editor) continue;
        completion_indexes_[editor] = build_completion_index(
            NamedScript{editor->GetName().ToStdString(wxConvUTF8),
                        editor->GetText().ToStdString(wxConvUTF8)});
    }
    MergeCompletionIndex();
}

void EditorNotebook::RefreshCompletionDocument(wxStyledTextCtrl* editor) {
    if (!editor) return;
    completion_indexes_[editor] = build_completion_index(
        NamedScript{editor->GetName().ToStdString(wxConvUTF8),
                    editor->GetText().ToStdString(wxConvUTF8)});
    MergeCompletionIndex();
}

void EditorNotebook::MergeCompletionIndex() {
    std::vector<CompletionIndex> indexes;
    indexes.reserve(GetPageCount());
    for (size_t index = 0; index < GetPageCount(); ++index) {
        auto* editor = EditorAt(index);
        const auto found = completion_indexes_.find(editor);
        if (found != completion_indexes_.end()) indexes.push_back(found->second);
    }
    completion_index_ = merge_completion_indexes(indexes);
}

void EditorNotebook::OnStyleNeeded(wxStyledTextEvent& event) {
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject());
    if (!editor) return;
    const int requested = event.GetPosition();
    const int start_line = editor->LineFromPosition(editor->GetEndStyled());
    const int end_line = editor->LineFromPosition(requested);
    const int start = editor->PositionFromLine(start_line);
    const int end = end_line + 1 < editor->GetLineCount() ? editor->PositionFromLine(end_line + 1)
                                                         : editor->GetTextLength();
    editor->StartStyling(start);
    int cursor = start;
    for (int line_number = start_line; line_number <= end_line; ++line_number) {
        const int line_start = editor->PositionFromLine(line_number);
        const int line_end = editor->GetLineEndPosition(line_number);
        const std::string line = editor->GetTextRange(line_start, line_end).ToStdString(wxConvUTF8);
        const auto spans = highlight_line(line, speakers_[editor]);
        int local = 0;
        for (const auto& span : spans) {
            if (static_cast<int>(span.begin) > local)
                editor->SetStyling(static_cast<int>(span.begin) - local, kDefault);
            editor->SetStyling(static_cast<int>(span.end - span.begin),
                               static_cast<int>(span.token_class));
            local = static_cast<int>(span.end);
        }
        if (line_end - line_start > local) editor->SetStyling(line_end - line_start - local, kDefault);
        const int next = line_number + 1 < editor->GetLineCount() ? editor->PositionFromLine(line_number + 1) : line_end;
        if (next > line_end) editor->SetStyling(next - line_end, kDefault);
        cursor = next;
    }
    (void)cursor;
}

void EditorNotebook::OnModified(wxStyledTextEvent& event) {
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject());
    if (!editor) return;
    // Style updates also raise wxEVT_STC_MODIFIED; reacting to them would recurse
    // through Colourise. Only text edits can change speaker definitions.
    if ((event.GetModificationType() & (wxSTC_MOD_INSERTTEXT | wxSTC_MOD_DELETETEXT)) == 0) {
        event.Skip();
        return;
    }
    UpdateEmptyHint(editor);
    const wxString changed = event.GetText();
    const int line_number = editor->LineFromPosition(std::max(0, event.GetPosition()));
    const wxString current = editor->GetLine(line_number);
    const bool declaration_may_have_changed = event.GetLinesAdded() != 0 ||
        changed.Find("Character") != wxNOT_FOUND || current.Find("Character") != wxNOT_FOUND ||
        speaker_lines_[editor].count(line_number) != 0;
    if (declaration_may_have_changed && RefreshSpeakers(editor)) {
        // Alias changes can affect dialogue highlighting anywhere in the document.
        // Scintilla already invalidates the edited range when the alias set is unchanged.
        editor->Colourise(0, -1);
    }
    pending_completion_editors_.insert(editor);
    completion_timer_.StartOnce(kCompletionDelayMs);
    if (!find_query_.empty()) RefreshFindHighlights();
    analysis_timer_.StartOnce(kAnalysisDelayMs);
    event.Skip();
}

void EditorNotebook::OnCharAdded(wxStyledTextEvent& event) {
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject());
    if (!editor) return;
    if (event.GetKey() == '\n' || event.GetKey() == '\r') {
        const int line = editor->LineFromPosition(editor->GetCurrentPos());
        if (line > 0) {
            const auto previous = editor->GetLine(line - 1).ToStdString(wxConvUTF8);
            editor->SetLineIndentation(
                line, static_cast<int>(next_line_indentation(previous)));
            editor->GotoPos(editor->GetLineIndentPosition(line));
        }
        editor->AutoCompCancel();
        completions_.erase(editor);
        event.Skip();
        return;
    }
    const std::string source = editor->GetText().ToStdString(wxConvUTF8);
    auto result = project_completions(source, static_cast<std::size_t>(editor->GetCurrentPos()),
                                      completion_index_);
    if (result.items.empty()) {
        editor->AutoCompCancel();
        completions_.erase(editor);
        event.Skip();
        return;
    }
    wxString choices;
    for (const auto& item : result.items) {
        if (!choices.empty()) choices += '\n';
        choices += wxString::FromUTF8(item.text);
    }
    const int replace_length = static_cast<int>(result.replace_length);
    completions_[editor] = std::move(result);
    editor->AutoCompShow(replace_length, choices);
    event.Skip();
}

void EditorNotebook::OnAutoCompCompleted(wxStyledTextEvent& event) {
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject());
    const auto pending = editor ? completions_.find(editor) : completions_.end();
    if (!editor || pending == completions_.end()) return;
    const std::string selected = event.GetText().ToStdString(wxConvUTF8);
    const auto item = std::find_if(pending->second.items.begin(), pending->second.items.end(),
                                   [&](const auto& candidate) { return candidate.text == selected; });
    if (item == pending->second.items.end()) return;
    const int end = editor->GetCurrentPos();
    const int start = end - static_cast<int>(selected.size());
    if (item->insert != selected) {
        editor->SetTargetStart(start); editor->SetTargetEnd(end);
        editor->ReplaceTarget(wxString::FromUTF8(item->insert));
    }
    if (item->select_start != std::string::npos && item->select_end != std::string::npos) {
        editor->SetSelection(start + static_cast<int>(item->select_start),
                             start + static_cast<int>(item->select_end));
    }
    completions_.erase(pending);
    event.Skip();
}

void EditorNotebook::OnDwellStart(wxStyledTextEvent& event) {
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject());
    if (!editor || event.GetPosition() < 0) return;
    const int page = GetPageIndex(editor);
    if (page == wxNOT_FOUND) return;
    const std::size_t line = static_cast<std::size_t>(editor->LineFromPosition(event.GetPosition()) + 1);
    std::string message;
    for (const auto& diagnostic : diagnostics_) {
        if (diagnostic.file_index != static_cast<std::size_t>(page) || diagnostic.line_number != line) continue;
        if (!message.empty()) message += "\n";
        message += diagnostic.message;
    }
    if (!message.empty()) editor->CallTipShow(event.GetPosition(), wxString::FromUTF8(message));
}

void EditorNotebook::OnDwellEnd(wxStyledTextEvent& event) {
    if (auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject())) editor->CallTipCancel();
}

void EditorNotebook::ApplyCommandResult(wxStyledTextCtrl* editor, const EditorCommandResult& result) {
    const std::string current = editor->GetText().ToStdString(wxConvUTF8);
    if (result.text != current) {
        editor->BeginUndoAction();
        editor->SetTargetStart(0);
        editor->SetTargetEnd(editor->GetTextLength());
        editor->ReplaceTarget(wxString::FromUTF8(result.text));
        editor->EndUndoAction();
    }
    editor->SetSelection(static_cast<int>(result.selection_start),
                         static_cast<int>(result.selection_end));
    editor->EnsureCaretVisible();
}

void EditorNotebook::ToggleComments() {
    const int page = GetSelection();
    auto* editor = page == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(page));
    if (!editor) return;
    ApplyCommandResult(editor, toggle_line_comments(editor->GetText().ToStdString(wxConvUTF8),
        static_cast<std::size_t>(editor->GetSelectionStart()),
        static_cast<std::size_t>(editor->GetSelectionEnd())));
}

void EditorNotebook::SetNvimNormalMode(wxStyledTextCtrl* editor, bool normal) {
    if (!editor) return;
    nvim_normal_modes_[editor] = normal;
    nvim_pending_g_.erase(editor);
    editor->SetCaretStyle(normal ? wxSTC_CARETSTYLE_BLOCK : wxSTC_CARETSTYLE_LINE);
    if (normal) ClampNvimCaret(editor);
    NotifyNvimMode();
}

void EditorNotebook::ClampNvimCaret(wxStyledTextCtrl* editor) {
    if (!editor) return;
    const int position = editor->GetCurrentPos();
    const int line = editor->LineFromPosition(position);
    const int start = editor->PositionFromLine(line);
    const int end = editor->GetLineEndPosition(line);
    if (end > start && position >= end) editor->SetEmptySelection(editor->PositionBefore(end));
}

void EditorNotebook::NotifyNvimMode() {
    if (!nvim_mode_handler_) return;
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    const bool normal = editor && nvim_normal_modes_[editor];
    nvim_mode_handler_(settings_.nvim_motions, normal);
}

bool EditorNotebook::HandleNvimNormalKey(wxStyledTextCtrl* editor, wxKeyEvent& event) {
    const int key = event.GetKeyCode();
    const int unicode = event.GetUnicodeKey();
    const bool control = event.ControlDown() || event.CmdDown();

    if (event.AltDown()) return false;
    if (control) {
        if (key == 'D' || key == 'd') {
            editor->PageDown();
            ClampNvimCaret(editor);
            return true;
        }
        if (key == 'U' || key == 'u') {
            editor->PageUp();
            ClampNvimCaret(editor);
            return true;
        }
        return false;
    }

    const bool had_pending_g = nvim_pending_g_.erase(editor) != 0;
    if (unicode == 'g') {
        if (had_pending_g) {
            editor->DocumentStart();
            ClampNvimCaret(editor);
        } else {
            nvim_pending_g_.insert(editor);
        }
        return true;
    }

    switch (unicode) {
        case 'h': {
            const int position = editor->GetCurrentPos();
            const int start = editor->PositionFromLine(editor->LineFromPosition(position));
            if (position > start) editor->SetEmptySelection(editor->PositionBefore(position));
            return true;
        }
        case 'j': editor->LineDown(); ClampNvimCaret(editor); return true;
        case 'k': editor->LineUp(); ClampNvimCaret(editor); return true;
        case 'l': {
            const int position = editor->GetCurrentPos();
            const int end = editor->GetLineEndPosition(editor->LineFromPosition(position));
            const int next = editor->PositionAfter(position);
            if (next < end) editor->SetEmptySelection(next);
            return true;
        }
        case 'w': editor->WordRight(); ClampNvimCaret(editor); return true;
        case 'b': editor->WordLeft(); ClampNvimCaret(editor); return true;
        case 'e': {
            editor->WordRightEnd();
            const int position = editor->GetCurrentPos();
            if (position > 0) editor->SetEmptySelection(editor->PositionBefore(position));
            ClampNvimCaret(editor);
            return true;
        }
        case '0': editor->Home(); ClampNvimCaret(editor); return true;
        case '^': editor->VCHome(); ClampNvimCaret(editor); return true;
        case '$': editor->LineEnd(); ClampNvimCaret(editor); return true;
        case 'G': editor->DocumentEnd(); ClampNvimCaret(editor); return true;
        case 'i': SetNvimNormalMode(editor, false); return true;
        case 'a': {
            const int position = editor->GetCurrentPos();
            const int end = editor->GetLineEndPosition(editor->LineFromPosition(position));
            if (position < end) editor->SetEmptySelection(editor->PositionAfter(position));
            SetNvimNormalMode(editor, false);
            return true;
        }
        case 'I': editor->VCHome(); SetNvimNormalMode(editor, false); return true;
        case 'A': editor->LineEnd(); SetNvimNormalMode(editor, false); return true;
        case 'o': editor->LineEnd(); editor->NewLine(); SetNvimNormalMode(editor, false); return true;
        case 'O': {
            const int line = editor->GetCurrentLine();
            const int start = editor->PositionFromLine(line);
            const wxString indentation = editor->GetTextRange(start, editor->GetLineIndentPosition(line));
            editor->BeginUndoAction();
            editor->InsertText(start, indentation + "\n");
            editor->EndUndoAction();
            editor->SetEmptySelection(start + static_cast<int>(indentation.length()));
            SetNvimNormalMode(editor, false);
            return true;
        }
        default: break;
    }

    switch (key) {
        case WXK_LEFT: editor->CharLeft(); ClampNvimCaret(editor); return true;
        case WXK_RIGHT: editor->CharRight(); ClampNvimCaret(editor); return true;
        case WXK_UP: editor->LineUp(); ClampNvimCaret(editor); return true;
        case WXK_DOWN:
        case WXK_RETURN:
        case WXK_NUMPAD_ENTER: editor->LineDown(); ClampNvimCaret(editor); return true;
        case WXK_HOME: editor->Home(); ClampNvimCaret(editor); return true;
        case WXK_END: editor->LineEnd(); ClampNvimCaret(editor); return true;
        case WXK_PAGEUP: editor->PageUp(); ClampNvimCaret(editor); return true;
        case WXK_PAGEDOWN: editor->PageDown(); ClampNvimCaret(editor); return true;
        case WXK_ESCAPE:
        case WXK_BACK:
        case WXK_DELETE:
        case WXK_TAB: return true;
        default: break;
    }

    // Normal mode must consume unmodified text keys so they cannot edit the document.
    if (unicode != WXK_NONE) return true;
    return false;
}

void EditorNotebook::OnKeyDown(wxKeyEvent& event) {
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject());
    if (!editor) { event.Skip(); return; }
    const int key = event.GetKeyCode();
    const bool control = event.ControlDown() || event.CmdDown();
    if (settings_.nvim_motions) {
        const bool normal = nvim_normal_modes_[editor];
        if (!normal && key == WXK_ESCAPE) {
            editor->AutoCompCancel();
            editor->SetEmptySelection(editor->GetCurrentPos());
            SetNvimNormalMode(editor, true);
            return;
        }
        if (normal && HandleNvimNormalKey(editor, event)) return;
    }
    if (control && !event.AltDown()) {
        if (!event.ShiftDown() && (key == WXK_UP || key == WXK_DOWN)) {
            const auto target = adjacent_label_line(editor->GetText().ToStdString(wxConvUTF8),
                static_cast<std::size_t>(editor->LineFromPosition(editor->GetCurrentPos()) + 1),
                key == WXK_DOWN ? 1 : -1);
            if (target) JumpToLine(*target);
            return;
        }
        if (!event.ShiftDown() && (key == WXK_PAGEUP || key == WXK_PAGEDOWN)) {
            if (GetPageCount() == 0) return;
            const int current = GetSelection();
            const int offset = key == WXK_PAGEDOWN ? 1 : -1;
            const int next = (current + offset + static_cast<int>(GetPageCount())) %
                             static_cast<int>(GetPageCount());
            SetSelection(static_cast<size_t>(next));
            if (auto* selected = EditorAt(static_cast<size_t>(next))) selected->SetFocus();
            return;
        }
        const int unicode = event.GetUnicodeKey();
        if (!event.ShiftDown() && (unicode == '/' || key == '/')) {
            ToggleComments();
            return;
        }
    }
    if (!control && event.AltDown() && (key == WXK_UP || key == WXK_DOWN)) {
        editor->AutoCompCancel();
        const std::string source = editor->GetText().ToStdString(wxConvUTF8);
        const auto start = static_cast<std::size_t>(editor->GetSelectionStart());
        const auto end = static_cast<std::size_t>(editor->GetSelectionEnd());
        const int direction = key == WXK_DOWN ? 1 : -1;
        ApplyCommandResult(editor, event.ShiftDown()
            ? duplicate_selected_lines(source, start, end, direction)
            : move_selected_lines(source, start, end, direction));
        return;
    }
    if (!control && !event.AltDown()) {
        const int unicode = event.GetUnicodeKey();
        if (unicode == '"' || unicode == '(' || unicode == '[' || unicode == ')' || unicode == ']') {
            const auto result = apply_pair_input(editor->GetText().ToStdString(wxConvUTF8),
                static_cast<std::size_t>(editor->GetSelectionStart()),
                static_cast<std::size_t>(editor->GetSelectionEnd()), static_cast<char>(unicode));
            if (result) { ApplyCommandResult(editor, *result); return; }
        }
    }
    event.Skip();
}

void EditorNotebook::AnalyzeActive() {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    if (!editor || !analysis_handler_) return;
    const wxString source = editor->GetText();
    analysis_handler_(source, analyze_script(source.ToStdString(wxConvUTF8), {count_menu_choices_}));
    RefreshDiagnostics();
}

void EditorNotebook::SetDiagnosticsHandler(DiagnosticsHandler handler) {
    diagnostics_handler_ = std::move(handler);
    RefreshDiagnostics();
}

void EditorNotebook::RefreshDiagnostics() {
    diagnostics_ = diagnose_project(ProjectScripts());
    ApplyDiagnostics();
    if (diagnostics_handler_) diagnostics_handler_(diagnostics_);
}

void EditorNotebook::ApplyDiagnostics() {
    for (size_t index = 0; index < GetPageCount(); ++index) {
        auto* editor = EditorAt(index);
        if (!editor) continue;
        for (const int indicator : {kDiagnosticErrorIndicator, kDiagnosticWarningIndicator,
                                    kDiagnosticNoticeIndicator}) {
            editor->SetIndicatorCurrent(indicator);
            editor->IndicatorClearRange(0, editor->GetTextLength());
        }
        for (const int marker : {kDiagnosticErrorMarker, kDiagnosticWarningMarker,
                                 kDiagnosticNoticeMarker}) editor->MarkerDeleteAll(marker);
    }
    for (const auto& diagnostic : diagnostics_) {
        if (diagnostic.file_index >= GetPageCount()) continue;
        auto* editor = EditorAt(diagnostic.file_index);
        if (!editor) continue;
        int indicator = kDiagnosticWarningIndicator;
        int marker = kDiagnosticWarningMarker;
        if (diagnostic.severity == DiagnosticSeverity::Error) {
            indicator = kDiagnosticErrorIndicator; marker = kDiagnosticErrorMarker;
        } else if (diagnostic.severity == DiagnosticSeverity::Notice) {
            indicator = kDiagnosticNoticeIndicator; marker = kDiagnosticNoticeMarker;
        }
        if (diagnostic.position < static_cast<std::size_t>(editor->GetTextLength())) {
            editor->SetIndicatorCurrent(indicator);
            editor->IndicatorFillRange(static_cast<int>(diagnostic.position),
                                       static_cast<int>(std::min<std::size_t>(
                                           diagnostic.length, editor->GetTextLength() - diagnostic.position)));
        }
        if (diagnostic.line_number > 0 && diagnostic.line_number <= static_cast<std::size_t>(editor->GetLineCount()))
            editor->MarkerAdd(static_cast<int>(diagnostic.line_number - 1), marker);
    }
}

void EditorNotebook::SelectDiagnostic(const Diagnostic& diagnostic) {
    if (diagnostic.file_index >= GetPageCount()) return;
    SetSelection(diagnostic.file_index);
    auto* editor = EditorAt(diagnostic.file_index);
    if (!editor) return;
    const std::size_t end = std::min<std::size_t>(diagnostic.position + diagnostic.length,
                                                 editor->GetTextLength());
    editor->SetSelection(static_cast<int>(std::min<std::size_t>(diagnostic.position, end)),
                         static_cast<int>(end));
    editor->EnsureCaretVisible();
    editor->SetFocus();
}

void EditorNotebook::JumpToLine(std::size_t line_number) {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    if (!editor || line_number == 0) return;
    const int line = std::min<int>(static_cast<int>(line_number - 1), editor->GetLineCount() - 1);
    const int position = editor->PositionFromLine(line);
    editor->GotoPos(position);
    editor->EnsureVisible(line);
    editor->SetFocus();
}

std::string EditorNotebook::SelectedText() const {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    return editor ? editor->GetSelectedText().ToStdString(wxConvUTF8) : std::string{};
}

void EditorNotebook::SetFindStatusHandler(FindStatusHandler handler) {
    find_status_handler_ = std::move(handler);
}

FindStatus EditorNotebook::RefreshFindHighlights() {
    FindStatus status;
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    if (!editor) return status;

    editor->SetIndicatorCurrent(kFindIndicator);
    editor->IndicatorClearRange(0, editor->GetTextLength());
    const auto found = find_matches(editor->GetText().ToStdString(wxConvUTF8), find_query_, find_options_);
    status.valid = found.valid;
    status.total = found.matches.size();
    if (found.valid && found.matches.size() <= kMaxHighlightedMatches) {
        for (const auto& match : found.matches)
            editor->IndicatorFillRange(static_cast<int>(match.position), static_cast<int>(match.length));
    }

    const std::size_t start = static_cast<std::size_t>(editor->GetSelectionStart());
    const std::size_t end = static_cast<std::size_t>(editor->GetSelectionEnd());
    for (std::size_t index = 0; index < found.matches.size(); ++index) {
        const auto& match = found.matches[index];
        if (match.position == start && match.position + match.length == end) {
            status.current = index + 1;
            break;
        }
        if (match.position < start) status.current = index + 1;
    }
    if (find_status_handler_) find_status_handler_(status);
    return status;
}

FindStatus EditorNotebook::SetFindQuery(std::string query, FindOptions options) {
    find_query_ = std::move(query);
    find_options_ = options;
    return RefreshFindHighlights();
}

FindStatus EditorNotebook::FindNext(int direction) {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    if (!editor || find_query_.empty()) return RefreshFindHighlights();
    const auto found = find_matches(editor->GetText().ToStdString(wxConvUTF8), find_query_, find_options_);
    if (!found.valid || found.matches.empty()) return RefreshFindHighlights();

    const std::size_t caret = static_cast<std::size_t>(
        direction >= 0 ? editor->GetSelectionEnd() : editor->GetSelectionStart());
    const FindMatch* selected = nullptr;
    if (direction >= 0) {
        const auto item = std::find_if(found.matches.begin(), found.matches.end(),
            [&](const auto& match) { return match.position >= caret; });
        selected = item == found.matches.end() ? &found.matches.front() : &*item;
    } else {
        const auto item = std::find_if(found.matches.rbegin(), found.matches.rend(),
            [&](const auto& match) { return match.position < caret; });
        selected = item == found.matches.rend() ? &found.matches.back() : &*item;
    }
    editor->SetSelection(static_cast<int>(selected->position),
                         static_cast<int>(selected->position + selected->length));
    editor->EnsureCaretVisible();
    editor->SetFocus();
    return RefreshFindHighlights();
}

std::vector<NamedScript> EditorNotebook::ProjectScripts() const {
    std::vector<NamedScript> scripts;
    scripts.reserve(GetPageCount());
    for (size_t index = 0; index < GetPageCount(); ++index) {
        if (auto* editor = EditorAt(index))
            scripts.push_back({editor->GetName().ToStdString(wxConvUTF8), editor->GetText().ToStdString(wxConvUTF8)});
    }
    return scripts;
}

std::size_t EditorNotebook::CurrentFileIndex() const {
    const int selection = GetSelection();
    return selection == wxNOT_FOUND ? 0 : static_cast<std::size_t>(selection);
}

void EditorNotebook::SelectFileIndex(std::size_t index) {
    if (index < GetPageCount()) SetSelection(index);
}

void EditorNotebook::SetCountMenuChoices(bool enabled) {
    if (count_menu_choices_ == enabled) return;
    count_menu_choices_ = enabled;
    RefreshDiagnostics();
    AnalyzeActive();
}

wxString EditorNotebook::CurrentFilePath() const {
    const int selection = GetSelection();
    return selection == wxNOT_FOUND ? wxString{} : FilePath(EditorAt(static_cast<std::size_t>(selection)));
}

wxString EditorNotebook::CurrentFileName() const {
    const int selection = GetSelection();
    const auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<std::size_t>(selection));
    return editor ? editor->GetName() : wxString{};
}

std::size_t EditorNotebook::CurrentLine() const {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<std::size_t>(selection));
    return editor ? static_cast<std::size_t>(editor->LineFromPosition(editor->GetCurrentPos()) + 1) : 0;
}

bool EditorNotebook::SaveAll() {
    for (std::size_t index = 0; index < GetPageCount(); ++index) {
        auto* editor = EditorAt(index);
        if (!editor || !editor->GetModify()) continue;
        const wxString path = FilePath(editor);
        if (path.empty()) {
            wxMessageBox("Save every modified tab to the connected project before continuing.",
                         "Unsaved script", wxOK | wxICON_ERROR, this);
            return false;
        }
        if (!SaveEditor(editor, path)) return false;
    }
    return true;
}

std::vector<WorkspaceFileState> EditorNotebook::CaptureWorkspaceFiles() const {
    std::vector<WorkspaceFileState> files;
    for (std::size_t index = 0; index < GetPageCount(); ++index) {
        auto* editor = EditorAt(index);
        const wxString path = FilePath(editor);
        if (!editor || path.empty()) continue;
        files.push_back({path.ToStdString(wxConvUTF8),
                         static_cast<std::size_t>(std::max(0, editor->GetCurrentPos())),
                         static_cast<std::size_t>(std::max(0, editor->GetFirstVisibleLine()))});
    }
    return files;
}

void EditorNotebook::RestoreWorkspaceViews(const std::vector<WorkspaceFileState>& files,
                                           std::string_view active_file) {
    for (const auto& saved : files) {
        const wxString path = wxFileName(wxString::FromUTF8(saved.path)).GetAbsolutePath();
        for (std::size_t index = 0; index < GetPageCount(); ++index) {
            auto* editor = EditorAt(index);
            if (!editor || FilePath(editor) != path) continue;
            editor->GotoPos(std::min<int>(static_cast<int>(saved.caret), editor->GetTextLength()));
            editor->SetFirstVisibleLine(std::min<int>(static_cast<int>(saved.first_visible_line),
                                                       std::max(0, editor->GetLineCount() - 1)));
            if (saved.path == active_file) SetSelection(index);
            break;
        }
    }
    AnalyzeActive();
}

bool EditorNotebook::OpenAndJump(const wxString& path, std::size_t line) {
    const wxString absolute = wxFileName(path).GetAbsolutePath();
    if (!OpenFiles({absolute})) return false;
    for (std::size_t index = 0; index < GetPageCount(); ++index) {
        if (FilePath(EditorAt(index)) == absolute) { SetSelection(index); JumpToLine(line); return true; }
    }
    return false;
}

void EditorNotebook::InsertAtCaret(std::string_view text) {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<std::size_t>(selection));
    if (!editor) return;
    editor->BeginUndoAction();
    editor->ReplaceSelection(wxString::FromUTF8(text.data(), text.size()));
    editor->EndUndoAction();
    editor->SetFocus();
}

std::optional<ManuscriptEditorPreview> EditorNotebook::PrepareManuscriptConversion() const {
    const int page = GetSelection();
    auto* editor = page == wxNOT_FOUND ? nullptr : EditorAt(static_cast<std::size_t>(page));
    if (!editor) return std::nullopt;

    const int selection_start = editor->GetSelectionStart();
    const int selection_end = editor->GetSelectionEnd();
    const bool has_selection = selection_start != selection_end;
    const int start = has_selection ? selection_start : 0;
    const int end = has_selection ? selection_end : editor->GetTextLength();
    const wxString source = editor->GetTextRange(start, end);
    if (source.Strip(wxString::both).empty()) return std::nullopt;
    const std::string source_utf8 = source.ToStdString(wxConvUTF8);
    const auto existing_characters = analyze_project(ProjectScripts()).character_names;
    ManuscriptEditorPreview preview;
    preview.start = start;
    preview.end = end;
    preview.selection = has_selection;
    preview.source = source_utf8;
    preview.lines = review_manuscript_lines(source_utf8);
    preview.safe_conversion = convert_mixed_manuscript_to_renpy(
        source_utf8, !has_selection, existing_characters, false);
    preview.inclusive_conversion = convert_mixed_manuscript_to_renpy(
        source_utf8, !has_selection, existing_characters, true);
    return preview;
}

bool EditorNotebook::PrepareOfflineAiConversion(
    ManuscriptEditorPreview* preview, std::string_view rewritten_manuscript) const {
    if (!preview || rewritten_manuscript.empty()) return false;
    const auto existing_characters = analyze_project(ProjectScripts()).character_names;
    const auto conversion = convert_mixed_manuscript_to_renpy(
        rewritten_manuscript, !preview->selection, existing_characters, true);
    if (conversion.script.empty()) return false;
    // Offline AI resolves review lines itself. Both choices intentionally point
    // at the same validated result so Apply still performs one undoable edit.
    preview->safe_conversion = conversion;
    preview->inclusive_conversion = conversion;
    return true;
}

bool EditorNotebook::ApplyManuscriptConversion(
    const ManuscriptEditorPreview& preview, bool include_uncertain_lines) {
    const int page = GetSelection();
    auto* editor = page == wxNOT_FOUND ? nullptr : EditorAt(static_cast<std::size_t>(page));
    if (!editor || editor->GetTextRange(preview.start, preview.end).ToStdString(wxConvUTF8) != preview.source)
        return false;
    const auto& converted = include_uncertain_lines ? preview.inclusive_conversion
                                                    : preview.safe_conversion;
    if (converted.script.empty() || converted.script == preview.source) return false;

    editor->BeginUndoAction();
    editor->SetTargetStart(preview.start);
    editor->SetTargetEnd(preview.end);
    editor->ReplaceTarget(wxString::FromUTF8(converted.script));
    editor->EndUndoAction();
    const int caret = preview.start + static_cast<int>(converted.script.size());
    editor->SetSelection(caret, caret);
    editor->EnsureCaretVisible();
    editor->SetFocus();
    return true;
}

bool EditorNotebook::RestoreProjectScripts(const std::vector<NamedScript>& scripts) {
    if (scripts.empty()) return false;
    std::unordered_set<wxStyledTextCtrl*> retained;
    std::vector<wxStyledTextCtrl*> ordered;
    ordered.reserve(scripts.size());
    for (const auto& script : scripts) {
        wxStyledTextCtrl* editor = nullptr;
        for (size_t index = 0; index < GetPageCount(); ++index) {
            auto* candidate = EditorAt(index);
            if (candidate && retained.count(candidate) == 0 &&
                candidate->GetName().ToStdString(wxConvUTF8) == script.name) {
                editor = candidate;
                break;
            }
        }
        if (!editor) {
            editor = new wxStyledTextCtrl(this, wxID_ANY);
            ConfigureEditor(editor);
            editor->SetName(wxString::FromUTF8(script.name));
            editor->SetSavePoint();
            editor->Bind(wxEVT_STC_SAVEPOINTLEFT, &EditorNotebook::OnSavePointChanged, this);
            editor->Bind(wxEVT_STC_SAVEPOINTREACHED, &EditorNotebook::OnSavePointChanged, this);
            AddPage(editor, editor->GetName(), false);
        }
        const wxString restored = NormalizeTabs(wxString::FromUTF8(script.content));
        if (editor->GetText() != restored) {
            editor->BeginUndoAction();
            editor->SetTargetStart(0);
            editor->SetTargetEnd(editor->GetTextLength());
            editor->ReplaceTarget(restored);
            editor->EndUndoAction();
        }
        RefreshSpeakers(editor);
        editor->Colourise(0, -1);
        UpdateTabLabel(editor);
        retained.insert(editor);
        ordered.push_back(editor);
    }
    for (size_t index = GetPageCount(); index-- > 0;) {
        auto* editor = EditorAt(index);
        if (!editor || retained.count(editor) != 0) continue;
        DropEditorState(editor);
        RemovePage(index);
        editor->Destroy();
    }
    while (GetPageCount()) RemovePage(0);
    for (auto* editor : ordered)
        AddPage(editor, editor->GetName() + (editor->GetModify() ? " ●" : ""), false);
    SetSelection(0);
    RefreshCompletionIndex();
    RefreshDiagnostics();
    AnalyzeActive();
    return true;
}

void EditorNotebook::SelectProjectMatch(const ProjectFindMatch& match) {
    if (match.file_index >= GetPageCount()) return;
    SetSelection(match.file_index);
    auto* editor = EditorAt(match.file_index);
    if (!editor) return;
    editor->SetSelection(static_cast<int>(match.position),
                         static_cast<int>(match.position + match.length));
    editor->EnsureCaretVisible();
    editor->SetFocus();
    RefreshFindHighlights();
}

FindStatus EditorNotebook::FindNextAcrossFiles(int direction) {
    const int page = GetSelection();
    auto* editor = page == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(page));
    if (!editor || find_query_.empty()) return RefreshFindHighlights();
    const auto found = find_project_matches(ProjectScripts(), find_query_, find_options_);
    if (!found.valid || found.matches.empty()) return RefreshFindHighlights();
    const std::size_t current_file = static_cast<std::size_t>(page);
    const std::size_t caret = static_cast<std::size_t>(
        direction >= 0 ? editor->GetSelectionEnd() : editor->GetSelectionStart());
    const ProjectFindMatch* selected = nullptr;
    if (direction >= 0) {
        const auto item = std::find_if(found.matches.begin(), found.matches.end(), [&](const auto& match) {
            return match.file_index > current_file ||
                   (match.file_index == current_file && match.position >= caret);
        });
        selected = item == found.matches.end() ? &found.matches.front() : &*item;
    } else {
        const auto item = std::find_if(found.matches.rbegin(), found.matches.rend(), [&](const auto& match) {
            return match.file_index < current_file ||
                   (match.file_index == current_file && match.position < caret);
        });
        selected = item == found.matches.rend() ? &found.matches.back() : &*item;
    }
    SelectProjectMatch(*selected);
    return RefreshFindHighlights();
}

bool EditorNotebook::ReplaceCurrent(std::string_view replacement, bool across_files) {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    if (!editor || find_query_.empty()) return false;
    const std::string source = editor->GetText().ToStdString(wxConvUTF8);
    const auto found = find_matches(source, find_query_, find_options_);
    const std::size_t start = static_cast<std::size_t>(editor->GetSelectionStart());
    const std::size_t end = static_cast<std::size_t>(editor->GetSelectionEnd());
    const bool selected_match = std::any_of(found.matches.begin(), found.matches.end(), [&](const auto& match) {
        return match.position == start && match.position + match.length == end;
    });
    if (!found.valid || !selected_match) {
        if (across_files) FindNextAcrossFiles(1);
        else FindNext(1);
        return false;
    }

    const auto changed = replace_all_matches(source.substr(start, end - start), find_query_,
                                             replacement, find_options_);
    if (!changed.valid || changed.count == 0) return false;
    editor->BeginUndoAction();
    editor->SetTargetStart(static_cast<int>(start));
    editor->SetTargetEnd(static_cast<int>(end));
    editor->ReplaceTarget(wxString::FromUTF8(changed.text));
    editor->EndUndoAction();
    editor->GotoPos(static_cast<int>(start + changed.text.size()));
    if (across_files) FindNextAcrossFiles(1);
    else FindNext(1);
    return true;
}

std::size_t EditorNotebook::ReplaceAll(std::string_view replacement) {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    if (!editor || find_query_.empty()) return 0;
    const auto changed = replace_all_matches(editor->GetText().ToStdString(wxConvUTF8), find_query_,
                                             replacement, find_options_);
    if (!changed.valid || changed.count == 0) {
        RefreshFindHighlights();
        return 0;
    }
    const int caret = std::min<int>(editor->GetCurrentPos(), static_cast<int>(changed.text.size()));
    editor->BeginUndoAction();
    editor->SetTargetStart(0);
    editor->SetTargetEnd(editor->GetTextLength());
    editor->ReplaceTarget(wxString::FromUTF8(changed.text));
    editor->EndUndoAction();
    editor->GotoPos(caret);
    RefreshFindHighlights();
    return changed.count;
}

std::size_t EditorNotebook::ReplaceAllAcrossFiles(
    const std::vector<std::size_t>& selected_files, std::string_view replacement) {
    if (find_query_.empty() || selected_files.empty()) return 0;
    const auto changed = replace_project_matches(ProjectScripts(), selected_files, find_query_,
                                                 replacement, find_options_);
    if (!changed.valid || changed.count == 0) return 0;
    for (const auto index : selected_files) {
        if (index >= GetPageCount() || index >= changed.per_file_counts.size() ||
            changed.per_file_counts[index] == 0) continue;
        auto* editor = EditorAt(index);
        if (!editor) continue;
        editor->BeginUndoAction();
        editor->SetTargetStart(0);
        editor->SetTargetEnd(editor->GetTextLength());
        editor->ReplaceTarget(wxString::FromUTF8(changed.scripts[index].content));
        editor->EndUndoAction();
    }
    RefreshFindHighlights();
    return changed.count;
}

void EditorNotebook::ClearFind() {
    find_query_.clear();
    for (size_t index = 0; index < GetPageCount(); ++index) {
        if (auto* editor = EditorAt(index)) {
            editor->SetIndicatorCurrent(kFindIndicator);
            editor->IndicatorClearRange(0, editor->GetTextLength());
        }
    }
    if (find_status_handler_) find_status_handler_({});
}

void EditorNotebook::OnAnalysisTimer(wxTimerEvent&) {
    AnalyzeActive();
}

void EditorNotebook::OnCompletionTimer(wxTimerEvent&) {
    const auto pending = std::move(pending_completion_editors_);
    pending_completion_editors_.clear();
    for (auto* editor : pending)
        if (completion_indexes_.count(editor) != 0) RefreshCompletionDocument(editor);
}

void EditorNotebook::OnPageChanged(wxAuiNotebookEvent& event) {
    analysis_timer_.Stop();
    AnalyzeActive();
    if (!find_query_.empty()) RefreshFindHighlights();
    NotifyNvimMode();
    event.Skip();
}

void EditorNotebook::SetWordWrap(bool enabled) {
    settings_.word_wrap = enabled;
    for (size_t i = 0; i < GetPageCount(); ++i)
        EditorAt(i)->SetWrapMode(enabled ? wxSTC_WRAP_WORD : wxSTC_WRAP_NONE);
}

void EditorNotebook::SetFontSize(int size) {
    settings_.font_size = std::clamp(size, 10, 32);
    for (size_t i = 0; i < GetPageCount(); ++i) ConfigureEditor(EditorAt(i));
}

void EditorNotebook::SetTheme(app::EditorTheme theme) {
    settings_.theme = theme;
    for (size_t i = 0; i < GetPageCount(); ++i) ConfigureEditor(EditorAt(i));
}

void EditorNotebook::SetNvimMotions(bool enabled) {
    settings_.nvim_motions = enabled;
    nvim_pending_g_.clear();
    for (size_t index = 0; index < GetPageCount(); ++index) {
        auto* editor = EditorAt(index);
        if (!editor) continue;
        nvim_normal_modes_[editor] = enabled;
        editor->SetEmptySelection(editor->GetCurrentPos());
        editor->SetCaretStyle(enabled ? wxSTC_CARETSTYLE_BLOCK : wxSTC_CARETSTYLE_LINE);
        if (enabled) ClampNvimCaret(editor);
    }
    NotifyNvimMode();
}

void EditorNotebook::SetNvimModeHandler(NvimModeHandler handler) {
    nvim_mode_handler_ = std::move(handler);
    NotifyNvimMode();
}

bool EditorNotebook::SaveEditor(wxStyledTextCtrl* editor, const wxString& path) {
    // Write a sibling temp file and rename so a failed write never truncates the script.
    const wxString temporary = path + ".saycount-tmp";
    wxFile file;
    bool written = file.Create(temporary, true) &&
                   file.Write(editor->GetText(), wxConvUTF8) && file.Close();
    if (written) written = wxRenameFile(temporary, path, true);
    if (!written) {
        if (wxFileName::FileExists(temporary)) wxRemoveFile(temporary);
        wxMessageBox(wxString::Format("Could not save %s.", path), "Save failed",
                     wxOK | wxICON_ERROR, this);
        return false;
    }
    SetFilePath(editor, wxFileName(path).GetAbsolutePath());
    editor->SetSavePoint();
    return true;
}

bool EditorNotebook::SaveCurrent() {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    if (!editor) return false;
    const wxString path = FilePath(editor);
    return path.empty() ? SaveCurrentAs() : SaveEditor(editor, path);
}

bool EditorNotebook::SaveCurrentAs() {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    if (!editor) return false;
    wxFileDialog dialog(this, "Save Ren'Py script", wxEmptyString, editor->GetName(),
                        "Ren'Py scripts (*.rpy)|*.rpy", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dialog.ShowModal() != wxID_OK) return false;
    return SaveEditor(editor, dialog.GetPath());
}

wxStyledTextCtrl* EditorNotebook::EditorAt(size_t index) const {
    return dynamic_cast<wxStyledTextCtrl*>(GetPage(index));
}

void EditorNotebook::UpdateTabLabel(wxStyledTextCtrl* editor) {
    const int index = GetPageIndex(editor);
    if (index == wxNOT_FOUND) {
        return;
    }
    SetPageText(static_cast<size_t>(index),
                editor->GetModify() ? editor->GetName() + " ●" : editor->GetName());
}

void EditorNotebook::OnSavePointChanged(wxStyledTextEvent& event) {
    if (auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject())) {
        UpdateTabLabel(editor);
    }
    event.Skip();
}

bool EditorNotebook::ConfirmDiscard(size_t index) {
    const auto* editor = EditorAt(index);
    if (!editor || !editor->GetModify()) {
        return true;
    }

    wxMessageDialog dialog(this,
                           wxString::Format("Discard changes to %s?", editor->GetName()),
                           "Unsaved changes", wxYES_NO | wxNO_DEFAULT | wxICON_WARNING);
    dialog.SetYesNoLabels("Discard", "Cancel");
    return dialog.ShowModal() == wxID_YES;
}

void EditorNotebook::CloseCurrentTab() {
    const int selection = GetSelection();
    if (selection == wxNOT_FOUND || !ConfirmDiscard(static_cast<size_t>(selection))) {
        return;
    }
    // DeletePage raises no close events, so drop per-editor state here.
    if (auto* editor = EditorAt(static_cast<size_t>(selection))) DropEditorState(editor);
    DeletePage(static_cast<size_t>(selection));
    if (GetPageCount() == 0) {
        NewTab();
    } else {
        MergeCompletionIndex();
        AnalyzeActive();
    }
}

bool EditorNotebook::ConfirmCloseAll() {
    for (size_t index = 0; index < GetPageCount(); ++index) {
        if (!ConfirmDiscard(index)) {
            SetSelection(index);
            return false;
        }
    }
    return true;
}

void EditorNotebook::OnPageClose(wxAuiNotebookEvent& event) {
    const int selection = event.GetSelection();
    if (selection == wxNOT_FOUND || !ConfirmDiscard(static_cast<size_t>(selection))) {
        event.Veto();
        return;
    }

    DropEditorState(EditorAt(static_cast<size_t>(selection)));
    event.Skip();
}

void EditorNotebook::OnPageClosed(wxAuiNotebookEvent& event) {
    MergeCompletionIndex();
    if (GetPageCount() == 0) {
        NewTab();
    } else {
        AnalyzeActive();
    }
    event.Skip();
}

}  // namespace say_count::ui
