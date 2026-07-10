#include "ui/editor_notebook.h"

#include <wx/file.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/stc/stc.h>

namespace say_count::ui {
namespace {

constexpr const char* kFilePathProperty = "say-count-file-path";

wxString NormalizeTabs(wxString text) {
    text.Replace("\t", "    ");
    return text;
}

}  // namespace

EditorNotebook::EditorNotebook(wxWindow* parent)
    : wxAuiNotebook(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                    wxAUI_NB_DEFAULT_STYLE | wxAUI_NB_CLOSE_ON_ALL_TABS) {
    Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSE, &EditorNotebook::OnPageClose, this);
    Bind(wxEVT_AUINOTEBOOK_PAGE_CLOSED, &EditorNotebook::OnPageClosed, this);
    NewTab();
}

void EditorNotebook::NewTab() {
    auto* editor = new wxStyledTextCtrl(this, wxID_ANY);
    const wxString title = wxString::Format("scene-%u.rpy", next_untitled_number_++);
    editor->SetName(title);
    editor->SetSavePoint();
    editor->Bind(wxEVT_STC_SAVEPOINTLEFT, &EditorNotebook::OnSavePointChanged, this);
    editor->Bind(wxEVT_STC_SAVEPOINTREACHED, &EditorNotebook::OnSavePointChanged, this);
    AddPage(editor, title, true);
    editor->SetFocus();
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
        editor->SetText(NormalizeTabs(content));
        SetFilePath(editor, absolute_path);
        editor->SetSavePoint();
        editor->Bind(wxEVT_STC_SAVEPOINTLEFT, &EditorNotebook::OnSavePointChanged, this);
        editor->Bind(wxEVT_STC_SAVEPOINTREACHED, &EditorNotebook::OnSavePointChanged, this);
        AddPage(editor, editor->GetName(), true);
        editor->SetFocus();
        opened = true;
    }
    return opened;
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

    event.Skip();
}

void EditorNotebook::OnPageClosed(wxAuiNotebookEvent& event) {
    if (GetPageCount() == 0) {
        NewTab();
    }
    event.Skip();
}

}  // namespace say_count::ui
