#include "app/nvim_client.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>
#include <utility>

#include <msgpack.h>
#include <wx/filename.h>
#include <wx/process.h>
#include <wx/stream.h>
#include <wx/utils.h>

namespace say_count::app {
namespace {

constexpr long kRpcTimeoutMs = 5000;

void PackString(msgpack_packer* packer, std::string_view value) {
    msgpack_pack_str(packer, value.size());
    msgpack_pack_str_body(packer, value.data(), value.size());
}

std::vector<std::string> SplitLines(std::string_view source) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (start <= source.size()) {
        const auto newline = source.find('\n', start);
        if (newline == std::string_view::npos) {
            lines.emplace_back(source.substr(start));
            break;
        }
        std::size_t end = newline;
        if (end > start && source[end - 1] == '\r') --end;
        lines.emplace_back(source.substr(start, end - start));
        start = newline + 1;
    }
    if (lines.empty()) lines.emplace_back();
    return lines;
}

std::pair<std::size_t, std::size_t> CursorFromOffset(std::string_view source,
                                                     std::size_t offset) {
    offset = std::min(offset, source.size());
    std::size_t row = 1;
    std::size_t line_start = 0;
    for (std::size_t index = 0; index < offset; ++index) {
        if (source[index] == '\n') {
            ++row;
            line_start = index + 1;
        }
    }
    return {row, offset - line_start};
}

std::size_t OffsetFromCursor(const std::vector<std::string>& lines,
                             std::size_t row, std::size_t column) {
    if (lines.empty()) return 0;
    row = std::clamp<std::size_t>(row, 1, lines.size());
    std::size_t offset = 0;
    for (std::size_t index = 0; index + 1 < row; ++index) offset += lines[index].size() + 1;
    return offset + std::min(column, lines[row - 1].size());
}

std::string JoinLines(const std::vector<std::string>& lines) {
    std::string result;
    for (std::size_t index = 0; index < lines.size(); ++index) {
        if (index) result.push_back('\n');
        result += lines[index];
    }
    return result;
}

}  // namespace

struct NvimClient::Value {
    enum class Type { Nil, Boolean, Integer, String, Array, Map } type = Type::Nil;
    bool boolean = false;
    std::int64_t integer = 0;
    std::string string;
    std::vector<Value> array;
    std::unordered_map<std::string, Value> map;

    const Value* Find(std::string_view key) const {
        const auto found = map.find(std::string(key));
        return found == map.end() ? nullptr : &found->second;
    }
};

namespace {

NvimClient::Value CopyValue(const msgpack_object& object) {
    NvimClient::Value value;
    switch (object.type) {
        case MSGPACK_OBJECT_NIL: break;
        case MSGPACK_OBJECT_BOOLEAN:
            value.type = NvimClient::Value::Type::Boolean;
            value.boolean = object.via.boolean;
            break;
        case MSGPACK_OBJECT_POSITIVE_INTEGER:
            value.type = NvimClient::Value::Type::Integer;
            value.integer = static_cast<std::int64_t>(object.via.u64);
            break;
        case MSGPACK_OBJECT_NEGATIVE_INTEGER:
            value.type = NvimClient::Value::Type::Integer;
            value.integer = object.via.i64;
            break;
        case MSGPACK_OBJECT_STR:
            value.type = NvimClient::Value::Type::String;
            value.string.assign(object.via.str.ptr, object.via.str.size);
            break;
        case MSGPACK_OBJECT_BIN:
            value.type = NvimClient::Value::Type::String;
            value.string.assign(object.via.bin.ptr, object.via.bin.size);
            break;
        case MSGPACK_OBJECT_ARRAY:
            value.type = NvimClient::Value::Type::Array;
            value.array.reserve(object.via.array.size);
            for (std::uint32_t index = 0; index < object.via.array.size; ++index)
                value.array.push_back(CopyValue(object.via.array.ptr[index]));
            break;
        case MSGPACK_OBJECT_MAP:
            value.type = NvimClient::Value::Type::Map;
            for (std::uint32_t index = 0; index < object.via.map.size; ++index) {
                const auto& pair = object.via.map.ptr[index];
                const auto key = CopyValue(pair.key);
                if (key.type == NvimClient::Value::Type::String)
                    value.map.emplace(key.string, CopyValue(pair.val));
            }
            break;
        case MSGPACK_OBJECT_EXT: {
            value.type = NvimClient::Value::Type::Integer;
            std::uint64_t handle = 0;
            for (std::uint32_t index = 0; index < object.via.ext.size; ++index) {
                handle = (handle << 8) |
                    static_cast<unsigned char>(object.via.ext.ptr[index]);
            }
            value.integer = static_cast<std::int64_t>(handle);
            break;
        }
        case MSGPACK_OBJECT_FLOAT32:
        case MSGPACK_OBJECT_FLOAT64:
            break;
    }
    return value;
}

std::string DescribeValue(const NvimClient::Value& value) {
    if (value.type == NvimClient::Value::Type::String) return value.string;
    if (value.type == NvimClient::Value::Type::Integer) return std::to_string(value.integer);
    if (value.type == NvimClient::Value::Type::Map) {
        if (const auto* message = value.Find("message")) return DescribeValue(*message);
    }
    return "Neovim RPC error";
}

void PackInteger(msgpack_packer* packer, std::int64_t value) {
    msgpack_pack_int64(packer, value);
}

}  // namespace

