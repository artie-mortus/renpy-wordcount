#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ui/chat_conversion_dialog.h"
#include "ui/editor_notebook.h"
#include "ui/guide_dialog.h"
#include "ui/welcome_dialog.h"
#include "ui/story_element_dialog.h"
#include "ui/story_library_panel.h"
#include "ui/project_navigator_panel.h"
#include "ui/command_model.h"
#include "ui/command_button.h"
#include "ui/writer_draft_dialog.h"
#include "app/settings.h"

#include <chrono>
#include <thread>
#include <filesystem>
#include <fstream>

#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/clipbrd.h>
#include <wx/frame.h>
#include <wx/html/htmlwin.h>
#include <wx/init.h>
#include <wx/listbox.h>
#include <wx/panel.h>
#include <wx/radiobox.h>
#include <wx/stattext.h>
#include <wx/stc/stc.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

namespace {

class ChatUiTestApp final : public wxApp {
public:
    bool OnInit() override { return true; }
};

wxIMPLEMENT_APP_NO_MAIN(ChatUiTestApp);

bool WaitForText(wxTextCtrl* control) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (control->GetValue().empty() && std::chrono::steady_clock::now() < deadline) {
        wxYieldIfNeeded();
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    return !control->GetValue().empty();
}

template<typename T>
T* Named(wxWindow& parent, const char* name) {
    return dynamic_cast<T*>(wxWindow::FindWindowByName(name, &parent));
}

wxString ShowAndReadGuide(say_count::ui::GuideDialog& dialog) {
    wxString text;
    wxTheApp->CallAfter([&dialog, &text] {
        if (dialog.content_page()) text = dialog.content_page()->ToText();
        dialog.EndModal(wxID_OK);
    });
    dialog.ShowModal();
    return text;
}

}  // namespace

TEST_CASE("chat conversion dialog exposes keyboard-accessible forward controls") {
    say_count::ui::ChatConversionDialog dialog(nullptr, "Robo: Hello", true,
                                                {{"r", "Robo"}});
    auto* channel = Named<wxTextCtrl>(dialog, "Default chat channel");
    auto* app_style = Named<wxRadioBox>(dialog, "Chat app style");
    auto* wrap = Named<wxCheckBox>(dialog, "Wrap in chat bridge calls");
    auto* narration = Named<wxCheckBox>(dialog, "Send narration as chat messages");
    auto* preview = Named<wxTextCtrl>(dialog, "Chat conversion preview");
    auto* visual = Named<wxHtmlWindow>(dialog, "Chat visual preview");
    auto* copy = Named<wxButton>(dialog, "Copy chat conversion result");
    auto* replace = Named<wxButton>(dialog, "Replace source with chat conversion");
    auto* cancel = Named<wxButton>(dialog, "Cancel chat conversion");
    REQUIRE(channel);
    REQUIRE(app_style);
    REQUIRE(wrap);
    REQUIRE(visual);
    CHECK(app_style->GetCount() == 2);
    CHECK(wrap->GetValue());
    REQUIRE(narration);
    REQUIRE(preview);
    REQUIRE(copy);
    REQUIRE(replace);
    REQUIRE(cancel);
    CHECK(channel->AcceptsFocusFromKeyboard());
    CHECK(narration->AcceptsFocusFromKeyboard());
    CHECK(copy->AcceptsFocusFromKeyboard());
    CHECK(replace->AcceptsFocusFromKeyboard());
    CHECK(cancel->AcceptsFocusFromKeyboard());
    REQUIRE(WaitForText(preview));
    CHECK(preview->GetValue().Contains("r \"Hello\""));
    CHECK(dialog.conversion().messages == 1);
}

TEST_CASE("embedded writing guides render their UTF-8 resource content") {
    SECTION("prose") {
        say_count::ui::GuideDialog dialog(nullptr, say_count::ui::GuideKind::Prose);
        REQUIRE(dialog.content_loaded());
        REQUIRE(dialog.content_page());
        const wxString text = ShowAndReadGuide(dialog);
        CHECK(text.Contains("Write in the script editor"));
        CHECK(text.Contains(wxString::FromUTF8("MANUSCRIPT → REN'PY")));
    }

    SECTION("chat") {
        say_count::ui::GuideDialog dialog(nullptr, say_count::ui::GuideKind::Chat);
        REQUIRE(dialog.content_loaded());
        REQUIRE(dialog.content_page());
        const wxString text = ShowAndReadGuide(dialog);
        CHECK(text.Contains("Write messenger scenes as prose"));
        CHECK(text.Contains(wxString::FromUTF8("CHAT FORMAT · SOCIAL-MEDIA SCENES")));
    }
}

