#include "ui/speaker_stats_panel.h"

#include <algorithm>
#include <cmath>
#include <string>

#include <wx/dcbuffer.h>
#include <wx/settings.h>

namespace say_count::ui {
namespace {

constexpr int kPadding = 12;
constexpr int kRowHeight = 76;

}  // namespace

SpeakerStatsPanel::SpeakerStatsPanel(wxWindow* parent)
    : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                       wxVSCROLL | wxBORDER_NONE) {
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    SetScrollRate(0, 12);
    Bind(wxEVT_PAINT, &SpeakerStatsPanel::OnPaint, this);
}

void SpeakerStatsPanel::SetAnalysis(const ScriptAnalysis& analysis) {
    analysis_ = analysis;
    std::stable_sort(analysis_.speakers.begin(), analysis_.speakers.end(),
                     [](const AggregateStats& left, const AggregateStats& right) {
                         return left.words > right.words;
                     });
    SetVirtualSize(-1, std::max(1, kPadding * 2 +
        static_cast<int>(analysis_.speakers.size()) * kRowHeight));
    Refresh();
}

void SpeakerStatsPanel::OnPaint(wxPaintEvent&) {
    wxAutoBufferedPaintDC dc(this);
    PrepareDC(dc);
    const wxColour background = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
    const wxColour foreground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
    const wxColour muted = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
    dc.SetBackground(wxBrush(background));
    dc.Clear();
    dc.SetTextForeground(foreground);

    if (analysis_.speakers.empty()) {
        dc.DrawText("No dialogue yet.", kPadding, kPadding);
        return;
    }

    const int width = std::max(1, GetClientSize().GetWidth() - kPadding * 2);
    const int bar_width = std::max(1, width);
    for (std::size_t index = 0; index < analysis_.speakers.size(); ++index) {
        const auto& speaker = analysis_.speakers[index];
        const int y = kPadding + static_cast<int>(index) * kRowHeight;
        wxColour color("#77d6c3");
        const auto found = analysis_.speaker_colors.find(speaker.name);
        if (found != analysis_.speaker_colors.end()) {
            const wxColour parsed(wxString::FromUTF8(found->second));
            if (parsed.IsOk()) color = parsed;
        }
        const int percentage = analysis_.total_words == 0 ? 0 : static_cast<int>(
            std::lround(static_cast<double>(speaker.words) * 100.0 / analysis_.total_words));

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(color));
        dc.DrawCircle(kPadding + 5, y + 7, 5);
        dc.SetTextForeground(foreground);
        dc.DrawText(wxString::FromUTF8(speaker.name), kPadding + 16, y);

        // Narrow literals with non-ASCII bytes convert through the locale; keep them UTF-8.
        const std::string counts_utf8 = std::to_string(speaker.words) + " words \xc2\xb7 " +
                                        std::to_string(speaker.lines) + " lines \xc2\xb7 " +
                                        std::to_string(percentage) + "%";
        const wxString counts = wxString::FromUTF8(counts_utf8);
        dc.SetTextForeground(muted);
        dc.DrawText(counts, kPadding, y + 23);

        const int bar_y = y + 48;
        dc.SetBrush(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE)));
        dc.DrawRoundedRectangle(kPadding, bar_y, bar_width, 10, 5);
        dc.SetBrush(wxBrush(color));
        dc.DrawRoundedRectangle(kPadding, bar_y,
                                static_cast<int>(bar_width * percentage / 100.0), 10, 5);
    }
}

}  // namespace say_count::ui
