#pragma once

#include <optional>

#include <wx/gdicmn.h>
#include <wx/string.h>

namespace say_count::app {

struct WindowSettings {
    wxPoint position;
    wxSize size;
    bool maximized = false;
};

class Settings final {
public:
    Settings();

    std::optional<WindowSettings> LoadWindow() const;
    bool SaveWindow(const WindowSettings& window) const;
    const wxString& path() const { return path_; }

private:
    wxString directory_;
    wxString path_;
};

}  // namespace say_count::app
