#pragma once

#include <wx/aui/auibook.h>
#include <wx/string.h>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <string>

#include "app/settings.h"

class wxStyledTextCtrl;
class wxStyledTextEvent;

namespace say_count::ui {

class EditorNotebook final : public wxAuiNotebook {
public:
    EditorNotebook(wxWindow* parent, const app::EditorSettings& settings);

    void NewTab();
    bool OpenFiles(const std::vector<wxString>& paths);
    bool SaveCurrent();
    bool SaveCurrentAs();
    void CloseCurrentTab();
    bool ConfirmCloseAll();
    void SetWordWrap(bool enabled);
    void SetFontSize(int size);
    void SetTheme(app::EditorTheme theme);

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
    void RefreshSpeakers(wxStyledTextCtrl* editor);

    unsigned int next_untitled_number_ = 1;
    app::EditorSettings settings_;
    std::unordered_map<wxStyledTextCtrl*, std::unordered_set<std::string>> speakers_;
};

}  // namespace say_count::ui
