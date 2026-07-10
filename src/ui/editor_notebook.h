#pragma once

#include <wx/aui/auibook.h>

class wxStyledTextCtrl;
class wxStyledTextEvent;

namespace say_count::ui {

class EditorNotebook final : public wxAuiNotebook {
public:
    explicit EditorNotebook(wxWindow* parent);

    void NewTab();
    void CloseCurrentTab();
    bool ConfirmCloseAll();

private:
    wxStyledTextCtrl* EditorAt(size_t index) const;
    bool ConfirmDiscard(size_t index);
    void UpdateTabLabel(wxStyledTextCtrl* editor);
    void OnPageClose(wxAuiNotebookEvent& event);
    void OnPageClosed(wxAuiNotebookEvent& event);
    void OnSavePointChanged(wxStyledTextEvent& event);

    unsigned int next_untitled_number_ = 1;
};

}  // namespace say_count::ui
