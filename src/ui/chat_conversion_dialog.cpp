#include "ui/chat_conversion_dialog.h"

#include "core/chat_dialogue_adapter.h"
#include "core/chat_program_formatter.h"
#include "core/chat_program_parser.h"

#include <algorithm>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/clipbrd.h>
#include <wx/collpane.h>
#include <wx/dataview.h>
#include <wx/html/htmlwin.h>
#include <wx/notebook.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/thread.h>

#include "ui/guide_dialog.h"
#include "ui/style.h"

namespace say_count::ui {
namespace {

wxDEFINE_EVENT(kChatConversionReady, wxThreadEvent);

std::string HtmlEscape(const std::string& text) {
    std::string out;
    out.reserve(text.size());
    for (const char c : text) {
        switch (c) {
            case '&': out += "&amp;"; break;
            case '<': out += "&lt;"; break;
            case '>': out += "&gt;"; break;
            default: out += c;
        }
    }
    return out;
}

}  // namespace

void ShowChatGuide(wxWindow* parent) {
    GuideDialog(parent, GuideKind::Chat).ShowModal();
}

ChatConversion ChatConversionDialog::Convert(const ConversionRequest& request) const {
    ChatConversion conversion = to_chat_
        ? convert_manuscript_to_chat(source_, request.channel, existing_characters_,
            request.chat_narration ? request.narrator_alias : std::string{}, "Narrator",
            request.bridge_skin)
        : convert_chat_to_manuscript(source_, request.ordinary_renpy);
    if (!to_chat_) return conversion;

    std::map<std::string, std::string> aliases_by_name;
    for (const auto& [alias, name] : existing_characters_) aliases_by_name[name] = alias;
    for (const auto& character : conversion.document.characters)
        aliases_by_name[character.name] = character.alias;
    std::map<std::string, std::string> alias_changes;
    for (const auto& [name, alias] : aliases_by_name) {
        const auto override = request.alias_overrides.find(name);
        if (override != request.alias_overrides.end() && !override->second.empty())
            alias_changes[alias] = override->second;
    }
    for (auto& character : conversion.document.characters) {
        const auto changed = alias_changes.find(character.alias);
        if (changed != alias_changes.end()) character.alias = changed->second;
    }
    const auto remap = [&](const auto& self, std::vector<ChatEvent>* events) -> void {
        for (auto& event : *events) {
            const auto speaker = alias_changes.find(event.speaker);
            if (speaker != alias_changes.end()) event.speaker = speaker->second;
            for (auto& typer : event.other_typers) {
                const auto changed = alias_changes.find(typer);
                if (changed != alias_changes.end()) typer = changed->second;
            }
            self(self, &event.children);
        }
    };
    remap(remap, &conversion.document.events);
    conversion.text = format_chat_program(conversion.document, "chat_scene", true,
                                          request.bridge_skin);
    return conversion;
}

ChatConversionDialog::ChatConversionDialog(wxWindow* parent, std::string_view source, bool to_chat,
                                           std::map<std::string, std::string> existing_characters)
    : wxDialog(parent, wxID_ANY, to_chat ? "Convert to Chat Format" : "Convert Chat to Dialogue",
               wxDefaultPosition, wxSize(1040, 720), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
      source_(source), to_chat_(to_chat), existing_characters_(std::move(existing_characters)) {
    Bind(kChatConversionReady, &ChatConversionDialog::OnConversionReady, this);
    auto* layout = new wxBoxSizer(wxVERTICAL);
    auto* heading = new wxStaticText(this, wxID_ANY,
        to_chat ? "Turn your writing into a chat scene"
                : "Turn a chat scene back into normal writing");
    heading->SetFont(style::BodyFont(14, wxFONTWEIGHT_BOLD));
    layout->Add(heading, 0, wxLEFT | wxRIGHT | wxTOP, 16);
    auto* subtitle = new wxStaticText(this, wxID_ANY,
        to_chat ? "Pick a look, check the preview below, then click \"Use this\". "
                  "Undo brings your original text back."
                : "Pick a format, check the preview below, then click \"Use this\". "
                  "Undo brings the chat scene back.");
    subtitle->SetForegroundColour(style::Colors().ink_soft);
    layout->Add(subtitle, 0, wxLEFT | wxRIGHT | wxTOP | wxBOTTOM, 16);
    if (to_chat_) {
        const wxString styles[] = {"Discord-style channels", "Kik-style messenger"};
        app_style_ = new wxRadioBox(this, wxID_ANY, "How should the chat app look?",
                                    wxDefaultPosition, wxDefaultSize, 2, styles, 2,
                                    wxRA_SPECIFY_COLS);
        app_style_->SetName("Chat app style");
        layout->Add(app_style_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        channel_label_ = new wxStaticText(this, wxID_ANY, "Chat room name");
        row->Add(channel_label_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        channel_ = new wxTextCtrl(this, wxID_ANY, "#general");
        channel_->SetName("Default chat channel");
        row->Add(channel_, 1);
        layout->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        channel_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RequestConversion(); });
        app_style_->Bind(wxEVT_RADIOBOX, [this](wxCommandEvent&) {
            // A channel reads as a contact/thread name in the messenger style;
            // only swap the value if the user has not customized it.
            const bool kik = app_style_->GetSelection() == 1;
            channel_label_->SetLabel(kik ? "Who the player is chatting with" : "Chat room name");
            if (kik && channel_->GetValue() == "#general") {
                std::string contact = "dms";
                for (const auto& character : conversion_.document.characters) {
                    if (character.is_player) continue;
                    contact = character.name;
                    break;
                }
                channel_->ChangeValue(wxString::FromUTF8(contact));
            } else if (!kik && (channel_->GetValue() == "dms" ||
                                channel_->GetValue().Find('#') == wxNOT_FOUND)) {
                channel_->ChangeValue("#general");
            }
            Layout();
            RequestConversion();
        });
        wrap_bridge_ = new wxCheckBox(this, wxID_ANY,
            "Make a complete scene: open the chat app here and return to normal dialogue "
            "at the end (recommended)");
        wrap_bridge_->SetName("Wrap in chat bridge calls");
        wrap_bridge_->SetValue(true);
        layout->Add(wrap_bridge_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
        wrap_bridge_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { RequestConversion(); });
        auto* advanced = new wxCollapsiblePane(this, wxID_ANY, "Advanced options");
        advanced->SetName("Advanced chat options");
        auto* pane = advanced->GetPane();
        auto* pane_sizer = new wxBoxSizer(wxVERTICAL);
        auto* narration_row = new wxBoxSizer(wxHORIZONTAL);
        chat_narration_ = new wxCheckBox(pane, wxID_ANY,
            "Show narration (text without a speaker) as messages from:");
        chat_narration_->SetName("Send narration as chat messages");
        narrator_alias_ = new wxTextCtrl(pane, wxID_ANY, "narrator");
        narrator_alias_->SetName("Narrator chat alias");
        narration_row->Add(chat_narration_, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        narration_row->Add(narrator_alias_, 1);
        pane_sizer->Add(narration_row, 0, wxEXPAND | wxBOTTOM, 8);
        chat_narration_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
            narrator_alias_->Enable(chat_narration_->GetValue());
            RequestConversion();
        });
        narrator_alias_->Bind(wxEVT_TEXT, [this](wxCommandEvent&) { RequestConversion(); });
        narrator_alias_->Disable();
        pane_sizer->Add(new wxStaticText(pane, wxID_ANY,
            "Characters: the short code used in the script, and who it stands for.\n"
            "Double-click a short code to change it."),
            0, wxBOTTOM, 8);
        mappings_ = new wxDataViewListCtrl(pane, wxID_ANY, wxDefaultPosition, wxSize(-1, 130));
        mappings_->SetName("Chat character mappings");
        mappings_->AppendTextColumn("Short code", wxDATAVIEW_CELL_EDITABLE, 180);
        mappings_->AppendTextColumn("Character", wxDATAVIEW_CELL_INERT, 320);
        pane_sizer->Add(mappings_, 0, wxEXPAND);
        pane->SetSizer(pane_sizer);
        layout->Add(advanced, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
        advanced->Bind(wxEVT_COLLAPSIBLEPANE_CHANGED,
                       [this](wxCollapsiblePaneEvent&) { Layout(); });
        mappings_->Bind(wxEVT_DATAVIEW_ITEM_VALUE_CHANGED, [this](wxDataViewEvent& event) {
            const int row = mappings_->ItemToRow(event.GetItem());
            if (row == wxNOT_FOUND) return;
            const std::string alias = mappings_->GetTextValue(row, 0).ToStdString(wxConvUTF8);
            const std::string name = mappings_->GetTextValue(row, 1).ToStdString(wxConvUTF8);
            if (!valid_chat_alias(alias)) {
                wxBell();
                const auto existing = std::find_if(existing_characters_.begin(), existing_characters_.end(),
                    [&](const auto& entry) { return entry.second == name; });
                std::string original = existing == existing_characters_.end() ? "character" : existing->first;
                for (const auto& character : conversion_.document.characters)
                    if (character.name == name) original = character.alias;
                mappings_->SetTextValue(wxString::FromUTF8(original), row, 0);
                return;
            }
            alias_overrides_[name] = alias;
            RequestConversion();
        });
    } else {
        ordinary_renpy_ = new wxCheckBox(this, wxID_ANY,
            "Output game script lines (alias \"text\") instead of plain \"Name: text\" writing");
        ordinary_renpy_->SetName("Chat conversion output format");
        layout->Add(ordinary_renpy_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
        ordinary_renpy_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) { RequestConversion(); });
    }
    summary_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    summary_->SetForegroundColour(style::Colors().ink_soft);
    layout->Add(summary_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    losses_ = new wxStaticText(this, wxID_ANY, wxEmptyString);
    losses_->SetName("Chat metadata losses");
    losses_->SetForegroundColour(style::Colors().plum);
    losses_->Hide();
    layout->Add(losses_, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
    acknowledge_losses_ = new wxCheckBox(this, wxID_ANY,
        "I understand: the chat-only details listed above will be removed");
    acknowledge_losses_->SetName("Acknowledge chat metadata loss");
    acknowledge_losses_->Hide();
    layout->Add(acknowledge_losses_, 0, wxLEFT | wxRIGHT | wxBOTTOM, 16);
    acknowledge_losses_->Bind(wxEVT_CHECKBOX, [this](wxCommandEvent&) {
        replace_->Enable(!conversion_.text.empty() &&
                         (conversion_.losses.empty() || acknowledge_losses_->GetValue()));
    });
    if (to_chat_) {
        auto* tabs = new wxNotebook(this, wxID_ANY);
        tabs->SetName("Chat preview tabs");
        visual_ = new wxHtmlWindow(tabs, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                   wxHW_SCROLLBAR_AUTO | wxBORDER_NONE);
        visual_->SetName("Chat visual preview");
        visual_->SetBorders(10);
        preview_ = new wxTextCtrl(tabs, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
        preview_->SetName("Chat conversion preview");
        tabs->AddPage(visual_, "What players will see");
        tabs->AddPage(preview_, "Script code (advanced)");
        layout->Add(tabs, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);
    } else {
        preview_ = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                  wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
        preview_->SetName("Chat conversion preview");
        layout->Add(preview_, 1, wxEXPAND | wxLEFT | wxRIGHT, 16);
    }
    auto* actions = new wxStdDialogButtonSizer();
    auto* guide = new wxButton(this, wxID_HELP, "Guide");
    guide->SetName("Open chat format guide");
    guide->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowChatGuide(this); });
    actions->AddButton(guide);
    copy_ = new wxButton(this, wxID_ANY, "Copy code");
    copy_->SetName("Copy chat conversion result");
    replace_ = new wxButton(this, wxID_OK, "Use this");
    replace_->SetName("Replace source with chat conversion");
    replace_->SetToolTip(to_chat_ ? "Replace your selected text with the chat scene (Undo restores it)"
                                  : "Replace the chat scene with the plain writing (Undo restores it)");
    actions->AddButton(copy_);
    actions->AddButton(replace_);
    auto* cancel = new wxButton(this, wxID_CANCEL, "Cancel");
    cancel->SetName("Cancel chat conversion");
    actions->AddButton(cancel);
    actions->Realize();
    layout->Add(actions, 0, wxEXPAND | wxALL, 16);
    SetSizer(layout);
    copy_->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (wxTheClipboard->Open()) {
            wxTheClipboard->SetData(new wxTextDataObject(wxString::FromUTF8(conversion_.text)));
            wxTheClipboard->Close();
        }
    });
    worker_ = std::thread(&ChatConversionDialog::WorkerLoop, this);
    RequestConversion();
    CentreOnParent();
}

