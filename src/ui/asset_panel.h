#pragma once

#include <wx/panel.h>

#include "core/assets.h"

#include <functional>

class wxButton;
class wxChoice;
class wxListBox;
class wxMediaCtrl;
class wxStaticBitmap;
class wxStaticText;
class wxTextCtrl;

namespace say_count::ui {

class AssetPanel final : public wxPanel {
public:
    explicit AssetPanel(wxWindow* parent);
    void SetAssets(std::vector<ProjectAsset> assets);
    void SetInsertHandler(std::function<void(const std::string&)> handler);
private:
    void RefreshList();
    void PreviewSelection();
    const ProjectAsset* Selected() const;
    void Insert(AssetInsertAction action);

    std::vector<ProjectAsset> assets_;
    std::vector<std::size_t> visible_;
    wxTextCtrl* search_ = nullptr;
    wxChoice* kind_ = nullptr;
    wxListBox* list_ = nullptr;
    wxStaticBitmap* image_ = nullptr;
    wxMediaCtrl* audio_ = nullptr;
    wxStaticText* preview_text_ = nullptr;
    wxButton* show_ = nullptr;
    wxButton* scene_ = nullptr;
    wxButton* music_ = nullptr;
    wxButton* sound_ = nullptr;
    std::function<void(const std::string&)> insert_handler_;
};

}  // namespace say_count::ui
