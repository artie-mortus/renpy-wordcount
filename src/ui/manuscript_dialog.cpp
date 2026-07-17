#include "ui/manuscript_dialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dataview.h>
#include <wx/dialog.h>
#include <wx/filepicker.h>
#include <wx/filename.h>
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
<h1>Write in the script editor. Convert when ready.</h1>
<p>Type prose directly in the normal script tab, then click <b>Convert prose</b> in the top toolbar. A review ledger shows exactly what will change before the conversion makes one undoable edit.</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr><td>
<b>Whole tab:</b> With nothing selected, the button converts the entire active tab and adds character definitions plus <tt>label start:</tt>.<br><br>
<b>Selection:</b> Select part of the tab first to convert only that prose. The result is indented for the current Ren'Py label and assumes its character aliases are already defined.
</td></tr></table>
<p><b>Safe around existing code:</b> Recognizable Ren'Py lines are copied unchanged while nearby prose is converted. A code-only tab is left byte-for-byte intact. You can still select a smaller passage when you want tighter control.</p>
<p>The review ledger marks clear manuscript syntax as <b>PROSE</b>, existing script as <b>REN'PY</b>, and ordinary ambiguous sentences as <b>REVIEW</b>. Review lines remain unchanged unless you explicitly include them.</p>
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
<tt>“Not yet,” Lucy whispered.</tt><br>
<tt>“Wait.” Eileen grabbed his sleeve.</tt><br>
<tt>Eileen looked away. “I can’t.”</tt><br>
<tt>“Why?” asked Lucy.</tt>
</td></tr></table>
<p>Speech attributions and action beats preserve the spoken words and keep the action as narration. Common verbs such as <tt>said, asked, whispered, shouted, replied, hissed, snapped, sighed, warned</tt>, and more are recognized.</p>

<p>Present-tense tags and more conversational phrasing work too: <tt>Eileen says with a grin, "Ready."</tt>, <tt>“Almost,” replies Lucy softly.</tt>, and interrupted quotations such as <tt>“I mean it,” Eileen insists. “Today.”</tt>.</p>

<p>Quotation marks may be straight double, curly double or single, guillemets (<tt>«like this»</tt>), German-style, or CJK corner brackets. You can also use <tt>Eileen — We should go.</tt> (an em dash, en dash, or spaced hyphen).</p>

<p><b>Speaker on a separate line</b> also works when the following line is quoted:</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr><td><tt>Eileen<br>“We should go.”</tt></td></tr></table>

<p><b>Screenplay style</b> accepts an uppercase speaker cue followed by unquoted dialogue. An optional parenthetical mood is understood:</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr><td><tt>EILEEN<br>(angrily)<br>We are leaving.</tt></td></tr></table>
<p>A name followed by a colon on its own line also starts an unquoted dialogue block.</p>

<h3>Sprite expressions and image attributes</h3>
<p>Put attributes in square brackets after the speaker. They become Ren'Py say-statement image attributes:</p>
<table width="100%" cellpadding="8" bgcolor="#EEF1F3"><tr>
<td width="48%"><tt>Eileen [happy]: We did it.</tt></td>
<td><tt>eileen happy "We did it."</tt></td>
</tr></table>
<p>Use spaces or commas for multiple attributes: <tt>Lucy [winter, coat]: I'm freezing.</tt> becomes <tt>lucy winter coat "I'm freezing."</tt>. Attribute names ignore capitalization and are written lowercase. Invalid tags remain narration instead of becoming executable code.</p>
<p><b>You can also write the mood naturally.</b> Clear cues such as <tt>Eileen said happily</tt>, <tt>Lucy whispered nervously</tt>, or <tt>Eileen looked angry</tt> infer the common attributes <tt>happy</tt>, <tt>nervous</tt>, and <tt>angry</tt>. Supported common moods include happy, angry, sad, nervous, surprised, and embarrassed. Use a bracket tag when your project uses a custom sprite name or when you want an exact override.</p>
<p>When conversion creates a character that uses expressions, its definition receives a matching image tag, such as <tt>Character("Eileen", image="eileen")</tt>, so Ren'Py can apply those sprite attributes.</p>

<h3>Action beats and pronouns</h3>
<p>Names can appear after introductory phrases or as possessives: <tt>“No.” With a sigh, Eileen turned.</tt> and <tt>Eileen's hand tightened. “Go.”</tt> are recognized. Adverbs around speech tags also work.</p>
<p><tt>She</tt>, <tt>he</tt>, and <tt>they</tt> resolve only to the most recently explicit speaker. The converter never guesses a character's gender. Without a recent speaker, the line stays narration for review.</p>