ChatConversionDialog::~ChatConversionDialog() {
    generation_.fetch_add(1, std::memory_order_relaxed);
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        stopping_ = true;
        pending_request_.reset();
    }
    worker_wakeup_.notify_one();
    if (worker_.joinable()) worker_.join();
}

void ChatConversionDialog::RequestConversion() {
    ConversionRequest request;
    request.generation = generation_.fetch_add(1, std::memory_order_relaxed) + 1;
    request.channel = channel_ ? channel_->GetValue().ToStdString(wxConvUTF8) : std::string{};
    request.chat_narration = chat_narration_ && chat_narration_->GetValue();
    request.narrator_alias = narrator_alias_
        ? narrator_alias_->GetValue().ToStdString(wxConvUTF8) : std::string{};
    if (wrap_bridge_ && wrap_bridge_->GetValue())
        request.bridge_skin = app_style_ && app_style_->GetSelection() == 1 ? "kik" : "discord";
    request.ordinary_renpy = ordinary_renpy_ && ordinary_renpy_->GetValue();
    request.alias_overrides = alias_overrides_;
    {
        std::lock_guard<std::mutex> lock(worker_mutex_);
        pending_request_ = std::move(request);
    }
    summary_->SetLabel("Converting…");
    replace_->Disable();
    copy_->Disable();
    worker_wakeup_.notify_one();
}

