#pragma once

#include <map>
#include <string>
#include <string_view>

namespace say_count {

struct RuntimeStateValidation {
    bool valid = false;
    std::string error;
};

RuntimeStateValidation validate_runtime_state(std::string_view json);
std::string renpy_runtime_source();
bool write_renpy_runtime(const std::string& game_directory, std::string* error = nullptr);

class RuntimePresetStore final {
public:
    explicit RuntimePresetStore(std::string path) : path_(std::move(path)) {}
    std::map<std::string, std::string> Load(std::string* error = nullptr) const;
    bool Save(const std::map<std::string, std::string>& presets, std::string* error = nullptr) const;
private:
    std::string path_;
};

}  // namespace say_count
