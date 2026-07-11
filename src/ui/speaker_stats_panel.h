#pragma once

#include <wx/scrolwin.h>

#include "core/parser.h"

namespace say_count::ui {

class SpeakerStatsPanel final : public wxScrolledWindow {
public:
    explicit SpeakerStatsPanel(wxWindow* parent);
    void SetAnalysis(const ScriptAnalysis& analysis);

private:
    void OnPaint(wxPaintEvent& event);

    ScriptAnalysis analysis_;
};

}  // namespace say_count::ui
