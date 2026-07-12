#include "ui/editor_notebook.h"
#include "core/parser.h"
#include "core/tokenizer.h"
#include "core/autocomplete.h"

#include <algorithm>

#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/stc/stc.h>
#include <wx/settings.h>

namespace say_count::ui {
namespace {

constexpr const char* kFilePathProperty = "say-count-file-path";
constexpr int kAnalysisDelayMs = 120;
constexpr int kFindIndicator = 8;
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
      settings_(settings), analysis_handler_(std::move(analysis_handler)), analysis_timer_(this) {
    Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &EditorNotebook::OnPageClose, this);
    Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSED, &EditorNotebook::OnPageClosed, this);
    Bind(wxEVT_AUINOTEBOOK_PAGE_CHANGED, &EditorNotebook::OnPageChanged, this);
    Bind(wxEVT_TIMER, &EditorNotebook::OnAnalysisTimer, this, analysis_timer_.GetId());
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
    RefreshCompletionIndex();
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

bool EditorNotebook::OpenFiles(const std::vector<wxString>& paths) {
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
            SetSelection(static_cast<size_t>(existing));
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
        RefreshCompletionIndex();
        editor->SetFocus();
        AnalyzeActive();
        opened = true;
    }
    return opened;
}

void EditorNotebook::ConfigureEditor(wxStyledTextCtrl* editor) {
    editor->SetLexer(wxSTC_LEX_CONTAINER);
    editor->Unbind(wxEVT_STC_STYLENEEDED, &EditorNotebook::OnStyleNeeded, this);
    editor->Bind(wxEVT_STC_STYLENEEDED, &EditorNotebook::OnStyleNeeded, this);
    editor->Unbind(wxEVT_STC_MODIFIED, &EditorNotebook::OnModified, this);
    editor->Bind(wxEVT_STC_MODIFIED, &EditorNotebook::OnModified, this);
    editor->Unbind(wxEVT_STC_CHARADDED, &EditorNotebook::OnCharAdded, this);
    editor->Bind(wxEVT_STC_CHARADDED, &EditorNotebook::OnCharAdded, this);
    editor->Unbind(wxEVT_STC_AUTOCOMP_COMPLETED, &EditorNotebook::OnAutoCompCompleted, this);
    editor->Bind(wxEVT_STC_AUTOCOMP_COMPLETED, &EditorNotebook::OnAutoCompCompleted, this);
    editor->AutoCompSetIgnoreCase(true);
    editor->AutoCompSetMaxHeight(8);
    editor->SetMarginType(0, wxSTC_MARGIN_NUMBER);
    editor->SetMarginWidth(0, editor->TextWidth(wxSTC_STYLE_LINENUMBER, "00000") + 8);
    editor->SetCaretLineVisible(true);
    editor->SetWrapMode(settings_.word_wrap ? wxSTC_WRAP_WORD : wxSTC_WRAP_NONE);
    const bool dark = settings_.theme == app::EditorTheme::Dark ||
                      (settings_.theme == app::EditorTheme::System && wxSystemSettings::GetAppearance().IsDark());
    const wxColour foreground(dark ? "#f2e9d6" : "#241a2e");
    const wxColour background(dark ? "#1a142c" : "#fffaea");
    const wxColour margin(dark ? "#211a33" : "#f5e9c9");
    editor->StyleSetFont(wxSTC_STYLE_DEFAULT,
                         wxFont(settings_.font_size, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    editor->StyleSetForeground(wxSTC_STYLE_DEFAULT, foreground);
    editor->StyleSetBackground(wxSTC_STYLE_DEFAULT, background);
    editor->StyleClearAll();
    editor->StyleSetForeground(kComment, dark ? wxColour("#8f9a85") : wxColour("#68765f"));
    editor->StyleSetItalic(kComment, true);
    editor->StyleSetForeground(kKeyword, dark ? wxColour("#e7a86e") : wxColour("#9b3f38"));
    editor->StyleSetBold(kKeyword, true);
    editor->StyleSetForeground(kLabel, dark ? wxColour("#f2ca72") : wxColour("#7b4f00"));
    editor->StyleSetBold(kLabel, true);
    editor->StyleSetForeground(kSpeaker, dark ? wxColour("#77d6c3") : wxColour("#087b70"));
    editor->StyleSetBold(kSpeaker, true);
    editor->StyleSetForeground(kString, dark ? wxColour("#b8d98b") : wxColour("#43752c"));
    editor->StyleSetForeground(kPython, dark ? wxColour("#d8a7e8") : wxColour("#7c3b92"));
    editor->StyleSetForeground(kStatement, dark ? wxColour("#91bdec") : wxColour("#2f62a0"));
    editor->StyleSetForeground(wxSTC_STYLE_LINENUMBER, dark ? wxColour("#9a91a7") : wxColour("#756a77"));
    editor->StyleSetBackground(wxSTC_STYLE_LINENUMBER, margin);
    editor->SetCaretForeground(foreground);
    editor->SetCaretLineBackground(dark ? wxColour("#281f3d") : wxColour("#f5e9c9"));
    editor->SetSelBackground(true, dark ? wxColour("#6b542d") : wxColour("#ead39c"));
    editor->IndicatorSetStyle(kFindIndicator, wxSTC_INDIC_ROUNDBOX);
    editor->IndicatorSetForeground(kFindIndicator, dark ? wxColour("#f2ca72") : wxColour("#d49a13"));
    editor->IndicatorSetAlpha(kFindIndicator, 80);
    editor->IndicatorSetOutlineAlpha(kFindIndicator, 180);
    editor->Colourise(0, -1);
}

void EditorNotebook::RefreshSpeakers(wxStyledTextCtrl* editor) {
    auto& aliases = speakers_[editor];
    aliases.clear();
    const auto analysis = analyze_script(editor->GetText().ToStdString());
    for (const auto& [alias, name] : analysis.character_names) {
        (void)name;
        aliases.insert(alias);
    }
}

void EditorNotebook::RefreshCompletionIndex() {
    std::vector<NamedScript> scripts;
    scripts.reserve(GetPageCount());
    for (size_t index = 0; index < GetPageCount(); ++index) {
        auto* editor = EditorAt(index);
        if (!editor) continue;
        scripts.push_back({editor->GetName().ToStdString(), editor->GetText().ToStdString()});
    }
    completion_index_ = build_completion_index(scripts);
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
        const std::string line = editor->GetTextRange(line_start, line_end).ToStdString();
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
    const wxString changed = event.GetText();
    const int line_number = editor->LineFromPosition(std::max(0, event.GetPosition()));
    const wxString current = editor->GetLine(line_number);
    if (changed.Find("Character") != wxNOT_FOUND || current.Find("Character") != wxNOT_FOUND) {
        RefreshSpeakers(editor);
        editor->Colourise(0, -1);
    }
    RefreshCompletionIndex();
    if (!find_query_.empty()) RefreshFindHighlights();
    analysis_timer_.StartOnce(kAnalysisDelayMs);
    event.Skip();
}

void EditorNotebook::OnCharAdded(wxStyledTextEvent& event) {
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject());
    if (!editor) return;
    const std::string source = editor->GetText().ToStdString();
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
    const std::string selected = event.GetText().ToStdString();
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

void EditorNotebook::AnalyzeActive() {
    const int selection = GetSelection();
    auto* editor = selection == wxNOT_FOUND ? nullptr : EditorAt(static_cast<size_t>(selection));
    if (!editor || !analysis_handler_) return;
    const wxString source = editor->GetText();
    analysis_handler_(source, analyze_script(source.ToStdString()));
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
    return editor ? editor->GetSelectedText().ToStdString() : std::string{};
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
    const auto found = find_matches(editor->GetText().ToStdString(), find_query_, find_options_);
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
    const auto found = find_matches(editor->GetText().ToStdString(), find_query_, find_options_);
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
            scripts.push_back({editor->GetName().ToStdString(), editor->GetText().ToStdString()});
    }
    return scripts;
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
    const std::string source = editor->GetText().ToStdString();
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
    const auto changed = replace_all_matches(editor->GetText().ToStdString(), find_query_,
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

void EditorNotebook::OnPageChanged(wxAuiNotebookEvent& event) {
    analysis_timer_.Stop();
    AnalyzeActive();
    if (!find_query_.empty()) RefreshFindHighlights();
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

bool EditorNotebook::SaveEditor(wxStyledTextCtrl* editor, const wxString& path) {
    wxFile file;
    if (!file.Create(path, true) || !file.Write(editor->GetText(), wxConvUTF8) || !file.Close()) {
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
    DeletePage(static_cast<size_t>(selection));
    if (GetPageCount() == 0) {
        NewTab();
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

    auto* editor = EditorAt(static_cast<size_t>(selection));
    speakers_.erase(editor);
    completions_.erase(editor);
    event.Skip();
}

void EditorNotebook::OnPageClosed(wxAuiNotebookEvent& event) {
    RefreshCompletionIndex();
    if (GetPageCount() == 0) {
        NewTab();
    }
    event.Skip();
}

}  // namespace say_count::ui
