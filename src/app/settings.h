#pragma once

#include <optional>
#include <vector>

#include <wx/gdicmn.h>
#include <wx/string.h>

namespace say_count::app {

struct WindowSettings {
    wxPoint position;
    wxSize size;
    bool maximized = false;
};

enum class EditorTheme { System, Light, Dark };

struct EditorSettings {
    int font_size = 16;
    bool word_wrap = false;
    bool nvim_motions = false;
    EditorTheme theme = EditorTheme::System;
    wxString renpy_sdk_path;
    bool offline_prose_ai = false;
    wxString offline_ai_runner_path;
    wxString offline_ai_model_path;
    int onboarding_version = 0;
};

class Settings final {
public:
    Settings();

    std::optional<WindowSettings> LoadWindow() const;
    EditorSettings LoadEditor() const;
    bool SaveWindow(const WindowSettings& window) const;
    bool SaveEditor(const EditorSettings& editor) const;
    std::vector<wxString> LoadRecentProjects() const;
    bool SaveRecentProjects(const std::vector<wxString>& projects) const;
    const wxString& path() const { return path_; }
    const wxString& data_directory() const { return directory_; }

private:
    bool Write(const std::optional<WindowSettings>& window, const EditorSettings& editor,
               const std::vector<wxString>& recent_projects) const;
    wxString directory_;
    wxString path_;
};

}  // namespace say_count::app
