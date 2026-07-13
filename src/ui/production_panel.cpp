#include "ui/production_panel.h"
#include "ui/style.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <sstream>

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace say_count::ui {
namespace {

wxButton* Button(wxWindow* parent, wxSizer* sizer, const wxString& text) {
    auto* button = new wxButton(parent, wxID_ANY, text);
    sizer->Add(button, 0, wxRIGHT, 5);
    return button;
}

long Selected(wxListCtrl* list) {
    return list ? list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED) : -1;
}

std::set<std::string> Words(const wxString& value) {
    std::set<std::string> result;
    std::istringstream input(value.ToStdString(wxConvUTF8));
    std::string word;
    while (input >> word) result.insert(word);
    return result;
}

void SectionHeader(wxPanel* panel, wxBoxSizer* layout, const wxString& eyebrow,
                   const wxString& title, const wxString& description) {
    auto* label = new wxStaticText(panel, wxID_ANY, eyebrow.Upper());
    label->SetFont(style::UtilityFont(8, wxFONTWEIGHT_BOLD));
    label->SetForegroundColour(style::Colors().plum);
    auto* heading = new wxStaticText(panel, wxID_ANY, title);
    heading->SetFont(style::BodyFont(15, wxFONTWEIGHT_BOLD));
    heading->SetForegroundColour(style::Colors().ink);
    auto* detail = new wxStaticText(panel, wxID_ANY, description);
    detail->SetFont(style::BodyFont(9));
    detail->SetForegroundColour(style::Colors().ink_soft);
    layout->Add(label, 0, wxLEFT | wxRIGHT | wxTOP, 16);
    layout->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, 16);
    layout->Add(detail, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 16);
}

}  // namespace

ProductionPanel::ProductionPanel(wxWindow* parent, std::string data_directory)
    : wxPanel(parent),
      voice_store_(data_directory + "/voice-production.dat"),
      continuity_store_(data_directory + "/continuity.dat"),
      accessibility_store_(data_directory + "/accessibility.dat") {
    voice_entries_ = voice_store_.Load();
    continuity_notes_ = continuity_store_.Load();
    acknowledged_ = accessibility_store_.Load();
    auto* layout = new wxBoxSizer(wxVERTICAL);
    notebook_ = new wxNotebook(this, wxID_ANY);
    notebook_->AddPage(BuildProse(), "Prose");
    notebook_->AddPage(BuildVoice(), "Voice");
    notebook_->AddPage(BuildTranslations(), "Translations");
    notebook_->AddPage(BuildContinuity(), "Continuity");
    notebook_->AddPage(BuildAccessibility(), "Accessibility");
    layout->Add(notebook_, 1, wxEXPAND);
    SetSizer(layout);
}

wxPanel* ProductionPanel::BuildProse() {
    auto* panel = new wxPanel(notebook_);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    SectionHeader(panel, layout, "Prose", "Language rhythm",
                  "Spot repetition and compare sentence shape across voices.");
    prose_summary_ = new wxStaticText(panel, wxID_ANY, "No dialogue yet.");
    auto* filters = new wxBoxSizer(wxHORIZONTAL);
    prose_speaker_ = new wxChoice(panel, wxID_ANY);
    prose_exclusions_ = new wxTextCtrl(panel, wxID_ANY);
    prose_exclusions_->SetHint("Exclude common words");
    filters->Add(prose_speaker_, 0, wxRIGHT, 5);
    filters->Add(prose_exclusions_, 1);
    prose_list_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    prose_list_->AppendColumn("Term", wxLIST_FORMAT_LEFT, 240);
    prose_list_->AppendColumn("Count", wxLIST_FORMAT_RIGHT, 65);
    layout->Add(prose_summary_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    layout->Add(filters, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    layout->Add(prose_list_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    panel->SetSizer(layout);
    prose_speaker_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { RefreshProse(); });
    prose_exclusions_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshProse(); });
    prose_list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event) {
        const auto index = static_cast<std::size_t>(event.GetIndex());
        if (index < prose_terms_.size() && search_handler_) search_handler_(prose_terms_[index].text);
    });
    return panel;
}

