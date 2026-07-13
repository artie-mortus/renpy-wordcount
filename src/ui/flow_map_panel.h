#pragma once

#include <optional>
#include <functional>
#include <string>

#include <wx/panel.h>

#include "core/routes.h"

class wxStaticText;

namespace say_count::ui {

class FlowMapCanvas;

class FlowMapPanel final : public wxPanel {
public:
    explicit FlowMapPanel(wxWindow* parent);

    void SetReport(const std::optional<RouteReport>& report);
    void SetJumpHandler(std::function<void(const std::string&, std::size_t)> handler);

private:
    void SetZoom(double zoom);
    void FitToView();

    FlowMapCanvas* canvas_ = nullptr;
    wxStaticText* zoom_label_ = nullptr;
};

}  // namespace say_count::ui
