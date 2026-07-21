#pragma once

#include <functional>
#include <string>
#include <vector>

#include <wx/panel.h>

#include "core/project.h"

class wxButton;
class wxListBox;
class wxStaticText;
class wxTextCtrl;

namespace say_count::ui {

class ProjectNavigatorPanel final : public wxPanel {
public:
    using OpenHandler = std::function<void(std::size_t)>;

    explicit ProjectNavigatorPanel(wxWindow* parent);

    void SetProject(std::vector<ProjectScriptFile> scripts);
    void SetOpenHandler(OpenHandler handler);
    void SelectPath(const wxString& absolute_path);
    std::size_t script_count() const { return scripts_.size(); }

private:
    void RefreshList();
    void OpenSelection();

    wxTextCtrl* search_ = nullptr;
    wxListBox* list_ = nullptr;
    wxStaticText* summary_ = nullptr;
    wxStaticText* empty_ = nullptr;
    wxButton* open_ = nullptr;
    std::vector<ProjectScriptFile> scripts_;
    std::vector<std::size_t> visible_scripts_;
    OpenHandler open_handler_;
};

}  // namespace say_count::ui
