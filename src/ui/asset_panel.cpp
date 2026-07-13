#include "ui/asset_panel.h"

#include <algorithm>
#include <wx/button.h>
#include <wx/choice.h>
#include <wx/image.h>
#include <wx/listbox.h>
#include <wx/mediactrl.h>
#include <wx/sizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace say_count::ui {

AssetPanel::AssetPanel(wxWindow* parent) : wxPanel(parent) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    search_ = new wxTextCtrl(this, wxID_ANY); search_->SetHint("Filter asset paths");
    kind_ = new wxChoice(this, wxID_ANY); kind_->Append("All assets"); kind_->Append("Images"); kind_->Append("Audio"); kind_->SetSelection(0);
    auto* filters = new wxBoxSizer(wxHORIZONTAL); filters->Add(search_, 1, wxRIGHT, 6); filters->Add(kind_, 0);
    layout->Add(filters, 0, wxEXPAND | wxALL, 6);
    list_ = new wxListBox(this, wxID_ANY); layout->Add(list_, 1, wxEXPAND | wxLEFT | wxRIGHT, 6);
    preview_text_ = new wxStaticText(this, wxID_ANY, "Select an image or audio file.");
    layout->Add(preview_text_, 0, wxEXPAND | wxALL, 6);
    image_ = new wxStaticBitmap(this, wxID_ANY, wxNullBitmap, wxDefaultPosition, wxSize(240, 140));
    layout->Add(image_, 0, wxALIGN_CENTER | wxBOTTOM, 6);
    audio_ = new wxMediaCtrl(this, wxID_ANY, wxString{}, wxDefaultPosition, wxSize(-1, 30));
    layout->Add(audio_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
    auto* actions = new wxBoxSizer(wxHORIZONTAL);
    show_ = new wxButton(this, wxID_ANY, "Insert Show"); scene_ = new wxButton(this, wxID_ANY, "Insert Scene");
    music_ = new wxButton(this, wxID_ANY, "Play Music"); sound_ = new wxButton(this, wxID_ANY, "Play Sound");
    for (auto* button : {show_, scene_, music_, sound_}) actions->Add(button, 0, wxRIGHT, 5);
    layout->Add(actions, 0, wxALL, 6); SetSizer(layout);
    search_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RefreshList(); });
    kind_->Bind(wxEVT_CHOICE, [this](wxCommandEvent&) { RefreshList(); });
    list_->Bind(wxEVT_LISTBOX, [this](wxCommandEvent&) { PreviewSelection(); });
    show_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Insert(AssetInsertAction::Show); });
    scene_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Insert(AssetInsertAction::Scene); });
    music_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Insert(AssetInsertAction::PlayMusic); });
    sound_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Insert(AssetInsertAction::PlaySound); });
    PreviewSelection();
}

void AssetPanel::SetAssets(std::vector<ProjectAsset> assets) { assets_ = std::move(assets); RefreshList(); }
void AssetPanel::SetInsertHandler(std::function<void(const std::string&)> handler) { insert_handler_ = std::move(handler); }

void AssetPanel::RefreshList() {
    const wxString query = search_->GetValue().Lower(); const int filter = kind_->GetSelection();
    visible_.clear(); list_->Clear();
    for (std::size_t i = 0; i < assets_.size(); ++i) {
        if (filter == 1 && assets_[i].kind != AssetKind::Image) continue;
        if (filter == 2 && assets_[i].kind != AssetKind::Audio) continue;
        if (!query.empty() && !wxString::FromUTF8(assets_[i].relative_path).Lower().Contains(query)) continue;
        visible_.push_back(i); list_->Append(wxString::FromUTF8(assets_[i].relative_path));
    }
    if (!visible_.empty()) list_->SetSelection(0);
    PreviewSelection();
}

const ProjectAsset* AssetPanel::Selected() const {
    const int selection = list_->GetSelection();
    return selection == wxNOT_FOUND || static_cast<std::size_t>(selection) >= visible_.size()
        ? nullptr : &assets_[visible_[static_cast<std::size_t>(selection)]];
}

void AssetPanel::PreviewSelection() {
    const auto* asset = Selected(); const bool image = asset && asset->kind == AssetKind::Image;
    const bool audio = asset && asset->kind == AssetKind::Audio;
    show_->Enable(image); scene_->Enable(image); music_->Enable(audio); sound_->Enable(audio);
    image_->SetBitmap(wxNullBitmap); audio_->Stop(); audio_->Hide(); image_->Hide();
    if (!asset) { preview_text_->SetLabel("No matching assets."); Layout(); return; }
    preview_text_->SetLabel(wxString::FromUTF8(asset->relative_path));
    if (image) {
        wxImage loaded(wxString::FromUTF8(asset->absolute_path));
        if (loaded.IsOk()) {
            const double scale = std::min(240.0 / loaded.GetWidth(), 140.0 / loaded.GetHeight());
            if (scale < 1.0) loaded.Rescale(static_cast<int>(loaded.GetWidth() * scale),
                                            static_cast<int>(loaded.GetHeight() * scale), wxIMAGE_QUALITY_HIGH);
            image_->SetBitmap(wxBitmap(loaded)); image_->Show();
        } else preview_text_->SetLabel("Could not preview " + wxString::FromUTF8(asset->relative_path));
    } else {
        if (audio_->Load(wxString::FromUTF8(asset->absolute_path))) { audio_->Show(); audio_->Play(); }
        else preview_text_->SetLabel("Could not preview " + wxString::FromUTF8(asset->relative_path));
    }
    Layout();
}

void AssetPanel::Insert(AssetInsertAction action) {
    const auto* asset = Selected(); if (!asset || !insert_handler_) return;
    insert_handler_(asset_insert_statement(*asset, action));
}

}  // namespace say_count::ui