void ChatConversionDialog::WorkerLoop() {
    for (;;) {
        ConversionRequest request;
        {
            std::unique_lock<std::mutex> lock(worker_mutex_);
            worker_wakeup_.wait(lock, [this] { return stopping_ || pending_request_.has_value(); });
            if (stopping_) return;
            request = std::move(*pending_request_);
            pending_request_.reset();
        }
        auto conversion = Convert(request);
        if (generation_.load(std::memory_order_relaxed) != request.generation) continue;
        auto* event = new wxThreadEvent(kChatConversionReady);
        event->SetPayload(std::pair<std::uint64_t, ChatConversion>{
            request.generation, std::move(conversion)});
        wxQueueEvent(this, event);
    }
}

std::string ChatConversionDialog::BuildVisualHtml() const {
    std::map<std::string, const ChatCharacter*> characters;
    for (const auto& character : conversion_.document.characters)
        characters[character.alias] = &character;
    std::string body;
    std::string last_channel = conversion_.document.default_channel;
    const auto append = [&](const auto& self, const std::vector<ChatEvent>& events) -> void {
        for (const auto& event : events) {
            if (!event.channel.empty() &&
                (event.channel_explicit || event.channel != last_channel)) {
                body += "<tr><td colspan=2 align=center><font size=2 color=\"#76546E\">— " +
                        HtmlEscape(event.channel) + " —</font></td></tr>";
                last_channel = event.channel;
            }
            switch (event.kind) {
                case ChatEventKind::Message: {
                    if (!event.other_typers.empty()) {
                        std::string typers;
                        for (std::size_t i = 0; i < event.other_typers.size(); ++i) {
                            if (i) typers += i + 1 == event.other_typers.size() ? " and " : ", ";
                            const auto typer = characters.find(event.other_typers[i]);
                            typers += HtmlEscape(typer == characters.end()
                                ? event.other_typers[i] : typer->second->name);
                        }
                        body += "<tr><td colspan=2><font size=2 color=\"#8A94A0\"><i>" +
                                typers + (event.other_typers.size() == 1 ? " is" : " are") +
                                " typing…</i></font></td></tr>";
                    }
                    const auto found = characters.find(event.speaker);
                    const ChatCharacter* who =
                        found == characters.end() ? nullptr : found->second;
                    const bool player = who && who->is_player;
                    const std::string text = HtmlEscape(event.text);
                    if (player) {
                        body += "<tr><td width=\"30%\"></td>"
                                "<td bgcolor=\"#D8E9FC\" align=right>"
                                "<font size=2 color=\"#4C5A6D\">You</font><br>" +
                                text + "</td></tr>";
                    } else {
                        const std::string name =
                            HtmlEscape(who ? who->name : event.speaker);
                        body += "<tr><td bgcolor=\"#EEF1F3\"><b><font color=\"#5A2E52\">" +
                                name + "</font></b><br>" + text +
                                "</td><td width=\"30%\"></td></tr>";
                    }
                    break;
                }
                case ChatEventKind::Choice:
                    body += "<tr><td width=\"30%\"></td>"
                            "<td bgcolor=\"#FBF0D8\" align=right>"
                            "<font size=2 color=\"#4C5A6D\">Player choice</font><br>" +
                            HtmlEscape(event.text) + "</td></tr>";
                    break;
                case ChatEventKind::Narration:
                    body += "<tr><td colspan=2 align=center><i><font color=\"#4C5A6D\">" +
                            HtmlEscape(event.text) + "</font></i></td></tr>";
                    break;
                case ChatEventKind::Passthrough:
                    if (!event.original.empty())
                        body += "<tr><td colspan=2><font size=1 color=\"#8A94A0\">"
                                "kept as-is: <tt>" + HtmlEscape(event.original) +
                                "</tt></font></td></tr>";
                    break;
            }
            self(self, event.children);
        }
    };
    append(append, conversion_.document.events);
    if (body.empty())
        body = "<tr><td align=center><font color=\"#4C5A6D\">Nothing to show yet — "
               "select or type some writing first.</font></td></tr>";
    return "<html><body bgcolor=\"#FCFBF8\" text=\"#17243A\">"
           "<table width=\"100%\" cellpadding=\"6\" cellspacing=\"4\">" + body +
           "</table></body></html>";
}

