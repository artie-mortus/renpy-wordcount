#include <wx/app.h>

class SayCountApp final : public wxApp {
public:
    bool OnInit() override {
        // Step 0.2 only verifies wxWidgets application initialization. The
        // first real frame and lifecycle belong to Step 1.1.
        return false;
    }
};

wxIMPLEMENT_APP(SayCountApp);

