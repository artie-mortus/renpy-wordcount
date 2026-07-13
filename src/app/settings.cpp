#include "app/settings.h"

#include <fstream>
#include <algorithm>
#include <cctype>
#include <regex>
#include <sstream>
#include <string>

#include <wx/filename.h>
#include <wx/log.h>
#include <wx/utils.h>

namespace say_count::app {
namespace {

std::optional<int> ReadInt(const std::string& json, const char* key) {
    const std::regex expression(std::string("\\\"") + key + "\\\"\\s*:\\s*(-?[0-9]+)");
    std::smatch match;
    if (!std::regex_search(json, match, expression)) {
        return std::nullopt;
    }
    try {
        return std::stoi(match[1].str());
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<bool> ReadBool(const std::string& json, const char* key) {
    const std::regex expression(std::string("\\\"") + key + "\\\"\\s*:\\s*(true|false)");
    std::smatch match;
    if (!std::regex_search(json, match, expression)) {
        return std::nullopt;
    }
    return match[1].str() == "true";
}

std::optional<std::string> ReadString(const std::string& json, const char* key) {
    const std::regex expression(std::string("\\\"") + key + "\\\"\\s*:\\s*\\\"([^\\\"]*)\\\"");
    std::smatch match;
    if (!std::regex_search(json, match, expression)) return std::nullopt;
    return match[1].str();
}

std::optional<std::string> ReadFile(const wxString& path) {
    std::ifstream input(path.ToStdString());
    if (!input) return std::nullopt;
    std::ostringstream contents;
    contents << input.rdbuf();
    if (!input.good() && !input.eof()) return std::nullopt;
    return contents.str();
}

std::string JsonEscape(const std::string& value) {
    std::string escaped;
    for (const char c : value) {
        if (c == '\\' || c == '"') { escaped.push_back('\\'); escaped.push_back(c); }
        else if (c == '\n') escaped += "\\n";
        else if (c == '\r') escaped += "\\r";
        else if (c == '\t') escaped += "\\t";
        else escaped.push_back(c);
    }
    return escaped;
}

std::vector<wxString> ReadStringArray(const std::string& json, const char* key) {
    std::vector<wxString> values;
    const auto key_position = json.find(std::string("\"") + key + "\"");
    if (key_position == std::string::npos) return values;
    const auto open = json.find('[', key_position);
    if (open == std::string::npos) return values;
    for (std::size_t index = open + 1; index < json.size();) {
        while (index < json.size() && std::isspace(static_cast<unsigned char>(json[index]))) ++index;
        if (index >= json.size() || json[index] == ']') break;
        if (json[index] == ',') { ++index; continue; }
        if (json[index] != '"') break;
        ++index;
        std::string value;
        bool escaped = false;
        for (; index < json.size(); ++index) {
            const char c = json[index];
            if (escaped) {
                if (c == 'n') value.push_back('\n');
                else if (c == 'r') value.push_back('\r');
                else if (c == 't') value.push_back('\t');
                else value.push_back(c);
                escaped = false;
            } else if (c == '\\') escaped = true;
            else if (c == '"') { ++index; break; }
            else value.push_back(c);
        }
        values.push_back(wxString::FromUTF8(value));
    }
    return values;
}

}  // namespace

Settings::Settings() {
    // wxStandardPaths 3.2 ignores FileLayout_XDG for GetUserDataDir (returns
    // legacy ~/.say-count), so resolve the XDG data directory ourselves.
    wxString base;
    if (!wxGetEnv("XDG_DATA_HOME", &base) || base.empty()) {
        base = wxGetHomeDir() + wxFILE_SEP_PATH + ".local" + wxFILE_SEP_PATH + "share";
    }
    directory_ = base + wxFILE_SEP_PATH + "say-count";
    path_ = directory_ + wxFILE_SEP_PATH + "settings.json";
}

std::optional<WindowSettings> Settings::LoadWindow() const {
    const auto contents = ReadFile(path_);
    if (!contents) return std::nullopt;
    const std::string& json = *contents;
    const auto x = ReadInt(json, "x");
    const auto y = ReadInt(json, "y");
    const auto width = ReadInt(json, "width");
    const auto height = ReadInt(json, "height");
    const auto maximized = ReadBool(json, "maximized");
    if (!x || !y || !width || !height || !maximized || *width < 400 || *height < 300) {
        return std::nullopt;
    }

    return WindowSettings{wxPoint(*x, *y), wxSize(*width, *height), *maximized};
}

EditorSettings Settings::LoadEditor() const {
    EditorSettings result;
    const auto contents = ReadFile(path_);
    if (!contents) return result;
    if (const auto size = ReadInt(*contents, "fontSize")) result.font_size = std::clamp(*size, 10, 32);
    if (const auto wrap = ReadBool(*contents, "wordWrap")) result.word_wrap = *wrap;
    if (const auto theme = ReadString(*contents, "theme")) {
        if (*theme == "light") result.theme = EditorTheme::Light;
        else if (*theme == "dark") result.theme = EditorTheme::Dark;
    }
    if (const auto path = ReadString(*contents, "renpySdkPath"))
        result.renpy_sdk_path = wxString::FromUTF8(*path);
    return result;
}

bool Settings::SaveWindow(const WindowSettings& window) const {
    return Write(window, LoadEditor(), LoadRecentProjects());
}

bool Settings::SaveEditor(const EditorSettings& editor) const {
    return Write(LoadWindow(), editor, LoadRecentProjects());
}

std::vector<wxString> Settings::LoadRecentProjects() const {
    const auto contents = ReadFile(path_);
    return contents ? ReadStringArray(*contents, "recentProjects") : std::vector<wxString>{};
}

bool Settings::SaveRecentProjects(const std::vector<wxString>& projects) const {
    return Write(LoadWindow(), LoadEditor(), projects);
}

bool Settings::Write(const std::optional<WindowSettings>& window, const EditorSettings& editor,
                     const std::vector<wxString>& recent_projects) const {
    if (!wxFileName::DirExists(directory_) && !wxFileName::Mkdir(directory_, wxS_DIR_DEFAULT, wxPATH_MKDIR_FULL)) {
        wxLogWarning("Could not create settings directory: %s", directory_);
        return false;
    }

    const wxString temporary = path_ + ".tmp";
    std::ofstream output(temporary.ToStdString(), std::ios::trunc);
    const char* theme = editor.theme == EditorTheme::Light ? "light" :
                        editor.theme == EditorTheme::Dark ? "dark" : "system";
    output << "{\n";
    if (window) {
        output << "  \"window\": {\n"
               << "    \"x\": " << window->position.x << ",\n"
               << "    \"y\": " << window->position.y << ",\n"
               << "    \"width\": " << window->size.x << ",\n"
               << "    \"height\": " << window->size.y << ",\n"
               << "    \"maximized\": " << (window->maximized ? "true" : "false") << "\n"
               << "  },\n";
    }
    output << "  \"editor\": {\n"
           << "    \"fontSize\": " << editor.font_size << ",\n"
           << "    \"wordWrap\": " << (editor.word_wrap ? "true" : "false") << ",\n"
           << "    \"theme\": \"" << theme << "\",\n"
           << "    \"renpySdkPath\": \"" << JsonEscape(editor.renpy_sdk_path.ToStdString()) << "\"\n"
           << "  },\n"
           << "  \"recentProjects\": [";
    for (std::size_t index = 0; index < recent_projects.size(); ++index) {
        if (index) output << ", ";
        output << "\"" << JsonEscape(recent_projects[index].ToStdString()) << "\"";
    }
    output << "]\n}\n";
    output.close();
    if (!output) {
        wxLogWarning("Could not write settings file: %s", path_);
        wxRemoveFile(temporary);
        return false;
    }

    if (wxFileName::FileExists(path_) && !wxRemoveFile(path_)) {
        wxRemoveFile(temporary);
        return false;
    }
    if (!wxRenameFile(temporary, path_)) {
        wxRemoveFile(temporary);
        return false;
    }
    return true;
}

}  // namespace say_count::app
