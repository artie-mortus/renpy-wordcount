#include "ui/manuscript_dialog.h"

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/html/htmlwin.h>
#include <wx/sizer.h>

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
<h1>Write in the script editor. Convert when ready.</h1>
<p>Type prose directly in the normal script tab, then click <b>Convert prose</b> in the top toolbar. The conversion replaces the prose in one undoable edit.</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr><td>
<b>Whole tab:</b> With nothing selected, the button converts the entire active tab and adds character definitions plus <tt>label start:</tt>.<br><br>
<b>Selection:</b> Select part of the tab first to convert only that prose. The result is indented for the current Ren'Py label and assumes its character aliases are already defined.
</td></tr></table>
<p>The converter turns each non-empty prose line into one Ren'Py statement. You can mix every format on this page in the same conversion.</p>

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
<p>Converting a whole tab creates <tt>label start:</tt>. A <tt>::</tt> heading starts each additional scene.</p>

<hr>
<h2>Characters</h2>
<p>When converting a whole tab, the converter creates character definitions above the scene:</p>
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
<p><b>If the result is not what you expected, use Undo.</b> Make the speaker clearer with <tt>Name: dialogue</tt>, or wrap narration in straight quotes, then convert again.</p>
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

}  // namespace say_count::ui
