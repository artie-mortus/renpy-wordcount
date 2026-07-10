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
    if (argc >= 3 && std::string_view(argv[1]) == "--dump-json") {
        std::vector<say_count::NamedScript> scripts;
        for (int index = 2; index < argc; ++index) {
            std::ifstream input(argv[index], std::ios::binary);
            if (!input) {
                std::cerr << "Unable to read " << argv[index] << '\n';
                return 2;
            }
            scripts.push_back({argv[index], std::string((std::istreambuf_iterator<char>(input)),
                                                        std::istreambuf_iterator<char>())});
        }
        const auto analysis = scripts.size() == 1 ? say_count::analyze_script(scripts.front().content)
                                                  : say_count::analyze_project(scripts);
        std::cout << say_count::analysis_json(analysis) << '\n';
        return 0;
    }
    return wxEntry(argc, argv);
}
