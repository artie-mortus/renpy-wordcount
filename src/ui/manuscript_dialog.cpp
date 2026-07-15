#include "ui/manuscript_dialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/html/htmlwin.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/style.h"

namespace say_count::ui {
namespace {

class ManuscriptGuideDialog final : public wxDialog {
public:
    explicit ManuscriptGuideDialog(wxWindow* parent)
        : wxDialog(parent, wxID_ANY, "Prose Writing Guide", wxDefaultPosition, wxSize(820, 720),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
        auto* layout = new wxBoxSizer(wxVERTICAL);
        auto* page = new wxHtmlWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                      wxHW_SCROLLBAR_AUTO | wxBORDER_NONE);
        page->SetBorders(18);
        page->SetPage(R"HTML(
<html><body bgcolor="#FCFBF8" text="#17243A">
<font face="DejaVu Sans">
<font color="#76546E" size="2"><b>MANUSCRIPT → REN'PY</b></font>
<h1>Write naturally. Keep the speaker clear.</h1>
<p>The converter turns each non-empty manuscript line into one Ren'Py statement. You can mix every format on this page in the same scene.</p>

<hr>
<h2>Dialogue</h2>
<p><b>Name and a colon</b> is the clearest form:</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr>
<td width="48%"><font size="2" color="#4C5A6D">WRITE</font><br><tt>Eileen: We should go.</tt></td>
<td><font size="2" color="#4C5A6D">BECOMES</font><br><tt>eileen "We should go."</tt></td>
</tr></table>

<p><b>Book-style attribution</b> works before or after the quote:</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr><td>
<tt>Eileen said, "We should go."</tt><br>
<tt>“Not yet,” Lucy whispered.</tt>
</td></tr></table>
<p>Recognized speech verbs: <tt>said, asked, replied, answered, whispered, shouted, yelled, murmured, muttered, cried, called, added, continued</tt>.</p>

<p><b>Speaker on a separate line</b> also works when the following line is quoted:</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr><td><tt>Eileen<br>“We should go.”</tt></td></tr></table>

<hr>
<h2>Narration</h2>
<p>Any ordinary line without a recognized speaker becomes narrator text:</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr>
<td width="48%"><tt>Rain tapped against the glass.</tt></td>
<td><tt>"Rain tapped against the glass."</tt></td>
</tr></table>
<p>You may wrap narration in straight or curly quotes. This is useful when narration contains a colon:</p>
<p><tt>"The clock read: midnight."</tt></p>

<hr>
<h2>Scenes and labels</h2>
<p>Start a line with two colons to create another label. Spaces and punctuation are normalized for Ren'Py:</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr>
<td width="48%"><tt>:: The Crossing</tt></td>
<td><tt>label the_crossing:</tt></td>
</tr></table>
<p><b>Opening label</b> creates the first label, usually <tt>start</tt>. Leave it blank only when inserting the result inside a label that already exists; the generated lines will still receive four-space indentation.</p>

<hr>
<h2>Characters</h2>
<p>With <b>Add Character definitions</b> enabled, the converter creates definitions above the scene:</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr><td><tt>define eileen = Character("Eileen")</tt></td></tr></table>
<p>Speaker names become lowercase aliases with underscores. For example, <tt>Captain Vale</tt> becomes <tt>captain_vale</tt>. If two names produce the same alias, later aliases receive <tt>_2</tt>, <tt>_3</tt>, and so on.</p>

<hr>
<h2>What conversion preserves</h2>
<ul>
<li>Blank lines are ignored, so use them freely for readability.</li>
<li>Straight and curly outer quotation marks are removed.</li>
<li>Quotes and backslashes inside dialogue are escaped.</li>
<li>Opening square brackets are doubled so Ren'Py prints them literally.</li>
</ul>

<h2>A complete example</h2>
<table width="100%" cellpadding="10" bgcolor="#17243A"><tr><td><font color="#FFFFFF"><tt>
Rain pressed silver lines against the window.<br><br>
Eileen: We should go.<br><br>
“Not yet,” Lucy whispered.<br><br>
:: The Crossing<br><br>
Lucy said, "The road is flooded."
</tt></font></td></tr></table>
<p><b>Check the preview before inserting.</b> If a sentence is classified incorrectly, use the explicit <tt>Name: dialogue</tt> form or wrap narration in straight quotes.</p>
</font></body></html>
)HTML");
        layout->Add(page, 1, wxEXPAND);
        auto* actions = new wxStdDialogButtonSizer();
        actions->AddButton(new wxButton(this, wxID_OK, "Close guide"));
        actions->Realize();
        layout->Add(actions, 0, wxEXPAND | wxALL, 12);
        SetSizer(layout);
        CentreOnParent();
    }
};

}  // namespace

void ShowManuscriptGuide(wxWindow* parent) {
    ManuscriptGuideDialog(parent).ShowModal();
}

