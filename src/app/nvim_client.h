#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

class wxProcess;

namespace say_count::app {

struct NvimEditorState {
    std::string text;
    std::size_t caret = 0;
    std::size_t selection_start = 0;
    std::size_t selection_end = 0;
    std::size_t visual_anchor = 0;
    std::string mode = "n";
    std::string command_line;
    bool visual = false;
    bool visual_line = false;
    bool visual_block = false;
};

class NvimClient final {
public:
    struct Value;

    NvimClient();
    ~NvimClient();

    NvimClient(const NvimClient&) = delete;
    NvimClient& operator=(const NvimClient&) = delete;

    bool Start(std::string* error = nullptr);
    void Stop();
    bool running() const;

    std::optional<std::int64_t> CreateBuffer(std::string_view text,
                                             std::string_view name,
                                             std::string* error = nullptr);
    bool CloseBuffer(std::int64_t buffer);
    std::optional<NvimEditorState> ApplyKey(std::int64_t buffer,
                                            std::string_view source,
                                            std::size_t caret,
                                            std::string_view key,
                                            std::string* error = nullptr);

private:
    struct ModeProbe {
        bool blocking = false;
        std::string mode;
    };

    std::optional<Value> Call(std::string_view method,
                              const std::function<void(void*)>& pack_arguments,
                              std::string* error,
                              ModeProbe* probe = nullptr);
    bool Notify(std::string_view method, const std::function<void(void*)>& pack_arguments);
    bool WriteMessage(const char* data, std::size_t size);
    bool ReadResponse(std::uint64_t request_id, Value* value, std::string* error,
                      ModeProbe* probe);
    bool SetBufferText(std::int64_t buffer, std::string_view source, std::string* error,
                       ModeProbe* probe = nullptr);
    bool SetCursor(std::string_view source, std::size_t caret, std::string* error,
                   ModeProbe* probe = nullptr);
    std::optional<NvimEditorState> ReadState(std::int64_t buffer, std::string* error,
                                             ModeProbe* probe);

    std::unique_ptr<wxProcess> process_;
    long pid_ = 0;
    std::uint64_t next_request_id_ = 1;
    void* unpacker_ = nullptr;
    std::unordered_map<std::int64_t, std::string> buffer_sources_;
    std::unordered_map<std::int64_t, std::size_t> buffer_carets_;
    std::unordered_map<std::int64_t, std::string> buffer_modes_;
};

}  // namespace say_count::app