<hr>
<h2>Narration</h2>
<p>Any ordinary line without a recognized speaker is marked <b>REVIEW</b>. Include it in the review window to make it narrator text:</p>
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
<p>Existing characters are matched across every open project script by display name or alias, without regard to capitalization. For example, <tt>Eileen:</tt>, <tt>eileen:</tt>, and <tt>EILEEN:</tt> all reuse an existing <tt>e = Character("Eileen")</tt> definition.</p>
<p>Matching uses full Unicode case folding, so names such as <tt>Élodie</tt>, <tt>ÉLODIE</tt>, and <tt>élodie</tt> resolve to the same character.</p>

<hr>
<h2>What conversion preserves</h2>
<ul>
<li>Blank lines are ignored, so use them freely for readability.</li>
<li>Straight and curly outer quotation marks are removed.</li>
<li>Quotes and backslashes inside dialogue are escaped.</li>
<li>Opening square brackets are doubled so Ren'Py prints them literally.</li>
<li>The final status reports converted prose, preserved code, reused characters, new characters, and lines still needing review.</li>
</ul>

<h2>Optional offline AI</h2>
<p>Choose <b>Edit → Configure Offline Prose AI</b> to use a locally installed <tt>llama-cli</tt> runner and GGUF model. Say Count never downloads a model and never calls a cloud AI service.</p>
<table width="100%" cellpadding="8" bgcolor="#E7F2EC"><tr><td>
<b>LOCAL ONLY · NETWORK BLOCKED</b><br><br>
Each model run is placed in a network-isolated sandbox. Only lines marked PROSE or REVIEW enter its prompt; recognized Ren'Py code is excluded before the process starts. The model's records are validated and passed through the safe converter, then shown in this review ledger before any edit occurs.
</td></tr></table>
<p>Turn the option off at any time to use only the built-in rule-based converter.</p>

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

