#pragma once

#include <functional>
#include <optional>
#include <string>

#include <wx/panel.h>

#include "core/routes.h"

class wxStaticText;
class wxTreeCtrl;
class wxTreeEvent;

namespace say_count::ui {

class FlowMapPanel;

class RoutePanel final : public wxPanel {
public:
    using JumpHandler = std::function<void(const std::string&, std::size_t)>;

    explicit RoutePanel(wxWindow* parent);

    void SetReport(std::optional<RouteReport> report);
    void SetJumpHandler(JumpHandler handler);

private:
    void Rebuild();
    void OnActivated(wxTreeEvent& event);

    std::optional<RouteReport> report_;
    JumpHandler jump_handler_;
    wxStaticText* summary_ = nullptr;
    FlowMapPanel* flow_map_ = nullptr;
    wxTreeCtrl* details_ = nullptr;
};

}  // namespace say_count::ui
