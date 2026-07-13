#include "ui/flow_map_panel.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <unordered_map>

#include <wx/button.h>
#include <wx/dcbuffer.h>
#include <wx/graphics.h>
#include <wx/scrolwin.h>
#include <wx/settings.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include "core/flow_layout.h"

namespace say_count::ui {
namespace {

wxColour Blend(const wxColour& first, const wxColour& second, double amount) {
    const auto channel = [&](unsigned char left, unsigned char right) {
        return static_cast<unsigned char>(std::lround(left + (right - left) * amount));
    };
    return {channel(first.Red(), second.Red()), channel(first.Green(), second.Green()),
            channel(first.Blue(), second.Blue())};
}

}  // namespace

class FlowMapCanvas final : public wxScrolledWindow {
public:
    explicit FlowMapCanvas(wxWindow* parent)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxSize(-1, 280),
                           wxHSCROLL | wxVSCROLL | wxBORDER_SIMPLE) {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetScrollRate(10, 10);
        Bind(wxEVT_PAINT, &FlowMapCanvas::OnPaint, this);
    }

    void SetReport(const std::optional<RouteReport>& report) {
        layout_ = report ? build_flow_layout(*report) : FlowLayout{};
        UpdateVirtualSize();
        Refresh();
    }

    void SetZoom(double zoom) {
        zoom_ = std::clamp(zoom, 0.5, 2.0);
        UpdateVirtualSize();
        Refresh();
    }

    double zoom() const noexcept { return zoom_; }

private:
    void UpdateVirtualSize() {
        const int width = std::max(1, static_cast<int>(std::ceil(layout_.width * zoom_)));
        const int height = std::max(1, static_cast<int>(std::ceil(layout_.height * zoom_)));
        SetVirtualSize(width, height);
    }

    void OnPaint(wxPaintEvent&) {
        wxAutoBufferedPaintDC dc(this);
        PrepareDC(dc);
        const wxColour background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        dc.SetBackground(wxBrush(background));
        dc.Clear();

        if (layout_.too_many_nodes) {
            dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT));
            dc.DrawText(wxString::FromUTF8(std::to_string(layout_.requested_nodes) +
                                           " labels — too many to draw."), 10, 10);
            return;
        }
        if (layout_.nodes.size() < 2) {
            dc.SetTextForeground(wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT));
            dc.DrawText("Add another label to draw the flow map.", 10, 10);
            return;
        }

        std::unique_ptr<wxGraphicsContext> graphics(wxGraphicsContext::Create(dc));
        if (!graphics) return;
        graphics->Scale(zoom_, zoom_);

        std::unordered_map<std::string, const FlowNodeLayout*> nodes;
        for (const auto& node : layout_.nodes) nodes[node.name] = &node;
        const wxColour faint = Blend(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT),
                                     background, 0.62);
        for (const auto& edge : layout_.edges) {
            const auto* from = nodes.at(edge.from);
            const auto* to = nodes.at(edge.to);
            wxPen pen(edge.back_edge ? wxColour("#b3261e") : faint, 1.5,
                      edge.back_edge ? wxPENSTYLE_SHORT_DASH : wxPENSTYLE_SOLID);
            graphics->SetPen(pen);
            wxGraphicsPath path = graphics->CreatePath();
            const double from_x = from->bounds.x + from->bounds.width / 2.0;
            const double from_y = from->bounds.y + from->bounds.height / 2.0;
            const double to_x = to->bounds.x + to->bounds.width / 2.0;
            const double to_y = to->bounds.y + to->bounds.height / 2.0;
            if (edge.back_edge) {
                const double side = std::max(from_x, to_x) + kFlowNodeWidth * 0.75;
                path.MoveToPoint(from_x + kFlowNodeWidth / 2.0, from_y);
                path.AddCurveToPoint(side, from_y, side, to_y,
                                     to_x + kFlowNodeWidth / 2.0, to_y);
            } else {
                const double start_y = from_y + kFlowNodeHeight / 2.0;
                const double end_y = to_y - kFlowNodeHeight / 2.0;
                const double middle_y = (start_y + end_y) / 2.0;
                path.MoveToPoint(from_x, start_y);
                path.AddCurveToPoint(from_x, middle_y, to_x, middle_y, to_x, end_y);
            }
            graphics->StrokePath(path);
        }

        const wxColour foreground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        const wxColour muted = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
        const wxColour fill = Blend(background, foreground, 0.08);
        wxFont name_font(wxFontInfo(8).Family(wxFONTFAMILY_TELETYPE));
        wxFont count_font(wxFontInfo(7).Family(wxFONTFAMILY_TELETYPE));
        for (const auto& node : layout_.nodes) {
            wxColour border = faint;
            double width = 1.5;
            if (node.start) { border = wxColour("#c48a00"); width = 2.0; }
            else if (node.ending) border = wxColour("#b3261e");
            const wxPenStyle style = node.unreachable ? wxPENSTYLE_SHORT_DASH : wxPENSTYLE_SOLID;
            graphics->SetPen(wxPen(border, width, style));
            graphics->SetBrush(wxBrush(fill));
            graphics->DrawRoundedRectangle(node.bounds.x, node.bounds.y, node.bounds.width,
                                           node.bounds.height, 7.0);

            wxString label = wxString::FromUTF8(node.name);
            if (label.length() > 14) label = label.Left(13) + wxString::FromUTF8("…");
            graphics->SetFont(name_font, foreground);
            double text_width = 0, text_height = 0, descent = 0, leading = 0;
            graphics->GetTextExtent(label, &text_width, &text_height, &descent, &leading);
            graphics->DrawText(label, node.bounds.x + (node.bounds.width - text_width) / 2.0,
                               node.bounds.y + 4.0);
            const wxString count = wxString::FromUTF8(std::to_string(node.words) + " w");
            graphics->SetFont(count_font, muted);
            graphics->GetTextExtent(count, &text_width, &text_height, &descent, &leading);
            graphics->DrawText(count, node.bounds.x + (node.bounds.width - text_width) / 2.0,
                               node.bounds.y + 17.0);
        }
    }

    FlowLayout layout_;
    double zoom_ = 1.0;
};