wxPanel* ProductionPanel::BuildVoice() {
    auto* panel = new wxPanel(notebook_);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    SectionHeader(panel, layout, "Voice", "Recording board",
                  "Move every cue from unrecorded to approved without losing context.");
    voice_summary_ = new wxStaticText(panel, wxID_ANY, "No dialogue found.");
    voice_list_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                 wxLC_REPORT | wxLC_SINGLE_SEL);
    voice_list_->AppendColumn("Speaker", wxLIST_FORMAT_LEFT, 110);
    voice_list_->AppendColumn("Source", wxLIST_FORMAT_LEFT, 130);
    voice_list_->AppendColumn("Text", wxLIST_FORMAT_LEFT, 260);
    voice_list_->AppendColumn("Status", wxLIST_FORMAT_LEFT, 85);
    auto* edit = new wxBoxSizer(wxHORIZONTAL);
    voice_status_ = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        {"unrecorded", "recorded", "retake", "approved"});
    voice_status_->SetSelection(0);
    voice_file_ = new wxTextCtrl(panel, wxID_ANY); voice_file_->SetHint("Voice filename");
    voice_notes_ = new wxTextCtrl(panel, wxID_ANY); voice_notes_->SetHint("Notes");
    edit->Add(voice_status_, 0, wxRIGHT, 5); edit->Add(voice_file_, 1, wxRIGHT, 5);
    edit->Add(voice_notes_, 1, wxRIGHT, 5);
    auto* save = Button(panel, edit, "Save line");
    style::StylePrimaryButton(save);
    auto* exports = new wxBoxSizer(wxHORIZONTAL);
    voice_export_speaker_ = new wxChoice(panel, wxID_ANY);
    voice_source_context_ = new wxCheckBox(panel, wxID_ANY, "Include source context");
    voice_source_context_->SetValue(true);
    exports->Add(voice_export_speaker_, 0, wxRIGHT, 5);
    auto* csv = Button(panel, exports, "Export CSV");
    auto* html = Button(panel, exports, "Printable script");
    exports->Add(voice_source_context_, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
    layout->Add(voice_summary_, 0, wxEXPAND | wxALL, 8);
    layout->Add(voice_list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);
    layout->Add(edit, 0, wxEXPAND | wxALL, 8);
    layout->Add(exports, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    panel->SetSizer(layout);
    voice_list_->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent& event) {
        const auto index = static_cast<std::size_t>(event.GetIndex());
        if (index >= voice_rows_.size()) return;
        const auto found = voice_entries_.find(voice_rows_[index].id);
        const VoiceEntry entry = found == voice_entries_.end() ? VoiceEntry{} : found->second;
        voice_status_->SetStringSelection(wxString::FromUTF8(voice_status_name(entry.status)));
        voice_file_->SetValue(wxString::FromUTF8(entry.voice_file));
        voice_notes_->SetValue(wxString::FromUTF8(entry.notes));
    });
    voice_list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent& event) {
        const auto index = static_cast<std::size_t>(event.GetIndex());
        if (index < voice_rows_.size() && jump_handler_) jump_handler_(voice_rows_[index].file, voice_rows_[index].line);
    });
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SaveVoiceSelection(); });
    csv->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ExportVoice(false); });
    html->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ExportVoice(true); });
    return panel;
}