NvimClient::NvimClient() = default;
NvimClient::~NvimClient() { Stop(); }

bool NvimClient::Start(std::string* error) {
    if (running()) return true;
    Stop();
    process_ = std::make_unique<wxProcess>();
    process_->Redirect();
    wxString config_base;
    if (!wxGetEnv("XDG_CONFIG_HOME", &config_base) || config_base.empty())
        config_base = wxGetHomeDir() + wxFILE_SEP_PATH + ".config";
    const wxString config_path = config_base + wxFILE_SEP_PATH + "say-count" +
                                 wxFILE_SEP_PATH + "init.lua";
    std::vector<std::string> arguments{"nvim", "--embed", "--headless"};
    if (wxFileName::FileExists(config_path)) {
        arguments.emplace_back("-u");
        arguments.push_back(config_path.ToStdString(wxConvUTF8));
    } else {
        arguments.emplace_back("--clean");
    }
    arguments.emplace_back("--cmd");
    arguments.emplace_back("let g:say_count = 1");
    std::vector<const char*> argv;
    argv.reserve(arguments.size() + 1);
    for (const auto& argument : arguments) argv.push_back(argument.c_str());
    argv.push_back(nullptr);
    pid_ = wxExecute(argv.data(), wxEXEC_ASYNC, process_.get());
    if (pid_ <= 0) {
        if (error) *error = "Could not start Neovim. Install nvim or disable Neovim motions.";
        process_.reset();
        pid_ = 0;
        return false;
    }
    auto* unpacker = new msgpack_unpacker;
    if (!msgpack_unpacker_init(unpacker, 8192)) {
        if (error) *error = "Could not initialize the Neovim RPC decoder.";
        delete unpacker;
        Stop();
        return false;
    }
    unpacker_ = unpacker;
    const auto info = Call("nvim_get_api_info", [](void* raw) {
        msgpack_pack_array(static_cast<msgpack_packer*>(raw), 0);
    }, error);
    if (!info) {
        Stop();
        return false;
    }
    return true;
}

void NvimClient::Stop() {
    buffer_sources_.clear();
    buffer_carets_.clear();
    buffer_modes_.clear();
    if (unpacker_) {
        auto* unpacker = static_cast<msgpack_unpacker*>(unpacker_);
        msgpack_unpacker_destroy(unpacker);
        delete unpacker;
        unpacker_ = nullptr;
    }
    if (process_) {
        process_->CloseOutput();
        if (pid_ > 0 && wxProcess::Exists(pid_)) wxProcess::Kill(pid_, wxSIGTERM, wxKILL_CHILDREN);
        process_->Detach();
        process_.release();
    }
    pid_ = 0;
}

bool NvimClient::running() const {
    return process_ && pid_ > 0 && wxProcess::Exists(pid_);
}

bool NvimClient::WriteMessage(const char* data, std::size_t size) {
    if (!running() || !process_->GetOutputStream()) return false;
    auto* output = process_->GetOutputStream();
    std::size_t written = 0;
    const auto started = std::chrono::steady_clock::now();
    while (written < size) {
        const std::size_t chunk = std::min<std::size_t>(8192, size - written);
        output->Reset();
        output->Write(data + written, chunk);
        const std::size_t count = output->LastWrite();
        if (count == 0) {
            const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started).count();
            if (!running() || elapsed >= kRpcTimeoutMs) return false;
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        written += count;
    }
    output->Reset();
    return true;
}

