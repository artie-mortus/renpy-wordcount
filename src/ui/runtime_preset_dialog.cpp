#include "ui/runtime_preset_dialog.h"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace say_count::ui {

RuntimePresetDialog::RuntimePresetDialog(wxWindow* parent, RuntimePresetStore& store,
                                         std::string selected_name, std::string selected_json)
    : wxDialog(parent, wxID_ANY, "Runtime State Presets", wxDefaultPosition, wxSize(650, 520),
               wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER), store_(store),
      presets_(store.Load()), selected_name_(std::move(selected_name)),
      selected_json_(std::move(selected_json)) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    layout->Add(new wxStaticText(this, wxID_ANY,
        "Preset values are assigned to Ren'Py store variables before the game starts."),
        0, wxEXPAND | wxALL, 10);
    choices_ = new wxChoice(this, wxID_ANY);
    layout->Add(choices_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    layout->Add(new wxStaticText(this, wxID_ANY, "Preset name"), 0, wxLEFT | wxRIGHT, 10);
    name_ = new wxTextCtrl(this, wxID_ANY);
    layout->Add(name_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    layout->Add(new wxStaticText(this, wxID_ANY, "State JSON object"), 0, wxLEFT | wxRIGHT, 10);
    json_ = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(selected_json_),
                           wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_RICH2);
    layout->Add(json_, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    auto* preset_actions = new wxBoxSizer(wxHORIZONTAL);
    auto* save = new wxButton(this, wxID_ANY, "Save Preset");
    auto* remove = new wxButton(this, wxID_ANY, "Delete Preset");
    preset_actions->Add(save, 0, wxRIGHT, 8); preset_actions->Add(remove);
    layout->Add(preset_actions, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);
    auto* actions = new wxStdDialogButtonSizer();
    auto* use = new wxButton(this, wxID_OK, "Use This State");
    actions->AddButton(use); actions->AddButton(new wxButton(this, wxID_CANCEL, "Cancel")); actions->Realize();
    layout->Add(actions, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    SetSizer(layout);
    RebuildChoices(selected_name_);
    choices_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { LoadSelection(); });
    save->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SavePreset(); });
    remove->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { DeletePreset(); });
    use->Bind(wxEVT_BUTTON, [this](wxCommandEvent& event) {
        if (!ValidateEditor()) { event.StopPropagation(); return; }
        selected_name_ = name_->GetValue().Strip(wxString::both).ToStdString();
        selected_json_ = json_->GetValue().ToStdString();
        EndModal(wxID_OK);
    });
    CentreOnParent();
}

void RuntimePresetDialog::RebuildChoices(const std::string& selection) {
    choices_->Clear(); choices_->Append("No preset ({})");
    int selected = 0; int index = 1;
    for (const auto& [name, json] : presets_) { (void)json; choices_->Append(wxString::FromUTF8(name)); if (name == selection) selected = index; ++index; }
    choices_->SetSelection(selected); LoadSelection();
}

void RuntimePresetDialog::LoadSelection() {
    if (choices_->GetSelection() <= 0) { name_->Clear(); json_->SetValue("{}"); return; }
    const std::string name = choices_->GetStringSelection().ToStdString();
    name_->SetValue(wxString::FromUTF8(name)); json_->SetValue(wxString::FromUTF8(presets_.at(name)));
}

bool RuntimePresetDialog::ValidateEditor() {
    const auto validation = validate_runtime_state(json_->GetValue().ToStdString());
    if (validation.valid) return true;
    wxMessageBox(wxString::FromUTF8(validation.error), "Invalid runtime state", wxOK | wxICON_ERROR, this);
    return false;
}

void RuntimePresetDialog::SavePreset() {
    const std::string name = name_->GetValue().Strip(wxString::both).ToStdString();
    if (name.empty()) { wxMessageBox("Name the state preset.", "Preset name required", wxOK | wxICON_ERROR, this); return; }
    if (!ValidateEditor()) return;
    presets_[name] = json_->GetValue().ToStdString();
    std::string error;
    if (!store_.Save(presets_, &error)) { wxMessageBox(wxString::FromUTF8(error), "Preset save failed", wxOK | wxICON_ERROR, this); return; }
    RebuildChoices(name);
}

void RuntimePresetDialog::DeletePreset() {
    const std::string name = name_->GetValue().Strip(wxString::both).ToStdString();
    if (name.empty() || presets_.erase(name) == 0) return;
    std::string error;
    if (!store_.Save(presets_, &error)) { wxMessageBox(wxString::FromUTF8(error), "Preset delete failed", wxOK | wxICON_ERROR, this); return; }
    RebuildChoices();
}

}  // namespace say_count::ui