wxPanel* ProductionPanel::BuildTranslations() {
    auto* panel = new wxPanel(notebook_); auto* layout = new wxBoxSizer(wxVERTICAL);
    SectionHeader(panel, layout, "Translations", "Localization gaps",
                  "Review untranslated lines in their source and speaker context.");
    auto* controls = new wxBoxSizer(wxHORIZONTAL);
    translation_language_ = new wxTextCtrl(panel, wxID_ANY, "spanish");
    auto* refresh = Button(panel, controls, "Scan missing"); style::StylePrimaryButton(refresh); controls->Prepend(translation_language_, 1, wxRIGHT, 8);
    translation_summary_ = new wxStaticText(panel, wxID_ANY, "Choose a language and scan.");
    translation_list_ = new wxListCtrl(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                        wxLC_REPORT | wxLC_SINGLE_SEL);
    translation_list_->AppendColumn("File", wxLIST_FORMAT_LEFT, 160);
    translation_list_->AppendColumn("Line", wxLIST_FORMAT_RIGHT, 55);
    translation_list_->AppendColumn("Context", wxLIST_FORMAT_LEFT, 130);
    translation_list_->AppendColumn("Text", wxLIST_FORMAT_LEFT, 260);
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    auto* source = Button(panel, actions, "Open source");
    auto* translated = Button(panel, actions, "Open translation");
    layout->Add(controls, 0, wxEXPAND | wxALL, 8); layout->Add(translation_summary_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 8);
    layout->Add(translation_list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 8); layout->Add(actions, 0, wxALL, 8); panel->SetSizer(layout);
    refresh->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { RefreshTranslations(); });
    source->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { const long i=Selected(translation_list_); if(i>=0&&jump_handler_) { const auto& e=translation_.missing[static_cast<std::size_t>(i)]; jump_handler_(e.source_file.empty()?e.file:e.source_file,e.line); }});
    translated->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { const long i=Selected(translation_list_); if(i>=0&&jump_handler_) { const auto& e=translation_.missing[static_cast<std::size_t>(i)]; jump_handler_(e.translation_file,e.translation_line); }});
    translation_list_->Bind(wxEVT_LIST_ITEM_ACTIVATED, [this](wxListEvent&) { const long i=Selected(translation_list_); if(i>=0&&jump_handler_) { const auto& e=translation_.missing[static_cast<std::size_t>(i)]; jump_handler_(e.source_file.empty()?e.file:e.source_file,e.line); }});
    return panel;
}

wxPanel* ProductionPanel::BuildContinuity() {
    auto* panel = new wxPanel(notebook_); auto* layout = new wxBoxSizer(wxVERTICAL);
    SectionHeader(panel, layout, "Continuity", "Story bible",
                  "Keep character, place, timeline, and inventory facts beside the script.");
    auto* add = new wxBoxSizer(wxHORIZONTAL);
    continuity_kind_ = new wxChoice(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        {"character","location","timeline","relationship","inventory","fact"}); continuity_kind_->SetSelection(0);
    continuity_subject_ = new wxTextCtrl(panel, wxID_ANY); continuity_subject_->SetHint("Subject");
    continuity_note_ = new wxTextCtrl(panel, wxID_ANY); continuity_note_->SetHint("Continuity note");
    continuity_attach_ = new wxCheckBox(panel, wxID_ANY, "Attach line"); continuity_attach_->SetValue(true);
    add->Add(continuity_kind_,0,wxRIGHT,5); add->Add(continuity_subject_,1,wxRIGHT,5); add->Add(continuity_note_,2,wxRIGHT,5); add->Add(continuity_attach_,0,wxALIGN_CENTER_VERTICAL|wxRIGHT,5);
    auto* add_button = Button(panel, add, "Add");
    style::StylePrimaryButton(add_button);
    auto* filter = new wxBoxSizer(wxHORIZONTAL); continuity_search_ = new wxTextCtrl(panel,wxID_ANY); continuity_search_->SetHint("Search notes");
    continuity_filter_kind_ = new wxChoice(panel,wxID_ANY,wxDefaultPosition,wxDefaultSize,{"All","character","location","timeline","relationship","inventory","fact"}); continuity_filter_kind_->SetSelection(0);
    filter->Add(continuity_search_,1,wxRIGHT,5); filter->Add(continuity_filter_kind_,0);
    continuity_list_ = new wxListCtrl(panel,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxLC_REPORT|wxLC_SINGLE_SEL);
    continuity_list_->AppendColumn("Kind",wxLIST_FORMAT_LEFT,90); continuity_list_->AppendColumn("Subject",wxLIST_FORMAT_LEFT,130);
    continuity_list_->AppendColumn("Note",wxLIST_FORMAT_LEFT,300); continuity_list_->AppendColumn("Source",wxLIST_FORMAT_LEFT,130);
    auto* actions=new wxBoxSizer(wxHORIZONTAL); auto* open=Button(panel,actions,"Open"); auto* remove=Button(panel,actions,"Delete");
    layout->Add(add,0,wxEXPAND|wxALL,8); layout->Add(filter,0,wxEXPAND|wxLEFT|wxRIGHT|wxBOTTOM,8); layout->Add(continuity_list_,1,wxEXPAND|wxLEFT|wxRIGHT,8); layout->Add(actions,0,wxALL,8); panel->SetSizer(layout);
    add_button->Bind(wxEVT_BUTTON,[this](wxCommandEvent&){ const auto subject=continuity_subject_->GetValue().Strip(wxString::both).ToStdString(wxConvUTF8); const auto note=continuity_note_->GetValue().Strip(wxString::both).ToStdString(wxConvUTF8); if(subject.empty()||note.empty()) return; const auto now=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count(); continuity_notes_.push_back({std::to_string(now)+":"+std::to_string(continuity_notes_.size()),continuity_kind_->GetStringSelection().ToStdString(wxConvUTF8),subject,note,continuity_attach_->GetValue()?active_file_:std::string{},continuity_attach_->GetValue()?active_line_:0,now}); continuity_store_.Save(continuity_notes_); continuity_note_->Clear(); RefreshContinuity(); });
    continuity_search_->Bind(wxEVT_TEXT,[this](wxCommandEvent&){RefreshContinuity();}); continuity_filter_kind_->Bind(wxEVT_CHOICE,[this](wxCommandEvent&){RefreshContinuity();});
    open->Bind(wxEVT_BUTTON,[this](wxCommandEvent&){const long i=Selected(continuity_list_);if(i>=0&&jump_handler_&&!visible_notes_[i].file.empty())jump_handler_(visible_notes_[i].file,visible_notes_[i].line);});
    remove->Bind(wxEVT_BUTTON,[this](wxCommandEvent&){const long i=Selected(continuity_list_);if(i<0)return;const auto id=visible_notes_[i].id;continuity_notes_.erase(std::remove_if(continuity_notes_.begin(),continuity_notes_.end(),[&](const auto& n){return n.id==id;}),continuity_notes_.end());continuity_store_.Save(continuity_notes_);RefreshContinuity();});
    continuity_list_->Bind(wxEVT_LIST_ITEM_ACTIVATED,[this](wxListEvent&){const long i=Selected(continuity_list_);if(i>=0&&jump_handler_&&!visible_notes_[i].file.empty())jump_handler_(visible_notes_[i].file,visible_notes_[i].line);});
    return panel;
}