TEST_CASE("writing guide shows a visible fallback when content cannot render") {
    say_count::ui::GuideDialog dialog(nullptr, "Unavailable Guide", "Unavailable guide content",
                                      "<html><body></body></html>");
    CHECK_FALSE(dialog.content_loaded());
    CHECK(dialog.content_page() == nullptr);
    bool fallback_shown = false;
    bool found_message = false;
    wxTheApp->CallAfter([&] {
        auto* fallback = Named<wxPanel>(dialog, "Guide display error");
        fallback_shown = fallback && fallback->IsShownOnScreen();
        if (fallback) {
            for (wxWindow* child : fallback->GetChildren()) {
                auto* label = dynamic_cast<wxStaticText*>(child);
                if (label && label->GetLabel().Contains("could not be displayed"))
                    found_message = true;
            }
        }
        dialog.EndModal(wxID_OK);
    });
    dialog.ShowModal();
    CHECK(fallback_shown);
    CHECK(found_message);
}

TEST_CASE("welcome dialog exposes keyboard-accessible writer-first actions") {
    say_count::ui::WelcomeDialog dialog(nullptr, {});
    auto* start = Named<wxButton>(dialog, "Start a new story");
    auto* game = Named<wxButton>(dialog, "Open a game");
    auto* script = Named<wxButton>(dialog, "Open a script");
    REQUIRE(start);
    REQUIRE(game);
    REQUIRE(script);
    CHECK(start->AcceptsFocusFromKeyboard());
    CHECK(game->AcceptsFocusFromKeyboard());
    CHECK(script->AcceptsFocusFromKeyboard());
    CHECK(start->IsEnabled());
    CHECK(game->IsEnabled());
    CHECK(script->IsEnabled());
    auto* recent = Named<wxListBox>(dialog, "Recent games");
    auto* empty = Named<wxStaticText>(dialog, "Recent games empty state");
    REQUIRE(recent);
    REQUIRE(empty);
    CHECK_FALSE(recent->IsShown());
    CHECK(empty->IsShown());
}

TEST_CASE("command bar keeps writing stable while problems change") {
    say_count::ui::ShellContext context;
    context.document_has_path = true;
    context.problem_count = 1;
    auto state = say_count::ui::DeriveCommandBarState(context);
    CHECK(state.show_write);
    CHECK(state.show_problems);
    CHECK(state.problem_label == "1 problem");

    context.fixable_problem_count = 2;
    context.has_game = true;
    context.renpy_available = true;
    state = say_count::ui::DeriveCommandBarState(context);
    CHECK(state.show_write);
    CHECK(state.problem_label == "Fix 2");
    CHECK(state.show_preview);
    CHECK(state.enable_preview);
    CHECK(say_count::ui::CommandFor(say_count::ui::kShowHome).label == std::string("Home"));
}

TEST_CASE("command bar buttons expose native keyboard and accessibility semantics") {
    wxFrame frame(nullptr, wxID_ANY, "command button test");
    say_count::ui::CommandButton button(&frame, "Preview game", true);
    CHECK(button.AcceptsFocusFromKeyboard());
    CHECK(button.GetLabel() == "Preview game");
    CHECK(button.GetName() == "Preview game");
    button.SetCommandLabel("Stop preview");
    CHECK(button.GetLabel() == "Stop preview");
    CHECK(button.GetName() == "Stop preview");
}

TEST_CASE("onboarding setting defaults safely and persists") {
    namespace fs = std::filesystem;
    const fs::path data = fs::temp_directory_path() / "say-count-onboarding-settings-test";
    fs::remove_all(data);
    wxString previous;
    const bool had_previous = wxGetEnv("XDG_DATA_HOME", &previous);
    wxSetEnv("XDG_DATA_HOME", wxString::FromUTF8(data.string()));
    {
        say_count::app::Settings settings;
        auto editor = settings.LoadEditor();
        CHECK(editor.onboarding_version == 0);
        editor.onboarding_version = 1;
        REQUIRE(settings.SaveEditor(editor));
        CHECK(settings.LoadEditor().onboarding_version == 1);
    }
    if (had_previous) wxSetEnv("XDG_DATA_HOME", previous); else wxUnsetEnv("XDG_DATA_HOME");
    fs::remove_all(data);
}

