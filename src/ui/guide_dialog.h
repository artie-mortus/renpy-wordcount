#pragma once

#include <wx/dialog.h>

class wxHtmlWindow;

namespace say_count::ui {

enum class GuideKind { Prose, Chat };

class GuideDialog final : public wxDialog {
public:
    GuideDialog(wxWindow* parent, GuideKind kind);
    GuideDialog(wxWindow* parent, const wxString& title, const wxString& content_name,
                const char* utf8_html);

    bool content_loaded() const { return content_loaded_; }
    wxHtmlWindow* content_page() const { return content_page_; }

private:
    bool content_loaded_ = false;
    wxHtmlWindow* content_page_ = nullptr;
};

}  // namespace say_count::ui
