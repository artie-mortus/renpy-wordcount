#pragma once

#include <wx/aui/auibook.h>
#include <wx/string.h>

#include <vector>

class wxStyledTextCtrl;
class wxStyledTextEvent;

namespace say_count::ui {

class EditorNotebook final : public wxAuiNotebook {
public:
    explicit EditorNotebook(wxWindow* parent);

    void NewTab();
    bool OpenFiles(const std::vector<wxString>& paths);
    bool SaveCurrent();
    bool SaveCurrentAs();
    void CloseCurrentTab();
    bool ConfirmCloseAll();

private:
    wxStyledTextCtrl* EditorAt(size_t index) const;
    wxString FilePath(wxStyledTextCtrl* editor) const;
    void SetFilePath(wxStyledTextCtrl* editor, const wxString& path);
    bool SaveEditor(wxStyledTextCtrl* editor, const wxString& path);
    bool ConfirmDiscard(size_t index);
    void UpdateTabLabel(wxStyledTextCtrl* editor);
    void OnPageClose(wxAuiNotebookEvent& event);
    void OnPageClosed(wxAuiNotebookEvent& event);
    void OnSavePointChanged(wxStyledTextEvent& event);

    unsigned int next_untitled_number_ = 1;
};

}  // namespace say_count::ui