class OfflineAiDialog final : public wxDialog {
public:
    OfflineAiDialog(wxWindow* parent, const app::EditorSettings& settings)
        : wxDialog(parent, wxID_ANY, "Offline Prose AI", wxDefaultPosition, wxSize(720, 480),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
        auto* layout = new wxBoxSizer(wxVERTICAL);
        auto* privacy = new wxPanel(this);
        privacy->SetBackgroundColour(wxColour("#E7F2EC"));
        auto* privacy_layout = new wxBoxSizer(wxVERTICAL);
        auto* privacy_title = new wxStaticText(privacy, wxID_ANY, "LOCAL ONLY · NETWORK BLOCKED");
        privacy_title->SetFont(style::UtilityFont(9, wxFONTWEIGHT_BOLD));
        privacy_title->SetForegroundColour(wxColour("#245C43"));
        privacy_layout->Add(privacy_title, 0, wxLEFT | wxRIGHT | wxTOP, 12);
        auto* privacy_detail = new wxStaticText(privacy, wxID_ANY,
            "During conversion, the runner is placed in a network-isolated sandbox. Only prose lines "
            "are provided; recognized Ren'Py code never enters the prompt.");
        privacy_detail->Wrap(640);
        privacy_layout->Add(privacy_detail, 0, wxEXPAND | wxALL, 12);
        privacy->SetSizer(privacy_layout);
        privacy->SetMinSize(wxSize(-1, 92));
        layout->Add(privacy, 0, wxEXPAND | wxALL, 16);

        enabled_ = new wxCheckBox(this, wxID_ANY, "Use offline AI when I choose Convert prose");
        enabled_->SetValue(settings.offline_prose_ai);
        layout->Add(enabled_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
        layout->Add(new wxStaticText(this, wxID_ANY, "llama.cpp runner (llama-cli)"),
                    0, wxLEFT | wxRIGHT | wxTOP, 16);
        runner_ = new wxFilePickerCtrl(this, wxID_ANY, settings.offline_ai_runner_path,
            "Choose llama-cli", "All files|*", wxDefaultPosition, wxDefaultSize,
            wxFLP_OPEN | wxFLP_FILE_MUST_EXIST | wxFLP_USE_TEXTCTRL);
        layout->Add(runner_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        layout->Add(new wxStaticText(this, wxID_ANY, "Local GGUF model"),
                    0, wxLEFT | wxRIGHT | wxTOP, 16);
        model_ = new wxFilePickerCtrl(this, wxID_ANY, settings.offline_ai_model_path,
            "Choose a local GGUF model", "GGUF models (*.gguf)|*.gguf|All files|*",
            wxDefaultPosition, wxDefaultSize,
            wxFLP_OPEN | wxFLP_FILE_MUST_EXIST | wxFLP_USE_TEXTCTRL);
        layout->Add(model_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        auto* note = new wxStaticText(this, wxID_ANY,
            "No model is downloaded by Say Count. The rule-based converter remains available when this is off.");
        note->Wrap(660);
        note->SetForegroundColour(style::Colors().ink_soft);
        layout->Add(note, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);

        auto* actions = new wxStdDialogButtonSizer();
        actions->AddButton(new wxButton(this, wxID_OK, "Save offline AI settings"));
        actions->AddButton(new wxButton(this, wxID_CANCEL, "Cancel"));
        actions->Realize();
        layout->Add(actions, 0, wxEXPAND | wxALL, 16);
        SetSizer(layout);
        CentreOnParent();
    }

    bool Save(app::EditorSettings* settings) {
        if (!settings) return false;
        if (enabled_->GetValue() &&
            (!wxFileName::FileExists(runner_->GetPath()) || !wxFileName::FileExists(model_->GetPath()))) {
            wxMessageBox("Choose an existing llama-cli executable and GGUF model, or turn offline AI off.",
                         "Local files required", wxOK | wxICON_WARNING, this);
            return false;
        }
        settings->offline_prose_ai = enabled_->GetValue();
        settings->offline_ai_runner_path = runner_->GetPath();
        settings->offline_ai_model_path = model_->GetPath();
        return true;
    }

private:
    wxCheckBox* enabled_ = nullptr;
    wxFilePickerCtrl* runner_ = nullptr;
    wxFilePickerCtrl* model_ = nullptr;
};

}  // namespace

void ShowManuscriptGuide(wxWindow* parent) {
    ManuscriptGuideDialog(parent).ShowModal();
}

bool ConfigureOfflineProseAi(wxWindow* parent, app::EditorSettings* settings) {
    OfflineAiDialog dialog(parent, *settings);
    while (dialog.ShowModal() == wxID_OK) {
        if (dialog.Save(settings)) return true;
    }
    return false;
}

ManuscriptReviewDialog::ManuscriptReviewDialog(
    wxWindow* parent, const std::vector<ManuscriptLineReview>& lines,
    const ManuscriptConversion& safe_conversion,
    const ManuscriptConversion& inclusive_conversion, std::string_view source, bool offline_ai_used)
    : wxDialog(parent, wxID_ANY, "Review Prose Conversion", wxDefaultPosition, wxSize(1120, 740),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      safe_conversion_(safe_conversion), inclusive_conversion_(inclusive_conversion), source_(source),
      offline_ai_used_(offline_ai_used) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* kicker = new wxStaticText(this, wxID_ANY, "CONVERSION LEDGER");
    kicker->SetFont(style::UtilityFont(9, wxFONTWEIGHT_BOLD));
    kicker->SetForegroundColour(style::Colors().plum);
    layout->Add(kicker, 0, wxLEFT | wxRIGHT | wxTOP, 16);
    auto* heading = new wxStaticText(this, wxID_ANY, "Review what changes before it touches your script");
    heading->SetFont(style::BodyFont(14, wxFONTWEIGHT_BOLD));
    layout->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, 16);
    summary_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    summary_->SetForegroundColour(style::Colors().ink_soft);
    layout->Add(summary_, 0, wxEXPAND | wxALL, 16);
    if (offline_ai_used_) {
        auto* local = new wxStaticText(this, wxID_ANY,
            "LOCAL AI · Network blocked · Prose interpreted on this computer");
        local->SetFont(style::UtilityFont(9, wxFONTWEIGHT_BOLD));
        local->SetForegroundColour(wxColour("#245C43"));
        layout->Add(local, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
    }

    auto* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                           wxSP_LIVE_UPDATE | wxSP_3D);
    splitter->SetMinimumPaneSize(320);
    auto* ledger_panel = new wxPanel(splitter);
    auto* ledger_layout = new wxBoxSizer(wxVERTICAL);
    ledger_layout->Add(new wxStaticText(ledger_panel, wxID_ANY, "Source classification"),
                       0, wxBOTTOM, 7);
    auto* ledger = new wxDataViewListCtrl(ledger_panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                          wxDV_ROW_LINES | wxDV_VERT_RULES);
    ledger->AppendTextColumn("State", wxDATAVIEW_CELL_INERT, 90);
    ledger->AppendTextColumn("Line", wxDATAVIEW_CELL_INERT, 55, wxALIGN_RIGHT);
    ledger->AppendTextColumn("Source", wxDATAVIEW_CELL_INERT, 370);
    for (const auto& line : lines) {
        wxString state;
        switch (line.kind) {
            case ManuscriptLineKind::Prose: state = "PROSE"; ++prose_lines_; break;
            case ManuscriptLineKind::Renpy: state = "REN'PY"; ++renpy_lines_; break;
            case ManuscriptLineKind::Uncertain: state = "REVIEW"; ++uncertain_lines_; break;
            case ManuscriptLineKind::Blank: state = ""; break;
        }
        wxVector<wxVariant> row;
        row.push_back(wxVariant(state));
        row.push_back(wxVariant(line.kind == ManuscriptLineKind::Blank
            ? wxString{} : wxString::Format("%zu", line.line_number)));
        row.push_back(wxVariant(wxString::FromUTF8(line.text)));
        ledger->AppendItem(row);
    }
    ledger_layout->Add(ledger, 1, wxEXPAND);
    ledger_panel->SetSizer(ledger_layout);

    auto* preview_panel = new wxPanel(splitter);
    auto* preview_layout = new wxBoxSizer(wxVERTICAL);
    preview_layout->Add(new wxStaticText(preview_panel, wxID_ANY, "Exact Ren'Py result"),
                        0, wxBOTTOM, 7);
    preview_ = new wxTextCtrl(preview_panel, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                              wxTE_MULTILINE | wxTE_READONLY | wxTE_RICH2);
    preview_->SetFont(style::UtilityFont(10));
    preview_layout->Add(preview_, 1, wxEXPAND);
    preview_panel->SetSizer(preview_layout);
    splitter->SplitVertically(ledger_panel, preview_panel, 540);
    layout->Add(splitter, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);

    include_uncertain_ = new wxCheckBox(this, wxID_ANY,
        wxString::Format("Include %zu uncertain line%s", uncertain_lines_,
                         uncertain_lines_ == 1 ? wxString{} : wxString("s")));
    include_uncertain_->SetValue(offline_ai_used_);
    include_uncertain_->Enable(uncertain_lines_ > 0 && !offline_ai_used_);
    if (offline_ai_used_ && uncertain_lines_ > 0)
        include_uncertain_->SetLabel(wxString::Format("Local AI resolved %zu review line%s",
            uncertain_lines_, uncertain_lines_ == 1 ? wxString{} : wxString("s")));
    layout->Add(include_uncertain_, 0, wxALL, 16);

    auto* actions = new wxStdDialogButtonSizer();
    convert_ = new wxButton(this, wxID_OK, "Convert prose");
    style::StylePrimaryButton(convert_);
    actions->AddButton(convert_);
    actions->AddButton(new wxButton(this, wxID_CANCEL, "Cancel"));
    actions->Realize();
    layout->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    SetSizer(layout);
    include_uncertain_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { RefreshPreview(); });
    RefreshPreview();
    CentreOnParent();
}

