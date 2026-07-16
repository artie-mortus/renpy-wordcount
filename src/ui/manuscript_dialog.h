#pragma once

#include <vector>
#include <string>
#include <string_view>

#include <wx/dialog.h>

#include "core/manuscript.h"
#include "app/settings.h"

class wxButton;
class wxCheckBox;
class wxDataViewListCtrl;
class wxStaticText;
class wxTextCtrl;
class wxWindow;

namespace say_count::ui {

void ShowManuscriptGuide(wxWindow* parent);
bool ConfigureOfflineProseAi(wxWindow* parent, app::EditorSettings* settings);

class ManuscriptReviewDialog final : public wxDialog {
public:
    ManuscriptReviewDialog(wxWindow* parent,
                           const std::vector<ManuscriptLineReview>& lines,
                           const ManuscriptConversion& safe_conversion,
                           const ManuscriptConversion& inclusive_conversion,
                           std::string_view source,
                           bool offline_ai_used = false);

    bool include_uncertain_lines() const;
    const ManuscriptConversion& selected_conversion() const;
    std::size_t converted_line_count() const;
    std::size_t preserved_line_count() const noexcept { return renpy_lines_; }
    std::size_t uncertain_line_count() const noexcept { return uncertain_lines_; }

private:
    void RefreshPreview();

    const ManuscriptConversion& safe_conversion_;
    const ManuscriptConversion& inclusive_conversion_;
    wxCheckBox* include_uncertain_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxStaticText* summary_ = nullptr;
    wxButton* convert_ = nullptr;
    std::string source_;
    std::size_t prose_lines_ = 0;
    std::size_t renpy_lines_ = 0;
    std::size_t uncertain_lines_ = 0;
    bool offline_ai_used_ = false;
};

}  // namespace say_count::ui
