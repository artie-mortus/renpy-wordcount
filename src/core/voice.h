#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/parser.h"

namespace say_count {

enum class VoiceStatus { unrecorded, recorded, retake, approved };

struct VoiceLine {
    std::string id;
    std::string file;
    std::size_t line = 0;
    std::string label;
    std::string speaker;
    bool narration = false;
    bool character = false;
    std::string text;
};

struct VoiceEntry {
    VoiceStatus status = VoiceStatus::unrecorded;
    std::string voice_file;
    std::string notes;
};

std::vector<VoiceLine> parse_voice_dialogue(const std::vector<NamedScript>& scripts);
std::vector<VoiceLine> voice_script_rows(const std::vector<VoiceLine>& rows,
                                         std::string_view speaker = {});
std::string voice_status_name(VoiceStatus status);
std::optional<VoiceStatus> parse_voice_status(std::string_view status);

class VoiceStore {
public:
    explicit VoiceStore(std::string path) : path_(std::move(path)) {}
    std::map<std::string, VoiceEntry> Load(std::string* error = nullptr) const;
    bool Save(const std::map<std::string, VoiceEntry>& entries,
              std::string* error = nullptr) const;
private:
    std::string path_;
};

}  // namespace say_count