TEST_CASE("story element builder exposes an accessible preview-first flow") {
    say_count::ui::StoryElementDialog dialog(nullptr, "    ");
    auto* type = Named<wxChoice>(dialog, "Story element type");
    auto* primary = Named<wxTextCtrl>(dialog, "Story element primary field");
    auto* preview = Named<wxTextCtrl>(dialog, "Story element preview");
    auto* insert = Named<wxButton>(dialog, "Insert into story");
    REQUIRE(type);
    REQUIRE(primary);
    REQUIRE(preview);
    REQUIRE(insert);
    CHECK(type->AcceptsFocusFromKeyboard());
    CHECK(primary->AcceptsFocusFromKeyboard());
    CHECK(preview->IsEditable() == false);
    CHECK_FALSE(insert->IsEnabled());
}

TEST_CASE("story element insertion reuses current indentation as one edit") {
    wxFrame frame(nullptr, wxID_ANY, "story insertion test");
    say_count::app::EditorSettings settings;
    say_count::ui::EditorNotebook notebook(&frame, settings, [](const wxString&, const say_count::ScriptAnalysis&) {});
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(notebook.GetPage(0));
    REQUIRE(editor);
    editor->SetText("label start:\n    ");
    editor->GotoPos(editor->GetTextLength());
    const auto built = say_count::build_story_element({say_count::StoryElementKind::Dialogue,
        "e", "Hello", {}, notebook.CurrentIndentation().ToStdString(wxConvUTF8)});
    REQUIRE(built.valid);
    notebook.InsertStoryElement(built.text);
    CHECK(editor->GetText() == "label start:\n    e \"Hello\"\n");
    editor->Undo();
    CHECK(editor->GetText() == "label start:\n    ");
}

TEST_CASE("character definitions are inserted at top level as one edit") {
    wxFrame frame(nullptr, wxID_ANY, "top-level definition test");
    say_count::app::EditorSettings settings;
    say_count::ui::EditorNotebook notebook(&frame, settings, [](const wxString&, const say_count::ScriptAnalysis&) {});
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(notebook.GetPage(0));
    REQUIRE(editor);
    editor->SetText("label start:\n    narrator \"Hello\"\n");
    editor->GotoPos(editor->GetTextLength());
    notebook.InsertTopLevelDefinition("define e = Character(\"Eileen\")\n");
    CHECK(editor->GetText() == "define e = Character(\"Eileen\")\n\nlabel start:\n    narrator \"Hello\"\n");
    editor->Undo();
    CHECK(editor->GetText() == "label start:\n    narrator \"Hello\"\n");
}

TEST_CASE("story library turns cast and project media into plain writing actions") {
    wxFrame frame(nullptr, wxID_ANY, "story library test");
    say_count::ui::StoryLibraryPanel panel(&frame);
    say_count::ScriptAnalysis analysis;
    analysis.character_names = {{"e", "Eileen"}, {"m", "Morgan"}};
    panel.SetLibrary(analysis, {
        {"images/bg_kitchen.png", "/game/images/bg_kitchen.png", say_count::AssetKind::Image},
        {"audio/theme.ogg", "/game/audio/theme.ogg", say_count::AssetKind::Audio},
    });

    auto* cast = Named<wxListBox>(panel, "Story library characters");
    auto* line = Named<wxTextCtrl>(panel, "Character dialogue");
    auto* add = Named<wxButton>(panel, "Add character dialogue");
    auto* places = Named<wxListBox>(panel, "Story library places");
    auto* background = Named<wxButton>(panel, "Set story background");
    auto* music = Named<wxButton>(panel, "Play story music");
    REQUIRE(cast); REQUIRE(line); REQUIRE(add); REQUIRE(places); REQUIRE(background); REQUIRE(music);
    CHECK(cast->GetCount() == 2);
    CHECK(cast->GetString(0).Contains("Eileen"));
    CHECK(places->GetCount() == 1);
    CHECK(add->AcceptsFocusFromKeyboard());
    CHECK_FALSE(add->IsEnabled());

    std::string inserted, status;
    panel.SetIndentationProvider([] { return std::string("    "); });
    panel.SetInsertHandler([&](const std::string& text, const std::string& message) {
        inserted = text; status = message;
    });
    cast->SetSelection(0);
    line->SetValue("The door is open.");
    wxCommandEvent add_event(wxEVT_BUTTON, add->GetId());
    add->GetEventHandler()->ProcessEvent(add_event);
    CHECK(inserted == "    e \"The door is open.\"\n");
    CHECK(status == "Dialogue added — Undo is available");

    wxCommandEvent background_event(wxEVT_BUTTON, background->GetId());
    background->GetEventHandler()->ProcessEvent(background_event);
    CHECK(inserted == "    scene images bg kitchen\n");
    wxCommandEvent music_event(wxEVT_BUTTON, music->GetId());
    music->GetEventHandler()->ProcessEvent(music_event);
    CHECK(inserted == "    play music \"audio/theme.ogg\"\n");

    auto* search = Named<wxTextCtrl>(panel, "Search story library");
    REQUIRE(search);
    search->SetValue("missing");
    CHECK(cast->GetCount() == 0);
    CHECK(places->GetCount() == 0);
}