bool ManuscriptReviewDialog::include_uncertain_lines() const {
    return include_uncertain_ && include_uncertain_->GetValue();
}

const ManuscriptConversion& ManuscriptReviewDialog::selected_conversion() const {
    return include_uncertain_lines() ? inclusive_conversion_ : safe_conversion_;
}

std::size_t ManuscriptReviewDialog::converted_line_count() const {
    return prose_lines_ + (include_uncertain_lines() ? uncertain_lines_ : 0);
}

void ManuscriptReviewDialog::RefreshPreview() {
    const auto& conversion = selected_conversion();
    preview_->ChangeValue(wxString::FromUTF8(conversion.script));
    const std::size_t converted = converted_line_count();
    const std::size_t pending = include_uncertain_lines() ? 0 : uncertain_lines_;
    const std::string text = std::to_string(converted) + " prose lines selected  ·  " +
        std::to_string(renpy_lines_) + " code lines preserved  ·  " +
        std::to_string(conversion.reused_aliases.size()) +
        (conversion.reused_aliases.size() == 1 ? " character reused  ·  " : " characters reused  ·  ") +
        std::to_string(conversion.characters.size()) +
        (conversion.characters.size() == 1 ? " character created  ·  " : " characters created  ·  ") +
        std::to_string(pending) + " still need review";
    summary_->SetLabel(wxString::FromUTF8(text));
    convert_->SetLabel(wxString::FromUTF8("Convert " + std::to_string(converted) +
                       (converted == 1 ? " line" : " lines")));
    convert_->Enable(converted > 0 && conversion.script != source_);
}

}  // namespace say_count::ui
