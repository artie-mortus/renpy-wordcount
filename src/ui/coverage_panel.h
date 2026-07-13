#pragma once

#include <wx/panel.h>

#include <functional>
#include <set>
#include <string>
#include <vector>

class wxButton;
class wxDataViewListCtrl;
class wxStaticText;

namespace say_count::ui {

class CoveragePanel final : public wxPanel {
public:
    explicit CoveragePanel(wxWindow* parent);
    void SetCoverage(std::vector<std::string> labels, std::set<std::string> manual,
                     std::set<std::string> playthrough);
    void SetManualHandler(std::function<void(const std::string&, bool)> handler);
    void SetClearHandler(std::function<void()> handler);
private:
    void Rebuild();
    std::string SelectedLabel() const;
    std::vector<std::string> labels_;
    std::set<std::string> manual_;
    std::set<std::string> playthrough_;
    wxStaticText* summary_ = nullptr;
    wxDataViewListCtrl* list_ = nullptr;
    wxButton* toggle_ = nullptr;
    std::function<void(const std::string&, bool)> manual_handler_;
    std::function<void()> clear_handler_;
};

}  // namespace say_count::ui