wxPanel* ProductionPanel::BuildAccessibility() {
    auto* panel=new wxPanel(notebook_);auto* layout=new wxBoxSizer(wxVERTICAL);SectionHeader(panel,layout,"Accessibility","Inclusive release check","Find missing descriptions, untranslated controls, and color-only voices.");accessibility_list_=new wxListCtrl(panel,wxID_ANY,wxDefaultPosition,wxDefaultSize,wxLC_REPORT|wxLC_SINGLE_SEL);
    accessibility_list_->AppendColumn("Severity",wxLIST_FORMAT_LEFT,75);accessibility_list_->AppendColumn("Type",wxLIST_FORMAT_LEFT,110);accessibility_list_->AppendColumn("Source",wxLIST_FORMAT_LEFT,150);accessibility_list_->AppendColumn("Finding",wxLIST_FORMAT_LEFT,340);
    auto* actions=new wxBoxSizer(wxHORIZONTAL);auto* refresh=Button(panel,actions,"Refresh audit");style::StylePrimaryButton(refresh);auto* acknowledge=Button(panel,actions,"Acknowledge");auto* open=Button(panel,actions,"Open");
    layout->Add(accessibility_list_,1,wxEXPAND|wxALL,8);layout->Add(actions,0,wxLEFT|wxRIGHT|wxBOTTOM,8);panel->SetSizer(layout);
    refresh->Bind(wxEVT_BUTTON,[this](wxCommandEvent&){RefreshAccessibility();});
    acknowledge->Bind(wxEVT_BUTTON,[this](wxCommandEvent&){const long i=Selected(accessibility_list_);if(i<0)return;const auto& issue=accessibility_issues_[i];if(issue.acknowledged)acknowledged_.erase(issue.id);else acknowledged_.insert(issue.id);accessibility_store_.Save(acknowledged_);RefreshAccessibility();});
    open->Bind(wxEVT_BUTTON,[this](wxCommandEvent&){const long i=Selected(accessibility_list_);if(i>=0&&jump_handler_)jump_handler_(accessibility_issues_[i].file,accessibility_issues_[i].line);});
    accessibility_list_->Bind(wxEVT_LIST_ITEM_ACTIVATED,[this](wxListEvent&){const long i=Selected(accessibility_list_);if(i>=0&&jump_handler_)jump_handler_(accessibility_issues_[i].file,accessibility_issues_[i].line);});return panel;
}