FlowMapPanel::FlowMapPanel(wxWindow* parent) : wxPanel(parent) {
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* controls = new wxBoxSizer(wxHORIZONTAL);
    auto* zoom_out = new wxButton(this, wxID_ANY, "−", wxDefaultPosition, wxDefaultSize,
                                  wxBU_EXACTFIT);
    auto* reset = new wxButton(this, wxID_ANY, "100%", wxDefaultPosition, wxDefaultSize,
                               wxBU_EXACTFIT);
    auto* zoom_in = new wxButton(this, wxID_ANY, "+", wxDefaultPosition, wxDefaultSize,
                                 wxBU_EXACTFIT);
    zoom_label_ = new wxStaticText(this, wxID_ANY, "100%");
    controls->Add(new wxStaticText(this, wxID_ANY, "Flow map"), 0,
                  wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
    controls->AddStretchSpacer();
    controls->Add(zoom_out, 0, wxRIGHT, 3);
    controls->Add(reset, 0, wxRIGHT, 3);
    controls->Add(zoom_in, 0, wxRIGHT, 6);
    controls->Add(zoom_label_, 0, wxALIGN_CENTER_VERTICAL);
    canvas_ = new FlowMapCanvas(this);
    layout->Add(controls, 0, wxEXPAND | wxBOTTOM, 4);
    layout->Add(canvas_, 1, wxEXPAND);
    SetSizer(layout);

    zoom_out->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SetZoom(canvas_->zoom() - 0.1); });
    reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SetZoom(1.0); });
    zoom_in->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { SetZoom(canvas_->zoom() + 0.1); });
}

void FlowMapPanel::SetReport(const std::optional<RouteReport>& report) {
    canvas_->SetReport(report);
}

void FlowMapPanel::SetZoom(double zoom) {
    canvas_->SetZoom(zoom);
    zoom_label_->SetLabel(wxString::Format("%d%%", static_cast<int>(std::lround(canvas_->zoom() * 100))));
}

}  // namespace say_count::ui