TEST_CASE("project scripts stay indexed while editor tabs open lazily") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "say-count-lazy-project-ui-test";
    fs::remove_all(root);
    fs::create_directories(root / "chapter-one");
    fs::create_directories(root / "chapter-two");
    std::ofstream(root / "chapter-one" / "scene.rpy") << "label one:\n    return\n";
    std::ofstream(root / "chapter-two" / "scene.rpy") << "label two:\n    return\n";

    wxFrame frame(nullptr, wxID_ANY, "lazy project test");
    say_count::app::EditorSettings settings;
    say_count::ui::EditorNotebook notebook(&frame, settings,
        [](const wxString&, const say_count::ScriptAnalysis&) {});
    const std::vector<say_count::ProjectScriptFile> files{
        {"chapter-one/scene.rpy", (root / "chapter-one" / "scene.rpy").string()},
        {"chapter-two/scene.rpy", (root / "chapter-two" / "scene.rpy").string()},
    };
    REQUIRE(notebook.OpenProjectFiles(files));
    CHECK(notebook.GetPageCount() == 1);
    const auto scripts = notebook.ProjectScripts();
    REQUIRE(scripts.size() == 2);
    CHECK(scripts[0].name == "chapter-one/scene.rpy");
    CHECK(scripts[1].name == "chapter-two/scene.rpy");
    notebook.SelectFileIndex(1);
    CHECK(notebook.GetPageCount() == 2);
    CHECK(notebook.CurrentFileName() == "chapter-two/scene.rpy");
    fs::remove_all(root);
}

TEST_CASE("script index filters relative paths and opens the selected project file") {
    wxFrame frame(nullptr, wxID_ANY, "script index test");
    say_count::ui::ProjectNavigatorPanel panel(&frame);
    panel.SetProject({
        {"chapter-one/scene.rpy", "/game/chapter-one/scene.rpy"},
        {"chapter-two/scene.rpy", "/game/chapter-two/scene.rpy"},
    });
    auto* search = Named<wxTextCtrl>(panel, "Search project scripts");
    auto* scripts = Named<wxListBox>(panel, "Project scripts");
    auto* open = Named<wxButton>(panel, "Open selected project script");
    REQUIRE(search); REQUIRE(scripts); REQUIRE(open);
    CHECK(panel.script_count() == 2);
    search->SetValue("chapter-two");
    REQUIRE(scripts->GetCount() == 1);
    CHECK(scripts->GetString(0) == "chapter-two/scene.rpy");
    std::optional<std::size_t> opened;
    panel.SetOpenHandler([&](std::size_t index) { opened = index; });
    scripts->SetSelection(0);
    wxCommandEvent event(wxEVT_BUTTON, open->GetId());
    open->GetEventHandler()->ProcessEvent(event);
    REQUIRE(opened);
    CHECK(*opened == 1);
}

TEST_CASE("writing draft keeps natural writing separate from generated script") {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "say-count-writer-dialog-test";
    fs::remove_all(root); fs::create_directories(root);
    const std::string path = (root / "scene.rpy").string();
    say_count::ui::WriterDraftDialog dialog(nullptr, path, "label start:\n    pass\n");
    auto* writing = Named<wxTextCtrl>(dialog, "Natural writing draft");
    auto* preview = Named<wxTextCtrl>(dialog, "Generated game script");
    auto* save = Named<wxButton>(dialog, "Save writing draft");
    auto* update = Named<wxButton>(dialog, "Update game script");
    REQUIRE(writing);
    REQUIRE(preview);
    REQUIRE(save);
    REQUIRE(update);
    writing->SetValue("Eileen: Hello");
    CHECK(preview->GetValue().Contains("Character(\"Eileen\")"));
    CHECK(save->IsEnabled());
    CHECK(update->IsEnabled());
    CHECK(dialog.script_differs());
    fs::remove_all(root);
}

