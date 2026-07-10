#include <wx/app.h>

#include "core/parser.h"
#include "ui/main_frame.h"

#include <fstream>
#include <iostream>
#include <iterator>

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

wxIMPLEMENT_APP_NO_MAIN(SayCountApp);

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "--dump-json") {
        std::ifstream input(argv[2], std::ios::binary);
        if (!input) {
            std::cerr << "Unable to read " << argv[2] << '\n';
            return 2;
        }
        const std::string script((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        std::cout << say_count::analysis_json(say_count::analyze_script(script)) << '\n';
        return 0;
    }
    return wxEntry(argc, argv);
}