std::optional<NvimClient::Value> NvimClient::Call(
    std::string_view method, const std::function<void(void*)>& pack_arguments, std::string* error) {
    if (!running()) {
        if (error) *error = "Neovim is not running.";
        return std::nullopt;
    }
    const std::uint64_t request_id = next_request_id_++;
    msgpack_sbuffer buffer;
    msgpack_sbuffer_init(&buffer);
    msgpack_packer packer;
    msgpack_packer_init(&packer, &buffer, msgpack_sbuffer_write);
    msgpack_pack_array(&packer, 4);
    msgpack_pack_int(&packer, 0);
    msgpack_pack_uint64(&packer, request_id);
    PackString(&packer, method);
    pack_arguments(&packer);
    if (!WriteMessage(buffer.data, buffer.size)) {
        if (error) *error = std::string(method) + ": could not write to Neovim.";
        msgpack_sbuffer_destroy(&buffer);
        return std::nullopt;
    }
    msgpack_sbuffer_destroy(&buffer);
    Value value;
    if (!ReadResponse(request_id, &value, error)) {
        if (error && !error->empty()) *error = std::string(method) + ": " + *error;
        return std::nullopt;
    }
    return value;
}

bool NvimClient::Notify(std::string_view method,
                        const std::function<void(void*)>& pack_arguments) {
    msgpack_sbuffer buffer;
    msgpack_sbuffer_init(&buffer);
    msgpack_packer packer;
    msgpack_packer_init(&packer, &buffer, msgpack_sbuffer_write);
    msgpack_pack_array(&packer, 3);
    msgpack_pack_int(&packer, 2);
    PackString(&packer, method);
    pack_arguments(&packer);
    const bool written = WriteMessage(buffer.data, buffer.size);
    msgpack_sbuffer_destroy(&buffer);
    return written;
}