void ProductionPanel::SetProject(std::vector<NamedScript> scripts, ScriptAnalysis analysis,
                                 std::string game_directory, std::string active_file,
                                 std::size_t active_line) {
    scripts_=std::move(scripts);analysis_=std::move(analysis);game_directory_=std::move(game_directory);active_file_=std::move(active_file);active_line_=active_line;
    RefreshProse();RefreshVoice();RefreshContinuity();RefreshAccessibility();
}

void ProductionPanel::RefreshProse() {
    const wxString selected=prose_speaker_->GetStringSelection();prose_speaker_->Clear();prose_speaker_->Append("All speakers");for(const auto& s:analysis_.speakers)prose_speaker_->Append(wxString::FromUTF8(s.name));int choice=prose_speaker_->FindString(selected);prose_speaker_->SetSelection(choice==wxNOT_FOUND?0:choice);
    ScriptAnalysis filtered=analysis_;if(prose_speaker_->GetSelection()>0){const auto speaker=prose_speaker_->GetStringSelection().ToStdString(wxConvUTF8);filtered.counted.erase(std::remove_if(filtered.counted.begin(),filtered.counted.end(),[&](const auto& line){return line.speaker!=speaker;}),filtered.counted.end());}
    const auto prose=analyze_prose(filtered,Words(prose_exclusions_->GetValue()));std::size_t sentences=0,words=0;for(const auto& s:prose.speakers){sentences+=s.sentences;words+=s.words;}const double average=sentences?static_cast<double>(words)/sentences:0;std::ostringstream summary;summary<<prose.unique_words<<" unique of "<<prose.total_words<<" words · ";summary.setf(std::ios::fixed);summary.precision(1);summary<<average<<" words/sentence · "<<sentences<<" sentences";prose_summary_->SetLabel(wxString::FromUTF8(summary.str()));
    prose_terms_=prose.overused_words;prose_terms_.insert(prose_terms_.end(),prose.repeated_phrases.begin(),prose.repeated_phrases.end());prose_list_->DeleteAllItems();for(std::size_t i=0;i<prose_terms_.size();++i){long row=prose_list_->InsertItem(i,wxString::FromUTF8(prose_terms_[i].text));prose_list_->SetItem(row,1,std::to_string(prose_terms_[i].count));}
}

void ProductionPanel::RefreshVoice(){voice_rows_=voice_script_rows(parse_voice_dialogue(scripts_));std::map<std::string,int> counts{{"unrecorded",0},{"recorded",0},{"retake",0},{"approved",0}};voice_list_->DeleteAllItems();std::set<std::string> speakers;for(std::size_t i=0;i<voice_rows_.size();++i){const auto& line=voice_rows_[i];const auto found=voice_entries_.find(line.id);const auto entry=found==voice_entries_.end()?VoiceEntry{}:found->second;++counts[voice_status_name(entry.status)];speakers.insert(line.speaker);long row=voice_list_->InsertItem(i,wxString::FromUTF8(line.speaker));voice_list_->SetItem(row,1,wxString::FromUTF8(line.file+":"+std::to_string(line.line)));voice_list_->SetItem(row,2,wxString::FromUTF8(line.text));voice_list_->SetItem(row,3,wxString::FromUTF8(voice_status_name(entry.status)));}const std::string summary=std::to_string(counts["unrecorded"])+" unrecorded · "+std::to_string(counts["recorded"])+" recorded · "+std::to_string(counts["retake"])+" retake · "+std::to_string(counts["approved"])+" approved";voice_summary_->SetLabel(wxString::FromUTF8(summary));const wxString selected=voice_export_speaker_->GetStringSelection();voice_export_speaker_->Clear();voice_export_speaker_->Append("All roles");for(const auto& s:speakers)voice_export_speaker_->Append(wxString::FromUTF8(s));int choice=voice_export_speaker_->FindString(selected);voice_export_speaker_->SetSelection(choice==wxNOT_FOUND?0:choice);}

