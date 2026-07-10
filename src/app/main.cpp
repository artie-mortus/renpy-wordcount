#include <wx/app.h>

#include "ui/main_frame.h"

class SayCountApp final : public wxApp {
public:
    bool OnInit() override {
        if (!wxApp::OnInit()) {
            return false;
        }

        SetAppName("say-count");
        SetAppDisplayName("Say Count");

        auto* frame = new say_count::ui::MainFrame();
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP(SayCountApp);