TEST_CASE("reverse preview requires loss acknowledgement and copy uses preview text") {
    const char* source =
        "default r = ChatCharacter(\"Robo\", icon=\"images/r.png\")\n"
        "r \"Hello\" (c=\"#side\", fastmode=0.1)\n";
    say_count::ui::ChatConversionDialog dialog(nullptr, source, false);
    auto* preview = Named<wxTextCtrl>(dialog, "Chat conversion preview");
    auto* acknowledge = Named<wxCheckBox>(dialog, "Acknowledge chat metadata loss");
    auto* replace = Named<wxButton>(dialog, "Replace source with chat conversion");
    auto* copy = Named<wxButton>(dialog, "Copy chat conversion result");
    REQUIRE(preview);
    REQUIRE(acknowledge);
    REQUIRE(replace);
    REQUIRE(copy);
    REQUIRE(WaitForText(preview));
    CHECK(acknowledge->IsShown());
    CHECK_FALSE(replace->IsEnabled());
    acknowledge->SetValue(true);
    wxCommandEvent checked(wxEVT_CHECKBOX, acknowledge->GetId());
    acknowledge->GetEventHandler()->ProcessEvent(checked);
    CHECK(replace->IsEnabled());

    wxCommandEvent clicked(wxEVT_BUTTON, copy->GetId());
    copy->GetEventHandler()->ProcessEvent(clicked);
    REQUIRE(wxTheClipboard->Open());
    wxTextDataObject copied;
    REQUIRE(wxTheClipboard->GetData(copied));
    wxTheClipboard->Close();
    CHECK(copied.GetText() == preview->GetValue());
}

TEST_CASE("cancel closes the modal conversion without accepting it") {
    say_count::ui::ChatConversionDialog dialog(nullptr, "Robo: Hello", true);
    wxTheApp->CallAfter([&dialog] {
        auto* cancel = Named<wxButton>(dialog, "Cancel chat conversion");
        REQUIRE(cancel);
        wxCommandEvent clicked(wxEVT_BUTTON, cancel->GetId());
        cancel->GetEventHandler()->ProcessEvent(clicked);
    });
    CHECK(dialog.ShowModal() == wxID_CANCEL);
}

TEST_CASE("chat replacement respects selection scope and is one undo step") {
    wxFrame frame(nullptr, wxID_ANY, "chat editor test");
    say_count::app::EditorSettings settings;
    say_count::ui::EditorNotebook notebook(&frame, settings,
                                           [](const wxString&, const say_count::ScriptAnalysis&) {});
    auto* editor = dynamic_cast<wxStyledTextCtrl*>(notebook.GetPage(0));
    REQUIRE(editor);
    const wxString original = "Robo: one\nRobo: two\n";
    editor->SetText(original);
    editor->EmptyUndoBuffer();
    const int start = original.Find("Robo: two");
    editor->SetSelection(start, start + 9);
    const auto selection = notebook.PrepareTextReplacement();
    REQUIRE(selection);
    CHECK(selection->selection);
    CHECK(selection->source == "Robo: two");
    REQUIRE(notebook.ApplyTextReplacement(*selection, "r \"two\""));
    CHECK(editor->GetText() == "Robo: one\nr \"two\"\n");
    REQUIRE(editor->CanUndo());
    editor->Undo();
    CHECK(editor->GetText() == original);
    CHECK_FALSE(editor->CanUndo());

    editor->SetSelection(0, 0);
    const auto document = notebook.PrepareTextReplacement();
    REQUIRE(document);
    CHECK_FALSE(document->selection);
    CHECK(document->source == original.ToStdString(wxConvUTF8));
}

int main(int argc, char* argv[]) {
    if (!wxEntryStart(argc, argv)) return 2;
    if (!wxTheApp || !wxTheApp->CallOnInit()) {
        wxEntryCleanup();
        return 2;
    }
    const int result = Catch::Session().run(argc, argv);
    wxTheApp->OnExit();
    wxEntryCleanup();
    return result;
}