ManuscriptDialog::ManuscriptDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Write Dialogue as Prose", wxDefaultPosition, wxSize(1040, 700),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
    auto* layout = new wxBoxSizer(wxVERTICAL);

    auto* title = new wxStaticText(this, wxID_ANY, "MANUSCRIPT  →  REN'PY");
    title->SetFont(style::UtilityFont(9, wxFONTWEIGHT_BOLD));
    title->SetForegroundColour(style::Colors().plum);
    layout->Add(title, 0, wxLEFT | wxRIGHT | wxTOP, 16);
    auto* guidance = new wxStaticText(this, wxID_ANY,
        "Write Name: dialogue, ordinary narration, or book-style lines such as “Not yet,” Lucy whispered. "
        "Start a new scene with :: Scene name.");
    guidance->Wrap(960);
    layout->Add(guidance, 0, wxEXPAND | wxLEFT | wxRIGHT | wxTOP, 16);

    auto* options = new wxBoxSizer(wxHORIZONTAL);
    options->Add(new wxStaticText(this, wxID_ANY, "Opening label"), 0,
                 wxALIGN_CENTER_VERTICAL | wxRIGHT, 7);
    label_ = new wxTextCtrl(this, wxID_ANY, "start", wxDefaultPosition, wxSize(180, -1));
    label_->SetHint("Leave blank when pasting inside a label");
    options->Add(label_, 0, wxRIGHT, 18);
    definitions_ = new wxCheckBox(this, wxID_ANY, "Add Character definitions");
    definitions_->SetValue(true);
    options->Add(definitions_, 0, wxALIGN_CENTER_VERTICAL);
    layout->Add(options, 0, wxEXPAND | wxALL, 16);

    auto* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                           wxSP_LIVE_UPDATE | wxSP_3D);
    splitter->SetMinimumPaneSize(260);
    auto* left = new wxPanel(splitter);
    auto* left_layout = new wxBoxSizer(wxVERTICAL);
    auto* left_label = new wxStaticText(left, wxID_ANY, "Your prose");
    left_label->SetFont(style::BodyFont(10, wxFONTWEIGHT_BOLD));
    left_layout->Add(left_label, 0, wxBOTTOM, 7);
    manuscript_ = new wxTextCtrl(left, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_RICH2);
    manuscript_->SetHint("Rain tapped against the glass.\n\nEileen: We should go.\n\n“Not yet,” Lucy whispered.");
    manuscript_->SetFont(style::BodyFont(11));
    left_layout->Add(manuscript_, 1, wxEXPAND);
    left->SetSizer(left_layout);

    auto* right = new wxPanel(splitter);
    auto* right_layout = new wxBoxSizer(wxVERTICAL);
    auto* right_label = new wxStaticText(right, wxID_ANY, "Ren'Py preview");
    right_label->SetFont(style::BodyFont(10, wxFONTWEIGHT_BOLD));
    right_layout->Add(right_label, 0, wxBOTTOM, 7);
    preview_ = new wxTextCtrl(right, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    preview_->SetFont(style::UtilityFont(10));
    right_layout->Add(preview_, 1, wxEXPAND);
    right->SetSizer(right_layout);
    splitter->SplitVertically(left, right, 500);
    layout->Add(splitter, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    summary_ = new wxStaticText(this, wxID_ANY, "Start writing to build the preview.");
    summary_->SetForegroundColour(style::Colors().ink_soft);
    layout->Add(summary_, 0, wxEXPAND | wxALL, 16);

    auto* action_row = new wxBoxSizer(wxHORIZONTAL);
    auto* guide = new wxButton(this, wxID_HELP, "Writing guide");
    action_row->Add(guide, 0, wxALIGN_CENTER_VERTICAL);
    action_row->AddStretchSpacer();
    auto* actions = new wxStdDialogButtonSizer();
    auto* insert = new wxButton(this, wxID_OK, "Convert and Insert");
    style::StylePrimaryButton(insert);
    actions->AddButton(insert);
    actions->AddButton(new wxButton(this, wxID_CANCEL, "Cancel"));
    actions->Realize();
    action_row->Add(actions, 0, wxALIGN_CENTER_VERTICAL);
    layout->Add(action_row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);

    SetSizer(layout);
    manuscript_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshPreview(); });
    label_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshPreview(); });
    definitions_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { RefreshPreview(); });
    insert->Bind(wxEVT_BUTTON, &ManuscriptDialog::Convert, this);
    guide->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowManuscriptGuide(this); });
    RefreshPreview();
    CentreOnParent();
    manuscript_->SetFocus();
}

void ManuscriptDialog::RefreshPreview() {
    if (manuscript_->GetValue().Strip(wxString::both).empty()) {
        conversion_ = {};
        preview_->ChangeValue(wxEmptyString);
        summary_->SetLabel("Start writing to build the preview.");
        return;
    }
    ManuscriptOptions options;
    options.label = label_->GetValue().Strip(wxString::both).ToStdString(wxConvUTF8);
    options.include_character_definitions = definitions_->GetValue();
    conversion_ = convert_manuscript_to_renpy(
        manuscript_->GetValue().ToStdString(wxConvUTF8), options);
    preview_->ChangeValue(wxString::FromUTF8(conversion_.script));
    summary_->SetLabel(wxString::Format("%zu spoken line%s  ·  %zu narration line%s  ·  %zu character%s",
        conversion_.dialogue_lines, conversion_.dialogue_lines == 1 ? "" : "s",
        conversion_.narration_lines, conversion_.narration_lines == 1 ? "" : "s",
        conversion_.characters.size(), conversion_.characters.size() == 1 ? "" : "s"));
}

void ManuscriptDialog::Convert(wxCommandEvent&) {
    RefreshPreview();
    if (manuscript_->GetValue().Strip(wxString::both).empty()) {
        wxMessageBox("Write at least one line before converting.", "Nothing to convert",
                     wxOK | wxICON_INFORMATION, this);
        manuscript_->SetFocus();
        return;
    }
    EndModal(wxID_OK);
}

}  // namespace say_count::ui
