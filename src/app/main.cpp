#include <wx/app.h>
#include <wx/cmdline.h>
#include <wx/filename.h>
#include <wx/image.h>

#include "core/parser.h"
#include "ui/main_frame.h"

#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

class SayCountApp final : public wxApp {
public:
    void OnInitCmdLine(wxCmdLineParser& parser) override {
        wxApp::OnInitCmdLine(parser);
        parser.AddParam("script", wxCMD_LINE_VAL_STRING,
                        wxCMD_LINE_PARAM_OPTIONAL | wxCMD_LINE_PARAM_MULTIPLE);
    }

    bool OnInit() override {
        if (!wxApp::OnInit()) {
            return false;
        }

        SetAppName("say-count");
        SetAppDisplayName("Say Count");
        wxInitAllImageHandlers();

        auto* frame = new say_count::ui::MainFrame();
        std::vector<wxString> initial_files;
        for (int index = 1; index < argc; ++index) {
            const wxFileName path(argv[index]);
            if (path.FileExists() && path.GetExt().CmpNoCase("rpy") == 0) {
                initial_files.push_back(path.GetFullPath());
            }
        }
        frame->OpenInitialFiles(initial_files);
        frame->Show();
        return true;
    }
};

wxIMPLEMENT_APP_NO_MAIN(SayCountApp);

int main(int argc, char** argv) {
    if (argc >= 3 && std::string_view(argv[1]) == "--dump-json") {
        const bool count_menu_choices = std::string_view(argv[2]) == "--count-menu-choices";
        const int first_file = count_menu_choices ? 3 : 2;
        if (first_file >= argc) return 2;
        std::vector<say_count::NamedScript> scripts;
        for (int index = first_file; index < argc; ++index) {
            std::ifstream input(argv[index], std::ios::binary);
            if (!input) {
                std::cerr << "Unable to read " << argv[index] << '\n';
                return 2;
            }
            scripts.push_back({argv[index], std::string((std::istreambuf_iterator<char>(input)),
                                                        std::istreambuf_iterator<char>())});
        }
        const say_count::AnalysisOptions options{count_menu_choices, {}, 35};
        const auto analysis = scripts.size() == 1 ? say_count::analyze_script(scripts.front().content, options)
                                                  : say_count::analyze_project(scripts, options);
        std::cout << say_count::analysis_json(analysis) << '\n';
        return 0;
    }
    return wxEntry(argc, argv);
}