bool NvimClient::ReadResponse(std::uint64_t request_id, Value* value, std::string* error) {
    auto* unpacker = static_cast<msgpack_unpacker*>(unpacker_);
    msgpack_unpacked unpacked;
    msgpack_unpacked_init(&unpacked);
    const auto started = std::chrono::steady_clock::now();
    while (std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - started).count() < kRpcTimeoutMs) {
        while (msgpack_unpacker_next(unpacker, &unpacked) == MSGPACK_UNPACK_SUCCESS) {
            const auto message = CopyValue(unpacked.data);
            if (message.type != Value::Type::Array || message.array.empty()) continue;
            const auto& kind = message.array[0];
            if (kind.type != Value::Type::Integer) continue;
            if (kind.integer == 1 && message.array.size() == 4 &&
                message.array[1].type == Value::Type::Integer &&
                static_cast<std::uint64_t>(message.array[1].integer) == request_id) {
                if (message.array[2].type != Value::Type::Nil) {
                    if (error) *error = DescribeValue(message.array[2]);
                    msgpack_unpacked_destroy(&unpacked);
                    return false;
                }
                *value = message.array[3];
                msgpack_unpacked_destroy(&unpacked);
                return true;
            }
        }
        if (!running()) break;
        auto* input = process_->GetInputStream();
        if (process_->IsInputAvailable() || (input && input->CanRead())) {
            char data[8192];
            input->Read(data, sizeof(data));
            const auto count = input->LastRead();
            if (count) {
                if (!msgpack_unpacker_reserve_buffer(unpacker, count)) break;
                std::memcpy(msgpack_unpacker_buffer(unpacker), data, count);
                msgpack_unpacker_buffer_consumed(unpacker, count);
                continue;
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    msgpack_unpacked_destroy(&unpacked);
    if (error) {
        *error = running() ? "Neovim did not answer within 5 seconds."
                           : "Neovim exited unexpectedly.";
        if (process_ && process_->IsErrorAvailable()) {
            char data[1024];
            process_->GetErrorStream()->Read(data, sizeof(data));
            const auto count = process_->GetErrorStream()->LastRead();
            if (count) *error += " " + std::string(data, count);
        }
    }
    return false;
}

std::optional<std::int64_t> NvimClient::CreateBuffer(std::string_view text,
                                                     std::string_view name,
                                                     std::string* error) {
    auto created = Call("nvim_create_buf", [](void* raw) {
        auto* packer = static_cast<msgpack_packer*>(raw);
        msgpack_pack_array(packer, 2);
        msgpack_pack_true(packer);
        msgpack_pack_false(packer);
    }, error);
    if (!created || created->type != Value::Type::Integer) {
        if (created && error) *error = "Neovim returned an invalid buffer handle.";
        return std::nullopt;
    }
    const auto buffer = created->integer;
    if (!name.empty()) {
        Call("nvim_buf_set_name", [buffer, name](void* raw) {
            auto* packer = static_cast<msgpack_packer*>(raw);
            msgpack_pack_array(packer, 2);
            PackInteger(packer, buffer);
            PackString(packer, name);
        }, nullptr);
    }
    Call("nvim_exec_lua", [buffer](void* raw) {
        auto* packer = static_cast<msgpack_packer*>(raw);
        msgpack_pack_array(packer, 2);
        PackString(packer, "vim.bo[(...)].undolevels = -1");
        msgpack_pack_array(packer, 1);
        PackInteger(packer, buffer);
    }, nullptr);
    if (!SetBufferText(buffer, text, error)) return std::nullopt;
    Call("nvim_exec_lua", [buffer](void* raw) {
        auto* packer = static_cast<msgpack_packer*>(raw);
        msgpack_pack_array(packer, 2);
        PackString(packer, "vim.bo[(...)].undolevels = vim.o.undolevels");
        msgpack_pack_array(packer, 1);
        PackInteger(packer, buffer);
    }, nullptr);
    buffer_carets_[buffer] = 0;
    buffer_modes_[buffer] = "n";
    return buffer;
}

bool NvimClient::CloseBuffer(std::int64_t buffer) {
    buffer_sources_.erase(buffer);
    buffer_carets_.erase(buffer);
    buffer_modes_.erase(buffer);
    return Call("nvim_buf_delete", [buffer](void* raw) {
        auto* packer = static_cast<msgpack_packer*>(raw);
        msgpack_pack_array(packer, 2);
        PackInteger(packer, buffer);
        msgpack_pack_map(packer, 1);
        PackString(packer, "force");
        msgpack_pack_true(packer);
    }, nullptr).has_value();
}

bool NvimClient::SetBufferText(std::int64_t buffer, std::string_view source, std::string* error) {
    const auto found = buffer_sources_.find(buffer);
    if (found != buffer_sources_.end() && found->second == source) return true;
    const auto lines = SplitLines(source);
    const auto result = Call("nvim_buf_set_lines", [buffer, &lines](void* raw) {
        auto* packer = static_cast<msgpack_packer*>(raw);
        msgpack_pack_array(packer, 5);
        PackInteger(packer, buffer);
        msgpack_pack_int(packer, 0);
        msgpack_pack_int(packer, -1);
        msgpack_pack_true(packer);
        msgpack_pack_array(packer, lines.size());
        for (const auto& line : lines) PackString(packer, line);
    }, error);
    if (!result) return false;
    buffer_sources_[buffer] = std::string(source);
    return true;
}

bool NvimClient::SetCursor(std::string_view source, std::size_t caret, std::string* error) {
    const auto [row, column] = CursorFromOffset(source, caret);
    return Call("nvim_win_set_cursor", [row, column](void* raw) {
        auto* packer = static_cast<msgpack_packer*>(raw);
        msgpack_pack_array(packer, 2);
        msgpack_pack_int(packer, 0);
        msgpack_pack_array(packer, 2);
        msgpack_pack_uint64(packer, row);
        msgpack_pack_uint64(packer, column);
    }, error).has_value();
}

std::optional<NvimEditorState> NvimClient::ApplyKey(std::int64_t buffer,
                                                    std::string_view source,
                                                    std::size_t caret,
                                                    std::string_view key,
                                                    std::string* error) {
    const bool source_changed = buffer_sources_.count(buffer) == 0 ||
                                buffer_sources_[buffer] != source;
    if (!SetBufferText(buffer, source, error)) return std::nullopt;
    if (!Call("nvim_set_current_buf", [buffer](void* raw) {
            auto* packer = static_cast<msgpack_packer*>(raw);
            msgpack_pack_array(packer, 1);
            PackInteger(packer, buffer);
        }, error)) return std::nullopt;
    const std::string prior_mode = buffer_modes_.count(buffer) ? buffer_modes_[buffer] : "n";
    const bool host_cursor_changed = buffer_carets_.count(buffer) == 0 ||
                                     buffer_carets_[buffer] != caret;
    if ((source_changed || (prior_mode == "n" && host_cursor_changed)) &&
        !SetCursor(source, caret, error)) return std::nullopt;
    auto replaced = Call("nvim_replace_termcodes", [key](void* raw) {
        auto* packer = static_cast<msgpack_packer*>(raw);
        msgpack_pack_array(packer, 4);
        PackString(packer, key);
        msgpack_pack_true(packer);
        msgpack_pack_false(packer);
        msgpack_pack_true(packer);
    }, error);
    if (!replaced || replaced->type != Value::Type::String) return std::nullopt;
    if (!Call("nvim_input", [&replaced](void* raw) {
            auto* packer = static_cast<msgpack_packer*>(raw);
            msgpack_pack_array(packer, 1);
            PackString(packer, replaced->string);
        }, error)) return std::nullopt;
    return ReadState(buffer, error);
}

std::optional<NvimEditorState> NvimClient::ReadState(std::int64_t buffer, std::string* error) {
    auto raw_lines = Call("nvim_buf_get_lines", [buffer](void* raw) {
        auto* packer = static_cast<msgpack_packer*>(raw);
        msgpack_pack_array(packer, 4);
        PackInteger(packer, buffer);
        msgpack_pack_int(packer, 0);
        msgpack_pack_int(packer, -1);
        msgpack_pack_true(packer);
    }, error);
    auto raw_cursor = Call("nvim_win_get_cursor", [](void* raw) {
        auto* packer = static_cast<msgpack_packer*>(raw);
        msgpack_pack_array(packer, 1);
        msgpack_pack_int(packer, 0);
    }, error);
    auto raw_mode = Call("nvim_get_mode", [](void* raw) {
        msgpack_pack_array(static_cast<msgpack_packer*>(raw), 0);
    }, error);
    if (!raw_lines || !raw_cursor || !raw_mode ||
        raw_lines->type != Value::Type::Array || raw_cursor->type != Value::Type::Array ||
        raw_cursor->array.size() < 2 || raw_mode->type != Value::Type::Map) return std::nullopt;

    std::vector<std::string> lines;
    lines.reserve(raw_lines->array.size());
    for (const auto& line : raw_lines->array) {
        if (line.type != Value::Type::String) return std::nullopt;
        lines.push_back(line.string);
    }
    if (lines.empty()) lines.emplace_back();
    const auto* mode_value = raw_mode->Find("mode");
    if (!mode_value || mode_value->type != Value::Type::String ||
        raw_cursor->array[0].type != Value::Type::Integer ||
        raw_cursor->array[1].type != Value::Type::Integer) return std::nullopt;

    NvimEditorState state;
    state.text = JoinLines(lines);
    const auto row = static_cast<std::size_t>(std::max<std::int64_t>(1, raw_cursor->array[0].integer));
    const auto column = static_cast<std::size_t>(std::max<std::int64_t>(0, raw_cursor->array[1].integer));
    state.caret = OffsetFromCursor(lines, row, column);
    state.selection_start = state.selection_end = state.caret;
    state.visual_anchor = state.caret;
    state.mode = mode_value->string;
    state.visual = state.mode == "v" || state.mode == "V" || state.mode == std::string(1, '\x16');
    state.visual_line = state.mode == "V";
    state.visual_block = state.mode == std::string(1, '\x16');

    if (state.visual) {
        auto anchor = Call("nvim_eval", [](void* raw) {
            auto* packer = static_cast<msgpack_packer*>(raw);
            msgpack_pack_array(packer, 1);
            PackString(packer, "getpos('v')");
        }, error);
        if (anchor && anchor->type == Value::Type::Array && anchor->array.size() >= 3 &&
            anchor->array[1].type == Value::Type::Integer && anchor->array[2].type == Value::Type::Integer) {
            const auto anchor_row = static_cast<std::size_t>(std::max<std::int64_t>(1, anchor->array[1].integer));
            const auto anchor_col = static_cast<std::size_t>(std::max<std::int64_t>(1, anchor->array[2].integer) - 1);
            const auto anchor_offset = OffsetFromCursor(lines, anchor_row, anchor_col);
            state.visual_anchor = anchor_offset;
            if (state.visual_line) {
                const auto first_row = std::min(row, anchor_row);
                const auto last_row = std::max(row, anchor_row);
                state.selection_start = OffsetFromCursor(lines, first_row, 0);
                state.selection_end = last_row < lines.size()
                    ? OffsetFromCursor(lines, last_row + 1, 0)
                    : state.text.size();
            } else {
                state.selection_start = std::min(anchor_offset, state.caret);
                state.selection_end = std::min(state.text.size(), std::max(anchor_offset, state.caret) + 1);
            }
        }
    }
    if (!state.mode.empty() && state.mode[0] == 'c') {
        auto command = Call("nvim_eval", [](void* raw) {
            auto* packer = static_cast<msgpack_packer*>(raw);
            msgpack_pack_array(packer, 1);
            PackString(packer, "getcmdtype() . getcmdline()");
        }, nullptr);
        if (command && command->type == Value::Type::String) state.command_line = command->string;
    }
    buffer_sources_[buffer] = state.text;
    buffer_carets_[buffer] = state.caret;
    buffer_modes_[buffer] = state.mode;
    return state;
}

}  // namespace say_count::app