void ChatConversionDialog::OnConversionReady(wxThreadEvent& event) {
    auto payload = event.GetPayload<std::pair<std::uint64_t, ChatConversion>>();
    if (payload.first != generation_.load(std::memory_order_relaxed)) return;
    conversion_ = std::move(payload.second);
    if (to_chat_) {
        std::map<std::string, std::string> aliases_by_name;
        for (const auto& [alias, name] : existing_characters_) aliases_by_name[name] = alias;
        for (const auto& character : conversion_.document.characters)
            aliases_by_name[character.name] = character.alias;
        if (!mappings_initialized_) {
            for (const auto& [name, alias] : aliases_by_name)
                mappings_->AppendItem({wxVariant(wxString::FromUTF8(alias)),
                                       wxVariant(wxString::FromUTF8(name))});
            mappings_initialized_ = true;
        }
    }
    preview_->ChangeValue(wxString::FromUTF8(conversion_.text));
    if (visual_) visual_->SetPage(wxString::FromUTF8(BuildVisualHtml()));
    std::string summary = std::to_string(conversion_.messages) + " messages · " +
        std::to_string(conversion_.narration) + " narration · " +
        std::to_string(conversion_.choices) + " choices";
    if (!conversion_.losses.empty()) summary += " · " + std::to_string(conversion_.losses.size()) + " metadata losses";
    if (!conversion_.document.diagnostics.empty()) summary += " · " + std::to_string(conversion_.document.diagnostics.size()) + " warnings";
    summary_->SetLabel(wxString::FromUTF8(summary));
    std::string loss_details;
    for (const auto& loss : conversion_.losses) {
        if (!loss_details.empty()) loss_details += "\n";
        loss_details += "• " + loss.message;
    }
    if (!loss_details.empty())
        loss_details = "These chat-only details cannot be kept and will be removed:\n" + loss_details;
    losses_->SetLabel(wxString::FromUTF8(loss_details));
    losses_->Show(!conversion_.losses.empty());
    acknowledge_losses_->Show(!conversion_.losses.empty());
    if (conversion_.losses.empty()) acknowledge_losses_->SetValue(false);
    replace_->Enable(!conversion_.text.empty() &&
                     (conversion_.losses.empty() || acknowledge_losses_->GetValue()));
    copy_->Enable(!conversion_.text.empty());
    Layout();
}

}  // namespace say_count::ui
