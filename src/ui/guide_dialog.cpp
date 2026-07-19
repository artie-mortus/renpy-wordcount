#include "ui/guide_dialog.h"

#include <wx/button.h>
#include <wx/html/htmlwin.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "guide_content.h"
#include "ui/style.h"

namespace say_count::ui {
namespace {

struct GuideDefinition {
    const char* title;
    const char* content_name;
    const char* html;
};

GuideDefinition DefinitionFor(GuideKind kind) {
    switch (kind) {
        case GuideKind::Prose:
            return {"Prose Writing Guide", "Prose writing guide content", guide_content::kProse};
        case GuideKind::Chat:
            return {"Chat Format Guide", "Chat format guide content", guide_content::kChat};
    }
    return {"Writing Guide", "Writing guide content", ""};
}

bool HasVisibleText(const wxString& html) {
    bool inside_tag = false;
    for (const wxUniChar character : html) {
        if (character == '<') {
            inside_tag = true;
        } else if (character == '>') {
            inside_tag = false;
        } else if (!inside_tag && character != ' ' && character != '\t' &&
                   character != '\r' && character != '\n') {
            return true;
        }
    }
    return false;
}

}  // namespace

GuideDialog::GuideDialog(wxWindow* parent, GuideKind kind)
    : GuideDialog(parent, wxString::FromUTF8(DefinitionFor(kind).title),
                  wxString::FromUTF8(DefinitionFor(kind).content_name),
                  DefinitionFor(kind).html) {}

GuideDialog::GuideDialog(wxWindow* parent, const wxString& title,
                         const wxString& content_name, const char* utf8_html)
    : wxDialog(parent, wxID_ANY, title, wxDefaultPosition, wxSize(820, 720),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* page = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                  wxHW_SCROLLBAR_AUTO | wxBORDER_NONE, content_name);
    page->SetBorders(18);
    const wxString html = utf8_html ? wxString::FromUTF8(utf8_html) : wxString{};
    content_loaded_ = HasVisibleText(html) && page->SetPage(html);

    if (content_loaded_) {
        content_page_ = page;
        layout->Add(page, 1, wxEXPAND);
    } else {
        delete page;
        auto* fallback = new wxPanel(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                     wxBORDER_NONE, "Guide display error");
        fallback->SetBackgroundColour(wxColour("#FCFBF8"));
        auto* fallback_layout = new wxBoxSizer(wxVERTICAL);
        fallback_layout->AddStretchSpacer();
        auto* heading = new wxStaticText(fallback, wxID_ANY, "This guide could not be displayed.");
        heading->SetFont(style::BodyFont(16, wxFONTWEIGHT_BOLD));
        heading->SetForegroundColour(wxColour("#17243A"));
        fallback_layout->Add(heading, 0, wxALIGN_CENTER | wxLEFT | wxRIGHT, 24);
        auto* detail = new wxStaticText(
            fallback, wxID_ANY,
            "Restart Say Count. If the guide is still unavailable, reinstall the application.");
        detail->SetForegroundColour(wxColour("#4C5A6D"));
        fallback_layout->Add(detail, 0, wxALIGN_CENTER | wxALL, 16);
        fallback_layout->AddStretchSpacer();
        fallback->SetSizer(fallback_layout);
        layout->Add(fallback, 1, wxEXPAND);
    }

    auto* actions = new wxStdDialogButtonSizer();
    actions->AddButton(new wxButton(this, wxID_OK, "Close guide"));
    actions->Realize();
    layout->Add(actions, 0, wxEXPAND | wxALL, 12);
    SetSizer(layout);
    CentreOnParent();
}

}  // namespace say_count::ui
