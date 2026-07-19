#include "ui/manuscript_dialog.h"

#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/dataview.h>
#include <wx/dialog.h>
#include <wx/filepicker.h>
#include <wx/filename.h>
#include <wx/msgdlg.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/splitter.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "ui/guide_dialog.h"
#include "ui/style.h"

namespace say_count::ui {
namespace {

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
    GuideDialog(parent, GuideKind::Prose).ShowModal();
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
