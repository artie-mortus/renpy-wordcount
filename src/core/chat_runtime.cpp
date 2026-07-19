#include "core/chat_runtime.h"

#include <filesystem>
#include <fstream>
#include <sstream>

namespace say_count {
namespace {

std::string Read(const std::filesystem::path& path, std::string* error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        if (error) *error = "Could not read " + path.string();
        return {};
    }
    std::ostringstream output;
    output << input.rdbuf();
    return output.str();
}

}  // namespace

bool ChatRuntimeInspection::has_modified_files() const noexcept {
    for (const auto& file : files)
        if (file.state == ChatRuntimeFileState::Modified) return true;
    return false;
}

bool ChatRuntimeInspection::has_new_files() const noexcept {
    for (const auto& file : files)
        if (file.state == ChatRuntimeFileState::NewFile) return true;
    return false;
}

ChatRuntimeInspection inspect_chat_runtime(std::string resource_directory,
                                           std::string scripts_directory,
                                           std::string vendor_directory) {
    namespace fs = std::filesystem;
    ChatRuntimeInspection result;
    const fs::path source(std::move(resource_directory));
    const fs::path destination = fs::path(scripts_directory) / "vendor" / vendor_directory;
    result.destination_directory = destination.string();
    struct Resource { std::string source_name; std::string destination_name; bool project_owned; };
    std::vector<Resource> resources{
        {"chat_program.rpy", "chat_program.rpy", false},
        {"say_count_chat_bridge.rpy", "say_count_chat_bridge.rpy", false},
        {"LICENSE.txt", "LICENSE.txt", false}, {"NOTICE", "NOTICE", false},
        {"INTEGRATION.md", "INTEGRATION.md", false}, {"manifest.json", "manifest.json", false},
        {"say_count_chat_config.rpy", "say_count_chat_config.rpy", true}};
    const fs::path assets = source / "assets";
    if (fs::exists(assets)) {
        for (const auto& entry : fs::recursive_directory_iterator(assets)) {
            if (!entry.is_regular_file()) continue;
            const auto relative = fs::relative(entry.path(), assets).generic_string();
            resources.push_back({"assets/" + relative, relative, true});
        }
    }
    for (const auto& resource : resources) {
        ChatRuntimeFile file;
        file.name = resource.destination_name;
        file.project_owned = resource.project_owned;
        file.source_path = (source / resource.source_name).string();
        file.destination_path = resource.project_owned
            ? (fs::path(scripts_directory) / resource.destination_name).string()
            : (destination / resource.destination_name).string();
        std::string read_error;
        const auto bundled = Read(file.source_path, &read_error);
        if (!read_error.empty()) {
            result.error = std::move(read_error);
            result.files.clear();
            return result;
        }
        if (!fs::exists(file.destination_path)) file.state = ChatRuntimeFileState::NewFile;
        else {
            const auto installed = Read(file.destination_path, &read_error);
            if (!read_error.empty()) {
                result.error = std::move(read_error);
                result.files.clear();
                return result;
            }
            file.state = installed == bundled || file.project_owned
                ? ChatRuntimeFileState::Identical : ChatRuntimeFileState::Modified;
        }
        result.files.push_back(std::move(file));
    }
    return result;
}

ChatRuntimeInstallResult install_chat_runtime(const ChatRuntimeInspection& inspection) {
    namespace fs = std::filesystem;
    ChatRuntimeInstallResult result;
    if (!inspection.error.empty()) { result.error = inspection.error; return result; }
    if (inspection.has_modified_files()) {
        result.error = "Locally modified chat runtime files were protected; nothing was installed.";
        return result;
    }
    std::error_code error;
    fs::create_directories(inspection.destination_directory, error);
    if (error) { result.error = "Could not create the chat runtime directory."; return result; }
    std::vector<fs::path> temporaries;
    for (const auto& file : inspection.files) {
        if (file.state == ChatRuntimeFileState::Identical) { ++result.identical; continue; }
        const fs::path temporary = file.destination_path + ".say-count-new";
        fs::create_directories(fs::path(file.destination_path).parent_path(), error);
        if (error) {
            for (const auto& path : temporaries) fs::remove(path);
            result.error = "Could not create a directory for " + file.name + ".";
            return result;
        }
        fs::copy_file(file.source_path, temporary, fs::copy_options::overwrite_existing, error);
        if (error) {
            for (const auto& path : temporaries) fs::remove(path);
            fs::remove(temporary);
            result.error = "Could not stage " + file.name + "; no existing file was replaced.";
            return result;
        }
        temporaries.push_back(temporary);
    }
    std::size_t temporary_index = 0;
    for (const auto& file : inspection.files) {
        if (file.state == ChatRuntimeFileState::Identical) continue;
        fs::rename(temporaries[temporary_index++], file.destination_path, error);
        if (error) {
            for (std::size_t index = temporary_index; index < temporaries.size(); ++index)
                fs::remove(temporaries[index]);
            result.error = "Could not finish installing " + file.name +
                "; files installed before it were already replaced.";
            return result;
        }
        ++result.installed;
    }
    return result;
}

}  // namespace say_count
