#pragma once

#include <string>
#include <string_view>
#include <map>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <optional>
#include <thread>

#include <wx/dialog.h>

#include "core/chat_document.h"

class wxCheckBox;
class wxStaticText;
class wxTextCtrl;
class wxButton;
class wxDataViewListCtrl;

namespace say_count::ui {

void ShowChatGuide(wxWindow* parent);

class ChatConversionDialog final : public wxDialog {
public:
    ChatConversionDialog(wxWindow* parent, std::string_view source, bool to_chat,
                         std::map<std::string, std::string> existing_characters = {});
    ~ChatConversionDialog() override;

    const ChatConversion& conversion() const noexcept { return conversion_; }

private:
    struct ConversionRequest {
        std::uint64_t generation = 0;
        std::string channel;
        std::string narrator_alias;
        bool chat_narration = false;
        bool ordinary_renpy = false;
        std::map<std::string, std::string> alias_overrides;
    };

    void RequestConversion();
    ChatConversion Convert(const ConversionRequest& request) const;
    void WorkerLoop();
    void OnConversionReady(wxThreadEvent& event);

    std::string source_;
    bool to_chat_ = true;
    std::map<std::string, std::string> existing_characters_;
    ChatConversion conversion_;
    wxTextCtrl* channel_ = nullptr;
    wxCheckBox* chat_narration_ = nullptr;
    wxTextCtrl* narrator_alias_ = nullptr;
    wxCheckBox* ordinary_renpy_ = nullptr;
    wxTextCtrl* preview_ = nullptr;
    wxStaticText* summary_ = nullptr;
    wxStaticText* losses_ = nullptr;
    wxCheckBox* acknowledge_losses_ = nullptr;
    wxButton* replace_ = nullptr;
    wxButton* copy_ = nullptr;
    wxDataViewListCtrl* mappings_ = nullptr;
    std::map<std::string, std::string> alias_overrides_;
    bool mappings_initialized_ = false;
    std::atomic<std::uint64_t> generation_{0};
    std::mutex worker_mutex_;
    std::condition_variable worker_wakeup_;
    std::optional<ConversionRequest> pending_request_;
    bool stopping_ = false;
    std::thread worker_;
};

}  // namespace say_count::ui
