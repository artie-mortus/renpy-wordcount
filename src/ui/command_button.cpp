#include "ui/command_button.h"

#include <wx/dcbuffer.h>
#include <wx/graphics.h>

#include "ui/style.h"

namespace say_count::ui {
namespace {

struct ButtonColors {
    wxColour face;
    wxColour edge;
    wxColour border;
    wxColour text;
};

ButtonColors ColorsFor(bool primary, bool hovered, bool pressed, bool enabled) {
    if (!enabled) {
        return {wxColour("#28364A"), wxColour("#202C3D"), wxColour("#3A485B"),
                wxColour("#8290A2")};
    }
    if (primary) {
        if (pressed) {
            return {wxColour("#BC8E2E"), wxColour("#9F7420"), wxColour("#E1B958"),
                    wxColour("#17243A")};
        }
        if (hovered) {
            return {wxColour("#E2B94F"), wxColour("#B88728"), wxColour("#F0D17D"),
                    wxColour("#17243A")};
        }
        return {wxColour("#CFA23E"), wxColour("#A97C24"), wxColour("#E4C166"),
                wxColour("#17243A")};
    }
    if (pressed) {
        return {wxColour("#18283D"), wxColour("#101B2B"), wxColour("#64748A"),
                style::Colors().white};
    }
    if (hovered) {
        return {wxColour("#30445F"), wxColour("#17283E"), wxColour("#718198"),
                style::Colors().white};
    }
    return {wxColour("#243650"), wxColour("#17283E"), wxColour("#52647C"),
            wxColour("#F5F7FA")};
}

}  // namespace

CommandButton::CommandButton(wxWindow* parent, const wxString& label, bool primary)
    : wxControl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                wxBORDER_NONE | wxWANTS_CHARS),
      primary_(primary) {
    SetLabel(label);
    SetFont(style::BodyFont(9, wxFONTWEIGHT_BOLD));
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetMinSize(DoGetBestClientSize());
    SetName(label);
    SetToolTip(label);

    Bind(wxEVT_PAINT, &CommandButton::OnPaint, this);
    Bind(wxEVT_LEFT_DOWN, &CommandButton::OnLeftDown, this);
    Bind(wxEVT_LEFT_UP, &CommandButton::OnLeftUp, this);
    Bind(wxEVT_ENTER_WINDOW, [this](wxMouseEvent& event) {
        hovered_ = true;
        Refresh(false);
        event.Skip();
    });
    Bind(wxEVT_LEAVE_WINDOW, [this](wxMouseEvent& event) {
        hovered_ = false;
        if (!HasCapture()) pressed_ = false;
        Refresh(false);
        event.Skip();
    });
    Bind(wxEVT_KEY_DOWN, &CommandButton::OnKeyDown, this);
    Bind(wxEVT_KEY_UP, &CommandButton::OnKeyUp, this);
    Bind(wxEVT_SET_FOCUS, [this](wxFocusEvent& event) {
        Refresh(false);
        event.Skip();
    });
    Bind(wxEVT_KILL_FOCUS, [this](wxFocusEvent& event) {
        pressed_ = false;
        Refresh(false);
        event.Skip();
    });
    Bind(wxEVT_MOUSE_CAPTURE_LOST, [this](wxMouseCaptureLostEvent&) {
        pressed_ = false;
        Refresh(false);
    });
}

void CommandButton::SetCommandLabel(const wxString& label) {
    SetLabel(label);
    SetName(label);
    SetToolTip(label);
    InvalidateBestSize();
    SetMinSize(DoGetBestClientSize());
    Refresh(false);
}

wxSize CommandButton::DoGetBestClientSize() const {
    const wxSize text = GetTextExtent(GetLabel());
    const int horizontal_padding = FromDIP(24);
    return wxSize(text.x + horizontal_padding, FromDIP(36));
}

void CommandButton::Activate() {
    if (!IsEnabled()) return;
    wxCommandEvent event(wxEVT_BUTTON, GetId());
    event.SetEventObject(this);
    event.SetString(GetLabel());
    ProcessWindowEvent(event);
}

void CommandButton::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    const wxColour background = GetParent() ? GetParent()->GetBackgroundColour()
                                            : style::Colors().ink;
    dc.SetBackground(wxBrush(background));
    dc.Clear();

    const auto colors = ColorsFor(primary_, hovered_, pressed_, IsEnabled());
    const wxSize size = GetClientSize();
    if (size.x <= 0 || size.y <= 0) return;

    auto graphics = wxGraphicsContext::Create(dc);
    if (!graphics) return;

    const double inset = FromDIP(1);
    const double edge_depth = pressed_ ? 0.0 : FromDIP(2);
    const double press_offset = pressed_ ? FromDIP(1) : 0.0;
    const double radius = FromDIP(7);
    const double width = size.x - inset * 2.0;
    const double height = size.y - inset * 2.0 - edge_depth;

    if (!pressed_) {
        graphics->SetPen(*wxTRANSPARENT_PEN);
        graphics->SetBrush(wxBrush(colors.edge));
        graphics->DrawRoundedRectangle(inset, inset + edge_depth, width, height, radius);
    }

    graphics->SetPen(wxPen(colors.border, FromDIP(1)));
    graphics->SetBrush(wxBrush(colors.face));
    graphics->DrawRoundedRectangle(inset, inset + press_offset, width, height, radius);

    if (HasFocus()) {
        graphics->SetPen(wxPen(style::Colors().mint, FromDIP(2)));
        graphics->SetBrush(*wxTRANSPARENT_BRUSH);
        graphics->DrawRoundedRectangle(inset + FromDIP(2), inset + FromDIP(2) + press_offset,
                                       width - FromDIP(4), height - FromDIP(4),
                                       radius - FromDIP(2));
    }

    graphics->SetFont(GetFont(), colors.text);
    double text_width = 0.0;
    double text_height = 0.0;
    graphics->GetTextExtent(GetLabel(), &text_width, &text_height);
    const double text_x = (size.x - text_width) / 2.0;
    const double text_y = (size.y - edge_depth - text_height) / 2.0 + press_offset;
    graphics->DrawText(GetLabel(), text_x, text_y);
    delete graphics;
}

void CommandButton::OnLeftDown(wxMouseEvent& event) {
    if (!IsEnabled()) return;
    SetFocus();
    pressed_ = true;
    if (!HasCapture()) CaptureMouse();
    Refresh(false);
    event.Skip();
}

void CommandButton::OnLeftUp(wxMouseEvent& event) {
    if (!pressed_) return;
    const bool activate = GetClientRect().Contains(event.GetPosition()) && IsEnabled();
    pressed_ = false;
    if (HasCapture()) ReleaseMouse();
    Refresh(false);
    if (activate) Activate();
}

void CommandButton::OnKeyDown(wxKeyEvent& event) {
    if (!IsEnabled()) return;
    if (event.GetKeyCode() == WXK_SPACE) {
        if (!pressed_) {
            pressed_ = true;
            Refresh(false);
        }
        return;
    }
    if (event.GetKeyCode() == WXK_RETURN || event.GetKeyCode() == WXK_NUMPAD_ENTER) {
        Activate();
        return;
    }
    event.Skip();
}

void CommandButton::OnKeyUp(wxKeyEvent& event) {
    if (event.GetKeyCode() == WXK_SPACE && pressed_) {
        pressed_ = false;
        Refresh(false);
        Activate();
        return;
    }
    event.Skip();
}

}  // namespace say_count::ui
