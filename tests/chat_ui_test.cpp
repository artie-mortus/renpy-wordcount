#include <catch2/catch_session.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ui/chat_conversion_dialog.h"
#include "ui/editor_notebook.h"

#include <chrono>
#include <thread>

#include <wx/app.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/clipbrd.h>
#include <wx/frame.h>
#include <wx/init.h>
#include <wx/radiobox.h>
#include <wx/stc/stc.h>
#include <wx/textctrl.h>

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

}  // namespace

TEST_CASE("chat conversion dialog exposes keyboard-accessible forward controls") {
    say_count::ui::ChatConversionDialog dialog(nullptr, "Robo: Hello", true,
                                                {{"r", "Robo"}});
    auto* channel = Named<wxTextCtrl>(dialog, "Default chat channel");
    auto* app_style = Named<wxRadioBox>(dialog, "Chat app style");
    auto* wrap = Named<wxCheckBox>(dialog, "Wrap in chat bridge calls");
    auto* narration = Named<wxCheckBox>(dialog, "Send narration as chat messages");
    auto* preview = Named<wxTextCtrl>(dialog, "Chat conversion preview");
    auto* copy = Named<wxButton>(dialog, "Copy chat conversion result");
    auto* replace = Named<wxButton>(dialog, "Replace source with chat conversion");
    auto* cancel = Named<wxButton>(dialog, "Cancel chat conversion");
    REQUIRE(channel);
    REQUIRE(app_style);
    REQUIRE(wrap);
    CHECK(app_style->GetCount() == 2);
    CHECK_FALSE(wrap->GetValue());
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