void ProductionPanel::SaveVoiceSelection(){const long i=Selected(voice_list_);if(i<0)return;const auto status=parse_voice_status(voice_status_->GetStringSelection().ToStdString(wxConvUTF8));if(!status)return;voice_entries_[voice_rows_[i].id]={*status,voice_file_->GetValue().ToStdString(wxConvUTF8),voice_notes_->GetValue().ToStdString(wxConvUTF8)};voice_store_.Save(voice_entries_);RefreshVoice();}

void ProductionPanel::ExportVoice(bool html){wxFileDialog dialog(this,html?"Export printable VA script":"Export voice tracker",wxEmptyString,html?"va-script.html":"voice-production.csv",html?"HTML files (*.html)|*.html":"CSV files (*.csv)|*.csv",wxFD_SAVE|wxFD_OVERWRITE_PROMPT);if(dialog.ShowModal()!=wxID_OK)return;const std::string speaker=voice_export_speaker_->GetSelection()>0?voice_export_speaker_->GetStringSelection().ToStdString(wxConvUTF8):std::string{};const std::string content=html?voice_script_html(voice_rows_,voice_entries_,speaker,voice_source_context_->GetValue()):voice_tracking_csv(voice_rows_,voice_entries_);std::ofstream output(dialog.GetPath().ToStdString(wxConvUTF8),std::ios::binary|std::ios::trunc);output<<content;if(!output)wxMessageBox("Could not write export.","Export failed",wxOK|wxICON_ERROR,this);}

void ProductionPanel::RefreshTranslations(){translation_=scan_translation_dashboard(game_directory_,translation_language_->GetValue().Strip(wxString::both).ToStdString(wxConvUTF8),scripts_);translation_list_->DeleteAllItems();translation_summary_->SetLabel(translation_.ready?wxString::FromUTF8(std::to_string(translation_.missing.size())+" missing strings · "+std::to_string(group_translation_entries(translation_.missing).size())+" source files"+(translation_.truncated?" · truncated":"")):"No generated translation folder for this language.");for(std::size_t i=0;i<translation_.missing.size();++i){const auto& e=translation_.missing[i];long row=translation_list_->InsertItem(i,wxString::FromUTF8(e.file));translation_list_->SetItem(row,1,std::to_string(e.line));translation_list_->SetItem(row,2,wxString::FromUTF8(e.label+(e.speaker.empty()?"":" · "+e.speaker)));translation_list_->SetItem(row,3,wxString::FromUTF8(e.text));}}

void ProductionPanel::RefreshContinuity(){const std::string kind=continuity_filter_kind_->GetSelection()>0?continuity_filter_kind_->GetStringSelection().ToStdString(wxConvUTF8):std::string{};visible_notes_=filter_continuity_notes(continuity_notes_,continuity_search_->GetValue().ToStdString(wxConvUTF8),kind);continuity_list_->DeleteAllItems();for(std::size_t i=0;i<visible_notes_.size();++i){const auto& n=visible_notes_[i];long row=continuity_list_->InsertItem(i,wxString::FromUTF8(n.kind));continuity_list_->SetItem(row,1,wxString::FromUTF8(n.subject));continuity_list_->SetItem(row,2,wxString::FromUTF8(n.note));continuity_list_->SetItem(row,3,wxString::FromUTF8(n.file.empty()?"Project":n.file+":"+std::to_string(n.line)));}}

void ProductionPanel::RefreshAccessibility(){accessibility_issues_=audit_accessibility(scripts_,acknowledged_);accessibility_list_->DeleteAllItems();for(std::size_t i=0;i<accessibility_issues_.size();++i){const auto& issue=accessibility_issues_[i];const char* kind=issue.kind==AccessibilityKind::translation?"translation":issue.kind==AccessibilityKind::visual_voice?"visual voice":issue.kind==AccessibilityKind::event_description?"event image":"image button";long row=accessibility_list_->InsertItem(i,issue.severity==AccessibilitySeverity::warning?"Warning":"Notice");accessibility_list_->SetItem(row,1,kind);accessibility_list_->SetItem(row,2,wxString::FromUTF8(issue.file+":"+std::to_string(issue.line)));accessibility_list_->SetItem(row,3,wxString::FromUTF8((issue.acknowledged?"[Acknowledged] ":"")+issue.message));if(issue.acknowledged)accessibility_list_->SetItemTextColour(row,wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));}}

}  // namespace say_count::ui
